#include <Arduino.h>
#include "WiFiManager.h"
#include "ElegantOTA.h"
#include "secrets.h"
#include "esp-config-page.h"

#define AP_NAME "ESP-DOORBELL-BELL"
#define AP_PASSWORD "admin13246"

#define RING_PIN 2
#define RING_MILLIS 1500

WiFiManager manager;
ESP8266WebServer server(80);
bool isRinging = false;
unsigned long ringingStart = 0;

void(* resetFunc) (void) = 0;

void IRAM_ATTR resetConfig() {
    manager.resetSettings();
    ESP.eraseConfig();

    delay(200);
    resetFunc();
}

void setup(void) {
    Serial.begin(115200);

    pinMode(RING_PIN, OUTPUT);
    digitalWrite(RING_PIN, LOW);

    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    manager.autoConnect(AP_NAME, AP_PASSWORD);

    server.on("/ring", HTTP_POST, []() {
        String body = server.arg("plain");

        if (body.equals(String(BELL_USER) + ";" + String(BELL_PASS))) {
            isRinging = true;
            ringingStart = millis();
            digitalWrite(RING_PIN, HIGH);
            server.send(200, "text/plain", "OK");
        } else {
            server.send(401, "text/plain", "INCORRECT AUTH");
        }
    });

    ElegantOTA.begin(&server);
    ElegantOTA.setAuth(BELL_USER, BELL_PASS);

    ESP_CONFIG_PAGE::setup(server, BELL_USER, BELL_PASS, "ESP-DOORBELL-BELL");
    ESP_CONFIG_PAGE::addCustomAction("Reset wireless settings", [](ESP8266WebServer &server) {
        server.send(200);
        resetConfig();
    });

    server.begin();
}

void loop(void) {
    server.handleClient();
    ElegantOTA.loop();

    if (isRinging && millis() - ringingStart > RING_MILLIS) {
        isRinging = false;
        digitalWrite(RING_PIN, LOW);
    }
}