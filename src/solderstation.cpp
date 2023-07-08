#include "Arduino.h"
#include "LiquidCrystal_I2C.h"

#define LCD_ADDR 0x27
#define LCD_COLS 16
#define LCD_LINES 2

#define FAN_CONTROL_PIN 4
#define FAN_SPEED_PIN 3
#define FAN_READING_PIN 2
#define FAN_INTERVAL 15
#define LED_CONTROL_PIN 5
#define LED_INTERVAL 15

#define TEMPERATURE_PIN 10

#define SWITCH_TIMEOUT_MINUTES 2

int ledPercent;
int lastLedPercent;
int speedPercent;
int lastSpeedPercent;
int speedRpm = 0;
int speedCounter = 0;
String inString;
char toPrint[16];

int userTemp = 300;
double realTemp = 300;

unsigned long lastUpdateDisplay;
unsigned long lastUpdatedRPM;

unsigned long lastSwitched = 0;
bool isHeatingOn = false;
bool isAllOn = false;

LiquidCrystal_I2C display(LCD_ADDR, LCD_COLS, LCD_LINES);

void printInfo();
void processCommands();
void processTemp();
double readTemp();
void changeLed(bool increase);
void changeFanSpeed(bool increase);
int getFanSpeed();
void countSpeed();

void setup() {
    pinMode(FAN_CONTROL_PIN, OUTPUT);
    pinMode(FAN_SPEED_PIN, OUTPUT);
    pinMode(LED_CONTROL_PIN, OUTPUT);
    pinMode(TEMPERATURE_PIN, OUTPUT);
    attachInterrupt(digitalPinToInterrupt(FAN_READING_PIN), countSpeed, RISING);

    ledPercent = 0;
    lastLedPercent = -1;
    speedPercent = 0;
    lastSpeedPercent = -1;

    display.begin();
    display.backlight();
    display.clear();

    lastUpdateDisplay = 0;
    lastUpdatedRPM = 0;
    Serial.begin(9600);
}

void loop() {
    processCommands();

    if (speedPercent != lastSpeedPercent && speedPercent <= 0) {
        digitalWrite(FAN_CONTROL_PIN, LOW);
        analogWrite(FAN_SPEED_PIN, 255);
    } else if (speedPercent != lastSpeedPercent) {
        digitalWrite(FAN_CONTROL_PIN, HIGH);
        analogWrite(FAN_SPEED_PIN, map(speedPercent, 0, 100, 0, 255));
    }

    if (ledPercent != lastLedPercent) {
        analogWrite(LED_CONTROL_PIN, map(ledPercent, 0, 100, 0, 255));
    }

    if (millis() - lastUpdateDisplay > 1000) {
        realTemp = readTemp();
        processTemp();
        printInfo();
        lastUpdateDisplay = millis();
    }

    lastSpeedPercent = speedPercent;
    lastLedPercent = ledPercent;
}

void printInfo() {
    snprintf(toPrint, sizeof(toPrint), (String("T: %i") + (char)223 + String("C/%i") + (char)223 + "C").c_str(), userTemp, realTemp);
    display.setCursor(0, 0);
    display.printstr(toPrint);

    snprintf(toPrint, sizeof(toPrint), "V: %i - %iRPM   ", getFanSpeed(), speedRpm);
    display.setCursor(0, 1);
    display.printstr(toPrint);
}

void processCommands() {
    while (Serial.available() > 0) {
        int inChar = Serial.read();
        inString += (char)inChar;

        if (inChar == 10) {
            if (inString.startsWith("v+")) {
                changeFanSpeed(true);
                printInfo();
            } else if (inString.startsWith("v-")) {
                changeFanSpeed(false);
                printInfo();
            }

            inString = "";
        }
    }
}

void processTemp() {
    if (isAllOn) {
        if (isHeatingOn && realTemp >= userTemp && millis()-lastSwitched > (SWITCH_TIMEOUT_MINUTES*1000)) {
            isHeatingOn = false;
            digitalWrite(TEMPERATURE_PIN, LOW);
            lastSwitched = millis();
        }

        if (!isHeatingOn && realTemp < userTemp && millis()-lastSwitched > (SWITCH_TIMEOUT_MINUTES*1000)) {
            isHeatingOn = true;
            digitalWrite(TEMPERATURE_PIN, HIGH);
            lastSwitched = millis();
        }
    }
}

double readTemp() {
    return 300;
}

void changeLed(bool increase) {
    ledPercent = increase ? ledPercent+LED_INTERVAL : speedPercent-LED_INTERVAL;
    ledPercent = ledPercent < 0 ? 0 : (ledPercent > 100 ? 100 : ledPercent);
}

void changeFanSpeed(bool increase) {
    speedPercent = increase ? speedPercent+FAN_INTERVAL : speedPercent-FAN_INTERVAL;
    speedPercent = speedPercent < 0 ? 0 : (speedPercent > 100 ? 100 : speedPercent);
}

int getFanSpeed() {
    return ceil(speedPercent/FAN_INTERVAL);
}

void countSpeed() {
    speedCounter++;

    if (millis() - lastUpdatedRPM > 1000) {
        speedRpm = speedCounter * 60 / 2;
        speedCounter = 0;
        lastUpdatedRPM = millis();
    }
}