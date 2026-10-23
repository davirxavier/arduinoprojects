#ifndef ARDUINO_AVR_ATTINY13
#include <Arduino.h>
#include <util.h>

#define LX_PIN 3
#define LY_PIN 5
#define RX_PIN 6
#define RY_PIN 9
#define SQUARE_PIN 7
#define X_PIN 8

#define LX_POT_PIN A1
#define RX_POT_PIN A2
#define RY_POT_PIN A3

#define SENSITIVE_L_PIN 11
#define R_ON_PIN 12
#define R_BUTTON_PIN 13

void setup()
{
    pinMode(SQUARE_PIN, OUTPUT);
    pinMode(X_PIN, OUTPUT);
    digitalWrite(SQUARE_PIN, LOW);
    digitalWrite(X_PIN, LOW);

    pinMode(SENSITIVE_L_PIN, INPUT);
    pinMode(R_ON_PIN, INPUT);
    pinMode(R_BUTTON_PIN, INPUT);

    writeStick(LX_PIN, 512);
    writeStick(LY_PIN, 512);
    writeStick(RX_PIN, 512);
    writeStick(RY_PIN, 512);
}

void loop()
{
    bool sensitiveL = digitalRead(SENSITIVE_L_PIN) == HIGH;
    bool ron = digitalRead(R_ON_PIN) == HIGH;
    bool rbtn = digitalRead(R_BUTTON_PIN) == HIGH;

    writeStick(LX_PIN, analogRead(LX_POT_PIN), sensitiveL);

    if (!ron)
    {
        digitalWrite(SQUARE_PIN, LOW);
        digitalWrite(X_PIN, LOW);
        writeStick(RX_PIN, 512);
        writeStick(RY_PIN, 512);
        return;
    }

    if (rbtn)
    {
        writeStick(RX_PIN, 512);
        writeStick(RY_PIN, 512);

        static constexpr int threshold = 550;
        digitalWrite(SQUARE_PIN, analogRead(RX_POT_PIN) > threshold ? HIGH : LOW);
        digitalWrite(X_PIN, analogRead(RX_POT_PIN) > threshold ? HIGH : LOW);
    }
    else
    {
        writeStick(RX_PIN, analogRead(RX_POT_PIN));
        writeStick(RY_PIN, analogRead(RY_POT_PIN));

        digitalWrite(SQUARE_PIN, LOW);
        digitalWrite(X_PIN, LOW);
    }
}
#endif