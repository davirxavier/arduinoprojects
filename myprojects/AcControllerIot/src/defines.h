//
// Created by xav on 3/21/25.
//

#ifndef DEFINES_H
#define DEFINES_H

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
    R_NTC = log(R_NTC);
    double Temp = 1 / (0.001129148 + (0.000234125 + (0.0000000876741 * R_NTC * R_NTC )) * R_NTC);
    Temp = Temp - 273.15;
    return Temp;
}

#endif //DEFINES_H
