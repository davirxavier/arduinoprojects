#include <Arduino.h>
#include "4display.h"
#include "IRremote.h"
#include "AirConditioner.h"
#include "Thermistor.h"

#define MAX_TEMP 30
#define MIN_TEMP 18
#define DEGREES_UPPER_LIMIT 0.85
#define DEGREES_LOWER_LIMIT 1.35
#define SWITCH_TIMEOUT_MINUTES 5
#define TEMP_CHECK_INTERVAL_SECONDS 1
#define TEMP_AVERAGE_PERIOD_SECONDS 15
#define POWER_TIMEOUT_SECONDS 15

#define RECV_PIN A3
#define MAIN_RELAY_PIN 3
#define FAN_RELAY_PIN A0
#define DEFROST_SENSOR A0
#define TEMP_SENSOR A1
#define BEEPER A2

#define TEMP_SENSOR_OFFSET 9.08

#define COMMAND_POWER 67
#define COMMAND_MINUS 7
#define COMMAND_PLUS 21
#define COMMAND_BEEP 9
#define COMMAND_TEMP 70

uint8_t POS_PINS[7] = {4, 5, 6, 7, 8, 9, 10};
uint8_t DISPLAY_PINS[4] {255, 255, 12, 13};
uint8_t DP_PIN = 11;
FourDisplay display(POS_PINS, DISPLAY_PINS, DP_PIN);

IRrecv remote(RECV_PIN);
unsigned long remoteTimeout;
bool remoteResumed;
bool hasPowerTimeout;
unsigned long powerTimeout;

unsigned long tempCheckTimer;
uint8_t userTemp;
AirConditioner ac(MAIN_RELAY_PIN, FAN_RELAY_PIN);
Thermistor tempSensor(TEMP_SENSOR);
double tempsOverPeriod[TEMP_AVERAGE_PERIOD_SECONDS];
uint8_t currentTempOverPeriod;

bool showRoomTemp;
bool beepActivated;
bool isBeeping;
unsigned long beepTimeout;

bool isOn;

void setUserTemp(uint8_t temp);
void updateDisplay();
void turn();
void startBeep();
double readTemp();
double readCoilTemp();

double currentAvgTemp = 30;

void setup() {
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

    remote.enableIRIn();
    remoteTimeout = 0;
    remoteResumed = false;

    display.eraseAll();
    display.turnAllOff();

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

    if (!remoteResumed && millis()-remoteTimeout > 650) {
        remote.resume();
        remoteResumed = true;
    }

    if (remoteResumed && remote.decode()) {
        bool commandRead = false;

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
            }

            tempsOverPeriod[currentTempOverPeriod] = readTemp();
            currentTempOverPeriod++;
            tempCheckTimer = millis();
        }

        updateDisplay();
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
    if (showRoomTemp) {
        display.write1(2, ((int)currentAvgTemp/10)%10, false);
        display.write1(3, ((int)currentAvgTemp)%10, true);
    } else {
        display.write1(2, (userTemp/10)%10, false);
        display.write1(3, userTemp % 10, false);
    }
}

void turn() {
    isOn = !isOn;

    if (isOn) {
        ac.start();
    } else {
        ac.stop();
        display.turnAllOff();
        display.eraseAll();
    }
}

void startBeep() {
    if (beepActivated) {
        isBeeping = true;
        beepTimeout = millis();
        analogWrite(BEEPER, 255);
    }
}

double readTemp() {
    return tempSensor.getTemp() + TEMP_SENSOR_OFFSET;
}

double readCoilTemp() {
    return 20; // TODO DO
}