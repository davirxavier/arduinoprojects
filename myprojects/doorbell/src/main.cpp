#include <Arduino.h>
#include "LittleFS.h"
#include "WiFiManager.h"
#include "WiFiClientSecure.h"
#include "UniversalTelegramBot.h"

#define BUTTON_PIN 3
#define RESET_CONFIG_PIN 2

#define AP_NAME "ESP-DOORBELL-0842"
#define AP_PASSWORD "admin13246"
#define APIKEY_SIZE 46
#define CHAT_ID_SIZE 20
#define CREDS_FILE "credentials.txt"
#define NOTIF_TEXT "A campainha está tocando."

bool shouldSaveCredentials;

struct Credentials {
    char apiKey[APIKEY_SIZE];
    char chatId[CHAT_ID_SIZE];
};

Credentials credentials{};
X509List cert(TELEGRAM_CERTIFICATE_ROOT);
WiFiClientSecure wiFiClientSecure;
WiFiManager manager;

void saveCredentials() {
    char bytes[sizeof(Credentials)];
    memcpy(bytes, (const char*)&credentials, sizeof(Credentials));

    File file = LittleFS.open(CREDS_FILE, "w");
    file.write(bytes, sizeof(Credentials));
    file.close();
}

void recoverCredentials() {
    if (!LittleFS.exists(CREDS_FILE)) {
        Serial.println("Credentials file does not exist.");
        return;
    }

    File file = LittleFS.open(CREDS_FILE, "r");
    Serial.println(file.readString());
    char bytes[file.size()];
    file.readBytes(bytes, file.size());
    memcpy((char*) &credentials, bytes, sizeof(Credentials));
    file.close();
}

void(* resetFunc) (void) = 0;

void IRAM_ATTR resetConfig() {
    Serial.println("Resetting the wifi configuration.");
    manager.resetSettings();
    ESP.eraseConfig();

    if (LittleFS.exists(CREDS_FILE)) {
        LittleFS.remove(CREDS_FILE);
    }

    delay(200);
    resetFunc();
}

void setup() {
    Serial.begin(9600);

    pinMode(RESET_CONFIG_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(RESET_CONFIG_PIN), resetConfig, FALLING);

    Serial.print("Connecting");

    WiFi.setSleepMode(WIFI_NONE_SLEEP);

    WiFiManagerParameter apiKey("apiKey", "Telegram api key", NULL, APIKEY_SIZE);
    WiFiManagerParameter chatId("chatId", "Telegram chat id", NULL, CHAT_ID_SIZE);

    manager.addParameter(&apiKey);
    manager.addParameter(&chatId);
    manager.setSaveConfigCallback([]() {
        shouldSaveCredentials = true;
    });
    manager.setDebugOutput(true);
    manager.autoConnect(AP_NAME, AP_PASSWORD);

    if (shouldSaveCredentials) {
        Serial.println("Saving credentials");
        strncpy(credentials.apiKey, apiKey.getValue(), APIKEY_SIZE);
        strncpy(credentials.chatId, chatId.getValue(), CHAT_ID_SIZE);

        saveCredentials();
    } else {
        Serial.println("Recovering Credentials.");

        recoverCredentials();
        Serial.print("API KEY: ");
        Serial.println(credentials.apiKey);
    }

    wiFiClientSecure.setTrustAnchors(&cert);
    UniversalTelegramBot bot(credentials.apiKey, wiFiClientSecure);
    bot.sendMessage(credentials.chatId, F(NOTIF_TEXT));
}

void loop() {

}