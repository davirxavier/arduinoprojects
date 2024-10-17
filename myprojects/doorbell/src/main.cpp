#include <Arduino.h>
#include "LittleFS.h"
#include "WiFiManager.h"
#include "FastBot.h"
#include "logger.h"
#include <TZ.h>
#include "esp-config-page.h"

#define WIRELESS_DOORBELL_PIN 14
#define BUTTON_PIN 13

#define AP_NAME "ESP-DOORBELL-0842"
#define AP_PASSWORD "admin13246"
#define NOTIF_TEXT "A campainha está tocando."

#define ENABLE_SERIAL_LOGGING
#define LOGS_FILE "logs.txt"
#define MAX_LOG_SIZE_BYTES 900000
#define SERIAL_LOG_LN(str) dxlogger::log(str, true)
#define SERIAL_LOG(str) dxlogger::log(str, false)

String turnOnCommand = "@notificatronbot ligar";
String turnOffCommand = "@notificatronbot desligar";
String logsCommand = "@notificatronbot logs";
String resetCommand = "@notificatronbot resetar";
String cleanLogsCommand = "@notificatronbot limpar";
String ringCommand = "@notificatronbot tocar";

auto *userVar = new ESP_CONFIG_PAGE::EnvVar{"ADMIN_USERNAME", ""};
auto *passVar = new ESP_CONFIG_PAGE::EnvVar{"ADMIN_PASSWORD", ""};
auto *bellUser = new ESP_CONFIG_PAGE::EnvVar{"BELL_USER", ""};
auto *bellPass = new ESP_CONFIG_PAGE::EnvVar{"BELL_PASS", ""};
auto *bellHost = new ESP_CONFIG_PAGE::EnvVar{"BELL_IP", ""};
auto *telegramApiKeyVar = new ESP_CONFIG_PAGE::EnvVar{"TELEGRAM_API_KEY", ""};
auto *telegramChatIdVar = new ESP_CONFIG_PAGE::EnvVar{"TELEGRAM_CHAT_ID", ""};

WiFiManager manager;
FastBot bot;
ESP8266WebServer server(80);
WiFiClient wifiClient;
HTTPClient http;

bool hasSignal;
unsigned long lastSentMessage;
unsigned long lastRang;
boolean isOn;

void(* resetFunc) (void) = 0;

void IRAM_ATTR resetConfig() {
    SERIAL_LOG_LN("Resetting the wifi configuration.");
    manager.resetSettings();
    ESP.eraseConfig();
    delay(200);
    resetFunc();
}

void callHttpRing() {
    SERIAL_LOG_LN("Sending doorbell wireless signal.");

    http.begin(wifiClient, bellHost->value);
    int response = http.POST(bellUser->value + ";" + bellPass->value);
    http.end();

    SERIAL_LOG_LN("Signal response: " + String(response));
}

void IRAM_ATTR button_interrupt()
{
//    SERIAL_LOG_LN("Interrupt activated.");

    if (!isOn) {
        return;
    }

    hasSignal = true;
}

void onMaxLogsFile(File file) {
//    bot.sendMessage("Tamanho de arquivo de logs excedido, enviando os logs atuais.", telegramChatIdVar->value);
//    bot.sendFile(file, FB_DOC, "logs.txt", telegramChatIdVar->value);
//
//    LittleFS.remove(file.fullName());
}

void handleMessage(FB_msg& msg) {
    SERIAL_LOG("Received message from chat id ");
    SERIAL_LOG(msg.chatID);
    SERIAL_LOG_LN(" - " + msg.text);

    if (msg.chatID != telegramChatIdVar->value) {
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
        bot.sendFile(file, FB_DOC, "logs.txt", telegramChatIdVar->value);
        file.close();
    } else if (msg.text.equals(resetCommand)) {
        bot.replyMessage(F("Resetando configurações do sistema, será necessário reconfigurá-lo."), msg.messageID, msg.chatID);
        resetConfig();
    } else if (msg.text.equals(cleanLogsCommand)) {
        if (!LittleFS.exists(LOGS_FILE)) {
            return;
        }

        LittleFS.remove(LOGS_FILE);
        bot.replyMessage(F("Arquivo de logs limpo."), msg.messageID, msg.chatID);
    }
//    else if (msg.text.equals(ringCommand)) {
//        callHttpRing();
//    }
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

    lastSentMessage = 0;
    lastRang = 0;
    hasSignal = false;
    isOn = true;
    turnOnCommand.toUpperCase();
    turnOffCommand.toUpperCase();
    logsCommand.toUpperCase();
    resetCommand.toUpperCase();
    cleanLogsCommand.toUpperCase();
    ringCommand.toUpperCase();
    LittleFS.begin();

    WiFi.setSleepMode(WIFI_NONE_SLEEP);
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

    delay(200);

    ESP_CONFIG_PAGE::addEnvVar(userVar);
    ESP_CONFIG_PAGE::addEnvVar(passVar);
    ESP_CONFIG_PAGE::addEnvVar(bellUser);
    ESP_CONFIG_PAGE::addEnvVar(bellPass);
    ESP_CONFIG_PAGE::addEnvVar(bellHost);
    ESP_CONFIG_PAGE::addEnvVar(telegramApiKeyVar);
    ESP_CONFIG_PAGE::addEnvVar(telegramChatIdVar);

    auto *storage = new ESP_CONFIG_PAGE::LittleFSEnvVarStorage("env.txt");
    ESP_CONFIG_PAGE::setAndUpdateEnvVarStorage(storage);

    ESP_CONFIG_PAGE::setup(server, userVar->value, passVar->value, "ESP-DOORBELL-MASTER");

    ESP_CONFIG_PAGE::addCustomAction("Reset wireless settings", [](ESP8266WebServer &server) {
        server.send(200);
        resetConfig();
    });
    ESP_CONFIG_PAGE::addCustomAction("Ring test", [](ESP8266WebServer &server) {
        hasSignal = true;
        server.send(200);
    });

    server.begin();

    bot.skipUpdates();
    bot.setToken(telegramApiKeyVar->value);
    bot.attach(handleMessage);
    bot.sendMessage("Campainha online.", telegramChatIdVar->value);

    SERIAL_LOG_LN("Startup");
}

void loop() {
    dxlogger::update();
    bot.tick();
    server.handleClient();

    if (isOn && digitalRead(BUTTON_PIN) == LOW) {
        hasSignal = true;
    }

    if (isOn && hasSignal && millis() - lastRang > 250) {
        if (millis() - lastSentMessage > 3000) {
            SERIAL_LOG_LN("Sending message.");
            uint8_t res = bot.sendMessage(NOTIF_TEXT, telegramChatIdVar->value);
            SERIAL_LOG("Message sent: ");
            SERIAL_LOG_LN(String(res));

            lastSentMessage = millis();
        }

        callHttpRing();

        lastRang = millis();
        hasSignal = false;
    }
}