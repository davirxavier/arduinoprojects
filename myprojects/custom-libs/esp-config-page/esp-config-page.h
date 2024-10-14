//
// Created by xav on 7/25/24.
//

#ifndef DX_ESP_CONFIG_PAGE_H
#define DX_ESP_CONFIG_PAGE_H

#include <Arduino.h>
#include "ESP8266WebServer.h"
#include "LittleFS.h"
#include "config-html.h"

namespace ESP_CONFIG_PAGE {

    enum REQUEST_TYPE {
        CONFIG_PAGE,
        RESET,
        SAVE,
        CUSTOM_ACTIONS
    };

    void handleRequest(ESP8266WebServer &server, String username, String password, REQUEST_TYPE reqType);

    bool handleLogin(ESP8266WebServer &server, String username, String password);

    void setup(ESP8266WebServer &server, String username, String password) {
        LittleFS.begin();

        server.on("/config", HTTP_GET, [&server, username, password]() {
            handleRequest(server, username, password, CONFIG_PAGE);
        });

        server.on("/config/reset", HTTP_POST, [&server, username, password]() {
            handleRequest(server, username, password, RESET);
        });

        server.on("/config/save", HTTP_POST, [&server, username, password]() {
            handleRequest(server, username, password, SAVE);
        });

        server.on("/config/customa", HTTP_POST, [&server, username, password]() {
            handleRequest(server, username, password, CUSTOM_ACTIONS);
        });
    }

    String buildPage() {
        
    }

    inline void handleRequest(ESP8266WebServer &server, String username, String password, REQUEST_TYPE reqType) {
        if (!server.authenticate(username.c_str(), password.c_str())) {
            delay(1500);
            server.requestAuthentication();
            return;
        }

        switch (reqType) {
            case CONFIG_PAGE:

                break;
            case RESET:
                break;
        }
    }

}

#endif //DX_ESP_CONFIG_PAGE_H
