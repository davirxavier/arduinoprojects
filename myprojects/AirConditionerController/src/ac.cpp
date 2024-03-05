#include <Arduino.h>
#include "IRremote.h"
#include "AirConditioner.h"
#include "AirConditionerRemote.h"
//#include "../lib/4Display/4Display.h"
#include "EEPROM.h"

#define MAX_TEMP 32
#define MIN_TEMP 18
#define DEGREES_UPPER_LIMIT 0.85
#define DEGREES_LOWER_LIMIT 1.35
#define SWITCH_TIMEOUT_MINUTES 4
#define TEMP_CHECK_INTERVAL_SECONDS 1
#define TEMP_AVERAGE_PERIOD_SECONDS 15
#define POWER_TIMEOUT_SECONDS 10
#define SAVE_CONFIG_SIGNATURE 0x2B
#define SAVE_DATA_TRUE_VALUE 0x1C
#define SAVE_DATA_FALSE_VALUE 0x1D

// Uncomment to enable test env
//#define IS_TEST_ENVIRONMENT
#define ENABLE_LOGGING

#ifdef IS_TEST_ENVIRONMENT
    #define RECV_PIN A3
    #define MAIN_RELAY_PIN 3
    #define FAN_RELAY_PIN A0
    #define TEMP_SENSOR A1
    #define BEEPER A2
#else
    #define RECV_PIN A3
    #define MAIN_RELAY_PIN 3
    #define FAN_RELAY_PIN 4
    #define TEMP_SENSOR A0
    #define BEEPER 10
#endif

#define TEMP_SENSOR_OFFSET 4.08

#define VERBOSE_SENSOR_ENABLED 0

#ifdef IS_TEST_ENVIRONMENT
    #define COMMAND_POWER 162
    #define COMMAND_MINUS 152
    #define COMMAND_PLUS 2
    #define COMMAND_BEEP 226
    #define COMMAND_TEMP 34
#endif

#ifdef IS_TEST_ENVIRONMENT
    #define TEMP_SENSOR_BETA 3950
    #define TEMP_SENSOR_RESISTANCE 10000
    #define TEMP_SENSOR_SERIES_RESISTOR 10000
#else
    #define TEMP_SENSOR_BETA 4150
    #define TEMP_SENSOR_RESISTANCE 12000
    #define TEMP_SENSOR_SERIES_RESISTOR 10000
#endif

#define THERMISTOR_CONSTANT_A 0.00102192985237609
#define THERMISTOR_CONSTANT_B 0.000241453242427025
#define THERMISTOR_CONSTANT_C -0.000000247620754758454
#define THERMISTOR_CONSTANT_D 0.000000165394923419592

//uint8_t POS_PINS[7] = {4, 5, 6, 7, 8, 9, 10};
//uint8_t DISPLAY_PINS[4] {255, 255, 12, 13};
//uint8_t DP_PIN = 11;
//FourDisplay display(POS_PINS, DISPLAY_PINS, DP_PIN);

IRrecv remote(RECV_PIN);
unsigned long remoteTimeout;
bool remoteResumed;
bool hasPowerTimeout;
unsigned long powerTimeout;

unsigned long tempCheckTimer;
uint8_t userTemp;
AirConditioner ac(MAIN_RELAY_PIN, FAN_RELAY_PIN);
AirConditionerRemote acReader;
double tempsOverPeriod[TEMP_AVERAGE_PERIOD_SECONDS];
uint8_t currentTempOverPeriod;

bool showRoomTemp;
bool beepActivated;
bool isBeeping;
unsigned long beepTimeout;

bool isOn;
bool hasTimeout;
int savedAddress;

double currentAvgTemp = MAX_TEMP;

void setUserTemp(uint8_t temp);
void updateDisplay();
void turn();
void startBeep();
double readTemp();
double readCoilTemp();

int getAddress() {
    int address = 0;
    for (int i = 0; i < 512; i++) {
        if (EEPROM.read(i) == SAVE_CONFIG_SIGNATURE) {
            address = i;
            break;
        }
    }

    return address+1;
}

void saveConfig() {
    bool hasTimeoutEeprom = EEPROM.read(savedAddress) == SAVE_DATA_TRUE_VALUE;
    if (hasTimeoutEeprom != hasTimeout) {
        EEPROM.write(savedAddress, hasTimeout ? SAVE_DATA_TRUE_VALUE : SAVE_DATA_FALSE_VALUE);
    }

    if (EEPROM.read(savedAddress-1) != SAVE_CONFIG_SIGNATURE) {
        EEPROM.write(savedAddress-1, SAVE_CONFIG_SIGNATURE);
    }
}

void readConfig() {
    int eepromRead = EEPROM.read(savedAddress);
    if (eepromRead == SAVE_DATA_TRUE_VALUE || eepromRead == SAVE_DATA_FALSE_VALUE) {
        hasTimeout = eepromRead == SAVE_DATA_TRUE_VALUE;
    } else {
        hasTimeout = false;
    }
}

void setup() {
#ifdef ENABLE_LOGGING
    Serial.begin(9600);
#endif

    isOn = false;
    beepActivated = true;

    tempCheckTimer = 0;
    userTemp = MAX_TEMP;
    ac.stop();
    ac.userTemperature = userTemp;
    ac.temperatureLowerLimit = DEGREES_LOWER_LIMIT;
    ac.temperatureUpperLimit = DEGREES_UPPER_LIMIT;
    ac.switchTimeoutMinutes = SWITCH_TIMEOUT_MINUTES;
    hasPowerTimeout = false;
    powerTimeout = 0;
    currentTempOverPeriod = 0;
    showRoomTemp = false;
    hasTimeout = false;

    savedAddress = getAddress();
    readConfig();
    ac.setFirstTimeout(!hasTimeout);

#ifdef ENABLE_LOGGING
    Serial.print("Initial state: ");
    Serial.print("Has timeout: ");
    Serial.println(hasTimeout ? "true" : "false");
#endif

    remote.enableIRIn();
    remoteTimeout = 0;
    remoteResumed = false;

//    display.eraseAll();
//    display.turnAllOff();

    pinMode(MAIN_RELAY_PIN, OUTPUT);
    pinMode(FAN_RELAY_PIN, OUTPUT);

    pinMode(BEEPER, OUTPUT);

    analogWrite(BEEPER, 0);
    startBeep();
}

void loop() {
    if (hasPowerTimeout && millis()-powerTimeout > POWER_TIMEOUT_SECONDS*1000) {
        hasPowerTimeout = false;
        powerTimeout = 0;
    }

    if (isBeeping && millis() > beepTimeout + 200) {
        isBeeping = false;
        analogWrite(BEEPER, 0);
    }

    if (!remoteResumed && millis()-remoteTimeout > 200) {
        remote.resume();
        remoteResumed = true;
    }

    if (remoteResumed && remote.decode()) {
        bool commandRead = false;

#ifdef IS_TEST_ENVIRONMENT
        switch (remote.decodedIRData.command) {
            case COMMAND_PLUS:
                if (isOn) {
                    commandRead = true;
                    setUserTemp(userTemp+1);
                }

                break;
            case COMMAND_MINUS:
                if (isOn) {
                    commandRead = true;
                    setUserTemp(userTemp-1);
                }

                break;
            case COMMAND_POWER:
                if (!hasPowerTimeout) {
                    hasPowerTimeout = true;
                    powerTimeout = millis();
                    commandRead = true;
                    turn();
                }
                break;
            case COMMAND_BEEP:
                if (isOn) {
                    commandRead = true;
                    startBeep();
                    beepActivated = !beepActivated;
                }
                break;
            case COMMAND_TEMP:
                if (isOn) {
                    commandRead = true;
                    startBeep();
                    showRoomTemp = !showRoomTemp;
                }
                break;
            default: ;
        }
#else
        if (acReader.readCommand(remote.decodedIRData.decodedRawDataArray,
                                 remote.decodedIRData.protocol,
                                 remote.decodedIRData.numberOfBits)) {
            if (acReader.powerToggle && !hasPowerTimeout) {
                hasPowerTimeout = true;
                powerTimeout = millis();
                turn();

                commandRead = true;
            }

            if (isOn) {
                setUserTemp(acReader.temp);

                if (acReader.swingToggle) {
                    showRoomTemp = !showRoomTemp;
                }

                if (!(acReader.powerToggle && hasPowerTimeout)) {
                    commandRead = true;
                }
            }

            acReader.resetToggles();
        }
#endif

        if (commandRead) {
            remoteTimeout = millis();
            startBeep();
        }
        remoteResumed = false;
    }

    if (isOn) {
        if (millis()-tempCheckTimer > TEMP_CHECK_INTERVAL_SECONDS*1000) {
            if (currentTempOverPeriod == TEMP_AVERAGE_PERIOD_SECONDS) {
                double tempAvg = 0;
                for (double temp : tempsOverPeriod) {
                    tempAvg = tempAvg + temp;
                }
                tempAvg = tempAvg / TEMP_AVERAGE_PERIOD_SECONDS;

                currentAvgTemp = tempAvg;

                ac.doChecks(tempAvg, readCoilTemp());
                currentTempOverPeriod = 0;

                hasTimeout = !ac.isSwitchTimeoutOver();
                saveConfig();
            }

            tempsOverPeriod[currentTempOverPeriod] = readTemp();
            currentTempOverPeriod++;
            tempCheckTimer = millis();

#ifdef ENABLE_LOGGING
            Serial.print("Current room temp: ");
            Serial.println(readTemp());
            Serial.print("Current user temp: ");
            Serial.println(userTemp);
            Serial.print("Compressor on: ");
            Serial.println(ac.isCompressorRunning);
#endif
        }

        updateDisplay();
    } else if (millis()-tempCheckTimer > TEMP_CHECK_INTERVAL_SECONDS*2*1000) {
        hasTimeout = !ac.isSwitchTimeoutOver();
        saveConfig();
    }
}

void setUserTemp(uint8_t temp) {
    userTemp = temp;

    if (userTemp > MAX_TEMP) {
        userTemp = MAX_TEMP;
    } else if (userTemp < MIN_TEMP) {
        userTemp = MIN_TEMP;
    }

    ac.userTemperature = userTemp;
}

void updateDisplay() {
//    if (showRoomTemp) {
//        display.write1(2, ((int)currentAvgTemp/10)%10, false);
//        display.write1(3, ((int)currentAvgTemp)%10, true);
//    } else {
//        display.write1(2, (userTemp/10)%10, false);
//        display.write1(3, userTemp % 10, false);
//    }
}

void turn() {
    isOn = !isOn;

    if (isOn) {
        ac.setFirstTimeout(!hasTimeout);
        ac.start();
    } else {
        ac.stop();
//        display.turnAllOff();
//        display.eraseAll();
    }
}

void startBeep() {
    if (beepActivated) {
        isBeeping = true;
        beepTimeout = millis();
        analogWrite(BEEPER, 255);
    }
}

float f_ReadTemp_ThABC(byte TPin, long THERMISTOR, float R1, float A, float B, float C, float D) {
    float res, temp;
    res = R1/(1023.0/((float)analogRead(TPin)) - 1.0);
    res = log(res);
    temp = A + B*res + C*res*res + D*res*res*res;
    temp = 1.0/temp;
    temp -= 273.15;
    return temp;
}

double readTemp() {
    return f_ReadTemp_ThABC(TEMP_SENSOR, TEMP_SENSOR_RESISTANCE, TEMP_SENSOR_SERIES_RESISTOR,
                            THERMISTOR_CONSTANT_A,
                            THERMISTOR_CONSTANT_B,
                            THERMISTOR_CONSTANT_C,
                            THERMISTOR_CONSTANT_D);
}

double readCoilTemp() {
    return 20; // TODO DO
}