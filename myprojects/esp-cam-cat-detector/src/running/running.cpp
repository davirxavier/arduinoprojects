#include <Arduino.h>
#include <general/config_page_setup.h>
#include <general/cam_config.h>
#include <general/inference_util.h>

volatile bool cameraInit = false;
bool timeInit = false;
unsigned long inferenceTimer = 0;
unsigned long inferenceDelay = 1000;
volatile bool inferenceOn = true;

[[noreturn]] void inferenceTask(void *args)
{
    TickType_t interval = pdMS_TO_TICKS(inferenceDelay);
    TickType_t lastWake = xTaskGetTickCount();

    while (true)
    {
        if (cameraInit && inferenceOn)
        {
            InferenceUtil::runInference();
        }
        else if (!cameraInit)
        {
            Serial.println("Camera not initialized, trying again.");
            cameraInit = CamConfig::initCamera();
            if (!cameraInit)
            {
                Serial.println("Error re-initializing camera.");
            }
        }

        vTaskDelayUntil(&lastWake, interval);
        vTaskDelay(1);
    }
}

void setup()
{
    Serial.begin(115200);
    camConfig.frame_size = FRAMESIZE_240X240;
    InferenceUtil::cameraImageWidth = 240;
    InferenceUtil::cameraImageHeight = 240;
    cameraInit = CamConfig::initCamera();

    // Not using right now (and conflicts with pin 13, needs pullup always)
    // cameraInit = CamConfig::initSdCard();

    ConfigPageSetup::setupConfigPage();

    server.on("/inf", HTTP_GET, []()
    {
        VALIDATE_AUTH();

        if (!cameraInit)
        {
            server.send(500, "text/plain", "camera not initialized.");
            return;
        }

        String action = server.arg("action");
        uint8_t *image = nullptr;
        size_t imageLen = 0;
        InferenceUtil::InferenceOutput inference = InferenceUtil::runInference(&image, &imageLen);
        if (inference.status == 0)
        {
            if (!action.isEmpty() && InferenceUtil::shouldTrigger(inference))
            {
                ActionUtil::doAction();
            }

            server.sendHeader("x-result", InferenceUtil::currentOutput);
            server.sendHeader("x-cat", String(inference.catValue));
            server.sendHeader("x-human", String(inference.humanValue));

            uint8_t *jpegout = nullptr;
            size_t jpegLen = 0;

            bool result = fmt2jpg(image, imageLen, 96, 96, PIXFORMAT_RGB888, 200, &jpegout, &jpegLen);
            if (result)
            {
                server.send_P(200, "image/jpeg", (char*) jpegout, jpegLen);
                free(image);
                free(jpegout);
            }
            else
            {
                server.send(500, "text/plain", "Error encoding image to jpeg.");
            }
        }
        else
        {
            server.send(500, "text/plain", InferenceUtil::currentOutput);
        }
    });

    server.on("/inf", HTTP_DELETE, []()
    {
        VALIDATE_AUTH();
        inferenceOn = false;
        server.send(200);
    });

    server.on("/inf", HTTP_POST, []()
    {
        VALIDATE_AUTH();
        inferenceOn = true;
        server.send(200);
    });

    delay(1000);
    Serial.println("Started.");

    xTaskCreatePinnedToCore(
        inferenceTask,
        "inferencetask",
        8192,
        nullptr,
        3,
        nullptr,
        0);
}

void loop()
{
    ConfigPageSetup::configPageLoop();

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
}