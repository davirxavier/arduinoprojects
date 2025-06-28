#include <Arduino.h>
#include <OneButton.h>
#include <thermistor.h>

#define STATUS_LED_RED 6
#define STATUS_LED_GREEN 13
#define STATUS_LED_BLUE 12
#define MAIN_RELAY_PIN 11
#define FAN_RELAY_PIN 10
#define FIRST_BOOT_VAL 542
#define WELD_BTN 7
#define MODE_BTN 9

#define THERMISTOR_PIN A1
#define THERMISTOR_REF_RES 10000

#define MAX_TEMP 40
#define FAN_TEMP 40

void writeColor(uint8_t red, uint8_t green, uint8_t blue) {
    analogWrite(STATUS_LED_RED, map(red, 0, 255, 255, 0));
    analogWrite(STATUS_LED_GREEN, map(green, 0, 255, 255, 0));
    analogWrite(STATUS_LED_BLUE, map(blue, 0, 255, 255, 0));
}

namespace Status
{
    unsigned long blinkDuration = 500;
    unsigned long blinkTimer = 0;
    bool isBlinking = false;
    bool blinkMarker = true;
    bool stopBlinking = false;

    enum Status
    {
        MODE_VERY_LIGHT,
        MODE_LIGHT,
        MODE_MEDIUM,
        MODE_HARD,
        MODE_VERY_HARD,
        ERROR_OVERHEATING,
        ERROR_RELAY,
    };

    uint8_t colors[][3] = {
        {0, 255, 255},
        {0, 0, 255},
        {0, 255, 0},
        {255, 255, 0},
        {255, 0, 0},
        {255, 0, 0},
        {130, 0, 240}
    };

    Status next(Status status)
    {
        return status == MODE_VERY_HARD ? MODE_VERY_LIGHT : static_cast<Status>(status+1);
    }

    void blink(unsigned long duration = 500)
    {
        isBlinking = true;
        stopBlinking = false;
        blinkTimer = millis();
        blinkDuration = duration;
    }

    void stopBlink()
    {
        stopBlinking = true;
    }
}

uint8_t saveAddr = 0;
Status::Status currentStatus = Status::MODE_VERY_LIGHT;
uint16_t firstBootVal = 0;
OneButton modeButton(MODE_BTN);
OneButton weldButton(WELD_BTN);

unsigned long weldTimes[] = {100, 250, 400, 600, 1200, 0, 0};
unsigned long delayBetweenWelds = 1000;
unsigned long delayTurnOffFan = 15000;//120000; TODO
unsigned long weldTimer = 0;
bool hasWeld = false;

unsigned long lastTempReading = 0;
unsigned long overheatTimer = 0;
Status::Status statusBeforeOverheat = Status::MODE_VERY_LIGHT;

void changeStatus(Status::Status newStatus);

void changeStatus(Status::Status newStatus)
{
    uint8_t* colors = Status::colors[newStatus];
    writeColor(colors[0], colors[1], colors[2]);

    if (newStatus == Status::ERROR_RELAY || newStatus == Status::ERROR_OVERHEATING)
    {
        Status::blink();
    } else
    {
        Status::stopBlink();
    }

    currentStatus = newStatus;
}

void modeChangeEvent()
{
    changeStatus(Status::next(currentStatus));
}

void weld()
{
    if (currentStatus == Status::ERROR_RELAY || currentStatus == Status::ERROR_OVERHEATING)
    {
        return;
    }

    if (!hasWeld && millis() - weldTimer > delayBetweenWelds)
    {
        digitalWrite(FAN_RELAY_PIN, HIGH);
        digitalWrite(MAIN_RELAY_PIN, HIGH);

        hasWeld = true;
        weldTimer = millis();
    }
}

void setup() {
    modeButton.debounce(true);
    modeButton.setDebounceMs(-50);
    modeButton.attachClick(modeChangeEvent);

    weldButton.debounce(true);
    weldButton.setDebounceMs(-50);
    weldButton.attachClick(weld);
    weldButton.attachDuringLongPress(weld);

    pinMode(MAIN_RELAY_PIN, OUTPUT);
    pinMode(FAN_RELAY_PIN, OUTPUT);
    changeStatus(Status::MODE_VERY_LIGHT);
}

void loop() {
    modeButton.tick();
    weldButton.tick();

    if (Status::isBlinking && millis() - Status::blinkTimer > Status::blinkDuration)
    {
        uint8_t *colors = Status::colors[currentStatus];
        writeColor(Status::blinkMarker ? colors[0] : 0, Status::blinkMarker ? colors[1] : 0, Status::blinkMarker ? colors[2] : 0);

        if (Status::blinkMarker && Status::stopBlinking)
        {
            Status::isBlinking = false;
        }

        Status::blinkTimer = millis();
        Status::blinkMarker = !Status::blinkMarker;
    }

    if (millis() - lastTempReading > 100)
    {
        double temp = readTemp(THERMISTOR_REF_RES, THERMISTOR_PIN);

        if (currentStatus != Status::ERROR_OVERHEATING &&
            currentStatus != Status::ERROR_RELAY &&
            temp > MAX_TEMP)
        {
            statusBeforeOverheat = currentStatus;
            changeStatus(Status::ERROR_OVERHEATING);

            digitalWrite(FAN_RELAY_PIN, HIGH);
            digitalWrite(MAIN_RELAY_PIN, LOW);

            hasWeld = false;
            overheatTimer = millis();
        }

        if (currentStatus == Status::ERROR_OVERHEATING &&
            temp < MAX_TEMP - (MAX_TEMP * 0.15) &&
            millis() - overheatTimer > 30000)
        {
            weldTimer = millis();
            changeStatus(statusBeforeOverheat);
            overheatTimer = millis();
        }

        if (temp > FAN_TEMP && millis() - weldTimer > 1000)
        {
            digitalWrite(FAN_RELAY_PIN, HIGH);
            weldTimer = millis();
        }

        if (millis() - weldTimer > delayTurnOffFan &&
            temp < FAN_TEMP &&
            millis() - overheatTimer > 30000)
        {
            digitalWrite(FAN_RELAY_PIN, LOW);
            weldTimer = millis();
        }

        lastTempReading = millis();
    }

    if (currentStatus == Status::ERROR_OVERHEATING || currentStatus == Status::ERROR_RELAY)
    {
        return;
    }

    if (hasWeld && millis() - weldTimer > weldTimes[currentStatus])
    {
        digitalWrite(MAIN_RELAY_PIN, LOW);
        hasWeld = false;
        weldTimer = millis();
    }
}