//
// Created by xav on 11/20/25.
//

#ifndef CAM_CONFIG_H
#define CAM_CONFIG_H

#include <general/util.h>
#include <esp_camera.h>
#include "FS.h"
#include "SD_MMC.h"
#include "driver/rtc_io.h"

#define PWDN_GPIO_NUM  (-1) // ?
#define RESET_GPIO_NUM (-1) // ?
#define XCLK_GPIO_NUM  15
#define SIOD_GPIO_NUM  4
#define SIOC_GPIO_NUM  5

#define Y9_GPIO_NUM    16
#define Y8_GPIO_NUM    17
#define Y7_GPIO_NUM    18
#define Y6_GPIO_NUM    12
#define Y5_GPIO_NUM    10
#define Y4_GPIO_NUM    8
#define Y3_GPIO_NUM    9
#define Y2_GPIO_NUM    11
#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM  7
#define PCLK_GPIO_NUM  13

inline camera_config_t camConfig = {
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

    .xclk_freq_hz = 10000000,
    .ledc_timer = LEDC_TIMER_2,
    .ledc_channel = LEDC_CHANNEL_2,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_240X240,
    .jpeg_quality = 10,
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
        SD_MMC.setPins(39, 38, 40);

        if (!SD_MMC.begin("/sdcard", true))
        {
            MLOGN("SD card mounting failed");
            return false;
        }

        uint8_t cardType = SD_MMC.cardType();
        if (cardType == CARD_NONE)
        {
            MLOGN("No SD card attached");
            return false;
        }

        MLOGN("SD mounted successfully");
        return true;
    }

    inline bool initCamera()
    {
        esp_err_t err = esp_camera_init(&camConfig);
        if (err != ESP_OK)
        {
            MLOGF("Camera init failed with error 0x%x\n", err);
            return false;
        }

        sensor_t *s = esp_camera_sensor_get();

        s->set_wb_mode(s, 1);
        s->set_denoise(s, 8);
        s->set_vflip(s, 1);
        s->set_sharpness(s, 3);

        camera_fb_t *fb = esp_camera_fb_get();
        delay(50);
        esp_camera_fb_return(fb);
        return true;
    }

    inline void setRes(framesize_t framesize)
    {
        sensor_t *s = esp_camera_sensor_get();
        s->set_framesize(s, framesize);
        camConfig.frame_size = framesize;
    }
}

#endif //CAM_CONFIG_H
