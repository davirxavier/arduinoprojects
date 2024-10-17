//
// Created by xav on 7/25/24.
//

#ifndef DX_ESP_CONFIG_PAGE_H
#define DX_ESP_CONFIG_PAGE_H

#include <Arduino.h>
#include "ESP8266WebServer.h"
#include "config-html.h"
#include "LittleFS.h"
#include "WiFiUdp.h"

namespace ESP_CONFIG_PAGE {

    enum REQUEST_TYPE {
        CONFIG_PAGE,
        SAVE,
        CUSTOM_ACTIONS,
        FILES,
        DOWNLOAD_FILE,
        DELETE_FILE,
        OTA_END,
        OTA_WRITE_FIRMWARE,
        OTA_WRITE_FILESYSTEM,
        INFO
    };

    struct EnvVar {
        const String key;
        String value;
    };

    class EnvVarStorage {
    public:
        EnvVarStorage() {}
        virtual void saveVars(EnvVar **envVars, uint8_t count);
        virtual uint8_t countVars();
        virtual void recoverVars(EnvVar *envVars[]);
    };

    struct CustomAction {
        const String key;
        std::function<void(ESP8266WebServer &server)> handler;
    };

    EnvVar **envVars;
    uint8_t envVarCount = 0;
    uint8_t maxEnvVars = 0;

    CustomAction **customActions;
    uint8_t customActionsCount;
    uint8_t maxCustomActions;

    String name;
    void (*saveEnvVarsCallback)(EnvVar **envVars, uint8_t envVarCount) = NULL;
    EnvVarStorage *envVarStorage = NULL;

    const char escapeChars[] = {':', ';', '+', '\0'};
    const char escaper = '|';

    int sizeWithEscaping(const char *str) {
        int len = strlen(str);
        int escapedCount = 0;

        for (int i = 0; i < len; i++) {
            const char c = str[i];
            if (strchr(escapeChars, c)) {
                escapedCount++;
            }
        }

        return escapedCount + len + 1;
    }

    int caSize() {
        int infoSize = 0;

        for (uint8_t i = 0; i < customActionsCount; i++) {
            CustomAction *ca = customActions[i];
            infoSize += sizeWithEscaping(ca->key.c_str()) + 4;
        }

        return infoSize;
    }

    int envSize() {
        int infoSize = 0;

        for (uint8_t i = 0; i < envVarCount; i++) {
            EnvVar *ev = envVars[i];
            infoSize += sizeWithEscaping(ev->key.c_str()) + sizeWithEscaping(ev->value.c_str()) + 4;
        }

        return infoSize;
    }

    void unescape(char buf[], const char *source) {
        unsigned int len = strlen(source);
        bool escaped = false;
        int bufIndex = 0;

        for (unsigned int i = 0; i < len; i++) {
            const char c = source[i];

            if (c == escaper && !escaped) {
                escaped = true;
            } else if (strchr(escapeChars, c) && !escaped) {
                buf[bufIndex] = c;
                bufIndex++;
            } else {
                escaped = false;
                buf[bufIndex] = c;
                bufIndex++;
            }
        }

        buf[bufIndex] = '\0';
    }

    void escape(char buf[], const char *source) {
        int len = strlen(source);
        int bufIndex = 0;

        for (int i = 0; i < len; i++) {
            if (strchr(escapeChars, source[i])) {
                buf[bufIndex] = escaper;
                bufIndex++;
            }

            buf[bufIndex] = source[i];
            bufIndex++;
        }

        buf[bufIndex] = '\0';
    }

    uint8_t countChar(const char str[], const char separator) {
        uint8_t delimiterCount = 0;
        bool escaped = false;
        int len = strlen(str);

        for (int i = 0; i < len; i++) {
            char cstr = str[i];
            if (cstr == escaper && !escaped) {
              escaped = true;
            } else if (cstr == separator && !escaped) {
              delimiterCount++;
            } else {
              escaped = false;
            }
        }

        return delimiterCount;
    }

    void getValueSplit(const char data[], char separator, std::shared_ptr<char[]> ret[])
    {
        uint8_t currentStr = 0;
        int lastIndex = 0;
        bool escaped = false;
        unsigned int len = strlen(data);

        for (unsigned int i = 0; i < len; i++) {
            const char c = data[i];

            if (c == '|' && !escaped) {
                escaped = true;
            } else if (c == separator && !escaped) {
                int newLen = i - lastIndex + 1;
                ret[currentStr] = std::make_unique<char[]>(newLen);

                strncpy(ret[currentStr].get(), data + lastIndex, newLen-1);
                ret[currentStr].get()[newLen-1] = '\0';

                lastIndex = i+1;
                currentStr++;
            } else {
                escaped = false;
            }
        }
    }

    void handleRequest(ESP8266WebServer &server, String username, String password, REQUEST_TYPE reqType);

    bool handleLogin(ESP8266WebServer &server, String username, String password);

    /**
     * @param server
     * @param username
     * @param password
     * @param nodeName - DOT NOT USE THE CHARACTERS ":", ";" or "+"
     */
    void setup(ESP8266WebServer &server, String username, String password, String nodeName) {
        LittleFS.begin();

        customActionsCount = 0;
        maxCustomActions = 0;
        name = nodeName;

        server.on(F("/config"), HTTP_GET, [&server, username, password]() {
            handleRequest(server, username, password, CONFIG_PAGE);
        });

        server.on(F("/config/info"), HTTP_GET, [&server, username, password]() {
            handleRequest(server, username, password, INFO);
        });

        server.on(F("/config/save"), HTTP_POST, [&server, username, password]() {
            handleRequest(server, username, password, SAVE);
        });

        server.on(F("/config/customa"), HTTP_POST, [&server, username, password]() {
            handleRequest(server, username, password, CUSTOM_ACTIONS);
        });

        server.on(F("/config/files"), HTTP_POST, [&server, username, password]() {
            handleRequest(server, username, password, FILES);
        });

        server.on(F("/config/files/download"), HTTP_POST, [&server, username, password]() {
            handleRequest(server, username, password, DOWNLOAD_FILE);
        });

        server.on(F("/config/files/delete"), HTTP_POST, [&server, username, password]() {
            handleRequest(server, username, password, DELETE_FILE);
        });

        server.on(F("/config/update/firmware"), HTTP_POST, [&server, username, password]() {
            handleRequest(server, username, password, OTA_END);
        }, [&server, username, password]() {
            handleRequest(server, username, password, OTA_WRITE_FIRMWARE);
        });

        server.on(F("/config/update/filesystem"), HTTP_POST, [&server, username, password]() {
            handleRequest(server, username, password, OTA_END);
        }, [&server, username, password]() {
            handleRequest(server, username, password, OTA_WRITE_FILESYSTEM);
        });
    }

    void setAndUpdateEnvVarStorage(EnvVarStorage *storage) {
        envVarStorage = storage;

        if (envVarStorage != NULL) {
            LittleFS.begin();

            uint8_t count = envVarStorage->countVars();

            if (count > 0) {
                EnvVar *recovered[count];
                envVarStorage->recoverVars(recovered);

                for (uint8_t i = 0; i < count; i++) {
                    EnvVar *ev = recovered[i];
                    if (ev == NULL) {
                        break;
                    }

                    for (uint8_t j = 0; j < envVarCount; j++) {
                        EnvVar *masterEv = envVars[j];
                        if (masterEv == NULL) {
                            break;
                        }

                        if (masterEv->key == ev->key) {
                            masterEv->value = ev->value;
                        }
                    }

                    free(ev);
                }
            }
        }
    }

    void addEnvVar(EnvVar *ev) {
        if (envVarCount + 1 > maxEnvVars) {
            maxEnvVars = maxEnvVars == 0 ? 1 : ceil(maxEnvVars * 1.5);
            envVars = (EnvVar**) realloc(envVars, sizeof(EnvVar*) * maxEnvVars);
        }

        envVars[envVarCount] = ev;
        envVarCount++;
    }

    void addCustomAction(String key, std::function<void(ESP8266WebServer &server)> handler) {
        if (customActionsCount + 1 > maxCustomActions) {
            maxCustomActions = maxCustomActions == 0 ? 1 : ceil(maxCustomActions * 1.5);
            customActions = (CustomAction**) realloc(customActions, sizeof(CustomAction*) * maxCustomActions);
        }

        customActions[customActionsCount] = new CustomAction{key, handler};
        customActionsCount++;
    }

    void ota(ESP8266WebServer &server, String username, String password, REQUEST_TYPE reqType) {
        HTTPUpload& upload = server.upload();
        int command = U_FLASH;
        if (reqType == OTA_WRITE_FILESYSTEM) {
            command = U_FS;
        }

        if (upload.status == UPLOAD_FILE_START) {
            WiFiUDP::stopAll();

            uint32_t maxSpace = 0;
            if (command == U_FLASH) {
                maxSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
            } else {
                maxSpace = FS_end - FS_start;
            }

            Update.runAsync(true);
            if (!Update.begin(maxSpace, command)) {  // start with max available size
                server.send(400, "text/plain", Update.getErrorString());
            }

        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                server.send(400, "text/plain", Update.getErrorString());
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) {  // true to set the size to the current progress
                server.send(200);
            } else {
                server.send(400, "text/plain", Update.getErrorString());
            }
        }
        yield();
    }

    inline void handleRequest(ESP8266WebServer &server, String username, String password, REQUEST_TYPE reqType) {
        if (!server.authenticate(username.c_str(), password.c_str())) {
            delay(1500);
            server.requestAuthentication();
            return;
        }

        switch (reqType) {
            case CONFIG_PAGE:
                server.send(200, F("text/html"), ESP_CONFIG_HTML);
                break;
            case FILES: {
                String path = server.arg("plain");
                if (path.isEmpty()) {
                    path = "/";
                }

                String ret;

                Dir dir = LittleFS.openDir(path);
                while (dir.next()) {
                    ret += dir.fileName() + ":" + (dir.isDirectory() ? "true" : "false") + ":" + dir.fileSize() + ";";
                }

                server.send(200, "text/plain", ret);
                break;
            }
            case DOWNLOAD_FILE: {
                String path = server.arg("plain");
                if (path.isEmpty()) {
                    server.send(404);
                    return;
                }

                if (!LittleFS.exists(path)) {
                    server.send(404);
                    return;
                }

                File file = LittleFS.open(path, "r");
                server.sendHeader("Content-Disposition", file.name());
                server.streamFile(file, "text");
                file.close();
                break;
            }
            case DELETE_FILE: {
                String path = server.arg("plain");
                if (path.isEmpty()) {
                    server.send(404);
                    return;
                }

                if (!LittleFS.exists(path)) {
                    server.send(404);
                    return;
                }

                LittleFS.remove(path);
                server.send(200);
                break;
            }
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
                    ca->handler(server);
                } else {
                    server.send(200);
                }
                break;
            }
            case SAVE: {
                if (envVarCount == 0) {
                    server.send(200);
                    return;
                }

                String body = server.arg("plain");

                uint8_t size = countChar(body.c_str(), ';');
                std::shared_ptr<char[]> split[size];
                getValueSplit(body.c_str(), ';', split);

                for (uint8_t i = 0; i < size; i++) {
                    uint8_t keyAndValueSize = countChar(split[i].get(), ':');

                    if (keyAndValueSize < 2) {
                        continue;
                    }

                    std::shared_ptr<char[]> keyAndValue[keyAndValueSize];
                    getValueSplit(split[i].get(), ':', keyAndValue);

                    char key[strlen(keyAndValue[0].get())];
                    char newValue[strlen(keyAndValue[1].get())];

                    unescape(key, keyAndValue[0].get());
                    unescape(newValue, keyAndValue[1].get());

                    for (uint8_t j = 0; j < envVarCount; j++) {
                        EnvVar *ev = envVars[j];
                        if (ev == NULL) {
                            break;
                        }

                        if (ev->key == key) {
                            ev->value = newValue;
                            break;
                        }
                    }
                }

                if (saveEnvVarsCallback != NULL) {
                    saveEnvVarsCallback(envVars, envVarCount);
                }

                if (envVarStorage != NULL) {
                    envVarStorage->saveVars(envVars, envVarCount);
                }

                server.send(200);
                delay(100);
                ESP.reset();
                break;
            }
            case OTA_END: {
                server.sendHeader("Connection", "close");
                server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
                delay(100);
                ESP.restart();
                break;
            }
            case OTA_WRITE_FIRMWARE: {
                ota(server, username, password, OTA_WRITE_FIRMWARE);
                break;
            }
            case OTA_WRITE_FILESYSTEM: {
                ota(server, username, password, OTA_WRITE_FILESYSTEM);
                break;
            }
            case INFO: {
                FSInfo fsInfo;
                LittleFS.info(fsInfo);
                String usedBytes = String(fsInfo.usedBytes);
                String totalBytes = String(fsInfo.totalBytes);
                String freeHeap = String(ESP.getFreeHeap());

                int nameLen = name.length();
                int infoSize = nameLen + WiFi.macAddress().length() + usedBytes.length() + totalBytes.length() + freeHeap.length() + 64;
                infoSize += envSize() + caSize();

                char buf[infoSize];

                strcpy(buf, name.c_str());
                strcat(buf, "+");
                strcat(buf, WiFi.macAddress().c_str());
                strcat(buf, "+");
                strcat(buf, usedBytes.c_str());
                strcat(buf, "+");
                strcat(buf, totalBytes.c_str());
                strcat(buf, "+");
                strcat(buf, freeHeap.c_str());
                strcat(buf, "+");

                for (uint8_t i = 0; i < envVarCount; i++) {
                    EnvVar *ev = envVars[i];

                    char bufKey[sizeWithEscaping(ev->key.c_str())];
                    char bufVal[sizeWithEscaping(ev->value.c_str())];

                    escape(bufKey, ev->key.c_str());
                    escape(bufVal, ev->value.c_str());

                    strcat(buf, bufKey);
                    strcat(buf, ":");
                    strcat(buf, bufVal);
                    strcat(buf, ":;");
                }
                strcat(buf, "+");

                for (uint8_t i = 0; i < customActionsCount; i++) {
                    CustomAction *ca = customActions[i];

                    char bufKey[sizeWithEscaping(ca->key.c_str())];
                    escape(bufKey, ca->key.c_str());

                    strcat(buf, bufKey);
                    strcat(buf, ";");
                }
                strcat(buf, "+");

                server.send(200, "text/plain", buf);
                break;
            }
        }
    }

    class LittleFSEnvVarStorage : public EnvVarStorage {
    public:
        LittleFSEnvVarStorage(const String filePath) : filePath(filePath) {};

        void saveVars(ESP_CONFIG_PAGE::EnvVar **toStore, uint8_t count) override {
            int size = envSize() + 32;
            char buf[size];
            strcpy(buf, "");

            for (uint8_t i = 0; i < count; i++) {
                EnvVar *ev = toStore[i];
                if (ev == NULL) {
                    break;
                }

                char bufKey[sizeWithEscaping(ev->key.c_str())];
                char bufVal[sizeWithEscaping(ev->value.c_str())];

                escape(bufKey, ev->key.c_str());
                escape(bufVal, ev->value.c_str());

                strcat(buf, bufKey);
                strcat(buf, ":");
                strcat(buf, bufVal);
                strcat(buf, ":;");
            }

            File file = LittleFS.open(filePath, "w");
            file.write(buf);
            file.close();
        }

        uint8_t countVars() override {
            if (!LittleFS.exists(filePath)) {
                return 0;
            }

            File file = LittleFS.open(filePath, "r");
            String content = file.readString();
            file.close();

            return ESP_CONFIG_PAGE::countChar(content.c_str(), ';');
        }

        void recoverVars(ESP_CONFIG_PAGE::EnvVar *recovered[]) override {
            if (!LittleFS.exists(filePath)) {
                return;
            }

            File file = LittleFS.open(filePath, "r");
            String content = file.readString();

            uint8_t count = countChar(content.c_str(), ';');
            std::shared_ptr<char[]> split[count];
            getValueSplit(content.c_str(), ';', split);

            for (uint8_t i = 0; i < count; i++) {
                uint8_t varStrCount = countChar(split[i].get(), ':');

                if (varStrCount < 2) {
                    continue;
                }

                std::shared_ptr<char[]> varStr[varStrCount];
                getValueSplit(split[i].get(), ':', varStr);

                char key[strlen(varStr[0].get())];
                char val[strlen(varStr[1].get())];

                unescape(key, varStr[0].get());
                unescape(val, varStr[1].get());

                recovered[i] = new EnvVar{String(key), String(val)};
            }

            file.close();
        }

    private:
        const String filePath;
    };

}

#endif //DX_ESP_CONFIG_PAGE_H
