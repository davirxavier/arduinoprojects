//
// Created by xav on 10/25/24.
//

#include "led.h"

DxLed::DxLed(uint8_t pin, bool activeHigh) : pin(pin), activeHigh(activeHigh) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, activeHigh ? LOW : HIGH);
    currentBlinkQuantity = 0;
    blinkTimer = 0;
    currentBlinkDuration = 0;
    currentBlinkDelay = 0;
    isBlinking = false;
    onAfterEnd = false;
}

void DxLed::on()
{
    stop();
    digitalWrite(pin, activeHigh ? HIGH : LOW);
}

void DxLed::blink(unsigned long blinkDurationMs, unsigned int blinkQuantity, unsigned long delayBetweenBlinksMs, bool onAfterEnd) {
    stop();
    currentBlinkDuration = blinkDurationMs;
    currentBlinkQuantity = blinkQuantity;
    currentBlinkDelay = delayBetweenBlinksMs;
    blinkTimer = millis() - delayBetweenBlinksMs;
    isBlinking = false;
    this->onAfterEnd = onAfterEnd;
}

void DxLed::update() {
    if (currentBlinkQuantity == 0) {
        return;
    }

    unsigned long currentMillis = millis();

    if (isBlinking && currentMillis - blinkTimer > currentBlinkDuration) {
        blinkTimer = currentMillis;
        isBlinking = false;
        currentBlinkQuantity--;

        digitalWrite(pin, activeHigh ? LOW : HIGH);
        if (currentBlinkQuantity == 0 && onAfterEnd)
        {
            on();
        }
    } else if (!isBlinking && currentMillis - blinkTimer > currentBlinkDelay) {
        digitalWrite(pin, activeHigh ? HIGH : LOW);
        blinkTimer = currentMillis-1;
        isBlinking = true;
    }
}

void DxLed::stop() {
    isBlinking = false;
    blinkTimer = millis();
    currentBlinkQuantity = 0;
    currentBlinkDuration = 0;
    currentBlinkDelay = 0;
    onAfterEnd = false;
    digitalWrite(pin, activeHigh ? LOW : HIGH);
}