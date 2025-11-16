#include <Arduino.h>
#include <buzzer.h>

#define ESP_CONFIG_PAGE_ENABLE_LOGGING
#include <esp-config-page.h>

#define RELAY_PIN 2
#define BUZZER_PIN 3
#define HALL_OUT_PIN 4
#define MIN_ROTATIONS 5
#define PROTECTION_DELAY 5000

volatile unsigned int rotationCount = 0;
unsigned long rotationResetTimer = 0;

unsigned long protectionTimer = 0;
bool hasProtectionTimer = false;
bool protectionActivated = false;

DxBuzzer buzzer(BUZZER_PIN, true);

void IRAM_ATTR hallInterrupt()
{
    rotationCount++;
}

void setup() {
#ifdef ESP_CONFIG_PAGE_ENABLE_LOGGING
    Serial.begin(115200);
#endif

    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, HIGH);
    attachInterrupt(digitalPinToInterrupt(HALL_OUT_PIN), hallInterrupt, FALLING); // TODO check type of interrupt
    buzzer.beep(500);
}

void loop() {
    buzzer.update();

    if (protectionActivated)
    {
        return;
    }

    if (!hasProtectionTimer && rotationCount < MIN_ROTATIONS)
    {
        protectionTimer = millis();
        hasProtectionTimer = true;
    }

    if (hasProtectionTimer && rotationCount >= MIN_ROTATIONS)
    {
        hasProtectionTimer = false;
    }

    if (hasProtectionTimer && millis() - protectionTimer > PROTECTION_DELAY)
    {
        digitalWrite(RELAY_PIN, LOW);
        protectionActivated = true;
        buzzer.beep(200, UINT_MAX, 500);
    }

    if (millis() - rotationResetTimer > 5000)
    {
        LOGF("Rotations per 5s: %d\n", rotationCount);
        rotationCount = 0;
        rotationResetTimer = millis();
    }
}