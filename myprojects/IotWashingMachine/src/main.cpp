#define BLYNK_TEMPLATE_ID "TMPL2l3dLawc_"
#define BLYNK_TEMPLATE_NAME "Máquina de Lavar"

#include <Arduino.h>
#include "defines.h"

#ifdef ESP8266
#include <Hash.h>
#endif

#ifdef ESP32
#include <esp-config-page-logging.h>
#endif

#include <esp-config-page.h>

#define MODE_PIN V0
#define POWER_PIN V1
#define PROGRESS_PIN V2
#define STAGE_PIN V3
#define ACTION_PIN V4

#define BLYNK_PRINT Serial

#include <BlynkSimpleEsp32.h>
#include <TouchyTouch.h>
#include <buzzer.h>

#define LOG(s) Serial.print(s)
#define LOGN(s) Serial.println(s)
#define LOGF(s, p...) Serial.printf(s, p)
#define VW(p, v...) LOGF("Virtual write for pin %d.\n", p); if (blinkReady) Blynk.virtualWrite(p, v)

#define DATA_FILE "/saved.txt"
#define ON_VALUE HIGH
#define OFF_VALUE LOW
#define MODULE_POWER_CHANGE_DELAY 15000

// TOOD update pins
#define WATER_VALVE_PIN 2
#define DRAIN_PUMP_PIN 1
#define MOTOR_PIN 10
#define DIRECTION_PIN 0
#define WATER_LEVEL_SENSE_PIN 4
#define TOUCH_BUTTON_PIN 3
#define BEEPER_PIN 9

ESP_CONFIG_PAGE::WEBSERVER_T webserver(80);

ESP_CONFIG_PAGE::EnvVar *webpageUser = new ESP_CONFIG_PAGE::EnvVar("username", "admin");
ESP_CONFIG_PAGE::EnvVar *webpagePassword = new ESP_CONFIG_PAGE::EnvVar("password", "admin");
ESP_CONFIG_PAGE::EnvVar *blynkAuthKey = new ESP_CONFIG_PAGE::EnvVar("blynk_auth_key", nullptr);

TouchyTouch touchButton;
unsigned long lastTouchPressedTimer = 0;
unsigned int touchPressedCounter = 0;

DxBuzzer buzzer(BEEPER_PIN);
uint8_t beepsPerMode[] = {1, 2, 3, 4};

const char *nodeName = "ESP-WM-01";
const char *apPass = "admin123";

bool blinkInit = false;
bool blinkReady = false;
bool isRunning = false;
bool powerLoss = false;

Data currentData;
Stage::Stage validStagesForSkip[] = {Stage::WASH_SHAKING, Stage::RINSE_SHAKING, Stage::RINSE_SOAKING, Stage::DRY_SPINNING, Stage::RINSE_SPINNING};
unsigned int validStagesCount = 5;

unsigned long shakeSoakStartTime = 0;
unsigned long motorTimeCounter = 0;
bool spinning = false;
bool direction = false;

unsigned long modulePowerChangeTimer = 0;
bool changeModulePowerQueued = false;
Module::Module currentModuleChange;

unsigned long spinTime = 3000;
unsigned long spinPauseTime = 3000;

uint8_t rinseSpinMinutes = 2;
uint8_t agitateMinutesByMode[] = {5, 10, 20, 30, 0};
uint8_t agitateRinseMinutesByMode[] = {3, 5, 5, 5, 0};
uint8_t rinseSoakMinutesByMode[] = {0, 0, 3, 5, 0};
uint8_t rinseCyclesByMode[] = {0, 1, 2, 3, 0};
uint8_t dryMinutesByMode[] = {5, 7, 7, 10, 0};

volatile unsigned int waterLevelSignalCounter = 0;
unsigned long waterLevelSignalTimer = 0;
bool waterLevelReached = false;
unsigned long waterLevelReachedTime = 0;

void IRAM_ATTR waterLevelSignalReceived()
{
    waterLevelSignalCounter++;
}

bool isWaterFilled()
{
    return waterLevelReached && millis() - waterLevelReachedTime > MODULE_POWER_CHANGE_DELAY;
}

bool isWaterDrained()
{
    return !waterLevelReached && millis() - waterLevelReachedTime > MODULE_POWER_CHANGE_DELAY;
}

void turnOffAllModules()
{
    LOGN("Turn all modules off.");
    digitalWrite(WATER_VALVE_PIN, OFF_VALUE);
    digitalWrite(MOTOR_PIN, OFF_VALUE);
    digitalWrite(DRAIN_PUMP_PIN, OFF_VALUE);
    digitalWrite(DIRECTION_PIN, OFF_VALUE);
}

void changeModulePower(Module::Module module, bool on)
{
    turnOffAllModules();

    if (!on)
    {
        return;
    }

    LOGF("Start module %d power up process.\n", module);
    modulePowerChangeTimer = millis();
    changeModulePowerQueued = true;
    currentModuleChange = module;
}

void saveData()
{
    LOGN("Saving data to flash.");
    unsigned int size = sizeof(Data);
    uint8_t bytes[size];
    memcpy(bytes, &currentData, size);

    File file = LittleFS.open(DATA_FILE, "w");
    file.write(bytes, size);
    file.close();
}

void recoverData()
{
    LOGN("Trying to recover data from flash.");
    if (!LittleFS.exists(DATA_FILE))
    {
        LOGN("Data file not found.");
        return;
    }

    File file = LittleFS.open(DATA_FILE, "r");

    const unsigned int size = file.size();
    uint8_t bytes[size];
    file.read(bytes, size);

    file.close();

    memcpy(&currentData, bytes, size);
}

void updateStates(States::States stateChange, Stage::Stage newStage = Stage::OFF)
{
    if (stateChange == States::MODE && currentData.currentStage != Stage::OFF)
    {
        VW(MODE_PIN, currentData.currentMode);
        return;
    }

    switch (stateChange)
    {
    case States::POWER:
        {
            if (isRunning)
            {
                currentData.currentStage = Stage::WASH_FILLING_UP;
            }
            else
            {
                currentData.currentStage = Stage::OFF;
            }

            currentData.currentRinse = 0;
            VW(STAGE_PIN, currentData.currentStage);
            break;
        }
    case States::MODE:
        {
            break;
        }
    case States::STAGE:
        {
            switch (newStage)
            {
            case Stage::WASH_FILLING_UP:
                {
                    changeModulePower(Module::WATER_VALVE, true);
                    break;
                }
            case Stage::WASH_SHAKING:
                {
                    break;
                }
            case Stage::WASH_DRAINING:
                {
                    changeModulePower(Module::DRAIN_PUMP, true);
                    break;
                }
            case Stage::RINSE_FILLING_UP:
                {
                    changeModulePower(Module::WATER_VALVE, true);
                    currentData.currentRinse++;
                    break;
                }
            case Stage::RINSE_SOAKING:
                {
                    turnOffAllModules();
                    break;
                }
            case Stage::RINSE_SHAKING:
                {
                    break;
                }
            case Stage::RINSE_DRAINING:
                {
                    changeModulePower(Module::DRAIN_PUMP, true);
                    break;
                }
            case Stage::RINSE_SPINNING:
                {
                    break;
                }
            case Stage::DRY_SPINNING:
                {
                    break;
                }
            case Stage::OFF:
                {
                    if (blinkReady)
                    {
                        Blynk.logEvent(Events::EVENT_IDS[Events::CYCLE_ENDED]);
                    }

                    turnOffAllModules();
                    currentData.currentRinse = 0;
                    buzzer.beep(3000);
                    isRunning = false;
                    break;
                }
            }

            currentData.currentStage = newStage;
            if (currentData.currentStage != newStage)
            {
                VW(STAGE_PIN, newStage);
            }
            break;
        }
    }

    saveData();
}

void skipStage()
{
    bool found = false;
    for (unsigned int i = 0; i < validStagesCount; i++)
    {
        if (currentData.currentStage == validStagesForSkip[i])
        {
            found = true;
            break;
        }
    }

    if (!found)
    {
        return;
    }

    LOGN("Skipping stage.");
    updateStates(States::STAGE, static_cast<Stage::Stage>(currentData.currentStage+1));
}

bool changeMode(Mode::Mode newMode)
{
    if (currentData.currentStage != Stage::OFF)
    {
        return false;
    }

    currentData.currentMode = newMode;
    return true;
}

// On/off
BLYNK_WRITE(POWER_PIN)
{
    auto power = static_cast<BooleanEnum::Boolean>(param.asInt());
    isRunning = power == BooleanEnum::TRUE;
    LOGF("Power state change: %s.\n", isRunning ? "on" : "off");
    updateStates(States::POWER);
}

// Mode
BLYNK_WRITE(MODE_PIN)
{
    if (!changeMode(static_cast<Mode::Mode>(param.asInt())))
    {
        VW(MODE_PIN, currentData.currentMode);
    }

    LOGF("Mode state change: %d.\n", currentData.currentMode);
    updateStates(States::MODE);
}

// Action
BLYNK_WRITE(ACTION_PIN)
{
    auto action = static_cast<Actions::Actions>(param.asInt());
    LOGF("Action triggered: %d.\n", action);
    if (action == Actions::SKIP_STAGE)
    {
        skipStage();
    }
}

void spinAlternating()
{
    unsigned long currentMillis = millis();

    if (!spinning && currentMillis - motorTimeCounter > spinPauseTime)
    {
        spinning = true;
        direction = !direction;
        digitalWrite(DIRECTION_PIN, direction ? HIGH : LOW);
        changeModulePower(Module::MOTOR, true);
    }

    if (spinning && currentMillis - motorTimeCounter > spinTime)
    {
        spinning = false;
        motorTimeCounter = currentMillis;
        turnOffAllModules();
    }
}

void setup()
{
    delay(2000);
    Serial.begin(115200);
    LOGN("Initializing.");

    touchButton.begin(TOUCH_BUTTON_PIN, 30);
    pinMode(WATER_LEVEL_SENSE_PIN, INPUT_PULLUP);
    attachInterrupt(WATER_LEVEL_SENSE_PIN, waterLevelSignalReceived, FALLING);

    pinMode(WATER_VALVE_PIN, OUTPUT);
    pinMode(MOTOR_PIN, OUTPUT);
    pinMode(DIRECTION_PIN, OUTPUT);
    pinMode(DRAIN_PUMP_PIN, OUTPUT);
    turnOffAllModules();

    buzzer.beep(800);

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

    recoverData();
    LOGF("Current stage: %d\n", currentData.currentStage);

    if (currentData.currentStage != Stage::OFF)
    {
        LOGN("Power loss in last operation detected.");
        powerLoss = true;
    }

    LOGN("Setting up config page.");

    ESP_CONFIG_PAGE::setAPConfig(nodeName, "admin1234");
    ESP_CONFIG_PAGE::tryConnectWifi(false, 5000);

    ESP_CONFIG_PAGE::addEnvVar(webpageUser);
    ESP_CONFIG_PAGE::addEnvVar(webpagePassword);
    ESP_CONFIG_PAGE::addEnvVar(blynkAuthKey);
    ESP_CONFIG_PAGE::setAndUpdateEnvVarStorage(new ESP_CONFIG_PAGE::LittleFSKeyValueStorage("/env"));

    ESP_CONFIG_PAGE::initModules(&webserver, webpageUser->value, webpagePassword->value, nodeName);

    ESP_CONFIG_PAGE::addCustomAction("RESTART", [](ESP_CONFIG_PAGE::WEBSERVER_T &webserver)
    {
        ESP.restart();
        return true;
    });

    webserver.begin();
    LOGN("Config page set up.");
}

void loop()
{
    webserver.handleClient();
    ESP_CONFIG_PAGE::loop();
    touchButton.update();

    if (millis() - waterLevelSignalTimer > 1000)
    {
        bool newWaterLevelReached = waterLevelSignalCounter > 10;

        if (newWaterLevelReached || (!newWaterLevelReached && waterLevelReached))
        {
            waterLevelReachedTime = millis();
        }

        waterLevelReached = newWaterLevelReached;
        waterLevelSignalCounter = 0;
        waterLevelSignalTimer = millis();
    }

    if (ESP_CONFIG_PAGE::isWiFiReady() && !blinkInit && blynkAuthKey->value != nullptr)
    {
        LOGN("Connected to wireless network successfully, initializing Blynk.");
        Blynk.config(blynkAuthKey->value);
        Blynk.connect();
        blinkInit = true;
    }

    if (blinkInit && !blinkReady && Blynk.connected())
    {
        LOGN("Blynk init successful.");

        if (powerLoss)
        {
            VW(POWER_PIN, BooleanEnum::FALSE);
            VW(STAGE_PIN, currentData.currentStage);
            powerLoss = false;
        }

        blinkReady = true;
    }

    if (blinkReady)
    {
        Blynk.run();
    }

    if (!isRunning || currentData.currentStage == Stage::OFF)
    {
        unsigned long currentMillis = millis();
        bool isPressed = touchButton.pressed();

        if (currentMillis - lastTouchPressedTimer > 1000 && isPressed)
        {
            touchPressedCounter++;
            lastTouchPressedTimer = millis();
        }

        if (currentMillis - lastTouchPressedTimer > 350 && !isPressed)
        {
            if (touchPressedCounter >= 5)
            {
                isRunning = true;
                updateStates(States::POWER);
            }
            else if (touchPressedCounter < 4)
            {
                Mode::Mode newMode = Mode::cycleMode(currentData.currentMode);
                if (changeMode(newMode))
                {
                    updateStates(States::MODE);
                    buzzer.beep(350, beepsPerMode[newMode], 100);
                }
            }

            touchPressedCounter = 0;
        }
        return;
    }

    if (changeModulePowerQueued)
    {
        if (millis() - modulePowerChangeTimer > MODULE_POWER_CHANGE_DELAY)
        {
            LOGN("Doing module power change.");

            switch (currentModuleChange)
            {
            case Module::WATER_VALVE:
                {
                    digitalWrite(WATER_VALVE_PIN, ON_VALUE);
                    break;
                }
            case Module::MOTOR:
                {
                    motorTimeCounter = millis();
                    digitalWrite(MOTOR_PIN, ON_VALUE);
                    break;
                }
            case Module::DRAIN_PUMP:
                {
                    digitalWrite(DRAIN_PUMP_PIN, ON_VALUE);
                    break;
                }
            }

            changeModulePowerQueued = false;
        }

        return;
    }

    switch (currentData.currentStage)
    {
    case Stage::WASH_FILLING_UP:
        {
            if (isWaterFilled())
            {
                updateStates(States::STAGE, Stage::WASH_SHAKING);
                shakeSoakStartTime = millis();
            }
            break;
        }
    case Stage::WASH_SHAKING:
        {
            if (millis() - shakeSoakStartTime > (agitateMinutesByMode[currentData.currentMode] * 60 * 1000))
            {
                updateStates(States::STAGE, Stage::WASH_DRAINING);
            }
            else
            {
                spinAlternating();
            }
            break;
        }
    case Stage::WASH_DRAINING:
        {
            if (isWaterDrained())
            {
                bool hasRinse = rinseCyclesByMode[currentData.currentMode] > 0;
                updateStates(States::STAGE, hasRinse ? Stage::RINSE_FILLING_UP : Stage::OFF);
            }
            break;
        }
    case Stage::RINSE_FILLING_UP:
        {
            if (isWaterFilled())
            {
                bool hasSoak = rinseSoakMinutesByMode[currentData.currentMode] > 0;
                updateStates(States::STAGE, hasSoak ? Stage::RINSE_SOAKING : Stage::RINSE_SHAKING);
                shakeSoakStartTime = millis();
            }
            break;
        }
    case Stage::RINSE_SOAKING:
        {
            if (millis() - shakeSoakStartTime > (rinseSoakMinutesByMode[currentData.currentMode] * 60 * 1000))
            {
                updateStates(States::STAGE, Stage::RINSE_SHAKING);
                shakeSoakStartTime = millis();
            }
            break;
        }
    case Stage::RINSE_SHAKING:
        {
            if (millis() - shakeSoakStartTime > (agitateRinseMinutesByMode[currentData.currentMode] * 60 * 1000))
            {
                updateStates(States::STAGE, Stage::RINSE_DRAINING);
            }
            else
            {
                spinAlternating();
            }
            break;
        }
    case Stage::RINSE_DRAINING:
        {
            if (isWaterDrained())
            {
                updateStates(States::STAGE, Stage::RINSE_SPINNING);
                changeModulePower(Module::MOTOR, true);
                motorTimeCounter = millis();
                spinning = false;
            }
            break;
        }
    case Stage::RINSE_SPINNING:
        {
            if (millis() - motorTimeCounter > (rinseSpinMinutes * 60 * 1000))
            {
                turnOffAllModules();

                bool hasNextCycle = currentData.currentRinse < rinseCyclesByMode[currentData.currentMode];
                updateStates(States::STAGE, hasNextCycle ? Stage::RINSE_FILLING_UP : Stage::DRY_SPINNING);
                changeModulePower(Module::MOTOR, true);
                motorTimeCounter = millis();
                spinning = false;
            }
            break;
        }
    case Stage::DRY_SPINNING:
        {
            if (millis() - motorTimeCounter > (dryMinutesByMode[currentData.currentMode] * 60 * 1000))
            {
                updateStates(States::STAGE, Stage::OFF);
            }
            break;
        }
    case Stage::OFF:
        {
            break;
        }
    }

    unsigned long currentMillis = millis();
    bool isPressed = touchButton.pressed();

    if (currentMillis - lastTouchPressedTimer > 1000 && isPressed)
    {
        touchPressedCounter++;
        lastTouchPressedTimer = millis();
    }

    if (currentMillis - lastTouchPressedTimer > 350 && !isPressed)
    {
        if (touchPressedCounter > 5)
        {
            updateStates(States::STAGE, Stage::OFF);
        }

        touchPressedCounter = 0;
    }
}