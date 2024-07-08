#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include "secrets.h"

// Provide the token generation process info.
#include <addons/TokenHelper.h>

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

boolean set = false;
bool setPath = false;
String path = "";

void setup() {
    Serial.begin(115200);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.println();
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(300);
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();

    config.api_key = API_KEY;

    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;

    config.database_url = DATABASE_URL;

    // Comment or pass false value when WiFi reconnection will control by your code or third party library e.g. WiFiManager
    Firebase.reconnectNetwork(true);

    fbdo.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);
    fbdo.setResponseSize(4096);

    /* Assign the callback function for the long running token generation task */
    config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

    /* Initialize the library with the Firebase authen and config */
    Firebase.begin(&config, &auth);
}

void loop() {
    if (!set && Firebase.ready())
    {
        if (!setPath) {
            path = PATH_PREFIX + String(auth.token.uid.c_str()) + "/" + VEHICLE_UID;
            setPath = true;
        }

        FirebaseJson json;
        json.set("lat", "12.466451");
        json.set("lon", "124.465123");

        bool success = Firebase.RTDB.setJSON(&fbdo, path, &json);
        Serial.printf("Set val... %s\n", success ? "ok" : fbdo.errorReason().c_str());
        set = true;
    }
}