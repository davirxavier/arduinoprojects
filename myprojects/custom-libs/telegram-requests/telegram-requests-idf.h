#ifndef TELEGRAM_REQUESTS_IDF_H
#define TELEGRAM_REQUESTS_IDF_H

#ifndef TELEGRAM_REQUESTS_USING_ARDUINO

#define TELEGRAM_REQUESTS_USING_IDF
#include <telegram-requests.h>
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

namespace TelegramRequests
{
    char fullUrl[333] = "https://api.telegram.org";
    esp_http_client_handle_t client = nullptr;
    bool running = false;
    bool hasRequest = false;
    bool _isConnected = false;

    unsigned long retryTimeout = 5000;
    unsigned long retryTimer = -retryTimeout-1;
    unsigned long keepAliveTimeout = 150*1000;
    unsigned long keepAliveTimer = 0;
    unsigned long transactionTimer = 0;

    inline void readRemaining(unsigned long timeout)
    {
#ifdef TELEGRAM_LOG_DEBUG
        TEL_LOGN("Telegram response:");
#endif

        char buffer[32];
        int readLen = 0;
        while ((readLen = esp_http_client_read(client, buffer, sizeof(buffer))) > 0)
        {
#ifdef TELEGRAM_LOG_DEBUG
            Serial.write((char*) buffer, readLen);
#endif
        }

#ifdef TELEGRAM_LOG_DEBUG
        TEL_LOG("\n\n");
#endif
    }

    inline bool tryConnect(int contentLength)
    {
        if (!running)
        {
            return false;
        }

        if (esp_http_client_open(client, contentLength) != ESP_OK)
        {
            TEL_LOGN("Connection to telegram failed.");
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
        esp_http_client_config_t telegramConfig = {
            .url = fullUrl,
            .method = HTTP_METHOD_POST,
            .timeout_ms = 10000,
            .event_handler = [](esp_http_client_event_t *event)
            {
                if (event->event_id == HTTP_EVENT_ON_CONNECTED)
                {
                    TEL_LOGN("Connected to host.");
                    _isConnected = true;
                }
                else if (event->event_id == HTTP_EVENT_DISCONNECTED)
                {
                    TEL_LOGN("Disconnected from host.");
                    _isConnected = false;
                }

                return ESP_OK;
            },
            .buffer_size = 512,
            .buffer_size_tx = 4069,
#ifdef TELEGRAM_BOT_INSECURE
            .skip_cert_common_name_check = true,
#else
            .crt_bundle_attach = esp_crt_bundle_attach,
#endif
            .keep_alive_enable = true,
        };
        client = esp_http_client_init(&telegramConfig);
        esp_http_client_set_header(client, "Content-Type", TelegramConsts::multipartContentTypeHeader);
        esp_http_client_set_header(client, "Accept", "*/*");
        // loop();
    }

    inline TelegramConsts::TelegramStatus telegramStartTransaction(TelegramConsts::TelegramRequestType type,
                                                                   const char *chatId,
                                                                   const char *data,
                                                                   const size_t dataSize)
    {
        transactionTimer = millis();
        if (type >= TelegramConsts::LAST)
        {
            return TelegramConsts::UNKNOWN_TYPE;
        }

        if (TelegramConsts::token == nullptr)
        {
            TEL_LOGN("No token set, aborting request.");
            return TelegramConsts::NO_TOKEN;
        }

        const char *pathName = TelegramConsts::requestPathByType[type];
        snprintf(fullUrl, sizeof(fullUrl), "https://%s/bot%s/%s?chat_id=%s", TelegramConsts::host, TelegramConsts::token, pathName, chatId);
        esp_http_client_set_url(client, fullUrl);

        const char *fieldName = TelegramConsts::requestFieldByType[type];
        int contentLength = 0;
        uint8_t endBoundarySize = 6;

        char dispositionHeader[256]{};
        if (type == TelegramConsts::MESSAGE)
        {
            snprintf(dispositionHeader,
                sizeof(dispositionHeader),
                "--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n",
                TelegramConsts::multipartBoundary,
                fieldName);

            contentLength = strlen(dispositionHeader) + strlen(data) + strlen(TelegramConsts::multipartBoundary) + endBoundarySize;
        }
        else
        {
            snprintf(dispositionHeader,
                sizeof(dispositionHeader),
                "--%s\r\nContent-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n\r\n",
                TelegramConsts::multipartBoundary,
                fieldName,
                data);

            contentLength = strlen(dispositionHeader) + dataSize + strlen(TelegramConsts::multipartBoundary) + endBoundarySize;
        }

        esp_http_client_set_method(client, HTTP_METHOD_POST);
        if (!tryConnect(contentLength))
        {
            return TelegramConsts::CONNECTION_FAILED;
        }

        esp_http_client_write(client, dispositionHeader, strlen(dispositionHeader));
        hasRequest = true;
        return TelegramConsts::OK;
    }

    inline size_t telegramWriteData(uint8_t *data, size_t size)
    {
        return esp_http_client_write(client, (char*) data, size);
    }

    inline TelegramConsts::TelegramStatus telegramEndTransaction()
    {
        esp_http_client_write(client, "\r\n--", 4);
        esp_http_client_write(client, TelegramConsts::multipartBoundary, strlen(TelegramConsts::multipartBoundary));
        esp_http_client_write(client, "--", 2);

        readRemaining();
        hasRequest = false;
        TEL_LOGF("Transaction total time: %lu\n", millis() - transactionTimer);
        return TelegramConsts::OK;
    }

    inline TelegramConsts::TelegramStatus sendMessage(const char *message, const char *chatId)
    {
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

        return telegramEndTransaction();
    }

    inline void sendKeepAlive()
    {
        if (TelegramConsts::token == nullptr || hasRequest)
        {
            return;
        }

        TEL_LOGN("Sending keep-alive.");

        snprintf(fullUrl, sizeof(fullUrl), "https://%s/bot%s/getMe", TelegramConsts::host, TelegramConsts::token);
        esp_http_client_set_url(client, fullUrl);
        esp_http_client_set_method(client, HTTP_METHOD_GET);

        tryConnect(0);
        readRemaining();
    }

    inline TelegramConsts::TelegramStatus loop()
    {
        if (!running)
        {
            return TelegramConsts::NOT_CONNECTED;
        }

        if (!_isConnected && !hasRequest && millis() - retryTimer > retryTimeout)
        {
            sendKeepAlive();
            retryTimer = millis();
            return TelegramConsts::CONNECT_RETRY;
        }

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
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
    }

    inline bool isConnected()
    {
        return _isConnected;
    }
}

#endif
#endif