#include <Arduino.h>
#include "CheapStepper.h"

CheapStepper stepper (8,9,10,11);
bool moveClockwise = true;

void setup() {
    stepper.setRpm(8);
    stepper.newMoveDegrees(moveClockwise, 180);
}

void loop() {
    stepper.run();

    if (stepper.getStepsLeft() == 0) {
        moveClockwise = !moveClockwise;
        stepper.newMoveDegrees(moveClockwise, 180);
    }
}