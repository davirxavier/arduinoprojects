#include <Arduino.h>
#include "4display.h"

uint8_t digitPins[4] = {6, 5, 3, 9};
uint8_t pins[7] = {8, 2, 13, 11, 10, 7, A0};
uint8_t dp = 12;
unsigned long counter = 0;
int val = 0;

FourDisplay d(pins, digitPins, dp);

void setup() {
    d.eraseAll();
    d.turnAllOff();
}

void loop() {
    if (millis() - counter > 5) {
        val = val+1 <= 9999 ? val+1 : 0;
        counter = millis();
    }

    d.write4(val, false);
}