//
// Created by Davi on 16/09/2023.
//

#ifndef ARDUINOPROJECTS_WIFICONFIG_H
#define ARDUINOPROJECTS_WIFICONFIG_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "LittleFS.h"
#include "daviutil.h"

struct WifiConfigStrut {
    String ssid;
    String pass;
};

class WifiConfig {
public:
    WifiConfig();
    bool hasConfig();
    void setConfig(String ssid, String passphrase);
    void tryRestoreConfig();
    void resetConfig();
    void saveConfig(WifiConfigStrut config);
private:
    const char* cfgPath;
};


#endif //ARDUINOPROJECTS_WIFICONFIG_H
