## ESP32-CAM

Usage details for this board.

### Issues

- Wi-Fi with the onboard antenna is extremely bad because of the board's design. Always use an external antenna, will
  give an extreme bump to speed and stability.
- Almost no free usable io pins.
- All usable ADCs are ADC2, needs to turn the Wi-Fi off whenever you want to take a reading.
- The camera images are very noisy because of camera clock, SPI and Wi-Fi traces being badly designed.

### Usable Pins

List of usable pins while using all onboard features (cam, psram and sdcard).

| GPIO | Notes                                                  | ADC Channel |
|------|--------------------------------------------------------|-------------|
| 2    | Free if using SD in 1-bit mode                         | ADC2_CH2    |
| 3    | RX pin, okay to use for simple things                  |             |
| 4    | Free if using SD in 1-bit mode, connected to flash LED | ADC2_CH0    |
| 12   | Free if using SD in 1-bit mode                         |             |

- ADC2 needs Wi-Fi to be stopped to be used correctly.

Yes, that's it for pins, there aren't any others that are usable.

### Config

- platformio.ini

```ini
[env:esp32cam]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/54.03.21/platform-espressif32.zip
board = esp32cam
framework = arduino, espidf
board_build.filesystem = littlefs
board_build.partitions = min_spiffs.csv
board_upload.flash_size = 4MB
monitor_speed = 115200
upload_speed = 921600
lib_deps =
    esp-config-page=symlink://../../../esp-config-page
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

- sdkconfig.defaults

```dotenv
CONFIG_AUTOSTART_ARDUINO=y
CONFIG_FREERTOS_HZ=1000

CONFIG_ESP_HTTP_CLIENT_ENABLE_HTTPS=y
CONFIG_ESP_HTTP_CLIENT_ENABLE_BASIC_AUTH=y
CONFIG_ESP_HTTP_CLIENT_ENABLE_DIGEST_AUTH=y

CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=240

#
# PSRAM (required for camera)
#
CONFIG_ESP32_SPIRAM_SUPPORT=y
```

- idf_component.yml

```yaml
dependencies:
  idf:
    version: '>=5.4.0'
  espressif/esp-tflite-micro: ^1.3.5 # If model running is needed
  espressif/esp32-camera: =2.0.15

# Use 2.0.15 if jpeg -> bgr888 decoding is needed or later releases if jpeg -> rgb888 is needed.
# Default byte order changed here: https://github.com/espressif/esp32-camera/pull/740/commits/6b684158003d9a6f771d6b3a82f570b1dee11694
```

## ESP32-S3 N16R8 CAM

Usage details for this board.

### Usable Pins

List of usable pins while using all onboard features (cam, psram and sdcard).

| GPIO | Notes                                 | ADC Channel |
|------|---------------------------------------|-------------|
| 1    | Free                                  | ADC1_CH0    |
| 2    | Free, but connected to onboard LED    | ADC1_CH1    |
| 3    | Usable if not using JTAG              | ADC1_CH2    |
| 14   | Free                                  | ADC2_CH3    |
| 19   | Free if not using native USB          | ADC2_CH8    |
| 20   | Free if not using native USB          | ADC2_CH9    |
| 21   | Free                                  | -           |
| 41   | Free if SD in 1-bit mode              | -           |
| 42   | Free if SD in 1-bit mode              | -           |
| 44   | RX pin, okay to use for simple things | -           |
| 45   | Probably okay to use                  | -           |
| 47   | Free                                  | -           |
| 48   | Free, but connected to WS2812 LED     | -           |

- ADC2 needs Wi-Fi to be stopped to be used correctly. Wiki suggests that you can use it while Wi-Fi is running but may
  fail? Needs testing.

### Config

- platformio.ini

```ini
[env:esp32cam]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/54.03.21/platform-espressif32.zip
board = esp32cam
framework = arduino, espidf
board_build.filesystem = littlefs
board_build.partitions = min_spiffs.csv
board_upload.flash_size = 16MB
monitor_speed = 115200
upload_speed = 921600
lib_deps =
    esp-config-page=symlink://../../../esp-config-page
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

- sdkconfig.defaults: Same as ESP32-CAM.
- idf_component.yml: Same as ESP32-CAM.