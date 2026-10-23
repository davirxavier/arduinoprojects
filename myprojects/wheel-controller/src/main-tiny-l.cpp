#ifdef ARDUINO_AVR_ATTINY13
#include <Arduino.h>
#include <util/delay.h>

volatile byte timerCounter = 0;
volatile uint16_t overflowCount = 0;

ISR(TIM0_OVF_vect)
{
    if (++overflowCount >= 480)
    {
        overflowCount = 0;
        timerCounter++;
    }
}

constexpr byte X_OUT_PIN = PB1;
constexpr analog_pin_t X_IN_PIN = A3;

constexpr byte MODE_BUTTON_PIN = PB0;
constexpr byte MODE_IN_PIN = PB2;
constexpr byte MODE_OUT_PIN = PB4;
constexpr byte LED_PIN = PB5;

enum State
{
    NO_ANALOG,
    ANALOG,
    ANALOG_SENSITIVE,
    INVALID,
};
State currentState = NO_ANALOG;
bool hasSignal = false;

constexpr byte DEBOUNCE_TICKS = 7; // about 25.6 ms

bool rawButtonState = false;
bool stableButtonState = false;
byte lastButtonChange = 0;

bool readButtonPressedEdge()
{
    bool reading = (digitalRead(MODE_BUTTON_PIN) == LOW);
    byte now = timerCounter;

    if (reading != rawButtonState)
    {
        rawButtonState = reading;
        lastButtonChange = now;
    }

    if (rawButtonState != stableButtonState &&
        (byte)(now - lastButtonChange) >= DEBOUNCE_TICKS)
    {
        stableButtonState = rawButtonState;
        return stableButtonState;
    }

    return false;
}

void writeLedUpdate(State newState)
{
    if (newState == ANALOG_SENSITIVE && currentState == ANALOG)
    {
        digitalWrite(LED_PIN, LOW);
    }
    else if (newState >= ANALOG && currentState == NO_ANALOG)
    {
        digitalWrite(LED_PIN, HIGH);
    }
    else if (newState == NO_ANALOG && currentState >= ANALOG)
    {
        digitalWrite(LED_PIN, LOW);
    }
}

void sendToggleMode()
{
    digitalWrite(MODE_OUT_PIN, HIGH);
    _delay_ms(50);
    digitalWrite(MODE_OUT_PIN, LOW);
    _delay_ms(50);
}

void updateBtn(bool newSignalState, bool buttonPressedEdge)
{
    if (currentState == NO_ANALOG && !hasSignal && newSignalState)
    {
        writeLedUpdate(ANALOG);
        currentState = ANALOG;
    }
    else if ((currentState >= ANALOG && hasSignal && !newSignalState))
    {
        writeLedUpdate(NO_ANALOG);
        currentState = NO_ANALOG;
    }


    if (buttonPressedEdge) {
        uint8_t oldState = currentState;
        uint8_t newState = oldState + 1;

        if (newState >= INVALID)
        {
            newState = NO_ANALOG;
        }

        if ((oldState == NO_ANALOG) != (newState == NO_ANALOG))
        {
            sendToggleMode();
        }

        writeLedUpdate((State)newState);
        currentState = (State)newState;
    }

    hasSignal = newSignalState;
}

void writeStick(const uint16_t rawInput, const bool sensitive = false)
{
    constexpr uint16_t centerPoint = 512;
    constexpr uint16_t deadzoneWidth = 25;
    constexpr uint8_t sensitivityMultiplier = 3;
    constexpr uint8_t outputCenter = 127;
    constexpr uint8_t outputMax = 255;

    uint8_t rawOutput = outputCenter; // Default output for deadzone (center)

    // 1. Check if input is in the lower active region
    if (rawInput < (centerPoint - deadzoneWidth)) {
        uint8_t standardOutput = (uint32_t)rawInput * outputCenter / (centerPoint - deadzoneWidth);

        if (sensitive) {
            uint8_t distanceFromCenter = outputCenter - standardOutput;
            uint16_t boostedDistance = distanceFromCenter * sensitivityMultiplier;
            rawOutput = (boostedDistance >= outputCenter) ? 0 : outputCenter - boostedDistance;
        } else {
            rawOutput = standardOutput;
        }
    }
    // 2. Check if input is in the upper active region
    else if (rawInput > (centerPoint + deadzoneWidth)) {
        uint8_t standardOutput = outputCenter + (uint32_t)(rawInput - (centerPoint + deadzoneWidth)) * (outputMax - outputCenter) / (1023 - (centerPoint + deadzoneWidth));

        if (sensitive) {
            uint8_t distanceFromCenter = standardOutput - outputCenter;
            uint16_t boostedDistance = distanceFromCenter * sensitivityMultiplier;
            rawOutput = (boostedDistance >= (outputMax - outputCenter)) ? outputMax : outputCenter + boostedDistance;
        } else {
            rawOutput = standardOutput;
        }
    }

    OCR0B = rawOutput;
}

void setup()
{
    pinMode(MODE_OUT_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(MODE_OUT_PIN, LOW);

    pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
    pinMode(MODE_IN_PIN, INPUT);
    pinMode(X_OUT_PIN, OUTPUT);

    // Timer0: Fast PWM on OC0B (PB1), 37.5 kHz at 9.6 MHz
    TCCR0A = (1 << COM0B1) | (1 << WGM01) | (1 << WGM00);
    TCCR0B = (1 << CS00);
    TIMSK0 = (1 << TOIE0);
    OCR0B = 127;  // Start at approximately 50% duty cycle
    sei();
}

void loop()
{
    writeStick(analogRead(X_IN_PIN), currentState == ANALOG_SENSITIVE);

    bool buttonPressedEdge = readButtonPressedEdge();
    bool newSignalState = digitalRead(MODE_IN_PIN);
    updateBtn(newSignalState, buttonPressedEdge);

    if (currentState == ANALOG_SENSITIVE)
    {
        byte count = timerCounter;

        if (count % 64 == 0)
        {
            bool ledState = (count / 64) % 2 == 0;
            digitalWrite(LED_PIN, ledState ? HIGH : LOW);
        }
    }
}
#endif