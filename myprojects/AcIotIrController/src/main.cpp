#define SINRICPRO_NOSSL
#define FIRMWARE_VERSION "0.0.13"

#include <Arduino.h>
#include "LittleFS.h"
#include "WiFiManager.h"
#include "ArduinoJson.h"
#include "logger.h"
#include "TZ.h"
#include "Capabilities/TemperatureSensor.h"
#include "weather-utils.h"
#include "ir_Samsung.h"
#include "SinricPro.h"
#include "SinricProWindowAC.h"
#include "ESP8266OTAHelper.h"
#include "SemVer.h"

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

struct AcData {
    char weatherApiKey[OPENWEATHER_API_KEY_SIZE];
    char lat[LAT_SIZE];
    char lon[LON_SIZE];
    char appKey[APP_KEY_SIZE];
    char appSecret[APP_SECRET_SIZE];
    char deviceId[DEVICE_ID_SIZE];
    uint8_t targetTemp;
    uint8_t roomTemp;
    uint8_t fanSpeed;
    uint8_t degreesSubtract;
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
IRSamsungAc acIr(4);
bool firstUpdate = false;

bool isCountingFanSets;
unsigned long fanSpeedCountTimeout;
uint8_t fanSpeedSetCount;
uint8_t firstFanSpeedInCount;

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
    doc["level"] = data.fanSpeed;
    doc["degreesSub"] = data.degreesSubtract;
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
    data.fanSpeed = doc["level"];
    data.roomTemp = doc["roomTemp"];
    data.isAutoMode = doc["mode"];
    data.isOn = doc["isOn"];

    // Needs to do this for every value added after first release
    if (doc.containsKey("degreesSub")) {
        data.degreesSubtract = doc["degreesSub"];
    }

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

void updateState() {
    uint8_t targetTemp = data.targetTemp;

    if (data.isAutoMode) {
        targetTemp = (data.roomTemp > 30 ? 30 : data.roomTemp);
        targetTemp = data.roomTemp < 16 ? 16 : (targetTemp - data.degreesSubtract);
    }

    uint8_t fanSpeedMapping;
    switch (data.fanSpeed) {
        case 1:
            fanSpeedMapping = kSamsungAcFanHigh;
            break;
        case 2:
            fanSpeedMapping = kSamsungAcFanAuto2;
            break;
        case 3:
            fanSpeedMapping = kSamsungAcFanTurbo;
            break;
        default:
            fanSpeedMapping = kSamsungAcFanTurbo;
            break;
    }

    SERIAL_LOG("Sending state: ");
    SERIAL_LOG(data.isOn ? "POWER ON, " : "POWER OFF, ");
    SERIAL_LOG(data.isAutoMode ? "IS AUTO MODE, " : "AUTO MODE DISABLED, ");
    SERIAL_LOG_LN("TEMP " + String(targetTemp) + String(data.isAutoMode ? "(AUTO)" : ""));

    acIr.setTemp(targetTemp);
    acIr.setMode(kSamsungAcCool);
    acIr.setFan(fanSpeedMapping);
    acIr.setSwing(false);
    acIr.setPower(data.isOn);
    acIr.send();

    ac->sendTargetTemperatureEvent(targetTemp);
}

void power(bool &on) {
    data.isOn = on;
    saveData();
    updateState();
}

void setTemp(uint8_t temp) {
    SERIAL_LOG_LN("Setting temp.");
    if (data.isAutoMode) {
        SERIAL_LOG_LN("Is auto mode, ignoring target temp event.");
        return;
    }

    data.targetTemp = temp;
    saveData();
    updateState();
}

void setMode(String &mode) {
    if (mode == "AUTO") {
        data.isAutoMode = true;
    } else if (mode == "COOL") {
        data.isAutoMode = false;
    }

    saveData();
    updateState();
}

void setFanSpeed(int &range) {
    if (isCountingFanSets) {
        fanSpeedSetCount++;

        if (range != firstFanSpeedInCount || millis() - fanSpeedCountTimeout > 60000) {
            isCountingFanSets = false;
        } else if (fanSpeedSetCount >= 4) {
            isCountingFanSets = false;
            data.degreesSubtract = range;
            ac->sendPushNotification("Diferença de temperatura do modo automático ajustada para -" + String(range) + "°C.");
        }
    } else {
        isCountingFanSets = true;
        fanSpeedCountTimeout = millis();
        fanSpeedSetCount = 1;
        firstFanSpeedInCount = range;
    }

    data.fanSpeed = range;
    saveData();
    updateState();
}

bool handleOTAUpdate(const String& url, int major, int minor, int patch, bool forceUpdate) {
    Version currentVersion  = Version(FIRMWARE_VERSION);
    Version newVersion      = Version(String(major) + "." + String(minor) + "." + String(patch));
    bool updateAvailable    = newVersion > currentVersion;

    SERIAL_LOG("URL: ");
    SERIAL_LOG_LN(url.c_str());
    SERIAL_LOG("Current version: ");
    SERIAL_LOG_LN(currentVersion.toString());
    SERIAL_LOG("New version: ");
    SERIAL_LOG_LN(newVersion.toString());
    if (forceUpdate) SERIAL_LOG_LN("Enforcing OTA update!");

    // Handle OTA update based on forceUpdate flag and update availability
    if (forceUpdate || updateAvailable) {
        if (updateAvailable) {
            SERIAL_LOG_LN("Update available!");
        }

        String result = startOtaUpdate(url);
        if (!result.isEmpty()) {
            SinricPro.setResponseMessage(std::move(result));
            return false;
        }
        return true;
    } else {
        String result = "Current version is up to date.";
        SinricPro.setResponseMessage(std::move(result));
        SERIAL_LOG_LN(result);
        return false;
    }
}

void setup() {
#ifdef ENABLE_SERIAL_LOGGING
    Serial.begin(9600);
    bool enableSerial = true;
#else
    bool enableSerial = false;
#endif

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

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
    data.fanSpeed = 1;
    data.isAutoMode = false;
    data.targetTemp = 30;
    data.roomTemp = 30;
    data.isOn = false;
    data.isAutoMode = false;
    data.degreesSubtract = 1;

    isCountingFanSets = false;
    fanSpeedSetCount = 0;
    fanSpeedCountTimeout = 0;
    firstFanSpeedInCount = 1;

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

    SERIAL_LOG_LN("Startup sinric....");
    acIr.begin();
    SinricACWithTemp &localAc = SinricPro[data.deviceId];
    ac = &localAc;

    ac->onPowerState([](const String &deviceId, bool &state) {
        isCountingFanSets = false;
        SERIAL_LOG("SINRIC_EVENT: Turn ");
        SERIAL_LOG_LN(state ? "ON" : "OFF");
        power(state);
        return true;
    });

    ac->onTargetTemperature([](const String &deviceId, float &temperature) {
        isCountingFanSets = false;
        SERIAL_LOG("SINRIC_EVENT: Set target temperature to ");
        SERIAL_LOG_LN(String(temperature));
        setTemp(temperature);
        return true;
    });

    ac->onThermostatMode([](const String &deviceId, String &mode) {
        isCountingFanSets = false;
        SERIAL_LOG("SINRIC_EVENT: Set mode to ");
        SERIAL_LOG_LN(mode);
        setMode(mode);
        return true;
    });

    ac->onRangeValue([](const String &deviceId, int &rangeValue) {
        SERIAL_LOG("SINRIC_EVENT: Set fan speed to ");
        SERIAL_LOG_LN(String(rangeValue));
        setFanSpeed(rangeValue);
        return true;
    });

//    ac.onAdjustTargetTemperature(onAdjustTargetTemperature);
//    ac.onAdjustRangeValue(onAdjustRangeValue);

    // setup SinricPro
    SinricPro.onConnected([](){ SERIAL_LOG_LN("Connected to SinricPro"); });
    SinricPro.onDisconnected([](){ SERIAL_LOG_LN("Disconnected from SinricPro"); });
    SinricPro.onOTAUpdate(handleOTAUpdate);

    SinricPro.begin(data.appKey, data.appSecret);
    ac->sendPowerStateEvent(data.isOn);
    ac->sendTargetTemperatureEvent(data.targetTemp);
    ac->sendTemperatureEvent(data.roomTemp);
    ac->sendRangeValueEvent(data.fanSpeed);
    ac->sendThermostatModeEvent(data.isAutoMode ? "AUTO" : "COOL");
    ac->sendPushNotification("Sistema automático para ar condicionado ligado, diferença de temperatura configurada é " +
                             String(data.degreesSubtract) + "°C.");

    lastTempEvent = -TEMP_UPDATE_INTERVAL;
    SERIAL_LOG_LN("Startup done");
    delay(5000);
}

void loop() {
    SinricPro.handle();
    dxlogger::update();

    if (isCountingFanSets && millis() - fanSpeedCountTimeout > 60000) {
        isCountingFanSets = false;
    }

    if (SinricPro.isConnected() && !firstUpdate) {
        SERIAL_LOG_LN("Doing first state update.");
        updateState();
        firstUpdate = true;
    }

    if (SinricPro.isConnected() && data.isOn && millis() - lastTempEvent > TEMP_UPDATE_INTERVAL) {
        digitalWrite(LED_BUILTIN, LOW);
        unsigned long startMillis = millis();

        SERIAL_LOG_LN("Getting weather info...");
        dxweather::getWeatherInfo(data.weatherApiKey, data.lat, data.lon, weatherInfo);

        if (weatherInfo.hasError) {
            SERIAL_LOG("Error getting weather info: ");
            SERIAL_LOG_LN(weatherInfo.error);
        } else {
            SERIAL_LOG_LN("Success.");

            SERIAL_LOG("Ambient temperature is ");
            SERIAL_LOG(String(weatherInfo.temp));
            SERIAL_LOG(", feelsLike is ");
            SERIAL_LOG_LN(String(weatherInfo.feelsLike));

            SERIAL_LOG("Rounded temp to: ");
            float rounded = round(weatherInfo.temp);
            SERIAL_LOG_LN(String(rounded));
            ac->sendTemperatureEvent(rounded, weatherInfo.humidity);
            SERIAL_LOG_LN("Temperature updated.");

            if (rounded != data.roomTemp) {
                ac->sendPushNotification("Temperatura ambiente é " + String(rounded) + "°C, alterando "
                                           "temperatura do ar condicionado "
                                           "automaticamente para " + String(data.roomTemp - data.degreesSubtract) + "°C.");

                SERIAL_LOG_LN("Temperature is different from current, updating AC unit.");
                data.roomTemp = rounded;
                updateState();
            }
        }

        unsigned long endMillis = millis();
        if (endMillis - startMillis < 2000) {
            delay(2000-(endMillis-startMillis));
        }
        digitalWrite(LED_BUILTIN, HIGH);

        lastTempEvent = millis();
    }
}