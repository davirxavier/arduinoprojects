#include <Arduino.h>
#include <SPI.h>
#include "MFRC522.h"
#include "EEPROMex.h"
#include "secrets.h"

#define ENABLE_RC
//#define ENABLE_LOGGING

#ifdef ENABLE_RC
#include "RCSwitch.h"

#define RC_PIN 2

unsigned long disableCodes[] = DISABLE_CODES;
unsigned long enableCodes[] = ENABLE_CODES;

RCSwitch rc = RCSwitch();
#endif

String kayCardIds[] = KEYCARD_IDS;

#define DELAY_TRIES_MS 15000
#define TIMEOUT_MS 40000
#define EEPROM_CHECK_VAL 0xB4
#define EEPROM_LOCKED_BIT 2

#define SS_PIN 10 //PINO SDA
#define RST_PIN 9 //PINO DE RESET
#define LED_PIN 5
#define RELAY_PIN A0

boolean isOpen = false;
boolean isLocked = false;
boolean userHasUnlocked = false;
boolean userHasTimedOut = false;
boolean manualLock = false;

MFRC522 rfid(SS_PIN, RST_PIN); //PASSAGEM DE PARÂMETROS REFERENTE AOS PINOS
int eepromAddr = 1;

void searchEeprom() {
#ifdef ENABLE_LOGGING
    Serial.println("Searching EEPROM for address.");
#endif

    int addr = 1;
    for (int i = 1; i < 1000; i++) {
        if (EEPROM.read(i) == EEPROM_CHECK_VAL) {
#ifdef ENABLE_LOGGING
            Serial.print("Found EEPROM marker at position ");
            Serial.println(i);
#endif
            addr = i;
            break;
        }
    }

#ifdef ENABLE_LOGGING
    Serial.print("Setting current EEPROM address to ");
    Serial.println(addr-1);
#endif
    eepromAddr = addr-1;
    EEPROM.update(addr, EEPROM_CHECK_VAL);
}

void setLocked(boolean locked) {
#ifdef ENABLE_LOGGING
    Serial.print("Setting hard lock status to ");
    Serial.println(locked ? "locked" : "open");
#endif

    EEPROM.updateBit(eepromAddr, EEPROM_LOCKED_BIT, locked);
    isLocked = locked;
}

void toggle(boolean open) {
#ifdef ENABLE_LOGGING
    Serial.print("Setting lock to ");
    Serial.println(open ? "open" : "closed");
#endif

    digitalWrite(RELAY_PIN, open ? HIGH : LOW);
    isOpen = open;
}

void unauthorize() {
    digitalWrite(LED_PIN, LOW);
    userHasUnlocked = false;
    manualLock = true;
    setLocked(true);
    toggle(false);

    delay(2000);
}

void authorize() {
    digitalWrite(LED_PIN, HIGH);
    userHasUnlocked = true;
    setLocked(false);
    toggle(true);

    delay(2000);
}

void setup() {
#ifdef ENABLE_LOGGING
    Serial.begin(9600);
    Serial.println("Starting.");
#endif

    pinMode(RELAY_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);

#ifdef ENABLE_RC
    rc.enableReceive(digitalPinToInterrupt(RC_PIN));
#endif

    SPI.begin();
    rfid.PCD_Init();

    isLocked = EEPROM.readBit(eepromAddr, EEPROM_LOCKED_BIT);
    toggle(!isLocked);
}

void loop() {
#ifdef ENABLE_RC
    if (manualLock) {
        return;
    }
#endif

    if (!userHasTimedOut && !userHasUnlocked && millis() > TIMEOUT_MS) {
#ifdef ENABLE_LOGGING
        Serial.println("Timeout for authentication, locking system.");
#endif

        userHasTimedOut = true;
        setLocked(true);
        toggle(false);
        return;
    }

#ifdef ENABLE_RC
    if (rc.available()) {
        unsigned long val = rc.getReceivedValue();

#ifdef ENABLE_LOGGING
        Serial.print("Read RC value ");
        Serial.println(val);
#endif

        if (!isLocked) {
            for (unsigned long code : disableCodes) {
                if (code == val) {
#ifdef ENABLE_LOGGING
                    Serial.println("Found RC code for disabling");
#endif
                    unauthorize();
                    break;
                }
            }
        }

        if (!userHasUnlocked) {
            for (unsigned long code : enableCodes) {
                if (code == val) {
#ifdef ENABLE_LOGGING
                    Serial.println("Found RC code for enabling");
#endif
                    authorize();
                    break;
                }
            }
        }

#ifdef ENABLE_LOGGING
        Serial.println("Resetting RC.");
#endif
        rc.resetAvailable();
    }
#endif

    if (!userHasUnlocked && rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
#ifdef ENABLE_LOGGING
        Serial.println("New RFID card present for reading");
#endif

        String readId = "";
        for (byte i = 0; i < rfid.uid.size; i++)
        {
            readId.concat(String(rfid.uid.uidByte[i] < 0x10 ? " 0" : ":"));
            readId.concat(String(rfid.uid.uidByte[i], HEX));
        }

#ifdef ENABLE_LOGGING
        Serial.print("Read RFID UID: ");
        Serial.println(readId);
#endif

        bool found = false;
        for (String id : kayCardIds) {
            if (id == readId) {
#ifdef ENABLE_LOGGING
                Serial.println("RFID UID is authorized");
#endif
                found = true;
                break;
            }
        }

        if (found) {
            authorize();
        } else {
            unauthorize();
        }

        rfid.PICC_HaltA();
        rfid.PCD_StopCrypto1();
    }
}