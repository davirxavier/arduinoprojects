//
// Created by xav on 11/26/25.
//

#ifndef UTIL_H
#define UTIL_H

#define ACTION_PIN 13

namespace ActionUtil
{
    inline bool hasAction = false;
    inline unsigned long actionTimer = 0;
    inline unsigned long actionOnTime = 800;

    inline const unsigned long minDelay = 200;
    inline unsigned long lastActionEnd = 0;

    inline unsigned long timeout = 300 * 1000;
    inline int maxActionsPerPeriod = 45;
    inline int actionsCount = 0;
    inline unsigned long actionsOveruseTimer = 0;
    inline unsigned long actionsOveruseReset = 90 * 1000;
    inline bool hasTimeout = false;
    inline unsigned long timeoutStart = 0;

    inline void setup()
    {
        pinMode(ACTION_PIN, OUTPUT);
        digitalWrite(ACTION_PIN, LOW);
        actionsOveruseTimer = millis();
    }

    inline void doAction()
    {
        if (hasAction || hasTimeout || millis() - lastActionEnd < minDelay)
        {
            return;
        }

        hasAction = true;
        actionTimer = millis();
        digitalWrite(ACTION_PIN, HIGH);
    }

    inline void loop()
    {
        if (hasTimeout && millis() - timeoutStart > timeout)
        {
            hasTimeout = false;
        }

        if (hasTimeout)
        {
            return;
        }

        if (hasAction && millis() - actionTimer > actionOnTime)
        {
            hasAction = false;
            digitalWrite(ACTION_PIN, LOW);
            actionsCount++;
            lastActionEnd = millis();
        }

        if (millis() - actionsOveruseTimer > actionsOveruseReset)
        {
            actionsCount = 0;
            actionsOveruseTimer = millis();
        }

        if (actionsCount > maxActionsPerPeriod)
        {
            hasTimeout = true;
            timeoutStart = millis();
            hasAction = false;
            digitalWrite(ACTION_PIN, LOW);
            actionTimer = 0;
        }
    }
}

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
