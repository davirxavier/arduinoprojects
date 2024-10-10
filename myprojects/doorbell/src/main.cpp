#include <Arduino.h>
#include "LittleFS.h"
#include "WiFiManager.h"
#include "FastBot.h"
#include "ArduinoJson.h"
#include "logger.h"
#include <TZ.h>
#include "ElegantOTA.h"
#include "secrets.h"

#define WIRELESS_DOORBELL_PIN 14
#define BUTTON_PIN 13

#define AP_NAME "ESP-DOORBELL-0842"
#define AP_PASSWORD "admin13246"
#define APIKEY_SIZE 47
#define CHAT_ID_SIZE 20
#define CREDS_FILE "credentials.txt"
#define NOTIF_TEXT "A campainha está tocando."

#define ENABLE_SERIAL_LOGGING
#define LOGS_FILE "logs.txt"
#define MAX_LOG_SIZE_BYTES 900000
#define SERIAL_LOG_LN(str) dxlogger::log(str, true)
#define SERIAL_LOG(str) dxlogger::log(str, false)

bool shouldSaveCredentials;

struct Credentials {
    char apiKey[APIKEY_SIZE];
    char chatId[CHAT_ID_SIZE];
} __attribute__((packed));

String turnOnCommand = "@notificatronbot ligar";
String turnOffCommand = "@notificatronbot desligar";
String logsCommand = "@notificatronbot logs";
String resetCommand = "@notificatronbot resetar";

Credentials credentials{};
WiFiManager manager;
FastBot bot;
ESP8266WebServer server(80);

volatile unsigned long lastSignal;
volatile bool hasSignal;
unsigned long lastActivated;
boolean isOn;

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

void IRAM_ATTR button_interrupt()
{
//    SERIAL_LOG_LN("Interrupt activated.");

    if (!isOn) {
        return;
    }

    if (millis() - lastSignal > 1000) {
        SERIAL_LOG_LN("Sending doorbell wireless signal.");

        digitalWrite(WIRELESS_DOORBELL_PIN, HIGH);
        delay(50);
        digitalWrite(WIRELESS_DOORBELL_PIN, LOW);

        hasSignal = true;
    } else {
//        SERIAL_LOG_LN("Delay too short, ignoring.");
    }

    lastSignal = millis();
}

void onMaxLogsFile(File file) {
//    bot.sendMessage("Tamanho de arquivo de logs excedido, enviando os logs atuais.", credentials.chatId);
//    bot.sendFile(file, FB_DOC, "logs.txt", credentials.chatId);
}

void handleMessage(FB_msg& msg) {
    SERIAL_LOG("Received message from chat id ");
    SERIAL_LOG(msg.chatID);
    SERIAL_LOG_LN(" - " + msg.text);

    if (msg.chatID != credentials.chatId) {
        return;
    }

    msg.text.toUpperCase();

    if (msg.text.equals(turnOnCommand)) {
        isOn = true;
        SERIAL_LOG_LN("Turn on");
        bot.replyMessage(F("Campainha ligada."), msg.messageID, msg.chatID);
    } else if (msg.text.equals(turnOffCommand)) {
        isOn = false;
        SERIAL_LOG_LN("Turn off");
        bot.replyMessage(F("Campainha desligada."), msg.messageID, msg.chatID);
    } else if (msg.text.equals(logsCommand)) {
        File file = LittleFS.open(LOGS_FILE, "r");
        bot.sendFile(file, FB_DOC, "logs.txt", credentials.chatId);
        file.close();
    } else if (msg.text.equals(resetCommand)) {
        bot.replyMessage(F("Resetando configurações do sistema, será necessário reconfigurá-lo."), msg.messageID, msg.chatID);
        resetConfig();
    }
}

void setup() {
#ifdef ENABLE_SERIAL_LOGGING
    Serial.begin(9600);
    bool enableSerial = true;
#else
    bool enableSerial = false;
#endif

    pinMode(WIRELESS_DOORBELL_PIN, OUTPUT);
    digitalWrite(WIRELESS_DOORBELL_PIN, LOW);

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), button_interrupt, FALLING);

    lastSignal = 0;
    lastActivated = 0;
    hasSignal = false;
    isOn = true;
    turnOnCommand.toUpperCase();
    turnOffCommand.toUpperCase();
    logsCommand.toUpperCase();
    resetCommand.toUpperCase();
    LittleFS.begin();

    WiFi.setSleepMode(WIFI_NONE_SLEEP);

    WiFiManagerParameter apiKey("apiKey", "Telegram api key", NULL, APIKEY_SIZE);
    WiFiManagerParameter chatId("chatId", "Telegram chat id", NULL, 100);

    manager.addParameter(&apiKey);
    manager.addParameter(&chatId);
    manager.setSaveConfigCallback([]() {
        shouldSaveCredentials = true;
    });
    manager.autoConnect(AP_NAME, AP_PASSWORD);

#ifdef ENABLE_SERIAL_LOGGING
    manager.setDebugOutput(true);
#endif

    dxlogger::setup(TZ_America_Fortaleza,
                    30000,
                    enableSerial,
                    LOGS_FILE,
                    MAX_LOG_SIZE_BYTES,
                    onMaxLogsFile);

    if (shouldSaveCredentials) {
        SERIAL_LOG_LN("Saving credentials");
        strncpy(credentials.apiKey, apiKey.getValue(), APIKEY_SIZE);
        strncpy(credentials.chatId, chatId.getValue(), CHAT_ID_SIZE);

        saveCredentials();
    } else {
        SERIAL_LOG_LN("Recovering Credentials.");
        recoverCredentials();
    }

//    SERIAL_LOG("API KEY: ");
//    SERIAL_LOG_LN(credentials.apiKey);
//    SERIAL_LOG("CHATID: ");
//    SERIAL_LOG_LN(credentials.chatId);

    ElegantOTA.begin(&server);
    ElegantOTA.setAuth(UPDATE_USER, UPDATE_PASS);
    server.begin();

    bot.skipUpdates();
    bot.setToken(credentials.apiKey);
    bot.attach(handleMessage);
    bot.sendMessage("Campainha online.", credentials.chatId);

    SERIAL_LOG_LN("Startup");
}

void loop() {
    dxlogger::update();
    bot.tick();

    if (isOn && hasSignal && millis() - lastActivated > 5000) {
        SERIAL_LOG_LN("Sending message.");
        uint8_t res = bot.sendMessage(NOTIF_TEXT, credentials.chatId);
        SERIAL_LOG("Message sent: ");
        SERIAL_LOG_LN(String(res));

        lastActivated = millis();
        hasSignal = false;
    }
}