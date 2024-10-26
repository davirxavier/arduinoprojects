//
// Created by xav on 10/25/24.
//

#ifndef MOTOR_REMOTE_SENSOR_BUZZER_H
#define MOTOR_REMOTE_SENSOR_BUZZER_H

#include "Arduino.h"

class DxBuzzer {
public:
    DxBuzzer(uint8_t pin);

    void beep() { beep(700); }
    void beep(unsigned long beepDurationMs) { beep(beepDurationMs, 1, 0); }
    void beep(unsigned long beepDurationMs, unsigned int beepQuantity, unsigned long delayBetweenBeepsMs);

    void update();
    void stop();
private:
    uint8_t pin;
    bool isBeeping;
    unsigned long currentBeepDuration;
    unsigned long currentBeepDelay;
    unsigned long beepTimer;
    unsigned int currentBeepQuantity;
};


#endif //MOTOR_REMOTE_SENSOR_BUZZER_H
