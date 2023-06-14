#include <Arduino.h>

void reset() {
    analogWrite(11, 0);
    analogWrite(2, 0);
    analogWrite(4, 0);
    analogWrite(5, 0);
    analogWrite(6, 0);
    analogWrite(7, 0);
    analogWrite(9, 0);
    analogWrite(10, 0);
}

void setup() {
}

void loop() {
    reset();

    analogWrite(11, 255);
    analogWrite(2, 255);
    analogWrite(4, 255);
    analogWrite(6, 255);
    analogWrite(10, 255);
    delay(1000);

    reset();
    analogWrite(11, 255);
    analogWrite(2, 255);
    analogWrite(4, 255);
    analogWrite(6, 255);
    analogWrite(7, 255);
    analogWrite(10, 255);
    delay(1000);

    reset();
    analogWrite(11, 255);
    analogWrite(2, 255);
    analogWrite(4, 255);
    delay(1000);

    reset();
    analogWrite(4, 255);
    analogWrite(6, 255);
    delay(1000);

    reset();
    delay(1500);
}