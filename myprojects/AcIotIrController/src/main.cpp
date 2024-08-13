#include <Arduino.h>
#include "LittleFS.h"
#include "WiFiManager.h"
#include "ArduinoJson.h"
#include "logger.h"
#include "TZ.h"
#include "SinricPro.h"
#include "SinricProWindowAC.h"
#include "Capabilities/TemperatureSensor.h"
#include "weather-utils.h"

#define AP_NAME "ESP-AC-CONTROL-1"
#define AP_PASSWORD "admin13246"
#define APP_KEY_SIZE 37
#define APP_SECRET_SIZE 74
#define DEVICE_ID_SIZE 25
#define OPENWEATHER_API_KEY_SIZE 33
#define LAT_SIZE 21
#define LON_SIZE 21
#define DATA_FILE "data"
#define TEMP_UPDATE_INTERVAL 600000

#define ENABLE_SERIAL_LOGGING
#define SERIAL_LOG_LN(str) dxlogger::log(str, true)
#define SERIAL_LOG(str) dxlogger::log(str, false)

bool shouldSaveCredentials;

class SinricACWithTemp : public SinricProWindowAC, public TemperatureSensor<SinricACWithTemp> {
    friend class TemperatureSensor<SinricACWithTemp>;
public:
    SinricACWithTemp(const String &deviceId) : SinricProWindowAC(deviceId) {}
};

enum {
    MODE_COOL = 0,
    MODE_AUTO = 1,
};

struct AcData {
    char weatherApiKey[OPENWEATHER_API_KEY_SIZE];
    char lat[LAT_SIZE];
    char lon[LON_SIZE];
    char appKey[APP_KEY_SIZE];
    char appSecret[APP_SECRET_SIZE];
    char deviceId[DEVICE_ID_SIZE];
    float targetTemp;
    uint8_t level;
    uint8_t roomTemp;
    bool isAutoMode;
    bool isOn;
} __attribute__((packed));

dxweather::WeatherInfo weatherInfo{};
AcData data{};
WiFiManager manager;
unsigned long lastTempEvent;
SinricACWithTemp *ac;
HTTPClient http;
WiFiClientSecure client;

void saveData() {
    File file = LittleFS.open(DATA_FILE, "w");
    if (!file) {
        SERIAL_LOG_LN(F("Failed to create file"));
        return;
    }

    JsonDocument doc;
    doc["weatherApiKey"] = data.weatherApiKey;
    doc["lat"] = data.lat;
    doc["lon"] = data.lon;
    doc["appKey"] = data.appKey;
    doc["appSecret"] = data.appSecret;
    doc["deviceId"] = data.deviceId;
    doc["targetTemp"] = data.targetTemp;
    doc["level"] = data.level;
    doc["roomTemp"] = data.roomTemp;
    doc["mode"] = data.isAutoMode;
    doc["isOn"] = data.isOn;

    if (serializeJson(doc, file) == 0) {
        SERIAL_LOG_LN(F("Failed to write to file"));
    }

    file.close();
}

void recoverData() {
    if (!LittleFS.exists(DATA_FILE)) {
        SERIAL_LOG_LN("Credentials file does not exist.");
        return;
    }

    File file = LittleFS.open(DATA_FILE, "r");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    if (error)
        SERIAL_LOG_LN(F("Failed to read file, using default configuration"));

    strlcpy(data.weatherApiKey,
            doc["weatherApiKey"] | "",
            sizeof(data.weatherApiKey));
    strlcpy(data.lat,
            doc["lat"] | "",
            sizeof(data.lat));
    strlcpy(data.lon,
            doc["lon"] | "",
            sizeof(data.lon));
    strlcpy(data.appKey,
            doc["appKey"] | "",
            sizeof(data.appKey));
    strlcpy(data.appSecret,
            doc["appSecret"] | "",
            sizeof(data.appSecret));
    strlcpy(data.deviceId,
            doc["deviceId"] | "",
            sizeof(data.deviceId));

    data.targetTemp = doc["targetTemp"];
    data.level = doc["level"];
    data.roomTemp = doc["roomTemp"];
    data.isAutoMode = doc["mode"];
    data.isOn = doc["isOn"];

    file.close();
}

void(* resetFunc) (void) = 0;

void IRAM_ATTR resetConfig() {
    SERIAL_LOG_LN("Resetting the wifi configuration.");
    manager.resetSettings();
    ESP.eraseConfig();

    if (LittleFS.exists(DATA_FILE)) {
        LittleFS.remove(DATA_FILE);
    }

    delay(200);
    resetFunc();
}

void power(bool &on) {
    // TODO do
    data.isOn = on;
    saveData();
}

void setTemp(float &temp) {
    // todo do
    data.targetTemp = temp;
    saveData();
}

void setMode(String &mode) {
    if (mode == "AUTO") {
        data.isAutoMode = true;
    } else if (mode == "COOL") {
        data.isAutoMode = false;
    }

    saveData();
}

void setRange(int &range) {
    // todo do
    data.level = range;
    saveData();
}

void setup() {
#ifdef ENABLE_SERIAL_LOGGING
    Serial.begin(9600);
    bool enableSerial = true;
#else
    bool enableSerial = false;
#endif

    LittleFS.begin();
//    resetConfig(); // Enable to reset

    WiFi.setSleepMode(WIFI_NONE_SLEEP);

    WiFiManagerParameter weatherApiKey("weatherApiKey", "weatherApiKey", NULL, OPENWEATHER_API_KEY_SIZE);
    WiFiManagerParameter lat("lat", "latitude", NULL, LAT_SIZE);
    WiFiManagerParameter lon("lon", "longitude", NULL, LON_SIZE);
    WiFiManagerParameter appKey("appKey", "Sinric app key", NULL, APP_KEY_SIZE);
    WiFiManagerParameter appSecret("appSecret", "Sinric app secret", NULL, APP_SECRET_SIZE);
    WiFiManagerParameter deviceId("deviceId", "Sinric device id", NULL, DEVICE_ID_SIZE);

    manager.addParameter(&weatherApiKey);
    manager.addParameter(&lat);
    manager.addParameter(&lon);
    manager.addParameter(&appKey);
    manager.addParameter(&appSecret);
    manager.addParameter(&deviceId);

    manager.setSaveConfigCallback([]() {
        shouldSaveCredentials = true;
    });
    manager.autoConnect(AP_NAME, AP_PASSWORD);

#ifdef ENABLE_SERIAL_LOGGING
    manager.setDebugOutput(true);
#endif

    dxlogger::setup(TZ_America_Fortaleza,
                    30000,
                    enableSerial);
    dxweather::setup();

    if (shouldSaveCredentials) {
        SERIAL_LOG_LN("Saving data");
        strncpy(data.weatherApiKey, weatherApiKey.getValue(), OPENWEATHER_API_KEY_SIZE);
        strncpy(data.lat, lat.getValue(), LAT_SIZE);
        strncpy(data.lon, lon.getValue(), LON_SIZE);
        strncpy(data.appKey, appKey.getValue(), APP_KEY_SIZE);
        strncpy(data.appSecret, appSecret.getValue(), APP_SECRET_SIZE);
        strncpy(data.deviceId, deviceId.getValue(), DEVICE_ID_SIZE);
        saveData();
    } else {
        SERIAL_LOG_LN("Recovering data.");
        recoverData();
    }

    SERIAL_LOG_LN("Startup sinric");

    SinricACWithTemp &localAc = SinricPro[data.deviceId];
    ac = &localAc;

    ac->onPowerState([](const String &deviceId, bool &state) {
        SERIAL_LOG("SINRIC_EVENT: Turn ");
        SERIAL_LOG_LN(state ? "ON" : "OFF");
        power(state);
        return true;
    });

    ac->onTargetTemperature([](const String &deviceId, float &temperature) {
        SERIAL_LOG("SINRIC_EVENT: Set target temperature to ");
        SERIAL_LOG_LN(String(temperature));
        setTemp(temperature);
        return true;
    });

    ac->onThermostatMode([](const String &deviceId, String &mode) {
        SERIAL_LOG("SINRIC_EVENT: Set mode to ");
        SERIAL_LOG_LN(mode);
        setMode(mode);
        return true;
    });

    ac->onRangeValue([](const String &deviceId, int &rangeValue) {
        SERIAL_LOG("SINRIC_EVENT: Set fan speed to ");
        SERIAL_LOG_LN(String(rangeValue));
        setRange(rangeValue);
        return true;
    });

//    ac.onAdjustTargetTemperature(onAdjustTargetTemperature);
//    ac.onAdjustRangeValue(onAdjustRangeValue);

    // setup SinricPro
    SinricPro.onConnected([](){ SERIAL_LOG_LN("Connected to SinricPro"); });
    SinricPro.onDisconnected([](){ SERIAL_LOG_LN("Disconnected from SinricPro"); });
    SinricPro.begin(data.appKey, data.appSecret);
    ac->sendPowerStateEvent(true);

    lastTempEvent = -TEMP_UPDATE_INTERVAL;
    SERIAL_LOG_LN("Startup done");
}

void loop() {
    SinricPro.handle();
    dxlogger::update();

    if (SinricPro.isConnected() && millis() - lastTempEvent > TEMP_UPDATE_INTERVAL) {
        SERIAL_LOG_LN("Getting weather info...");
        dxweather::getWeatherInfo(data.weatherApiKey, data.lat, data.lon, weatherInfo);

        if (weatherInfo.hasError) {
            SERIAL_LOG("Error getting weather info: ");
            SERIAL_LOG_LN(weatherInfo.error);
        } else {
            SERIAL_LOG_LN("Success.");

            SERIAL_LOG("Ambient temperature is ");
            SERIAL_LOG(String(weatherInfo.temp));
            SERIAL_LOG(", rounded to: ");
            float rounded = round(weatherInfo.temp);
            SERIAL_LOG_LN(String(rounded));
            ac->sendTemperatureEvent(rounded);
            SERIAL_LOG_LN("Temperature updated.");
        }

        lastTempEvent = millis();
    }
}