#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "fauxmoESP.h"
#include "WifiConfig.h"
#include <LittleFS.h>
#include "util.h"

#define CONNECTION_TIMEOUT_SECONDS 15

fauxmoESP fmo;
WifiConfig config;

bool isConnecting;
unsigned long connectionStart;
unsigned long lastCheckedConn;

bool started;

void setupFauxmo() {
    fmo.addDevice("led");

    fmo.setPort(80);
    fmo.enable(true);

    fmo.onSetState([](unsigned char device_id, const char * device_name, bool state, unsigned char value) {
        analogWrite(0, state ? value : 0);
        Serial.print("State change: ");
        Serial.println(state);
        Serial.print("Value: ");
        Serial.println(value);
    });
}

void setup() {
    if(!LittleFS.begin()) {
        Serial.println("An Error has occurred while mounting LittleFS");
    }

    Serial.begin(9600);
    pinMode(0, OUTPUT);
    digitalWrite(0, LOW);

    started = false;
    isConnecting = false;
}

void loop() {
    if (!started && millis() > 3000) {
        started = true;

        if (config.hasConfig()) {
            Serial.print("Connecting");
            config.tryRestoreConfig();
            isConnecting = true;
            connectionStart = millis();
            lastCheckedConn = millis();
        } else {
            Serial.println("No config set, please set wifi config using the command 'CONFIG=<ssid>;<pasword>'.");
        }
    }

    if (isConnecting && millis() - lastCheckedConn > 700) {
        wl_status_t status = WiFi.status();
        if (status == WL_CONNECTED) {
            isConnecting = false;
            setupFauxmo();
            Serial.println();
            Serial.println("CONNECTION OK");
        } else {
            Serial.print(".");

            if (millis() - connectionStart > (CONNECTION_TIMEOUT_SECONDS * 1000)) {
                isConnecting = false;
                config.resetConfig();
                Serial.println();
                Serial.println("Connection failed.");
            }
        }

        lastCheckedConn = millis();
    }

    if (config.hasConfig() && WiFi.status() == WL_CONNECTED) {
        fmo.handle();
    }

    if (Serial.available()) {
        String command = Serial.readString();  //read until timeout
        command.trim();
        if (command.startsWith("CONFIG=")) {
            String vals = getValue(command, '=', 1);
            String ssid = getValue(vals, ';', 0);
            String pass = getValue(vals, ';', 1);

            if (ssid.length() > 0 && pass.length() > 0) {
                fmo.enable(false);
                config.setConfig(ssid, pass);

                isConnecting = true;
                connectionStart = millis();
                lastCheckedConn = millis();
                Serial.print("Connecting");
            } else {
                Serial.println("Command is invalid. Format: CONFIG=ssid;password");
            }
        } else if (command == "STATUS") {
            Serial.println(WiFi.status());
        } else {
            Serial.println("Unknown command.");
        }
    }
}