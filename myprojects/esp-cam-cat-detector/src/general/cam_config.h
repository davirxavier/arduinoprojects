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

    .xclk_freq_hz = 10000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_240X240,
    .jpeg_quality = 8,
    .fb_count = 2,
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
        esp_err_t err = esp_camera_init(&camConfig);
        if (err != ESP_OK)
        {
            Serial.printf("Camera init failed with error 0x%x\n", err);
            return false;
        }

        sensor_t *s = esp_camera_sensor_get();

        // Required for denoise to work well
        s->set_gain_ctrl(s, 1);     // Auto gain ON
        s->set_agc_gain(s, 0);      // Let AGC choose the gain

        // Camera tweaks
        s->set_wb_mode(s, 1);
        s->set_denoise(s, 3);       // High denoise (0-3)
        s->set_vflip(s, 1);
        s->set_brightness(s, 1);
        s->set_saturation(s, -2);
        s->set_aec2(s, 1);          // Better auto-exposure
        s->set_gainceiling(s, GAINCEILING_2X);

        s->set_bpc(s, 1);
        s->set_wpc(s, 1);
        s->set_raw_gma(s, 1);

        camera_fb_t *fb = esp_camera_fb_get();
        delay(50);
        esp_camera_fb_return(fb);
        delay(200);
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
