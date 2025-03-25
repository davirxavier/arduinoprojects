//
// Created by DX on 28/09/2023.
//

#include "AirConditionerRemote.h"

AirConditionerRemote::AirConditionerRemote() {
    mode = AC_COOL;
    fanMode = FAN_HIGH;
    powerToggle = false;
    swingToggle = false;
    temp = 30;
}

bool AirConditionerRemote::readCommand(uint64_t *data, uint16_t protocol, uint16_t bitNumber) {
    if (protocol != PULSE_DISTANCE && protocol != PULSE_WIDTH) {
        return false;
    }

    uint8_t dataLength = ((bitNumber - 1) / 32) + 1;
    if (dataLength < 2) {
        return false;
    }

    uint32_t secondVal = data[1];
    if (secondVal == 0x9000 || secondVal == 0x80 || secondVal == 0x1FFFF) {
        return false;
    }

    AirConditionerDataRaw raw{};
    memcpy(&raw, (const char*) &data[0], sizeof(AirConditionerDataRaw));

    if (raw.signature != ACCEPTED_SIGNATURE) {
        return false;
    }

    mode = static_cast<AcMode>(raw.mode >= 8 ? raw.mode-8 : raw.mode);
    fanMode = static_cast<FanMode>(raw.fanToggle >= 4 ? raw.fanToggle-4 : raw.fanToggle);
    powerToggle = raw.fanToggle >= 4 && raw.fanToggle <= 7;
    swingToggle = raw.swing != 0;
    temp = raw.temp + MIN_TEMP;

    if (fanMode > 3) {
        fanMode = FAN_LOW;
    }

    return true;
}

void AirConditionerRemote::resetToggles() {
    powerToggle = false;
    swingToggle = false;
}