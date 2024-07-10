#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include "webServer.h"
#include "fetch.h"
#include "timeSync.h"
#include "secrets.h"

#define USE_WIFI

#ifndef USE_WIFI
#define TINY_GSM_MODEM_SIM800
#include <TinyGsmClient.h>
#include <SoftwareSerial.h>
SoftwareSerial SerialAT(14, 12);  // RX, TX

TinyGsm modem(SerialAT);
TinyGsmClient gsm_client(modem);
#endif

#define ENABLE_LOGGING
#define INTERVAL_US 20e6

bool ran;
unsigned long runCount;

void writeRunCount() {
#ifdef ENABLE_LOGGING
    Serial.println("Saving run counter.");
#endif

    File f = LittleFS.open("/counter", "w");
    f.print(String(runCount));
    f.close();
}

void readRunCount() {
#ifdef ENABLE_LOGGING
    Serial.println("Reading FS for counter.");
#endif

    File f = LittleFS.open("/counter",  "r");
    if (f) {
        runCount = f.readString().toInt();
#ifdef ENABLE_LOGGING
        Serial.print("Counter found: ");
        Serial.println(runCount);
#endif
    } else {
#ifdef ENABLE_LOGGING
        Serial.print("Counter not found, setting to 0 and saving file.");
#endif
        runCount = 0;
        writeRunCount();
    }

    f.close();
}

void reset(boolean isFast) {
    ESP.deepSleep(isFast ? INTERVAL_US/6 : INTERVAL_US);
}

void setup() {
    LittleFS.begin();

#ifdef ENABLE_LOGGING
    Serial.begin(115200);
    Serial.println("Initializing modem...");
#endif

    readRunCount();

#ifdef USE_WIFI
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.println();
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(300);
    }

    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();
#else
    if (!modem.init())
    {
#ifdef ENABLE_LOGGING
        Serial.println("Failed to restart modem, delaying 10s and retrying");
#endif
        reset(true);
        return;
    }

#ifdef ENABLE_LOGGING
    String name = modem.getModemName();
    Serial.print("Modem Name:");
    Serial.println(name);

    String modemInfo = modem.getModemInfo();
    Serial.print("Modem Info:");
    Serial.println(modemInfo);
#endif

    fbdo.setGSMClient(&gsm_client, &modem, "", "", "", "");
#endif

    ran = false;
    timeSync.begin();
}

void loop() {
    if (!ran)
    {
#ifdef ENABLE_LOGGING
        Serial.println("Ready, setting val in firebase.");
#endif

        String path = String(DATABASE_URL) + "/" + PATH_PREFIX + "/" + USER_KEY + "/" + VEHICLE_UID + ".json";

#ifdef ENABLE_LOGGING
        Serial.print("Path: ");
        Serial.println(path);
#endif

        fetch.begin(path);
        fetch.addHeader("content-type", "application/json");
        int response = fetch.PATCH(String() + R"({"lat": ")" + "12.032146" + R"(", "lon": ")" + String(random(-150, 150)) + "." + String(random(100000, 999999)) + "\"}");

#ifdef ENABLE_LOGGING
        Serial.printf("Set val, response status: %s\n", String(response).c_str());
#endif

        runCount++;
        writeRunCount();
        ran = true;
        reset(false);
    }
}