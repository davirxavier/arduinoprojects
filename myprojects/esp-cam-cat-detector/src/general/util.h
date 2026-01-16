//
// Created by xav on 11/26/25.
//

#ifndef UTIL_H
#define UTIL_H

#define ENABLE_LOGGING
#ifdef ENABLE_LOGGING
#define MLOG(str) Serial.print(str)
#define MLOGN(str) Serial.println(str)
#define MLOGF(str, p...) Serial.printf(str, p)
#define MVLOGF(str, p...) Serial.vprintf(str, p)
#else
#define MLOG(str)
#define MLOGN(str)
#define MLOGF(str, p...)
#define MVLOGF(str, p...)
#endif

// gpio4 -> action mosfet (gpio4 is for flash led, should desolder led)
// using 4 because i can pull-down it as needed without interfering with other functionality

// gpio3 -> lights mosfet (gpio3 is rx, pulled up high on boot but fine for lights only)
// using 3 because i can set it to what i want after boot without issues

// gpio12 -> luminosity reading

#define CAM_FLASH_PIN 3
#define ACTION_PIN 4
#define LUMINOSITY_PIN 12
#include <esp_camera.h>

inline uint32_t readLuminosity()
{
    esp_wifi_stop();
    uint32_t lum = analogReadMilliVolts(LUMINOSITY_PIN);
    Serial.printf("Luminosity: %lu\n", lum);
    esp_wifi_start();
    return lum;
}

inline void toggleFlash(bool on)
{
    digitalWrite(CAM_FLASH_PIN, on ? HIGH : LOW);
}

inline camera_fb_t *getFrameWithFlash()
{
    toggleFlash(true);
    delay(30);
    camera_fb_t *fb = esp_camera_fb_get();
    delay(5);
    toggleFlash(false);
    return fb;
}

inline void setupPins()
{
    pinMode(3, OUTPUT);
    toggleFlash(false);

    pinMode(ACTION_PIN, OUTPUT);
    digitalWrite(ACTION_PIN, LOW);
}

inline void testRun()
{
    digitalWrite(ACTION_PIN, HIGH);
    toggleFlash(true);
    delay(500);
    digitalWrite(ACTION_PIN, LOW);
    toggleFlash(false);
}

class ActionController {
public:
    void setup() {
        pinMode(ACTION_PIN, OUTPUT);
        digitalWrite(ACTION_PIN, LOW);
        actionsOveruseTimer = millis();
    }

    void doAction() {
        if (hasAction || hasTimeout || millis() - lastActionEnd < minDelay) {
            return;
        }

        hasAction = true;
        actionTimer = millis();
        digitalWrite(ACTION_PIN, HIGH);
        MLOGN("Do action start.");
    }

    void loop() {
        if (hasTimeout && millis() - timeoutStart > timeout) {
            hasTimeout = false;
            MLOGN("Overuse timeout ended.");
        }

        if (hasTimeout) {
            return;
        }

        if (hasAction && millis() - actionTimer > actionOnTime) {
            hasAction = false;
            digitalWrite(ACTION_PIN, LOW);
            actionsCount++;
            lastActionEnd = millis();
            MLOGN("Do action end.");
        }

        if (millis() - actionsOveruseTimer > actionsOveruseReset) {
            actionsCount = 0;
            actionsOveruseTimer = millis();
        }

        if (actionsCount > maxActionsPerPeriod) {
            hasTimeout = true;
            timeoutStart = millis();
            hasAction = false;
            digitalWrite(ACTION_PIN, LOW);
            actionTimer = 0;
            MLOGN("Action overuse detected, probably some problem is happening.");
        }
    }

private:
    bool hasAction = false;
    unsigned long actionTimer = 0;
    const unsigned long actionOnTime = 800;

    const unsigned long minDelay = 200;
    unsigned long lastActionEnd = 0;

    const unsigned long timeout = 300 * 1000;
    const int maxActionsPerPeriod = 45;
    int actionsCount = 0;
    unsigned long actionsOveruseTimer = 0;
    const unsigned long actionsOveruseReset = 90 * 1000;
    bool hasTimeout = false;
    unsigned long timeoutStart = 0;
};

typedef enum {
    STR2INT_SUCCESS,
    STR2INT_OVERFLOW,
    STR2INT_UNDERFLOW,
    STR2INT_INCONVERTIBLE
} str2int_errno;

/* Convert string s to int out.
 *
 * @param[out] out The conv erted int. Cannot be NULL.
 *
 * @param[in] s Input string to be converted.
 *
 *     The format is the same as strtol,
 *     except that the following are inconvertible:
 *
 *     - empty string
 *     - leading whitespace
 *     - any trailing characters that are not part of the number
 *
 *     Cannot be NULL.
 *
 * @param[in] base Base to interpret string in. Same range as strtol (2 to 36).
 *
 * @return Indicates if the operation succeeded, or why it failed.
 */
inline str2int_errno str2int(int *out, char *s, int base) {
    char *end;
    if (s[0] == '\0' || isspace((unsigned char) s[0]))
        return STR2INT_INCONVERTIBLE;
    errno = 0;
    long l = strtol(s, &end, base);
    /* Both checks are needed because INT_MAX == LONG_MAX is possible. */
    if (l > INT_MAX || (errno == ERANGE && l == LONG_MAX))
        return STR2INT_OVERFLOW;
    if (l < INT_MIN || (errno == ERANGE && l == LONG_MIN))
        return STR2INT_UNDERFLOW;
    if (*end != '\0')
        return STR2INT_INCONVERTIBLE;
    *out = l;
    return STR2INT_SUCCESS;
}

#endif //UTIL_H
