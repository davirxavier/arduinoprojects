#include <Arduino.h>
#include "LittleFS.h"
#include "WiFiManager.h"
#include "WiFiClientSecure.h"
#include "FastBot.h"
#include "ArduinoJson.h"
#include "Bounce2.h"

#define RESET_CONFIG_PIN 14
#define WIRELESS_DOORBELL_PIN 12
#define BUTTON_PIN 13

#define AP_NAME "ESP-DOORBELL-0842"
#define AP_PASSWORD "admin13246"
#define APIKEY_SIZE 47
#define CHAT_ID_SIZE 20
#define CREDS_FILE "credentials.txt"
#define NOTIF_TEXT "A campainha está tocando."

//#define ENABLE_LOGGING

#ifdef ENABLE_LOGGING
#define SERIAL_LOG_LN(str) Serial.println(str)
#define SERIAL_LOG(str) Serial.print(str)
#else
#define SERIAL_LOG_LN(str)
#define SERIAL_LOG(str)
#endif

bool shouldSaveCredentials;

struct Credentials {
    char apiKey[APIKEY_SIZE];
    char chatId[CHAT_ID_SIZE];
};

Credentials credentials{};
WiFiClientSecure wiFiClientSecure;
WiFiManager manager;
unsigned long startedTime;
boolean hasTimeout;
Bounce2::Button button = Bounce2::Button();
FastBot* bot;

void saveCredentials() {
    File file = LittleFS.open(CREDS_FILE, "w");
    if (!file) {
        SERIAL_LOG_LN(F("Failed to create file"));
        return;
    }

    JsonDocument doc;
    doc["apiKey"] = credentials.apiKey;
    doc["chatId"] = credentials.chatId;

    if (serializeJson(doc, file) == 0) {
        SERIAL_LOG_LN(F("Failed to write to file"));
    }

    file.close();
}

void recoverCredentials() {
    if (!LittleFS.exists(CREDS_FILE)) {
        SERIAL_LOG_LN("Credentials file does not exist.");
        return;
    }

    File file = LittleFS.open(CREDS_FILE, "r");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    if (error)
        SERIAL_LOG_LN(F("Failed to read file, using default configuration"));

    strlcpy(credentials.apiKey,
            doc["apiKey"] | "",
            sizeof(credentials.apiKey));
    strlcpy(credentials.chatId,
            doc["chatId"] | "",
            sizeof(credentials.chatId));

    file.close();
}

void(* resetFunc) (void) = 0;

void IRAM_ATTR resetConfig() {
    SERIAL_LOG_LN("Resetting the wifi configuration.");
    manager.resetSettings();
    ESP.eraseConfig();

    if (LittleFS.exists(CREDS_FILE)) {
        LittleFS.remove(CREDS_FILE);
    }

    delay(200);
    resetFunc();
}

void setup() {
#ifdef ENABLE_LOGGING
    Serial.begin(9600);
#endif
    startedTime = millis();
    LittleFS.begin();

    pinMode(RESET_CONFIG_PIN, INPUT_PULLUP);
    delay(10);
    if (digitalRead(RESET_CONFIG_PIN) == LOW) {
        resetConfig();
        return;
    }

    pinMode(WIRELESS_DOORBELL_PIN, OUTPUT);
    digitalWrite(WIRELESS_DOORBELL_PIN, HIGH);

    SERIAL_LOG("Connecting");

    WiFi.setSleepMode(WIFI_NONE_SLEEP);

    WiFiManagerParameter apiKey("apiKey", "Telegram api key", NULL, APIKEY_SIZE);
    WiFiManagerParameter chatId("chatId", "Telegram chat id", NULL, 100);

    manager.addParameter(&apiKey);
    manager.addParameter(&chatId);
    manager.setSaveConfigCallback([]() {
        shouldSaveCredentials = true;
    });
    manager.setDebugOutput(true);
    manager.autoConnect(AP_NAME, AP_PASSWORD);

    if (shouldSaveCredentials) {
        SERIAL_LOG_LN("Saving credentials");
        strncpy(credentials.apiKey, apiKey.getValue(), APIKEY_SIZE);
        strncpy(credentials.chatId, chatId.getValue(), CHAT_ID_SIZE);

        saveCredentials();
    } else {
        Serial.println("Recovering Credentials.");
        recoverCredentials();
    }

    SERIAL_LOG("API KEY: ");
    SERIAL_LOG_LN(credentials.apiKey);
    SERIAL_LOG("CHATID: ");
    SERIAL_LOG_LN(credentials.chatId);

    configTime(0, 0, "pool.ntp.org");
    bot = new FastBot(credentials.apiKey);

    button.attach(BUTTON_PIN, INPUT_PULLUP);
    button.interval(10);
    button.setPressedState(LOW);

    hasTimeout = false;
}

void loop() {
    bot->tick();
    button.update();

    if (bot != nullptr && button.pressed()) {
        SERIAL_LOG_LN("Sending message.");
        bot->sendMessage(NOTIF_TEXT, credentials.chatId);
        SERIAL_LOG_LN("Message sent.");

        SERIAL_LOG_LN("Sending signal.");
        digitalWrite(WIRELESS_DOORBELL_PIN, HIGH);
        hasTimeout = true;
        startedTime = millis();
    }

    if(hasTimeout && millis() - startedTime > 800) {
        SERIAL_LOG_LN("Signal turned off.");
        hasTimeout = false;
        digitalWrite(WIRELESS_DOORBELL_PIN, false);
    }
}