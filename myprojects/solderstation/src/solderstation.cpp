#include <Arduino.h>
#include "Wire.h"
#include "LiquidCrystal_I2C.h"
#include "IRremote.h"
#include "max6675.h"
#include "EEPROM.h"

#define LCD_ADDRESS 0x27
#define IR_PIN 1
#define HEAT_PIN 2
#define HEAT_ON_VAL LOW
#define HEAT_OFF_VAL HIGH
#define SENSOR_CLK 1
#define SENSOR_CS 2
#define SENSOR_MISO 3
#define COMMAND_TIMEOUT 500
#define MAX_FAN_SPEED 5
#define MIN_FAN_SPEED 0
#define TEMP_THRESHOLD 15
#define TEMP_HEAT_TIMEOUT 20000
#define MAX_TEMP 500
#define EEPROM_ADDR 32
#define EEPROM_MARKER 58

/*
 * 69 - power
 * 71 - mute
 * 68 - mode
 * 67 - eq
 * 21 - next
 * 7 - prev
 * 9 - pause
 * 25 - vol+
 * 22 - vol-
 * 64 - ret
 * 13 - 0
 * 12 - 1
 * 24 - 2
 * 94 - 3
 * 8 - 4
 * 28 - 5
 * 90 - 6
 * 66 - 7
 * 82 - 8
 * 74 - 9
 */
#define CMD_POWER 69
#define FAN_PLUS 25
#define FAN_MINUS 22
#define TEMP_PLUS 21
#define TEMP_MINUS 7
#define CMD_SAVE 67
#define CMD_RECOVER 64
#define CMD_MODE 68
uint8_t numCommands[] = {13, 12, 24, 94, 8, 28, 90, 66, 82, 74};

struct Config {
    uint16_t userTemp;
    uint8_t fanSpeed;

    Config() {
        userTemp = 300;
        fanSpeed = 3;
    }
};

namespace States {
    enum States {
        ON,
        USER_TEMP,
        TEMP,
        FAN,
        NO_HEATING,
        SAVE_MODE,
        RECOVER_MODE
    };
}

MAX6675 tempSensor(SENSOR_CLK, SENSOR_CS, SENSOR_MISO);
LiquidCrystal_I2C display(LCD_ADDRESS, 16, 2);
char displayBuffer[33];
Config *currentConfig;
Config savedConfigs[10];
uint16_t currentTemp = 30;

bool isOn = false;
bool isHeating = false;
bool forceHeatingOff = false;
bool isSaveMode = false;
bool isRecoverMode = false;
unsigned long commandTimer = 0;
unsigned long heatTimer = 0;
unsigned long tempUpdateTimer = 0;

void recoverConfigs() {
    if (EEPROM.read(EEPROM_ADDR) != EEPROM_MARKER) {
        EEPROM.put(EEPROM_ADDR, (uint8_t) EEPROM_MARKER);
    } else {
        EEPROM.get(EEPROM_ADDR+1, savedConfigs);
    }
    currentConfig = &savedConfigs[0];
}

/*
 * T: 000ºC|000ºC H
 * F: x | V: xx.xx
 */
void updateDisplay() {
    if (isSaveMode) {
        display.clear();
        display.print("Salvar conf.\npressione numero");
        return;
    }

    if (isRecoverMode) {
        display.clear();
        display.print("Recuperar conf.\npressione numero");
        return;
    }

    sprintf(displayBuffer, "T: %03d\223C|%03d\223C %c\nF: %01d",
            forceHeatingOff ? 0 : currentConfig.userTemp,
            currentTemp,
            forceHeatingOff ? 'N' : (isHeating ? 'H' : ' '),
            currentConfig.fanSpeed > 9 ? 9 : currentConfig.fanSpeed);

    display.clear();
    display.print(displayBuffer);
}

void updateStates(States::States s, unsigned int extra) {
    if(s == States::ON) {
        isOn = !isOn;
        isHeating = false;

        if (isOn) {
            display.display();
            display.backlight();
        } else {
            display.noDisplay();
            display.noBacklight();
        }
    } else if (s == States::NO_HEATING) {
        forceHeatingOff = true;
        isHeating = false;
    } else if (s == States::SAVE_MODE) {
        isSaveMode = true;
        isRecoverMode = false;
    } else if (s == States::RECOVER_MODE) {
        isSaveMode = false;
        isRecoverMode = true;
    } else if (s == States::FAN) { // TODO
    } else if (s == States::USER_TEMP) {
        currentConfig.userTemp = extra > MAX_TEMP ? MAX_TEMP : extra;
    } else if (s == States::TEMP && millis() - heatTimer > TEMP_HEAT_TIMEOUT) {
        currentTemp = extra > 999 ? 999 : extra;

        if (currentTemp + TEMP_THRESHOLD >= currentConfig.userTemp) {
            isHeating = false;
            digitalWrite(HEAT_PIN, HEAT_OFF_VAL);
            heatTimer = millis();
        } else {
            isHeating = true;
            digitalWrite(HEAT_PIN, HEAT_ON_VAL);
            heatTimer = millis();
        }
    } else {
        return;
    }

    updateDisplay();
}

void processIrCommand() {
    IrReceiver.decode();
    IrReceiver.resume();

    if (millis() - commandTimer < COMMAND_TIMEOUT) {
        return;
    }

    auto command = IrReceiver.decodedIRData.command;

    if (isSaveMode || isRecoverMode) {
        for (const uint8_t num : numCommands) {

        }
        return;
    }

    switch (command) {
        case CMD_POWER: {
            updateStates(States::ON, 0);
            break;
        }
        case TEMP_PLUS: {
            updateStates(States::USER_TEMP, currentConfig.userTemp+10);
            break;
        }
        case TEMP_MINUS: {
            updateStates(States::USER_TEMP, currentConfig.userTemp-10);
            break;
        }
        case FAN_PLUS: {
            updateStates(States::FAN, currentConfig.fanSpeed+1);
            break;
        }
        case FAN_MINUS: {
            updateStates(States::FAN, currentConfig.fanSpeed-1);
            break;
        }
        case CMD_SAVE: {
            updateStates(States::SAVE_MODE, 0);
            break;
        }
        case CMD_RECOVER: {
            updateStates(States::RECOVER_MODE, 0);
            break;
        }
        case CMD_MODE: {
            updateStates(States::SAVE_MODE, 0);
            break;
        }
    }
}

void setup() {
    pinMode(HEAT_PIN, OUTPUT);

    IrReceiver.begin(IR_PIN);
    IrReceiver.registerReceiveCompleteCallback(processIrCommand);

    display.init();
    display.backlight();
    display.print("T: 000\223C|000\223C H\nF: x");
}

void loop() {
    if (millis() - tempUpdateTimer > 2000) {
        auto temp = (uint16_t) tempSensor.readCelsius();
        if (temp != currentTemp) {
            currentTemp = temp;
            updateStates(States::TEMP, currentTemp);
        }
    }
}