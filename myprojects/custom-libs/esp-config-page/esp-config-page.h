//
// Created by xav on 7/25/24.
//

#ifndef DX_ESP_CONFIG_PAGE_H
#define DX_ESP_CONFIG_PAGE_H

#include <Arduino.h>
#include "ESP8266WebServer.h"
#include "config-html.h"

namespace ESP_CONFIG_PAGE {

    enum REQUEST_TYPE {
        CONFIG_PAGE,
        SAVE,
        CUSTOM_ACTIONS
    };

    struct EnvVar {
        const String key;
        String value;
    };

    struct CustomAction {
        const String key;
        std::function<void(void)> handler;
    };

    EnvVar **envVars;
    uint8_t envVarCount;
    uint8_t maxEnvVars;

    CustomAction **customActions;
    uint8_t customActionsCount;
    uint8_t maxCustomActions;

    String name = "ESP";
    void (*saveEnvVarsCallback)(EnvVar **envVars, uint8_t envVarCount);

    String getValueSplit(String data, char separator, int index)
    {
        int found = 0;
        int strIndex[] = {0, -1};
        int maxIndex = data.length()-1;

        for(int i=0; i<=maxIndex && found<=index; i++){
            if(data.charAt(i)==separator || i==maxIndex){
                found++;
                strIndex[0] = strIndex[1]+1;
                strIndex[1] = (i == maxIndex) ? i+1 : i;
            }
        }

        return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
    }

    void handleRequest(ESP8266WebServer &server, String username, String password, REQUEST_TYPE reqType);

    bool handleLogin(ESP8266WebServer &server, String username, String password);

    void setup(ESP8266WebServer &server, String username, String password, String nodeName) {
        customActionsCount = 0;
        maxCustomActions = 0;
        envVarCount = 0;
        maxEnvVars = 0;
        saveEnvVarsCallback = NULL;
        name = nodeName;

        server.on(F("/config"), HTTP_GET, [&server, username, password]() {
            handleRequest(server, username, password, CONFIG_PAGE);
        });

        server.on(F("/config/save"), HTTP_POST, [&server, username, password]() {
            handleRequest(server, username, password, SAVE);
        });

        server.on(F("/config/customa"), HTTP_POST, [&server, username, password]() {
            handleRequest(server, username, password, CUSTOM_ACTIONS);
        });
    }

    void addEnvVar(String key, String initialValue) {
        if (envVarCount + 1 > maxEnvVars) {
            maxEnvVars = maxEnvVars == 0 ? 1 : ceil(maxEnvVars * 1.5);
            envVars = (EnvVar**) realloc(envVars, sizeof(EnvVar*) * maxEnvVars);
        }

        envVars[envVarCount] = new EnvVar{key, initialValue};
        envVarCount++;
    }

    void addCustomAction(String key, std::function<void(void)> handler) {
        if (customActionsCount + 1 > maxCustomActions) {
            maxCustomActions = maxCustomActions == 0 ? 1 : ceil(maxCustomActions * 1.5);
            customActions = (CustomAction**) realloc(customActions, sizeof(CustomAction*) * maxCustomActions);
        }

        customActions[customActionsCount] = new CustomAction{key, handler};
        customActionsCount++;
    }

    String buildPage() {
        String html = ESP_CONFIG_HTML;

        String customActionsStr;
        for (int i = 0; i < customActionsCount; i++) {
            customActionsStr += "<button onclick=\"customAction('" + customActions[i]->key + "')\">" + customActions[i]->key + "</button>";
        }

        String envVarsStr;
        for (int i = 0; i < envVarCount; i++) {
            EnvVar *ev = envVars[i];

            envVarsStr += "<div style=\"display: flex; align-items: center; justify-content: space-between;\">"
                          "<label style=\"margin-right: 1rem\">" + ev->key + "</label>"
                          "<input id=\"input-" + ev->key + "\" class=\"env-input\" style=\"flex: 1\" value=\"" + ev->value + "\">"
                          "</div>";
        }

        html.replace(F("{{custom-actions}}"), customActionsStr);
        html.replace(F("{{env}}"), envVarsStr);
        html.replace(F("{{name}}"), name);
        html.replace(F("{{mac}}"), WiFi.macAddress());

        return html;
    }

    inline void handleRequest(ESP8266WebServer &server, String username, String password, REQUEST_TYPE reqType) {
        if (!server.authenticate(username.c_str(), password.c_str())) {
            delay(1500);
            server.requestAuthentication();
            return;
        }

        switch (reqType) {
            case CONFIG_PAGE:
                server.send(200, F("text/html"), buildPage());
                break;
            case CUSTOM_ACTIONS: {
                if (customActionsCount == 0) {
                    return;
                }

                CustomAction *ca = NULL;
                String body = server.arg(F("plain"));

                for (uint8_t i = 0; i < customActionsCount; i++) {
                    if (String(customActions[i]->key) == body) {
                        ca = customActions[i];
                    }
                }

                if (ca != NULL) {
                    ca->handler();
                }
                break;
            }
            case SAVE:
                if (envVarCount == 0) {
                    return;
                }

                String body = server.arg(F("plain"));

                uint8_t delimiterCount = 0;
                for (const char c : body) {
                    if (c == ';') {
                        delimiterCount++;
                    }
                }

                for (uint8_t i = 0; i < delimiterCount; i++) {
                    String keyAndValue = getValueSplit(body, ';', i);
                    String key = getValueSplit(keyAndValue, ':', 0);
                    String newValue = getValueSplit(keyAndValue, ':', 1);
                    newValue = newValue.substring(0, newValue.length()-1);

                    for (uint8_t j = 0; j < envVarCount; j++) {
                        EnvVar *ev = envVars[j];
                        if (ev->key.equals(key)) {
                            ev->value = newValue;
                        }
                    }
                }

                if (saveEnvVarsCallback != NULL) {
                    saveEnvVarsCallback(envVars, envVarCount);
                }
                break;
        }
    }

}

#endif //DX_ESP_CONFIG_PAGE_H
