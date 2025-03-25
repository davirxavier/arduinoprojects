//
// Created by DX on 28/09/2023.
//

#ifndef TESTINGARDUINO_AIRCONDITIONERREMOTE_H
#define TESTINGARDUINO_AIRCONDITIONERREMOTE_H

#include "Arduino.h"
#include "IRremoteInt.h"

#define ACCEPTED_SIGNATURE 0x683
#define MIN_TEMP 18

typedef enum {
    FAN_AUTO = 0,
    FAN_HIGH = 1,
    FAN_MEDIUM = 2,
    FAN_LOW = 3
} FanMode;

typedef enum {
    AUTO = 0,
    AC_COOL = 2,
    AC_HUMID = 3,
    AC_FAN = 4
} AcMode;

struct AirConditionerDataRaw {
    uint16_t signature: 12;
    uint8_t unused2: 4;
    uint8_t fanToggle: 4;
    uint8_t swing: 4;
    uint8_t mode: 4;
    uint8_t temp: 4;
};

class AirConditionerRemote {
public:
    AirConditionerRemote();
    bool readCommand(uint64_t *data, uint16_t protocol, uint16_t bitNumber);
    void resetToggles();

    AcMode mode;
    FanMode fanMode;
    bool powerToggle;
    bool swingToggle;
    uint16_t temp;
};



#endif //TESTINGARDUINO_AIRCONDITIONERREMOTE_H
