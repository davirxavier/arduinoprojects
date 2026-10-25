/*
  ATtiny13 internal oscillator tuner (Fully automated for 9600 - 115200 Baud)
  By Ralph Doncaster (2020)
  Tweaked and tuned by MCUdude
  ------------------------------------------------------------------------------
*/

#ifndef CUSTOM_CYCLES
#define CUSTOM_CYCLES ((float)F_CPU / CUSTOM_BAUD_RATE)
#endif

// Baud rates: 9600, 19200, 38400, 57600, or 115200
#include <Arduino.h>
#include <EEPROM.h>

// Automatically select the optimal Timer0 prescaler based on BAUD
// - 9600 baud needs prescaler 64 to avoid 8-bit timer overflow
// - 19200 to 115200 baud use prescaler 8 for higher timer resolution
#if CUSTOM_BAUD_RATE <= 9600
  #define TIMER_PRESCALER 64
  #define TCCR0B_PRESCALER (_BV(CS01) | _BV(CS00))
#else
  #define TIMER_PRESCALER 8
  #define TCCR0B_PRESCALER _BV(CS01)
#endif

const uint16_t CYCLES_PER_BIT = (uint16_t)(CUSTOM_CYCLES + 0.5);

// Converts 4-bit nibble to ascii hex
uint8_t nibbletohex(uint8_t value)
{
  value &= 0x0F;
  return value > 9 ? value + 'A' - 10 : value + '0';
}

void printHex(uint8_t value)
{
  putx(nibbletohex(value / 16));
  putx(nibbletohex(value));
}

// RX interrupt
ISR(PCINT0_vect)
{
  static uint8_t cal_counter;

  // Start timer with automatic prescaler when pin transitions low
  if ((PINB & _BV(PURXBIT)) == 0)
    TCCR0B = TCCR0B_PRESCALER;
  else
  {
    uint8_t current = TCNT0;
    // End of interval, reset counter
    TCCR0B = 0;
    TCNT0 = 0;

    // 'x' begins with 3 zeros + start bit = 4 * bit-time, scaled down by prescaler
    uint16_t expected = (uint16_t)((PUBIT_CYCLES * 4) / TIMER_PRESCALER + 0.5);
    int8_t delta = (int8_t)(expected - current);

    prints(F("OSCCAL: 0x"));
    printHex(OSCCAL);
    if (delta > 3)
    {
      OSCCAL++;
      prints(F(" - slow\n"));
    }
    else if (delta < -3)
    {
      OSCCAL--;
      prints(F(" - fast\n"));
    }
    else
    {
      prints(F(" - good\n"));
      cal_counter++;
    }
  }

  // Store new OSCCAL to EEPROM when stable
  if (cal_counter >= 5)
  {
    EEPROM.write(0, OSCCAL);
    prints(F("New OSCCAL stored to EEPROM addr. 0\n"));
    while (true);
  }

  GIFR = _BV(PCIF);
}

void setup()
{
  // Serial monitor open delay
  delay(500);

  // Pullup RX line
  PORTB |= _BV(PURXBIT);

  printHex(OSCCAL);
  prints(F(" Hit x to test.\n"));

  wait_x:
  // Ensure pin is high before waiting for start
  while ((PINB & _BV(PURXBIT)) == 0);
  // Wait for tuning character start (falling edge)
  while (PINB & _BV(PURXBIT));

  // Start timer with automatic prescaler
  TCNT0 = 0;
  TCCR0B = TCCR0B_PRESCALER;

  // Wait for rising edge
  while ((PINB & _BV(PURXBIT)) == 0);

  // Stop timer
  TCCR0B = 0;
  uint8_t counter = TCNT0;

  // Calculate 1-bit equivalent from the 4-bit low period measurement
  uint16_t measured_1bit = counter / 4;
  uint16_t expected_1bit = CYCLES_PER_BIT / TIMER_PRESCALER;

  // Expanded 50% margin to safely handle uncalibrated factory oscillator drift
  uint16_t margin = expected_1bit / 2;

  int16_t delta = (int16_t)measured_1bit - (int16_t)expected_1bit;
  if (__builtin_abs(delta) > margin)
  {
    prints(F("Noise skipped\n"));
    goto wait_x;
  }

  // Skip remaining bits in frame
  delay(1);

  prints(F(" OSCCAL "));
  if (__builtin_abs(delta) < 3)
  {
    prints(F("OK\n"));
  }
  else
  {
    prints(F("Imperfect\n"));
  }

  // Reset counter for first interrupt
  TCCR0B = 0;
  TCNT0 = 0;

  // Prepare RX interrupt
  PCMSK = _BV(PURXBIT);
  GIMSK = _BV(PCIE);

  // Enable interrupts
  sei();

  // Prepare sleep
  MCUCR |= _BV(SE);
}

void loop()
{
  // Nothing here really...
  asm("sleep");
}