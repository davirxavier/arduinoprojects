#include <Arduino.h>
#include <secrets.h>

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

#define ACTION_PIN 16
#define PAN_PIN 12
#define TILT_PIN 13

#define ENABLED_ID "enabled"
#define ACTION_TEST_ID "action_test"
#define IMAGE_ID "picture"
#define EVENT_ID "events"
#define INFERENCE_ID "inference-model"

const char *events[] = {
    "Detection"
};
size_t eventCount = 1;
bool enabled = true;

ESP_CONFIG_PAGE::WEBSERVER_T httpServer(80);
Mosquetteer mqtt;
bool mqttBegun = false;
bool mqttStarted = false;

bool nextPic = true;
unsigned long pictureDelay = 1500;
unsigned long lastPicture = pictureDelay;

bool hasAction = false;
unsigned long actionTimer = 0;
bool finishAction = false;

Servo tiltServo;
Servo panServo;

int getCenter(int v1, int v2)
{
    return ((v2 - v1) / 2) + v1;
}

constexpr int PAN_REST = 90;
constexpr int TILT_REST = 180;

bool isHuman(int classId, double confidence)
{
    return classId == 1 && confidence >= 0.5;
}

bool isCat(int classId, double confidence)
{
    return classId == 0 && confidence >= 0.6;
}

void startAction()
{
    hasAction = true;
    finishAction = false;
    actionTimer = millis();
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

#ifdef ENABLE_LOGGING
    Serial.begin(115200);
    delay(5000); // TODO Remove
#endif

    // MLOGN("Init camera");
    CamConfig::initCamera();

    ESP_CONFIG_PAGE::setAPConfig("ESP32-CAT-DETECTOR", "adminadmin");
    ESP_CONFIG_PAGE::initModules(&httpServer, CONFIG_PAGE_USER, CONFIG_PAGE_PASS, "ESP32-BELL-V2");
    httpServer.begin();

    mqtt.setClientConfig(MQTT_URL, MQTT_USER, MQTT_PASS);
    mqtt.setDeviceInfo("esp_cat_detector", "Cat Detector");
    // mqtt.setTlsConfig(cacert);

    mqtt.define(ENABLED_ID, "Enabled", MosquetteerShared::SWITCH);
    mqtt.onCommand(ENABLED_ID, [](const MosquetteerProp &p)
    {
        enabled = p.isOnState();
    });

    mqtt.define(ACTION_TEST_ID, "Pump", MosquetteerShared::BUTTON);
    mqtt.onCommand(ACTION_TEST_ID, [](const MosquetteerProp &p)
    {
        startAction();
    });

    static constexpr char pictureContentType[] = "image/jpeg";
    MosquetteerShared::ImageConfig imageConfig{};
    imageConfig.binaryConfig.contentType = pictureContentType;
    mqtt.define(IMAGE_ID, "Picture", MosquetteerShared::IMAGE, &imageConfig);

    MosquetteerShared::EventConfig eventConfig{};
    eventConfig.eventTypes = events;
    eventConfig.eventTypeCount = eventCount;
    mqtt.define(EVENT_ID, "Events", MosquetteerShared::EVENT, &eventConfig);

    static constexpr char customSendTopic[] = "model-requests/image/couch-detector/1";
    static constexpr char customReceiveTopic[] = "model-responses/image/couch-detector/1";
    MosquetteerShared::CustomConfig customConfig{};
    customConfig.sendTopic = customSendTopic;
    customConfig.receiveTopic = customReceiveTopic;

    mqtt.define(INFERENCE_ID, "Inference", MosquetteerShared::CUSTOM_TOPIC, &customConfig);
    mqtt.onData(INFERENCE_ID, [](const char *data, size_t len)
    {
        nextPic = true;
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, data, len);

        if (err != DeserializationError::Ok)
        {
            MLOGF("Error reading inference json: %.*s\n", len, data);
            return;
        }

        bool success = doc["success"];
        if (!success)
        {
            MLOGN("Inference error, check logs.");
            return;
        }

        if (doc["abs"])
        {
            JsonArray arr = doc["abs"].as<JsonArray>();
            panServo.write(arr[0]);
            tiltServo.write(arr[1]);
            return;
        }

        bool hasCat = false;
        bool hasHuman = false;

        int x1 = -1;
        int x2 = -1;
        int y1 = -1;
        int y2 = -1;

        JsonArray detections = doc["detections"].as<JsonArray>();
        for (JsonVariant d : detections)
        {
            int classId = d["class_id"];
            double confidence = d["confidence"];
            int localX1 = d["x1"];
            int localX2 = d["x2"];
            int localY1 = d["y1"];
            int localY2 = d["y2"];

            MLOGF("Detection for class %d with confidence %f, x/y: %d/%d\n", classId, confidence, getCenter(localX1, localX2), getCenter(localY1, localY2));

            if (isCat(classId, confidence))
            {
                hasCat = true;

                if (x1 == -1)
                {
                    x1 = localX1;
                    x2 = localX2;
                    y1 = localY1;
                    y2 = localY2;
                }
            }
            else if (isHuman(classId, confidence))
            {
                hasHuman = true;
            }
        }

        if (hasCat && !hasHuman && !hasAction && millis() - actionTimer > 1000)
        {
            int x = getCenter(x1, x2);
            int y = getCenter(y1, y2);
            float pan = 0;
            float tilt = 0;
            pixelToServo(x, y, pan, tilt);

            panServo.write(pan);
            tiltServo.write(tilt);

            MLOGF("Cat detected, queueing action. Center: %d/%d (x/y), Angles: %d/%d (pan/tilt)\n", x, y, pan, tilt);
            startAction();
            mqtt.sendState(EVENT_ID, events[0]);
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
            lastPicture = millis();
            mqttStarted = true;
        }
        else
        {
            mqttStarted = false;
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

    if (!hasAction && mqttStarted && millis() - lastPicture > pictureDelay)
    {
        MLOGN("Sending picture update.");
        camera_fb_t* fb = esp_camera_fb_get();
        if (fb != nullptr)
        {
            mqtt.sendState(IMAGE_ID, (char*) fb->buf, fb->len);
            if (enabled && nextPic)
            {
                nextPic = false;
                mqtt.sendState(INFERENCE_ID, (char*) fb->buf, fb->len);
            }
            esp_camera_fb_return(fb);
        }
        lastPicture = millis();
    }

    if (!enabled)
    {
        return;
    }

    if (hasAction && finishAction && millis() - actionTimer > 500)
    {
        MLOGN("Action finished.");
        digitalWrite(ACTION_PIN, LOW);
        finishAction = false;
        hasAction = false;
        // panServo.write(PAN_REST);
        // tiltServo.write(TILT_REST);
        actionTimer = millis();
    }

    if (hasAction && !finishAction && millis() - actionTimer > 500)
    {
        MLOGN("Aimed, starting action.");
        digitalWrite(ACTION_PIN, HIGH);
        finishAction = true;
        actionTimer = millis();
    }
}
