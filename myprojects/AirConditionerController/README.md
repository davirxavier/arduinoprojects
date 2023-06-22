# Air Conditioner Controller

Using an Arduino Pro Mini to control my dad's legacy split air conditioner unit.

### Wiring

Janky wiring diagram:
![Janky wiring diagram](/arduinoac.png)

### Air conditioner controlling process

1. Read remote commands and do accordingly:
    - Turn on/off
      - If turn on:
        1. Set evaporator fan on
        2. Set a timeout of a few minutes for compressor to turn on (high gas pressure protection)
      - If turn off:
        1. Turn evaporator fan off
        2. Turn compressor off
        3. Turn display off
    - Lower/increase desired temperature
    - Toggle beep
    - Toggle show/hide average ambient temperature in display (only 2 digits available in display)
2. Update display to show desired temperature (or ambient) if ac is on
3. If ac is turned on, and it has passed 1s from the last check:
   - If it has passed x seconds from the last check:
     1. Calculate average from saved temperatures
     2. Do air conditioner checks:
        1. If ac's compressor is on, it has been running for a few minutes and the temperature hasn't changed or is higher than the last reading
            - Turn compressor off and set a timeout of a few minutes for turning on again
        2. If the ac's compressor is off, has been off for a few minutes and the current room temperature is above the user's set temperature by a few degrees:
            - Turn the compressor on and set a timeout for turning it off again
        3. If the ac's compressor is on, has been running for a few minutes and the current room temperature average is below the user's set temperature by one degree:
            - Turn off the compressor the set a timeout for turning it on again
   - Read current temperature on the sensor
   - Save current temperature