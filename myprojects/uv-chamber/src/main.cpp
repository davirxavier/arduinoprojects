#include <Arduino.h>
#include <display.h>
#include <OneButton.h>

#define RELAY_PIN 2
#define BUZZ_PIN 3
#define BTN_TIME_PIN A6
#define BTN_START_PIN A7
#define TIMER_LED_PIN A1
#define MODE_TIMER_PIN A2
#define MODE_CHRONO_PIN A3

#define TIMER_CAP ((60 * 99) + 99) // max of 99m:99s

OneButton timeButton = {};
OneButton startButton = {};
OneButton timerModeButton(MODE_TIMER_PIN);
OneButton chronoModeButton(MODE_CHRONO_PIN);

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

void addTimerVal(long val)
{
    uint8_t oldSREG = SREG;
    noInterrupts();
    secondsTimer += val;

    if (secondsTimer < 0)
    {
        secondsTimer = 0;
    }
    else if (secondsTimer >= TIMER_CAP)
    {
        secondsTimer = TIMER_CAP;
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
    else if (secondsTimer >= TIMER_CAP)
    {
        secondsTimer = TIMER_CAP;
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
    needsDisplayUpdate = true;
    return copy;
}

void changeState(State newState)
{
    if (newState > STANDBY && currentMode == OFF)
    {
        return;
    }

    if (newState == STANDBY)
    {
        noTone(BUZZ_PIN);
    }

    if (currentState == STANDBY && newState == COUNTING && currentMode != MODE_TIMER)
    {
        setTimerVal(1);
    }

    SevenDisplay::flashing = newState == PAUSED;
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

    if (currentState != STANDBY || currentMode != MODE_TIMER)
    {
        return;
    }

    addTimerVal(10);
    hasTimePress = true;
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
}

void onStartPress()
{
    Serial.println("Start press");
    if (currentState != STANDBY)
    {
        return;
    }

    if (hasTimePress)
    {
        setTimerVal(0);
        return;
    }

    hasStartPress = true;
    changeState(COUNTING);
}

void onStartIdle()
{
    Serial.println("Start idle");
    hasStartPress = false;
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

        if (secondsTimer > 0 && secondsTimer < TIMER_CAP)
        {
            addTimerVal(timerIncrement ? 1 : -1);
            if (secondsTimer <= 0 || secondsTimer >= TIMER_CAP)
            {
                secondsTimer = secondsTimer <= 0 ? 0 : TIMER_CAP;
                finishPending = true;
            }
        }
    }
}

void setup()
{
    Serial.begin(9600);
    setupDisplay();

    timeButton.attachClick(onTimeClick);
    timeButton.setLongPressIntervalMs(150);
    timeButton.attachLongPressStart(onTimePress);
    timeButton.attachDuringLongPress(onTimePress);
    timeButton.attachIdle(onTimeIdle);

    startButton.attachClick(onStartClick);
    startButton.setLongPressIntervalMs(150);
    startButton.attachLongPressStart(onStartPress);
    startButton.attachDuringLongPress(onStartPress);
    startButton.attachIdle(onStartIdle);

    timerModeButton.attachClick([]() {onModeEvent(MODE_TIMER);});
    // timerModeButton.attachIdle([]() {onModeEvent(OFF);});
    chronoModeButton.attachClick([]() {onModeEvent(MODE_CHRONO);});
    // chronoModeButton.attachIdle([]() {onModeEvent(OFF);});

    pinMode(TIMER_LED_PIN, OUTPUT);
    digitalWrite(TIMER_LED_PIN, SevenDisplay::SOFF);

    changeState(STANDBY);
    setTimerVal(0);
    Serial.println("Started");
}

void loop()
{
    timeButton.tick(analogRead(BTN_TIME_PIN) > 512);
    startButton.tick(analogRead(BTN_START_PIN) > 512);

    timerModeButton.tick();
    chronoModeButton.tick();

    if (needsDisplayUpdate)
    {
        noInterrupts();
        long copy = secondsTimer;
        needsDisplayUpdate = false;
        interrupts();

        copy = (copy / 60) * 100 + (copy % 60);
        SevenDisplay::updateDisplay(copy, true);
    }

    if (finishPending)
    {
        finishPending = false;
        changeState(PLAYING_ALERT);
    }

    if (currentState == PLAYING_ALERT)
    {
        // TODO
    }
}