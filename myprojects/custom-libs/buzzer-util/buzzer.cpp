//
// Created by xav on 10/25/24.
//

#include "buzzer.h"

DxBuzzer::DxBuzzer(uint8_t pin) : pin(pin) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    noTone(pin);
    currentBeepQuantity = 0;
    beepTimer = 0;
    currentBeepDuration = 0;
    currentBeepDelay = 0;
    isBeeping = false;
}

void DxBuzzer::beep(unsigned long beepDurationMs, unsigned int beepQuantity, unsigned long delayBetweenBeepsMs) {
    currentBeepDuration = beepDurationMs;
    currentBeepQuantity = beepQuantity;
    currentBeepDelay = delayBetweenBeepsMs;
    beepTimer = millis() - delayBetweenBeepsMs;
    isBeeping = false;
}

void DxBuzzer::update() {
    if (currentBeepQuantity == 0) {
        return;
    }

    unsigned long currentMillis = millis();

    if (isBeeping && currentMillis - beepTimer > currentBeepDuration) {
        beepTimer = currentMillis;
        isBeeping = false;
        currentBeepQuantity--;
    } else if (!isBeeping && currentMillis - beepTimer > currentBeepDelay) {
        tone(pin, 500, currentBeepDuration);
        beepTimer = currentMillis-1;
        isBeeping = true;
    }
}

void DxBuzzer::stop() {
    isBeeping = false;
    beepTimer = millis();
    currentBeepQuantity = 0;
    currentBeepDuration = 0;
    currentBeepDelay = 0;
    noTone(pin);
}