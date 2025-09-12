#ifndef TELEGRAM_REQUESTS_ARDUINO_H
#define TELEGRAM_REQUESTS_ARDUINO_H

#ifndef TELEGRAM_REQUESTS_USING_IDF

#define TELEGRAM_REQUESTS_USING_ARDUINO
#include <telegram-requests.h>

namespace TelegramRequests
{
    inline WiFiClientSecure client;
    inline bool running = false;
    inline bool hasRequest = false;

    inline unsigned long retryTimeout = 5000;
    inline unsigned long retryTimer = -retryTimeout;
    inline unsigned long keepAliveTimeout = 150*1000;
    inline unsigned long keepAliveTimer = 0;

    inline void readRemaining(unsigned long timeout)
    {
#ifdef TELEGRAM_LOG_DEBUG
        TEL_LOGN("Telegram response:");
#endif

        unsigned long timeoutCounter = millis();
        bool hasRead = false;

        while (client.connected() || hasRead)
        {
            if (client.available())
            {
                timeoutCounter = millis();
                hasRead = true;
#ifdef TELEGRAM_LOG_DEBUG
                TEL_LOG((char) client.read());
#else
                client.read();
#endif
            }
            else if (hasRead)
            {
                break;
            }

            if (millis() - timeoutCounter > timeout)
            {
                break;
            }
        }

#ifdef TELEGRAM_LOG_DEBUG
        TEL_LOG("\n\n");
#endif
    }

    inline bool tryConnect()
    {
        if (!running)
        {
            return false;
        }

        if (!client.connected() && millis() - retryTimer > retryTimeout && !client.connect(TelegramConsts::host, 443))
        {
            TEL_LOGN("Connection to telegram failed, trying again after some time.");
            retryTimer = millis();
            return false;
        }

        return true;
    }

    inline void init(const char *newToken, const char *chatId)
    {
        TelegramConsts::token = nullptr;
        TelegramConsts::setToken(newToken);

        TelegramConsts::defaultChatId = nullptr;
        TelegramConsts::setDefaultChatId(chatId);

        running = true;

#ifdef TELEGRAM_BOT_INSECURE
        client.setInsecure();
#else
#ifdef ESP32
        client.setCACert(TelegramConsts::telegramCert);
#elif ESP8266

        tm time;
        if (!getLocalTime(&time))
        {
            configTime(0, 0, "pool.ntp.org");
        }

        X509List certs(TelegramConsts::telegramCert);
        client.setTrustAnchors(&certs);
#endif
#endif
    }


    inline TelegramConsts::TelegramStatus telegramStartTransaction(TelegramConsts::TelegramRequestType type,
                                                                   const char *chatId,
                                                                   const char *data,
                                                                   const size_t dataSize)
    {
        if (type >= TelegramConsts::LAST)
        {
            return TelegramConsts::UNKNOWN_TYPE;
        }

        if (TelegramConsts::token == nullptr)
        {
            TEL_LOGN("No token set, aborting request.");
            return TelegramConsts::NO_TOKEN;
        }

        if (!tryConnect())
        {
            return TelegramConsts::CONNECTION_FAILED;
        }

        char buffer[256];
        size_t bufferSize = sizeof(buffer);

        const char *pathName = TelegramConsts::requestPathByType[type];
        snprintf(buffer, bufferSize, "POST /bot%s/%s?chat_id=%s HTTP/1.1\r\n", TelegramConsts::token, pathName, chatId);
        TEL_PRINT_BOTH(buffer);

        const char *fieldName = TelegramConsts::requestFieldByType[type];
        size_t contentLength = 0;
        uint8_t endBoundarySize = 6;

        if (type == TelegramConsts::MESSAGE)
        {
            snprintf(buffer,
                bufferSize,
                "--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n",
                TelegramConsts::multipartBoundary,
                fieldName);

            contentLength = strlen(buffer) + strlen(data) + strlen(TelegramConsts::multipartBoundary) + endBoundarySize;
        }
        else
        {
            snprintf(buffer,
                bufferSize,
                "--%s\r\nContent-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n\r\n",
                TelegramConsts::multipartBoundary,
                fieldName,
                data);

            contentLength = strlen(buffer) + dataSize + strlen(TelegramConsts::multipartBoundary) + endBoundarySize;
        }

        TEL_PRINT_BOTH(F("Host: "));
        TEL_PRINT_BOTH(TelegramConsts::host);
        TEL_PRINT_BOTH(F("\r\n"));
        TEL_PRINT_BOTH(F("Connection: keep-alive\r\n"));
        TEL_PRINT_BOTH(F("Content-Type: "));
        TEL_PRINT_BOTH(TelegramConsts::multipartContentTypeHeader);
        TEL_PRINT_BOTH(F("\r\n"));
        TEL_PRINT_BOTH(F("Content-Length: "));
        TEL_PRINT_BOTH(contentLength);
        TEL_PRINT_BOTH(F("\r\n"));
        TEL_PRINT_BOTH(F("Accepts: */*"));
        TEL_PRINT_BOTH(F("\r\n"));
        TEL_PRINT_BOTH(F("User-Agent: esp-telegram-bot-client"));
        TEL_PRINT_BOTH(F("\r\n\r\n"));
        TEL_PRINT_BOTH(buffer);

        hasRequest = true;
        return TelegramConsts::OK;
    }

    inline size_t telegramWriteData(uint8_t *data, size_t size)
    {
        if (!client.connected())
        {
            return 0;
        }

        return client.write(data, size);
    }

    inline TelegramConsts::TelegramStatus telegramEndTransaction()
    {
        if (!client.connected())
        {
            hasRequest = false;
            return TelegramConsts::NOT_CONNECTED;
        }

        TEL_PRINT_BOTH(F("\r\n--"));
        TEL_PRINT_BOTH(TelegramConsts::multipartBoundary);
        TEL_PRINT_BOTH(F("--"));
        TEL_LOG("\n");

        client.flush();
        readRemaining();
        hasRequest = false;
        return TelegramConsts::OK;
    }

    inline TelegramConsts::TelegramStatus sendMessage(const char *message, const char *chatId)
    {
        unsigned long timeCounter = millis();

        TelegramConsts::TelegramStatus startStatus = telegramStartTransaction(TelegramConsts::MESSAGE,
            chatId == nullptr ? TelegramConsts::defaultChatId : chatId,
            message);

        if (startStatus != TelegramConsts::OK)
        {
            TEL_LOGF("Start transaction error: %d\n", startStatus);
            return startStatus;
        }

        if (telegramWriteData((uint8_t*) message, strlen(message)) == 0)
        {
            return TelegramConsts::WRITE_ERROR;
        }

        TelegramConsts::TelegramStatus ret = telegramEndTransaction();
        TEL_LOGF("Telegram sendMessage time taken (ms): %lu\n", millis() - timeCounter);
        return ret;
    }

    inline void sendKeepAlive()
    {
        if (TelegramConsts::token == nullptr || hasRequest)
        {
            return;
        }

        TEL_LOGN("Sending keep-alive.");
        tryConnect();

        TEL_PRINT_BOTH(F("GET /bot"));
        TEL_PRINT_BOTH(TelegramConsts::token);
        TEL_PRINT_BOTH(F("/getMe HTTP/1.1\r\n"));
        TEL_PRINT_BOTH(F("Host: "));
        TEL_PRINT_BOTH(TelegramConsts::host);
        TEL_PRINT_BOTH(F("\r\n"));
        TEL_PRINT_BOTH(F("Connection: keep-alive\r\n"));
        TEL_PRINT_BOTH(F("Accepts: */*\r\n"));
        TEL_PRINT_BOTH(F("User-Agent: esp-telegram-bot-client\r\n\r\n"));
        readRemaining();
    }

    inline TelegramConsts::TelegramStatus loop()
    {
        if (!running)
        {
            return TelegramConsts::NOT_CONNECTED;
        }

        tryConnect();

        if (!hasRequest && millis() - keepAliveTimer > keepAliveTimeout)
        {
            sendKeepAlive();
            keepAliveTimer = millis();
            return TelegramConsts::KEEP_ALIVE;
        }

        return TelegramConsts::OK;
    }

    inline void stop()
    {
        running = false;
        client.stop();
    }

    inline bool isConnected()
    {
        return client.connected();
    }
}

#endif
#endif //TELEGRAM_REQUESTS_ARDUINO_H