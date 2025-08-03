#include <Arduino.h>
#include "Wire.h"
#include "LiquidCrystal_I2C.h"
#include <IRremote.hpp>
#include <EEPROMWearLevel.h>
#include <float.h>
#include <lc.h>

#define LCD_ADDRESS 0x27
#define LCD_ROWS 2
#define LCD_COLS 16

#define IR_PIN 3

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
#define CMD_MODE_NEXT 68
#define CMD_MODE_BACK 64
#define CMD_MORE 25
#define CMD_MORE_MORE 21
#define CMD_LESS 22
#define CMD_LESS_LESS 7

#define EEPROM_VER 0
#define EEPROM_INDEXES 1

LiquidCrystal_I2C display(LCD_ADDRESS, LCD_COLS, LCD_ROWS);
char displayBuffer[LCD_COLS + 1];
unsigned long lastUpdateDisplay = 0;

unsigned long commandTimer = 0;
unsigned long saveStateTimer = 0;
bool hasSaveState = false;

namespace Mode
{
    enum Mode
    {
        DC_VOLTAGE,
        DC_CURRENT,
        AC_VOLTAGE,
        AC_CURRENT,
        INDUCTANCE,
        CAPACITANCE,
        BOOST,
    };

    enum Submode
    {
        MAX,
        MIN,
        AVERAGE
    };
}

struct State
{
    Mode::Mode mode = Mode::DC_VOLTAGE;
    Mode::Submode modeParam = Mode::AVERAGE;
    double currentTargetVoltage = 0;
};

State currentState{};
double currentVoltage = 0;
double currentCurrent = 0;
double currentLCReading = 0;
double currentFreqReading = 0;

double currentOutputVoltage = 0;

void saveState()
{
    EEPROMwl.put(0, currentState);
}

void recoverState()
{
    EEPROMwl.get(0, currentState);
}

void printModeParam()
{
    if (currentState.modeParam == Mode::MAX)
    {
        display.print("MAX");
    }
    else if (currentState.modeParam == Mode::MIN)
    {
        display.print("MIN");
    }
    else
    {
        display.print("AVG");
    }
}

void printVal(double value,
              const char prefix[],
              uint8_t decimalSize,
              const char mainUnit[],
              char firstUnit,
              char secondUnit = ' ',
              double secondVal = DBL_MAX,
              char thirdUnit = ' ',
              double thirdVal = DBL_MAX)
{
    display.print(prefix);

    uint8_t numberSize = LCD_COLS - strlen(prefix) - strlen(mainUnit) - 1;
    if (numberSize <= 0)
    {
        display.print("INV SIZES");
        return;
    }

    char formatBuffer[LCD_COLS + 1];
    snprintf(formatBuffer, LCD_COLS + 1, "%%%d.%df%%c%s", numberSize, decimalSize, mainUnit);

    char unit = firstUnit;
    if (value > secondVal)
    {
        value = value / 1000.0;
        unit = secondUnit;
    }

    if (value > thirdVal)
    {
        value = value / 1000.0;
        unit = thirdUnit;
    }

    snprintf(displayBuffer, LCD_COLS + 1, formatBuffer, value, unit);
}

void updateDisplay()
{
    display.cursor_off();
    display.clear();
    display.setCursor(0, 0);

    switch (currentState.mode)
    {
    case Mode::DC_VOLTAGE:
        {
            printModeParam();
            display.print(" VDC");
            display.setCursor(0, 1);
            printVal(currentVoltage, "", 3, "V", 'm', ' ', 1500);
            break;
        }
    case Mode::AC_VOLTAGE:
        {
            printModeParam();
            display.print(" VAC");
            display.setCursor(0, 1);
            printVal(currentVoltage, "", 3, "V", 'm', ' ', 1500);
            break;
        }
    case Mode::DC_CURRENT:
        {
            printModeParam();
            display.print(" DC CURRENT");
            display.setCursor(0, 1);
            printVal(currentCurrent, "", 3, "A", 'm', ' ', 1500);
            break;
        }
    case Mode::AC_CURRENT:
        {
            printModeParam();
            display.print(" AC CURRENT");
            display.setCursor(0, 1);
            printVal(currentCurrent, "", 3, "A", 'm', ' ', 1500);
            break;
        }
    case Mode::INDUCTANCE:
        {
            printVal(currentLCReading, "L: ", 3, "H", (char)0xE4, 'm', 3000, ' ', 3000);
            display.setCursor(0, 1);
            printVal(currentLCReading, "F: ", 2, "hz", ' ', 'k', 10000, 'm', 10000);
            break;
        }
    case Mode::CAPACITANCE:
        {
            printVal(currentLCReading, "C: ", 3, "F", (char)0xE4, 'm', 3000, ' ', 3000);
            break;
        }
    case Mode::BOOST:
        {
            snprintf(displayBuffer, LCD_COLS + 1, "TARG: %9.2fV", currentState.currentTargetVoltage);
            display.print(displayBuffer);

            display.setCursor(0, 1);
            snprintf(displayBuffer, LCD_COLS + 1, "CURR: %9.2fV", currentOutputVoltage);
            break;
        }
    }
}

void parseControls()
{
    IrReceiver.resume();

    if (millis() - commandTimer < 75)
    {
        return;
    }
    commandTimer = millis();

    auto command = IrReceiver.decodedIRData.command;
    if (command == CMD_MODE_NEXT)
    {
        int newState = currentState.mode == Mode::BOOST ? Mode::BOOST : currentState.mode+1;
        currentState.mode = static_cast<Mode::Mode>(newState);
        updateDisplay();
        hasSaveState = true;
        saveStateTimer = millis();
    }
    else if (command == CMD_MODE_BACK)
    {
        int newState = currentState.mode == 0 ? 0 : currentState.mode-1;
        currentState.mode = static_cast<Mode::Mode>(newState);
        updateDisplay();
        hasSaveState = true;
        saveStateTimer = millis();
    }
    else if ((command == CMD_LESS || command == CMD_LESS_LESS) && currentState.mode == Mode::BOOST)
    {
        currentState.currentTargetVoltage -= command == CMD_LESS_LESS ? 1.0 : 0.1;
        if (currentState.currentTargetVoltage <= 0.1)
        {
            currentState.currentTargetVoltage = 0.1;
        }

        hasSaveState = true;
        saveStateTimer = millis();
    }
    else if ((command == CMD_MORE || command == CMD_MORE_MORE) && currentState.mode == Mode::BOOST)
    {
        currentState.currentTargetVoltage += command == CMD_MORE_MORE ? 1.0 : 0.1;
        if (currentState.currentTargetVoltage > 32)
        {
            currentState.currentTargetVoltage = 32;
        }

        hasSaveState = true;
        saveStateTimer = millis();
    }
}

void setup()
{
    Serial.begin(9600);
    Inductance::setup();

    EEPROMwl.begin(EEPROM_VER, EEPROM_INDEXES);

    display.init();
    display.display();
    display.backlight();

    IrReceiver.begin(IR_PIN, false);
}

void loop()
{
    if (millis() - lastUpdateDisplay > 1500)
    {
        updateDisplay();
        lastUpdateDisplay = millis();
    }

    if (hasSaveState && millis() - saveStateTimer > 2000)
    {
        saveState();
        hasSaveState = false;
        saveStateTimer = millis();
    }

    if (IrReceiver.decode()) {
        parseControls();
        IrReceiver.resume();
    }
}
