//
// Created by Davi on 13/06/2023.
//

#ifndef ARDUINOPLATFORMIO_AIRCONDITIONER_H
#define ARDUINOPLATFORMIO_AIRCONDITIONER_H

#include <Arduino.h>

class AirConditioner {
public:
    AirConditioner(uint8_t compressorTurnOnPin, uint8_t fanTurnOnPin);

    void start();
    void stop();
    void doChecks(double roomTemperature, double coilTemperature);
    bool isSwitchTimeoutOver(float multiplier = 1);
    void setFirstTimeout(bool hasTimeout);

    uint8_t compressorTurnOnPin;
    uint8_t fanTurnOnPin;
    uint8_t userTemperature;

    double temperatureUpperLimit;
    double temperatureLowerLimit;

    uint8_t switchTimeoutMinutes;
    bool isCompressorRunning;

protected:
    double lastTemp;
    bool isOn;
    bool firstTimeout;
    unsigned long switchTimeoutMillis;
    void toggleCompressor(bool on);
    void toggleFan(bool on);
};


#endif //ARDUINOPLATFORMIO_AIRCONDITIONER_H
