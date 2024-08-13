#ifndef DX_TELEGRAMUTILS_H
#define DX_TELEGRAMUTILS_H

#define TELEGRAM_BASE_URL "https://api.telegram.org/bot"

namespace tutils {
    HTTPClient http;
    WiFiClientSecure client;

    void setup() {
        client.setInsecure();
        client.setBufferSizes(2048, 2048);
    }

    String sendMessage(String chatId, String text, String token, bool getResponse) {
        String url = TELEGRAM_BASE_URL + token + "/sendMessage";
        http.begin(client, url);
        http.addHeader("Content-Type", "application/json");

        String body = "{\"chat_id\": \"" + chatId + "\", \"text\": \"" + text + "\"}";
        int res = http.POST(body);
        String resBody = getResponse ? http.getString() : String();
        http.end();

        return getResponse ? (String(res) + "," + resBody) : (String(res));
    }
}

#endif //DX_TELEGRAMUTILS_H