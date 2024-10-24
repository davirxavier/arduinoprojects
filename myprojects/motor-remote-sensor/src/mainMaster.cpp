#include <Arduino.h>
#include "ESP8266WiFi.h"
#include "esp-config-page.h"
#include "shared.h"
#include "EEPROM.h"
#include "Bounce2.h"

#define BEEPER_PIN 1 // TODO change
#define HEALTH_LED_PIN 2 // TODO change
#define STATUS_LED_RED 3 // TODO change
#define STATUS_LED_GREEN 4 // TODO change
#define STATUS_LED_BLUE 5 // TODO change
#define TURN_OFF_BTN 6 // TODO change
#define TURN_ON_BTN 7 // TODO change

#define EEPROM_TRUE_VAL 182
#define EEPROM_FALSE_VAL 255
#define EEPROM_ISON_ADDR 1
#define EEPROM_ISDOWN_ADDR 2
#define ACK_TIMEOUT 60000
#define HEALTH_DOWN_TIMEOUT (5 * 60 * 1000)

bool shouldRunServer = false;
unsigned long lastHealthPingReceived = 0;
bool isOn = false;
bool isDown = false;
bool isWaitingAck = false;
unsigned long ackTimer = 0;

bool isBeeping = false;
unsigned long beepQuantity = 0;
unsigned long beepTimer = 0;
unsigned long beepTimeout = 500;

Bounce2::Button turnOffButton;
Bounce2::Button turnOnButton;

namespace LedStatus {
    enum LedStatus {
        WAITING,
        OFF,
        ON,
        DANGER
    };

    void writeColor(uint8_t red, uint8_t green, uint8_t blue) {
        analogWrite(STATUS_LED_RED, red);
        analogWrite(STATUS_LED_GREEN, green);
        analogWrite(STATUS_LED_BLUE, blue);
    }
};

void writeStatus(LedStatus::LedStatus s) {
    switch (s) {
        case LedStatus::WAITING:
            LedStatus::writeColor(255, 255, 0);
            digitalWrite(HEALTH_LED_PIN, LOW);
            break;
        case LedStatus::OFF:
            LedStatus::writeColor(0, 0, 0);
            digitalWrite(HEALTH_LED_PIN, LOW);
            break;
        case LedStatus::ON:
            LedStatus::writeColor(0, 255, 0);
            digitalWrite(HEALTH_LED_PIN, HIGH);
            break;
        case LedStatus::DANGER:
            LedStatus::writeColor(255, 0, 0);
            digitalWrite(HEALTH_LED_PIN, LOW);
            break;
    }
}

void readEeprom() {
    isOn = EEPROM.read(EEPROM_ISON_ADDR) == EEPROM_TRUE_VAL;
    isDown = EEPROM.read(EEPROM_ISDOWN_ADDR) == EEPROM_TRUE_VAL;
}

void writeEepromVal(int addr, uint8_t newVal) {
    if (EEPROM.read(addr) != newVal) {
        EEPROM.write(addr, newVal);
    }
}

void writeEeprom() {
    writeEepromVal(EEPROM_ISON_ADDR, isOn ? EEPROM_TRUE_VAL : EEPROM_FALSE_VAL);
    writeEepromVal(EEPROM_ISDOWN_ADDR, isDown ? EEPROM_TRUE_VAL : EEPROM_FALSE_VAL);
}

void beep(unsigned long timeout, unsigned long times) {
    if (times == 0) {
        return;
    }

    beepTimer = millis() - 500;
    beepTimeout = timeout;
    beepQuantity = times;
}

void stopBeep() {
    isBeeping = false;
    beepTimer = 0;
    beepQuantity = 0;
    digitalWrite(BEEPER_PIN, LOW);
}

void turn(bool on) {
    isOn = !on;
    writeEeprom();
    writeStatus(LedStatus::WAITING);

    if (on) {
        sendCommand(MotorCommand::TURN_ON);
    } else {
        sendCommand(MotorCommand::TURN_OFF);
    }

    isWaitingAck = true;
    ackTimer = millis();

    if (isOn) {
        lastHealthPingReceived = millis();
    }
}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.setOutputPower(15);

    pinMode(BEEPER_PIN, OUTPUT);
    pinMode(HEALTH_LED_PIN, OUTPUT);

    turnOnButton.attach(TURN_ON_BTN, INPUT_PULLUP);
    turnOnButton.interval(10);
    turnOnButton.setPressedState(LOW);

    turnOffButton.attach(TURN_OFF_BTN, INPUT_PULLUP);
    turnOffButton.interval(10);
    turnOffButton.setPressedState(LOW);

    EEPROM.begin(4);
    readEeprom();

    if (isDown) {
        writeStatus(LedStatus::DANGER);
    } else {
        writeStatus(isOn ? LedStatus::ON : LedStatus::OFF);
    }

    LittleFS.begin();
    shouldRunServer = shouldRunConfigPage();

    if (!initConfigPage(true)) {
        return;
    }

    if (!shouldRunServer) {
        dataRecvCb = [](uint8_t command, uint8_t extra, uint8_t *mac) {
            if (isWaitingAck) {
                if (command == MotorCommand::ACK && extra == MotorCommand::TURN_ON) {
                    writeStatus(LedStatus::ON);
                } else if (command == MotorCommand::ACK && extra == MotorCommand::TURN_OFF) {
                    writeStatus(LedStatus::OFF);
                }

                isWaitingAck = false;
            }

            lastHealthPingReceived = millis();

            if (isDown) {
                isDown = false;
                writeStatus(isOn ? LedStatus::ON : LedStatus::OFF);
                stopBeep();
                delay(100);
                beep(1000, 1);
            }
        };
    }

    LOGN("Setup complete.");
}

void loop() {
    if (shouldRunServer) {
        server.handleClient();
        return;
    }

    turnOnButton.update();
    turnOffButton.update();

    if (turnOnButton.pressed() && isDown) {
        stopBeep();
    } else if (!isDown) {
        turn(true);
    }

    if (turnOffButton.pressed() && isDown) {
        stopBeep();
    } else if (!isDown) {
        turn(false);
    }

    if (isWaitingAck && millis() - ackTimer > ACK_TIMEOUT) {
        LOGN("Timeout waiting for ACK for turn command.");
        beep(1500, 1);

        if (isOn) {
            writeStatus(LedStatus::OFF);
        } else {
            writeStatus(LedStatus::ON);
        }

        isWaitingAck = false;
        isOn = !isOn;
        writeEeprom();
    }

    if (beepQuantity > 0 && millis() - beepTimer > beepTimeout) {
        isBeeping = true;
        beepQuantity--;
        digitalWrite(BEEPER_PIN, HIGH);
        beepTimer = millis();
    }

    if (isBeeping && millis() - beepTimer > beepTimeout) {
        isBeeping = false;
        digitalWrite(BEEPER_PIN, LOW);
        beepTimer = millis();
    }

    if (isOn && !isDown && millis() - lastHealthPingReceived > HEALTH_DOWN_TIMEOUT) {
        isDown = true;
        writeStatus(LedStatus::DANGER);
        beep(500, ULONG_MAX-1);
    }
}