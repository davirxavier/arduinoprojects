//
// Created by xav on 3/31/26.
//

#ifndef DISPLAY_H
#define DISPLAY_H

namespace SevenDisplay
{
    bool DOFF = HIGH;
    bool DON = LOW;
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
    constexpr int8_t segmentPins[INVALID] = {A2, 9, A1, 12, A0, 11, A4};
    constexpr int8_t digitPins[] = {4, 5, 7, 6, A5};

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

    bool extraDigitSegments[sizeof(segmentPins)/sizeof(int8_t)] = {};

    const int8_t iconPins[] = {8, 13};
    bool iconMap[sizeof(digitPins)][sizeof(iconPins)] = {};

    volatile int8_t currentDisplayNumbers[sizeof(digitPins)] = {-1, -1, -1, -1, 0};
    volatile bool flashing = false;
    volatile bool on = true;

    inline void setIcon(int digit, int iconIndex, bool on)
    {
        uint8_t oldSREG = SREG;
        noInterrupts();
        iconMap[digit][iconIndex] = on;
        SREG = oldSREG;
    }

    inline void updateExtraDigit(int segment, bool on)
    {
        if (segment < 0 || segment >= sizeof(extraDigitSegments))
        {
            return;
        }

        uint8_t oldSREG = SREG;
        noInterrupts();
        extraDigitSegments[segment] = on;
        SREG = oldSREG;
    }

    inline void updateDisplay(int val, bool leadingZeroes = false)
    {
        uint8_t oldSREG = SREG;
        noInterrupts();
        val = abs(val);
        currentDisplayNumbers[0] = val > 999 ? (val / 1000) : (leadingZeroes ? 0 : -1);
        currentDisplayNumbers[1] = val > 99 ? ((val % 1000) / 100) : (leadingZeroes ? 0 : -1);
        currentDisplayNumbers[2] = val > 9 ? ((val % 100) / 10) : (leadingZeroes ? 0 : -1);
        currentDisplayNumbers[3] = val % 10;
        SREG = oldSREG;
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

        for (int8_t pin : iconPins)
        {
            pinMode(pin, OUTPUT);
            digitalWrite(pin, SOFF);
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

        if ((flashing && !flashFlag) || !on)
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
        if (tickCounter < 2)
        {
            return;
        }
        tickCounter = 0;

        currentDigit++;
        if (currentDigit >= sizeof(digitPins))
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

        if (currentDigit == sizeof(digitPins)-1)
        {
            for (uint8_t i = 0; i < sizeof(extraDigitSegments); i++)
            {
                if (extraDigitSegments[i])
                {
                    digitalWrite(segmentPins[i], SON);
                }
            }
        }
        else
        {
            for (const int8_t seg : numberPinMap[currentNum])
            {
                if (seg == NONE)
                {
                    continue;
                }
                digitalWrite(segmentPins[seg], SON);
            }
        }

        for (signed char iconPin : iconPins)
        {
            digitalWrite(iconPin, SOFF);
        }

        bool *digitIcons = iconMap[currentDigit];
        for (size_t i = 0; i < sizeof(iconPins); i++)
        {
            bool on = digitIcons[i];
            digitalWrite(iconPins[i], on ? SON : SOFF);
        }

        digitalWrite(digitPins[currentDigit], DON);
    }
}

#endif //DISPLAY_H
