//
// Created by xav on 12/4/25.
//

// #define ESP32_CONP_OTA_USE_WEBSOCKETS
#define ESP_CONFIG_PAGE_ENABLE_LOGGING

#ifndef CONFIG_PAGE_SETUP_H
#define CONFIG_PAGE_SETUP_H

#include <esp-config-page.h>
#include <WebServer.h>
#include <FTPFilesystem.h>
#include <ESP-FTP-Server-Lib.h>
#include <general/secrets.h>
#include <general/cam_config.h>

FTPServer ftp;
WebServer server(8080);

namespace ConfigPageSetup
{
    inline void setupConfigPage()
    {
        ESP_CONFIG_PAGE::addCustomAction("RESTART", [](ESP_CONFIG_PAGE::WEBSERVER_T &server)
        {
            server.send(200);
            ESP.restart();
        });

        ESP_CONFIG_PAGE::setAPConfig(nodeName, password);

        ESP_CONFIG_PAGE::initModules(&server, username, password, nodeName);
        server.begin();

        ftp.addUser(username, password);
        ftp.addFilesystem("SD", &SD_MMC);
        ftp.begin();
    }

    inline void configPageLoop()
    {
        ESP_CONFIG_PAGE::loop();
        server.handleClient();
        ftp.handle();
    }
}

#endif //CONFIG_PAGE_SETUP_H
