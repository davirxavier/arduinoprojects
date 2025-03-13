## ESP32-C3-MINI

Helpful things to remember when working with AliExpress ESP32-C3-MINI boards.

### Serial Logging

Add the following to platformio.ini to be able to see things that are logged to serial:

````
build_flags =
    -D ARDUINO_USB_MODE=1
    -D ARDUINO_USB_CDC_ON_BOOT=1
````

### LittleFS

Add the following config to setup filesystem storage, by default there is no space allocated to the filesystem (always use with esp-config-page).

````
board_build.filesystem = littlefs
board_build.partitions = min_spiffs.csv
````

### Speeds

Monitor and update speeds setup.

````
upload_speed = 921600
monitor_speed = 115200
````