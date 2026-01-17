//
// Created by xav on 11/20/25.
//

#ifndef CAM_CONFIG_H
#define CAM_CONFIG_H

#include <general/util.h>
#include <general/cam_optimizations.h>
#include <esp_camera.h>
#include "FS.h"
#include "SD_MMC.h"
#include "driver/rtc_io.h"

#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM (-1)
#define XCLK_GPIO_NUM  0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27

#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM    5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22

// 4 for flash led or 33 for normal led
#define FLASH_PIN GPIO_NUM_4

camera_config_t camConfig = {
    .pin_pwdn = PWDN_GPIO_NUM,
    .pin_reset = RESET_GPIO_NUM,
    .pin_xclk = XCLK_GPIO_NUM,
    .pin_sccb_sda = SIOD_GPIO_NUM,
    .pin_sccb_scl = SIOC_GPIO_NUM,
    .pin_d7 = Y9_GPIO_NUM,
    .pin_d6 = Y8_GPIO_NUM,
    .pin_d5 = Y7_GPIO_NUM,
    .pin_d4 = Y6_GPIO_NUM,
    .pin_d3 = Y5_GPIO_NUM,
    .pin_d2 = Y4_GPIO_NUM,
    .pin_d1 = Y3_GPIO_NUM,
    .pin_d0 = Y2_GPIO_NUM,
    .pin_vsync = VSYNC_GPIO_NUM,
    .pin_href = HREF_GPIO_NUM,
    .pin_pclk = PCLK_GPIO_NUM,

    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_SVGA,
    .jpeg_quality = 8,
    .fb_count = 1,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_LATEST
};

namespace CamConfig
{
    enum CamFlashMode
    {
        OFF,
        ON,
        AUTO
    };

    inline bool initSdCard()
    {
        if (!SD_MMC.begin("/sdcard", true))
        {
            Serial.println("SD card mounting failed");
            return false;
        }

        uint8_t cardType = SD_MMC.cardType();
        if (cardType == CARD_NONE)
        {
            Serial.println("No SD card attached");
            return false;
        }

        return true;
    }

    inline bool initCamera()
    {
        rtc_gpio_hold_dis(FLASH_PIN);
        pinMode(FLASH_PIN, OUTPUT);
        digitalWrite(FLASH_PIN, LOW);

        esp_err_t err = esp_camera_init(&camConfig);
        if (err != ESP_OK)
        {
            Serial.printf("Camera init failed with error 0x%x\n", err);
            return false;
        }

        Serial.println("Getting initial frame buffers.");
        for (size_t i = 0; i < 3; i++)
        {
            camera_fb_t* fb = esp_camera_fb_get();
            if (!fb)
            {
                Serial.println("Initial frame buffer could not be acquired");
                return false;
            }
            esp_camera_fb_return(fb);
            delay(1000);
        }
        return true;
    }
}

#endif //CAM_CONFIG_H
