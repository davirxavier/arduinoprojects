/******************************************************************************/
// AVR High Voltage Serial Programmer 1.01 by Mr.Blinky Mar-Apr.2020 CC0 Licence
/******************************************************************************/

// Modified by DaviXavier to fit better into my project, including adapting to a Nano and replacing direct AVR
// port register manipulation with standard Arduino GPIO functions.

#pragma once

#include <avr/wdt.h>

#define HVSP_SDO 13
#define HVSP_SII 12
#define HVSP_SDI 11
#define HVSP_SCI 4

#define VCC_PIN A0
#define VCC_PIN_E LOW
#define VCC_PIN_D HIGH

#define VPP_PIN 3
#define VPP_PIN_E HIGH
#define VPP_PIN_D LOW

namespace HVSP
{
    uint16_t currAddr = 0;
    uint8_t flashPage = 0;
    bool progMode = false;

    inline void vccDown()
    {
        digitalWrite(VCC_PIN, VCC_PIN_D);
    }

    inline void vccUp()
    {
        digitalWrite(VCC_PIN, VCC_PIN_E);
    }

    inline void HVSP_enable()
    {
        if (progMode) return;

        pinMode(HVSP_SCI, OUTPUT);
        digitalWrite(HVSP_SCI, LOW);

        pinMode(HVSP_SDO, OUTPUT);
        digitalWrite(HVSP_SDO, LOW);

        pinMode(HVSP_SII, OUTPUT);
        digitalWrite(HVSP_SII, LOW);

        pinMode(HVSP_SDI, OUTPUT);
        digitalWrite(HVSP_SDI, LOW);

        // Enter high-voltage serial programming mode.
        vccUp();
        delayMicroseconds(20);

        digitalWrite(VPP_PIN, VPP_PIN_E);
        delayMicroseconds(10);

        // Release SDO and enable its pull-up.
        pinMode(HVSP_SDO, INPUT_PULLUP);

        delayMicroseconds(300);
        progMode = true;
    }

    inline void HVSP_disable()
    {
        if (progMode)
        {
            unsigned long timer = millis();
            while (digitalRead(HVSP_SDO) == LOW)
            {
                // Wait for any pending command to finish.
                if (millis() - timer > 2000)
                {
                    break;
                }
            }
        }

        digitalWrite(VPP_PIN, VPP_PIN_D);

        pinMode(HVSP_SDI, INPUT);
        pinMode(HVSP_SII, INPUT);
        pinMode(HVSP_SCI, INPUT);

        // Disable the SDO pull-up.
        digitalWrite(HVSP_SDO, LOW);
        pinMode(HVSP_SDO, INPUT);

        vccDown();
        progMode = false;
    }

    // -----------------------------------------------------------------------------
    // Low-level hardware functions
    // -----------------------------------------------------------------------------

    void clockPulse()
    {
        delayMicroseconds(8);

        digitalWrite(HVSP_SCI, HIGH);
        delayMicroseconds(2);
        digitalWrite(HVSP_SCI, LOW);
    }

    uint8_t transfer(uint8_t data, uint8_t cmd)
    {
        uint8_t result = 0;

        unsigned long timer = millis();
        while (digitalRead(HVSP_SDO) == LOW)
        {
            // Wait for target ready.
            if (millis() - timer > 500)
            {
                HVSP_disable();
                delay(10);
                wdt_enable(WDTO_15MS);
                while (true) {}
            }
        }

        // Start bits
        digitalWrite(HVSP_SDI, LOW);
        digitalWrite(HVSP_SII, LOW);
        clockPulse();

        // Eight data bits, MSB first.
        for (uint8_t mask = 128; mask != 0; mask >>= 1)
        {
            digitalWrite(HVSP_SDI, (data & mask) ? HIGH : LOW);
            digitalWrite(HVSP_SII, (cmd & mask) ? HIGH : LOW);

            if (digitalRead(HVSP_SDO) == HIGH)
            {
                result |= mask;
            }

            clockPulse();
        }

        // Two stop clocks
        digitalWrite(HVSP_SDI, LOW);
        digitalWrite(HVSP_SII, LOW);

        clockPulse();
        clockPulse();

        return result;
    }

    // -----------------------------------------------------------------------------
    // HVSP device commands
    // -----------------------------------------------------------------------------

    uint8_t readSignatureByte(uint8_t idx)
    {
        transfer(0x08, 0x4C);
        transfer(idx, 0x0C);
        transfer(0x00, 0x68);
        return transfer(0x00, 0x6C);
    }

    void chipErase()
    {
        transfer(0x80, 0x4C);
        transfer(0x00, 0x64);
        transfer(0x00, 0x6C);
    }

    void readFlashMode()
    {
        transfer(0x02, 0x4C);
    }

    uint8_t readFlashLowByte()
    {
        transfer(currAddr, 0x0C);
        transfer(currAddr >> 8, 0x1C);
        transfer(0x00, 0x68);
        return transfer(0x00, 0x6C);
    }

    uint8_t readFlashHighByte()
    {
        transfer(0x00, 0x6C);
        transfer(0x00, 0x78);

        uint8_t result = transfer(0x00, 0x7C);

        transfer(0x00, 0x7C);

        return result;
    }

    void writeFlashMode()
    {
        transfer(0x10, 0x4C);
    }

    void loadFlashPageLowByte(uint8_t b)
    {
        flashPage = currAddr >> 8;

        transfer(currAddr & 0xFF, 0x0C);
        transfer(b, 0x2C);
    }

    void loadFlashPageHighByte(uint8_t b)
    {
        transfer(b, 0x3C);
        transfer(0x00, 0x7D);
        transfer(0x00, 0x7C);
    }

    void writeFlashPage()
    {
        transfer(flashPage, 0x1C);
        transfer(0x00, 0x64);
        transfer(0x00, 0x6C);
    }

    void readEepromMode()
    {
        transfer(0x03, 0x4C);
    }

    uint8_t readEepromByte()
    {
        return readFlashLowByte();
    }

    void writeEepromMode()
    {
        transfer(0x11, 0x4C);
    }

    void loadEepromPage(uint8_t b)
    {
        transfer(currAddr, 0x0C);
        transfer(currAddr >> 8, 0x1C);
        transfer(b, 0x2C);
        transfer(0x00, 0x6D);
        transfer(0x00, 0x6C);
    }

    void writeEepromPage()
    {
        transfer(0x00, 0x64);
        transfer(0x00, 0x6C);
    }

    void writeEepromByte(uint8_t b)
    {
        transfer(currAddr, 0x0C);
        transfer(currAddr >> 8, 0x1C);
        transfer(b, 0x2C);
        transfer(0x00, 0x6D);
        transfer(0x00, 0x64);
        transfer(0x00, 0x6C);
    }

    uint8_t readLowFuse()
    {
        transfer(0x04, 0x4C);
        transfer(0x00, 0x68);
        return transfer(0x00, 0x6C);
    }

    void writeLowFuse(uint8_t fuse)
    {
        transfer(0x40, 0x4C);
        transfer(fuse, 0x2C);
        transfer(0x00, 0x64);
        transfer(0x00, 0x6C);
    }

    uint8_t readHighFuse()
    {
        transfer(0x04, 0x4C);
        transfer(0x00, 0x7A);
        return transfer(0x00, 0x7E);
    }

    void writeHighFuse(uint8_t fuse)
    {
        transfer(0x40, 0x4C);
        transfer(fuse, 0x2C);
        transfer(0x00, 0x74);
        transfer(0x00, 0x7C);
    }

    uint8_t readExtFuse()
    {
        transfer(0x04, 0x4C);
        transfer(0x00, 0x6A);
        return transfer(0x00, 0x6E);
    }

    void writeExtFuse(uint8_t fuse)
    {
        transfer(0x40, 0x4C);
        transfer(fuse, 0x2C);
        transfer(0x00, 0x66);
        transfer(0x00, 0x6E);
    }

    uint8_t readLockBits()
    {
        transfer(0x04, 0x4C);
        transfer(0x00, 0x78);
        return transfer(0x00, 0x7C);
    }

    void writeLockBits(uint8_t fuse)
    {
        transfer(0x20, 0x4C);
        transfer(fuse, 0x2C);
        transfer(0x00, 0x64);
        transfer(0x00, 0x6C);
    }

    inline void setup()
    {
        // Configure target power control first.
        pinMode(VCC_PIN, OUTPUT);
        digitalWrite(VCC_PIN, VCC_PIN_D); // VCC off

        // Configure high-voltage control.
        pinMode(VPP_PIN, OUTPUT);
        digitalWrite(VPP_PIN, VPP_PIN_D); // VPP off

        // Ensure the programmer starts in a safe state.
        HVSP_disable();
    }
}
