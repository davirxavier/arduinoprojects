#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <espnow.h>
#include <secrets.h>

#define BUTTON_PIN 13

#define AP_NAME "DOORBELL0842"
#define AP_PASSWORD "admin13246"

WiFiClient wifiClient;
uint8_t data[] = {1};

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

bool hasSignal;
unsigned long lastSentMessage = 0;
unsigned long lastRang = 0;

void IRAM_ATTR button_interrupt()
{
    hasSignal = true;
}

void setup() {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    delay(200);

    httpUpdater.setup(&httpServer);

    httpServer.on("/test", HTTP_GET, []()
    {
        esp_now_send(peerMac, data, 1);
    });
    httpServer.begin();

    if (esp_now_init() != 0) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
}

void loop() {
    httpServer.handleClient();

    if (digitalRead(BUTTON_PIN) == LOW) {
        hasSignal = true;
    }

    if (hasSignal && millis() - lastRang > 250) {
        if (millis() - lastSentMessage > 3000) {
            esp_now_send(peerMac, data, 1);
            lastSentMessage = millis();
        }

        lastRang = millis();
        hasSignal = false;
    }
}