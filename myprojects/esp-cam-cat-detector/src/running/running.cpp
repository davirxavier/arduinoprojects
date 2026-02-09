#define ESP_CONFIG_PAGE_ENABLE_LOGGING
#define INFERENCE_ENABLE_LOG
#define ENABLE_LOGGING

#include <Arduino.h>
#include <esp_wifi.h>
#include <sdkconfig.h>
#include <general/secrets.h>
#include <general/config_page_setup.h>
#include <general/cam_config.h>
#include <general/inference_util.h>
#include <general/model_util.h>
#include <general/util.h>
#include <general/iot_setup.h>

#define AVERAGING_WINDOW 3
#define INFERENCE_THRESHOLD 0.7f
#define DETECTION_FOLDER "/final-detections"
#define EMPTY_FOLDER "/final-empty"

volatile bool cameraInit = false;
bool timeInit = false;

unsigned long inferenceTimer = 0;
unsigned long inferenceDelay = 800;

unsigned long luminosityUpdateInterval = 30 * 60 * 1000;
unsigned long luminosityReadTimer = 0;

unsigned long saveEmptyImageInterval = 45 * 60 * 1000;
unsigned long lastSavedEmptyImage = -saveEmptyImageInterval;

volatile bool doAction = false;
esp_jpeg_image_scale_t jpegScale = JPEG_IMAGE_SCALE_0;

void updateLuminosity()
{
    IotProperties::setLuminosity(readLuminosity());
    luminosityReadTimer = millis();
}

void delayTaskFn(TickType_t &lastWake, TickType_t interval)
{
    vTaskDelayUntil(&lastWake, interval);
}

void inferenceTask(void *args)
{
    TickType_t interval = pdMS_TO_TICKS(inferenceDelay);
    TickType_t lastWake = xTaskGetTickCount();

    while (true)
    {
        if (!cameraInit || !IotProperties::isInferenceOn())
        {
            delayTaskFn(lastWake, interval);
            continue;
        }

        camera_fb_t *fb = getFrameWithFlash();
        if (!fb)
        {
            MLOGN("Could not acquire framebuffer.");
            delayTaskFn(lastWake, interval);
            continue;
        }

        float average = 0;
        for (uint8_t i = 0; i < AVERAGING_WINDOW; i++)
        {
            InferenceUtil::InferenceOutput result{};
            InferenceUtil::runInferenceFromImage(result, fb->buf, fb->len, nullptr, nullptr, jpegScale);

            float val = InferenceUtil::triggerCertainty(result);
            MLOGF("Inference ran, current certainty: %f\n", val);

            average += val;
            delay(5);
        }
        average /= AVERAGING_WINDOW;

        if (average >= INFERENCE_THRESHOLD)
        {
            doAction = true;
            MLOGF("Average %f is higher than threshold, triggering.\n", average);
            saveImg(fb, average, DETECTION_FOLDER);
        }
        else if (millis() - lastSavedEmptyImage > saveEmptyImageInterval)
        {
            saveImg(fb, average, EMPTY_FOLDER);
            lastSavedEmptyImage = millis();
        }

        esp_camera_fb_return(fb);

        vTaskDelayUntil(&lastWake, interval);
    }
}

void actionTask(void *args)
{
    TickType_t interval = pdMS_TO_TICKS(100);
    TickType_t lastWake = xTaskGetTickCount();

    ActionController action;
    action.setup();

    while (true)
    {
        if (doAction)
        {
            action.doAction();
            doAction = false;
        }

        action.loop();
        vTaskDelayUntil(&lastWake, interval);
    }
}

void handleServerUpload()
{
    constexpr size_t maxUploadSize = 8192;
    static uint8_t buffer[maxUploadSize];
    static size_t uploadOffset = 0;
    static String error = "";

    server.on("/inf-upload", HTTP_POST, []()
    {
        VALIDATE_AUTH();

        if (!error.isEmpty())
        {
            server.send(500, "text/plain", error);
            return;
        }

        if (uploadOffset == 0)
        {
            server.send(400, "text/plain", "no data sent");
            return;
        }

        size_t srcSize = MODEL_DATA_INPUT_WIDTH * MODEL_DATA_INPUT_HEIGHT * MODEL_DATA_INPUT_CHANNELS;
        auto src = (uint8_t*) ps_malloc(srcSize);
        fmt2rgb888(buffer, uploadOffset, PIXFORMAT_JPEG, src);

        uint8_t *outImg = nullptr;
        size_t outImgSize = 0;

        InferenceUtil::InferenceOutput result{};
        InferenceUtil::runInferenceFromImage(result, buffer, uploadOffset, &outImg, &outImgSize);
        if (result.status == 0)
        {
            char buf[256]{};
            char numBuf[16]{};

            for (size_t i = 0; i < result.count; i++)
            {
                InferenceUtil::InferenceValues &vals = result.foundValues[i];
                strcat(buf, vals.label);
                strcat(buf, " - prob=");

                snprintf(numBuf, sizeof(numBuf), "%f", vals.value);
                strcat(buf, numBuf);

                snprintf(numBuf, sizeof(numBuf), "%f", vals.x);
                strcat(buf, ", x=");
                strcat(buf, numBuf);

                strcat(buf, ", y=");
                strcat(buf, numBuf);

                strcat(buf, "; ");
            }

            snprintf(numBuf, sizeof(numBuf), "%f", InferenceUtil::triggerCertainty(result));
            strcat(buf, "Should trigger certainty: ");
            strcat(buf, numBuf);

            snprintf(numBuf, sizeof(numBuf), "%lu", result.totalLatency);
            strcat(buf, "; latency=");
            strcat(buf, numBuf);
            strcat(buf, "ms");

            server.sendHeader("x-result", buf);
            InferenceUtil::drawMarkers(result, outImg);

            uint8_t *jpegOut = nullptr;
            size_t jpegOutSize = 0;
            fmt2jpg(outImg, outImgSize, MODEL_INPUT_WIDTH, MODEL_INPUT_HEIGHT, PIXFORMAT_RGB888, 200, &jpegOut, &jpegOutSize);
            free(outImg);

            server.send_P(200, "image/jpeg", (char*) jpegOut, jpegOutSize);
            free(jpegOut);
        }
        else
        {
            server.send(500, "text/plain", InferenceUtil::currentOutput);
        }
    }, []()
    {
        VALIDATE_AUTH();

        HTTPUpload &upload = server.upload();
        MLOGF("Received upload event %d with name %s and size %zu\n", upload.status, upload.filename.c_str(), upload.totalSize);

        if (upload.status == UPLOAD_FILE_START)
        {
            memset(buffer, 0, maxUploadSize);
            uploadOffset = 0;
            error = "";
        }
        else if (upload.status == UPLOAD_FILE_WRITE)
        {
            if (!error.isEmpty())
            {
                return;
            }

            if (uploadOffset + upload.currentSize >= maxUploadSize)
            {
                error = "upload overflow, limit is 4kb";
                return;
            }

            memcpy(buffer + uploadOffset, upload.buf, upload.currentSize);
            uploadOffset += upload.currentSize;
        }
    });
}

void setup()
{
#ifdef ENABLE_LOGGING
    Serial.begin(115200);
#endif

    delay(3000);

    MLOGF("PSRAM: %s\n", psramFound() ? "OK" : "FAILED");
    MLOGF("Free PSRAM: %d bytes\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    setupPins();

    WiFi.setSleep(WIFI_PS_NONE);
    esp_log_level_set("*", ESP_LOG_NONE);

    camConfig.frame_size = FRAMESIZE_SXGA;
    cameraInit = CamConfig::initCamera();
    if (cameraInit)
    {
        CamConfig::setRes(FRAMESIZE_240X240);
        jpegScale = JPEG_IMAGE_SCALE_0;
    }

    MLOGF("Free PSRAM after camera init: %lu\n", ESP.getFreePsram());
    MLOGF("Free DRAM after camera init: %zu\n", ESP.getFreeHeap());

    sdInit = CamConfig::initSdCard();
    updateLuminosity();

    int modelInitRes = ModelUtil::loadModel();
    if (modelInitRes != ModelUtil::OK)
    {
        MLOGF("Error initializing model: %d\n", modelInitRes);
    }

    server.on("/test-run", HTTP_POST, []()
    {
        VALIDATE_AUTH();
        testRun();
    });

    server.on("/snap", HTTP_GET, []()
    {
        VALIDATE_AUTH();
        camera_fb_t *fb = getFrameWithFlash();
        server.send_P(200, "text/plain", (char*) fb->buf, fb->len);
        esp_camera_fb_return(fb);
    });

    server.on("/inf-toggle", HTTP_POST, []()
    {
        VALIDATE_AUTH();
        IotProperties::toggleInference();
        server.send(200, "text/plain", "ok");
    });

    server.on("/inf", HTTP_GET, []()
    {
        VALIDATE_AUTH();

        camera_fb_t *fb = getFrameWithFlash();
        uint8_t *outImg = nullptr;
        size_t outImgSize = 0;

        InferenceUtil::InferenceOutput result{};
        InferenceUtil::runInferenceFromImage(result, fb->buf, fb->len, &outImg, &outImgSize, jpegScale);
        server.sendHeader("x-trigger-certainty", String(InferenceUtil::triggerCertainty(result)));

        server.sendHeader("x-result", InferenceUtil::currentOutput);
        esp_camera_fb_return(fb);

        if (outImg != nullptr)
        {
            InferenceUtil::drawMarkers(result, outImg);

            uint8_t *outJpeg = nullptr;
            size_t outJpegSize = 0;
            fmt2jpg(outImg, outImgSize, 96, 96, PIXFORMAT_RGB888, 200, &outJpeg, &outJpegSize);
            free(outImg);

            server.send_P(200, "image/jpeg", (char*) outJpeg, outJpegSize);
            free(outJpeg);
        }
        else
        {
            server.send(500, "text/plain", "error running inference");
        }
    });

    handleServerUpload();
    ConfigPageSetup::setupConfigPage();
    IotProperties::setup();

    MLOGN("Started.");

    xTaskCreatePinnedToCore(
        inferenceTask,
        "inferencetask",
        8192,
        nullptr,
        3,
        nullptr,
        1);

    xTaskCreatePinnedToCore(
        actionTask,
        "actiontask",
        2048,
        nullptr,
        4,
        nullptr,
        1);
}

void loop()
{
    ConfigPageSetup::configPageLoop();
    IotProperties::loop();

    if (WiFi.status() == WL_CONNECTED)
    {
        if (!timeInit)
        {
            // gmt-3
            configTime(-3 * 3600, 0, "pool.ntp.org");
            tm info{};
            timeInit = getLocalTime(&info);
        }
    }

    if (millis() - luminosityReadTimer > luminosityUpdateInterval)
    {
        updateLuminosity();
    }
}