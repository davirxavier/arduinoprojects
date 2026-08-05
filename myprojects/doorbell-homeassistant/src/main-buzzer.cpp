#include <Arduino.h>
#include <secrets.h>

#define ESP_CONFIG_PAGE_ENABLE_LOGGING
#define MQTTO_ENABLE_LOGGING
#include <Mosquetteer.h>
#include <esp-config-page.h>

#define BUTTON_ID "buzz_button"
#define BUZZ_PIN 7

ESP_CONFIG_PAGE::WEBSERVER_T httpServer(80);
Mosquetteer mosquetteer;

bool mqttBegun = false;
bool hasBuzz = false;
unsigned long buzzTimer = 0;

void setup()
{
    Serial.begin(115200);
    delay(2000);
    pinMode(BUZZ_PIN, OUTPUT);

    ESP_CONFIG_PAGE::setAPConfig("ESP32-BELL-BUZ-V2", "adminadmin");
    ESP_CONFIG_PAGE::initModules(&httpServer, CONFIG_PAGE_USER, CONFIG_PAGE_PASS, "ESP32-BELL-BUZ-V2");
    httpServer.begin();

    mosquetteer.setClientConfig(MQTT_URL, MQTT_USER, MQTT_PASS);
    mosquetteer.setDeviceInfo("esp_doorbell_buzzer_v2", "Doorbell Buzzer");
    mosquetteer.setTlsConfig(cacert);

    mosquetteer.define(BUTTON_ID, "Buzz", MosquetteerShared::BUTTON);
    mosquetteer.onCommand(BUTTON_ID, [](const MosquetteerProp &prop)
    {
        Serial.println("Received buz command.");
        if (!hasBuzz) {
            hasBuzz = true;
            buzzTimer = millis();
            digitalWrite(BUZZ_PIN, HIGH);
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

    if (hasBuzz && millis() - buzzTimer > 2000)
    {
        hasBuzz = false;
        digitalWrite(BUZZ_PIN, LOW);
    }
}