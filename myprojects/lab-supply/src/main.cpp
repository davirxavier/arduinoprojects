// #define SIMULATION

#include <Arduino.h>
#include <INA226_WE.h>
#include <IRremote.hpp>
#include <EEPROMWearLevel.h>
#include <lc.h>
#include <util.h>
#include <OneButton.h>
#include "Wire.h"
#include "LiquidCrystal_I2C.h"
#include "util.h"
#include <float.h>

#define LCD_ADDRESS 0x27
#define LCD_ROWS 2
#define LCD_COLS 16
#define DISPLAY_BUFSIZE (LCD_COLS + 1)

#define INA_ADDRESS 0x40
#define INA_SHUNT_RESISTANCE 0.0015
#define INA_SHUNT_MAX_CURRENT 50
#define INA_READ_INTERVAL 10

#define IR_PIN 11
#define BUTTON_PIN A0

#define CAP_FAST_CHARGE_PIN 6
#define CAP_SLOW_CHARGE_PIN 2
#define CAP_DISCHARGE_PIN 3
#define CAP_SENSE_PIN A3
#define CAP_FAST_CHARGE_RESISTANCE 1000 // ~1Mohm
#define CAP_SLOW_CHARGE_RESISTANCE 972000 // ~1kohm

#define INDUCT_SIGNAL_PIN 4
#define INDUCT_PULSE_PIN 5
#define INDUCT_CAPACITANCE_F 1.E-6 // 1uF

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
#ifdef SIMULATION
#define CMD_POWER 162
#define CMD_MODE_NEXT 226
#define CMD_MODE_BACK 194
#define CMD_MORE 2
#define CMD_MORE_MORE 144
#define CMD_LESS 152
#define CMD_LESS_LESS 224
#define CMD_CHANGE_SUBMODE 34
#define CMD_CHANGE_ACC 168
#define CMD_PAUSE 82
#define CMD_RESET 74
#else
#define CMD_POWER 69
#define CMD_MODE_NEXT 68
#define CMD_MODE_BACK 64
#define CMD_MORE 25
#define CMD_MORE_MORE 21
#define CMD_LESS 22
#define CMD_LESS_LESS 7
#define CMD_CHANGE_SUBMODE 67
#define CMD_CHANGE_ACC 71
#define CMD_PAUSE 9
#define CMD_RESET 70
#endif

#define EEPROM_VER 1
#define EEPROM_INDEXES 1

// #define ENABLE_LOG
#ifdef ENABLE_LOG
#define LOG(s) Serial.print(s)
#define LOGN(s) Serial.println(s)
#else
#define LOG(s)
#define LOGN(s)
#endif

OneButton button(BUTTON_PIN, true, true);

INA226_WE ina(INA_ADDRESS);
int inaError = -1;
unsigned long lastMeasurement = 0;

LiquidCrystal_I2C display(LCD_ADDRESS, LCD_COLS, LCD_ROWS);
char displayBuffer[LCD_COLS + 1]{};

unsigned long lastUpdateDisplay = 0;

unsigned long commandTimer = 0;
unsigned long saveStateTimer = 0;
bool hasSaveState = false;
bool paused = false;

namespace Mode
{
    enum Mode
    {
        DC,
        AC,
        INDUCTANCE,
        CAPACITANCE,
        BOOST,
    };

    bool isAc(Mode mode)
    {
        return mode == AC;
    }

    bool isVoltage(Mode mode)
    {
        return mode == AC || mode == DC;
    }

    enum Submode
    {
        VOLTAGE_MAX,
        VOLTAGE_MIN,
        VOLTAGE_CURRENT,
        CURRENT_MAX,
        CURRENT_MIN,
    };

    enum Accuracy
    {
        ACC_LOW,
        ACC_MED,
        ACC_HI
    };
}

struct State
{
    Mode::Mode mode = Mode::DC;
    Mode::Submode modeParam = Mode::VOLTAGE_MAX;
    Mode::Accuracy accuracy = Mode::ACC_MED;
    double currentTargetVoltage = 0;
};

struct UnitValues
{
    double value;
    char units[5][10];
    uint8_t unitsSize;
    double thresholds[5];
    double divisionFactors[5];
};

State currentState{};

#ifdef SIMULATION
double currentValue = 1234.2;
double currentSubValue = 1231.4002456;
#else
double currentValue = 0;
double currentSubValue = 0;
#endif

double currentOutputVoltage = 0;
bool fastCharge = false;
bool modeChanging = false;

void saveState()
{
    LOGN("Save state");
    EEPROMwl.put(0, currentState);
}

void recoverState()
{
    EEPROMwl.get(0, currentState);
}

inline void chooseDefinitiveUnits(UnitValues *values, char definitiveUnit[])
{
    uint8_t lastThreshold = 0;

    for (uint8_t i = 0; i < values->unitsSize; i++)
    {
        if (values->value >= values->thresholds[i])
        {
            values->value /= values->divisionFactors[i];
            lastThreshold = i;
        }
    }

    strcpy(definitiveUnit, values->units[lastThreshold]);
}

inline void printVal(const char prefix[], UnitValues *val1, UnitValues *val2 = nullptr, bool err = false)
{
    char definitiveUnit1[10]{};
    chooseDefinitiveUnits(val1, definitiveUnit1);

    if (val2 != nullptr)
    {
        char definitiveUnit2[10]{};
        chooseDefinitiveUnits(val2, definitiveUnit2);

        snprintf(displayBuffer, DISPLAY_BUFSIZE, "%s %s/%s%s", prefix, definitiveUnit1, definitiveUnit2, paused ? "*" : "");
        display.setCursor(0, 0);
        printAndPad(displayBuffer, display);

        if (!err)
        {
            char val1str[8]{};
            printFloatSmart(val1->value, val1str, 7, 3);

            char val2str[9]{};
            printFloatSmart(val2->value, val2str, 8, 3);

            snprintf(displayBuffer, DISPLAY_BUFSIZE, "%7s/%-8s", val1str, val2str);
            display.setCursor(0, 1);
            printAndPad(displayBuffer, display);
        }
    }
    else
    {
        snprintf(displayBuffer, DISPLAY_BUFSIZE, "%s %s%s", prefix, definitiveUnit1, paused ? "*" : "");
        display.setCursor(0, 0);
        printAndPad(displayBuffer, display);

        if (!err)
        {
            char val1str[17]{};
            printFloatSmart(val1->value, val1str, 16, 3);

            display.setCursor(0, 1);
            printAndPad(val1str, display);
        }
    }
}

inline void updateDisplay()
{
    display.cursor_off();
    display.setCursor(0, 0);

    switch (currentState.mode)
    {
    case Mode::AC:
    case Mode::DC:
        {
            UnitValues val1{
                .value = currentValue,
                .units = {"mV", "V"},
                .unitsSize = 2,
                .thresholds = {1, 950},
                .divisionFactors = {1, 1000}
            };

            UnitValues val2{
                .value = currentSubValue,
                .units = {"1234567", "1234567"},
                .unitsSize = 2,
                .thresholds = {1, 950},
                .divisionFactors = {1, 1000}
            };

            switch (currentState.modeParam)
            {
            case Mode::VOLTAGE_MAX:
                {
                    strcpy(val2.units[0], "mV(max)");
                    strcpy(val2.units[1], "V(max)");
                    break;
                }
            case Mode::VOLTAGE_MIN:
                {
                    strcpy(val2.units[0], "mV(min)");
                    strcpy(val2.units[1], "V(min)");
                    break;
                }
            case Mode::VOLTAGE_CURRENT:
                {
                    strcpy(val2.units[0], "mA");
                    strcpy(val2.units[1], "A");
                    break;
                }
            case Mode::CURRENT_MAX:
                {
                    strcpy(val1.units[0], "mA");
                    strcpy(val1.units[1], "A");

                    strcpy(val2.units[0], "mA(max)");
                    strcpy(val2.units[1], "A(max)");
                    break;
                }
            case Mode::CURRENT_MIN:
                {
                    strcpy(val1.units[0], "mA");
                    strcpy(val1.units[1], "A");

                    strcpy(val2.units[0], "mA(min)");
                    strcpy(val2.units[1], "A(min)");
                    break;
                }
            }

            switch (currentState.accuracy)
            {
            case Mode::ACC_LOW:
                {
                    printVal(Mode::isAc(currentState.mode) ? "AC L" : "DC L", &val1, &val2);
                    break;
                }
            case Mode::ACC_MED:
                {
                    printVal(Mode::isAc(currentState.mode) ? "AC M" : "DC M", &val1, &val2);
                    break;
                }
            case Mode::ACC_HI:
                {
                    printVal(Mode::isAc(currentState.mode) ? "AC H" : "DC H", &val1, &val2);
                    break;
                }
            default:;
            }
            break;
        }
    case Mode::INDUCTANCE:
        {
            UnitValues val1{
                .value = currentValue,
                .units = {"nH", {(char)0xE4, 'H', '\0'}, "mH"},
                .unitsSize = 3,
                .thresholds = {1, 950, 950},
                .divisionFactors = {1, 1000, 1000}
            };

            UnitValues val2{
                .value = currentSubValue,
                .units = {"hz", "khz", "mhz"},
                .unitsSize = 3,
                .thresholds = {1, 1000, 1000},
                .divisionFactors = {1, 1000, 1000}
            };

            printVal("L", &val1, &val2, currentValue < 0);

            if (currentValue < 0)
            {
                display.setCursor(0, 1);
                printAndPad("READ ERROR", display);
            }
            break;
        }
    case Mode::CAPACITANCE:
        {
            UnitValues val1{
                .value = currentValue,
                .units = {"nF", {(char)0xE4, 'F', '\0'}, "mF"},
                .unitsSize = 3,
                .thresholds = {1, 500, 5000},
                .divisionFactors = {1, 1000, 1000}
            };

            printVal(fastCharge ? "C >10uF" : "C <10uF", &val1, nullptr, currentValue < 0);

            if (currentValue < 0)
            {
                display.setCursor(0, 1);
                switch ((int) currentValue)
                {
                case Capacitance::CHARGE_TIMEOUT:
                    {
                        printAndPad("CHRG TMOUT", display);
                        break;
                    }
                case Capacitance::DISCHARGE_TIMEOUT:
                    {
                        printAndPad("DSCHRG TMOUT", display);
                        break;
                    }
                default:;
                }
            }
            break;
        }
    case Mode::BOOST:
        {
            UnitValues val1{
                .value = currentState.currentTargetVoltage,
                .units = {"mV(tg)", "V(tg)"},
                .unitsSize = 2,
                .thresholds = {1, 950},
                .divisionFactors = {1, 1000}
            };

            UnitValues val2{
                .value = currentOutputVoltage,
                .units = {"mV", "V"},
                .unitsSize = 2,
                .thresholds = {1, 950},
                .divisionFactors = {1, 1000}
            };

            printVal("ADJ", &val1, &val2);
            break;
        }
    default:
        {
            display.setCursor(0, 0);
            printAndPad("MODE NOT IMPL", display);
        }
    }
}

void parseValuesIna()
{
    switch (currentState.modeParam)
    {
    case Mode::VOLTAGE_MAX:
        {
            currentValue = abs(ina.getBusVoltage_V())*1000.0;

            if (currentValue > currentSubValue)
            {
                currentSubValue = currentValue;
            }
            break;
        }
    case Mode::VOLTAGE_MIN:
        {
            currentValue = abs(ina.getBusVoltage_V())*1000.0;

            if (currentSubValue == 0)
            {
                currentSubValue = DBL_MAX;
            }

            if (currentValue < currentSubValue)
            {
                currentSubValue = currentValue;
            }
            break;
        }
    case Mode::VOLTAGE_CURRENT:
        {
            currentValue = abs(ina.getBusVoltage_V())*1000.0;
            currentSubValue = ina.getCurrent_mA();
            break;
        }
    case Mode::CURRENT_MAX:
        {
            currentValue = abs(ina.getCurrent_mA());

            if (currentValue > currentSubValue)
            {
                currentSubValue = currentValue;
            }
            break;
        }
    case Mode::CURRENT_MIN:
        {
            currentValue = abs(ina.getCurrent_mA());

            if (currentSubValue == 0)
            {
                currentSubValue = DBL_MAX;
            }

            if (currentValue < currentSubValue)
            {
                currentSubValue = currentValue;
            }
            break;
        }
    }
}

void resetVals()
{
    currentValue = 0;
    currentSubValue = 0;
}

void nextMode(bool cycle = false)
{
    int newState = currentState.mode == Mode::BOOST ? (cycle ? 0 : Mode::BOOST) : currentState.mode+1;
    currentState.mode = static_cast<Mode::Mode>(newState);
    hasSaveState = true;
    saveStateTimer = millis();
    resetVals();

    if (Mode::isVoltage(currentState.mode) && !Mode::isAc(currentState.mode))
    {
        INA226_CONV_TIME convTime = CONV_TIME_332;
        INA226_AVERAGES averages = AVERAGE_1;

        switch (currentState.accuracy)
        {
        case Mode::ACC_LOW:
            {
                convTime = CONV_TIME_332;
                averages = AVERAGE_1;
                break;
            }
        case Mode::ACC_MED:
            {
                convTime = CONV_TIME_588;
                averages = AVERAGE_4;
                break;
            }
        case Mode::ACC_HI:
            {
                convTime = CONV_TIME_2116;
                averages = AVERAGE_128;
                break;
            }
        }

        ina.setConversionTime(convTime);
        ina.setAverage(averages);
    }
}

void changeSubmode()
{
    if (Mode::isVoltage(currentState.mode))
    {
        currentState.modeParam = static_cast<Mode::Submode>(currentState.modeParam == Mode::CURRENT_MIN ? 0 : currentState.modeParam+1);
        hasSaveState = true;
        saveStateTimer = millis();
        resetVals();
    }
    else if (currentState.mode == Mode::CAPACITANCE)
    {
        fastCharge = !fastCharge;
        resetVals();
    }
}

void changeAcc()
{
    currentState.accuracy = static_cast<Mode::Accuracy>(currentState.accuracy == Mode::ACC_HI ? 0 : currentState.accuracy+1);
    hasSaveState = true;
    saveStateTimer = millis();
}

void parseControls()
{
    if (millis() - commandTimer < 75)
    {
        return;
    }
    commandTimer = millis();

    auto command = IrReceiver.decodedIRData.command;
    if (command == CMD_MODE_NEXT)
    {
        nextMode();
    }
    else if (command == CMD_MODE_BACK)
    {
        int newState = currentState.mode == 0 ? 0 : currentState.mode-1;
        currentState.mode = static_cast<Mode::Mode>(newState);
        hasSaveState = true;
        saveStateTimer = millis();
        resetVals();
    }
    else if ((command == CMD_LESS || command == CMD_LESS_LESS) && currentState.mode == Mode::BOOST)
    {
        currentState.currentTargetVoltage -= command == CMD_LESS_LESS ? 1.0 : 0.1;
        if (currentState.currentTargetVoltage <= 0.1)
        {
            currentState.currentTargetVoltage = 0.1;
        }

        updateDisplay();
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
    else if (command == CMD_CHANGE_SUBMODE)
    {
        changeSubmode();
    }
    else if (command == CMD_CHANGE_ACC)
    {
        changeAcc();
    }
    else if (command == CMD_PAUSE)
    {
        paused = !paused;
    }
    else if (command == CMD_RESET && !paused)
    {
        resetVals();
    }

    updateDisplay();
}

void setup()
{
#ifdef ENABLE_LOG
    Serial.begin(9600);
#endif

    // Wire.setClock(400000L);

    EEPROMwl.begin(EEPROM_VER, EEPROM_INDEXES);
    recoverState();

    display.init();
    display.display();
    display.backlight();

    IrReceiver.begin(IR_PIN, false);

    Inductance::setup(INDUCT_SIGNAL_PIN, INDUCT_PULSE_PIN);
    Capacitance::setup(CAP_FAST_CHARGE_PIN, CAP_SLOW_CHARGE_PIN, CAP_DISCHARGE_PIN, CAP_SENSE_PIN);

    button.setPressMs(1500);
    button.setLongPressIntervalMs(1000);
    button.attachLongPressStart([]()
    {
        modeChanging = true;
    });
    button.attachDuringLongPress([]()
    {
        nextMode(true);
        updateDisplay();
    });
    button.attachLongPressStop([]()
    {
       modeChanging = false;
    });
    button.attachClick([]()
    {
        changeSubmode();
        updateDisplay();
    });

    if (ina.init())
    {
        inaError = -1;
        ina.setResistorRange(INA_SHUNT_RESISTANCE, INA_SHUNT_MAX_CURRENT);
        ina.waitUntilConversionCompleted();
    }
    else
    {
        inaError = 0;
    }
}

void loop()
{
    button.tick();

    if (modeChanging)
    {
        return;
    }

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

    if (paused)
    {
        return;
    }

    if (Mode::isVoltage(currentState.mode) &&
        !Mode::isAc(currentState.mode) &&
        inaError < 0 &&
        millis() - lastMeasurement > INA_READ_INTERVAL)
    {
        parseValuesIna();
        lastMeasurement = millis();
    }

    if (currentState.mode == Mode::INDUCTANCE && millis() - lastMeasurement > 1500)
    {
        Inductance::measure(INDUCT_SIGNAL_PIN, INDUCT_PULSE_PIN, INDUCT_CAPACITANCE_F, currentValue, currentSubValue);
        lastMeasurement = millis();
    }

    if (currentState.mode == Mode::CAPACITANCE && millis() - lastMeasurement > 1500)
    {
        currentValue = Capacitance::measure(CAP_FAST_CHARGE_PIN,
            CAP_SLOW_CHARGE_PIN,
            CAP_DISCHARGE_PIN,
            CAP_SENSE_PIN,
            fastCharge ? CAP_FAST_CHARGE_RESISTANCE : CAP_SLOW_CHARGE_RESISTANCE,
            fastCharge);

        lastMeasurement = millis();
    }
}
