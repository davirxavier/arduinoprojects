#include "Arduino.h"
#include "WiFiEsp.h"
#include "SoftwareSerial.h"
#include "fauxmoESP.h"

#define WIFI_RX 3
#define WIFI_TX 2
#define WIFI_SSID "d-6969"
#define WIFI_PASS "drx13246"
#define SERVER_PORT 80
#define SERVER_IP ""

SoftwareSerial wifiSerial(WIFI_RX, WIFI_TX);
RingBuffer buf(8);
int statusLed = LOW;
int status = WL_IDLE_STATUS;
fauxmoESP fmo;

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, statusLed);

    Serial.begin(9600);
    wifiSerial.begin(9600);

    IPAddress ip;
    ip.fromString(SERVER_IP);

    WiFi.init(&wifiSerial);
    if (strlen_P(SERVER_IP) > 0) {
        WiFi.config(ip);
    }

    if(WiFi.status() == WL_NO_SHIELD){
        while (true);
    }
    while(status != WL_CONNECTED){
        status = WiFi.begin(WIFI_SSID, WIFI_PASS);
    }

    fmo.addDevice("led");

    fmo.setPort(SERVER_PORT);
    fmo.enable(true);

    fmo.onSetState([](unsigned char device_id, const char * device_name, bool state, unsigned char value) {
        Serial.begin(9600);
    });
}

void loop() {

}