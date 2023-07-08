#include "Arduino.h"

unsigned long last;
String inString;
int val = 0;

int count(const String& str, char c) {
    int count = 0;
    for (char c2 : str) {
        if (c2 == c) {
            count++;
        }
    }
    return count;
}

void setup() {
    val = 0;
    last = 0;
    Serial.begin(9600);
    inString = "";
    analogWrite(3, 0);
}

void loop() {
    while (Serial.available() > 0) {
        int inChar = Serial.read();
        inString += (char)inChar;

        if (inChar == 10) {
            if (inString.startsWith("+")) {
                val = val + (1*count(inString, '+'));
            } else if (inString.startsWith("-")) {
                val = val - (1*count(inString, '-'));
            }

            val = val <= 0 ? 0 : (val >= 100 ? 100 : val);

            Serial.println(val);
            analogWrite(3, map(val, 0, 100, 0, 255));
            inString = "";
        }
    }
}