//
// Created by xav on 12/4/25.
//

#define ESP32_CONP_OTA_USE_WEBSOCKETS
// #define ESP32_CONFIG_PAGE_USE_ESP_IDF_OTA
#define ESP_CONFIG_PAGE_ENABLE_LOGGING
// #define ESP_CONP_ASYNC_WEBSERVER
#define ESP_CONP_HTTPS_SERVER
#define ESP32_CONP_OTA_WS_PORT 443
#include "inference_util.h"

#ifndef CONFIG_PAGE_SETUP_H
#define CONFIG_PAGE_SETUP_H

#include <WebSocketsServer.h>
#include <esp-config-page.h>
#include <FTPFilesystem.h>
#include <ESP-FTP-Server-Lib.h>
#include <general/secrets.h>
#include <general/cam_config.h>

FTPServer ftp;

namespace ConfigPageSetup
{
    inline int maxFps = 20;
    inline framesize_t maxFramesize = FRAMESIZE_VGA;
    inline framesize_t defaultFramesize = camConfig.frame_size;
    inline framesize_t infFramesize = FRAMESIZE_96X96;

    inline int frameIntervalMs = (1000 / 1);
    inline unsigned long lastFrameSent = 0;
    inline WiFiClient currentClient;
    inline bool streamActive = false;

    inline void mjpegStreamHandle()
    {
        ESP_CONFIG_PAGE::addServerHandler("/stream", HTTP_GET, [](ESP_CONFIG_PAGE::REQUEST_T req)
        {
            MLOGN("Stream connection received");
            LOGF("%s", esp_camera_available_frames() ? "has frame" : "no frame");
            if (streamActive)
            {
                MLOGN("Stream already in use");
                ESP_CONFIG_PAGE::sendInstantResponse(ESP_CONFIG_PAGE::CONP_STATUS_CODE::BAD_REQUEST, "stream already in use.", req);
                return;
            }

            char paramBuf[128]{};
            if (ESP_CONFIG_PAGE::getParam(req, "framesize", paramBuf, sizeof(paramBuf)))
            {
                int framesize = String(paramBuf).toInt();
                if (framesize >= 0 && framesize <= maxFramesize)
                {
                    CamConfig::setRes((framesize_t)framesize);
                }
            }

            if (ESP_CONFIG_PAGE::getParam(req, "fps", paramBuf, sizeof(paramBuf)))
            {
                int fps = String(paramBuf).toInt();
                if (fps > 0 && fps <= maxFps)
                {
                    frameIntervalMs = 1000 / fps;
                }
            }

            bool inferenceActive = false;
            if (ESP_CONFIG_PAGE::getParam(req, "inf", paramBuf, sizeof(paramBuf)))
            {
                inferenceActive = true;
                CamConfig::setRes(infFramesize);
            }

            streamActive = true;

            ESP_CONFIG_PAGE::ResponseContext c{};
            ESP_CONFIG_PAGE::initResponseContext(ESP_CONFIG_PAGE::CONP_STATUS_CODE::OK, "multipart/x-mixed-replace; boundary=frame", 0, c);
            ESP_CONFIG_PAGE::startResponse(req, c);
            ESP_CONFIG_PAGE::sendHeader("cache-control", "no-cache", c);

            bool first = true;
            char headerBuf[128]{};
            while (true)
            {
                camera_fb_t* fb = esp_camera_fb_get();
                if (fb == nullptr)
                {
                    break;
                }

                uint8_t *outImg = nullptr;
                size_t outImgLen = 0;
                if (inferenceActive)
                {
                    InferenceUtil::InferenceOutput output{};
                    InferenceUtil::runInferenceFromImage(output, fb->buf, fb->len, &outImg, &outImgLen);
                    if (outImg != nullptr)
                    {
                        InferenceUtil::drawMarkers(output, outImg);
                        uint8_t *outJpeg = nullptr;
                        size_t outJpegLen = 0;
                        fmt2jpg(outImg, outImgLen, MODEL_INPUT_WIDTH, MODEL_INPUT_HEIGHT, PIXFORMAT_RGB888, 90, &outJpeg, &outJpegLen);
                        free(outImg);

                        outImg = outJpeg;
                        outImgLen = outJpegLen;
                    }
                }
                else
                {
                    outImg = fb->buf;
                    outImgLen = fb->len;
                }

                snprintf(headerBuf,
                         sizeof(headerBuf),
                         "%s--frame\r\n"
                         "Content-Type: image/jpeg\r\n"
                         "Content-Length: %u\r\n\r\n",
                         first ? "" : "\r\n",
                         outImgLen);

                ESP_CONFIG_PAGE::writeResponse(headerBuf, c);
                int written = ESP_CONFIG_PAGE::writeResponse(outImg, outImgLen, c);
                if (written < 0)
                {
                    esp_camera_fb_return(fb);
                    if (inferenceActive)
                    {
                        free(outImg);
                    }
                    break;
                }

                first = false;
                esp_camera_fb_return(fb);
                if (inferenceActive)
                {
                    free(outImg);
                }
                vTaskDelay(pdMS_TO_TICKS(frameIntervalMs));
            }

            streamActive = false;
            CamConfig::setRes(defaultFramesize);
            ESP_CONFIG_PAGE::endResponse(req, c);
            MLOGN("Stream ended");
        });
    }

    inline void setupConfigPage()
    {
        WiFi.begin();

        ESP_CONFIG_PAGE::addCustomAction("RESTART", [](ESP_CONFIG_PAGE::REQUEST_T req)
        {
            ESP_CONFIG_PAGE::sendInstantResponse(ESP_CONFIG_PAGE::CONP_STATUS_CODE::OK, "ok", req);
            ESP.restart();
        });

        ESP_CONFIG_PAGE::addCustomAction("TEST", [](ESP_CONFIG_PAGE::REQUEST_T req)
        {
            ESP_CONFIG_PAGE::sendInstantResponse(ESP_CONFIG_PAGE::CONP_STATUS_CODE::OK, "ok", req);
            testRun();
        });

        httpd_ssl_config sslConfig = HTTPD_SSL_CONFIG_DEFAULT();
        ESP_CONFIG_PAGE::setupServerConfig(&sslConfig);

        static httpd_handle_t server = nullptr;
        int res = httpd_ssl_start(&server, &sslConfig);
        if (res != ESP_OK)
        {
            LOGF("Error initializing server: 0x%x\n", res);
            return;
        }

        ESP_CONFIG_PAGE::setAPConfig(nodeName, password);
        ESP_CONFIG_PAGE::initModules(&server, username, password, nodeName);

        defaultFramesize = camConfig.frame_size;
        mjpegStreamHandle();

        ESP_CONFIG_PAGE::otaStartCallback = []()
        {
            esp_camera_deinit();
        };

        ftp.addUser(username, password);
        ftp.addFilesystem("SD", &SD_MMC);
        ftp.begin();
    }

    inline void configPageLoop()
    {
        ESP_CONFIG_PAGE::loop();
        ftp.handle();
    }
}

#endif //CONFIG_PAGE_SETUP_H