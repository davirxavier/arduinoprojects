#pragma once
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

namespace TelegramRequest
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

    inline esp_http_client_handle_t telegramClient;
    constexpr char multipartBoundary[] = "ESP_BELL_BTN";
    constexpr char multipartContentTypeHeader[] = "multipart/form-data; boundary=ESP_BELL_BTN";

    constexpr char telegramHost[] = "api.telegram.org";
    inline char telegramUrl[256] = "https://api.telegram.org";

    inline bool isFileRequest(TelegramRequestType t)
    {
        return t != MESSAGE;
    }

    inline void initClient(int rxBufferSize = 512, int txBufferSize = 4096)
    {
        esp_http_client_config_t telegramConfig = {
            .url = telegramUrl,
            .method = HTTP_METHOD_POST,
            .timeout_ms = 10000,
            .buffer_size = rxBufferSize,
            .buffer_size_tx = txBufferSize,
            .crt_bundle_attach = esp_crt_bundle_attach,
            .keep_alive_enable = true,
        };
        telegramClient = esp_http_client_init(&telegramConfig);
        esp_http_client_set_header(telegramClient, "Content-Type", multipartContentTypeHeader);
    }

    inline bool telegramStartSending(
        const char *token,
        const char *chatId,
        const TelegramRequest::TelegramRequestType type,
        const char* data,
        const size_t dataSize = 0)
    {
        if (token == nullptr || strlen(token) == 0 || chatId == nullptr || strlen(chatId) == 0)
        {
            return false;
        }

        const char* pathName = TelegramRequest::requestPathByType[type];
        const char* fieldName = TelegramRequest::requestFieldByType[type];

        char fullDispositionHeader[256]{};
        if (TelegramRequest::isFileRequest(type))
        {
            snprintf(fullDispositionHeader, sizeof(fullDispositionHeader),
                     "--%s\r\nContent-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n\r\n",
                     multipartBoundary,
                     fieldName,
                     data);
        }
        else
        {
            snprintf(fullDispositionHeader, sizeof(fullDispositionHeader),
                     "--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n",
                     multipartBoundary,
                     fieldName);
        }

        int contentLength = strlen(fullDispositionHeader) +
            strlen(multipartBoundary) +
            (type == TelegramRequest::MESSAGE ? strlen(data) : dataSize) +
            6;

        ESP_LOGI("TELEGRAM", "Calculated content length is: %d\n", contentLength);

        snprintf(telegramUrl, sizeof(telegramUrl), "https://%s/bot%s/%s?chat_id=%s",
            telegramHost,
            token,
            pathName,
            chatId);
        ESP_LOGI("TELEGRAM", "Telegram url: %s\n", telegramUrl);
        esp_http_client_set_url(telegramClient, telegramUrl);

        if (esp_http_client_open(telegramClient, contentLength) != ESP_OK)
        {
            ESP_LOGI("TELEGRAM", "Error opening connection to telegram.");
            return false;
        }

        esp_http_client_write(telegramClient, fullDispositionHeader, strlen(fullDispositionHeader));
        return true;
    }

    inline size_t telegramSendData(const char* data, int dataSize)
    {
        return esp_http_client_write(telegramClient, data, dataSize);
    }

    inline void telegramEnd(bool reuseCon = false)
    {
        esp_http_client_write(telegramClient, "\r\n--", 4);
        esp_http_client_write(telegramClient, multipartBoundary, strlen(multipartBoundary));
        esp_http_client_write(telegramClient, "--", 2);

        // int64_t responseContentLength = esp_http_client_fetch_headers(telegramClient);
        // if (responseContentLength < 0)
        // {
        //     ESP_LOGI("TELEGRAM", "Error fetching telegram response headers: %lld\n", responseContentLength);
        //     esp_http_client_close(telegramClient);
        //     return;
        // }

        // int status = esp_http_client_get_status_code(telegramClient);
        // ESP_LOGI("TELEGRAM", "Upload to telegram finished, status: %d\n", status);
        //
        // if (status < 200 || status >= 300)
        // {
        //     ESP_LOGI("TELEGRAM", "Error uploading to telegram:");
        //     char buf[512];
        //     int read = esp_http_client_read(telegramClient, buf, sizeof(buf));
        //     buf[read < sizeof(buf) ? read : sizeof(buf) - 1] = 0;
        //     ESP_LOGI("TELEGRAM", "%s", buf);
        // }

        if (!reuseCon)
        {
            esp_http_client_close(telegramClient);
        }
    }

    inline bool telegramSendMessage(const char* message, const char *token, const char *chatId, bool reuseCon = false)
    {
        if (!telegramStartSending(token, chatId, TelegramRequest::MESSAGE, message))
        {
            return false;
        }

        telegramSendData(message, strlen(message));
        telegramEnd(reuseCon);
        return true;
    }

#ifdef ESP_ARDUINO_VERSION
    inline bool telegramSendFile(
        const char *fileName,
        Stream &file,
        size_t fileSize,
        TelegramRequestType fileType,
        const char *token,
        const char *chatId,
        const size_t bufSize = 512)
    {
        if (!isFileRequest(fileType))
        {
            return false;
        }

        if (!telegramStartSending(token, chatId, TelegramRequest::MESSAGE, fileName, fileSize))
        {
            return true;
        }

        size_t readLen = 0;
        uint8_t buf[bufSize];
        while ((readLen = file.readBytes(buf, bufSize)) > 0)
        {
            telegramSendData((char*) buf, readLen);
        }

        telegramEnd();
        return true;
    }
#endif
}
