//
// Created by xav on 10/25/24.
//

#include "buzzer.h"

void DxBuzzer::init(uint8_t pin, bool isTone) {
    this->pin = pin;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    noTone(pin);
    currentBeepQuantity = 0;
    beepTimer = 0;
    currentBeepDuration = 0;
    currentBeepDelay = 0;
    isBeeping = false;
    this->isTone = isTone;
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
        if (!isTone)
        {
            digitalWrite(pin, LOW);
        }
        return;
    }

    unsigned long currentMillis = millis();

    if (isBeeping && currentMillis - beepTimer > currentBeepDuration) {
        beepTimer = currentMillis;
        isBeeping = false;
        currentBeepQuantity--;
    } else if (!isBeeping && currentMillis - beepTimer > currentBeepDelay) {
        if (isTone)
        {
            tone(pin, 500, currentBeepDuration);
        }
        else
        {
            digitalWrite(pin, HIGH);
        }

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

    if (isTone)
    {
        noTone(pin);
    }
    else
    {
        digitalWrite(pin, LOW);
    }
}