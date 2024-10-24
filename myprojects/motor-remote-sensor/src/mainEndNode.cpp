#include <Arduino.h>
#include "ESP8266WiFi.h"
#include "esp-config-page.h"
#include "shared.h"
#include "EEPROM.h"
#include "Wire.h"
#include "Adafruit_MAX31865.h"

#define RELAY_PIN 1 // TODO change

#define EEPROM_TRUE_VAL 182
#define EEPROM_FALSE_VAL 255
#define EEPROM_IS_RELAY_ON_ADDR 1

#define HEALTH_PING_DELAY 30000
#define TEMP_CHECK_DELAY 1200

// The value of the Rref resistor. Use 430.0 for PT100 and 4300.0 for PT1000
#define RREF      430.0
// The 'nominal' 0-degrees-C resistance of the sensor
// 100.0 for PT100, 1000.0 for PT1000
#define RNOMINAL  100.0

bool shouldRunServer = false;
bool isRelayOn = false;
unsigned long lastPingSent = 0;
uint8_t currentTemp = 30;
unsigned long lastTempCheck = 0;

Adafruit_MAX31865 tempSensor = Adafruit_MAX31865(10, 11, 12, 13); // TODO change pins
auto maxTempVar = new ESP_CONFIG_PAGE::EnvVar("MAX_TEMP", "120");
int maxTemp = 120;

void readEeprom() {
    isRelayOn = EEPROM.read(EEPROM_IS_RELAY_ON_ADDR) == EEPROM_TRUE_VAL;
}

void writeEepromVal(int addr, uint8_t newVal) {
    if (EEPROM.read(addr) != newVal) {
        EEPROM.write(addr, newVal);
    }
}

void writeEeprom() {
    writeEepromVal(EEPROM_IS_RELAY_ON_ADDR, isRelayOn ? EEPROM_TRUE_VAL : EEPROM_FALSE_VAL);
}

void readTempSensor() {
    uint8_t fault = tempSensor.readFault();
    if (fault) {
        Serial.print("Temp sensor fault 0x"); Serial.println(fault, HEX);
        if (fault & MAX31865_FAULT_HIGHTHRESH) {
            Serial.println("RTD High Threshold");
        }
        if (fault & MAX31865_FAULT_LOWTHRESH) {
            Serial.println("RTD Low Threshold");
        }
        if (fault & MAX31865_FAULT_REFINLOW) {
            Serial.println("REFIN- > 0.85 x Bias");
        }
        if (fault & MAX31865_FAULT_REFINHIGH) {
            Serial.println("REFIN- < 0.85 x Bias - FORCE- open");
        }
        if (fault & MAX31865_FAULT_RTDINLOW) {
            Serial.println("RTDIN- < 0.85 x Bias - FORCE- open");
        }
        if (fault & MAX31865_FAULT_OVUV) {
            Serial.println("Under/Over voltage");
        }
        tempSensor.clearFault();
        return;
    }

    currentTemp = ceil(tempSensor.temperature(RNOMINAL, RREF));
    LOGF("Read temp sensor, temp is %d", currentTemp);
}

void turn(bool on) {
    isRelayOn = on;
    writeEeprom();
    digitalWrite(RELAY_PIN, isRelayOn ? HIGH : LOW);
}

void sendHealthPing() {
    LOGN("Sending health ping.");

    const uint8_t buf[] = {currentTemp, currentTemp > maxTemp, isRelayOn};
    sendCommand(MotorCommand::HEALTH_PING, buf, 3);
    lastPingSent = millis();
}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.setOutputPower(15);

    pinMode(RELAY_PIN, OUTPUT);
    EEPROM.begin(4);
    readEeprom();
    tempSensor.begin(MAX31865_3WIRE);

    LittleFS.begin();
    shouldRunServer = shouldRunConfigPage();

    if (!initConfigPage(true)) {
        return;
    }

    if (!shouldRunServer) {
        maxTemp = maxTempVar->value.toInt();

        if (maxTemp == 0 || maxTemp < 0) {
            LOGN("Invalid max temperature, resetting.");
            changeMode(false);
            return;
        }

        dataRecvCb = [](uint8_t command, uint8_t extra, uint8_t *mac) {

        };
    }

    LOGN("Setup complete.");
}

void loop() {
    if (shouldRunServer) {
        server.handleClient();
        return;
    }

    if (millis() - lastTempCheck > TEMP_CHECK_DELAY) {
        readTempSensor();
        lastTempCheck = millis();
    }

    if (currentTemp > maxTemp) {
        LOGN("Temp limit reached! Turning motor off.");
        turn(false);
        sendHealthPing();
    }

    if (millis() - lastPingSent > HEALTH_PING_DELAY) {
        sendHealthPing();
    }
}