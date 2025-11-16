#ifndef TELEGRAM_REQUESTS_IDF_H
#define TELEGRAM_REQUESTS_IDF_H

#ifndef TELEGRAM_REQUESTS_USING_ARDUINO

#define TELEGRAM_REQUESTS_USING_IDF
#include <telegram-requests.h>
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

#ifdef TELEGRAM_LOG_DEBUG
#define TELEGRAM_PRINT_BOTH_IDF(s, l) esp_http_client_write(s, l); TEL_LOGN(s)
#else
#define TELEGRAM_PRINT_BOTH_IDF(s, l)
#endif

namespace TelegramRequests
{
    char fullUrl[333] = "https://api.telegram.org";
    esp_http_client_handle_t client = nullptr;
    bool running = false;
    bool hasRequest = false;
    bool _isConnected = false;

    unsigned long retryTimeout = 5000;
    unsigned long retryTimer = -retryTimeout-1;
    unsigned long keepAliveTimeout = 120000;
    unsigned long keepAliveTimer = 0;
    unsigned long transactionTimer = 0;

    wl_status_t lastWifiStatus = WL_STOPPED;
    bool needsKeepAlive = false;

    inline void initClient()
    {
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
                    hasRequest = false;
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
        esp_http_client_set_header(client, "Accept", "application/json");
    }

    inline void readRemaining(unsigned long timeout)
    {
        if (!_isConnected)
        {
            return;
        }

        int64_t len = esp_http_client_fetch_headers(client);
        int statusCode = esp_http_client_get_status_code(client);
#ifdef TELEGRAM_LOG_DEBUG
        TEL_LOGF("Telegram response (status/content-length/body): %d/%lld\n", statusCode, len);
#endif

        char buffer[128]{};
        int readLen = 0;
        while ((readLen = esp_http_client_read(client, buffer, sizeof(buffer))) > 0)
        {
            if (!_isConnected)
            {
                return;
            }

#ifdef TELEGRAM_LOG_DEBUG
            for (int i = 0; i < readLen; i++)
            {
                Serial.print(buffer[i]);
            }
#endif
        }

#ifdef TELEGRAM_LOG_DEBUG
        TEL_LOG("\n\n");
#endif
    }

    inline bool tryConnect(int contentLength)
    {
        TEL_LOGN("Trying to connect to telegram.");
        if (!running)
        {
            TEL_LOGN("Not running, cancelling connection.");
            return false;
        }

        TEL_LOGF("Trying connection with content length: %d\n", contentLength);
        if (esp_http_client_open(client, contentLength) != ESP_OK)
        {
            TEL_LOGN("Connection to telegram failed.");
            return false;
        }

        _isConnected = true;
        TEL_LOGN("Telegram open success.");
        return true;
    }

    inline void init(const char *newToken, const char *chatId)
    {
        TelegramConsts::token = nullptr;
        TelegramConsts::setToken(newToken);

        TelegramConsts::defaultChatId = nullptr;
        TelegramConsts::setDefaultChatId(chatId);

        running = true;
        initClient();
        // loop();
    }

    inline TelegramConsts::TelegramStatus telegramStartTransaction(TelegramConsts::TelegramRequestType type,
                                                                   const char *chatId,
                                                                   const char *data,
                                                                   const size_t dataSize)
    {
        if (!_isConnected || needsKeepAlive)
        {
            return TelegramConsts::CONNECTION_FAILED;
        }

        if (hasRequest)
        {
            return TelegramConsts::BUSY;
        }

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

#ifdef TELEGRAM_LOG_DEBUG
        TEL_LOGN(dispositionHeader);
#endif
        esp_http_client_write(client, dispositionHeader, strlen(dispositionHeader));
        hasRequest = true;
        return TelegramConsts::OK;
    }

    inline size_t telegramWriteData(uint8_t *data, size_t size)
    {
        if (!_isConnected || !hasRequest)
        {
            return 0;
        }

        int written = esp_http_client_write(client, (char*) data, size);
#ifdef TELEGRAM_LOG_DEBUG
        TEL_LOGF("Written %lu bytes of %lu.\n", written, size);
#endif
        return written;
    }

    inline TelegramConsts::TelegramStatus telegramEndTransaction()
    {
        if (!_isConnected || !hasRequest)
        {
            return TelegramConsts::NOT_CONNECTED;
        }

#ifdef TELEGRAM_LOG_DEBUG
        TEL_LOGF("\\r\\n--%s--\n", TelegramConsts::multipartBoundary);
#endif

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

    inline TelegramConsts::TelegramStatus sendKeepAlive()
    {
        if (TelegramConsts::token == nullptr)
        {
            return TelegramConsts::NO_TOKEN;
        }

        if (hasRequest)
        {
            return TelegramConsts::BUSY;
        }

        TEL_LOGN("Sending keep-alive.");

        snprintf(fullUrl, sizeof(fullUrl), "https://%s/bot%s/getMe", TelegramConsts::host, TelegramConsts::token);
        esp_http_client_set_url(client, fullUrl);
        esp_http_client_set_method(client, HTTP_METHOD_GET);

        if (!tryConnect(0))
        {
            return TelegramConsts::CONNECTION_FAILED;
        }
        readRemaining();
        return TelegramConsts::OK;
    }

    inline TelegramConsts::TelegramStatus loop()
    {
        if (!running)
        {
            return TelegramConsts::NOT_CONNECTED;
        }

        if (WiFi.status() == WL_CONNECTED && lastWifiStatus != WL_CONNECTED)
        {
            needsKeepAlive = true;
        }

        if (!needsKeepAlive && _isConnected && !hasRequest && millis() - keepAliveTimer > keepAliveTimeout)
        {
            sendKeepAlive();
            keepAliveTimer = millis();
            return TelegramConsts::KEEP_ALIVE;
        }

        if (needsKeepAlive && millis() - keepAliveTimer > 5000)
        {
            TEL_LOGN("Wifi has reconnected, trying to reconnect to telegram.");
            for (int i = 0; i < 5; i++)
            {
                if (sendKeepAlive() == TelegramConsts::OK)
                {
                    needsKeepAlive = false;
                    break;
                }
                delay(10);
            }
            keepAliveTimer = millis();
        }

        lastWifiStatus = WiFi.status();
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