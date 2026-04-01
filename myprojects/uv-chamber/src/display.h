//
// Created by xav on 3/31/26.
//

#ifndef DISPLAY_H
#define DISPLAY_H

namespace SevenDisplay
{
    bool DOFF = LOW;
    bool DON = HIGH;
    bool SOFF = HIGH;
    bool SON = LOW;

    enum Segments
    {
        NONE = -1,
        TOP = 0,
        TOP_LEFT,
        TOP_RIGHT,
        BOTTOM,
        BOTTOM_LEFT,
        BOTTOM_RIGHT,
        MIDDLE,
        INVALID, // DO NOT USE
    };

    // Same order as the enum above
    constexpr int8_t segmentPins[INVALID] = {4, 5, 6, 7, 8, 9, 10};
    constexpr int8_t digitPins[4] = {11, 12, 13, A0};

    // pins to change by number
    constexpr Segments numberPinMap[10][7] = {
        {TOP, TOP_LEFT, TOP_RIGHT, BOTTOM, BOTTOM_LEFT, BOTTOM_RIGHT, NONE},
        {TOP_RIGHT, BOTTOM_RIGHT, NONE, NONE, NONE, NONE, NONE},
        {TOP, TOP_RIGHT, MIDDLE, BOTTOM_LEFT, BOTTOM, NONE, NONE},
        {TOP, TOP_RIGHT, MIDDLE, BOTTOM_RIGHT, BOTTOM, NONE, NONE},
        {TOP_LEFT, TOP_RIGHT, MIDDLE, BOTTOM_RIGHT, NONE, NONE, NONE},
        {TOP, TOP_LEFT, MIDDLE, BOTTOM_RIGHT, BOTTOM, NONE, NONE},
        {TOP, TOP_LEFT, MIDDLE, BOTTOM_LEFT, BOTTOM_RIGHT, BOTTOM, NONE},
        {TOP, TOP_LEFT, TOP_RIGHT, BOTTOM_RIGHT, NONE, NONE, NONE},
        {TOP, TOP_LEFT, TOP_RIGHT, MIDDLE, BOTTOM_LEFT, BOTTOM_RIGHT, BOTTOM},
        {TOP, TOP_LEFT, TOP_RIGHT, MIDDLE, BOTTOM_RIGHT, BOTTOM, NONE},
    };

    volatile int8_t currentDisplayNumbers[4] = {-1, -1, -1, -1};
    volatile bool flashing = false;

    inline void updateDisplay(int val, bool leadingZeroes = false)
    {
        val = abs(val);
        currentDisplayNumbers[0] = val > 999 ? (val / 1000) : (leadingZeroes ? 0 : -1);
        currentDisplayNumbers[1] = val > 99 ? ((val % 1000) / 100) : (leadingZeroes ? 0 : -1);
        currentDisplayNumbers[2] = val > 9 ? ((val % 100) / 10) : (leadingZeroes ? 0 : -1);
        currentDisplayNumbers[3] = val % 10;
    }

    inline void initDisplay()
    {
        for (int8_t pin : segmentPins)
        {
            pinMode(pin, OUTPUT);
            digitalWrite(pin, SOFF);
        }

        for (int8_t pin : digitPins)
        {
            pinMode(pin, OUTPUT);
            digitalWrite(pin, DOFF);
        }
    }

    inline void displayInterrupt()
    {
        static bool flashFlag = false;
        static uint8_t currentDigit = 0;
        static uint8_t tickCounter = 0;
        static uint16_t flashTickCounter = 0;

        flashTickCounter++;
        if (flashing && ((flashFlag && flashTickCounter >= 350) || (!flashFlag && flashTickCounter >= 500)))
        {
            flashTickCounter = 0;
            flashFlag = !flashFlag;
        }

        if (flashing && !flashFlag)
        {
            for (const int8_t d : digitPins)
            {
                digitalWrite(d, DOFF);
            }
            for (const int8_t seg : segmentPins)
            {
                digitalWrite(seg, SOFF);
            }
            return;
        }

        tickCounter++; // one update very 3 ticks = 3ms per digit
        if (tickCounter < 3)
        {
            return;
        }
        tickCounter = 0;

        currentDigit++;
        if (currentDigit > 3)
        {
            currentDigit = 0;
        }

        for (const int8_t d : digitPins)
        {
            digitalWrite(d, DOFF);
        }

        int8_t currentNum = currentDisplayNumbers[currentDigit];
        if (currentNum < 0 || currentNum > 9)
        {
            digitalWrite(digitPins[currentDigit], DOFF);
            return;
        }

        for (const int8_t seg : segmentPins)
        {
            digitalWrite(seg, SOFF);
        }

        for (const int8_t seg : numberPinMap[currentNum])
        {
            if (seg == NONE)
            {
                continue;
            }
            digitalWrite(segmentPins[seg], SON);
        }

        digitalWrite(digitPins[currentDigit], DON);
    }
}

#endif //DISPLAY_H
