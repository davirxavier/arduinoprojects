#include <Arduino.h>
#include "ESP8266WiFi.h"
#include "esp-config-page.h"
#include "shared.h"
#include "EEPROM.h"
#include "Bounce2.h"
#include "buzzer.h"

#define BEEPER_PIN 1 // TODO change
#define HEALTH_LED_PIN 2 // TODO change
#define STATUS_LED_RED 3 // TODO change
#define STATUS_LED_GREEN 4 // TODO change
#define STATUS_LED_BLUE 5 // TODO change
#define TURN_OFF_BTN 6 // TODO change
#define TURN_ON_BTN 7 // TODO change

#define EEPROM_TRUE_VAL 182
#define EEPROM_FALSE_VAL 255
#define EEPROM_STATE_ADDR 2
#define ACK_TIMEOUT 30000
#define HEALTH_DOWN_TIMEOUT (5 * 60 * 1000)

namespace States {
    enum States {
        ON = 122,
        OFF = 123,
        WAITING_ACK = 124,
        DOWN = 125,
        TOO_HOT = 126
    };
};

bool shouldRunServer = false;
unsigned long lastHealthPing = 0;
unsigned long ackTimer = 0;

DxBuzzer buzzer(BEEPER_PIN);
Bounce2::Button turnOffButton;
Bounce2::Button turnOnButton;
States::States currentState;

void writeColor(uint8_t red, uint8_t green, uint8_t blue) {
    analogWrite(STATUS_LED_RED, red);
    analogWrite(STATUS_LED_GREEN, green);
    analogWrite(STATUS_LED_BLUE, blue);
}

void writeStatus(States::States state) {
    switch (state) {
        case States::WAITING_ACK:
            writeColor(255, 255, 0);
            digitalWrite(HEALTH_LED_PIN, HIGH);
            break;
        case States::OFF:
            writeColor(0, 0, 0);
            digitalWrite(HEALTH_LED_PIN, LOW);
            break;
        case States::ON:
            writeColor(0, 255, 0);
            digitalWrite(HEALTH_LED_PIN, HIGH);
            break;
        case States::DOWN:
            writeColor(255, 255, 0);
            digitalWrite(HEALTH_LED_PIN, LOW);
            break;
        case States::TOO_HOT:
            writeColor(255, 0, 0);
            digitalWrite(HEALTH_LED_PIN, LOW);
            break;
    }
}

void readEeprom() {
    currentState = static_cast<States::States>(EEPROM.read(EEPROM_STATE_ADDR));
}

void writeEepromVal(int addr, uint8_t newVal) {
    if (EEPROM.read(addr) != newVal) {
        EEPROM.write(addr, newVal);
    }
}

void writeEeprom() {
    writeEepromVal(EEPROM_STATE_ADDR, currentState);
}

void changeState(States::States newState) {
    if (newState == currentState) {
        return;
    }

    if (newState == States::TOO_HOT) {
        currentState = newState;
        writeStatus(newState);
        buzzer.beep(500, UINT_MAX, 250);
        return;
    }

    if (newState == States::DOWN) {
        currentState = newState;
        writeStatus(newState);
        buzzer.beep(1500, UINT_MAX, 1000);
        return;
    }

    if (newState == States::WAITING_ACK && (currentState == States::ON || currentState == States::OFF)) {
        currentState = newState;
        writeStatus(newState);
        return;
    }

    if (newState == States::ON && (currentState == States::OFF || currentState == States::WAITING_ACK)) {
        currentState = newState;
        writeStatus(newState);
        return;
    }

    if (newState == States::OFF && (currentState == States::ON || currentState == States::WAITING_ACK)) {
        currentState = newState;
        writeStatus(newState);
        return;
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

    LittleFS.begin();
    shouldRunServer = shouldRunConfigPage();

    if (!initConfigPage(true)) {
        return;
    }

    if (!shouldRunServer) {
        dataRecvCb = [](uint8_t c, const uint8_t *extras, uint8_t extrasLength, uint8_t *mac) {
            auto command = static_cast<MotorCommand::MotorCommands>(c);

            if (command == MotorCommand::HEALTH_PING) {
                bool isOverheated = extras[1];
                bool isOn = extras[2];

                if (currentState == States::DOWN) {
                    buzzer.stop();
                    ESP.reset();
                }

                if (isOverheated) {
                    changeState(States::TOO_HOT);
                }

                if (!isOverheated && currentState != States::TOO_HOT) {
                    changeState(isOn ? States::ON : States::OFF);
                }

                if (!isOverheated && currentState == States::TOO_HOT) {
                    buzzer.stop();
                    ESP.reset();
                }
            }

            if (command == MotorCommand::ACK && currentState == States::WAITING_ACK) {
                auto response = static_cast<CommandResponses::CommandResponses>(extras[1]);
                bool isOn = extras[2];

                if (response == CommandResponses::OK) {
                    changeState(isOn ? States::ON : States::OFF);
                } else if (response == CommandResponses::TOO_HOT) {
                    changeState(States::TOO_HOT);
                }
            }

            lastHealthPing = millis();
        };
    }

    LOGN("Setup complete.");
    requestUpdate();
}

void loop() {
    if (shouldRunServer) {
        server.handleClient();
        return;
    }

    turnOnButton.update();
    turnOffButton.update();

    if (turnOnButton.isPressed() && currentState == States::OFF) {
        changeState(States::WAITING_ACK);
        sendCommand(MotorCommand::TURN_ON);
        ackTimer = millis();
    }

    if (turnOffButton.isPressed() && currentState == States::ON) {
        changeState(States::WAITING_ACK);
        sendCommand(MotorCommand::TURN_OFF);
        ackTimer = millis();
    }

    if (currentState == States::WAITING_ACK && millis() - ackTimer > ACK_TIMEOUT) {
        changeState(States::DOWN);
    }

    if (millis() - lastHealthPing > HEALTH_DOWN_TIMEOUT) {
        changeState(States::DOWN);
    }
}