#include <Arduino.h>
#include "EEPROMex.h"
#include <SPI.h> //INCLUSÃO DE BIBLIOTECA
#include "MFRC522.h" //INCLUSÃO DE BIBLIOTECA

// Chaveiro: 96:B2:F5:5F
// Cartão 1A:DB:B8:89

#define SS_PIN 10 //PINO SDA
#define RST_PIN 9 //PINO DE RESET

MFRC522 rfid(SS_PIN, RST_PIN); //PASSAGEM DE PARÂMETROS REFERENTE AOS PINOS

short booleansAddr = 0;

unsigned short lockedBit = 1;
bool locked = false;
unsigned int timeToLock = 48;

unsigned short timeoutBit = 0;
bool timeout;
unsigned int defaultTimeout = 30;

bool readSuccessfully = false;

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
        analogWrite(3, 0);
    } else {
        analogWrite(3, 255);
    }
}

void setup() {
    Serial.begin(9600); //INICIALIZA A SERIAL
    SPI.begin(); //INICIALIZA O BARRAMENTO SPI
    rfid.PCD_Init(); //INICIALIZA MFRC522
    rfid.PCD_SetAntennaGain(rfid.RxGain_max);

    timeout = readTimeout();
    locked = readLocked();

    toggle(!locked);
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

    if (readSuccessfully || (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial())) //VERIFICA SE O CARTÃO PRESENTE NO LEITOR É DIFERENTE DO ÚLTIMO CARTÃO LIDO. CASO NÃO SEJA, FAZ
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

    Serial.println("reading");
    if (strID == F("96:B2:F5:5F") || strID == F("1A:DB:B8:89")) {
        toggle(false);
        delay(700);
        toggle(true);

        readSuccessfully = true;
        setLocked(false);
    } else {
        toggle(false);
        setHasTimeout(true);
    }

    rfid.PICC_HaltA(); //PARADA DA LEITURA DO CARTÃO
    rfid.PCD_StopCrypto1(); //PARADA DA CRIPTOGRAFIA NO PCD
}