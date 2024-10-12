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

String chatId = String(TELEGRAM_CHAT_ID);
String apiKey = String(TELEGRAM_API_KEY);

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

    http.begin(wifiClient, BELL_HOSTNAME);
    int response = http.POST(String(BELL_USER) + ";" + String(BELL_PASS));
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
//    bot.sendMessage("Tamanho de arquivo de logs excedido, enviando os logs atuais.", credentials.chatId);
//    bot.sendFile(file, FB_DOC, "logs.txt", credentials.chatId);
}

void handleMessage(FB_msg& msg) {
    SERIAL_LOG("Received message from chat id ");
    SERIAL_LOG(msg.chatID);
    SERIAL_LOG_LN(" - " + msg.text);

    if (msg.chatID != chatId) {
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
        bot.sendFile(file, FB_DOC, "logs.txt", chatId);
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

    ElegantOTA.begin(&server);
    ElegantOTA.setAuth(BELL_USER, BELL_PASS);
    server.begin();

    bot.skipUpdates();
    bot.setToken(apiKey);
    bot.attach(handleMessage);
    bot.sendMessage("Campainha online.", chatId);

    SERIAL_LOG_LN("Startup");
}

void loop() {
    dxlogger::update();
    bot.tick();
    server.handleClient();
    ElegantOTA.loop();

    if (isOn && digitalRead(BUTTON_PIN) == LOW) {
        hasSignal = true;
    }

    if (isOn && hasSignal && millis() - lastRang > 250) {
        if (millis() - lastSentMessage > 3000) {
            SERIAL_LOG_LN("Sending message.");
            uint8_t res = bot.sendMessage(NOTIF_TEXT, chatId);
            SERIAL_LOG("Message sent: ");
            SERIAL_LOG_LN(String(res));

            lastSentMessage = millis();
        }

        callHttpRing();

        lastRang = millis();
        hasSignal = false;
    }
}