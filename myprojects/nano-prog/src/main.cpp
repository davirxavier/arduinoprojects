#ifndef ARDUINO_AVR_ATTINY13

#define READY_LED A4
#define SERIAL_LED 6

#include <Arduino.h>
#include <stk500.h>
#include <hvsp.h>

void setup()
{
    Serial.begin(115200);
    HVSP::setup();

    pinMode(READY_LED, OUTPUT);
    digitalWrite(READY_LED, HIGH);

    pinMode(SERIAL_LED, OUTPUT);
    digitalWrite(SERIAL_LED, LOW);
}

void loop()
{
    if (Serial.available())
    {
        digitalWrite(SERIAL_LED, HIGH);
        PROGRAMMER::loop();
        digitalWrite(SERIAL_LED, LOW);
    }
}
#endif