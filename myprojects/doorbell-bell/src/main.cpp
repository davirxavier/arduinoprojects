#include <Arduino.h>
#include "esp-config-page.h"

#define AP_NAME "ESP-DOORBELL-BELL"
#define AP_PASSWORD "admin13246"

#define RING_PIN 2
#define RING_MILLIS 1500

ESP8266WebServer server(80);
bool isRinging = false;
unsigned long ringingStart = 0;

void(* resetFunc) (void) = 0;

void IRAM_ATTR resetConfig() {
    WiFi.disconnect(false, true);
    ESP.eraseConfig();

    delay(200);
    resetFunc();
}

void setup(void) {
    Serial.begin(115200);

    pinMode(RING_PIN, OUTPUT);
    digitalWrite(RING_PIN, LOW);

    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    ESP_CONFIG_PAGE::setAPConfig(AP_NAME, AP_PASSWORD);
    ESP_CONFIG_PAGE::dnsName = "esp-doorbell-bell.local";
    ESP_CONFIG_PAGE::tryConnectWifi(false);

    auto *userEv = new ESP_CONFIG_PAGE::EnvVar{"BELL_USER", ""};
    auto *passEv = new ESP_CONFIG_PAGE::EnvVar{"BELL_PASSWORD", ""};

    ESP_CONFIG_PAGE::addEnvVar(userEv);
    ESP_CONFIG_PAGE::addEnvVar(passEv);
    auto *storage = new ESP_CONFIG_PAGE::LittleFSEnvVarStorage("env2.txt");
    ESP_CONFIG_PAGE::setAndUpdateEnvVarStorage(storage);

    ESP_CONFIG_PAGE::setup(server, userEv->value, passEv->value, "ESP-DOORBELL-BELL");

    ESP_CONFIG_PAGE::addCustomAction("Reset wireless settings", [](ESP8266WebServer &server) {
        server.send(200);
        resetConfig();
    });
    ESP_CONFIG_PAGE::addCustomAction("Ring", [](ESP8266WebServer &server) {
        isRinging = true;
        ringingStart = millis();
        digitalWrite(RING_PIN, HIGH);
        server.send(200);
    });

    server.on("/ring", HTTP_POST, [userEv, passEv]() {
        String body = server.arg("plain");

        if (body.equals(userEv->value + ";" + passEv->value)) {
            isRinging = true;
            ringingStart = millis();
            digitalWrite(RING_PIN, HIGH);
            server.send(200, "text/plain", "OK");
        } else {
            server.send(401, "text/plain", "INCORRECT AUTH");
        }
    });

    server.begin();
}

void loop(void) {
    server.handleClient();
    ESP_CONFIG_PAGE::loop();

    if (isRinging && millis() - ringingStart > RING_MILLIS) {
        isRinging = false;
        digitalWrite(RING_PIN, LOW);
    }
}