//
// Created by xav on 2/8/26.
//

#ifndef JPEG_UTIL_H
#define JPEG_UTIL_H

/*
* This code is derived from esp32-camera
* https://github.com/espressif/esp32-camera
*
* Original copyright:
*   Copyright (c) 2020 Espressif Systems
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Modifications:
*   - Copied into this project
*   - Changed swap color bytes order
*   - Added into namespace JPEG_UTIL
*/
namespace JPEG_UTIL
{
     static uint8_t work[3100];

     // OUTPUTS BGR
     inline bool jpg2rgb888(const uint8_t *src, size_t src_len, uint8_t *out, esp_jpeg_image_scale_t scale)
     {
          esp_jpeg_image_cfg_t jpeg_cfg = {
               .indata = (uint8_t*) src,
               .indata_size = src_len,
               .outbuf = out,
               .outbuf_size = UINT32_MAX,
               .out_format = JPEG_IMAGE_FORMAT_RGB888,
               .out_scale = scale,
               .flags = { .swap_color_bytes = 1 },
               .advanced = {
                    .working_buffer = work,
                    .working_buffer_size = sizeof(work)
                },
            };

          esp_jpeg_image_output_t output_img = {};

          if(esp_jpeg_decode(&jpeg_cfg, &output_img) != ESP_OK){
               return false;
          }

          return true;
     }
}

#endif //JPEG_UTIL_H
