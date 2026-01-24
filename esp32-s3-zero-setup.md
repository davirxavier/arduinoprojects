## ESP32-S3-Zero

Setup for this board. Actual chip is ESP32-S3-FH4R2. Has 320KB of RAM and 2MB of PSRAM.

### Config - Plain Arduino

- platformio.ini:
```ini
[env:esp32s3zero]
# Always use pioarduino core, it's more up to date
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
board = esp32-s3-devkitc-1
framework = arduino
upload_speed = 921600
monitor_speed = 115200
monitor_filters = esp32_exception_decoder
board_upload.flash_size = 4MB[platformio.ini](../../ard-test-projects/test-model-speed/platformio.ini)
board_build.partitions = default.csv
build_flags =
    -D ARDUINO_USB_CDC_ON_BOOT=1
    -DBOARD_HAS_PSRAM
```

### Config - Arduino as IDF Component

- platformio.ini:
```ini
[env:esp32s3zero]
# Always use pioarduino core, it's more up to date
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
board = esp32-s3-devkitc-1
framework = arduino, espidf
upload_speed = 921600
monitor_speed = 115200
monitor_filters = esp32_exception_decoder
board_upload.flash_size = 4MB
board_build.partitions = default.csv
build_flags =
    -D ARDUINO_USB_CDC_ON_BOOT=1
    -DBOARD_HAS_PSRAM
build_unflags =
    -Werror=all
custom_component_remove =
    espressif/esp_hosted
    espressif/esp_wifi_remote
    espressif/esp-dsp
    espressif/esp32-camera
    espressif/libsodium
    espressif/esp-modbus
    espressif/qrcode
    espressif/esp_insights
    espressif/esp_diag_data_store
    espressif/esp_diagnostics
    espressif/esp_rainmaker
    espressif/rmaker_common
```

- sdkconfig.defaults (at project root):
```
CONFIG_AUTOSTART_ARDUINO=y
CONFIG_FREERTOS_HZ=1000

CONFIG_ESP_HTTP_CLIENT_ENABLE_HTTPS=y
CONFIG_ESP_HTTP_CLIENT_ENABLE_BASIC_AUTH=y
CONFIG_ESP_HTTP_CLIENT_ENABLE_DIGEST_AUTH=y

CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=240

CONFIG_SPIRAM=y

#
# SPI RAM config
#
CONFIG_SPIRAM_MODE_QUAD=y
# CONFIG_SPIRAM_MODE_OCT is not set
CONFIG_SPIRAM_TYPE_AUTO=y
# CONFIG_SPIRAM_TYPE_ESPPSRAM64 is not set
CONFIG_SPIRAM_CLK_IO=30
CONFIG_SPIRAM_CS_IO=26
# CONFIG_SPIRAM_XIP_FROM_PSRAM is not set
# CONFIG_SPIRAM_FETCH_INSTRUCTIONS is not set
# CONFIG_SPIRAM_RODATA is not set
# CONFIG_SPIRAM_SPEED_80M is not set
CONFIG_SPIRAM_SPEED_40M=y
CONFIG_SPIRAM_SPEED=40
# CONFIG_SPIRAM_ECC_ENABLE is not set
CONFIG_SPIRAM_BOOT_HW_INIT=y
CONFIG_SPIRAM_BOOT_INIT=y
CONFIG_SPIRAM_PRE_CONFIGURE_MEMORY_PROTECTION=y
# CONFIG_SPIRAM_IGNORE_NOTFOUND is not set
# CONFIG_SPIRAM_USE_MEMMAP is not set
# CONFIG_SPIRAM_USE_CAPS_ALLOC is not set
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_SPIRAM_MEMTEST=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384
# CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP is not set
CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768
# CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY is not set
# CONFIG_SPIRAM_ALLOW_NOINIT_SEG_EXTERNAL_MEMORY is not set
# end of SPI RAM config
```