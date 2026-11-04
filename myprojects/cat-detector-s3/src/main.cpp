#include <Arduino.h>
#include <secrets.h>
#include <Adafruit_VEML7700.h>
#include <SD_MMC.h>
#include <atomic>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#define ENABLE_LOGGING
#define ESP_CONFIG_PAGE_ENABLE_LOGGING
#define MQTTO_ENABLE_LOGGING

#include <servo_util.h>

#ifdef ENABLE_LOGGING
#define MLOG(str) Serial.print(str)
#define MLOGN(str) Serial.println(str)
#define MLOGF(str, p...) Serial.printf(str, p)
#define MVLOGF(str, p...) Serial.vprintf(str, p)
#else
#define MLOG(str)
#define MLOGN(str)
#define MLOGF(str, p...)
#define MVLOGF(str, p...)
#endif
#include <cam_config.h>
#include <Mosquetteer.h>
#include <esp-config-page.h>
#include <ESP32Servo.h>

#define ACTION_PIN 1
#define LIGHTS_PIN 2
#define PAN_PIN 20
#define TILT_PIN 19

#define Y_IGNORE_PX_THRESHOLD 640

#define ENABLED_ID "enabled"
#define ACTION_TEST_ID "action_test"
#define IMAGE_ID "picture"
#define EVENT_ID "events"
#define LUX_ID "lux-sense"
#define INFERENCE_ID "inference-model"
#define LIGHT_MODE_ID "lights"

enum EventType
{
    EVENT_DETECTION,
    EVENT_PROBABLE_DETECTION,
    EVENT_INFERENCE_TIMEOUT,
    EVENT_INFERENCE_JSON_ERROR,
    EVENT_INFERENCE_FAILED,
    EVENT_CAMERA_ERROR,
    EVENT_QUEUE_ERROR,
    EVENT_ACTION_TIMER_ERROR,
    EVENT_COUNT
};

const char *eventsNames[] = {
    "Detection",
    "Probable Detection",
    "Error: Inference Timeout",
    "Error: Invalid Inference JSON",
    "Error: Inference Failed",
    "Error: Camera Capture",
    "Error: Detection Queue",
    "Error: Action Timer"
};

size_t eventCount = EVENT_COUNT;
bool cameraErrorReported = false;

ESP_CONFIG_PAGE::WEBSERVER_T httpServer(80);
Mosquetteer mqtt;

Servo tiltServo;
Servo panServo;

TwoWire LuxWire(0);
Adafruit_VEML7700 veml = Adafruit_VEML7700();
bool luxInit = false;
unsigned long lastLuxReading = 0;

int getCenter(int v1, int v2)
{
    return ((v2 - v1) / 2) + v1;
}

constexpr int PAN_REST = 90;
constexpr int TILT_REST = 180;

bool mqttBegun = false;
unsigned long pictureDelay = 200;

std::atomic_bool enabled{true};
std::atomic_bool mqttStarted{false};
std::atomic_bool nextPic{true};
std::atomic<uint32_t> inferenceRequestAt{0};
std::atomic<uint32_t> lastPicture{200};

std::atomic<float> smoothedCatConfidenceAtomic = 0.0f;
std::atomic<float> smoothedHumanConfidenceAtomic = 0.0f;
constexpr float SMOOTHING_FACTOR = 0.5f;

std::atomic_bool hasActionAtomic{false};
std::atomic<uint32_t> actionTimerAtomic{0};
esp_timer_handle_t actionTimerHandle = nullptr;

struct DetectionCommand
{
    int pan;
    int tilt;
    bool activate;
};

QueueHandle_t detectionQueue = nullptr;

void sendEvent(EventType event)
{
    mqtt.sendState(EVENT_ID, eventsNames[event]);
}

bool isHuman(int classId, double confidence)
{
    return classId == 1 && confidence >= 0.5;
}

bool isCat(int classId, double confidence)
{
    return classId == 0 && confidence >= 0.6;
}

bool startAction()
{
    if (actionTimerHandle == nullptr)
    {
        sendEvent(EVENT_ACTION_TIMER_ERROR);
        MLOGN("Action timer is not initialized.");
        return false;
    }

    if (hasActionAtomic.load() || esp_timer_is_active(actionTimerHandle))
    {
        return false;
    }

    actionTimerAtomic.store(millis());
    hasActionAtomic.store(true);

    digitalWrite(ACTION_PIN, HIGH);

    esp_err_t err = esp_timer_start_once(actionTimerHandle, 200000);
    if (err != ESP_OK)
    {
        digitalWrite(ACTION_PIN, LOW);
        hasActionAtomic.store(false);
        sendEvent(EVENT_ACTION_TIMER_ERROR);
        MLOGF("Failed to start action timer: %d\n", err);
        return false;
    }

    return true;
}

void actionFinished(void* arg)
{
    digitalWrite(ACTION_PIN, LOW);
    actionTimerAtomic.store(millis());
    hasActionAtomic.store(false);
}

bool enqueueDetectionCommand(const DetectionCommand& command)
{
    if (detectionQueue == nullptr)
    {
        sendEvent(EVENT_QUEUE_ERROR);
        MLOGN("Detection queue unavailable.");
        return false;
    }

    if (xQueueOverwrite(detectionQueue, &command) != pdPASS)
    {
        sendEvent(EVENT_QUEUE_ERROR);
        return false;
    }

    return true;
}

void setup()
{
    ESP32PWM::allocateTimer(LEDC_TIMER_2);
    ESP32PWM::allocateTimer(LEDC_TIMER_3);

    panServo.setPeriodHertz(50);
    panServo.attach(PAN_PIN, 500, 2400);
    tiltServo.setPeriodHertz(50);
    tiltServo.attach(TILT_PIN, 500, 2400);
    panServo.write(PAN_REST);
    tiltServo.write(TILT_REST);

    LittleFS.begin();

    pinMode(ACTION_PIN, OUTPUT);
    digitalWrite(ACTION_PIN, LOW);

    pinMode(LIGHTS_PIN, OUTPUT);
    digitalWrite(LIGHTS_PIN, LOW);

#ifdef ENABLE_LOGGING
    Serial.begin(115200);
    delay(5000);
#endif

    detectionQueue = xQueueCreate(1, sizeof(DetectionCommand));
    if (detectionQueue == nullptr)
    {
        MLOGN("Failed to create detection queue!");
    }

    esp_timer_create_args_t args = {
        .callback = &actionFinished,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "actionTimer"
    };

    esp_err_t timerErr = esp_timer_create(&args, &actionTimerHandle);

    if (timerErr != ESP_OK)
    {
        actionTimerHandle = nullptr;
        MLOGF("Failed to create action timer: %d\n", timerErr);
    }

    LuxWire.begin(47, 21, 100000);
    luxInit = veml.begin(&LuxWire);

    // MLOGN("Init camera");
    CamConfig::initCamera();

    ESP_CONFIG_PAGE::setAPConfig("ESP32-CAT-DETECTOR", "adminadmin");
    ESP_CONFIG_PAGE::initModules(&httpServer, CONFIG_PAGE_USER, CONFIG_PAGE_PASS, "ESP32-SOFA-V2");
    httpServer.begin();

    mqtt.setClientConfig(MQTT_URL, MQTT_USER, MQTT_PASS);
    mqtt.setDeviceInfo("esp_cat_detector", "Cat Detector");
    // mqtt.setTlsConfig(cacert);

    mqtt.define(ENABLED_ID, "Enabled", MosquetteerShared::SWITCH);
    mqtt.onCommand(ENABLED_ID, [](const MosquetteerProp &p)
    {
        enabled.store(p.isOnState());
    });

    mqtt.define(ACTION_TEST_ID, "Pump", MosquetteerShared::BUTTON);
    mqtt.onCommand(ACTION_TEST_ID, [](const MosquetteerProp &p)
    {
        DetectionCommand command{
            .pan = -1,
            .tilt = -1,
            .activate = true
        };

        enqueueDetectionCommand(command);
    });

    mqtt.define(LIGHT_MODE_ID, "Lights", MosquetteerShared::SWITCH);
    mqtt.onCommand(LIGHT_MODE_ID, [](const MosquetteerProp &v)
    {
        bool isOn = v.isOnState();
        digitalWrite(LIGHTS_PIN, isOn ? HIGH : LOW);
        mqtt.sendState(LIGHT_MODE_ID, isOn);
    });

    static constexpr char pictureContentType[] = "image/jpeg";
    MosquetteerShared::ImageConfig imageConfig{};
    imageConfig.binaryConfig.contentType = pictureContentType;
    mqtt.define(IMAGE_ID, "Picture", MosquetteerShared::IMAGE, &imageConfig);

    MosquetteerShared::EventConfig eventConfig{};
    eventConfig.eventTypes = eventsNames;
    eventConfig.eventTypeCount = eventCount;
    mqtt.define(EVENT_ID, "Events", MosquetteerShared::EVENT, &eventConfig);

    MosquetteerShared::SensorConfig luxConfig{};
    luxConfig.unit.setUnit("lx");
    mqtt.define(LUX_ID, "Luminosity", MosquetteerShared::SENSOR, &luxConfig);

    static constexpr char customSendTopic[] = "model-requests/image/couch-detector/1";
    static constexpr char customReceiveTopic[] = "model-responses/image/couch-detector/1";
    MosquetteerShared::CustomConfig customConfig{};
    customConfig.sendTopic = customSendTopic;
    customConfig.receiveTopic = customReceiveTopic;

    mqtt.define(INFERENCE_ID, "Inference", MosquetteerShared::CUSTOM_TOPIC, &customConfig);
    mqtt.onData(INFERENCE_ID, [](const char *data, size_t len)
    {
        nextPic.store(true);
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, data, len);

        if (err != DeserializationError::Ok)
        {
            sendEvent(EVENT_INFERENCE_JSON_ERROR);
            MLOGF("Error reading inference json: %.*s\n", len, data);
            return;
        }

        bool success = doc["success"];
        if (!success)
        {
            sendEvent(EVENT_INFERENCE_FAILED);
            MLOGN("Inference error, check logs.");
            return;
        }

        if (doc["abs"])
        {
            JsonArray arr = doc["abs"].as<JsonArray>();

            DetectionCommand command{
                .pan = arr[0],
                .tilt = arr[1],
                .activate = false
            };

            enqueueDetectionCommand(command);
            return;
        }

        int x1 = -1;
        int x2 = -1;
        int y1 = -1;
        int y2 = -1;

        float catConfidence = 0.0f;
        float humanConfidence = 0.0f;

        auto detections = doc["detections"].as<JsonArray>();
        for (JsonVariant d : detections)
        {
            int classId = d["class_id"];
            float confidence = d["confidence"];

            int localX1 = d["x1"];
            int localX2 = d["x2"];
            int localY1 = d["y1"];
            int localY2 = d["y2"];

            MLOGF("Detection for class %d with confidence %f, x/y: %d/%d\n",
                  classId, confidence,
                  getCenter(localX1, localX2),
                  getCenter(localY1, localY2));

            if (isCat(classId, confidence) && confidence > catConfidence)
            {
                catConfidence = confidence;
                x1 = localX1;
                x2 = localX2;
                y1 = localY1;
                y2 = localY2;
            }
            else if (isHuman(classId, confidence))
            {
                humanConfidence = max(humanConfidence, confidence);
            }
        }

        if (x1 != -1)
        {
            int centerY = getCenter(y1, y2);

            if (centerY > Y_IGNORE_PX_THRESHOLD)
            {
                MLOGF("Ignoring selected cat. Center Y: %d\n", centerY);
                catConfidence = 0.0f;
                x1 = -1;
            }
        }

        if (catConfidence > 0.4)
        {
            sendEvent(EVENT_PROBABLE_DETECTION);
        }

        float smoothedCatConfidence = SMOOTHING_FACTOR * catConfidence + (1.0f - SMOOTHING_FACTOR) * smoothedCatConfidenceAtomic.load();
        float smoothedHumanConfidence = SMOOTHING_FACTOR * humanConfidence + (1.0f - SMOOTHING_FACTOR) * smoothedHumanConfidenceAtomic.load();
        float shouldActivateConfidence = smoothedCatConfidence * (1.0f - smoothedHumanConfidence);
        constexpr float ACTIVATION_THRESHOLD = 0.6f;

        smoothedCatConfidenceAtomic.store(smoothedCatConfidence);
        smoothedHumanConfidenceAtomic.store(smoothedHumanConfidence);

        if (enabled.load() && x1 != -1 && shouldActivateConfidence >= ACTIVATION_THRESHOLD && !hasActionAtomic.load() && millis() - actionTimerAtomic.load() > 1000)
        {
            int x = getCenter(x1, x2);
            int y = getCenter(y1, y2);

            float pan = 0;
            float tilt = 0;

            pixelToServo(x, y, pan, tilt);

            DetectionCommand command{
                .pan = (int)pan,
                .tilt = (int)tilt,
                .activate = true
            };

            enqueueDetectionCommand(command);
            MLOGF("Cat detected, confidence: %f, Center: %d/%d\n", shouldActivateConfidence, x, y);
        }
        else
        {
            MLOGN("No detections.");
        }
    });

    mqtt.onEvent([](MosquetteerShared::Event event)
    {
        if (event == MosquetteerShared::CONNECTED)
        {
            lastPicture.store(millis());
            mqttStarted.store(true);
        }
        else
        {
            mqttStarted.store(false);
        }
    });
}

void loop()
{
    ESP_CONFIG_PAGE::loop();
    httpServer.handleClient();

    if (!mqttBegun && WiFi.status() == WL_CONNECTED)
    {
        mqtt.begin();
        mqttBegun = true;
    }

    if (!nextPic.load() && millis() - inferenceRequestAt.load() > 2000)
    {
        MLOGN("Inference timeout. Re-enabling next picture.");
        sendEvent(EVENT_INFERENCE_TIMEOUT);
        nextPic.store(true);
    }

    if (mqttStarted.load() && millis() - lastPicture.load() > pictureDelay)
    {
        MLOGN("Sending picture update.");
        camera_fb_t* fb = esp_camera_fb_get();
        if (fb != nullptr)
        {
            mqtt.sendState(IMAGE_ID, (char*) fb->buf, fb->len);

            if (!hasActionAtomic.load() && enabled.load() && nextPic.load())
            {
                inferenceRequestAt.store(millis());
                nextPic.store(false);
                mqtt.sendState(INFERENCE_ID, (char*)fb->buf, fb->len);
            }

            esp_camera_fb_return(fb);
            cameraErrorReported = false;
        }
        else if (!cameraErrorReported)
        {
            sendEvent(EVENT_CAMERA_ERROR);
            cameraErrorReported = true;
            MLOGN("Camera capture failed.");
        }
        lastPicture.store(millis());
    }

    if (luxInit && millis() - lastLuxReading > 10000)
    {
        float l = veml.readLux(VEML_LUX_AUTO);
        MLOG("Lux: ");
        MLOGN(l);
        mqtt.sendState(LUX_ID, round(l * 100.0) / 100.0);
        lastLuxReading = millis();
    }

    if (!enabled.load())
    {
        if (detectionQueue != nullptr)
        {
            xQueueReset(detectionQueue);
        }

        return;
    }

    DetectionCommand command{};
    if (detectionQueue != nullptr && xQueueReceive(detectionQueue, &command, 0) == pdTRUE)
    {
        if (command.pan >= 0 && command.pan <= 180)
        {
            panServo.write(command.pan);
        }

        if (command.tilt >= 0 && command.tilt <= 180)
        {
            tiltServo.write(command.tilt);
        }

        if (command.activate)
        {
            MLOGN("Aimed, starting action.");

            if (startAction())
            {
                sendEvent(EVENT_DETECTION);
            }
        }
    }
}
