//
// Created by Davi on 28/05/2023.
//
#include <Arduino.h>
#include "4Display.h"

uint8_t pinsByIndex[10][7] = {
    {0, 1, 2, 3, 4, 5, 255},
    {1, 2, 255, 255, 255, 255, 255},
    {0, 1, 6, 4, 3, 255, 255},
    {0, 1, 2, 3, 6, 255, 255},
    {1, 2, 6, 5, 255, 255, 255},
    {0, 5, 6, 2, 3, 255, 255},
    {0, 5, 6, 4, 3, 2, 255},
    {0, 1, 2, 255, 255, 255, 255},
    {0, 1, 2, 3, 4, 5, 6},
    {0, 1, 2, 3, 5, 6, 255}
};

FourDisplay::FourDisplay(uint8_t (&pins)[7], uint8_t (&digitPins)[4], uint8_t dotPin)
        : pins(pins), digitPins(digitPins), dotPin(dotPin) {
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 7; j++) {
            pinsByNumber[i][j] = pinsByIndex[i][j] == 255 ? 255 : pins[pinsByIndex[i][j]];
        }
    }

    brightness = 500;

    if (dotPin != 255) {
        pinMode(dotPin, OUTPUT);
    }

    for (auto p : pins) {
        if (p != 255) {
            pinMode(p, OUTPUT);
        }
    }
    for (auto p : digitPins) {
        if (p != 255) {
            pinMode(p, OUTPUT);
        }
    }
}

void FourDisplay::write1(uint8_t digitIndex, uint8_t numberWithOneDigit, bool displayDot) {
    turnAllOff();
    digitalWrite(digitPins[digitIndex], LOW);

    eraseAll();
    if (numberWithOneDigit != 255) {
        for (auto pin: pinsByNumber[numberWithOneDigit % 10]) {
            if (pin > 0 && pin != 255) {
                digitalWrite(pin, HIGH);
            }
        }
    }
    digitalWrite(dotPin, displayDot ? HIGH : LOW);
    delayMicroseconds(brightness);
    eraseAll();
    turnAllOff();
}

void FourDisplay::write4(int16_t number, bool displayDot) {
    uint8_t digits[4] = {255 ,255, 255, 255};

    if (number >= 1000) {
        digits[0] = (number/1000) % 10;
    }

    if (number >= 100) {
        digits[1] = (number/100) % 10;
    }

    if (number >= 10) {
        digits[2] = (number/10) % 10;
    }

    digits[3] = number%10;

    for (int i = 0; i < 4; i++) {
        short dg = digits[i];
        write1(i, dg, displayDot);
    }

}

void FourDisplay::turnAllOff() {
    for (auto dpin : digitPins) {
        if (dpin != 255) {
            digitalWrite(dpin, HIGH);
        }
    }
}

void FourDisplay::eraseAll() {
    for (auto p : pins) {
        if (p != 255) {
            digitalWrite(p, LOW);
        }
    }
}