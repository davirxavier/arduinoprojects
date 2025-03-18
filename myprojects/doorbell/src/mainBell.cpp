#include <Arduino.h>
#include <FastBot.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <espnow.h>
#include <secrets.h>
#include <shared.h>

volatile bool shouldRing = false;
bool ringing = false;
unsigned long ringTimer = 0;
bool isOn = true;

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
FastBot bot;

int32_t getWiFiChannel(const char *ssid) {
    if (int32_t n = WiFi.scanNetworks()) {
        for (uint8_t i=0; i<n; i++) {
            if (!strcmp(ssid, WiFi.SSID(i).c_str())) {
                return WiFi.channel(i);
            }
        }
    }
    return 0;
}

void handleMessage(FB_msg &msg)
{
    Commands::Commands command = Commands::parseCommand(msg.text);

    switch (command)
    {
        case (Commands::TURN_ON):
            {
                isOn = true;
                bot.replyMessage("Campainha ligada.", msg.messageID, CHAT_ID);
                break;
            }
        case (Commands::TURN_OFF):
            {
                isOn = false;
                bot.replyMessage("Campainha desligada.", msg.messageID, CHAT_ID);
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
    pinMode(RING_PIN, OUTPUT);

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
        WiFi.begin(WIFI_SSID, WIFI_PASS);
    }

    WiFi.setSleepMode(WIFI_NONE_SLEEP);

    httpUpdater.setup(&httpServer);
    httpServer.begin();

    uint8_t wifiChannel = getWiFiChannel(WIFI_SSID);
    if (wifiChannel == 0)
    {
        ESP.restart();
        return;
    }

    bot.skipUpdates();
    bot.setToken(TOKEN);
    bot.attach(handleMessage);
    bot.sendMessage("A campainha está online.", CHAT_ID);

    if (esp_now_init() != 0) {
        return;
    }

    esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
    esp_now_add_peer(peerMac, ESP_NOW_ROLE_SLAVE, wifiChannel, NULL, 0);
    esp_now_register_recv_cb([](u8 *mac_addr, u8 *data, u8 len)
    {
        shouldRing = true;
    });
}

void loop() {
    bot.tick();
    httpServer.handleClient();

    if (shouldRing && isOn)
    {
        bot.sendMessage("A campainha está tocando.", CHAT_ID);
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