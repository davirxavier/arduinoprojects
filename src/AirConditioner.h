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
    uint8_t tempCheckIntervalMinutes;
    double temperatureUpperLimit;
    double temperatureLowerLimit;

protected:
    bool isOn;
    bool isCompressorRunning;
    unsigned long lastTempCheckMillis;
    unsigned long switchTimeoutMillis;
    double lastTemp;
    void toggleCompressor(bool on);
    void toggleFan(bool on);
};


#endif //ARDUINOPLATFORMIO_AIRCONDITIONER_H
