#ifndef TELEGRAM_REQUESTS_CONSTS_H
#define TELEGRAM_REQUESTS_CONSTS_H

// #define TELEGRAM_BOT_INSECURE
#ifdef TELEGRAM_BOT_INSECURE
#warning "Telegram bot is in insecure mode, no data encryption will be used!"
#endif

// #define TELEGRAM_ENABLE_LOGGING
// #define TELEGRAM_LOG_DEBUG

#ifdef TELEGRAM_ENABLE_LOGGING
#define TEL_LOG(s) Serial.print(s)
#define TEL_LOGN(s) Serial.print("[TELEGRAM-BOT] "); Serial.println(s)
#define TEL_LOGF(s, p...) Serial.print("[TELEGRAM-BOT] "); Serial.printf(s, p)
#else
#define TEL_LOG(s)
#define TEL_LOGN(s)
#define TEL_LOGF(s, p...)
#endif

#ifdef TELEGRAM_LOG_DEBUG
#define TEL_PRINT_BOTH(s) client.print(s); TEL_LOG(s)
#else
#define TEL_PRINT_BOTH(s) client.print(s)
#endif

namespace TelegramConsts
{
    enum TelegramRequestType
    {
        MESSAGE,
        PHOTO,
        LAST, // Do not use
    };

    constexpr size_t requestCount = LAST + 1;
    constexpr char requestPathByType[requestCount][20] = {"sendMessage", "sendPhoto"};
    constexpr char requestFieldByType[requestCount][20] = {"text", "photo"};

    enum TelegramStatus
    {
        OK,
        NO_TOKEN,
        NO_CHAT_ID,
        CONNECTION_FAILED,
        UNKNOWN_TYPE,
        NO_WRITE_CALLBACK_DEFINED,
        NOT_CONNECTED,
        WRITE_ERROR,
        KEEP_ALIVE,
        CONNECT_RETRY,
        BUSY,
    };

    constexpr char host[] = "api.telegram.org";
    constexpr char multipartBoundary[] = "ESP_TELEGRAM";
    constexpr char multipartContentTypeHeader[] = "multipart/form-data; boundary=ESP_TELEGRAM";

#ifndef TELEGRAM_BOT_INSECURE
#ifdef TELEGRAM_REQUESTS_USING_ARDUINO
    const char telegramCert[] PROGMEM = \
        "-----BEGIN CERTIFICATE-----\n"
        "MIIEADCCAuigAwIBAgIBADANBgkqhkiG9w0BAQUFADBjMQswCQYDVQQGEwJVUzEh\n"
        "MB8GA1UEChMYVGhlIEdvIERhZGR5IEdyb3VwLCBJbmMuMTEwLwYDVQQLEyhHbyBE\n"
        "YWRkeSBDbGFzcyAyIENlcnRpZmljYXRpb24gQXV0aG9yaXR5MB4XDTA0MDYyOTE3\n"
        "MDYyMFoXDTM0MDYyOTE3MDYyMFowYzELMAkGA1UEBhMCVVMxITAfBgNVBAoTGFRo\n"
        "ZSBHbyBEYWRkeSBHcm91cCwgSW5jLjExMC8GA1UECxMoR28gRGFkZHkgQ2xhc3Mg\n"
        "MiBDZXJ0aWZpY2F0aW9uIEF1dGhvcml0eTCCASAwDQYJKoZIhvcNAQEBBQADggEN\n"
        "ADCCAQgCggEBAN6d1+pXGEmhW+vXX0iG6r7d/+TvZxz0ZWizV3GgXne77ZtJ6XCA\n"
        "PVYYYwhv2vLM0D9/AlQiVBDYsoHUwHU9S3/Hd8M+eKsaA7Ugay9qK7HFiH7Eux6w\n"
        "wdhFJ2+qN1j3hybX2C32qRe3H3I2TqYXP2WYktsqbl2i/ojgC95/5Y0V4evLOtXi\n"
        "EqITLdiOr18SPaAIBQi2XKVlOARFmR6jYGB0xUGlcmIbYsUfb18aQr4CUWWoriMY\n"
        "avx4A6lNf4DD+qta/KFApMoZFv6yyO9ecw3ud72a9nmYvLEHZ6IVDd2gWMZEewo+\n"
        "YihfukEHU1jPEX44dMX4/7VpkI+EdOqXG68CAQOjgcAwgb0wHQYDVR0OBBYEFNLE\n"
        "sNKR1EwRcbNhyz2h/t2oatTjMIGNBgNVHSMEgYUwgYKAFNLEsNKR1EwRcbNhyz2h\n"
        "/t2oatTjoWekZTBjMQswCQYDVQQGEwJVUzEhMB8GA1UEChMYVGhlIEdvIERhZGR5\n"
        "IEdyb3VwLCBJbmMuMTEwLwYDVQQLEyhHbyBEYWRkeSBDbGFzcyAyIENlcnRpZmlj\n"
        "YXRpb24gQXV0aG9yaXR5ggEAMAwGA1UdEwQFMAMBAf8wDQYJKoZIhvcNAQEFBQAD\n"
        "ggEBADJL87LKPpH8EsahB4yOd6AzBhRckB4Y9wimPQoZ+YeAEW5p5JYXMP80kWNy\n"
        "OO7MHAGjHZQopDH2esRU1/blMVgDoszOYtuURXO1v0XJJLXVggKtI3lpjbi2Tc7P\n"
        "TMozI+gciKqdi0FuFskg5YmezTvacPd+mSYgFFQlq25zheabIZ0KbIIOqPjCDPoQ\n"
        "HmyW74cNxA9hi63ugyuV+I6ShHI56yDqg+2DzZduCLzrTia2cyvk0/ZM/iZx4mER\n"
        "dEr/VxqHD3VILs9RaRegAhJhldXRQLIQTO7ErBBDpqWeCtWVYpoNz4iCxTIM5Cuf\n"
        "ReYNnyicsbkqWletNw+vHX/bvZ8=\n"
        "-----END CERTIFICATE-----";
#endif
#endif

    char *token = nullptr;
    char *defaultChatId = nullptr;

    /**
     * Sets the telegram bot token. Copies the passed string to a new dynamically allocated string.
     */
    inline void setToken(const char *newToken)
    {
        if (token != nullptr)
        {
            free(token);
            token = nullptr;
        }

        if (newToken == nullptr)
        {
            return;
        }

        token = (char*) malloc(strlen(newToken)+1);
        strcpy(token, newToken);
    }

    /**
    * Sets the default telegram chat id to send messaged to. Copies the passed string to a new dynamically allocated string.
    */
    inline void setDefaultChatId(const char *newChatId)
    {
        if (defaultChatId != nullptr)
        {
            free(defaultChatId);
            defaultChatId = nullptr;
        }

        if (newChatId == nullptr)
        {
            return;
        }

        defaultChatId = (char*) malloc(strlen(newChatId)+1);
        strcpy(defaultChatId, newChatId);
    }
}

namespace TelegramRequests
{
    /**
     * Reads all remaining available data in the telegram client.
     */
    void readRemaining(unsigned long timeout = 5000);

    /**
     * Tries to connect the client to the telegram server.
     */
    bool tryConnect(int contentLength);

    /**
     * Loop function, should be called in the main loop.
     * @return returns what has happened in this loop instance.
     */
    TelegramConsts::TelegramStatus loop();

    /**
     * Initializer for the module. Should be called before every send function.
     * @param newToken bot token for auth.
     * @param chatId default chat id to send messages.
     */
    void init(const char *newToken, const char *chatId);

    /**
     * Stops the module.
     */
    void stop();

    /**
     * Starts a telegram transaction.
     * @param type type of transaction
     * @param chatId chat id to send the message to, can be nullptr, in which the default chat id will be used.
     * @param data should be the message text if the type = MESSAGE and the filename if the request type is a file type.
     * @param dataSize should be the size of the file, if the type is a file type.
     * @return TelegramConsts::OK if success, other errors from TelegramConsts::TelegramStatus if not.
     */
    TelegramConsts::TelegramStatus telegramStartTransaction(TelegramConsts::TelegramRequestType type,
                                                            const char *chatId,
                                                            const char *data,
                                                            const size_t dataSize = 0);

    /**
     * Writes data to an open telegram transaction (opened with telegramStartTransaction).
     * @return the size of data written.
     */
    size_t telegramWriteData(uint8_t *data, size_t size);

    /**
     * Ends the current telegram transaction.
     * @return TelegramConsts::OK if success, other errors from TelegramConsts::TelegramStatus if not.
     */
    TelegramConsts::TelegramStatus telegramEndTransaction();

    /**
     * Sends a telegram message to the specified chat id.
     * @return TelegramConsts::OK if success, other errors from TelegramConsts::TelegramStatus if not.
     */
    TelegramConsts::TelegramStatus sendMessage(const char *message, const char *chatId = nullptr);

    /**
     * Sends a keep-alive request (for internal use only).
     */
    TelegramConsts::TelegramStatus sendKeepAlive();

    /**
     * Checks if the internal tcp client is currently connected to the telegram server.
     */
    bool isConnected();
}

#endif