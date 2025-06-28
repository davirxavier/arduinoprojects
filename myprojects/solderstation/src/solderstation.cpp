#include <Arduino.h>
#include "Wire.h"
#include "LiquidCrystal_I2C.h"
#include <IRremote.hpp>
#include "MAX6675.h"
#include "EEPROM.h"
#include "inductance.h"

#define PS_ON_PIN 2
#define LCD_ADDRESS 0x27
#define IR_PIN A0
#define HEAT_PIN 3
#define HEAT_ON_VAL LOW
#define HEAT_OFF_VAL HIGH
#define SENSOR_CLK 8
#define SENSOR_CS 7
#define SENSOR_MISO 6
#define COMMAND_TIMEOUT 150
#define FAN_POWER_PIN 11
#define FAN_CONTROL_PIN 10
#define MAX_FAN_SPEED 5
#define MIN_FAN_SPEED 0
#define TEMP_THRESHOLD 15
#define TEMP_HEAT_TIMEOUT 20000
#define MAX_TEMP 500
#define EEPROM_ADDR 32
#define EEPROM_MARKER 58
#define INDUCTANCE_SIGNAL_PIN A3
#define INDUCTANCE_INPUT_PIN A2
#define INDUCTANCE_CAPACITOR 9.E-6
#define MAX_VOLTAGE 45
#define MIN_VOLTAGE 0

/*
 * 69 - power
 * 71 - mute
 * 68 - mode
 * 64 - back
 * 67 - eq
 * 21 - next
 * 7 - prev
 * 9 - pause
 * 70 - stop
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
#define CMD_MODE 68
#define CMD_BACK 64
#define CMD_STOP 70
#define CMD_PAUSE 9
#define CMD_EQ 67
uint8_t numCommands[] = {13, 12, 24, 94, 8, 28, 90, 66, 82, 74};

struct Config {
    int userTemp;
    int fanSpeed;
    double targetVoltage;

    Config() {
        userTemp = 300;
        fanSpeed = 3;
        targetVoltage = -1;
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
        RECOVER_MODE,
        LC_METER_MODE,
        VOLTAGE_MODE
    };
}

void reset() { asm volatile ("jmp 0x7800"); }

MAX6675 tempSensor(SENSOR_CS, SENSOR_MISO, SENSOR_CLK);
LiquidCrystal_I2C display(LCD_ADDRESS, 16, 2);
char displayBuffer[17];
Config *currentConfig;
Config savedConfigs[10];
uint16_t currentTemp = 30;
int currentVoltageChar = 3;
double typingTargetVoltage = 0;
double currentVoltage = 0;
bool isTypingVoltage = false;

bool isOn = false;
bool isHeating = false;
bool forceHeatingOff = false;
States::States currentAltMode = States::ON;
unsigned long commandTimer = 0;
unsigned long heatTimer = -TEMP_HEAT_TIMEOUT;
unsigned long tempUpdateTimer = 0;
char voltageChars[] = {'0', '0', '.', '0', '\0'};
double currentInductance;
double currentFrequency;

void setupTimer9and10(){
    //Set PWM frequency to about 25khz on pins 9,10 (timer 1 mode 10, no prescale, count to 320)
    TCCR1A = (1 << COM1A1) | (1 << COM1B1) | (1 << WGM11);
    TCCR1B = (1 << CS10) | (1 << WGM13);
    ICR1 = 320;
    OCR1A = 0;
    OCR1B = 0;
}

/**
 * equivalent of analogWrite on pin 9
 * @param val duty cycle between 0 and 320
 */
void setPWM9(uint16_t val){
    OCR1A = constrain(val, 0, 320);
}

/**
 * equivalent of analogWrite on pin 10
 * @param val duty cycle between 0 and 320
 */
void setPWM10(uint16_t val){
    OCR1B = constrain(val, 0, 320);
}

void recoverConfigs() {
    if (EEPROM.read(EEPROM_ADDR) != EEPROM_MARKER) {
        EEPROM.put(EEPROM_ADDR, (uint8_t) EEPROM_MARKER);
    } else {
        EEPROM.get(EEPROM_ADDR+1, savedConfigs);
    }

    currentConfig = &savedConfigs[0];

    if (currentConfig->targetVoltage < 0) {
        currentConfig->targetVoltage = 0;
    }

    if (currentConfig->userTemp < 0) {
        currentConfig->userTemp = 0;
    }

    if (currentConfig->fanSpeed < 0) {
        currentConfig->fanSpeed = 0;
    }
}

void saveConfigs() {
    EEPROM.put(EEPROM_ADDR+1, savedConfigs);
}

/*
 * T: 000ºC|000ºC H
 * F: x | V: xx.xx
 */
void updateDisplay() {
    display.cursor_off();

    if (currentAltMode == States::SAVE_MODE) {
        display.clear();
        display.print("Salvar conf.");
        display.setCursor(0, 1);
        display.print("pressione numero");
        return;
    } else if (currentAltMode == States::RECOVER_MODE) {
        display.clear();
        display.print("Recuperar conf.");
        display.setCursor(0, 1);
        display.print("pressione numero");
        return;
    } else if (currentAltMode == States::LC_METER_MODE) {
        display.setCursor(0, 0);

        char unit = (char) 0xE4; // micro
        double value = currentInductance > 0 ? currentInductance : 0;
        if (currentInductance > 2000) {
            unit = 'm';
            value = value / 1000.0;
        }

        sprintf(displayBuffer, "L: %11.3f%cH",
                value,
                (char) unit);
        display.print(displayBuffer);
        display.setCursor(0, 1);

        unit = ' ';
        double freqVal = currentFrequency >= 0 ? currentFrequency : 0;
        if (freqVal > 10000) {
            freqVal = freqVal / 1000.0;
            unit = 'k';
        }

        sprintf(displayBuffer, "F: %10.2f%chz", freqVal, unit);
        display.print(displayBuffer);
        return;
    } else if (currentAltMode == States::VOLTAGE_MODE) {
        display.setCursor(0, 0);

        sprintf(displayBuffer, "Targ. V: %6.1fv", isTypingVoltage ? typingTargetVoltage : constrain(currentConfig->targetVoltage, 0, 9999));
        display.print(displayBuffer);

        display.setCursor(0, 1);
        sprintf(displayBuffer, "Vout: %9.2fv", currentVoltage);
        display.print(displayBuffer);

        if (isTypingVoltage) {
            display.cursor_on();
            display.setCursor(11 + currentVoltageChar, 0);
        }
        return;
    }

    display.setCursor(0, 0);

    sprintf(displayBuffer, "T: %3d%cC|%3d%cC %c",
            currentConfig->userTemp,
            (char) 223,
            currentTemp,
            (char) 223,
            forceHeatingOff ? '-' : (isHeating ? '*' : ' '));
    display.print(displayBuffer);

    display.setCursor(0, 1);
    sprintf(displayBuffer, "F: %1d            ",
            constrain(currentConfig->fanSpeed, MIN_FAN_SPEED, MAX_FAN_SPEED));
    display.print(displayBuffer);
}

void updateStates(States::States s, unsigned int extra, bool doUpdateDisplay) {
    if(s == States::ON) {
        isOn = !isOn;
        isHeating = false;

        if (isOn) {
            digitalWrite(PS_ON_PIN, LOW);
            delay(250);

            display.init();
            display.display();
            display.backlight();
        } else {
            digitalWrite(PS_ON_PIN, HIGH);
            reset();
        }
    } else if (s == States::SAVE_MODE) {
        currentAltMode = States::SAVE_MODE;
    } else if (s == States::RECOVER_MODE) {
        currentAltMode = States::RECOVER_MODE;
    } else if (s == States::LC_METER_MODE) {
        currentAltMode = States::LC_METER_MODE;
    } else if (s == States::FAN) {
        currentConfig->fanSpeed = constrain(extra, MIN_FAN_SPEED, MAX_FAN_SPEED);
        digitalWrite(FAN_POWER_PIN, currentConfig->fanSpeed > 0 ? HIGH : LOW);
        setPWM10(map(currentConfig->fanSpeed, 0, 5, 0, 320));
    } else if (s == States::USER_TEMP) {
        currentConfig->userTemp = extra > MAX_TEMP ? MAX_TEMP : extra;
    } else if (s == States::TEMP) {
        currentTemp = constrain(extra+2, 0, 999);

        if (millis() - heatTimer < TEMP_HEAT_TIMEOUT) {
            updateDisplay();
            return;
        }

        if (forceHeatingOff) {
            digitalWrite(HEAT_PIN, HEAT_OFF_VAL);
            heatTimer = millis();
            updateDisplay();
            return;
        }

        if (currentTemp + TEMP_THRESHOLD >= currentConfig->userTemp) {
            isHeating = false;
            digitalWrite(HEAT_PIN, HEAT_OFF_VAL);
            heatTimer = millis();
        } else {
            isHeating = true;
            digitalWrite(HEAT_PIN, HEAT_ON_VAL);
            heatTimer = millis();
        }
    }

    if (doUpdateDisplay) {
        updateDisplay();
    }
}

void updateStates(States::States s, unsigned int extra) {
    updateStates(s, extra, true);
}

void cycleAltMode() {
    if (currentAltMode == States::LC_METER_MODE) {
        currentAltMode = States::VOLTAGE_MODE;
    } else if (currentAltMode == States::VOLTAGE_MODE) {
        currentAltMode = States::ON;
    } else {
        currentAltMode = States::LC_METER_MODE;
    }
}

void processIrCommand() {
    IrReceiver.resume();

    if (millis() - commandTimer < COMMAND_TIMEOUT) {
        return;
    }

    commandTimer = millis();
    auto command = IrReceiver.decodedIRData.command;

    if (!isOn && command == CMD_POWER) {
        updateStates(States::ON, 0);
        return;
    } else if (!isOn) {
        return;
    }

    if (currentAltMode != States::ON && command == CMD_MODE) {
        cycleAltMode();
        if (currentAltMode == States::ON) { // Ignore unreachable warning, it's dumb
            updateDisplay();
        } else {
            updateStates(currentAltMode, 0);
        }
        return;
    }

    for (uint16_t i = 0; i < sizeof(numCommands); i++) {
        if (command != numCommands[i]) {
            continue;
        }

        if (currentAltMode == States::SAVE_MODE) {
            savedConfigs[i].userTemp = currentConfig->userTemp;
            savedConfigs[i].fanSpeed = currentConfig->fanSpeed;
            saveConfigs();

            currentAltMode = States::ON;
            updateDisplay();
            return;
        }  else if (currentAltMode == States::RECOVER_MODE) {
            currentAltMode = States::ON;
            currentConfig = &savedConfigs[i];
            updateDisplay();
            return;
        } else if (currentAltMode == States::VOLTAGE_MODE && isTypingVoltage) {
            voltageChars[currentVoltageChar] = i + '0';

            typingTargetVoltage = atof(voltageChars);
            if (currentVoltageChar == 0 && typingTargetVoltage > MAX_VOLTAGE) {
                typingTargetVoltage = MIN_VOLTAGE;
            }

            currentVoltageChar = currentVoltageChar == 0 ? 3 : currentVoltageChar-1;
            currentVoltageChar = currentVoltageChar == 2 ? currentVoltageChar-1 : currentVoltageChar;

            updateDisplay();
            return;
        }
    }

    if ((currentAltMode == States::SAVE_MODE || currentAltMode == States::RECOVER_MODE) && command == CMD_BACK) {
        currentAltMode = currentAltMode == States::SAVE_MODE ? States::ON : States::SAVE_MODE;
        updateDisplay();
        return;
    }

    if (currentAltMode == States::VOLTAGE_MODE && command == CMD_EQ) {
        isTypingVoltage = true;
        strcpy(voltageChars, "00.0");
        typingTargetVoltage = 0;
        currentVoltageChar = 3;
        updateDisplay();
        return;
    } else if (command == CMD_PAUSE) {
        isTypingVoltage = false;
        currentConfig->targetVoltage = typingTargetVoltage > MAX_VOLTAGE ? 0 : typingTargetVoltage;
        updateDisplay();
        return;
    }

    switch (command) {
        case CMD_POWER: {
            updateStates(States::ON, 0);
            break;
        }
        case TEMP_PLUS: {
            updateStates(States::USER_TEMP, currentConfig->userTemp+10);
            break;
        }
        case TEMP_MINUS: {
            updateStates(States::USER_TEMP, currentConfig->userTemp-10);
            break;
        }
        case FAN_PLUS: {
            updateStates(States::FAN, currentConfig->fanSpeed+1);
            break;
        }
        case FAN_MINUS: {
            updateStates(States::FAN, currentConfig->fanSpeed-1);
            break;
        }
        case CMD_MODE: {
            cycleAltMode();
            updateStates(currentAltMode, 0);
            break;
        }
        case CMD_STOP: {
            forceHeatingOff = !forceHeatingOff;
            break;
        }
        case CMD_BACK: {
            currentAltMode = States::RECOVER_MODE;
            updateDisplay();
            break;
        }
    }
}

void setup() {
    pinMode(HEAT_PIN, OUTPUT);
    pinMode(PS_ON_PIN, OUTPUT);
    pinMode(FAN_CONTROL_PIN, OUTPUT);
    pinMode(FAN_POWER_PIN, OUTPUT);
    digitalWrite(PS_ON_PIN, HIGH);
    digitalWrite(HEAT_PIN, HEAT_OFF_VAL);
    setupTimer9and10();

    Inductance::setup(INDUCTANCE_SIGNAL_PIN, INDUCTANCE_INPUT_PIN, INDUCTANCE_CAPACITOR);
    recoverConfigs();
    updateStates(States::FAN, currentConfig->fanSpeed, false);

    IrReceiver.begin(IR_PIN, false);
}

void loop() {
    if (isOn && millis() - tempUpdateTimer > 2000) {
        currentTemp = tempSensor.getCelsius();
        updateStates(States::TEMP, currentTemp);

        if (currentAltMode == States::LC_METER_MODE) {
            Inductance::measureInductance(currentInductance, currentFrequency);
            updateDisplay();
        }

        tempUpdateTimer = millis();
    }

    if (IrReceiver.decode()) {
        processIrCommand();
        IrReceiver.resume();
    }
}