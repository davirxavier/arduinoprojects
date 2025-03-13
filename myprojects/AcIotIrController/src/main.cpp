#define SINRICPRO_NOSSL
#define FIRMWARE_VERSION "1.1.2"

#include <Arduino.h>
#include "ArduinoJson.h"
#include "LittleFS.h"
#include "TZ.h"
#include "Capabilities/TemperatureSensor.h"
#include "weather-utils.h"
#include "ir_Samsung.h"
#include "SinricPro.h"
#include "SinricProWindowAC.h"
#include "esp-config-page.h"
#include "esp-config-page-logging.h"

#define AP_NAME "ESP-AC-CONTROL-1"
#define AP_PASSWORD "admin13246"
#define DATA_FILE "data"
#define TEMP_UPDATE_INTERVAL 600000

class SinricACWithTemp : public SinricProWindowAC, public TemperatureSensor<SinricACWithTemp> {
    friend class TemperatureSensor<SinricACWithTemp>;
public:
    SinricACWithTemp(const String &deviceId) : SinricProWindowAC(deviceId) {}
};

struct AcData {
    uint8_t targetTemp;
    uint8_t roomTemp;
    uint8_t fanSpeed;
    uint8_t degreesSubtract;
    bool isAutoMode;
    bool isOn;
    bool swing;
    bool isEco;
} __attribute__((packed));

dxweather::WeatherInfo weatherInfo{};
AcData data{};
unsigned long lastTempEvent;
SinricACWithTemp *ac;
HTTPClient http;
WiFiClientSecure client;
IRSamsungAc acIr(4);
bool firstUpdate = false;
bool sinricInit = false;

ESP_CONFIG_PAGE::WEBSERVER_T server(80);
auto adminUsername = new ESP_CONFIG_PAGE::EnvVar("ADMIN_USER");
auto adminPassword = new ESP_CONFIG_PAGE::EnvVar("ADMIN_PASS");
auto weatherApiKey = new ESP_CONFIG_PAGE::EnvVar("WEATHER_API_KEY");
auto lat = new ESP_CONFIG_PAGE::EnvVar("WEATHER_LAT");
auto lon = new ESP_CONFIG_PAGE::EnvVar("WEATHER_LON");
auto appKey = new ESP_CONFIG_PAGE::EnvVar("SINRIC_APP_KEY");
auto appSecret = new ESP_CONFIG_PAGE::EnvVar("SINRIC_APP_SECRET");
auto deviceId = new ESP_CONFIG_PAGE::EnvVar("SINRIC_DEVICE_ID");

ESP_CONFIG_PAGE_LOGGING::ConfigPageSerial webserial(Serial);
#define LOG(str) webserial.print(str)
#define LOGN(str) webserial.println(str)
#define LOGF(str, params...) webserial.printf(str, params)

void(* resetFunc) (void) = 0;

void IRAM_ATTR resetConfig() {
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
            fanSpeedMapping = kSamsungAcFanAuto;
            break;
        case 2:
            fanSpeedMapping = kSamsungAcFanHigh;
            break;
        default:
            fanSpeedMapping = kSamsungAcFanTurbo;
            break;
    }

    LOG("Sending state: ");
    LOG(data.isOn ? "POWER ON, " : "POWER OFF, ");
    LOG(data.isAutoMode ? "IS AUTO MODE, " : "AUTO MODE DISABLED, ");
    LOGN("TEMP " + String(targetTemp) + String(data.isAutoMode ? "(AUTO)" : ""));

    acIr.setTemp(targetTemp);
    acIr.setMode(kSamsungAcCool);
    acIr.setFan(fanSpeedMapping);
    acIr.setSwing(data.swing);
    acIr.setPower(data.isOn);
    acIr.send();

    ac->sendTargetTemperatureEvent(targetTemp);
}

void power(bool &on) {
    data.isOn = on;
    updateState();
}

void setTemp(uint8_t temp) {
    LOGN("Setting temp.");
    if (data.isAutoMode) {
        LOGN("Is auto mode, ignoring target temp event.");
        return;
    }

    data.targetTemp = temp;
    updateState();
}

void setMode(String &mode) {
    if (mode == "AUTO") {
        data.isAutoMode = true;
        data.swing = false;
        data.isEco = false;
    } else if (mode == "COOL") {
        data.isAutoMode = false;
        data.swing = false;
        data.isEco = false;
    } else if (mode == "HEAT") {
        data.swing = true;
        data.isEco = false;
    } else if (mode == "ECO") {
        data.isEco = true;
        data.swing = false;
    }

    updateState();
}

void setFanSpeed(int &range) {
    if (data.isEco) {
        data.degreesSubtract = range-1;
        ac->sendPushNotification("Diferença de temperatura ajustada para -" + String(range-1) + "ºC.");
    } else {
        data.fanSpeed = range;
    }

    updateState();
}

void setup() {
#ifdef ENABLE_LOGGING
    Serial.begin(9600);
#else
    bool enableSerial = false;
#endif

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    LittleFS.begin();

    char defaultCreds[] = "admin";
    adminUsername->value = (char*) malloc(strlen(defaultCreds)+1);
    strcpy(adminUsername->value, defaultCreds);

    adminPassword->value = (char*) malloc(strlen(defaultCreds)+1);
    strcpy(adminPassword->value, defaultCreds);

    ESP_CONFIG_PAGE::setSerial(&webserial);

    addEnvVar(adminUsername);
    addEnvVar(adminPassword);
    addEnvVar(weatherApiKey);
    addEnvVar(lat);
    addEnvVar(lon);
    addEnvVar(appKey);
    addEnvVar(appSecret);
    addEnvVar(deviceId);

    ESP_CONFIG_PAGE::setAndUpdateStorage(new ESP_CONFIG_PAGE::LittleFSKeyValueStorage("env"));
    ESP_CONFIG_PAGE::setAPConfig("ESP32-SAC-IOT", "adminadmin");
    ESP_CONFIG_PAGE::tryConnectWifi(false);
    ESP_CONFIG_PAGE::initModules(&server, String(adminUsername->value), String(adminPassword->value), "ESP32-SAC-IOT");

    dxweather::setup();
    data.fanSpeed = 1;
    data.isAutoMode = false;
    data.targetTemp = 30;
    data.roomTemp = 30;
    data.isOn = false;
    data.isAutoMode = false;
    data.degreesSubtract = 1;
    data.isEco = false;

    server.begin();
    ESP_CONFIG_PAGE_LOGGING::enableLogging(String(adminUsername->value), String(adminPassword->value), webserial);

    LOGN("Startup done");
    delay(5000);
}

void loop() {
    SinricPro.handle();

    if (!sinricInit && ESP_CONFIG_PAGE::isWiFiReady())
    {
        LOGN("Startup sinric....");
        acIr.begin();
        SinricACWithTemp &localAc = SinricPro[deviceId->value];
        ac = &localAc;

        ac->onPowerState([](const String &deviceId, bool &state) {
            LOG("SINRIC_EVENT: Turn ");
            LOGN(state ? "ON" : "OFF");
            power(state);
            return true;
        });

        ac->onTargetTemperature([](const String &deviceId, float &temperature) {
            LOG("SINRIC_EVENT: Set target temperature to ");
            LOGN(String(temperature));
            setTemp(temperature);
            return true;
        });

        ac->onThermostatMode([](const String &deviceId, String &mode) {
            LOG("SINRIC_EVENT: Set mode to ");
            LOGN(mode);
            setMode(mode);
            return true;
        });

        ac->onRangeValue([](const String &deviceId, int &rangeValue) {
            LOG("SINRIC_EVENT: Set fan speed to ");
            LOGN(String(rangeValue));
            setFanSpeed(rangeValue);
            return true;
        });

        // setup SinricPro
        SinricPro.onConnected([](){ LOGN("Connected to SinricPro"); });
        SinricPro.onDisconnected([](){ LOGN("Disconnected from SinricPro"); });

        SinricPro.begin(appKey->value, appSecret->value);
        ac->sendPowerStateEvent(data.isOn);
        ac->sendTargetTemperatureEvent(data.targetTemp);
        ac->sendTemperatureEvent(data.roomTemp);
        ac->sendRangeValueEvent(data.fanSpeed);
        ac->sendThermostatModeEvent(data.isAutoMode ? "AUTO" : "COOL");

        lastTempEvent = -TEMP_UPDATE_INTERVAL-1;
        sinricInit = true;
    }

    if (sinricInit && SinricPro.isConnected() && data.isOn && millis() - lastTempEvent > TEMP_UPDATE_INTERVAL) {
        digitalWrite(LED_BUILTIN, LOW);
        unsigned long startMillis = millis();

        LOGN("Getting weather info...");
        dxweather::getWeatherInfo(weatherApiKey->value, lat->value, lon->value, weatherInfo);

        if (weatherInfo.hasError) {
            LOGF("Error getting weather info: %s\n", weatherInfo.error.c_str());
        } else {
            LOGN("Success.");

            LOGF("Ambient temperature is %f, feelsLike is %f\n", weatherInfo.temp, weatherInfo.feelsLike);

            float rounded = round(weatherInfo.temp);
            LOGF("Rounded temp to: %f\n", rounded);
            ac->sendTemperatureEvent(rounded, weatherInfo.humidity);
            LOGN("Temperature updated.");

            if (rounded != data.roomTemp || !firstUpdate) {
                ac->sendPushNotification("Temperatura ambiente é " + String(rounded) + "°C, alterando "
                                           "temperatura do ar condicionado "
                                           "automaticamente para " + String(data.roomTemp - data.degreesSubtract) + "°C.");

                LOGN("Temperature is different from current, updating AC unit.");
                data.roomTemp = rounded;
                updateState();
            }

            if (!firstUpdate) {
                ac->sendPushNotification("Sistema automático para ar condicionado ligado, diferença de temperatura configurada é -" +
                                         String(data.degreesSubtract) + "°C.");
                firstUpdate = true;
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