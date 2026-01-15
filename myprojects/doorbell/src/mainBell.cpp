#include <Arduino.h>
#include <WiFi.h>

#define ESP_CONFIG_PAGE_ENABLE_LOGGING
#include <esp-config-page.h>

#define RING_PIN 7

volatile bool shouldRing = false;
bool ringing = false;
unsigned long ringTimer = 0;

WebServer httpServer(80);
WebSocketsServer socketServer(3333);

auto usernameVar = new ESP_CONFIG_PAGE::EnvVar("USERNAME", "");
auto passwordVar = new ESP_CONFIG_PAGE::EnvVar("PASSWORD", "");

bool useBot = false;

void setup() {
    Serial.begin(115200);
    pinMode(RING_PIN, OUTPUT);
    delay(1000);

    ESP_CONFIG_PAGE::setAPConfig("ESP-BELLBELL1", "admin13246");
    ESP_CONFIG_PAGE::tryConnectWifi(false, 15000);

    ESP_CONFIG_PAGE::addEnvVar(usernameVar);
    ESP_CONFIG_PAGE::addEnvVar(passwordVar);
    ESP_CONFIG_PAGE::setAndUpdateEnvVarStorage(new ESP_CONFIG_PAGE::LittleFSKeyValueStorage("/envfinal.txt"));

    ESP_CONFIG_PAGE::addCustomAction("RING", [](ESP_CONFIG_PAGE::WEBSERVER_T& server)
    {
        shouldRing = true;
    });

    ESP_CONFIG_PAGE::initModules(&httpServer, usernameVar->value, passwordVar->value, "ESP-BELLBELL1");

    httpServer.on("/ring", []()
    {
        const char *body = httpServer.arg("plain").c_str();
        char buf[strlen(usernameVar->value)+strlen(passwordVar->value) + 4];
        sprintf(buf, "%s:%s", usernameVar->value, passwordVar->value);

        if (strcmp(buf, body) == 0)
        {
            shouldRing = true;
            httpServer.send(200);
        }
        else
        {
            httpServer.send(401);
        }
    });

    httpServer.begin();

    socketServer.onEvent([](uint8_t client, WStype_t type, uint8_t* payload, size_t length)
    {
        if (type == WStype_TEXT)
        {
            char* text = reinterpret_cast<char*>(payload);

            size_t textLength = strlen(text);
            if (textLength > 999)
            {
                return;
            }

            char buf[strlen(usernameVar->value) + strlen(passwordVar->value) + 8];
            buf[0] = 0;
            sprintf(buf, "r:%s:%s", usernameVar->value, passwordVar->value);

            if (strcmp(buf, text) == 0)
            {
                shouldRing = true;
            }
        }
    });
    socketServer.begin();
}

void loop() {
    ESP_CONFIG_PAGE::loop();
    httpServer.handleClient();
    socketServer.loop();

    if (shouldRing)
    {
        shouldRing = false;
        ringing = true;
        ringTimer = millis();
        digitalWrite(RING_PIN, HIGH);
    }

    if (ringing && millis() - ringTimer > 2000)
    {
        digitalWrite(RING_PIN, LOW);
        ringing = false;
    }
}