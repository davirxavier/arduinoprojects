//
// Created by xav on 2/9/26.
//

#ifndef IOT_SETUP_H
#define IOT_SETUP_H

#define EMBER_ENABLE_LOGGING
#define EMBER_CHANNEL_COUNT 5
#include <EmberIot.h>
#include <general/secrets.h>

#include "iot_setup.h"

#define POWER_CH 0
#define LUM_CH 1

namespace IotProperties
{
    inline EmberIot ember(emberDbUrl, emberDeviceId, emberUsername, emberPassword, emberWebApiKey);
    static volatile bool inferenceOn = false;
    static float currentLuminosity = UINT32_MAX;

    inline void setup()
    {
        // ember.init();
    }

    inline void loop()
    {
        // ember.loop();
    }

    inline void setLuminosity(float lum)
    {
        currentLuminosity = lum;
        ember.channelWrite(LUM_CH, currentLuminosity);
    }

    inline void toggleInference()
    {
        inferenceOn = !inferenceOn;
        ember.channelWrite(POWER_CH, EMBER_BUTTON_OFF);
    }

    inline bool isInferenceOn()
    {
        return inferenceOn;
    }
}

EMBER_CHANNEL_CB(POWER_CH)
{
    IotProperties::inferenceOn = prop.toButtonIsOn();
}

#endif //IOT_SETUP_H
