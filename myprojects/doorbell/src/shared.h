//
// Created by xav on 3/17/25.
//

#ifndef SHARED_H
#define SHARED_H

#include <espnow.h>
#include <shared.h>

namespace Messages {
    enum Messages
    {
        RING,
        PING,
        NONE,
    };
}

namespace Commands
{
    enum Commands
    {
        TURN_ON,
        TURN_OFF,
        RING,
        NONE,
    };

    inline uint8_t commandCount = 4;

    inline char commandTextById[4][32] = {
        "ligar",
        "desligar",
        "tocar",
        "",
    };

    inline Commands parseCommand(String str)
    {
        str.toLowerCase();

        bool passedSpace = false;
        char buf[str.length()+1];
        buf[0] = 0;
        unsigned int currentChar = 0;
        for (unsigned int i = 0; i < str.length(); i++)
        {
            const char c = str[i];

            if (passedSpace)
            {
                buf[currentChar] = c;
                currentChar++;
            }
            else if (c == ' ')
            {
                passedSpace = true;
            }
        }

        if (!passedSpace || currentChar == 0)
        {
            return NONE;
        }

        buf[currentChar] = 0;
        for (uint8_t i = 0; i < commandCount; i++)
        {
            if (strcmp(buf, commandTextById[i]) == 0)
            {
                return (Commands) i;
            }
        }

        return NONE;
    }
}

inline bool stringToMac(const char *str, uint8_t *macAddress)
{
    if (strlen(str) != 17)
    {
        return false;
    }

    for (int i = 0; i < 6; ++i) {
        sscanf(str + i * 3, "%2hhx", &macAddress[i]);
    }
    return true;
}

#endif //SHARED_H
