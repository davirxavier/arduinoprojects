#include <Arduino.h>
#include <general/config_page_setup.h>
#include <general/cam_config.h>
#include <general/inference_util.h>

bool cameraInit = false;
bool timeInit = false;
unsigned long inferenceTimer = 0;
unsigned long inferenceDelay = 50;

void setup()
{
    Serial.begin(115200);
    camConfig.frame_size = FRAMESIZE_240X240;
    cameraInit = CamConfig::initCamera();
    cameraInit = CamConfig::initSdCard();

    ConfigPageSetup::setupConfigPage();

    server.on("/inf", HTTP_GET, []()
    {
        VALIDATE_AUTH();

        if (!cameraInit)
        {
            server.send(500, "text/plain", "camera not initialized.");
            return;
        }

        uint8_t *image = nullptr;
        size_t imageLen = 0;
        InferenceUtil::InferenceOutput inference = InferenceUtil::runInference(image, imageLen);

        if (inference.status == 0)
        {
            if (!server.arg("action").isEmpty() && InferenceUtil::shouldTrigger(inference))
            {
                ActionUtil::doAction();
            }

            server.sendHeader("x-result", InferenceUtil::currentOutput);
            server.sendHeader("x-cat", String(inference.catValue));
            server.sendHeader("x-human", String(inference.humanValue));

            uint8_t *jpegout = nullptr;
            size_t jpegLen = 0;

            fmt2jpg(image, imageLen, 96, 96, PIXFORMAT_RGB888, 200, &jpegout, &jpegLen);
            server.send_P(200, "image/jpeg", (char*) jpegout, jpegLen);

            free(image);
        }
        else
        {
            server.send(500, "text/plain", InferenceUtil::currentOutput);
        }
    });

    delay(1000);
    Serial.println("Started.");
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

    // if (!cameraInit)
    // {
    //     return;
    // }
    //
    // if (millis() - inferenceTimer > inferenceDelay)
    // {
    //     runInference();
    //     inferenceTimer = millis();
    // }
}