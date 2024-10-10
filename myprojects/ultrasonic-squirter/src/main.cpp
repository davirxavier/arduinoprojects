#include <Arduino.h>
#include <NewPing.h>

#define TRIGGER_PIN 11
#define ECHO_PIN 12
#define PUMP 10
#define PUMP_PERCENT 0.2
#define PING_DELAY_MS 1000
#define TRIGGER_DISTANCE_CM 40

#define ENABLE_LOGGING

#ifdef ENABLE_LOGGING
#define LOG(v) Serial.print(v)
#define LOG_LN(v) Serial.println(v)
#else
#define LOG(v)
#define LOG_LN(v)
#endif

NewPing sonar(TRIGGER_PIN, ECHO_PIN);
unsigned long lastPing;

void setup() {
#ifdef ENABLE_LOGGING
    Serial.begin(9600);
#endif

    pinMode(PUMP, OUTPUT);
    analogWrite(PUMP, 0);
    LOG_LN("Started");
    lastPing = millis();
}

void loop() {
    if (millis() - lastPing > PING_DELAY_MS) {
        LOG_LN("Pinging...");
        unsigned long ping = NewPingConvert(sonar.ping_median(), US_ROUNDTRIP_CM);
        LOG("Ping: ");
        LOG_LN(ping);

        if (ping < TRIGGER_DISTANCE_CM) {
            LOG_LN("Trigger, squirting...");
            analogWrite(PUMP, (int)(255 * PUMP_PERCENT));
            delay(300);
            LOG_LN("Stop squirt");
            analogWrite(PUMP, 0);
        }

        lastPing = millis();
    }
}