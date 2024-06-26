#include <Arduino.h>
#include <SPI.h>
#include "MFRC522.h"
#include "EEPROMex.h"
#include "RtcDS1302.h"

#define KEY1 "4D:BC:0D:32"
#define KEY2 "C5:7F:AC:AC"

#define SS_PIN 10 //PINO SDA
#define RST_PIN 9 //PINO DE RESET
#define LED_PIN 8
#define RELAY_PIN A0

#define EEPROM_CHECK_VAL 0xA6

ThreeWire rtcwire(7, A6, A2);
RtcDS1302<ThreeWire> rtc(rtcwire);
MFRC522 rfid(SS_PIN, RST_PIN); //PASSAGEM DE PARÂMETROS REFERENTE AOS PINOS

short booleansAddr = 0;

unsigned short isCountingBit = 2;

unsigned short lockedBit = 1;
bool locked = false;
unsigned int timeToLock = 47;

unsigned short timeoutBit = 0;
bool timeout;
unsigned int defaultTimeout = 30;

bool readSuccessfully = false;

void searchNextMemorySlot() {
    int addr = 0;
    for (int i = 1; i < 1000; i++) {
        if (EEPROM.read(i) == EEPROM_CHECK_VAL) {
            Serial.print("Found EEPROM marker at address: ");
            Serial.println(i);
            addr = i;
            break;
        }
    }

    booleansAddr = addr-1;
    EEPROM.update(addr, EEPROM_CHECK_VAL);
}

void setCounting(bool val) {
    EEPROM.updateBit(booleansAddr, lockedBit, val);
    locked = val;
}

bool readCounting() {
    return EEPROM.readBit(booleansAddr, isCountingBit);
}


void setLocked(bool val) {
    if (locked != val) {
        EEPROM.updateBit(booleansAddr, lockedBit, val);
    }
    locked = val;
}

bool readLocked() {
    return EEPROM.readBit(booleansAddr, lockedBit);
}

void setHasTimeout(bool hasTimeout) {
    if (timeout != hasTimeout) {
        EEPROM.updateBit(booleansAddr, timeoutBit, !hasTimeout);
    }
    timeout = hasTimeout;
}

bool readTimeout() {
    return !EEPROM.readBit(booleansAddr, timeoutBit);
}

void toggle(bool open) {
    if (open) {
        analogWrite(RELAY_PIN, 255);
        digitalWrite(LED_PIN, HIGH);
    } else {
        analogWrite(RELAY_PIN, 0);
        digitalWrite(LED_PIN, LOW);
    }
}

void setup() {
    pinMode(LED_PIN, OUTPUT);
    searchNextMemorySlot();

    rtc.Begin();

    SPI.begin(); //INICIALIZA O BARRAMENTO SPI
    rfid.PCD_Init(); //INICIALIZA MFRC522
    rfid.PCD_SetAntennaGain(rfid.RxGain_max);

    timeout = readTimeout();
    locked = readLocked();

    toggle(!locked);

    if (!locked && !readCounting()) {
        setCounting(true);
    }
}

void loop() {
    if (!readSuccessfully && millis() > timeToLock*1000) {
        toggle(false);
        setLocked(true);
    }

    if (!readSuccessfully && timeout) {
        delay(defaultTimeout * 1000);
        setHasTimeout(false);
    }

    if (!readSuccessfully && (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial())) //VERIFICA SE O CARTÃO PRESENTE NO LEITOR É DIFERENTE DO ÚLTIMO CARTÃO LIDO. CASO NÃO SEJA, FAZ
        return; //RETORNA PARA LER NOVAMENTE

    /***INICIO BLOCO DE CÓDIGO RESPONSÁVEL POR GERAR A TAG RFID LIDA***/
    String strID = "";
    for (byte i = 0; i < 4; i++) {
        strID +=
                (rfid.uid.uidByte[i] < 0x10 ? "0" : "") +
                String(rfid.uid.uidByte[i], HEX) +
                (i!=3 ? ":" : "");
    }
    strID.toUpperCase();
    /***FIM DO BLOCO DE CÓDIGO RESPONSÁVEL POR GERAR A TAG RFID LIDA***/

    if (strID == F(KEY1) || strID == F(KEY2)) {
        toggle(true);
        readSuccessfully = true;
        setLocked(false);
        setCounting(false);
    } else {
        toggle(false);
        setHasTimeout(true);
    }

    rfid.PICC_HaltA(); //PARADA DA LEITURA DO CARTÃO
    rfid.PCD_StopCrypto1(); //PARADA DA CRIPTOGRAFIA NO PCD
}