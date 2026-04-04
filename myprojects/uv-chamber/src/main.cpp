#include <Arduino.h>
#include <display.h>
#include <OneButton.h>
#include <EEPROMWearLevel.h>
#include <buzzer.h>

#define RELAY_PIN 2
#define BUZZ_PIN 3
#define BTN_TIME_PIN A6
#define BTN_START_PIN A7
#define TIMER_LED_PIN A1
#define MODE_TIMER_PIN A2
#define MODE_CHRONO_PIN A3

#define EEPROM_VER 1
#define EEPROM_INDEXES 2

OneButton timeButton = {};
OneButton startButton = {};
OneButton timerModeButton(MODE_TIMER_PIN);
OneButton chronoModeButton(MODE_CHRONO_PIN);

DxBuzzer buzzer(BUZZ_PIN);

enum Mode
{
    OFF = 0,
    MODE_TIMER,
    MODE_CHRONO,
};

enum State
{
    STANDBY = 0,
    PAUSED,
    COUNTING,
    PLAYING_ALERT,
};

volatile State currentState = STANDBY;
Mode currentMode = OFF;
volatile long secondsTimer = 0;
volatile bool needsDisplayUpdate = false;
volatile bool finishPending = false;
volatile bool timerIncrement = false;

bool hasTimePress = false;
bool hasStartPress = false;
bool ignoreStartPress = false;
unsigned long lastStartPress = 0;

void addTimerVal(long val)
{
    uint8_t oldSREG = SREG;
    noInterrupts();
    secondsTimer += val;

    if (secondsTimer < 0)
    {
        secondsTimer = 0;
    }

    SREG = oldSREG;
    needsDisplayUpdate = true;
}

void setTimerVal(long val)
{
    uint8_t oldSREG = SREG;
    noInterrupts();
    secondsTimer = val;

    if (secondsTimer < 0)
    {
        secondsTimer = 0;
    }

    SREG = oldSREG;
    needsDisplayUpdate = true;
}

long getTimerVal()
{
    uint8_t oldSREG = SREG;
    noInterrupts();
    long copy = secondsTimer;
    SREG = oldSREG;
    return copy;
}

void save()
{
    EEPROMwl.put(currentMode == MODE_TIMER ? 0 : 1, secondsTimer);
}

void recover()
{
    long val = 0;
    EEPROMwl.get(currentMode == MODE_TIMER ? 0 : 1, val);
    setTimerVal(val);
}

void changeState(State newState)
{
    if (newState > STANDBY && currentMode == OFF)
    {
        return;
    }

    if (newState == COUNTING && currentMode == MODE_TIMER && getTimerVal() <= 0)
    {
        return;
    }

    if (currentMode == MODE_TIMER && newState == COUNTING && currentState == STANDBY)
    {
        save();
    }

    if (currentMode == MODE_CHRONO && newState == STANDBY && currentState > STANDBY)
    {
        save();
    }

    if (newState == STANDBY)
    {
        buzzer.stop();
    }

    if (currentState == STANDBY && newState == COUNTING && currentMode != MODE_TIMER)
    {
        setTimerVal(1);
    }

    if (newState == COUNTING)
    {
        buzzer.beep(150);
        digitalWrite(RELAY_PIN, HIGH);
    }
    else
    {
        digitalWrite(RELAY_PIN, LOW);
    }

    SevenDisplay::flashing = newState == PAUSED || newState == PLAYING_ALERT;
    currentState = newState;
    Serial.print("State change: ");
    Serial.println(newState);
}

void onModeEvent(Mode mode)
{
    if (currentState > STANDBY && mode != OFF)
    {
        return;
    }

    Serial.print("Mode: ");
    Serial.println(mode);
    currentMode = mode;

    if (currentMode == OFF && currentState > STANDBY)
    {
        changeState(STANDBY);
    }

    if (currentMode == OFF)
    {
        SevenDisplay::on = false;
        return;
    }

    if (currentState == STANDBY)
    {
        recover();
    }
    else
    {
        setTimerVal(0);
    }

    SevenDisplay::on = true;
    digitalWrite(TIMER_LED_PIN, currentMode == MODE_TIMER ? SevenDisplay::SON : SevenDisplay::SOFF);
    timerIncrement = currentMode != MODE_TIMER;
    needsDisplayUpdate = true;
}

void onTimeClick()
{
    Serial.println("Time click");

    if (currentState != STANDBY || currentMode != MODE_TIMER)
    {
        return;
    }

    addTimerVal(10);
}

void onTimePress()
{
    Serial.println("Time press");
    if (ignoreStartPress)
    {
        return;
    }

    hasTimePress = true;

    if (currentState != STANDBY || currentMode != MODE_TIMER)
    {
        return;
    }

    if (hasStartPress || millis() - lastStartPress < 1000)
    {
        return;
    }

    addTimerVal(10);
}

void onTimeIdle()
{
    Serial.println("Time idle");
    hasTimePress = false;
}

void onStartClick()
{
    Serial.println("Start click");
    if (currentState == STANDBY && currentMode == MODE_TIMER)
    {
        addTimerVal(-10);
    }
    else if (currentState == COUNTING)
    {
        changeState(PAUSED);
    }
    else if (currentState == PAUSED)
    {
        changeState(COUNTING);
    }
    else if (currentState == PLAYING_ALERT)
    {
        changeState(STANDBY);
    }
}

void onStartPress()
{
    Serial.println("Start press");
    if (ignoreStartPress)
    {
        return;
    }

    if (currentState != STANDBY)
    {
        ignoreStartPress = true;
        changeState(STANDBY);
        if (currentMode == MODE_TIMER)
        {
            recover();
        }
        return;
    }

    if (hasTimePress && currentState == STANDBY)
    {
        setTimerVal(0);
        lastStartPress = millis();
        return;
    }

    hasStartPress = true;
    changeState(COUNTING);
    ignoreStartPress = true;
}

void onStartIdle()
{
    Serial.println("Start idle");
    hasStartPress = false;
    ignoreStartPress = false;
}

void setupDisplay()
{
    SevenDisplay::initDisplay();

    // CTC Mode
    TCCR1A = 0;
    TCCR1B = (1 << WGM12);

    // 1khz
    OCR1A = 249;

    // prescaler 64
    TCCR1B |= (1 << CS11) | (1 << CS10);

    // enable compare interrupt
    TIMSK1 |= (1 << OCIE1A);
    sei();
}

ISR(TIMER1_COMPA_vect) {
    SevenDisplay::displayInterrupt();

    if (currentState != COUNTING)
    {
        return;
    }

    static int timerCounter = 0;
    timerCounter++;
    if (timerCounter >= 1000)
    {
        timerCounter = 0;

        if (secondsTimer > 0)
        {
            secondsTimer += (timerIncrement ? 1 : -1);
            needsDisplayUpdate = true;
            if (secondsTimer <= 0)
            {
                secondsTimer = 0;
                finishPending = true;
            }
        }
    }
}

void setup()
{
    Serial.begin(9600);
    EEPROMwl.begin(EEPROM_VER, EEPROM_INDEXES);

    setupDisplay();

    timeButton.attachClick(onTimeClick);
    timeButton.setDebounceMs(15);
    timeButton.setLongPressIntervalMs(150);
    timeButton.attachLongPressStart(onTimePress);
    timeButton.attachDuringLongPress(onTimePress);
    timeButton.attachLongPressStop(onTimeIdle);

    startButton.attachClick(onStartClick);
    startButton.setLongPressIntervalMs(150);
    startButton.attachLongPressStart(onStartPress);
    startButton.attachDuringLongPress(onStartPress);
    startButton.attachLongPressStop(onStartIdle);

    timerModeButton.attachLongPressStart([]() {onModeEvent(MODE_TIMER);});
    timerModeButton.attachLongPressStop([]() {onModeEvent(OFF);});
    chronoModeButton.attachLongPressStart([]() {onModeEvent(MODE_CHRONO);});
    chronoModeButton.attachLongPressStop([]() {onModeEvent(OFF);});

    pinMode(TIMER_LED_PIN, OUTPUT);
    digitalWrite(TIMER_LED_PIN, SevenDisplay::SOFF);

    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);
    pinMode(BUZZ_PIN, OUTPUT);
    noTone(BUZZ_PIN);

    recover();
    changeState(STANDBY);
    onModeEvent(OFF);
    setTimerVal(0);
    Serial.println("Started");
}

void loop()
{
    timeButton.tick(analogRead(BTN_TIME_PIN) > 512);
    startButton.tick(analogRead(BTN_START_PIN) > 512);
    timerModeButton.tick();
    chronoModeButton.tick();
    buzzer.update();

    if (needsDisplayUpdate)
    {
        noInterrupts();
        long copy = secondsTimer;
        needsDisplayUpdate = false;
        interrupts();

        int hours = copy / 3600;
        int minutes = (copy % 3600) / 60;
        int seconds = copy % 60;

        int displayValue;
        if (hours > 0) {
            // hh:mm
            displayValue = hours * 100 + minutes;
        } else {
            // mm:ss
            displayValue = minutes * 100 + seconds;
        }
        SevenDisplay::updateDisplay(displayValue, true);
    }

    if (finishPending)
    {
        finishPending = false;
        changeState(PLAYING_ALERT);
        buzzer.beep(250, UINT32_MAX, 150);
    }
}