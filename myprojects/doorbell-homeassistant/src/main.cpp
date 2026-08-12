#include <Arduino.h>
#include <cfloat>

// #define ESP_CONFIG_PAGE_ENABLE_LOGGING
// #define MQTTO_ENABLE_LOGGING
#include <Mosquetteer.h>
#include <esp-config-page.h>
#include <secrets.h>

#define BUTTON_ID "doorbell_events"
#define ENABLED_ID "doorbell_enabled"
#define TEMP_ID "doorbell_temp"
#define GARAGE_TRIGGER_ID "garage_trigger"
#define GARAGE_ENABLE_ID "garage_enable"

#define BUTTON_PIN 1
#define GARAGE_TRIGGER_PIN 6
#define GARAGE_ENABLE_PIN 7
#define THERM_PIN 0

#define THERM_REF_TEMP_K 298.15f
#define THERM_RES 10000.0f // temp at 25º
#define THERM_RES_DIV 10000.0f // fixed resistor in divider
#define THERM_BETA 3950.0f
#define THERM_VCC 3300.0f
#define THERM_MAX_TEMP 2800.0f

ESP_CONFIG_PAGE::WEBSERVER_T httpServer(80);
Mosquetteer mosquetteer;
bool mqttBegun = false;

bool hasPress = false;
unsigned long lastPress = 0;

bool triggerGarage = false;
unsigned long lastGarageTrigger = 0;

unsigned long lastTempRead = 0;

const char *eventTypes[] = {
    "button_pressed"
};
size_t typesSize = 1;

float readTemp()
{
    constexpr int samples = 16;

    uint32_t adc = 0;

    for (int i = 0; i < samples; i++)
    {
        adc += analogRead(THERM_PIN);
        delayMicroseconds(20);
    }

    float adcAvg = adc / (float)samples;

    // ESP32-C3 ADC is nominally 12-bit
    constexpr float ADC_MAX = 4095.0f;

    if (adcAvg <= 0.0f || adcAvg >= ADC_MAX)
        return -9999.0f;

    // Divider:
    // 3.3V -> NTC -> ADC -> 10k -> GND
    float Rntc = THERM_RES_DIV * adcAvg / (ADC_MAX - adcAvg);

    float tempK =1.0f /(1.0f / THERM_REF_TEMP_K + log(Rntc / THERM_RES) / THERM_BETA);
    return tempK - 273.15f;
}
void setup()
{
    delay(1500);
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(GARAGE_TRIGGER_PIN, OUTPUT);
    pinMode(GARAGE_ENABLE_PIN, OUTPUT);
    pinMode(THERM_PIN, INPUT);

    digitalWrite(GARAGE_TRIGGER_PIN, LOW);
    digitalWrite(GARAGE_ENABLE_PIN, LOW);

    ESP_CONFIG_PAGE::setAPConfig("ESP32-BELL-V2", "adminadmin");
    ESP_CONFIG_PAGE::initModules(&httpServer, CONFIG_PAGE_USER, CONFIG_PAGE_PASS, "ESP32-BELL-V2");
    httpServer.begin();

    mosquetteer.setClientConfig(MQTT_URL, MQTT_USER, MQTT_PASS);
    mosquetteer.setDeviceInfo("esp_doorbell_button_v2", "Doorbell");
    mosquetteer.setTlsConfig(cacert);

    MosquetteerShared::EventConfig eventConfig{};
    eventConfig.eventTypes = eventTypes;
    eventConfig.eventTypeCount = typesSize;
    mosquetteer.define(BUTTON_ID, "Doorbell", MosquetteerShared::EVENT, &eventConfig);

    mosquetteer.define(ENABLED_ID, "Enabled", MosquetteerShared::SWITCH);
    mosquetteer.onCommand(ENABLED_ID, [](const MosquetteerProp &prop)
    {
        mosquetteer.sendState(ENABLED_ID, prop.isOnState());
    });

    MosquetteerShared::SensorConfig sensorConfig{};
    strcpy(sensorConfig.unit.unit, "\xC2\xBA" "C");
    mosquetteer.define(TEMP_ID, "Temperature", MosquetteerShared::SENSOR, &sensorConfig);

    mosquetteer.define(GARAGE_ENABLE_ID, "Enable Garage", MosquetteerShared::SWITCH);
    mosquetteer.onCommand(GARAGE_ENABLE_ID, [](const MosquetteerProp &prop)
    {
        bool enabled = prop.isOnState();
        mosquetteer.sendState(GARAGE_ENABLE_ID, enabled);
        Serial.println(prop.toString());
        digitalWrite(GARAGE_ENABLE_PIN, enabled ? LOW : HIGH);
    });

    mosquetteer.define(GARAGE_TRIGGER_ID, "Trigger Garage", MosquetteerShared::BUTTON);
    mosquetteer.onCommand(GARAGE_TRIGGER_ID, [](const MosquetteerProp &)
    {
        if (!triggerGarage && millis() - lastGarageTrigger > 100)
        {
            triggerGarage = true;
            digitalWrite(GARAGE_TRIGGER_PIN, HIGH);
            lastGarageTrigger = millis();
            Serial.println("Garage trigger.");
        }
    });
}

void loop()
{
    ESP_CONFIG_PAGE::loop();
    httpServer.handleClient();

    if (!mqttBegun && WiFi.status() == WL_CONNECTED)
    {
        mosquetteer.begin();
        mqttBegun = true;
    }

    if (digitalRead(BUTTON_PIN) == LOW && millis() - lastPress >= 2000)
    {
        hasPress = true;
        lastPress = millis();
    }

    if (hasPress)
    {
        hasPress = false;
        mosquetteer.sendState(BUTTON_ID, eventTypes[0]);
    }

    if (triggerGarage && millis() - lastGarageTrigger > 250)
    {
        triggerGarage = false;
        digitalWrite(GARAGE_TRIGGER_PIN, LOW);
        lastGarageTrigger = millis();
        Serial.println("Garage trigger ended.");
    }

    if (millis() - lastTempRead > 2000)
    {
        float temp = readTemp();
        mosquetteer.sendState(TEMP_ID, temp);
        lastTempRead = millis();
    }
}
