#include <Arduino.h>

// #define ESP_CONFIG_PAGE_ENABLE_LOGGING
// #define ESP32_CONFIG_PAGE_USE_ESP_IDF_OTA
#include <esp-config-page.h>
#include <WebSocketsClient.h>
#include <secrets.h>

// #define TELEGRAM_ENABLE_LOGGING
// #define TELEGRAM_LOG_DEBUG
#include <esp_http_client.h>
#include <telegram-requests-arduino.h>

#define BUTTON_PIN 3
#define SNAPSHOT_INTERVAL 15000

bool botStarted = false;
volatile bool shouldRing = false;
bool ringing = false;
unsigned long ringTimer = 0;
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
        return;
    }

    if (!hasValue(tokenVar) || !hasValue(chatIdVar))
    {
        ESP_LOGE("SNAP", "Telegram values not set.");
        return;
    }

    unsigned long startTime = millis();
    esp_err_t status = esp_http_client_perform(snapClient);
    if (status != ESP_OK)
    {
        ESP_LOGE("TELEGRAM", "Error trying to download snap.");
        esp_http_client_close(snapClient);
    }
    ESP_LOGE("SNAP", "Elapsed time: %lu", millis() - startTime);
}

void initClients()
{
    if (hasValue(tokenVar) && hasValue(chatIdVar))
    {
        TelegramRequests::init(tokenVar->value, chatIdVar->value);
    }

    if (!hasValue(cameraUrlVar))
    {
        return;
    }

    static volatile bool telegramStarted = false;
    static volatile int64_t totalWritten = 0;

    esp_http_client_config_t snapConfig = {
        .url = cameraUrlVar->value,
        .username = CAM_USER,
        .password = CAM_PASS,
        .auth_type = HTTP_AUTH_TYPE_DIGEST,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 30000,
        .event_handler = [](esp_http_client_event_t *e)
        {
            switch (e->event_id)
            {
            case HTTP_EVENT_ON_DATA:
                {
                    if (!telegramStarted)
                    {
                        totalWritten = 0;
                        int64_t length = esp_http_client_get_content_length(e->client);
                        int status = TelegramRequests::telegramStartTransaction(
                            TelegramConsts::PHOTO,
                            chatIdVar->value,
                            "fotocam.jpeg",
                            length);
                        telegramStarted = status == TelegramConsts::OK;

                        if (!telegramStarted)
                        {
                            TEL_LOGF("Error starting telegram transaction: %d\n", status);
                            lastPrintSent = millis() - SNAPSHOT_INTERVAL;
                            return ESP_FAIL;
                        }
                    }

                    totalWritten += TelegramRequests::telegramWriteData((uint8_t*) e->data, e->data_len);
                    break;
                }
            case HTTP_EVENT_ERROR:
            case HTTP_EVENT_ON_FINISH:
            case HTTP_EVENT_DISCONNECTED:
                {
                    TEL_LOGF("Snap image total written: %lld\n", totalWritten);

                    if (telegramStarted)
                    {
                        TelegramRequests::telegramEndTransaction();
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

    ESP_CONFIG_PAGE::setAPConfig("ESP32-BELLBTN", "adminadmin");
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

    ESP_CONFIG_PAGE::initModules(&httpServer, usernameVar->value, passwordVar->value, "ESP32-BELLBTN2");
    httpServer.begin();
    initClients();

    if (hasValue(bellIpVar))
    {
        webSocket.begin(bellIpVar->value, 3333);
        webSocket.setReconnectInterval(15000);
        webSocket.enableHeartbeat(5000, 15000, 10);
    }
}

void loop() {
    if (!botStarted && ESP_CONFIG_PAGE::isWiFiReady())
    {
        TelegramRequests::sendMessage("Campainha online.", chatIdVar->value);
        botStarted = true;
    }

    if (botStarted)
    {
        TelegramRequests::loop();
    }

    ESP_CONFIG_PAGE::loop();
    httpServer.handleClient();
    webSocket.loop();

    if (!ringing && shouldRing && millis() - lastRing > 1500)
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
            TelegramRequests::sendMessage("A campainha está tocando.", chatIdVar->value);
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