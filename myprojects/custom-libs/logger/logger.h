//
// Created by xav on 7/25/24.
//

#ifndef DX_LOGGER_H
#define DX_LOGGER_H

#include <utility>

#include "Arduino.h"
#include "time.h"
#include "TimeLib.h"

#define SERIAL_LOG_LN(str) dxlogger::log(str, true)
#define SERIAL_LOG(str) dxlogger::log(str, false)

namespace dxlogger {
    bool enableSerial = false;
    bool enableLogFile = false;
    String logsFile;
    unsigned long maxLogSize;
    void (*_fileMaxSizeCallback)(File logsFile) = nullptr;
    bool addDate = true;

    void setup(const char* timezone, unsigned long updateInterval, bool enableSerialLog) {
        configTime(timezone, "pool.ntp.org");
        enableSerial = enableSerialLog;
    }

    void setup(const char* timezone,
               unsigned long updateInterval,
               bool enableSerialLog,
               String logsFilePath,
               unsigned long maxLogFileSize,
               void (*fileMaxSizeCallback)(File logsFile)) {
        setup(timezone, updateInterval, enableSerialLog);
        enableLogFile = true;
        logsFile = logsFilePath;
        maxLogSize = maxLogFileSize;
        _fileMaxSizeCallback = fileMaxSizeCallback;
    }

    unsigned long getTime() {
        time_t now;
        tm timeinfo{};
        if (!getLocalTime(&timeinfo)) {
            return(0);
        }
        time(&now);
        return now;
    }

    String getDateTime() {
        time_t e = getTime();

        char buff[32];
        sprintf(buff, "%02d/%02d/%02d %02d:%02d:%02d", day(e), month(e), year(e), hour(e), minute(e), second(e));

        return String(buff);
    }

    void update() {}

    void log(String log, bool nl) {
        if (addDate) {
            log = getDateTime() + " - " + log;
        }

        if (nl) {
            addDate = true;
        } else {
            addDate = false;
        }

        if (enableSerial && nl) {
            Serial.println(log);
        } else if (enableSerial) {
            Serial.print(log);
        }

        if (enableLogFile) {
            File file = LittleFS.open(logsFile, "a");

            if (file.size() > maxLogSize) {
                file.close();

                if (_fileMaxSizeCallback != nullptr) {
                    file = LittleFS.open(logsFile, "r");
                    _fileMaxSizeCallback(file);
                    file.close();
                }

                file = LittleFS.open(logsFile, "w");
            }

            if (nl) {
                file.println(log);
            } else {
                file.print(log);
            }
            file.close();
        }
    }
}

#endif //DX_LOGGER_H
