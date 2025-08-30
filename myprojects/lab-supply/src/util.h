//
// Created by xav on 8/27/25.
//

#ifndef UTIL_H
#define UTIL_H

inline void printFloatSmart(double num, char output[], int width, int maxPrecision) {
    // Count digits in integer part
    int intPart = (int)num;
    int intDigits = intPart == 0 ? 1 : (int)log10(intPart) + 1;

    // Calculate max decimal precision we can use
    int precision = width - intDigits - 1; // -1 for decimal point

    if (precision < 0) precision = 0; // Not enough room for decimal digits
    if (precision > maxPrecision)
    {
        width = width + (maxPrecision - precision);
        precision = maxPrecision;
    }

    // Create format string and print
    char format[20];
    snprintf(format, sizeof(format), "%%%d.%df", width, precision);
    snprintf(output, width+1, format, num);

    // Trim trailing zeroes and dot if needed
    for (int i = strlen(output)-1; i > 1; i--)
    {
        if (output[i] == '0')
        {
            output[i] = 0;

            if (output[i-1] == '.')
            {
                output[i-1] = 0;
            }
        }
        else
        {
            break;
        }
    }
}

inline void printAndPad(const char print[], Print &display)
{
    int printed = strlen(print);
    if (printed < 16)
    {
        int remaining = 16 - printed;
        int padding = remaining / 2;
        if (padding < 0)
        {
            padding = 0;
        }

        char padBuf[padding+1]{};
        memset(padBuf, ' ', padding);

        display.print(padBuf);
        display.print(print);
        display.print(padBuf);

        if (remaining * 2 < 16)
        {
            display.print(' ');
        }
    }
    else
    {
        display.print(print);
    }
}

inline void printAndPad(const int print, Print &display)
{
    char buf[20];
    snprintf(buf, sizeof(buf), "%d", print);
    printAndPad(buf, display);
}

#endif //UTIL_H
