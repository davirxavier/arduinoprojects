#include "WifiConfig.h"

WifiConfig::WifiConfig() {
    cfgPath = "cfg.txt";
}

bool WifiConfig::hasConfig() {
    return LittleFS.exists(cfgPath);
}

void WifiConfig::setConfig(String ssid, String passphrase) {
    if (ssid.length() > 0 && passphrase.length() > 0) {
        WiFi.persistent(false);

        WiFi.mode(WIFI_OFF);
        WiFi.mode(WIFI_STA);

        WiFi.begin(ssid, passphrase);

        WifiConfigStrut cfg;
        cfg.ssid = ssid;
        cfg.pass = passphrase;

        saveConfig(cfg);
    } else {
        resetConfig();
    }
}

void WifiConfig::tryRestoreConfig() {
    if (LittleFS.exists(cfgPath)) {
        File file = LittleFS.open(cfgPath, "r");
        String configStr = file.readString();
        file.close();

        String ssid = getValue(configStr, ';', 0);
        String pass = getValue(configStr, ';', 1);
        if (ssid.length() > 0 && pass.length() > 0) {
            setConfig(ssid, pass);
        }
    }
}

void WifiConfig::saveConfig(WifiConfigStrut config) {
    File file = LittleFS.open(cfgPath, "w");
    file.println(config.ssid + ";" + config.pass + ";");
    file.close();
}

void WifiConfig::resetConfig() {
    if (LittleFS.exists(cfgPath)) {
        LittleFS.remove(cfgPath);
    }
}
