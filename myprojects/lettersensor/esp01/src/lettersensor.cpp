#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "SinricPro.h"
#include "SinricProMotionsensor.h"
#include "Ultrasonic.h"
#include "WiFiManager.h"
#include "LittleFS.h"
#include "EEPROM.h"

#define ULTRASONIC_T 0
#define ULTRASONIC_E 3
#define RESET_CONFIG_PIN 2
#define SENSOR_READINGS 5
#define SENSOR_DIFF_PERCENT 0.4

#define VALUES_FILE "saved_values.txt"
#define CREDENTIALS_FILE "credentials.txt"

struct SensorConfig {
    unsigned short currentReadingNumber;
    bool lastState;
    unsigned int lastMedian;
    unsigned int readings[SENSOR_READINGS];
};

struct Credentials {
    char apiKey[36];
    char secretKey[73];
    char deviceId[24];
};

Ultrasonic ultrasonic(ULTRASONIC_T, ULTRASONIC_E);
WiFiManager manager;
Credentials credentials{};

bool isOn;

long readings[SENSOR_READINGS];
long lastMedian;
int currentReading;
bool lastState;
bool shouldSaveCredentials;

void(* resetFunc) (void) = 0;

void handleSensor() {
    if (!isOn) {
        return;
    }

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

        SinricProMotionsensor& s = SinricPro[credentials.deviceId];
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
}

void setupDevice() {
    SinricProMotionsensor& s = SinricPro[credentials.deviceId];
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

    if (LittleFS.exists(VALUES_FILE)) {
        LittleFS.remove(VALUES_FILE);
    }
    if (LittleFS.exists(CREDENTIALS_FILE)) {
        LittleFS.remove(CREDENTIALS_FILE);
    }

    delay(200);
    resetFunc();
}

void saveConfig() {
    SensorConfig config{};

    config.currentReadingNumber = currentReading;
    config.lastMedian = lastMedian;
    config.lastState = lastState;

    for (int i = 0; i < SENSOR_READINGS; i++) {
        config.readings[i] = readings[i];
    }

    char bytes[sizeof(SensorConfig)];
    memcpy(bytes, (const uint8_t*)&config, sizeof(SensorConfig));

    File file = LittleFS.open(VALUES_FILE, "w");
    file.write(bytes, sizeof(SensorConfig));
    file.close();
}

void recoverConfig() {
    if (!LittleFS.exists(VALUES_FILE)) {
        return;
    }

    File file = LittleFS.open(VALUES_FILE, "r");
    char bytes[file.size()];
    file.readBytes(bytes, file.size());
    file.close();

    SensorConfig config{};
    memcpy((uint8_t*) &config, bytes, sizeof(SensorConfig));

    lastState = config.lastState;
    lastMedian = config.lastMedian;
    currentReading = config.currentReadingNumber;

    for (int i = 0; i < SENSOR_READINGS; i++) {
        readings[i] = config.readings[i];
    }
}

void saveCredentials() {
    char bytes[sizeof(Credentials)];
    memcpy(bytes, (const char*)&credentials, sizeof(Credentials));

    File file = LittleFS.open(CREDENTIALS_FILE, "w");
    file.write(bytes, sizeof(Credentials));
    file.close();
}

void recoverCredentials() {
    if (!LittleFS.exists(CREDENTIALS_FILE)) {
        Serial.println("Credentials file does not exist.");
        return;
    }

    File file = LittleFS.open(CREDENTIALS_FILE, "r");
    Serial.println(file.readString());
    char bytes[file.size()];
    file.readBytes(bytes, file.size());
    memcpy((char*) &credentials, bytes, sizeof(Credentials));
    file.close();
}

void setup() {
    shouldSaveCredentials = false;

    LittleFS.begin();
    Serial.begin(9600, SERIAL_8N1,SERIAL_TX_ONLY);
    pinMode(0, OUTPUT);
    digitalWrite(0, LOW);

    pinMode(RESET_CONFIG_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(RESET_CONFIG_PIN), resetConfig, FALLING);

    lastMedian = -9999999;
    currentReading = 0;

    Serial.print("Connecting");

    WiFi.setSleepMode(WIFI_NONE_SLEEP);

    WiFiManagerParameter apiKey("apiKey", "SINRIC API KEY", NULL, 36);
    WiFiManagerParameter secretKey("secretKey", "SINRIC SECRET KEY", NULL, 73);
    WiFiManagerParameter deviceId("deviceId", "SINRIC DEVICE ID", NULL, 24);

    manager.addParameter(&apiKey);
    manager.addParameter(&secretKey);
    manager.addParameter(&deviceId);
    manager.setSaveConfigCallback([]() {
        shouldSaveCredentials = true;
    });
    manager.setDebugOutput(true);
    manager.autoConnect("DXLetterSensor", "drx123456");

    if (shouldSaveCredentials) {
        Serial.println("Saving credentials");
        strncpy(credentials.apiKey, apiKey.getValue(), 36);
        strncpy(credentials.secretKey, secretKey.getValue(), 73);
        strncpy(credentials.deviceId, secretKey.getValue(), 24);

        saveCredentials();
    } else {
        Serial.println("Recovering Credentials.");
        recoverCredentials();

        Serial.print("API KEY: ");
        Serial.println(credentials.apiKey);
    }

    SinricPro.begin(credentials.apiKey, credentials.secretKey);
    setupDevice();
    recoverConfig();
}

void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        handleSensor();
        SinricPro.handle();

        saveConfig();
        delay(5000);
//        ESP.deepSleep(0);
    }
}