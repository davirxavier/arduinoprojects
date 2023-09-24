#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "SinricPro.h"
#include "SinricProMotionsensor.h"
#include "Ultrasonic.h"
#include "WiFiManager.h"
#include "credentials.h"

#define DEVICE_ID "650750b9813f7a9c818a229d"
#define ULTRASONIC_T 0
#define ULTRASONIC_E 3
#define RESET_CONFIG_PIN 2
#define SENSOR_CHECK_INTERVAL_SECONDS 10
#define SENSOR_READINGS 15
#define SENSOR_DIFF_PERCENT 0.4

Ultrasonic ultrasonic(ULTRASONIC_T, ULTRASONIC_E);
WiFiManager manager;

bool isOn;

long readings[SENSOR_READINGS];
long lastMedian;
int currentReading;
unsigned long lastCheckedSensor;
bool lastState;

void(* resetFunc) (void) = 0;

void handleSensor() {
    if (!isOn) {
        return;
    }

    if (millis() - lastCheckedSensor > (SENSOR_CHECK_INTERVAL_SECONDS*1000)) {
        readings[currentReading] = ultrasonic.read(CM);
        Serial.print("Current reading: ");
        Serial.print(readings[currentReading]);
        Serial.println("CM");
        currentReading++;

        if (currentReading >= SENSOR_READINGS) {
            currentReading = 0;

            long median = 0;
            for (long reading : readings) {
                median += reading;
            }
            median = median / SENSOR_READINGS;

            SinricProMotionsensor& s = SinricPro[DEVICE_ID];
            if (lastMedian > median && (double) lastMedian - (double) median > ((double) lastMedian * SENSOR_DIFF_PERCENT)) {
                s.sendMotionEvent(true);
                lastState = true;

                Serial.println("Motion detected.");
            } else if (lastState && lastMedian < median) {
                s.sendMotionEvent(false);
                lastState = false;

                Serial.println("Motion ended.");
            }

            char buff[50];
            sprintf(buff, "Last: %ld; Current: %ld", lastMedian, median);
            Serial.println(buff);
            lastMedian = median;
        }

        lastCheckedSensor = millis();
    }
}

void setupDevice() {
    SinricProMotionsensor& s = SinricPro[DEVICE_ID];
    s.onPowerState([](const String &deviceId, bool &state) {
        isOn = state;
        return true;
    });
    s.sendPowerStateEvent(true);
    s.sendMotionEvent(false);
    isOn = true;
    lastState = false;
}

void IRAM_ATTR resetConfig() {
    Serial.println("Resetting the wifi configuration.");
    manager.resetSettings();
    ESP.eraseConfig();
    delay(200);
    resetFunc();
}

void setup() {
    Serial.begin(9600, SERIAL_8N1,SERIAL_TX_ONLY);
    pinMode(0, OUTPUT);
    digitalWrite(0, LOW);

    pinMode(RESET_CONFIG_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(RESET_CONFIG_PIN), resetConfig, FALLING);

    lastMedian = -9999999;
    currentReading = 0;

    Serial.print("Connecting");

    WiFi.setSleepMode(WIFI_NONE_SLEEP);

    manager.setDebugOutput(true);
    manager.autoConnect("DXLetterSensor", "drx123456");

    SinricPro.begin(SINRIC_API_KEY, SINRIC_SECRET);
    setupDevice();
}

void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        handleSensor();
        SinricPro.handle();
    }
}