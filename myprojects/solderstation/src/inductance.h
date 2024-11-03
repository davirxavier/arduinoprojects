//
// Created by xav on 11/2/24.
//

#ifndef SOLDERSTATION_INDUCTANCE_H
#define SOLDERSTATION_INDUCTANCE_H

#include "Arduino.h"

namespace Inductance {

    uint8_t _signalPin;
    uint8_t _inputPin;
    double pulse;
    double _capacitance = 10.E-6;

    /**
     * @param signalPin Pin to send signal
     * @param inputPin Pin to receive signal output
     * @param capacitance Capacitance in series with inductor (in nF)
     */
    void setup(uint8_t signalPin, uint8_t inputPin, double capacitance) {
        _signalPin = signalPin;
        _inputPin = inputPin;
        _capacitance = capacitance;
    }

    void measureInductance(double &inductance, double &frequency) {
        digitalWrite(13, HIGH);
        delay(5);
        digitalWrite(13,LOW);
        delayMicroseconds(100);
        pulse = pulseIn(11,HIGH,5000);

        if(pulse > 0.1){
            frequency = 1.E6/(2*pulse);
            inductance = 1./(_capacitance*frequency*frequency*4.*3.14159*3.14159);
            inductance *= 1E6;
        } else {
            inductance = -1;
            frequency = -1;
        }
    }
}

#endif //SOLDERSTATION_INDUCTANCE_H
