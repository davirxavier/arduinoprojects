#pragma once

namespace JPEG_DECODE_UTIL
{
    uint8_t* currentBuf = nullptr;
    size_t currentBufSize = 0;
    size_t currentWidth = 0;

    enum Status
    {
        OK,
        BUFFER_TOO_SMALL,
        INVALID_BUFFER,
    };

    inline void rgb_to_bgr888(uint8_t* buffer, size_t length)
    {
        if (!buffer || length < 3) return;

        // Must be a multiple of 3 bytes per pixel
        size_t pixels = length / 3;

        for (size_t i = 0; i < pixels; i++) {
            uint8_t* p = buffer + i*3;

            uint8_t r = p[0];
            uint8_t g = p[1];
            uint8_t b = p[2];

            // write B,G,R
            p[0] = b;
            p[1] = g;
            p[2] = r;
        }
    }

    inline int decodeFn(JPEGDRAW* pDraw)
    {
        if (currentBuf == nullptr) return 0;

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
                    // uint8_t b = src[col * 4 + 0];
                    // uint8_t g = src[col * 4 + 1];
                    // uint8_t r = src[col * 4 + 2];
                    // // uint8_t a = src[col * 4 + 3]; // ignored
                    //
                    // dst[col * 3 + 0] = r;
                    // dst[col * 3 + 1] = g;
                    // dst[col * 3 + 2] = b;

                    uint8_t r = src[col * 4 + 0];
                    uint8_t g = src[col * 4 + 1];
                    uint8_t b = src[col * 4 + 2];

                    dst[col * 3 + 0] = r;
                    dst[col * 3 + 1] = g;
                    dst[col * 3 + 2] = b;
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

    inline JPEG_DRAW_CALLBACK* jpegToBuffer(
        uint8_t* buf,
        size_t bufSize,
        size_t width)
    {
        currentBuf = buf;
        currentBufSize = bufSize;
        currentWidth = width;

        return decodeFn;
    }
}

namespace IMAGE_UTIL
{
    enum Status
    {
        OK,
        DECODE_BUF_ALLOCATION_FAILED,
        OPEN_JPEG_ERROR,
        DECODE_ERROR,
        COORDINATES_OUT_OF_BOUND,
    };

    enum JpegScale
    {
        SCALE_FULL = 0,
        SCALE_HALF = JPEG_SCALE_HALF,
        SCALE_QUARTER = JPEG_SCALE_QUARTER,
        SCALE_EIGHTH = JPEG_SCALE_EIGHTH,
    };

    struct ImageDimensions
    {
        int width = -1;
        int height = -1;
    };

    JPEGDEC jpegdec;

#define crop_desired_size_to_limit(pos, dsiz, siz) (((pos + dsiz) >= siz) ? (siz - pos) : dsiz)

    inline Status getImageDimensions(uint8_t *buf, size_t bufSize, ImageDimensions *dimensions)
    {
        if (!jpegdec.openRAM(buf, bufSize, nullptr))
        {
            return OPEN_JPEG_ERROR;
        }

        dimensions->width = jpegdec.getWidth();
        dimensions->height = jpegdec.getHeight();
        jpegdec.close();
        return OK;
    }

    inline Status alignToMcu(uint8_t* image, size_t len,
                          int &x, int &y, int &width, int &height)
    {
        if (!jpegdec.openRAM(image, len, nullptr))
            return OPEN_JPEG_ERROR;

        int ss = jpegdec.getSubSample();
        int mcuW = 8, mcuH = 8;

        switch (ss)
        {
        case 0x00: // grayscale
        case 0x11: // 4:4:4
            mcuW = 8;  mcuH = 8;
            break;
        case 0x12: // 4:2:2 vertical
            mcuW = 8;  mcuH = 16;
            break;
        case 0x21: // 4:2:2 horizontal
            mcuW = 16; mcuH = 8;
            break;
        case 0x22: // 4:2:0
            mcuW = 16; mcuH = 16;
            break;
        }

        int imgW = jpegdec.getWidth();
        int imgH = jpegdec.getHeight();

        // Clamp input first
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x + width  > imgW) width  = imgW - x;
        if (y + height > imgH) height = imgH - y;

        // Align DOWN
        int ax = (x / mcuW) * mcuW;
        int ay = (y / mcuH) * mcuH;

        // Align UP using *clipped* end values
        int bx = ((x + width  + mcuW - 1) / mcuW) * mcuW;
        int by = ((y + height + mcuH - 1) / mcuH) * mcuH;

        // clamp AFTER we recomputed aligned coords
        if (bx > imgW) bx = imgW;
        if (by > imgH) by = imgH;

        // Aligned rectangle
        x      = ax;
        y      = ay;
        width  = bx - ax;
        height = by - ay;

        jpegdec.close();
        return OK;
    }

    inline Status calcRgb888FrameSize(
        uint8_t* image,
        size_t size,
        int frameX,
        int frameY,
        int frameWidth,
        int frameHeight,
        size_t* out)
    {
        if (!jpegdec.openRAM(image, size, nullptr))
        {
            return OPEN_JPEG_ERROR;
        }

        int finalFrameWidth = crop_desired_size_to_limit(frameX, frameWidth, jpegdec.getWidth());
        int finalFrameHeight = crop_desired_size_to_limit(frameY, frameHeight, jpegdec.getHeight());
        *out = finalFrameWidth * finalFrameHeight * 3;
        jpegdec.close();
        return OK;
    }

    // Frame x, y, width and height should be multiples of 8, ex: 128 or 256 or 384
    inline Status getImageFrameRgb888(
        uint8_t* image,
        size_t size,
        int frameX,
        int frameY,
        int frameWidth,
        int frameHeight,
        uint8_t* out,
        size_t outSize,
        size_t* written,
        int* outWidth,
        int* outHeight,
        JpegScale scale = SCALE_FULL)
    {
        if (!jpegdec.openRAM(image, size, nullptr))
        {
            return OPEN_JPEG_ERROR;
        }

        int imageWidth = jpegdec.getWidth();
        int imageHeight = jpegdec.getHeight();

        int finalFrameWidth = crop_desired_size_to_limit(frameX, frameWidth, imageWidth);
        int finalFrameHeight = crop_desired_size_to_limit(frameY, frameHeight, imageHeight);

        jpegdec.close();

        if (!jpegdec.openRAM(image, size, JPEG_DECODE_UTIL::jpegToBuffer(out, outSize, finalFrameWidth)))
        {
            return OPEN_JPEG_ERROR;
        }

        jpegdec.setPixelType(RGB8888);
        jpegdec.setCropArea(frameX, frameY, finalFrameWidth, finalFrameHeight);

        if (!jpegdec.decode(0, 0, scale))
        {
            Serial.printf("Decode error: %d\n", jpegdec.getLastError());
            return DECODE_ERROR;
        }

        *outWidth = finalFrameWidth;
        *outHeight = finalFrameHeight;
        *written = (*outWidth) * (*outHeight) * 3;
        return OK;
    }
}
