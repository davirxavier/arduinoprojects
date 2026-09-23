#include <Arduino.h>
#include <secrets.h>

#define ESP_CONFIG_PAGE_ENABLE_LOGGING
#define MQTTO_ENABLE_LOGGING
#include <esp-config-page.h>
#include <Mosquetteer.h>
#include <ir_Samsung.h>

#define IR_PIN 0
#define TEMP_ID "temp"
#define HUMIDITY_ID "hum"
#define DEW_POINT_ID "dewpoint"
#define TARGET_TEMP_ID "target_temp"
#define FAN_SPEED_ID "fan"
#define MODE_ID "mode"
#define AC_MODE_AUTO "AUTO"
#define AC_MODE_DRY "DRY"
#define AC_MODE_COOL "COOL"

#define FAN_AUTO "AUTO"
#define FAN_LOW "BAIXA"
#define FAN_MED "MED"
#define FAN_HIGH "ALTA"
#define FAN_TURBO "TURBO"

#define MIN_SETPOINT 16
#define MAX_SETPOINT 30

Mosquetteer mosquetteer;
bool mqttInit = false;

ESP_CONFIG_PAGE::WEBSERVER_T server(80);
IRSamsungAc ac(IR_PIN);

const char *modeOptions[] = {
    AC_MODE_AUTO,
    AC_MODE_DRY,
    AC_MODE_COOL,
};
size_t optionsSize = 3;

const char *fanOptions[] = {
    FAN_AUTO,
    FAN_LOW,
    FAN_MED,
    FAN_HIGH,
    FAN_TURBO,
};
size_t fanOptionsSize = 5;
int fanSpeedByMode[] = {kSamsungAcFanAuto, kSamsungAcFanLow, kSamsungAcFanMed, kSamsungAcFanHigh, kSamsungAcFanTurbo};

struct AcSettings
{
    int setpoint = 30;
    char userMode[16] = AC_MODE_COOL;
    char fanMode[16] = FAN_LOW;
};
AcSettings settings{};

unsigned long sensorUpdateInterval = 5000;
unsigned long lastUpdatedSensors = -sensorUpdateInterval;

int getFan(const char *mode)
{
    int speed = kSamsungAcFanLow;
    for (int i = 0; i < fanOptionsSize; i++)
    {
        if (strcasecmp(fanOptions[i], mode) == 0)
        {
            return fanSpeedByMode[i];
        }
    }
    return speed;
}

void updateSensors()
{
    float temp = 30;
    float humidity = 50;

    float alpha = ((17.27 * temp) / (237.7 + temp)) + log(humidity / 100.0);
    float dewPoint = (237.7 * alpha) / (17.27 - alpha);

    int setpointOffset = 0;
    int actualMode = kSamsungAcCool;
    int actualFanSpeed = getFan(settings.fanMode);

    if (strcasecmp(AC_MODE_AUTO, settings.userMode) == 0)
    {
        if (dewPoint >= 18.5)
        {
            setpointOffset = -99;
            actualMode = kSamsungAcDry;
            actualFanSpeed = kSamsungAcFanTurbo;
        }
        else if (dewPoint >= 14.5)
        {
            setpointOffset = -4;
            actualMode = kSamsungAcDry;
            actualFanSpeed = kSamsungAcFanTurbo;
        }
        else if (dewPoint >= 13)
        {
            setpointOffset = -2;
            actualMode = kSamsungAcCool;
        }
        else
        {
            setpointOffset = 0;
            actualMode = kSamsungAcCool;
        }
    }
    else
    {
        actualMode = strcasecmp(AC_MODE_COOL, settings.userMode) == 0 ? kSamsungAcCool : kSamsungAcDry;
    }

    ac.setMode(actualMode);
    ac.setFan(actualFanSpeed);
    ac.setTemp(constrain(settings.setpoint + setpointOffset, MIN_SETPOINT, MAX_SETPOINT));

    if (mqttInit)
    {
        mosquetteer.sendState(TEMP_ID, temp);
        mosquetteer.sendState(HUMIDITY_ID, humidity);
        mosquetteer.sendState(DEW_POINT_ID, dewPoint);
    }
}

void setup()
{
    delay(1500);
    Serial.begin(115200);

    ac.begin();

    ESP_CONFIG_PAGE::setAPConfig("ESP32-AC-CONTROL", "adminadmin");
    ESP_CONFIG_PAGE::initModules(&server, CONFIG_PAGE_USER, CONFIG_PAGE_PASS, "ESP32-AC-CONTROL");
    server.begin();

    mosquetteer.setClientConfig(MQTT_URL, MQTT_USER, MQTT_PASS);
    mosquetteer.setDeviceInfo("esp_ac_controller", "AC Controller");
    mosquetteer.setTlsConfig(cacert);

    MosquetteerShared::SensorConfig sensorConfig{};
    sensorConfig.stateClass = MosquetteerShared::MEASUREMENT;
    strcpy(sensorConfig.unit.unit, "\xC2\xBA" "C");
    mosquetteer.define(TEMP_ID, "Temperature", MosquetteerShared::SENSOR, &sensorConfig);

    sensorConfig.deviceClass = MosquetteerClasses::HUMIDITY;
    strcpy(sensorConfig.unit.unit, "%");
    mosquetteer.define(HUMIDITY_ID, "Humidity", MosquetteerShared::SENSOR, &sensorConfig);

    strcpy(sensorConfig.unit.unit, "\xC2\xBA" "C");
    mosquetteer.define(DEW_POINT_ID, "Dew Point", MosquetteerShared::SENSOR, &sensorConfig);

    MosquetteerShared::SelectConfig selectConfig{};
    selectConfig.options = modeOptions;
    selectConfig.optionsCount = optionsSize;
    mosquetteer.define(MODE_ID, "Mode", MosquetteerShared::SELECT, &selectConfig);
    mosquetteer.onCommand(MODE_ID, [](const MosquetteerProp &prop)
    {
        strcpy(settings.userMode, prop.toString());
        mosquetteer.sendState(MODE_ID, prop.toString());
    });

    MosquetteerShared::NumberConfig numberConfig{};
    numberConfig.minMax.min = MIN_SETPOINT;
    numberConfig.minMax.max = MAX_SETPOINT;
    numberConfig.step = 1;
    strcpy(numberConfig.unit.unit, "\xC2\xBA" "C");
    mosquetteer.define(TARGET_TEMP_ID, "Target Temp", MosquetteerShared::NUMBER, &numberConfig);
    mosquetteer.onCommand(TARGET_TEMP_ID, [](const MosquetteerProp &prop)
   {
       settings.setpoint = prop.toInt();
       mosquetteer.sendState(TARGET_TEMP_ID, prop.toString());
   });

    selectConfig.options = fanOptions;
    selectConfig.optionsCount = fanOptionsSize;
    mosquetteer.define(FAN_SPEED_ID, "Fan Speed", MosquetteerShared::SELECT, &selectConfig);
    mosquetteer.onCommand(FAN_SPEED_ID, [](const MosquetteerProp &prop)
   {
       snprintf(settings.fanMode, sizeof(settings.fanMode), "%s", prop.toString());
       mosquetteer.sendState(FAN_SPEED_ID, prop.toString());
   });
}

void loop()
{
    ESP_CONFIG_PAGE::loop();
    server.handleClient();

    if (!mqttInit && WiFi.status() == WL_CONNECTED)
    {
        mqttInit = true;
        mosquetteer.begin();
    }

    if (millis() - lastUpdatedSensors > sensorUpdateInterval)
    {
        updateSensors();
        lastUpdatedSensors = millis();
    }
}