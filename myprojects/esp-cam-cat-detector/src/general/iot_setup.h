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
#define SAVE_CH 1
#define LUM_CH 2

namespace IotProperties
{
    inline EmberIot ember(emberDbUrl, emberDeviceId, emberUsername, emberPassword, emberWebApiKey);

    static float currentLuminosity = UINT32_MAX;
    const char saveFile[] = "/prop_store";

    struct Props
    {
        uint8_t inferenceOn = 0;
        uint8_t saveOn = 0;
    };

    struct PropsFile
    {
        uint32_t magic;
        Props props;
    };

    inline Props props{};
    constexpr uint32_t PROPS_MAGIC = 0xDEADBEEF;

    inline void save()
    {
        File file = LittleFS.open(saveFile, "w");
        if (!file) return;

        PropsFile pf;
        pf.magic = PROPS_MAGIC;
        pf.props = props;

        file.write(reinterpret_cast<const uint8_t*>(&pf), sizeof(pf));
        file.close();
    }

    inline void recover()
    {
        if (!LittleFS.exists(saveFile)) return;

        File file = LittleFS.open(saveFile, "r");
        if (!file) return;

        if (file.size() != sizeof(PropsFile))
        {
            file.close();
            return;
        }

        PropsFile pf;
        file.read(reinterpret_cast<uint8_t*>(&pf), sizeof(pf));
        file.close();

        if (pf.magic != PROPS_MAGIC)
        {
            props = {};
            return;
        }

        props = pf.props;
    }

    inline void setup()
    {
        recover();
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
        props.inferenceOn = !props.inferenceOn;
        ember.channelWrite(POWER_CH, props.inferenceOn ? EMBER_BUTTON_ON : EMBER_BUTTON_OFF);
        save();
    }

    inline void toggleSaving()
    {
        props.saveOn = !props.saveOn;
        ember.channelWrite(SAVE_CH, props.saveOn);
        save();
    }

    inline bool isInferenceOn()
    {
        return props.inferenceOn;
    }

    inline bool isSavingOn()
    {
        return props.saveOn;
    }
}

EMBER_CHANNEL_CB(POWER_CH)
{
    IotProperties::props.inferenceOn = prop.toButtonIsOn();
}

#endif //IOT_SETUP_H
