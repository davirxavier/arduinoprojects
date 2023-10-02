#include <Arduino.h>
#include "LowPower.h"

#define EXTERNAL_WAKE_UP_PIN 3
#define EXTERNAL_MODULE_PIN 4
#define TRANSISTOR_PIN 5

void wakeUpByExternal() {
    digitalWrite(TRANSISTOR_PIN, LOW);
    detachInterrupt(digitalPinToInterrupt(EXTERNAL_WAKE_UP_PIN));
}

void handle() {
    digitalWrite(TRANSISTOR_PIN, HIGH);
    delay(1000);

    digitalWrite(EXTERNAL_MODULE_PIN, LOW);
    delay(100);
    digitalWrite(EXTERNAL_MODULE_PIN, HIGH);
    delay(500);
    digitalWrite(EXTERNAL_MODULE_PIN, LOW);
    delay(100);
    digitalWrite(EXTERNAL_MODULE_PIN, HIGH);

    attachInterrupt(digitalPinToInterrupt(EXTERNAL_WAKE_UP_PIN), wakeUpByExternal, FALLING);
    LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
}

void sleep(int minutes) {
    int sleepTimes = ceil((float) (minutes*60)/8);
    for (int i = 0; i < sleepTimes; i++) {
        LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    }
}

void setup() {
    pinMode(TRANSISTOR_PIN, OUTPUT);
    digitalWrite(TRANSISTOR_PIN, LOW);

    pinMode(EXTERNAL_MODULE_PIN, OUTPUT);
    digitalWrite(EXTERNAL_MODULE_PIN, HIGH);

    pinMode(EXTERNAL_WAKE_UP_PIN, INPUT_PULLUP);
}

void loop() {
    handle();
    sleep(1);
}