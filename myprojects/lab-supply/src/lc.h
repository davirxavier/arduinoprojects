#ifndef LC_H
#define LC_H

#include <Arduino.h>

namespace Inductance {

    /**
     * @param signalPin Pin to send signal
     * @param inputPin Pin to receive signal output
     */
    inline void setup(uint8_t signalPin, uint8_t inputPin) {
        pinMode(signalPin, OUTPUT);
        pinMode(inputPin, INPUT);
    }

    inline void measure(uint8_t signalPin, uint8_t inputPin, double capacitanceFarads, double &inductance, double &frequency) {
        digitalWrite(signalPin, HIGH);
        delay(5);
        digitalWrite(signalPin,LOW);
        delayMicroseconds(100);

        double pulse = pulseIn(inputPin,HIGH,5000);

        if(pulse > 0.1){
            frequency = 1.E6/(2*pulse);
            inductance = 1./(capacitanceFarads*frequency*frequency*4.*3.14159*3.14159);
            inductance *= 1E9;
        } else {
            inductance = -1;
            frequency = -1;
        }
    }
}

namespace Capacitance
{
    constexpr unsigned long timeoutMillis = 60000;
    constexpr unsigned long timeoutMicros = timeoutMillis*1000;

    enum Error
    {
        DISCHARGE_TIMEOUT = -1,
        CHARGE_TIMEOUT = -2,
        OK = 1,
    };

    inline long readVcc() {
        // Select internal 1.1V reference and Vcc as input (MUX3, MUX2, MUX1 set)
        ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);

        // Wait for Vref to settle
        delay(1);

        // Start ADC conversion
        ADCSRA |= _BV(ADSC);

        // Wait for conversion to finish
        while (bit_is_set(ADCSRA, ADSC)) {}

        // Get the ADC result (16-bit)
        uint16_t result = ADC;

        // Calculate VCC in millivolts (1.1 * 1023 * 1000)
        long vcc = 1125300L / result;

        // Return VCC in millivolts (mV)
        return vcc;
    }

    inline int calculateAdcThreshold(long vccMillivolts)
    {
        auto vcc = (double) vccMillivolts;
        double targetVoltage = vcc * 0.632;
        double adcValue = (targetVoltage / vcc) * 1023.0;
        return (int)adcValue;
    }

    inline int discharge(uint8_t discharge_pin, uint8_t sense_pin)
    {
#ifdef SIMULATION
        return 1;
#else
        pinMode(discharge_pin, OUTPUT);
        digitalWrite(discharge_pin,LOW);  // begin discharging
        unsigned long startTime = millis();
        while (analogRead(sense_pin) > 0)
        {
            if (millis() - startTime > timeoutMillis)
            {
                return DISCHARGE_TIMEOUT;
            }
        }

        pinMode(discharge_pin, INPUT);
        return 1;
#endif
    }

    /**
     * @param fast_charge_pin Pin for fast charging (suggested through a 1Mohm resistor)
     * @param slow_charge_pin Pin for slow charging (suggested through a 1kohm resistor)
     * @param discharge_pin Pin for discharging the capacitor (suggested through a 220ohm resistor)
     * @param sense_pin Pin for sensing voltage
     */
    inline int setup(uint8_t fast_charge_pin, uint8_t slow_charge_pin, uint8_t discharge_pin, uint8_t sense_pin)
    {
        pinMode(fast_charge_pin, INPUT);
        pinMode(slow_charge_pin, INPUT);
        pinMode(sense_pin, INPUT);

        return discharge(discharge_pin, sense_pin);
    }

    /**
     * Measure capacitance, will return an error (negative) or the measured capacitance (positive).
     * Suggested range for capacitance measuring:
     * - <=10uf through a 1Mohm resistor: slow charge pin.
     * - >10uf through a 1kohm reisistor: fast charge pin.
     *
     * @return negative for error or positive for measured capacitance in nF.
     */
    inline double measure(uint8_t fastChargePin,
        uint8_t slowChargePin,
        uint8_t dischargePin,
        uint8_t sensePin,
        double seriesResistance,
        bool fastCharge)
    {
        long vcc = readVcc();
        int adcThreshold = calculateAdcThreshold(vcc);

        discharge(dischargePin, sensePin);

        uint8_t chargePin = fastCharge ? fastChargePin : slowChargePin;
        uint8_t unusedPin = fastCharge ? slowChargePin : fastChargePin;

        pinMode(unusedPin, INPUT);
        pinMode(chargePin, OUTPUT);
        digitalWrite(chargePin, HIGH);

        unsigned long start = micros();
        while (analogRead(sensePin) < adcThreshold) {
            if (micros() - start > timeoutMicros)
            {
                pinMode(chargePin, INPUT);
                return CHARGE_TIMEOUT;
            }
        }

        unsigned long elapsedMicros = micros() - start;
        double elapsedSeconds = (double) elapsedMicros / 1000000.0;
        double capacitanceFarads = elapsedSeconds / seriesResistance;

        digitalWrite(chargePin, LOW);
        pinMode(chargePin, INPUT);
        discharge(dischargePin, sensePin);
        return capacitanceFarads * 1e9; // Convert to nF
    }
}

#endif //LC_H
