//
// Created by Davi on 13/06/2023.
//

/*
 * Steps
 * Turn on
 *  - turn on fan
 *  - wait for start timeout (minimum 5 minutes)
 *  - turn on compressor for the first time
 *  - check temperature periodically
 *      - if room temp is above user set temp + upper limit and the switch timeout has passed, open compressor and set switch timeout
 *      - if room temp is below user set temp - lower limit and half of the switch timeout has passed, turn off compressor and set switch on timeout (minimum 5 minutes)
 *
 * Turn off
 *  - set timer for turning off
 *  - after time has passed
 *  - turn off fan and compressor
 * */

#include <Arduino.h>
#include "AirConditioner.h"

AirConditioner::AirConditioner(uint8_t compressorTurnOnPin, uint8_t fanTurnOnPin)
    : compressorTurnOnPin(compressorTurnOnPin), fanTurnOnPin(fanTurnOnPin) {
    switchTimeoutMinutes = 10;
    temperatureLowerLimit = 0.8;
    temperatureUpperLimit = 1.5;
    switchTimeoutMillis = 0;
    userTemperature = 99;
    isCompressorRunning = false;
    isOn = false;
    lastTemp = 255;

    pinMode(compressorTurnOnPin, OUTPUT);
    pinMode(fanTurnOnPin, OUTPUT);
    toggleFan(false);
    toggleCompressor(false);
}

void AirConditioner::toggleFan(bool on) {
    if (on) {
        digitalWrite(fanTurnOnPin, HIGH);
    } else {
        digitalWrite(fanTurnOnPin, LOW);
    }
}

void AirConditioner::toggleCompressor(bool on) {
    if (on) {
        digitalWrite(compressorTurnOnPin, HIGH);
    } else {
        digitalWrite(compressorTurnOnPin, LOW);
    }

    isCompressorRunning = on;
}

void AirConditioner::start() {
    toggleFan(true);
    switchTimeoutMillis = millis();

    isOn = true;
}

void AirConditioner::stop() {
    toggleCompressor(false);
    toggleFan(false);
    switchTimeoutMillis = 0;

    isOn = false;
}

bool AirConditioner::isSwitchTimeoutOver() {
    return (unsigned long) (millis()-switchTimeoutMillis) > (switchTimeoutMinutes * 60000);
}

bool AirConditioner::isSwitchOffTimeoutOver() {
    return (unsigned long) (millis()-switchTimeoutMillis) > (unsigned long) ceil((switchTimeoutMinutes / 2.0) * 60000);
}

void AirConditioner::doChecks(double roomTemperature, double coilTemperature) {
    if (!isOn) {
        return;
    }

    if (isCompressorRunning && lastTemp != 255 && isSwitchTimeoutOver() && (roomTemperature > lastTemp || (lastTemp-roomTemperature) < 0.4)) {
        toggleCompressor(false);
        switchTimeoutMillis = millis();
        lastTemp = 255;
        return;
    }

    if (!isCompressorRunning && roomTemperature > ((double) userTemperature + temperatureUpperLimit) && isSwitchTimeoutOver()) {
        toggleCompressor(true);
        switchTimeoutMillis = millis();
        lastTemp = roomTemperature;
        return;
    }

    if (isCompressorRunning && roomTemperature <= (double) userTemperature - temperatureLowerLimit && isSwitchOffTimeoutOver()) {
        toggleCompressor(false);
        switchTimeoutMillis = millis();
        lastTemp = 255;
        return;
    }
}