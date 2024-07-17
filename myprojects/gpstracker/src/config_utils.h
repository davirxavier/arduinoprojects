//
// Created by xav on 7/11/24.
//

#include "Arduino.h"
#include "ArduinoJson.h"
#include "LittleFS.h"
#include "../../custom-libs/crypto-helper.h"
#include "secrets.h"
#include "Base64.h"

struct VehicleConfig {
    String databaseUrl;
    String encryptionKey;
    String userKey;
    String vehicleUid;
    boolean parseSuccess;
    boolean decryptionSuccess;
};

VehicleConfig getConfig() {
    File envFile = LittleFS.open(F("/env_config"), "r");
    char data[envFile.size()];
    envFile.readBytes(data, envFile.size());

    int dataLength = Base64.decodedLength(data, sizeof(data));
    char dataDecoded[dataLength];
    Base64.decode(dataDecoded, data, sizeof(data));

    char salt[] = CONFIG_DECRYPTION_SALT;
    int saltLength = Base64.decodedLength(salt, sizeof(salt));
    char saltDecoded[saltLength];
    Base64.decode(saltDecoded, salt, sizeof(salt));

    char nonce[] = CONFIG_DECRYPTION_NONCE;
    int nonceLength = Base64.decodedLength(nonce, sizeof(nonce));
    char nonceDecoded[nonceLength];
    Base64.decode(nonceDecoded, nonce, sizeof(nonce));

    char tag[] = CONFIG_DECRYPTION_TAG;
    int tagLength = Base64.decodedLength(tag, sizeof(tag));
    char tagDecoded[tagLength];
    Base64.decode(tagDecoded, tag, sizeof(tag));

    char key[] = CONFIG_DECRYPTION_KEY;

    bool decryptionSuccess = experimental::crypto::ChaCha20Poly1305::decrypt(dataDecoded,
                                                                             dataLength,
                                                                             key,
                                                                             saltDecoded,
                                                                             SALT_SIZE,
                                                                             nonceDecoded,
                                                                             tagDecoded);


    VehicleConfig config;
    config.decryptionSuccess = decryptionSuccess;

    if (!decryptionSuccess) {
        return config;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, dataDecoded);

    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        config.parseSuccess = false;
    } else {
        config.parseSuccess = true;
        config.databaseUrl = String(doc["databaseUrl"]);
        config.userKey = String(doc["userKey"]);
        config.vehicleUid = String(doc["vehicleUid"]);

        String keyStr = String(doc["encryptionKey"]);
        char *key = const_cast<char *>(keyStr.c_str());
        int keyLength = Base64.decodedLength(key, keyStr.length() + 1);
        char keyDecoded[keyLength];
        Base64.decode(keyDecoded, key, keyStr.length()+1);
        config.encryptionKey = String(keyDecoded);
    }

    return config;
}
