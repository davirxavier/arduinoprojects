//
// Created by xav on 11/20/25.
//

#ifndef CAM_CONFIG_H
#define CAM_CONFIG_H

#include <esp_camera.h>
#include "FS.h"
#include "SD_MMC.h"
#include "driver/rtc_io.h"

// ESP32-S3-CAM
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

//ESP32-CAM
// #define PWDN_GPIO_NUM  32
// #define RESET_GPIO_NUM (-1)
// #define XCLK_GPIO_NUM  0
// #define SIOD_GPIO_NUM  26
// #define SIOC_GPIO_NUM  27
// #define Y9_GPIO_NUM    35
// #define Y8_GPIO_NUM    34
// #define Y7_GPIO_NUM    39
// #define Y6_GPIO_NUM    36
// #define Y5_GPIO_NUM    21
// #define Y4_GPIO_NUM    19
// #define Y3_GPIO_NUM    18
// #define Y2_GPIO_NUM    5
// #define VSYNC_GPIO_NUM 25
// #define HREF_GPIO_NUM  23
// #define PCLK_GPIO_NUM  22

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

    .xclk_freq_hz = 8000000, // Below 20mhz all direct sunlight is pink tinted
    .ledc_timer = LEDC_TIMER_1,
    .ledc_channel = LEDC_CHANNEL_1,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_HD,
    .jpeg_quality = 10,
    .fb_count = 1,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_LATEST
};

namespace CamConfig
{
    struct ImageDimensions
    {
        int width = -1;
        int height = -1;
    };

    static constexpr ImageDimensions dimensionsByFramesize[] = {
        {96, 96},        // FRAMESIZE_96X96
        {160, 120},      // FRAMESIZE_QQVGA
        {128, 128},      // FRAMESIZE_128X128
        {176, 144},      // FRAMESIZE_QCIF
        {240, 176},      // FRAMESIZE_HQVGA
        {240, 240},      // FRAMESIZE_240X240
        {320, 240},      // FRAMESIZE_QVGA
        {320, 320},      // FRAMESIZE_320X320
        {400, 296},      // FRAMESIZE_CIF
        {480, 320},      // FRAMESIZE_HVGA
        {640, 480},      // FRAMESIZE_VGA
        {800, 600},      // FRAMESIZE_SVGA
        {1024, 768},     // FRAMESIZE_XGA
        {1280, 720},     // FRAMESIZE_HD
        {1280, 1024},    // FRAMESIZE_SXGA
        {1600, 1200},    // FRAMESIZE_UXGA
        {1920, 1080},    // FRAMESIZE_FHD
        {720, 1280},     // FRAMESIZE_P_HD
        {864, 1536},     // FRAMESIZE_P_3MP
        {2048, 1536},    // FRAMESIZE_QXGA
        {2560, 1440},    // FRAMESIZE_QHD
        {2560, 1600},    // FRAMESIZE_WQXGA
        {1080, 1920},    // FRAMESIZE_P_FHD
        {2560, 1920},    // FRAMESIZE_QSXGA
        {2592, 1944},    // FRAMESIZE_5MP
        {0, 0}           // FRAMESIZE_INVALID
    };
    static constexpr int dimensionsByFramesizeLength = sizeof(dimensionsByFramesize) / sizeof(dimensionsByFramesize[0]);

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

        // Change bits [5:4] of register 0x303D to 00 (/1) or 10 (/2) so the internal REFIN stays within the required 4–13.5 MHz range for the 8 MHz XVCLK input.
        s->set_reg(s, 0x303D, 0x30, 0x00);
        //

        s->set_denoise(s, 8);
        s->set_vflip(s, 1);
        s->set_sharpness(s, 3);

        camera_fb_t *fb = esp_camera_fb_get();
        delay(100);
        esp_camera_fb_return(fb);
        return true;
    }

    inline bool getDimsByFrameSize(framesize_t framesize, ImageDimensions &dims)
    {
        if (framesize < dimensionsByFramesizeLength && framesize >= 0)
        {
            dims = dimensionsByFramesize[framesize];
            return true;
        }

        return false;
    }
}

#endif //CAM_CONFIG_H
