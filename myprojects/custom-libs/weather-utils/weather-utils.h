//
// Created by xav on 8/13/24.
//

#ifndef ACIOTIRCONTROLLER_WEATHER_UTILS_H
#define ACIOTIRCONTROLLER_WEATHER_UTILS_H

#include <UrlEncode.h>
#include <ArduinoJson.h>
#include "ESP8266httpUpdate.h"

namespace dxweather {
    #define OPENWEATHER_URL "http://api.openweathermap.org/data/2.5/weather"

    HTTPClient http;
    WiFiClient client;

    struct WeatherInfo {
        float temp;
        float feelsLike;
        float tempMin;
        float tempMax;
        long pressure;
        long humidity;
        long seaLevel;
        long groundLevel;
        String cityName;
        String description;
        bool hasError;
        String error;
    };

    void setup() {}

    void getWeatherInfo(const String& key, const String& lat, const String& lng, WeatherInfo &info) {
        String url = String(OPENWEATHER_URL) + "?lat=" + urlEncode(lat) + "&lon=" + urlEncode(lng) + "&appid=" + key + "&units=metric";
        http.begin(client, url);

        int res = http.GET();
        String resBody = http.getString();
        http.end();

        if (res != 200 && res != -1) {
            info.hasError = true;
            info.error = String(res) + ": " + resBody;
            return;
        }

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, resBody);

        if (error) {
            info.hasError = true;
            info.error = error.c_str();
            return;
        }

        JsonObject weatherObj = doc["weather"][0];
        JsonObject mainObj = doc["main"];

        info.temp = mainObj["temp"];
        info.feelsLike = mainObj["feels_like"];
        info.tempMin = mainObj["temp_min"];
        info.tempMax = mainObj["temp_max"];
        info.pressure = mainObj["pressure"];
        info.humidity = mainObj["humidity"];
        info.seaLevel = mainObj["sea_level"];
        info.groundLevel = mainObj["grnd_level"];
        info.cityName = String(doc["name"]);
        info.description = String(weatherObj["description"]);
    }
}

#endif //ACIOTIRCONTROLLER_WEATHER_UTILS_H
