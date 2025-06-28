#include <Arduino.h>
#include <FastBot.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <esp-config-page.h>
#include <WebSocketsClient.h>

#define BUTTON_PIN 13

volatile bool shouldRing = false;
bool ringing = false;
unsigned long ringTimer = 0;
bool isOn = true;
unsigned long lastRing = 0;
unsigned long lastMessageSent = 0;

ESP8266WebServer httpServer(80);
FastBot bot;

auto usernameVar = new ESP_CONFIG_PAGE::EnvVar("USERNAME", "");
auto passwordVar = new ESP_CONFIG_PAGE::EnvVar("PASSWORD", "");
auto chatIdVar = new ESP_CONFIG_PAGE::EnvVar("CHAT_ID", "");
auto tokenVar = new ESP_CONFIG_PAGE::EnvVar("TOKEN", "");
auto bellIpVar = new ESP_CONFIG_PAGE::EnvVar("BELL_IP", "");

bool botStarted = false;

WebSocketsClient webSocket;

namespace Commands
{
    enum Commands
    {
        TURN_ON,
        TURN_OFF,
        RING,
        NONE,
    };

    inline uint8_t commandCount = 4;

    inline char commandTextById[4][32] = {
        "ligar",
        "desligar",
        "tocar",
        "",
    };

    inline Commands parseCommand(String &str)
    {
        int firstSpace = str.indexOf(' ');
        if (firstSpace < 0 || firstSpace+2 > str.length())
        {
            return NONE;
        }

        str.toLowerCase();

        for (size_t i = 0; i < commandCount-1; i++)
        {
            if (strcmp(str.c_str()+firstSpace+1, commandTextById[i]) == 0)
            {
                return static_cast<Commands>(i);
            }
        }

        return NONE;
    }
}

void handleMessage(FB_msg &msg)
{
    Commands::Commands command = Commands::parseCommand(msg.text);

    switch (command)
    {
        case (Commands::TURN_ON):
            {
                isOn = true;
                bot.replyMessage("Campainha ligada.", msg.messageID, chatIdVar->value);
                break;
            }
        case (Commands::TURN_OFF):
            {
                isOn = false;
                bot.replyMessage("Campainha desligada.", msg.messageID, chatIdVar->value);
                break;
            }
        case (Commands::RING):
            {
                shouldRing = true;
                break;
            }
    default: break;
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);

    ESP_CONFIG_PAGE::setAPConfig("ESP-BELLBTN1", "");
    ESP_CONFIG_PAGE::tryConnectWifi(false, 20000);

    ESP_CONFIG_PAGE::addEnvVar(usernameVar);
    ESP_CONFIG_PAGE::addEnvVar(passwordVar);
    ESP_CONFIG_PAGE::addEnvVar(chatIdVar);
    ESP_CONFIG_PAGE::addEnvVar(tokenVar);
    ESP_CONFIG_PAGE::addEnvVar(bellIpVar);
    ESP_CONFIG_PAGE::setAndUpdateEnvVarStorage(new ESP_CONFIG_PAGE::LittleFSKeyValueStorage("/envfinal.txt"));

    ESP_CONFIG_PAGE::initModules(&httpServer, usernameVar->value, passwordVar->value, "ESP-BELLBTN1");
    httpServer.begin();

    if (bellIpVar->value != nullptr && strlen(bellIpVar->value) > 0)
    {
        webSocket.begin(bellIpVar->value, 3333);
        webSocket.setReconnectInterval(1000);
        webSocket.enableHeartbeat(5000, 15000, 30000);
    }
}

void loop() {
    if (!botStarted && ESP_CONFIG_PAGE::isWiFiReady())
    {
        bot.skipUpdates();
        bot.setToken(tokenVar->value);
        bot.attach(handleMessage);
        bot.sendMessage("A campainha está online.", chatIdVar->value);
        botStarted = true;
    }

    if (botStarted)
    {
        bot.tick();
    }

    ESP_CONFIG_PAGE::loop();
    httpServer.handleClient();
    webSocket.loop();

    if (!isOn)
    {
        shouldRing = false;
    }

    if (!ringing && shouldRing && isOn && millis() - lastRing > 1500)
    {
        ringing = true;

        if (webSocket.isConnected())
        {
            char buf[strlen(usernameVar->value)+strlen(passwordVar->value)+8];
            buf[0] = 0;
            sprintf(buf, "r:%s:%s", usernameVar->value, passwordVar->value);
            webSocket.sendTXT(buf);
        }
        else
        {
            Serial.println("Websocket is not connected.");
        }

        if (millis() - lastMessageSent > 3000)
        {
            bot.sendMessage("A campainha está tocando.", chatIdVar->value);
            lastMessageSent = millis();
        }

        shouldRing = false;
        ringTimer = millis();
        lastRing = millis();
    }

    if (ringing && millis() - ringTimer > 2000)
    {
        ringing = false;
    }

    if (digitalRead(BUTTON_PIN) == LOW)
    {
        shouldRing = true;
    }
}