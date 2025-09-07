#include <Arduino.h>
#include <esp-config-page.h>
#include <WebSocketsClient.h>
#include <secrets.h>
#include <telegram-requests.h>
#include "esp_http_client.h"

#define BUTTON_PIN 2
#define SNAPSHOT_INTERVAL 15000

bool botStarted = false;
volatile bool shouldRing = false;
bool ringing = false;
unsigned long ringTimer = 0;
bool isOn = true;
unsigned long lastRing = 0;
unsigned long lastMessageSent = 0;
unsigned long lastPrintSent = 0 - 15000;

ESP_CONFIG_PAGE::WEBSERVER_T httpServer(80);

auto usernameVar = new ESP_CONFIG_PAGE::EnvVar("USERNAME", "");
auto passwordVar = new ESP_CONFIG_PAGE::EnvVar("PASSWORD", "");
auto tokenVar = new ESP_CONFIG_PAGE::EnvVar("TOKEN", "");
auto bellIpVar = new ESP_CONFIG_PAGE::EnvVar("BELL_IP", "");
auto cameraUrlVar = new ESP_CONFIG_PAGE::EnvVar("CAM_URL", "");
auto chatIdVar = new ESP_CONFIG_PAGE::EnvVar("CHAT_ID", "");

WebSocketsClient webSocket;

esp_http_client_handle_t snapClient;
unsigned telegramOperationCounter = 0;

bool hasValue(const ESP_CONFIG_PAGE::EnvVar *var)
{
    return var->value != nullptr && strlen(var->value) > 0;
}

void sendCamPhoto()
{
    if (!hasValue(cameraUrlVar))
    {
        ESP_LOGI("SNAP", "Cam url not set.");
        return;
    }

    ESP_LOGI("SNAP", "Starting snap download.");
    telegramOperationCounter = millis();

    esp_err_t status = esp_http_client_perform(snapClient);
    TelegramRequest::telegramEnd(status == ESP_OK);
    if (status != ESP_OK)
    {
        ESP_LOGI("TELEGRAM", "Error trying to download snap.");
        esp_http_client_close(snapClient);
        return;
    }
    ESP_LOGI("SNAP", "Snap total time taken: %lu", millis() - telegramOperationCounter);
}

void initClients()
{
    TelegramRequest::initClient();
    if (!hasValue(cameraUrlVar))
    {
        return;
    }

    static volatile bool telegramStarted = false;

    esp_http_client_config_t snapConfig = {
        .url = cameraUrlVar->value,
        .username = CAM_USER,
        .password = CAM_PASS,
        .auth_type = HTTP_AUTH_TYPE_DIGEST,
        .method = HTTP_METHOD_GET,
        .event_handler = [](esp_http_client_event_t *e)
        {
            switch (e->event_id)
            {
            case HTTP_EVENT_ON_DATA:
                {
                    if (!telegramStarted)
                    {
                        int64_t length = esp_http_client_get_content_length(e->client);
                        telegramStarted = TelegramRequest::telegramStartSending(
                            tokenVar->value,
                            chatIdVar->value,
                            TelegramRequest::PHOTO,
                            "fotocam.jpeg",
                            length);

                        if (!telegramStarted)
                        {
                            lastPrintSent = millis() - SNAPSHOT_INTERVAL;
                            return ESP_FAIL;
                        }
                    }

                    TelegramRequest::telegramSendData((char*) e->data, e->data_len);
                    break;
                }
            case HTTP_EVENT_ERROR:
            case HTTP_EVENT_ON_FINISH:
                {
                    if (telegramStarted)
                    {
                        telegramStarted = false;
                    }
                    break;
                }
            default: break;
            }
            return ESP_OK;
        },
        .buffer_size = 4096,
        .buffer_size_tx = 512,
    };
    snapClient = esp_http_client_init(&snapConfig);
}

void setup() {
    delay(1500);
    Serial.begin(115200);

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    ESP_CONFIG_PAGE::setAPConfig("ESP32-BELLBTN", "");
    ESP_CONFIG_PAGE::tryConnectWifi(false, 20000);

    ESP_CONFIG_PAGE::addEnvVar(usernameVar);
    ESP_CONFIG_PAGE::addEnvVar(passwordVar);
    ESP_CONFIG_PAGE::addEnvVar(chatIdVar);
    ESP_CONFIG_PAGE::addEnvVar(tokenVar);
    ESP_CONFIG_PAGE::addEnvVar(bellIpVar);
    ESP_CONFIG_PAGE::addEnvVar(cameraUrlVar);
    ESP_CONFIG_PAGE::setAndUpdateEnvVarStorage(new ESP_CONFIG_PAGE::LittleFSKeyValueStorage("/envfinal.txt"));

    ESP_CONFIG_PAGE::addCustomAction("TEST", [](ESP_CONFIG_PAGE::WEBSERVER_T &server)
    {
        server.send(200);
        shouldRing = true;
    });

    ESP_CONFIG_PAGE::addCustomAction("ESP-IDF TEST", [](ESP_CONFIG_PAGE::WEBSERVER_T &server)
    {
        server.send(200);
        sendCamPhoto();
    });

    ESP_CONFIG_PAGE::initModules(&httpServer, usernameVar->value, passwordVar->value, "ESP-BELLBTN1");
    httpServer.begin();
    initClients();

    if (hasValue(bellIpVar))
    {
        webSocket.begin(bellIpVar->value, 3333);
        webSocket.setReconnectInterval(1000);
        webSocket.enableHeartbeat(5000, 15000, 10);
    }
}

void loop() {
    if (!botStarted && ESP_CONFIG_PAGE::isWiFiReady())
    {
        TelegramRequest::telegramSendMessage("A campainha está online.", tokenVar->value, chatIdVar->value);
        botStarted = true;
    }

    ESP_CONFIG_PAGE::loop();
    httpServer.handleClient();
    webSocket.loop();

    if (!isOn)
    {
        shouldRing = false;
    }

    if (!ringing && shouldRing && isOn && millis() - lastRing > 1500)
    {
        ringing = true;

        if (webSocket.isConnected())
        {
            char buf[strlen(usernameVar->value)+strlen(passwordVar->value)+8];
            buf[0] = 0;
            snprintf(buf, sizeof(buf), "r:%s:%s", usernameVar->value, passwordVar->value);
            webSocket.sendTXT(buf);
        }
        else
        {
            ESP_LOGI("BELL", "Websocket is not connected.");
        }

        if (millis() - lastMessageSent > 3000)
        {
            TelegramRequest::telegramSendMessage("A campainha está tocando.", tokenVar->value, chatIdVar->value, true);
            lastMessageSent = millis();
        }

        if (millis() - lastPrintSent > SNAPSHOT_INTERVAL)
        {
            sendCamPhoto();
            lastPrintSent = millis();
        }

        shouldRing = false;
        ringTimer = millis();
        lastRing = millis();
    }

    if (ringing && millis() - ringTimer > 2000)
    {
        ringing = false;
    }

    if (digitalRead(BUTTON_PIN) == LOW)
    {
        shouldRing = true;
    }
}