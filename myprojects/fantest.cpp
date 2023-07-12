#include "Arduino.h"



unsigned long avg[AVG_SECONDS];
int currentAvg = 0;
unsigned long lastCheck = 0;

void setup() {

    pinMode(PIN_SENSE, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_SENSE), tachISR, FALLING);

    setPWM9(1);
    Serial.begin(9600);
}

void loop() {
    if (millis()-lastCheck > 1000) {
        avg[currentAvg] = calcRPM();
        currentAvg++;

        if (currentAvg == AVG_SECONDS) {
            unsigned long rpm = 0;
            for (unsigned long a : avg) {
                rpm += a;
            }
            rpm = rpm/AVG_SECONDS;

            Serial.print("RPM: ");
            Serial.println(rpm);
            currentAvg = 0;
        }
        lastCheck = millis();
    }
}