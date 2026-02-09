#pragma once

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

    struct ImageDimensions
    {
        int width = -1;
        int height = -1;
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

    inline bool jpegGetSize(const uint8_t* data, uint32_t len, ImageDimensions& dims)
    {
        if (!data || len < 4)
            return false;

        // SOI marker
        if (data[0] != 0xFF || data[1] != 0xD8)
            return false;

        uint32_t i = 2; // skip SOI

        while (i + 1 < len)
        {
            // Find marker prefix (0xFF)
            if (data[i] != 0xFF)
            {
                ++i;
                continue;
            }

            // Skip padding FF bytes
            while (i < len && data[i] == 0xFF)
                ++i;

            if (i >= len)
                return false;

            uint8_t marker = data[i++];

            // Standalone markers (no length field)
            if (marker == 0xD8 || marker == 0xD9 ||           // SOI, EOI
                (marker >= 0xD0 && marker <= 0xD7))           // RSTn
            {
                continue;
            }

            // Need at least a 2-byte length field
            if (i + 1 >= len)
                return false;

            uint16_t size = (data[i] << 8) | data[i + 1];
            if (size < 2)
                return false;

            // Ensure the full segment is inside the buffer
            if (i + size - 2 >= len)
                return false;

            // SOF markers that contain image size
            if (marker >= 0xC0 && marker <= 0xCF &&
                marker != 0xC4 &&  // DHT
                marker != 0xC8 &&  // JPG
                marker != 0xCC)    // DAC
            {
                // Segment layout:
                // [length(2)] [precision(1)] [height(2)] [width(2)] ...
                if (size < 7)
                    return false;

                dims.height = (data[i + 3] << 8) | data[i + 4];
                dims.width  = (data[i + 5] << 8) | data[i + 6];
                return true;
            }

            // Skip this segment
            i += size;
        }

        return false;
    }

    inline void adjustDimensionsScale(ImageDimensions &dimensions, esp_jpeg_image_scale_t scale)
    {
        uint32_t scale_div = 1;
        switch(scale) {
            case JPEG_IMAGE_SCALE_1_2: scale_div = 2; break;
            case JPEG_IMAGE_SCALE_1_4: scale_div = 4; break;
            case JPEG_IMAGE_SCALE_1_8: scale_div = 8; break;
            default: scale_div = 1;
        }

        dimensions.width  = ceil(dimensions.width / scale_div);
        dimensions.height = ceil(dimensions.height / scale_div);
    }
}
