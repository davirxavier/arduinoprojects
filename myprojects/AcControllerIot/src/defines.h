//
// Created by xav on 3/21/25.
//

#ifndef DEFINES_H
#define DEFINES_H

// 12.63ºC = 14616.09046 ohms
// 63.13ºC = 1872.06118 ohms
// 25.5ºC = 8460 ohms
// beta = 3910.85K
// resistance at 25ºC ~= 7113

#define THERMISTOR_CONSTANT_A 1.997792830e-3
#define THERMISTOR_CONSTANT_B 0.8590137377e-4
#define THERMISTOR_CONSTANT_C 7.683269300e-7

namespace State
{
    enum State
    {
        OFF,
        // FAN_ONLY,
        COOL_COMPRESSOR_OFF,
        COOL_COMPRESSOR_ON,
    };
}

struct Data
{
    State::State currentState;
    float userTemperature;
    float currentTemperature;

    Data()
    {
        currentState = State::OFF;
        userTemperature = 30;
        currentTemperature = 30;
    }
};

int espAnalogRead() {
    int val = 0;

    for(int i = 0; i < 20; i++) {
        val += analogRead(A0);
        delay(1);
    }

    val = val / 20;
    return val;
}

double readTemp(unsigned int r1, unsigned int r2) {
    int val = espAnalogRead();
    double V_NTC = ((double) val / 1023.0) * 0.637;
    double R_NTC = r1 * (V_NTC / (3.3 - V_NTC));
    R_NTC = -((R_NTC * r2) / (R_NTC - r2));
    double R_LOG = log(R_NTC);
    double Temp = 1.0 / (THERMISTOR_CONSTANT_A + (THERMISTOR_CONSTANT_B*R_LOG) + (THERMISTOR_CONSTANT_C * pow(R_LOG, 3)));
    Temp = Temp - 273.15;
    return Temp;
}

#endif //DEFINES_H
