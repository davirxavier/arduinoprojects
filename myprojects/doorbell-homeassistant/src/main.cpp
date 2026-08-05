#include <Arduino.h>

#define ESP_CONFIG_PAGE_ENABLE_LOGGING
#define MQTTO_ENABLE_LOGGING
#include <Mosquetteer.h>
#include <esp-config-page.h>
#include <secrets.h>

#define BUTTON_ID "doorbell_events"
#define ENABLED_ID "doorbell_enabled"
#define BUTTON_PIN 1

ESP_CONFIG_PAGE::WEBSERVER_T httpServer(80);
Mosquetteer mosquetteer;
bool mqttBegun = false;

bool hasPress = false;
unsigned long lastPress = 0;

const char *eventTypes[] = {
    "button_pressed"
};
size_t typesSize = 1;

void setup()
{
    delay(1500);
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

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
}
