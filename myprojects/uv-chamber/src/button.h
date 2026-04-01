//
// Created by xav on 3/31/26.
//

#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>

enum ButtonEvent {
    EVENT_PRESS,        // Initial click (after debounce)
    EVENT_LONG_PRESS,   // Fired once after holding for longPressDurationMs
    EVENT_REPEAT        // Fired periodically if repeatEvents is true
};

struct ButtonProperties
{
    unsigned int lowThreshold;
    unsigned int highThreshold;
};

class MultiButton
{
public:
    typedef void (*ButtonCallback)(uint8_t, ButtonEvent);

    explicit MultiButton(uint8_t pin, bool repeatEvents, ButtonProperties *buttons, uint8_t buttonsLen)
    {
        btnPin = pin;
        debounceMs = 50;
        longPressDurationMs = 800;
        repeatIntervalMs = 200;

        lastButton = 0;
        lastStableTime = 0;
        lastRepeatTime = 0;

        callback = nullptr;
        pressFired = false;
        longPressFired = false;
        this->repeatEvents = repeatEvents;
        this->buttons = buttons;
        this->buttonsLen = buttonsLen;
    }

    void loop()
    {
        uint16_t value = analogRead(btnPin);
        uint8_t currentButton = 0;
        for (uint8_t i = 0; i < buttonsLen; i++)
        {
            ButtonProperties &prop = buttons[i];
            if (value >= prop.lowThreshold && value < prop.highThreshold)
            {
                currentButton = i+1;
                break;
            }
        }

        unsigned long now = millis();
        if (currentButton != lastButton)
        {
            lastStableTime = now;
            lastButton = currentButton;
            pressFired = false;
            longPressFired = false;
            return;
        }

        if (currentButton == 0)
        {
            return;
        }

        if ((now - lastStableTime) > debounceMs && callback != nullptr)
        {
            if (!pressFired)
            {
                callback(currentButton, EVENT_PRESS);
                pressFired = true;
                lastRepeatTime = now;
            }
            else if (!longPressFired && (now - lastStableTime) >= longPressDurationMs)
            {
                callback(currentButton, EVENT_LONG_PRESS);
                longPressFired = true;
                lastRepeatTime = now;
            }
            else if (repeatEvents && (now - lastRepeatTime) >= repeatIntervalMs && longPressFired)
            {
                callback(currentButton, EVENT_REPEAT);
                lastRepeatTime = now;
            }
        }
    }

    unsigned long repeatIntervalMs;
    unsigned long longPressDurationMs;
    unsigned long debounceMs;
    ButtonCallback callback;

private:
    unsigned long lastStableTime;
    unsigned long lastRepeatTime;
    uint8_t lastButton;
    uint8_t btnPin;
    bool pressFired;
    bool longPressFired;
    bool repeatEvents;
    ButtonProperties *buttons;
    uint8_t buttonsLen;
};

#endif //BUTTON_H