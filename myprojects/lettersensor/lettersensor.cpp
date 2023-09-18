#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "SinricPro.h"
#include "SinricProMotionsensor.h"
#include "util.h"
#include "Ultrasonic.h"
#include "credentials.h"

#define DEVICE_ID "650750b9813f7a9c818a229d"
#define CONNECTION_TIMEOUT_SECONDS 15
#define ULTRASONIC_T 0
#define ULTRASONIC_E 2
#define SENSOR_CHECK_INTERVAL_SECONDS 1
#define SENSOR_READINGS 15
#define SENSOR_DIFF_PERCENT 0.5

Ultrasonic ultrasonic(ULTRASONIC_T, ULTRASONIC_E);

bool isOn;
bool isConnecting;
unsigned long connectionStart;
unsigned long lastCheckedConn;

long readings[SENSOR_READINGS];
long lastMedian;
int currentReading;
unsigned long lastCheckedSensor;
bool lastState;

void handleSensor() {
    if (!isOn) {
        return;
    }

    if (millis() - lastCheckedSensor > (SENSOR_CHECK_INTERVAL_SECONDS*1000)) {
        readings[currentReading] = ultrasonic.Ranging(CM);
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

void setup() {
    Serial.begin(9600);
    pinMode(0, OUTPUT);
    digitalWrite(0, LOW);

    lastMedian = -9999999;
    currentReading = 0;

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
        handleSensor();
        SinricPro.handle();
    }
}