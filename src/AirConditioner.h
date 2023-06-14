//
// Created by Davi on 13/06/2023.
//

#ifndef ARDUINOPLATFORMIO_AIRCONDITIONER_H
#define ARDUINOPLATFORMIO_AIRCONDITIONER_H

class AirConditioner {
public:
    AirConditioner(uint8_t compressorTurnOnPin, uint8_t fanTurnOnPin);

    void start();
    void stop();
    void doChecks(double roomTemperature, double coilTemperature);

    uint8_t compressorTurnOnPin;
    uint8_t fanTurnOnPin;
    uint8_t userTemperature;
    uint8_t switchTimeoutMinutes;
    double temperatureUpperLimit;
    double temperatureLowerLimit;

protected:
    unsigned long switchTimeoutMillis;
    void toggleCompressor(bool on) const;
    void toggleFan(bool on) const;
};


#endif //ARDUINOPLATFORMIO_AIRCONDITIONER_H
