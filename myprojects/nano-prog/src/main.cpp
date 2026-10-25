#ifndef ARDUINO_AVR_ATTINY13

#define READY_LED A4
#define MODE_LED 6
#define MODE_LED_ON analogWrite(MODE_LED, 50)
#define MODE_LED_OFF analogWrite(MODE_LED, 0)

#include <Arduino.h>
#include <stk500.h>
#include <hvsp.h>

void setup()
{
    Serial.begin(115200);
    HVSP::setup();

    pinMode(READY_LED, OUTPUT);
    digitalWrite(READY_LED, HIGH);

    pinMode(MODE_LED, OUTPUT);
    MODE_LED_OFF;
}

void loop()
{
    PROGRAMMER::loop();
}
#endif