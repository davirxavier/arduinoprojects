#define SINRICPRO_NOSSL

#include <Arduino.h>
#include <SinricPro.h>
#include <SinricProWindowAC.h>
#include <SinricProTemperaturesensor.h>
#include <esp-config-page.h>
#include "defines.h"
#include <IRremote.h>
#include <AirConditionerRemote.h>
#include <weather-utils.h>

#define COMPRESSOR_PIN 13
#define FAN_PIN 12
#define IR_PIN 14
#define BUZZER_PIN 5
#define TEMP_PIN A0

#define TEMP_READINGS 60
#define TEMP_READING_DELAY 1000
#define WEATHER_READING_DELAY 300000
#define TEMP_SENSOR_PULLUP 220000
#define TEMP_SENSOR_PULLDOWN 46000
#define DEGREES_UPPER_LIMIT 0.85
#define DEGREES_LOWER_LIMIT 1.35
#define MIN_TEMP 18
#define MAX_TEMP 32

#define ENABLE_LOG
#ifdef ENABLE_LOG
#define LOG(str) webserial.print(str)
#define LOGN(str) webserial.println(str)
#define LOGF(str, p...) webserial.printf(str, p)
#else
#define LOG(str)
#define LOGN(str)
#define LOGF(str, p...)
#endif

class SinricACWithTemp : public SinricProWindowAC, public TemperatureSensor<SinricACWithTemp>
{
    friend class TemperatureSensor<SinricACWithTemp>;

public:
    explicit SinricACWithTemp(const String& deviceId) : SinricProWindowAC(deviceId)
    {
    }
};

#ifdef ENABLE_LOG
ESP_CONFIG_PAGE_LOGGING::ConfigPageSerial webserial(Serial);
#endif

ESP_CONFIG_PAGE::WEBSERVER_T server(80);
auto usernameVar = new ESP_CONFIG_PAGE::EnvVar("USERNAME", "");
auto passVar = new ESP_CONFIG_PAGE::EnvVar("PASSWORD", "");
auto sinricAppKey = new ESP_CONFIG_PAGE::EnvVar("SINRIC_APP_KEY", "");
auto sinricAppSecret = new ESP_CONFIG_PAGE::EnvVar("SINRIC_APP_SECRET", "");
auto sinricDeviceId = new ESP_CONFIG_PAGE::EnvVar("SINRIC_DEVICE_ID", "");
auto turnOnDelayTime = new ESP_CONFIG_PAGE::EnvVar("COMPRESSOR_SWITCH_DELAY_M", "4");
auto weatherApiKey = new ESP_CONFIG_PAGE::EnvVar("WEATHER_API_KEY", "");
auto weatherLat = new ESP_CONFIG_PAGE::EnvVar("WEATHER_LATITUDE", "");
auto weatherLon = new ESP_CONFIG_PAGE::EnvVar("WEATHER_LONGITUDE", "");
bool sinricEnabled = true;

Data currentData;
unsigned long compressorTimeout = 0;
float tempReadings[TEMP_READINGS];
uint8_t currentTempReading = TEMP_READINGS-1;
unsigned long tempTimer = 0;
unsigned long compressorSwitchDelay = 4 * 60 * 1000;

bool sinricConnectionStart = false;
bool sinricInit = false;

IRrecv ir(IR_PIN);
AirConditionerRemote acRemote;

unsigned long remoteTimeout = 0;
bool remoteResumed = false;

unsigned long beep = 0;
unsigned long beepTimeout = 0;

void beepFn(unsigned long ms)
{
    beep = ms;
    beepTimeout = millis();
    digitalWrite(BUZZER_PIN, HIGH);
}

void beepShort()
{
    beepFn(120);
}

void beepLong()
{
    beepFn(220);
}

#define SINRIC_DISABLED_FILE "/sinric_disabled"

bool isSinricEnabled()
{
    return !LittleFS.exists(SINRIC_DISABLED_FILE);
}

void enableSinric(bool enabled)
{
    if (enabled)
    {
        if (LittleFS.exists(SINRIC_DISABLED_FILE))
        {
            LittleFS.remove(SINRIC_DISABLED_FILE);
        }
    }
    else
    {
        File file = LittleFS.open(SINRIC_DISABLED_FILE, "w");
        file.write('w');
        file.close();
    }
}

void updateTargetTemp(float &newTargetTemp)
{
    if (newTargetTemp > MAX_TEMP)
    {
        newTargetTemp = MAX_TEMP;
    }
    else if (newTargetTemp < MIN_TEMP)
    {
        newTargetTemp = MIN_TEMP;
    }

    currentData.userTemperature = newTargetTemp;
}

void updateState(const State::State newState, bool remote = false)
{
    switch (newState)
    {
    case State::OFF:
        {
            digitalWrite(COMPRESSOR_PIN, LOW);
            digitalWrite(FAN_PIN, LOW);
            break;
        }
    // case State::FAN_ONLY:
    //     {
    //         digitalWrite(FAN_PIN, HIGH);
    //         digitalWrite(COMPRESSOR_PIN, LOW);
    //         break;
    //     }
    case State::COOL_COMPRESSOR_OFF:
        {
            digitalWrite(FAN_PIN, HIGH);
            digitalWrite(COMPRESSOR_PIN, LOW);
            break;
        }
    case State::COOL_COMPRESSOR_ON:
        {
            digitalWrite(FAN_PIN, HIGH);
            digitalWrite(COMPRESSOR_PIN, HIGH);
            compressorTimeout = millis();
            break;
        }
    }

    if (currentData.currentState == State::COOL_COMPRESSOR_ON && newState != State::COOL_COMPRESSOR_ON)
    {
        compressorTimeout = millis();
    }

    if (SinricPro.isConnected())
    {
        SinricACWithTemp &ac = SinricPro[sinricDeviceId->value];

        if (currentData.currentState == State::OFF && newState != State::OFF)
        {
            if (remote)
            {
                ac.sendPowerStateEvent(true);
            }

            ac.sendRangeValueEvent(2);
        }

        if (currentData.currentState != State::OFF && newState == State::OFF)
        {
            if (remote)
            {
                ac.sendPowerStateEvent(false);
            }

            ac.sendRangeValueEvent(1);
        }

        if (currentData.currentState == State::COOL_COMPRESSOR_OFF && newState == State::COOL_COMPRESSOR_ON)
        {
            ac.sendRangeValueEvent(3);
        }

        if (currentData.currentState == State::COOL_COMPRESSOR_ON && newState == State::COOL_COMPRESSOR_OFF)
        {
            ac.sendRangeValueEvent(2);
        }
    }

    currentData.currentState = newState;
}

void sinricInitFn()
{
    if (!sinricEnabled)
    {
        return;
    }

    SinricPro.begin(sinricAppKey->value, sinricAppSecret->value);

    SinricACWithTemp &ac = SinricPro[sinricDeviceId->value];

    ac.onPowerState([](const String& deviceId, const bool& on)
    {
        updateState(on ? State::COOL_COMPRESSOR_OFF : State::OFF);
        beepShort();
        return true;
    });

    ac.onTargetTemperature([](const String& deviceId, float& targetTemp)
    {
        updateTargetTemp(targetTemp);
        beepShort();
        return true;
    });

    ac.onAdjustTargetTemperature([](const String& deviceId, float& delta)
    {
        delta = currentData.userTemperature;
        return true;
    });

    ac.onThermostatMode([](const String &deviceId, String &mode)
    {
        mode = "COOL";
        beepShort();
        return true;
    });

    ac.onRangeValue([](const String& deviceId, int& range)
    {
        range = currentData.currentState == State::COOL_COMPRESSOR_ON ? 3 : 2;
        return true;
    });

    ac.onAdjustRangeValue([](const String& deviceId, int& delta)
    {
        delta = currentData.currentState == State::COOL_COMPRESSOR_ON ? 3 : 2;
        return true;
    });
}

void setup()
{
#ifdef ENABLE_LOG
    webserial.begin(115200);
#endif

    pinMode(COMPRESSOR_PIN, OUTPUT);
    pinMode(FAN_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    digitalWrite(COMPRESSOR_PIN, LOW);
    digitalWrite(FAN_PIN, LOW);

    ESP_CONFIG_PAGE::setAPConfig("ESP-AC-01", "admin1234");
    ESP_CONFIG_PAGE::tryConnectWifi(false, 10000);

    ESP_CONFIG_PAGE::addEnvVar(usernameVar);
    ESP_CONFIG_PAGE::addEnvVar(passVar);
    ESP_CONFIG_PAGE::addEnvVar(sinricAppKey);
    ESP_CONFIG_PAGE::addEnvVar(sinricAppSecret);
    ESP_CONFIG_PAGE::addEnvVar(sinricDeviceId);
    ESP_CONFIG_PAGE::addEnvVar(turnOnDelayTime);
    ESP_CONFIG_PAGE::addEnvVar(weatherApiKey);
    ESP_CONFIG_PAGE::addEnvVar(weatherLat);
    ESP_CONFIG_PAGE::addEnvVar(weatherLon);
    ESP_CONFIG_PAGE::setAndUpdateEnvVarStorage(new ESP_CONFIG_PAGE::LittleFSKeyValueStorage("/env"));

    ESP_CONFIG_PAGE::addCustomAction("ENABLE SINRIC", [](ESP_CONFIG_PAGE::WEBSERVER_T &server)
    {
        enableSinric(true);
        server.send(200);
        delay(200);
        ESP.restart();
        return true;
    });
    ESP_CONFIG_PAGE::addCustomAction("DISABLE SINRIC", [](ESP_CONFIG_PAGE::WEBSERVER_T &server)
    {
        enableSinric(false);
        server.send(200);
        SinricPro.stop();
        delay(200);
        ESP.restart();
        return true;
    });

    ESP_CONFIG_PAGE::initModules(&server, usernameVar->value, passVar->value, "ESP-AC-1");

    server.begin();

#ifdef ENABLE_LOG
    ESP_CONFIG_PAGE_LOGGING::enableLogging(usernameVar->value, passVar->value, webserial);
#endif

    if (turnOnDelayTime->value != nullptr && strlen(turnOnDelayTime->value) > 0)
    {
        compressorSwitchDelay = String(turnOnDelayTime->value).toInt() * 60 * 1000;
    }

    ir.enableIRIn();

    sinricEnabled = isSinricEnabled();

    for (float &tempReading : tempReadings)
    {
        tempReading = 0;
    }

    digitalWrite(BUZZER_PIN, HIGH);
    delay(300);
    digitalWrite(BUZZER_PIN, LOW);
}

void loop()
{
    server.handleClient();
    ESP_CONFIG_PAGE::loop();
    ESP_CONFIG_PAGE_LOGGING::loop();
    SinricPro.handle();

    if (beep > 0 && millis() - beepTimeout > beep)
    {
        beep = 0;
        digitalWrite(BUZZER_PIN, LOW);
    }

    if (!sinricConnectionStart && ESP_CONFIG_PAGE::isWiFiReady())
    {
        sinricConnectionStart = true;
        sinricInitFn();
    }

    if (sinricConnectionStart && !sinricInit && sinricEnabled && SinricPro.isConnected())
    {
        LOGN("Sinric init successful");
        sinricInit = true;

        SinricACWithTemp &ac = SinricPro[sinricDeviceId->value];

        ac.sendThermostatModeEvent("COOL");
        ac.sendPowerStateEvent(false);
        ac.sendRangeValueEvent(1);
        ac.sendTargetTemperatureEvent(currentData.userTemperature);
    }

    if (!remoteResumed && millis()-remoteTimeout > 200) {
        ir.resume();
        remoteResumed = true;
    }

    if (remoteResumed && ir.decode()) {
        bool commandRead = false;

        if (acRemote.readCommand(ir.decodedIRData.decodedRawDataArray,
                                 ir.decodedIRData.protocol,
                                 ir.decodedIRData.numberOfBits)) {
            LOGN("Command read.");

            if (acRemote.powerToggle) {
                updateState(currentData.currentState == State::OFF ? State::COOL_COMPRESSOR_OFF : State::OFF, true);
                commandRead = true;
                LOGN("Power command read.");
            }

            if (currentData.currentState != State::OFF) {
                currentData.userTemperature = acRemote.temp;
                LOGF("Setting user temp: %f\n", currentData.userTemperature);

                if (SinricPro.isConnected())
                {
                    SinricACWithTemp &ac = SinricPro[sinricDeviceId->value];
                    ac.sendTargetTemperatureEvent(currentData.userTemperature);
                }

                if (!acRemote.powerToggle)
                {
                    commandRead = true;
                }
            }

            acRemote.resetToggles();
        }

        if (commandRead) {
            remoteTimeout = millis();
            beepShort();
        }

        remoteResumed = false;
    }

    if (millis() - tempTimer > TEMP_READING_DELAY)
    {
        tempReadings[currentTempReading] = readTemp(TEMP_SENSOR_PULLUP, TEMP_SENSOR_PULLDOWN);
        LOGF("Get current temperature: %f.\n", tempReadings[currentTempReading]);
        currentTempReading++;

        if (currentTempReading >= TEMP_READINGS)
        {
            currentTempReading = 0;

            float average = 0;
            for (float tempReading : tempReadings)
            {
                average += tempReading;
            }
            currentData.currentTemperature = average / TEMP_READINGS;
            LOGF("Get average temperature: %f.\n", currentData.currentTemperature);

            float humidity = 0;
            if (SinricPro.isConnected() &&
                strlen(weatherApiKey->value) > 0 &&
                strlen(weatherLat->value) > 0 &&
                strlen(weatherLon->value) > 0)
            {
                dxweather::WeatherInfo weatherInfo;
                dxweather::getWeatherInfo(weatherApiKey->value, weatherLat->value, weatherLon->value, weatherInfo);
                humidity = weatherInfo.hasError ? 0 : weatherInfo.humidity;

                LOGF("Get weather humidity: %f\n", weatherInfo.humidity);
            }

            SinricACWithTemp &ac = SinricPro[sinricDeviceId->value];
            ac.sendTemperatureEvent(currentData.currentTemperature, humidity);
        }

        tempTimer = millis();
    }

    if (currentData.currentState == State::COOL_COMPRESSOR_OFF &&
        currentData.currentTemperature > (currentData.userTemperature + DEGREES_UPPER_LIMIT) &&
        millis() - compressorTimeout > compressorSwitchDelay)
    {
        updateState(State::COOL_COMPRESSOR_ON);
    }

    if (currentData.currentState == State::COOL_COMPRESSOR_ON &&
        currentData.currentTemperature < (currentData.userTemperature - DEGREES_UPPER_LIMIT) &&
        millis() - compressorTimeout > compressorSwitchDelay)
    {
        updateState(State::COOL_COMPRESSOR_OFF);
    }
}
