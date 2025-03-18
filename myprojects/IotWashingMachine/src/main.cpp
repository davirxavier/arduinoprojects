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
#include <OneButton.h>
#include <led.h>

#define LOG(s) Serial.print(s)
#define LOGN(s) Serial.println(s)
#define LOGF(s, p...) Serial.printf(s, p)
#define VW(p, v...) LOGF("Virtual write for pin %d.\n", p); if (blinkReady) Blynk.virtualWrite(p, v)

#define DATA_FILE "/saved2.txt"
#define ON_VALUE HIGH
#define OFF_VALUE LOW
#define MODULE_POWER_CHANGE_DELAY 15000

#define WATER_VALVE_PIN 2
#define DRAIN_PUMP_PIN 1
#define MOTOR_PIN 10
#define DIRECTION_PIN 0
#define WATER_LEVEL_SENSE_PIN 4
#define BUTTON_PIN 3
#define LED_PIN 9

ESP_CONFIG_PAGE::WEBSERVER_T webserver(80);

ESP_CONFIG_PAGE::EnvVar *webpageUser = new ESP_CONFIG_PAGE::EnvVar("username", "admin");
ESP_CONFIG_PAGE::EnvVar *webpagePassword = new ESP_CONFIG_PAGE::EnvVar("password", "admin");
ESP_CONFIG_PAGE::EnvVar *blynkAuthKey = new ESP_CONFIG_PAGE::EnvVar("blynk_auth_key", nullptr);

unsigned long lastButtonPressedTimer = 0;
unsigned int buttonPressedCounter = 0;

OneButton button(BUTTON_PIN, true, true);
DxLed led(LED_PIN, true);
uint8_t blinksPerMode[] = {1, 2, 3, 4};

const char *nodeName = "ESP-WM-03";
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

unsigned long spinTime = 300;
unsigned long spinPauseTime = 1100;

uint8_t rinseSpinMinutes = 3;
uint8_t agitateMinutesByMode[] = {5, 10, 20, 30, 0};
uint8_t agitateRinseMinutesByMode[] = {3, 5, 5, 5, 0};
uint8_t rinseSoakMinutesByMode[] = {0, 0, 3, 5, 0};
uint8_t rinseCyclesByMode[] = {0, 1, 2, 3, 0};
uint8_t dryMinutesByMode[] = {5, 7, 7, 10, 10};
// uint8_t rinseSpinMinutes = 1;
// uint8_t agitateMinutesByMode[] = {1, 1, 1, 1, 0};
// uint8_t agitateRinseMinutesByMode[] = {1, 1, 1, 1, 0};
// uint8_t rinseSoakMinutesByMode[] = {0, 0, 1, 1, 0};
// uint8_t rinseCyclesByMode[] = {0, 1, 2, 3, 0};
// uint8_t dryMinutesByMode[] = {1, 1, 1, 1, 1};

volatile unsigned int waterLevelSignalCounter = 0;
unsigned long waterLevelSignalTimer = 0;
bool waterLevelReached = false;
unsigned long waterLevelReachedTime = 0;

unsigned long elapsedTimer = 0;

unsigned long subtractElapsed(unsigned long millis)
{
    unsigned long elapsedMillis = currentData.elapsedMinutes * 60 * 1000;
    return millis > elapsedMillis ? millis - elapsedMillis : 60000;
}

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
    return !waterLevelReached && millis() - waterLevelReachedTime > (MODULE_POWER_CHANGE_DELAY * 3);
}

void turnOffAllModules()
{
    LOGN("Turn all modules off.");
    digitalWrite(WATER_VALVE_PIN, OFF_VALUE);
    digitalWrite(MOTOR_PIN, OFF_VALUE);
    digitalWrite(DRAIN_PUMP_PIN, OFF_VALUE);
    digitalWrite(DIRECTION_PIN, OFF_VALUE);
    changeModulePowerQueued = false;
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

    if (module == Module::MOTOR)
    {
        motorTimeCounter = millis();
    }
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

void updateStates(States::States stateChange, Stage::Stage newStage = Stage::OFF, bool manual = false)
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
            LOGF("Changing power state: %s\n", isRunning ? "running" : "not running");
            if (isRunning)
            {
                if (currentData.currentMode == Mode::SPIN)
                {
                    motorTimeCounter = millis();
                    changeModulePower(Module::MOTOR, true);
                }

                updateStates(States::STAGE, currentData.currentMode == Mode::SPIN ? Stage::DRY_SPINNING : Stage::WASH_FILLING_UP);
                led.on();
                button.setPressMs(5000);
            }
            else
            {
                updateStates(States::STAGE, Stage::OFF, manual);
                led.stop();
                button.setPressMs(2000);
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
                    changeModulePower(Module::MOTOR, true);
                    break;
                }
            case Stage::DRY_SPINNING:
                {
                    changeModulePower(Module::MOTOR, true);
                    break;
                }
            case Stage::OFF:
                {
                    if (blinkReady && !manual)
                    {
                        Blynk.logEvent(Events::EVENT_IDS[Events::CYCLE_ENDED]);
                    }

                    turnOffAllModules();
                    currentData.currentRinse = 0;
                    isRunning = false;
                    VW(POWER_PIN, BooleanEnum::FALSE);
                    break;
                }
            }

            if (currentData.currentStage != newStage)
            {
                VW(STAGE_PIN, newStage);
            }

            currentData.currentStage = newStage;
            currentData.elapsedMinutes = 0;
            break;
        }
    }

    saveData();
}

void skipStage()
{
    // bool found = false;
    // for (unsigned int i = 0; i < validStagesCount; i++)
    // {
    //     if (currentData.currentStage == validStagesForSkip[i])
    //     {
    //         found = true;
    //         break;
    //     }
    // }
    //
    // if (!found)
    // {
    //     return;
    // }
    //
    // LOGN("Skipping stage.");
    //
    // auto newStage = static_cast<Stage::Stage>(currentData.currentStage+1);
    // if (newStage == Stage::OFF)
    // {
    //     updateStates(States::POWER, Stage::OFF, true);
    // }
    // else
    // {
    //     updateStates(States::STAGE, newStage);
    // }
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
    updateStates(States::POWER, Stage::OFF, true);
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
        LOGN("Spin turn on.");

        spinning = true;
        motorTimeCounter = millis();
        direction = !direction;
        digitalWrite(DIRECTION_PIN, direction ? HIGH : LOW);
        digitalWrite(MOTOR_PIN, ON_VALUE);
    }

    if (spinning && currentMillis - motorTimeCounter > spinTime)
    {
        spinning = false;
        motorTimeCounter = currentMillis;
        digitalWrite(MOTOR_PIN, OFF_VALUE);
    }
}

void buttonLongPress()
{
    LOGN("Long press detected");
    if (isRunning)
    {
        isRunning = false;
        updateStates(States::POWER, Stage::OFF, true);
    }
    else
    {
        led.blink(500, blinksPerMode[Mode::cycleMode(currentData.currentMode)], 300, false);
        Mode::Mode newMode = Mode::cycleMode(currentData.currentMode);
        if (changeMode(newMode))
        {
            VW(MODE_PIN, newMode);
        }
    }
}

void buttonShortPress()
{
    LOGN("Short press detected.");
    if (isRunning)
    {
        return;
    }

    isRunning = true;
    VW(POWER_PIN, BooleanEnum::TRUE);
    updateStates(States::POWER);
}

void setup()
{
    Serial.begin(115200);
    delay(1500);
    LOGN("Initializing.");

    pinMode(WATER_LEVEL_SENSE_PIN, INPUT_PULLUP);
    attachInterrupt(WATER_LEVEL_SENSE_PIN, waterLevelSignalReceived, FALLING);

    pinMode(WATER_VALVE_PIN, OUTPUT);
    pinMode(MOTOR_PIN, OUTPUT);
    pinMode(DIRECTION_PIN, OUTPUT);
    pinMode(DRAIN_PUMP_PIN, OUTPUT);
    turnOffAllModules();

    button.debounce(true);
    button.setDebounceMs(-50);
    button.attachClick(buttonShortPress);
    button.attachLongPressStart(buttonLongPress);
    button.setPressMs(2000);

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
    button.tick();
    led.update();

    if (millis() - waterLevelSignalTimer > 1000)
    {
        bool newWaterLevelReached = waterLevelSignalCounter > 10;

        if (newWaterLevelReached != waterLevelReached)
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

        if (powerLoss)
        {
            isRunning = true;
            powerLoss = true;
            led.on();
            updateStates(States::STAGE, currentData.currentStage);
        }
    }

    if (blinkInit && !blinkReady && Blynk.connected())
    {
        LOGN("Blynk init successful.");
        blinkReady = true;

        if (powerLoss)
        {
            VW(POWER_PIN, BooleanEnum::TRUE);
            VW(STAGE_PIN, currentData.currentStage);
        }
    }

    if (blinkReady)
    {
        Blynk.run();
    }

    if (!isRunning)
    {
        return;
    }

    if (millis() - elapsedTimer > 60000)
    {
        currentData.elapsedMinutes++;
        LOGF("Update elapsed minutes: %d\n", currentData.elapsedMinutes);
        saveData();
        elapsedTimer = millis();
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
                    digitalWrite(DIRECTION_PIN, ON_VALUE);
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
            if (millis() - shakeSoakStartTime > subtractElapsed(agitateMinutesByMode[currentData.currentMode] * 60 * 1000))
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
                updateStates(States::STAGE, hasRinse ? Stage::RINSE_FILLING_UP : Stage::DRY_SPINNING);
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
            if (millis() - shakeSoakStartTime > subtractElapsed(rinseSoakMinutesByMode[currentData.currentMode] * 60 * 1000))
            {
                updateStates(States::STAGE, Stage::RINSE_SHAKING);
                shakeSoakStartTime = millis();
            }
            break;
        }
    case Stage::RINSE_SHAKING:
        {
            if (millis() - shakeSoakStartTime > subtractElapsed(agitateRinseMinutesByMode[currentData.currentMode] * 60 * 1000))
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
            }
            break;
        }
    case Stage::RINSE_SPINNING:
        {
            if (millis() - motorTimeCounter > subtractElapsed(rinseSpinMinutes * 60 * 1000))
            {
                turnOffAllModules();

                bool hasNextCycle = currentData.currentRinse < rinseCyclesByMode[currentData.currentMode];
                updateStates(States::STAGE, hasNextCycle ? Stage::RINSE_FILLING_UP : Stage::DRY_SPINNING);
            }
            break;
        }
    case Stage::DRY_SPINNING:
        {
            if (millis() - motorTimeCounter > subtractElapsed(dryMinutesByMode[currentData.currentMode] * 60 * 1000))
            {
                isRunning = false;
                updateStates(States::POWER);
            }
            break;
        }
    case Stage::OFF:
        {
            break;
        }
    }
}