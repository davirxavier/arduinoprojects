#ifndef XAV_CRYPTO_HELPER_H
#define XAV_CRYPTO_HELPER_H

#include <Crypto.h>
#include "base64.h"

#define KEY_SIZE 32
#define SALT_SIZE 32
#define NONCE_SIZE 12
#define TAG_SIZE 16
const String ALPHABET = R"(0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz)";

void printHex(const char arr[], int size) {
    for (int i = 0; i < size; i++) {
        if (arr[i] < 16) { Serial.print("0"); }
        Serial.print(arr[i], HEX);
    }
    Serial.println();
}

String charToHex(const char input[], int size) {
    String res = "";

    for (uint8_t i = 0; i < size; i++) {
        char n = input[i];

        if (n < 16) {
            res += "0";
        }

        char buf[8 * sizeof(n) + 1];
        char *str = &buf[sizeof(buf) - 1];

        *str = '\0';
        do {
            auto m = n;
            n /= 16;
            char c = m - 16 * n;

            *--str = c < 10 ? c + '0' : c + 'A' - 10;
        } while (n);

        res += str;
    }

    return res;
}

/**
 * @return string with the following format, in order:
 *  - first three characters are the salt size (ex.: 032 or 128)
 *  - salt, respecting the size defined previously
 *  - 12 characters containing the nonce
 *  - 16 characters containing the authentication tag
 *  - the remaining length, containing the encrypted data
 */
String cryptoHelperEncrypt(const String &dataStr, const char *key) {
    char salt[SALT_SIZE];
    for (int i = 0; i < SALT_SIZE; i++) {
        salt[i] = ALPHABET.charAt(random(0, ALPHABET.length() - 1));
    }

    char nonceResult[12];
    char tagResult[16];

    char buf[dataStr.length() + 1];
    dataStr.toCharArray(buf, dataStr.length() + 1);

    experimental::crypto::ChaCha20Poly1305::encrypt(buf,
                                                    dataStr.length(),
                                                    key,
                                                    salt,
                                                    SALT_SIZE,
                                                    nonceResult,
                                                    tagResult);

    String saltHex = charToHex(salt, SALT_SIZE);
    String nonceHex = charToHex(nonceResult, NONCE_SIZE);
    String tagHex = charToHex(tagResult, TAG_SIZE);
    String dataHex = charToHex(buf, dataStr.length());

    return String("0") + saltHex.length() + saltHex +
           nonceHex.length() + nonceHex +
           tagHex.length() + tagHex +
           dataHex;
}

bool cryptoHelperDecrypt(char *data,
                         const unsigned int dataLength,
                         const char key[],
                         const char salt[],
                         const char nonce[],
                         const char tag[]) {
    return experimental::crypto::ChaCha20Poly1305::decrypt(data,
                                                    dataLength,
                                                    key,
                                                    salt,
                                                    SALT_SIZE,
                                                    nonce,
                                                    tag);
}

#endif