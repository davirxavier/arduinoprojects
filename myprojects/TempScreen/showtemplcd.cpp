#include <Arduino.h>
#include "LiquidCrystal_I2C.h"
#include "dht.h"

dht sensor;
LiquidCrystal_I2C lcd(0x27, 16, 2);
String str;

unsigned long oldTime;
unsigned long oldTime2;

byte atilde[8] = {
        0b01010,
        0b10100,
        0b01110,
        0b00001,
        0b01111,
        0b10001,
        0b01111,
        0b00000
};

void update() {
    sensor.read11(8);

    lcd.setCursor(8, 1);
    lcd.print(String(sensor.temperature, 0) + (char)223 + "C");
    lcd.setCursor(12, 1);
    lcd.print("/" + String(sensor.humidity, 0) + "%");
}

void setup() {
    lcd.begin();
    lcd.createChar(1, atilde);
    lcd.backlight();
    lcd.clear();

    delay(2000);
    str = "Daviz\001o            ";
    lcd.print(str);
    lcd.setCursor(0, 1);
    lcd.print("Tmp/Umd 00C/0%");

    oldTime = millis();
    oldTime2 = millis();

    Serial.begin(9600);
}

void loop() {
    unsigned long time = millis();

    if (time - oldTime2 > 300) {
        lcd.setCursor(0, 0);

        str = str.substring(1, str.length()) + str.charAt(0);
        lcd.print(str);

        oldTime2 = millis();
    }

    if (time - oldTime > 1100) {
        update();
        oldTime = millis();
    }
}