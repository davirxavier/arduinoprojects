//
// Created by xav on 10/23/24.
//

#ifndef MOTOR_REMOTE_SENSOR_SHARED_H
#define MOTOR_REMOTE_SENSOR_SHARED_H

#include "Arduino.h"
#include "espnow.h"

#define CONFIGURATION_DONE_FILE "configdone"

namespace MotorCommand {
    enum MotorCommands {
        HEALTH_PING = 52,
        TURN_OFF = 54,
        TURN_ON = 55,
        ACK = 56,
    };
}

ESP8266WebServer server(80);
auto usernameEnvVar = new ESP_CONFIG_PAGE::EnvVar("USERNAME", "");
auto passwordEnvVar = new ESP_CONFIG_PAGE::EnvVar("PASSWORD", "");
auto macAddrEnvVar = new ESP_CONFIG_PAGE::EnvVar("PEER_MAC_ADDRESS", "");
auto pmkVar = new ESP_CONFIG_PAGE::EnvVar("ENC_PMK", "");
auto lmkVar = new ESP_CONFIG_PAGE::EnvVar("ENC_LMK", "");
uint8_t peerMacAddr[6];
std::function<void(uint8_t *mac, uint8_t status)> dataSentCb;
std::function<void(uint8_t command, uint8_t extra, uint8_t *mac)> dataRecvCb;

bool parseMacAddr(char str[18], uint8_t resultAddr[6]) {
    unsigned int bytes[6];
    int i;

    if(6 == sscanf(str, "%x:%x:%x:%x:%x:%x%*c",
                   &bytes[0], &bytes[1], &bytes[2],
                   &bytes[3], &bytes[4], &bytes[5])) {
        for(i = 0; i < 6; ++i) {
            resultAddr[i] = (uint8_t) bytes[i];
        }

        return true;
    }

    return false;
}

void changeMode(bool espNowMode) {
    if (espNowMode) {
        LOGN("Changed mode to esp-now mode.");
        File f = LittleFS.open(CONFIGURATION_DONE_FILE, "w");
        f.write(";");
        f.close();
    } else if (LittleFS.exists(CONFIGURATION_DONE_FILE)) {
        LOGN("Changed mode to config mode.");
        LittleFS.remove(CONFIGURATION_DONE_FILE);
    }

    delay(150);
    ESP.reset();
}

bool shouldRunConfigPage() {
    return !LittleFS.exists(CONFIGURATION_DONE_FILE);
}

bool initConfigPage(bool isMaster) {
    LOGN("Configuration done file not found, starting ap mode with config page.");

    ESP_CONFIG_PAGE::addEnvVar(usernameEnvVar);
    ESP_CONFIG_PAGE::addEnvVar(passwordEnvVar);
    ESP_CONFIG_PAGE::addEnvVar(macAddrEnvVar);
    ESP_CONFIG_PAGE::addEnvVar(pmkVar);
    ESP_CONFIG_PAGE::addEnvVar(lmkVar);

    auto envVarStorage = new ESP_CONFIG_PAGE::LittleFSEnvVarStorage("env.store");
    ESP_CONFIG_PAGE::setAndUpdateEnvVarStorage(envVarStorage);

    if (shouldRunConfigPage()) {
        WiFi.mode(WIFI_AP);
        String apName = "MOTOR_CONTROLLER_MASTER";

        IPAddress apIp(192, 168, 1, 1);
        WiFi.softAPConfig(apIp, apIp, IPAddress(255, 255, 255, 0));
        WiFi.softAP(apName, passwordEnvVar->value);

        ESP_CONFIG_PAGE::setup(server, usernameEnvVar->value, passwordEnvVar->value, apName);

        ESP_CONFIG_PAGE::addCustomAction("Finish config (disable AP mode)", [](ESP8266WebServer &server) {
            LOGN("Finish config event");
            changeMode(true);
        });

        server.begin();
    } else {
        char buf[18];
        strncpy(buf, macAddrEnvVar->value.c_str(), 17);
        buf[17] = '\0';
        bool macAddrParsed = parseMacAddr(buf, peerMacAddr);

        if (!macAddrParsed) {
            LOGN("Invalid peer mac address, returning.");
            changeMode(false);
            return false;
        }

        if (pmkVar->value.isEmpty() || lmkVar->value.isEmpty()) {
            LOGN("Invalid encryption keys, plase set them again");
            changeMode(false);
            return false;
        }

        if (esp_now_init() != 0) {
            LOGN("Error initializing ESP-NOW");
            return false;
        }

        LOGN("ESP-NOW initialized.");

        esp_now_set_self_role(isMaster ? ESP_NOW_ROLE_CONTROLLER : ESP_NOW_ROLE_SLAVE);
        esp_now_set_kok((uint8 *) pmkVar->value.c_str(), pmkVar->value.length());

        esp_now_add_peer(peerMacAddr,
                         isMaster ? ESP_NOW_ROLE_SLAVE : ESP_NOW_ROLE_CONTROLLER,
                         1,
                         (uint8_t *) lmkVar->value.c_str(),
                         lmkVar->value.length());

        esp_now_register_send_cb([](u8 *mac_addr, u8 status) {
            LOGF("Message to address %02x:%02x:%02x:%02x:%02x:%02x sent, status: %s", mac_addr[0],
                 mac_addr[1],
                 mac_addr[2],
                 mac_addr[3],
                 mac_addr[4],
                 mac_addr[5],
                 status == 0 ? "Success" : "Fail");

            dataSentCb(mac_addr, status);
        });

        esp_now_register_recv_cb([](u8 *mac_addr, u8 *data, u8 len) {
            LOGF("Received message from address %02x:%02x:%02x:%02x:%02x:%02x of size %d", mac_addr[0],
                 mac_addr[1],
                 mac_addr[2],
                 mac_addr[3],
                 mac_addr[4],
                 mac_addr[5],
                 len);

            if (len < 1) {
                LOGN("Data is invalid, ignoring.");
                return;
            }

            uint8_t command = data[0];
            uint8_t extra = data[1];

            LOGF("Command received is %d, extra info is %d.", command, extra);
            dataRecvCb(command, extra, mac_addr);
        });
    }

    return true;
}

uint8_t sendCommand(MotorCommand::MotorCommands c, const uint8_t extraInfo[], uint8_t extraInfoLength) {
    uint8_t buf[extraInfoLength+1];
    buf[0] = c;

    if (extraInfoLength > 0) {
        memcpy(buf+1, extraInfo, extraInfoLength);
    }

    return esp_now_send(peerMacAddr, buf, extraInfoLength+1);
}

uint8_t sendCommand(MotorCommand::MotorCommands c) {
    return sendCommand(c, NULL, 0);
}

#endif //MOTOR_REMOTE_SENSOR_SHARED_H
