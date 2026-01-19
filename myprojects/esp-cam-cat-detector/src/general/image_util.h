#pragma once

#include <JPEGDEC.h>

namespace JPEG_DECODE_UTIL
{
    struct DecodeContext
    {
        uint8_t* buf = nullptr;
        size_t bufLen = 0;
        size_t imageWidth = 0;
        bool outputBGR = false;
    };

    enum Status
    {
        OK,
        BUFFER_TOO_SMALL,
        INVALID_BUFFER,
    };

    inline uint8_t rgb_to_gray(uint8_t r, uint8_t g, uint8_t b)
    {
        return (uint8_t)(0.299f * r + 0.587f * g + 0.114f * b);
    }

    inline void rgb_bgr_swap(uint8_t* buffer, size_t length)
    {
        if (!buffer || length < 3) return;

        // Must be a multiple of 3 bytes per pixel
        size_t pixels = length / 3;

        for (size_t i = 0; i < pixels; i++)
        {
            uint8_t* p = buffer + i * 3;

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
        BUFFER_TOO_SMALL,
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

    typedef struct
    {
        uint8_t b;
        uint8_t g;
        uint8_t r;
    } BGR;

    static inline void set_pixel(
        uint8_t* img,
        int x,
        int y,
        BGR c
    )
    {
        int idx = (y * 96 + x) * 3;
        img[idx + 0] = c.b;
        img[idx + 1] = c.g;
        img[idx + 2] = c.r;
    }

    inline void draw_cross(
        uint8_t* img,
        int imgWidth,
        int imgHeight,
        int cx,
        int cy,
        BGR c,
        int size = 4, // half-length of arms
        int thickness = 1 // line thickness
    )
    {
        if (thickness <= 0)
            return;

        int half = thickness / 2;

        for (int i = -size; i <= size; ++i)
        {
            // horizontal arm
            for (int t = -half; t <= half; ++t)
            {
                int x = cx + i;
                int y = cy + t;

                if (x >= 0 && x < imgWidth && y >= 0 && y < imgHeight)
                    set_pixel(img, x, y, c);
            }

            // vertical arm
            for (int t = -half; t <= half; ++t)
            {
                int x = cx + t;
                int y = cy + i;

                if (x >= 0 && x < imgWidth && y >= 0 && y < imgHeight)
                    set_pixel(img, x, y, c);
            }
        }
    }

    inline void draw_circle(
        uint8_t* img,
        int imgWidth,
        int imgHeight,
        int cx,
        int cy,
        BGR c,
        int size = 12, // outer radius
        int thickness = 1 // outline thickness
    )
    {
        if (thickness <= 0)
            return;

        if (thickness > size)
            thickness = size;

        const int outer2 = size * size;
        const int innerRadius = size - thickness;
        const int inner2 = innerRadius * innerRadius;

        for (int dy = -size; dy <= size; ++dy)
        {
            for (int dx = -size; dx <= size; ++dx)
            {
                int d2 = dx * dx + dy * dy;

                // keep only pixels in the ring
                if (d2 < inner2 || d2 > outer2)
                    continue;

                int x = cx + dx;
                int y = cy + dy;

                if (x >= 0 && x < imgWidth && y >= 0 && y < imgHeight)
                    set_pixel(img, x, y, c);
            }
        }
    }

    inline void crop_resize_square_bgr_inplace(uint8_t* src, int src_w, int src_h, int dst_w, int dst_h)
    {
        // Determine square crop (fit shortest axis)
        int crop_size = src_w < src_h ? src_w : src_h;
        int x_offset = (src_w - crop_size) / 2;
        int y_offset = (src_h - crop_size) / 2;

        float x_ratio = (float)(crop_size - 1) / (dst_w - 1);
        float y_ratio = (float)(crop_size - 1) / (dst_h - 1);

        // We write the output directly to src (overwrite top-left)
        for (int y = 0; y < dst_h; y++)
        {
            float fy = y * y_ratio;
            int y0 = (int)fy;
            int y1 = y0 + 1 < crop_size ? y0 + 1 : y0;
            float wy = fy - y0;

            for (int x = 0; x < dst_w; x++)
            {
                float fx = x * x_ratio;
                int x0 = (int)fx;
                int x1 = x0 + 1 < crop_size ? x0 + 1 : x0;
                float wx = fx - x0;

                for (int c = 0; c < 3; c++)
                {
                    // B, G, R channels
                    float val = (1 - wx) * (1 - wy) * src[((y0 + y_offset) * src_w + (x0 + x_offset)) * 3 + c] +
                        wx * (1 - wy) * src[((y0 + y_offset) * src_w + (x1 + x_offset)) * 3 + c] +
                        (1 - wx) * wy * src[((y1 + y_offset) * src_w + (x0 + x_offset)) * 3 + c] +
                        wx * wy * src[((y1 + y_offset) * src_w + (x1 + x_offset)) * 3 + c];
                    src[(y * dst_w + x) * 3 + c] = (uint8_t)(val + 0.5f);
                }
            }
        }
    }

#define crop_desired_size_to_limit(pos, dsiz, siz) (((pos + dsiz) >= siz) ? (siz - pos) : dsiz)

    inline Status getImageDimensions(uint8_t* buf, size_t bufSize, ImageDimensions* dimensions)
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
                             int& x, int& y, int& width, int& height)
    {
        if (!jpegdec.openRAM(image, len, nullptr))
            return OPEN_JPEG_ERROR;

        int ss = jpegdec.getSubSample();
        int mcuW = 8, mcuH = 8;

        switch (ss)
        {
        case 0x00: // grayscale
        case 0x11: // 4:4:4
            mcuW = 8;
            mcuH = 8;
            break;
        case 0x12: // 4:2:2 vertical
            mcuW = 8;
            mcuH = 16;
            break;
        case 0x21: // 4:2:2 horizontal
            mcuW = 16;
            mcuH = 8;
            break;
        case 0x22: // 4:2:0
            mcuW = 16;
            mcuH = 16;
            break;
        }

        int imgW = jpegdec.getWidth();
        int imgH = jpegdec.getHeight();

        // Clamp input first
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x + width > imgW) width = imgW - x;
        if (y + height > imgH) height = imgH - y;

        // Align DOWN
        int ax = (x / mcuW) * mcuW;
        int ay = (y / mcuH) * mcuH;

        // Align UP using *clipped* end values
        int bx = ((x + width + mcuW - 1) / mcuW) * mcuW;
        int by = ((y + height + mcuH - 1) / mcuH) * mcuH;

        // clamp AFTER we recomputed aligned coords
        if (bx > imgW) bx = imgW;
        if (by > imgH) by = imgH;

        // Aligned rectangle
        x = ax;
        y = ay;
        width = bx - ax;
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

    inline Status getRgb888ImageSize(uint8_t* image, size_t size, size_t* out)
    {
        ImageDimensions dim{};
        Status dimRet = getImageDimensions(image, size, &dim);
        if (dimRet != OK)
        {
            return dimRet;
        }
        *out = (unsigned int)dim.width * dim.height * 3;
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

        JPEG_DECODE_UTIL::DecodeContext context{};
        context.buf = out;
        context.bufLen = outSize;
        context.imageWidth = finalFrameWidth;

        if (!jpegdec.openRAM(image, size, JPEG_DECODE_UTIL::decodeFn))
        {
            return OPEN_JPEG_ERROR;
        }

        jpegdec.setUserPointer(&context);
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

    // out buffer should always be width * height * 3 of length
    inline Status jpegToRgb888(uint8_t* image, size_t imageLen, uint8_t* out, bool outputBGR = false)
    {
        ImageDimensions dimensions{};
        Status res = getImageDimensions(image, imageLen, &dimensions);
        if (res != OK)
        {
            return res;
        }

        size_t bufSize = dimensions.width * dimensions.height * 3;
        JPEG_DECODE_UTIL::DecodeContext context{};
        context.buf = out;
        context.bufLen = bufSize;
        context.imageWidth = dimensions.width;
        context.outputBGR = outputBGR;

        if (!jpegdec.openRAM(image, imageLen, JPEG_DECODE_UTIL::decodeFn))
        {
            return OPEN_JPEG_ERROR;
        }

        jpegdec.setUserPointer(&context);
        jpegdec.setPixelType(RGB8888);
        if (!jpegdec.decode(0, 0, 0))
        {
            return DECODE_ERROR;
        }

        return OK;
    }
}
