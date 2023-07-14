#include "Arduino.h"
#include "LiquidCrystal_I2C.h"
#include "EEPROMex.h"

#define LCD_ADDR 0x27
#define LCD_COLS 16
#define LCD_LINES 2

#define FAN_CONTROL_PIN 4
#define FAN_SPEED_PIN 9
#define FAN_INTERVAL 20
#define LED_CONTROL_PIN 5
#define LED_INTERVAL 20

#define TEMPERATURE_PIN 10
#define MAX_TEMP 500

#define SWITCH_TIMEOUT_MINUTES 2

struct Config {
    uint8_t speed : 3;
    uint8_t led : 3;
    uint16_t temperature : 9;
};

//configure Timer 1 (pins 9,10) to output 25kHz PWM
void setupTimer9and10(){
    //Set PWM frequency to about 25khz on pins 9,10 (timer 1 mode 10, no prescale, count to 320)
    TCCR1A = (1 << COM1A1) | (1 << COM1B1) | (1 << WGM11);
    TCCR1B = (1 << CS10) | (1 << WGM13);
    ICR1 = 320;
    OCR1A = 0;
    OCR1B = 0;
}

//equivalent of analogWrite on pin 9
void setPWM9(float f){
    f=f<0?0:f>1?1:f;
    OCR1A = (uint16_t)(320*f);
}

int ledPercent;
int lastLedPercent;
int speedPercent;
int lastSpeedPercent;
String inString;
char toPrint[17];

unsigned int userTemp = 300;
double realTemp = 300;

unsigned long lastUpdateDisplay = 0;
unsigned long lastSwitched = 0;

bool isHeatingOn = false;
bool isAllOn = false;

LiquidCrystal_I2C display(LCD_ADDR, LCD_COLS, LCD_LINES);

void updateDisplay();
void processCommands();
void processTemp();
double readTemp();
void changeLed(bool increase);
void changeFanSpeed(bool increase);
int getFanSpeed();
void turnOnOff();
void saveConfiguration(uint8_t slot);
void restoreConfiguration(uint8_t slot);

void setup() {
    pinMode(10, OUTPUT);

    pinMode(FAN_CONTROL_PIN, OUTPUT);
    pinMode(FAN_SPEED_PIN, OUTPUT);
    pinMode(LED_CONTROL_PIN, OUTPUT);
    pinMode(TEMPERATURE_PIN, OUTPUT);

    setupTimer9and10();

    ledPercent = 0;
    lastLedPercent = -1;
    speedPercent = 0;
    lastSpeedPercent = -1;

    display.begin();
    display.clear();

    isAllOn = true;
    turnOnOff();

    Serial.begin(9600);
}

void loop() {
    processCommands();

    if (isAllOn) {
        if (speedPercent != lastSpeedPercent && speedPercent <= 0) {
            digitalWrite(FAN_CONTROL_PIN, LOW);
            setPWM9(0);
        } else if (speedPercent != lastSpeedPercent) {
            digitalWrite(FAN_CONTROL_PIN, HIGH);
            setPWM9(speedPercent/100.0f);
        }

        if (ledPercent != lastLedPercent) {
            analogWrite(LED_CONTROL_PIN, map(ledPercent, 0, 100, 0, 255));
        }

        if (millis() - lastUpdateDisplay > 1000) {
            realTemp = readTemp();
            processTemp();
            updateDisplay();
            lastUpdateDisplay = millis();
        }
    }

    lastSpeedPercent = speedPercent;
    lastLedPercent = ledPercent;
}

void turnOnOff() {
    isAllOn = !isAllOn;

    if (isAllOn) {
        display.display();
        display.backlight();

        lastSpeedPercent = -1;
        lastLedPercent = -1;
        updateDisplay();
    } else {
        display.noDisplay();
        display.noBacklight();

        analogWrite(LED_CONTROL_PIN, 0);
        digitalWrite(FAN_CONTROL_PIN, LOW);
        setPWM9(0);
        digitalWrite(TEMPERATURE_PIN, LOW);
    }
}

void updateDisplay() {
    snprintf(toPrint, sizeof(toPrint), (String("T: %i") + (char)223 + String("C/%i") + (char)223 + "C").c_str(), userTemp, lround(realTemp));
    display.setCursor(0, 0);
    display.printstr(toPrint);

    snprintf(toPrint, sizeof(toPrint), "V: %i | LED: 100%%", getFanSpeed());
    display.setCursor(0, 1);
    display.printstr(toPrint);
}

void processCommands() {
    while (Serial.available() > 0) {
        int inChar = Serial.read();
        inString += (char)inChar;

        if (inChar == 10) {
            if (isAllOn && inString.startsWith("v+")) {
                changeFanSpeed(true);
                updateDisplay();
            } else if (isAllOn && inString.startsWith("v-")) {
                changeFanSpeed(false);
                updateDisplay();
            } else if (inString.startsWith("tg")) {
                turnOnOff();
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
    return 300.0;
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

int getLedValue() {
    return ceil(ledPercent/LED_INTERVAL);
}

void saveConfiguration(uint8_t slot) {
    Config cfg{};
    cfg.speed = getFanSpeed();
    cfg.led = getLedValue();
    cfg.temperature = userTemp;

    int address = slot*sizeof(Config);
    EEPROM.updateBlock(address, cfg);
}

void restoreConfiguration(uint8_t slot) {
    int address = slot*sizeof(Config);

    Config cfg{};
    EEPROM.readBlock(address, cfg);

    speedPercent = cfg.speed*FAN_INTERVAL;
    speedPercent = speedPercent >= 100 ? 100 : (speedPercent <= 0 ? 0 : speedPercent);
    ledPercent = cfg.led*LED_INTERVAL;
    ledPercent = ledPercent >= 100 ? 100 : (ledPercent <= 0 ? 0 : ledPercent);
    userTemp = cfg.temperature > MAX_TEMP ? MAX_TEMP : cfg.temperature;
}