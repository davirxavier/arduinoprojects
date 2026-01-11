#define ESP_CONFIG_PAGE_ENABLE_LOGGING

#include <Arduino.h>
#include <general/secrets.h>
#include <general/config_page_setup.h>
#include <general/cam_config.h>
#include <general/inference_util.h>

volatile bool cameraInit = false;
bool timeInit = false;
unsigned long inferenceTimer = 0;
unsigned long inferenceDelay = 1000;
volatile bool inferenceOn = false;
volatile bool doAction = false;

void inferenceTask(void *args)
{
    TickType_t interval = pdMS_TO_TICKS(inferenceDelay);
    TickType_t lastWake = xTaskGetTickCount();

    while (true)
    {
        if (cameraInit && inferenceOn)
        {
            camera_fb_t *fb = esp_camera_fb_get();
            InferenceUtil::InferenceOutput result = InferenceUtil::runInferenceFromImage(fb->buf, fb->len);
            esp_camera_fb_return(fb);

            if (result.status == EI_IMPULSE_OK && result.count > 0)
            {
                MLOGN("Inference returned positive result, triggering.");
                doAction = true;
            }
        }
        else if (!cameraInit)
        {
            MLOGN("Camera not initialized, trying again.");
            cameraInit = CamConfig::initCamera();
            if (!cameraInit)
            {
                MLOGN("Error re-initializing camera.");
            }
        }

        vTaskDelayUntil(&lastWake, interval);
    }
}

void actionTask(void *args)
{
    TickType_t interval = pdMS_TO_TICKS(50);
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

        InferenceUtil::InferenceOutput result = InferenceUtil::runInferenceFromImage(buffer, uploadOffset);
        if (result.status == 0)
        {
            size_t len = strlen(InferenceUtil::currentOutput);
            for (size_t i = 0; i < len; i++)
            {
                if (InferenceUtil::currentOutput[i] == ';')
                {
                    InferenceUtil::currentOutput[i] = '\n';
                }
            }

            server.send(200, "text/plain", InferenceUtil::currentOutput);
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

    camConfig.frame_size = FRAMESIZE_240X240;
    cameraInit = CamConfig::initCamera();

    // Not using right now (and conflicts with pin 13, needs pullup always)
    // cameraInit = CamConfig::initSdCard();

    server.on("/inf-toggle", HTTP_POST, []()
    {
        VALIDATE_AUTH();
        inferenceOn = !inferenceOn;
        server.send(200, "text/plain", "ok");
    });

    server.on("/inf", HTTP_GET, []()
    {
        camera_fb_t *fb = esp_camera_fb_get();
        uint8_t *outImg = nullptr;
        size_t outImgSize = 0;

        InferenceUtil::InferenceOutput result = InferenceUtil::runInferenceFromImage(fb->buf, fb->len, &outImg, &outImgSize);
        if (InferenceUtil::shouldTrigger(result))
        {
            doAction = true;
            server.sendHeader("x-action", "true");
        }

        server.sendHeader("x-result", InferenceUtil::currentOutput);

        uint8_t *outJpeg = nullptr;
        size_t outJpegSize = 0;
        fmt2jpg(outImg, outImgSize, 96, 96, PIXFORMAT_RGB888, 200, &outJpeg, &outJpegSize);
        free(outImg);
        server.send_P(200, "image/jpeg", (char*) outJpeg, outJpegSize);
        esp_camera_fb_return(fb);
        free(outJpeg);
    });

    handleServerUpload();
    ConfigPageSetup::setupConfigPage();

    delay(1000);
    MLOGN("Started.");

    xTaskCreatePinnedToCore(
        inferenceTask,
        "inferencetask",
        8192,
        nullptr,
        3,
        nullptr,
        1);

    // xTaskCreatePinnedToCore(
    //     actionTask,
    //     "actiontask",
    //     4096,
    //     nullptr,
    //     3,
    //     nullptr,
    //     1);
}

void loop()
{
    ConfigPageSetup::configPageLoop();

    // if (WiFi.status() == WL_CONNECTED)
    // {
    //     if (!timeInit)
    //     {
    //         // gmt-3
    //         configTime(-3 * 3600, 0, "pool.ntp.org");
    //         tm info{};
    //         timeInit = getLocalTime(&info);
    //     }
    // }
}