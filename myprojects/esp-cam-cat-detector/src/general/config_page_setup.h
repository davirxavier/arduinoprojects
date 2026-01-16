//
// Created by xav on 12/4/25.
//

// #define ESP32_CONP_OTA_USE_WEBSOCKETS
#define ESP_CONFIG_PAGE_ENABLE_LOGGING

#ifndef CONFIG_PAGE_SETUP_H
#define CONFIG_PAGE_SETUP_H

#include <WebSocketsServer.h>
#include <esp-config-page.h>
// #include <FTPFilesystem.h>
// #include <ESP-FTP-Server-Lib.h>
#include <general/secrets.h>
#include <general/cam_config.h>

// FTPServer ftp;
WebServer server(8080);

namespace ConfigPageSetup
{
    inline int frameIntervalMs = (1000 / 1);
    inline unsigned long lastFrameSent = 0;
    inline WiFiClient currentClient;
    inline bool streamActive = false;

    inline void mjpegStreamHandle()
    {
        server.on("/stream", []()
        {
            VALIDATE_AUTH();

            MLOGN("Stream connection received");
            if (streamActive)
            {
                MLOGN("Stream already in use");
                server.send(503, "text/plain", "stream already in use.");
                return;
            }

            streamActive = true;
            currentClient = server.client();
            currentClient.setNoDelay(true);

            currentClient.print(
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
                "Cache-Control: no-cache\r\n"
                "Connection: keep-alive\r\n\r\n"
            );
            MLOGN("Stream started");
        });
    }

    inline void setupConfigPage()
    {
        ESP_CONFIG_PAGE::addCustomAction("RESTART", [](ESP_CONFIG_PAGE::WEBSERVER_T &server)
        {
            server.send(200);
            ESP.restart();
        });

        ESP_CONFIG_PAGE::addCustomAction("TEST", [](ESP_CONFIG_PAGE::WEBSERVER_T &server)
        {
            server.send(200);
            testRun();
        });

        ESP_CONFIG_PAGE::setAPConfig(nodeName, password);
        ESP_CONFIG_PAGE::initModules(&server, username, password, nodeName);

        mjpegStreamHandle();
        server.begin();

        // ftp.addUser(username, password);
        // ftp.addFilesystem("SD", &SD_MMC);
        // ftp.begin();
    }

    inline void configPageLoop()
    {
        ESP_CONFIG_PAGE::loop();
        server.handleClient();
        // ftp.handle();

        if (!streamActive)
        {
            return;
        }

        if (!currentClient.connected())
        {
            streamActive = false;
            currentClient.stop();
            MLOGN("Stream client disconnected");
            return;
        }

        if (millis() - lastFrameSent < frameIntervalMs)
        {
            return;
        }

        camera_fb_t *fb = esp_camera_fb_get();
        if (fb == nullptr)
        {
            return;
        }

        currentClient.printf(
           "--frame\r\n"
           "Content-Type: image/jpeg\r\n"
           "Content-Length: %u\r\n\r\n",
           fb->len
         );

        currentClient.write(fb->buf, fb->len);
        esp_camera_fb_return(fb);
        currentClient.print("\r\n");
        currentClient.flush();

        lastFrameSent = millis();
    }
}

#endif //CONFIG_PAGE_SETUP_H
