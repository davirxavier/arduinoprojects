/*
* This code is very loosely based on the one from the following repository:
 * https://github.com/raduprv/esp32-cam_ov2640-timelapse
 *
 * Thanks to raduprv for the experimenting work with the ov2640 camera settings and providing his findings.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CAM_OPTIMIZATIONS_H
#define CAM_OPTIMIZATIONS_H

#include <Arduino.h>
#include <esp_camera.h>

namespace CamOpt
{
    inline void updateExposure(float luminosity)
    {
        sensor_t* s = esp_camera_sensor_get();

        // Clamp for safety
        if (luminosity < 0.0f) luminosity = 0.0f;
        if (luminosity > 1.0f) luminosity = 1.0f;

        // --- Default sensor settings ---
        s->set_whitebal(s, 1);
        s->set_awb_gain(s, 1);
        s->set_wb_mode(s, 2); // Cloudy
        s->set_gain_ctrl(s, 0);
        s->set_agc_gain(s, 0);
        s->set_gainceiling(s, (gainceiling_t)6);
        s->set_bpc(s, 1);
        s->set_wpc(s, 1);
        s->set_raw_gma(s, 1);
        s->set_lenc(s, 0);
        s->set_hmirror(s, 0);
        s->set_vflip(s, 0);
        s->set_dcw(s, 0);
        s->set_colorbar(s, 0);

        Serial.print("Linearized luminosity: ");
        Serial.println(luminosity, 3);

        // --- Select sensor bank 1 ---
        s->set_reg(s, 0xff, 0xff, 0x01);

        // ========= NIGHT / LOW LIGHT =========
        if (luminosity < 0.30f)
        {
            // Very dark → aggressive exposure
            if (luminosity < 0.10f)
            {
                s->set_reg(s, 0x11, 0xff, 1); // slower frame rate
            }

            s->set_reg(s, 0x13, 0xff, 0); // manual exposure/gain
            s->set_reg(s, 0x0c, 0x6, 0x8); // manual banding
            s->set_reg(s, 0x45, 0x3f, 0x3f); // max exposure
            s->set_reg(s, 0x43, 0xff, 0x40); // speed up frame processing
        }
        else
        {
            // ========= DAYLIGHT =========
            // Map luminosity [0.30 .. 1.0] → exposure reduction
            uint16_t frameLength = map(
                luminosity * 1000,
                300, 1000,
                0x400, // longer exposure
                0x020 // very short exposure
            );

            uint8_t lineAdjust = map(
                luminosity * 1000,
                300, 1000,
                0xF0,
                0x00
            );

            s->set_reg(s, 0x46, 0xff, frameLength & 0xFF); // LSB
            s->set_reg(s, 0x47, 0xff, frameLength >> 8); // MSB
            s->set_reg(s, 0x2a, 0xff, lineAdjust); // line adjust MSB
            s->set_reg(s, 0x2b, 0xff, 0xff); // line adjust LSB
            s->set_reg(s, 0x45, 0xff, 0x10); // base exposure
            s->set_reg(s, 0x11, 0xff, 0x0); // normal frame rate
            s->set_reg(s, 0x43, 0xff, 0x11); // restore default
        }

        // --- Return to bank 0 ---
        s->set_reg(s, 0xff, 0xff, 0x00);
        s->set_reg(s, 0xd3, 0xff, 0x08); // clock
    }
}

#endif //CAM_OPTIMIZATIONS_H
