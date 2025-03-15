//
// Created by xav on 3/9/25.
//

#ifndef DEFINES_H
#define DEFINES_H

namespace BooleanEnum
{
    enum Boolean
    {
        FALSE = 0,
        TRUE = 1,
    };
}

namespace Mode
{
    enum Mode
    {
        QUICK = 0,
        LOWM = 1,
        MEDIUM = 2,
        ROUGH = 3,
        SPIN = 4,
    };

    inline Mode cycleMode(Mode current)
    {
        return current == ROUGH ? QUICK : static_cast<Mode>(current+1);
    }
}

namespace Stage
{
    enum Stage
    {
        WASH_FILLING_UP = 0,
        WASH_SHAKING = 1,
        WASH_DRAINING = 2,
        RINSE_FILLING_UP = 3,
        RINSE_SOAKING = 4,
        RINSE_SHAKING = 5,
        RINSE_DRAINING = 6,
        RINSE_SPINNING = 7,
        DRY_SPINNING = 8,
        OFF = 9,
    };
}

namespace Actions
{
    enum Actions
    {
        NONE = 0,
        SKIP_STAGE = 1,
    };
}

namespace States
{
    enum States
    {
        POWER,
        MODE,
        STAGE,
    };
}

namespace Module
{
    enum Module
    {
        WATER_VALVE,
        MOTOR,
        DRAIN_PUMP,
    };
}

struct Data
{
    Mode::Mode currentMode;
    Stage::Stage currentStage;
    uint8_t currentRinse;
    unsigned int elapsedMinutes;

    Data()
    {
        currentMode = Mode::QUICK;
        currentStage = Stage::OFF;
        currentRinse = 0;
        elapsedMinutes = 0;
    }
};

namespace Events
{
    char EVENT_IDS[2][32] = {"cycle_ended", "power_failure"};

    enum Events
    {
        CYCLE_ENDED = 0,
        POWER_FAILURE = 1,
    };
}

#endif //DEFINES_H
