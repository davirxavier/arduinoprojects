//
// Created by xav on 03/14/25.
//

#ifndef MOTOR_LED_UTIL_H
#define MOTOR_LED_UTIL_H

#include "Arduino.h"

class DxLed {
public:
    DxLed(uint8_t pin, bool activeHigh);

    void on();
    void blink() { blink(700); }
    void blink(unsigned long blinkDurationMs) { blink(blinkDurationMs, 1, 0, false); }
    void blink(unsigned long blinkDurationMs, unsigned int blinkQuantity, unsigned long delayBetweenBlinksMs, bool onAfterEnd);

    void update();
    void stop();
private:
    uint8_t pin;
    bool activeHigh;
    bool onAfterEnd;
    bool isBlinking;
    unsigned long currentBlinkDuration;
    unsigned long currentBlinkDelay;
    unsigned long blinkTimer;
    unsigned int currentBlinkQuantity;
};


#endif //MOTOR_LED_UTIL_H
