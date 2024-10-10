#include <Arduino.h>
#include "WiFiManager.h"
#include "ElegantOTA.h"
#include "secrets.h"

#define AP_NAME "ESP-BELL-1"
#define AP_PASSWORD "admin13246"

#define RING_PIN 2
#define RING_MILLIS 1500
#define RESET_PIN 1

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
    pinMode(RING_PIN, OUTPUT);
    digitalWrite(RING_PIN, LOW);

    pinMode(RESET_PIN, INPUT_PULLUP);

    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    manager.autoConnect(AP_NAME, AP_PASSWORD);

    server.on("/ring", HTTP_POST, []() {
        isRinging = true;
        ringingStart = millis();
        digitalWrite(RING_PIN, HIGH);
        server.send(200, "text/plain", "OK");
    });

    ElegantOTA.begin(&server);
    ElegantOTA.setAuth(UPDATE_USER, UPDATE_PASS);
    server.begin();
}

void loop(void) {
    server.handleClient();
    ElegantOTA.loop();

    if (isRinging && millis() - ringingStart > RING_MILLIS) {
        isRinging = false;
        digitalWrite(RING_PIN, LOW);
    }

    if (digitalRead(RESET_PIN) == LOW) {
        resetConfig();
    }
}