#include <Arduino.h>
#include "LittleFS.h"
#include "WiFiManager.h"
#include "WiFiClientSecure.h"
#include "UniversalTelegramBot.h"
#include "ArduinoJson.h"

#define RESET_CONFIG_PIN 12
#define WIRELESS_DOORBELL_PIN 13

#define AP_NAME "ESP-DOORBELL-0842"
#define AP_PASSWORD "admin13246"
#define APIKEY_SIZE 47
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
unsigned long startedTime;

void saveCredentials() {
    File file = LittleFS.open(CREDS_FILE, "w");
    if (!file) {
        Serial.println(F("Failed to create file"));
        return;
    }

    JsonDocument doc;
    doc["apiKey"] = credentials.apiKey;
    doc["chatId"] = credentials.chatId;

    if (serializeJson(doc, file) == 0) {
        Serial.println(F("Failed to write to file"));
    }

    file.close();
}

void recoverCredentials() {
    if (!LittleFS.exists(CREDS_FILE)) {
        Serial.println("Credentials file does not exist.");
        return;
    }

    File file = LittleFS.open(CREDS_FILE, "r");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    if (error)
        Serial.println(F("Failed to read file, using default configuration"));

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

    Serial.print("Connecting");

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
        Serial.println("Saving credentials");
        strncpy(credentials.apiKey, apiKey.getValue(), APIKEY_SIZE);
        strncpy(credentials.chatId, chatId.getValue(), CHAT_ID_SIZE);

        saveCredentials();
    } else {
        Serial.println("Recovering Credentials.");

        recoverCredentials();
    }

    Serial.print("API KEY: ");
    Serial.println(credentials.apiKey);
    Serial.print("CHATID: ");
    Serial.println(credentials.chatId);

    configTime(0, 0, "pool.ntp.org");
    wiFiClientSecure.setTrustAnchors(&cert);
    UniversalTelegramBot bot(credentials.apiKey, wiFiClientSecure);
    bot.sendMessage(credentials.chatId, NOTIF_TEXT);

    unsigned long timeDiff = millis()-startedTime;
    Serial.println("Message sent.");
    if (timeDiff < 500) {
        Serial.println(timeDiff);
        delay(timeDiff);
    }
    digitalWrite(WIRELESS_DOORBELL_PIN, LOW);

    Serial.println("Going to sleep.");
    ESP.deepSleep(0);
}

void loop() {

}