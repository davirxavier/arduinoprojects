//
// Created by Davi on 28/05/2023.
//

#ifndef ARDUINOPLATFORMIO_4DISPLAY_H
#define ARDUINOPLATFORMIO_4DISPLAY_H

class FourDisplay {
public:
    FourDisplay(uint8_t (&pins)[7], uint8_t (&digitPins)[4], uint8_t dotPin);
    void write4(int16_t numberWithFourDigits, bool displayDot);
    void write1(uint8_t digitIndex, uint8_t numberWithOneDigit, bool displayDot);
    void eraseAll();
    void turnAllOff();
    uint8_t (&pins)[7];
    uint8_t (&digitPins)[4];
    uint8_t dotPin;
    uint8_t pinsByNumber[10][7];
    uint16_t brightness;
};

#endif //ARDUINOPLATFORMIO_4DISPLAY_H