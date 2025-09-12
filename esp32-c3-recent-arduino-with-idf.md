## Arduino as ESP-IDF component

Instructions for platformio for using a recent version of ESP-IDF together with recent Arduino core versions (Arduino as ESP-IDF component).

### Using hybrid compile (automic Arduino as IDF component by pio)

1 - Add platformio.ini, as follows:

````ini
[env:esp32-c3-devkitm-1]
; Desired version, can be default platformio espressif platform or pioarduno fork (recommended pioarduino)
; ESP-IDF v5.4.2
platform = https://github.com/pioarduino/platform-espressif32/releases/download/54.03.21-2/platform-espressif32.zip
;platform = espressif32 @ 6.11.0
; Default esp32c3 board
board = esp32-c3-devkitm-1
framework = arduino, espidf

; Add this if you need to use insights or rainmaker, necessary for compilation to work[mainBell.cpp](../doorbell/src/mainBell.cpp)
; (Will slow down build A LOT)
board_build.embed_txtfiles =
    managed_components/espressif__esp_insights/server_certs/https_server.crt
    managed_components/espressif__esp_rainmaker/server_certs/rmaker_mqtt_server.crt
    managed_components/espressif__esp_rainmaker/server_certs/rmaker_claim_service_server.crt
    managed_components/espressif__esp_rainmaker/server_certs/rmaker_ota_server.crt
; Or add this if to trim down the components that needs these files:
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
````

2 - Add skconfig.defaults file to project root as follows:

````
CONFIG_AUTOSTART_ARDUINO=y
CONFIG_FREERTOS_HZ=1000
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_MBEDTLS_PSK_MODES=y
CONFIG_MBEDTLS_KEY_EXCHANGE_PSK=y

CONFIG_BOOTLOADER_COMPILER_OPTIMIZATION_SIZE=y
CONFIG_COMPILER_OPTIMIZATION_SIZE=y
````

3 - Run sync and build.

---

### Manually

1 - Add platformio.ini, as follows:

````ini
[env:esp32-c3-devkitm-1]
; Desired version, can be default platformio espressif platform or pioarduno fork (recommended pioarduino)
; ESP-IDF v5.4.2
platform = https://github.com/pioarduino/platform-espressif32/releases/download/54.03.21-2/platform-espressif32.zip
;platform = espressif32 @ 6.11.0
; Default esp32c3 board
board = esp32-c3-devkitm-1
; Only ESP-IDF
framework = espidf

; Add this if you need to use insights or rainmaker, necessary for compilation to work[mainBell.cpp](../doorbell/src/mainBell.cpp)
; (Will slow down build A LOT)
board_build.embed_txtfiles =
    managed_components/espressif__esp_insights/server_certs/https_server.crt
    managed_components/espressif__esp_rainmaker/server_certs/rmaker_mqtt_server.crt
    managed_components/espressif__esp_rainmaker/server_certs/rmaker_claim_service_server.crt
    managed_components/espressif__esp_rainmaker/server_certs/rmaker_ota_server.crt
; Or add this if to trim down the components that needs these files:
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
````

2 - Run the idf.py command inside src folder to install the desired arduino version (must match compatible arduino core version, see platform repo for info): ```idf.py add-dependency "espressif/arduino-esp32=3.2.0"```


- You can also create the idf_component.yml file manually (inside src folder):

```yaml
## IDF Component Manager Manifest File
dependencies:
  ## Required IDF version
  idf:
    version: '>=5.4.1'
  espressif/arduino-esp32: =3.2.0
```

3 - Add skconfig.defaults file to project root as follows:

````
CONFIG_AUTOSTART_ARDUINO=y
CONFIG_FREERTOS_HZ=1000
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y

CONFIG_BOOTLOADER_COMPILER_OPTIMIZATION_SIZE=y
CONFIG_COMPILER_OPTIMIZATION_SIZE=y
````

4 - Run sync and build.

---

Extra:

- Min spiffs csv if more sketch space is needed, add to platformio.ini with ```board_build.partitions = min_spiffs.csv```.
````csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x1E0000,
app1,     app,  ota_1,   0x1F0000,0x1E0000,
spiffs,   data, spiffs,  0x3D0000,0x20000,
coredump, data, coredump,0x3F0000,0x10000,
````

- Useful platformio configs:

````ini
; Max stable upload speed on C3 superminis
upload_speed = 921600
board_build.filesystem = littlefs
; When exception decoding is needed, remember to after disable for lower size binary
monitor_filters = esp32_exception_decoder
; For c3 superminis usb serial to work
build_flags =
    -D ARDUINO_USB_MODE=1
    -D ARDUINO_USB_CDC_ON_BOOT=1
; Disable warnings as errors
build_unflags = -Werror=all
; Link local file lib
lib_deps =
    esp-config-page=symlink://../../../esp-config-page
````