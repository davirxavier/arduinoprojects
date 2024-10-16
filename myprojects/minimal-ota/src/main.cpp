#include <Arduino.h>
#include "esp-config-page.h"

ESP8266WebServer server(80);

void setup(void) {
    Serial.begin(115200);

    Serial.println("Connecting");
    WiFi.begin("", "");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(100);
        Serial.print(".");
    }
    Serial.println();
    Serial.println("Connected!");

    auto *userVar = new ESP_CONFIG_PAGE::EnvVar{"ADMIN_USERNAME", ""};
    auto *passVar = new ESP_CONFIG_PAGE::EnvVar{"ADMIN_PASSWORD", ""};
    auto *bellUser = new ESP_CONFIG_PAGE::EnvVar{"BELL_USER", ""};
    auto *bellPass = new ESP_CONFIG_PAGE::EnvVar{"BELL_PASS", ""};
    auto *bellHost = new ESP_CONFIG_PAGE::EnvVar{"BELL_IP", ""};
    auto *telegramApiKeyVar = new ESP_CONFIG_PAGE::EnvVar{"TELEGRAM_API_KEY", ""};
    auto *telegramChatIdVar = new ESP_CONFIG_PAGE::EnvVar{"TELEGRAM_CHAT_ID", ""};

    ESP_CONFIG_PAGE::addEnvVar(userVar);
    ESP_CONFIG_PAGE::addEnvVar(passVar);
    ESP_CONFIG_PAGE::addEnvVar(bellUser);
    ESP_CONFIG_PAGE::addEnvVar(bellPass);
    ESP_CONFIG_PAGE::addEnvVar(bellHost);
    ESP_CONFIG_PAGE::addEnvVar(telegramApiKeyVar);
    ESP_CONFIG_PAGE::addEnvVar(telegramChatIdVar);

    auto *storage = new ESP_CONFIG_PAGE::LittleFSEnvVarStorage("env");
    ESP_CONFIG_PAGE::setAndUpdateEnvVarStorage(storage);

    ESP_CONFIG_PAGE::setup(server, userVar->value, passVar->value, "ESP-DOORBELL-MASTER");
    ESP_CONFIG_PAGE::addCustomAction("Test", [](ESP8266WebServer &server) {
        server.send(200);
    });

    server.begin();
}

void loop(void) {
    server.handleClient();
}