#include <Arduino.h>
#include <NewPing.h>
#include <esp-config-page.h>
#include <capacitance.h>
#include <secrets.h>

#define EMBER_ENABLE_DEBUG_LOG
#define EMBER_ENABLE_LOGGING
#define EMBER_CHANNEL_COUNT 5
#include <EmberIot.h>
#include <EmberIotNotifications.h>

#define TRIGGER_PIN 8
#define ECHO_PIN 7
#define PUMP_PIN 1
#define PING_DELAY_MS 30
#define PING_READINGS 30
#define PING_IGNORE_OUTLIERS 7
#define MAX_PUMP_ON_TIME 200
#define MIN_PUMP_ON_TIME 100

#define EMBER_BOARD_ID 0
#define POWER_CHANNEL 0
#define MAX_DIST_CHANNEL 1
#define MAX_PUMP_POWER_CHANNEL 3
#define MIN_PUMP_POWER_CHANNEL 4
#define STATUS_CHANNEL 5

#define MIN_DISTANCE_CM 10

enum Status
{
    SQUIRT_SCANNING = 0,
    SQUIRT_WATER_LEVEL_LOW = 1,
    SQUIRT_SCAN_ERROR = 2,
};

Status currentStatus = SQUIRT_SCANNING;

const char *deviceName = "Esguichar";
constexpr char boardName[] = "ESP-SQUIRTER-1";
constexpr char apPass[] = "adminadmin";

ESP_CONFIG_PAGE::EnvVar* user = new ESP_CONFIG_PAGE::EnvVar("USER", "");
ESP_CONFIG_PAGE::EnvVar* pass = new ESP_CONFIG_PAGE::EnvVar("PASSWORD", "");
ESP_CONFIG_PAGE::EnvVar* rtdbUrl = new ESP_CONFIG_PAGE::EnvVar("RTDB_URL", "");
ESP_CONFIG_PAGE::EnvVar* emberUser = new ESP_CONFIG_PAGE::EnvVar("EMBER_USER", "");
ESP_CONFIG_PAGE::EnvVar* emberPassword = new ESP_CONFIG_PAGE::EnvVar("EMBER_PASSWORD", "");
ESP_CONFIG_PAGE::EnvVar* webapiKey = new ESP_CONFIG_PAGE::EnvVar("EMBER_WEBAPI_KEY", "");
ESP_CONFIG_PAGE::EnvVar* deviceId = new ESP_CONFIG_PAGE::EnvVar("EMBER_DEVICE_ID", "");

NewPing sonar(TRIGGER_PIN, ECHO_PIN, 200);
unsigned long lastPing;
unsigned long pingReadings[PING_READINGS] = {};
uint8_t currentReading = 0;

unsigned long squirtTimeout = 0;
uint8_t successiveSquirtCount = 0;
unsigned long successiveSquirtResetTimer = 0;

ESP_CONFIG_PAGE::WEBSERVER_T server(80);
EmberIot* ember = nullptr;
FCMEmberNotifications emberNotification(gcmEmail, gcmPrivateKey);
ESP_CONFIG_PAGE_LOGGING::SERIAL_T webserial;

bool isOn = true;

unsigned long lastSentNotification = 0;
bool hasSquirt = false;
unsigned long pumpTimer = 0;
unsigned long lastWaterLevelUpdate = 0;

int pumpOnTime = 100;
int minPower = 1;
int maxPower = 50;
int triggerDistanceCm = 45;
unsigned long timesReadingZero = 0;

// On/off
EMBER_CHANNEL_CB(0)
{
    int val = prop.toInt();
    if (val == EMBER_BUTTON_ON)
    {
        isOn = true;
    }
    else
    {
        isOn = false;
        ledcWrite(PUMP_PIN, 0);
    }
}

// Max distance cm
EMBER_CHANNEL_CB(1)
{
    double val = prop.toDouble();
    if (val > 0 && val < 200)
    {
        triggerDistanceCm = val;
    }
}

// Max pump power
EMBER_CHANNEL_CB(3)
{
    int val = prop.toInt();
    if (val > 0 && val <= 100)
    {
        maxPower = val;
    }
}

// Min pump power
EMBER_CHANNEL_CB(4)
{
    int val = prop.toInt();
    if (val > 0 && val <= 100)
    {
        minPower = val;
    }
}

bool hasValue(const char* str)
{
    return str != nullptr && strlen(str) > 0;
}

int getPumpPower(unsigned long distanceCm)
{
    if (minPower >= maxPower)
    {
        return maxPower;
    }

    pumpOnTime = constrain(map(distanceCm, MIN_DISTANCE_CM, triggerDistanceCm, MIN_PUMP_ON_TIME, MAX_PUMP_ON_TIME), MIN_PUMP_ON_TIME, MAX_PUMP_ON_TIME);
    return constrain(map(map(distanceCm, MIN_DISTANCE_CM, triggerDistanceCm, minPower, maxPower), 0, 100, 10, 255), 10, 255);
}

int compareULong(const void* a, const void* b) {
    unsigned long valA = *(unsigned long*)a;
    unsigned long valB = *(unsigned long*)b;

    if (valA < valB) return -1;
    if (valA > valB) return 1;
    return 0;
}

void setup()
{
    EmberIotShared::timezoneOffsetSeconds = -(3600*3);

#ifdef ENABLE_LOGGING
    Serial.begin(115200);
#endif

    delay(1500);

    ESP_CONFIG_PAGE::setSerial(&webserial);
    ESP_CONFIG_PAGE::setAPConfig(boardName, apPass);
    ESP_CONFIG_PAGE::tryConnectWifi(false, 15000);

    ESP_CONFIG_PAGE::addEnvVar(user);
    ESP_CONFIG_PAGE::addEnvVar(pass);
    ESP_CONFIG_PAGE::addEnvVar(rtdbUrl);
    ESP_CONFIG_PAGE::addEnvVar(emberUser);
    ESP_CONFIG_PAGE::addEnvVar(emberPassword);
    ESP_CONFIG_PAGE::addEnvVar(webapiKey);
    ESP_CONFIG_PAGE::addEnvVar(deviceId);
    ESP_CONFIG_PAGE::setAndUpdateEnvVarStorage(new ESP_CONFIG_PAGE::LittleFSKeyValueStorage("/envfinal"));

    ESP_CONFIG_PAGE::initModules(&server, user->value, pass->value, boardName);
    server.begin();

    ESP_CONFIG_PAGE_LOGGING::enableLogging(user->value, pass->value, webserial);

    if (hasValue(rtdbUrl->value) &&
        hasValue(emberUser->value) &&
        hasValue(emberPassword->value) &&
        hasValue(webapiKey->value) &&
        hasValue(deviceId->value))
    {
        ember = new EmberIot(rtdbUrl->value,
                             deviceId->value,
                             emberUser->value,
                             emberPassword->value,
                             webapiKey->value);
        ember->init();
        // ember->channelWrite(POWER_CHANNEL, isOn ? EMBER_BUTTON_ON : EMBER_BUTTON_OFF);
    }

    emberNotification.init(userUid);
    emberNotification.send(deviceName, "Esguichador online.");

    ledcSetup(PUMP_PIN, 500, 8);
    ledcAttachPin(PUMP_PIN, PUMP_PIN);
    ledcWrite(PUMP_PIN, 0);
    LOGN("Started");
    lastPing = millis();
}

void loop()
{
    if (hasSquirt && millis() - pumpTimer > pumpOnTime)
    {
        ledcWrite(PUMP_PIN, 0);
        hasSquirt = false;
        squirtTimeout = millis();
    }

    if (hasSquirt)
    {
        return;
    }

    server.handleClient();
    ESP_CONFIG_PAGE::loop();
    ESP_CONFIG_PAGE_LOGGING::loop();

    if (WiFi.status() == WL_CONNECTED)
    {
        emberNotification.loop();

        if (ember != nullptr)
        {
            ember->loop();
        }
    }

    if (!isOn)
    {
        return;
    }

    if (millis() - lastPing > PING_DELAY_MS) {
        portENTER_CRITICAL(&capacitanceSpinlock);
        unsigned long ping = sonar.ping_cm();
        portEXIT_CRITICAL(&capacitanceSpinlock);

        pingReadings[currentReading] = ping;
        currentReading++;

        if (currentReading >= PING_READINGS)
        {
            qsort(pingReadings, PING_READINGS, sizeof(unsigned long), compareULong);

            unsigned long average = 0;
            for (size_t i = PING_IGNORE_OUTLIERS; i < PING_READINGS-PING_IGNORE_OUTLIERS; i++)
            {
                average = average + pingReadings[i];
            }
            average = average / (PING_READINGS-(PING_IGNORE_OUTLIERS*2));

            LOG("Average: ");
            LOG(average);
            LOGN("cm");

            if (average == 0)
            {
                timesReadingZero++;
            }

            if (average == 0 && timesReadingZero >= 20)
            {
                LOGN("Error reading distance.");
                if (currentStatus != SQUIRT_SCAN_ERROR)
                {
                    currentStatus = SQUIRT_SCAN_ERROR;
                    ember->channelWrite(STATUS_CHANNEL, SQUIRT_SCAN_ERROR);
                    emberNotification.send(deviceName, "Erro ao realizar leitura do sensor ultrassônico.");
                }
            }
            else if (average < triggerDistanceCm) {
                timesReadingZero = 0;
                LOGN("Squirt set.");
                ledcWrite(PUMP_PIN, getPumpPower(average < MIN_DISTANCE_CM ? MIN_DISTANCE_CM : average));

                hasSquirt = true;
                pumpTimer = millis();
                squirtTimeout = millis();

                successiveSquirtCount++;
                successiveSquirtResetTimer = millis();

                if (millis() - lastSentNotification > 10000)
                {
                    char buf[128];
                    sprintf(buf, "Objeto detectado a %ldcm, esguichando!", average);
                    emberNotification.send(deviceName, buf);
                    lastSentNotification = millis();
                }

                if (currentStatus == SQUIRT_SCAN_ERROR)
                {
                    ember->channelWrite(STATUS_CHANNEL, SQUIRT_SCANNING);
                }
            }

            currentReading = 0;
        }

        lastPing = millis();
    }
}
