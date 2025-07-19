//
// Created by xav on 7/15/25.
//

#ifndef CAPACITANCE_H
#define CAPACITANCE_H

#include <Arduino.h>

inline void initCapacitance(uint8_t charge_pin, uint8_t sense_pin)
{
    pinMode(sense_pin, INPUT);
    pinMode(charge_pin,OUTPUT);

    digitalWrite(charge_pin,LOW);  // begin discharging
    delay(40);
}

inline double measureCapacitancePF(
    int num_measurements,
    byte charge_pin,
    byte sense_pin,
    double resistor_mohm,
    double v_ref
)
{
    const byte charge_time_us = 84; // instructions take ~54us +30us to charge
    const byte discharge_time_ms = 40;
    unsigned long charged_val = 0;

    for (int i = 0; i < num_measurements; i++)
    {
        digitalWrite(charge_pin, HIGH);
        delayMicroseconds(charge_time_us - 54);
        charged_val += analogReadMilliVolts(sense_pin);

        digitalWrite(charge_pin, LOW);
        delay(discharge_time_ms);
    }

    charged_val /= num_measurements;

    double capVoltage = charged_val / 1000.0;
    double capacitance_pf = -1.0 * ((charge_time_us) / resistor_mohm ) / log(1 - (capVoltage / v_ref));
    return capacitance_pf;
}

#endif //CAPACITANCE_H
