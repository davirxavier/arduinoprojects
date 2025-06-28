//
// Created by xav on 6/8/25.
//

#ifndef THERMISTOR_H
#define THERMISTOR_H

// 15ºC = 14014 ohms
// 25.5ºC = 9673 ohms
// 58.8ºC = 2708 ohms
// beta = 3910.85K
// https://www.thinksrs.com/downloads/programs/Therm%20Calc/NTCCalibrator/NTCcalculator.htm

#define THERMISTOR_CONSTANT_A 2.775110645e-3
#define THERMISTOR_CONSTANT_B (-0.6314513149e-4)
#define THERMISTOR_CONSTANT_C 14.91525406e-7

#include <Arduino.h>

inline double readResistance(unsigned int r1, unsigned int pin)
{
    int analogReading = analogRead(pin);  // Read thermistor value
    double voltage = analogReading * (3.3 / 1023.0); // Convert to voltage
    double R_NTC = r1 * ((3.3 / voltage) - 1); // Calculate thermistor resistance
    return R_NTC;
}

inline double readTemp(unsigned int r1, unsigned int pin) {
    double R_NTC = readResistance(r1, pin);
    double R_LOG = log(R_NTC);
    double Temp = 1.0 / (THERMISTOR_CONSTANT_A + (THERMISTOR_CONSTANT_B*R_LOG) + (THERMISTOR_CONSTANT_C * pow(R_LOG, 3)));
    Temp = Temp - 273.15;
    return Temp;
}

#endif //THERMISTOR_H
