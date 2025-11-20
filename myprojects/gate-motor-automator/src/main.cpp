#include <Arduino.h>
#include <secrets.h>
#include <RCSwitch.h>
#include <OneButton.h>

// #define ESP_CONFIG_PAGE_ENABLE_LOGGING
#include <esp-config-page.h>

#define LED_PIN 8
#define LEARN_BUTTON_PIN 10
#define RADIO_RECV_PIN 3
#define TOGGLE_RELAY_PIN 1
#define POWER_RELAY_PIN 20
#define RELAY_ON HIGH
#define RELAY_OFF LOW
#define REMOTES_FILE "/remotes.cfg"
#define MAX_REMOTES 100

// #define EMBER_ENABLE_LOGGING
// #define EMBER_ENABLE_DEBUG_LOG
#define EMBER_CHANNEL_COUNT 10
#include <EmberIot.h>
#include <EmberIotNotifications.h>

#define TOGGLE_CH 0
#define POWER_CH 1
#define CUSTOM_CONTROLLER_ENABLE_CH 2

ESP_CONFIG_PAGE::WEBSERVER_T server(80);

EmberIot ember(
    Secrets::dbUrl,
    Secrets::deviceId,
    Secrets::emberUser,
    Secrets::emberPass,
    Secrets::webApiKey);

RCSwitch rc = RCSwitch();
OneButton learnButton(LEARN_BUTTON_PIN, true);

bool emberInit = false;
bool hasPower = false;
volatile bool customControllerEnabled = true;
unsigned long disconnectedTimer = 0;
volatile bool inLearningMode = false;
unsigned long learningModeTimer = 0;
volatile bool inDeleteMode = false;

unsigned long *remotes = nullptr;
size_t remoteCount = 0;
size_t remoteArraySize = 0;

void doToggle()
{
    HTTP_LOGN("Do toggle");
    digitalWrite(TOGGLE_RELAY_PIN, RELAY_ON);
    delay(100);
    digitalWrite(TOGGLE_RELAY_PIN, RELAY_OFF);
}

EMBER_CHANNEL_CB(TOGGLE_CH)
{
    bool isPush = prop.toButtonIsPush();
    HTTP_LOGF("Received ember toggle event with value %d\n", prop.toInt());

    if (!isPush)
    {
        return;
    }

    HTTP_LOGN("Doing toggle.");
    doToggle();
    delay(50);
    ember.channelWrite(TOGGLE_CH, EMBER_BUTTON_OFF);
}

EMBER_CHANNEL_CB(POWER_CH)
{
    HTTP_LOGF("Received ember power button event with value %d\n", prop.toInt());
    hasPower = prop.toButtonIsOn();
    digitalWrite(POWER_RELAY_PIN, hasPower ? RELAY_OFF : RELAY_ON);
    delay(200);
}

EMBER_CHANNEL_CB(CUSTOM_CONTROLLER_ENABLE_CH)
{
    HTTP_LOGF("Received enable/disable custom controller event with value: %d\n", prop.toInt());
    customControllerEnabled = prop.toButtonIsOn();
}

void toggleLearningMode(bool on)
{
    HTTP_LOGN(on ? "Learning mode on." : "Learning mode off.");
    inLearningMode = on;
    digitalWrite(LED_PIN, on ? LOW : HIGH);
    learningModeTimer = millis();
}

void toggleDeleteMode(bool on)
{
    HTTP_LOGN(on ? "Delete mode on." : "Delete mode off.");
    inDeleteMode = on;
}

void addRemote(unsigned long code)
{
    if (remoteCount >= MAX_REMOTES)
    {
        HTTP_LOGN("Max remote count reached.");
        return;
    }

    if (remoteCount > 0)
    {
        for (size_t i = 0; i < remoteCount; i++)
        {
            if (remotes[i] == code)
            {
                HTTP_LOGN("Remote already exists.");
                return;
            }
        }
    }

    if (remoteCount + 1 > remoteArraySize)
    {
        size_t lastRemoteArraySize = remoteArraySize;
        remoteArraySize = remoteArraySize == 0 ? 1 : ceil(remoteArraySize * 1.5);

        unsigned long *arr = remotes;
        remotes = (unsigned long*) realloc(remotes, sizeof(unsigned long) * remoteArraySize);
        if (remotes == nullptr)
        {
            remoteArraySize = lastRemoteArraySize;
            remotes = arr;
            HTTP_LOGN("Error allocating remotes buffer.");
            return;
        }
    }

    remotes[remoteCount] = code;
    remoteCount++;
    HTTP_LOGF("Added remote: %lu\n", code);
}

void removeRemote(unsigned long code)
{
    HTTP_LOGF("Remove remote: %lu\n", code);
    size_t removeIndex = 0;
    bool found = false;
    for (size_t i = 0; i < remoteCount; i++)
    {
        if (code == remotes[i])
        {
            removeIndex = i;
            found = true;
            break;
        }
    }

    if (!found)
    {
        return;
    }

    for (size_t i = removeIndex; i < remoteCount-1; i++)
    {
        remotes[i] = remotes[i+1];
    }

    remoteCount--;
}

void saveRemotes()
{
    if (remoteCount == 0 && LittleFS.exists(REMOTES_FILE))
    {
        LittleFS.remove(REMOTES_FILE);
        return;
    }

    File file = LittleFS.open(REMOTES_FILE, FILE_WRITE);
    for (size_t i = 0; i < remoteCount; i++)
    {
        unsigned long code = remotes[i];
        file.print(code);

        if (i < remoteCount-1)
        {
            file.print(",");
        }
    }

    file.close();
}

void loadRemotes()
{
    if (!LittleFS.exists(REMOTES_FILE))
    {
        return;
    }

    File file = LittleFS.open(REMOTES_FILE, FILE_READ);

    while (file.available())
    {
        char buf[128]{};
        file.readBytesUntil(',', buf, sizeof(buf)-1);

        if (strlen(buf) == 0)
        {
            continue;
        }

        unsigned long code = 0;
        int ret = FirePropUtil::str2ul(&code, buf, 10);
        if (ret != FirePropUtil::STR2INT_SUCCESS)
        {
            HTTP_LOGF("Error parsing code from file: %s\n", buf);
            file.close();
            return;
        }

        addRemote(code);
    }

    file.close();
}

[[noreturn]] void radioDecode(void *arg)
{
    const TickType_t interval = pdMS_TO_TICKS(10);
    TickType_t lastWake = xTaskGetTickCount();
    bool needsReset = false;
    TickType_t resetTimer = xTaskGetTickCount();

    while (true)
    {
        if (!needsReset && rc.available() && customControllerEnabled)
        {
            bool found = false;
            const unsigned long receivedCode = rc.getReceivedValue();
            LOGF("Received code: %lu\n", receivedCode);

            if (remoteCount > 0)
            {
                for (size_t i = 0; i < remoteCount; i++)
                {
                    if (receivedCode == remotes[i])
                    {
                        found = true;
                    }
                }
            }

            if (inLearningMode)
            {
                addRemote(receivedCode);
                saveRemotes();
                toggleLearningMode(false);
            }
            else if (inDeleteMode)
            {
                removeRemote(receivedCode);
                saveRemotes();
            }
            else if (found)
            {
                doToggle();
            }

            needsReset = true;
            resetTimer = xTaskGetTickCount();
        }

        if ((xTaskGetTickCount() - resetTimer) > pdMS_TO_TICKS(800))
        {
            needsReset = false;
            rc.resetAvailable();
        }

        vTaskDelayUntil(&lastWake, interval);
    }
}

[[noreturn]] void buttonTick(void *arg)
{
    const TickType_t interval = pdMS_TO_TICKS(2);
    TickType_t lastWake = xTaskGetTickCount();

    while (true)
    {
        learnButton.tick();
        vTaskDelayUntil(&lastWake, interval);
    }
}

void learnButtonClick()
{
    toggleLearningMode(!inLearningMode);
}

void learnButtonDuringLongPress()
{
    if (!inDeleteMode)
    {
        digitalWrite(LED_PIN, LOW);
        toggleDeleteMode(true);
    }
}

void learnButtonLongPress()
{
    digitalWrite(LED_PIN, HIGH);
    toggleDeleteMode(false);
}

void setup() {
#ifdef ESP_CONFIG_PAGE_ENABLE_LOGGING
    Serial.begin(115200);
#endif

    LOGN("Trying to mount LittleFS.");
    if (!LittleFS.begin(false /* false: Do not format if mount failed */))
    {
        LOGN("Failed to mount LittleFS");
        if (!LittleFS.begin(true /* true: format */))
        {
            LOGN("Failed to format LittleFS");
        }
        else
        {
            LOGN("LittleFS formatted successfully");
            ESP.restart();
        }
    }

    pinMode(TOGGLE_RELAY_PIN, OUTPUT);
    pinMode(POWER_RELAY_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);

    digitalWrite(TOGGLE_RELAY_PIN, RELAY_OFF);
    digitalWrite(POWER_RELAY_PIN, RELAY_OFF);
    digitalWrite(LED_PIN, HIGH);

    learnButton.attachClick(learnButtonClick);
    learnButton.attachDuringLongPress(learnButtonDuringLongPress);
    learnButton.attachLongPressStop(learnButtonLongPress);
    learnButton.setPressMs(5000);
    learnButton.setDebounceMs(25);
    xTaskCreate(buttonTick, "buttonTick", 2048, nullptr, 3, nullptr);

    delay(1000);

    loadRemotes();
    pinMode(RADIO_RECV_PIN, INPUT);
    rc.enableReceive(digitalPinToInterrupt(RADIO_RECV_PIN));
    xTaskCreate(radioDecode, "radioDecodeTask", 4096, nullptr, 5, nullptr);

    ESP_CONFIG_PAGE::setAPConfig("ESP_GATE_AUTOMATOR", "admin13246");
    ESP_CONFIG_PAGE::initModules(&server, Secrets::username, Secrets::password, "ESP_GATE_AUTOMATOR");

    server.begin();

    LOGN("Started");
}

void loop() {
    server.handleClient();
    ESP_CONFIG_PAGE::loop();

    if (WiFi.status() == WL_CONNECTED)
    {
        disconnectedTimer = millis();

        if (!emberInit)
        {
            ember.init();
            ember.channelWrite(TOGGLE_CH, EMBER_BUTTON_OFF),
            emberInit = true;
        }

        ember.loop();
    }
    else if (millis() - disconnectedTimer > 180000)
    {
        ESP.restart();
    }

    if (inLearningMode && millis() - learningModeTimer > 10000)
    {
        toggleLearningMode(false);
    }
}