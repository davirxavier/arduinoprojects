#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "SinricPro.h"
#include "SinricProSwitch.h"
#include "credentials.h"

#define DEVICE_ID "650b5d2ecf058657a9eed49b"
#define CONNECTION_TIMEOUT_SECONDS 30
#define RELAY_PIN 3

bool isConnecting;
unsigned long connectionStart;
unsigned long lastCheckedConn;

void setupDevice() {
    SinricProSwitch& s = SinricPro[DEVICE_ID];
    s.onPowerState([](const String &deviceId, bool &state) {
        digitalWrite(RELAY_PIN, HIGH);
        delay(1500);
        digitalWrite(RELAY_PIN, LOW);
        return true;
    });
    s.sendPowerStateEvent(true);
}

void setup() {
    Serial.begin(9600, SERIAL_8N1,SERIAL_TX_ONLY);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

    Serial.print("Connecting");

    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    isConnecting = true;
    connectionStart = millis();
    lastCheckedConn = millis();
}

void loop() {
    if (isConnecting && millis() - lastCheckedConn > 700) {
        wl_status_t status = WiFi.status();
        if (status == WL_CONNECTED) {
            isConnecting = false;
            SinricPro.begin(SINRIC_API_KEY, SINRIC_SECRET);

            setupDevice();
            Serial.println();
            Serial.println("CONNECTION OK");
        } else {
            Serial.print(".");

            if (millis() - connectionStart > (CONNECTION_TIMEOUT_SECONDS * 1000)) {
                isConnecting = false;
                Serial.println();
                Serial.println("Connection failed.");
            }
        }

        lastCheckedConn = millis();
    }

    if (WiFi.status() == WL_CONNECTED) {
        SinricPro.handle();
    }
}