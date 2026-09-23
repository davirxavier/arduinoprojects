#pragma once
#include <Arduino.h>

#ifdef ESP32
inline portMUX_TYPE oneWireBusMux = portMUX_INITIALIZER_UNLOCKED;
#define CRITICAL_START() portENTER_CRITICAL(&oneWireBusMux)
#define CRITICAL_END() portEXIT_CRITICAL(&oneWireBusMux)
#else
#define CRITICAL_START() noInterrupts()
#define CRITICAL_END() interrupts()
#endif

class OneWireBus {
  uint8_t _pin;
  bool _isMaster;
public:
  unsigned long timingMicros = 100;

  OneWireBus(uint8_t pin, bool isMaster) : _pin(pin), _isMaster(isMaster) {}

  void begin() {
    if (_isMaster) {
      pinMode(_pin, OUTPUT);
      digitalWrite(_pin, HIGH); // idle HIGH
    } else {
      pinMode(_pin, INPUT_PULLUP); // idle HIGH
    }
  }

  // ----------------------
  // MASTER: send command + data, receive 16-bit response
  // ----------------------
  uint16_t send(uint8_t cmd, uint16_t data) {
    if (!_isMaster) return 0xFFFF;

    CRITICAL_START();

    // --- Start bit ---
    digitalWrite(_pin, LOW);
    delayMicroseconds(timingMicros);
    digitalWrite(_pin, HIGH);
    delayMicroseconds(timingMicros);

    // --- Send command byte ---
    for (int i = 0; i < 8; i++) {
      digitalWrite(_pin, (cmd >> i) & 0x01);
      delayMicroseconds(timingMicros);
    }

    // --- Send 16-bit data ---
    for (int i = 0; i < 16; i++) {
      digitalWrite(_pin, (data >> i) & 0x01);
      delayMicroseconds(timingMicros);
    }

    // --- Switch to input and read 16-bit response ---
    pinMode(_pin, INPUT_PULLUP);
    delayMicroseconds(timingMicros);

    uint16_t response = 0;
    for (int i = 0; i < 16; i++) {
      int bit = digitalRead(_pin);
      response |= (bit << i);
      delayMicroseconds(timingMicros);
    }

    // Return to idle
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, HIGH);

    CRITICAL_END();
    return response;
  }

  // ----------------------
  // SLAVE: listen for command, return response via callback
  // ----------------------
  void listen(uint16_t (*callback)(uint8_t cmd, uint16_t data)) {
    if (_isMaster) return;

    if (digitalRead(_pin) == LOW) {
      CRITICAL_START();

      delayMicroseconds(timingMicros); // debounce/start bit
      pinMode(_pin, OUTPUT);

      // --- Read command byte ---
      uint8_t cmd = 0;
      for (int i = 0; i < 8; i++) {
        delayMicroseconds(timingMicros);
        cmd |= (digitalRead(_pin) << i);
      }

      // --- Read 16-bit data ---
      uint16_t data = 0;
      for (int i = 0; i < 16; i++) {
        delayMicroseconds(timingMicros);
        data |= (digitalRead(_pin) << i);
      }

      // --- Execute callback ---
      uint16_t response = callback(cmd, data);

      // --- Send 16-bit response ---
      for (int i = 0; i < 16; i++) {
        digitalWrite(_pin, (response >> i) & 0x01);
        delayMicroseconds(timingMicros);
      }

      pinMode(_pin, INPUT_PULLUP); // back to idle
      CRITICAL_END();
    }
  }
};
