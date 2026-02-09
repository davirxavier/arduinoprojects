//
// Created by xav on 2/8/26.
//

#ifndef JPEGDEC_UTIL_H
#define JPEGDEC_UTIL_H

#include <JPEGDEC.h>
#include <general/image_util.h>

namespace JPEG_DEC_UTIL
{
    JPEGDEC jpegdec;

    using namespace IMAGE_UTIL;

    struct DecodeContext
    {
        uint8_t* buf = nullptr;
        size_t bufLen = 0;
        size_t imageWidth = 0;
        bool outputBGR = false;
    };

    inline int decodeFn(JPEGDRAW* pDraw)
    {
        if (pDraw->pUser == nullptr)
        {
            Serial.println("No pUser set to decode.");
            return 0;
        }

        auto* context = (DecodeContext*)pDraw->pUser;
        uint8_t* currentBuf = context->buf;
        size_t currentBufSize = context->bufLen;
        int currentWidth = context->imageWidth;

        const int bytesPerPixel = pDraw->iBpp / 8; // 2 for RGB565, 4 for ARGB8888

        // Loop each row of the current MCU
        for (int row = 0; row < pDraw->iHeight; ++row)
        {
            int dstY = pDraw->y + row;
            int dstX = pDraw->x;

            // Use iWidthUsed for right-edge MCUs
            int copyWidth = (pDraw->iWidthUsed > 0) ? pDraw->iWidthUsed : pDraw->iWidth;

            if (bytesPerPixel == 4)
            {
                size_t dstOffset = ((size_t)dstY * currentWidth + dstX) * 3; // RGB888 is 3 bytes
                size_t srcOffset = ((size_t)row * pDraw->iWidth) * bytesPerPixel;

                uint8_t* src = (uint8_t*)pDraw->pPixels + srcOffset;
                uint8_t* dst = currentBuf + dstOffset;

                for (int col = 0; col < copyWidth; ++col)
                {
                    if (context->outputBGR)
                    {
                        uint8_t r = src[col * 4 + 0];
                        uint8_t g = src[col * 4 + 1];
                        uint8_t b = src[col * 4 + 2];

                        dst[col * 3 + 0] = b;
                        dst[col * 3 + 1] = g;
                        dst[col * 3 + 2] = r;
                    }
                    else
                    {
                        uint8_t r = src[col * 4 + 0];
                        uint8_t g = src[col * 4 + 1];
                        uint8_t b = src[col * 4 + 2];

                        dst[col * 3 + 0] = r;
                        dst[col * 3 + 1] = g;
                        dst[col * 3 + 2] = b;
                    }
                }
            }
            else
            {
                size_t dstOffset = ((size_t)dstY * currentWidth + dstX) * bytesPerPixel;
                size_t srcOffset = ((size_t)row * pDraw->iWidth) * bytesPerPixel;
                size_t rowBytes = (size_t)copyWidth * bytesPerPixel;

                if (dstOffset + rowBytes > currentBufSize)
                {
                    Serial.printf("Out-of-bounds avoided at (%d,%d)\n", dstX, dstY);
                    return 0;
                }

                memcpy(currentBuf + dstOffset, (uint8_t*)pDraw->pPixels + srcOffset, rowBytes);
            }
        }

        return 1;
    }

    // out buffer should always be width * height * 3 of length
    inline Status jpegToRgb888(uint8_t* image, size_t imageLen, uint8_t* out, bool outputBGR = false, esp_jpeg_image_scale_t scale = JPEG_IMAGE_SCALE_0)
    {
        ImageDimensions dimensions{};
        bool res = jpegGetSize(image, imageLen, dimensions);
        if (!res)
        {
            return Status::OPEN_JPEG_ERROR;
        }

        size_t bufSize = dimensions.width * dimensions.height * 3;
        DecodeContext context{};
        context.buf = out;
        context.bufLen = bufSize;
        context.imageWidth = dimensions.width;
        context.outputBGR = outputBGR;

        if (!jpegdec.openRAM(image, imageLen, decodeFn))
        {
            return OPEN_JPEG_ERROR;
        }

        jpegdec.setUserPointer(&context);
        jpegdec.setPixelType(RGB8888);

        int actualScale = 0;
        switch(scale) {
            case JPEG_IMAGE_SCALE_1_2: actualScale = JPEG_SCALE_HALF; break;
            case JPEG_IMAGE_SCALE_1_4: actualScale = JPEG_SCALE_QUARTER; break;
            case JPEG_IMAGE_SCALE_1_8: actualScale = JPEG_SCALE_EIGHTH; break;
            default: actualScale = 0;
        }

        if (!jpegdec.decode(0, 0, actualScale))
        {
            return DECODE_ERROR;
        }

        return Status::OK;
    }
}

#endif //JPEGDEC_UTIL_H
