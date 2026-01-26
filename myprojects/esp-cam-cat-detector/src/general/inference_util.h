//
// Created by xav on 12/6/25.
//

#ifndef INFERENCE_UTIL_H
#define INFERENCE_UTIL_H

#include <general/cam_config.h>
#include <general/image_util.h>
#include "model_util.h"

// #define INFERENCE_ENABLE_LOG

#ifdef INFERENCE_ENABLE_LOG
#define INFERENCE_LOG_FN(p...) printLog(p)
#define INFERENCE_ERROR_FN(error, errorNum, output) printError(error, errorNum, output)
#else
#define INFERENCE_LOG_FN(p...)
#define INFERENCE_ERROR_FN(error, errorNum, output)
#endif

// #define MODEL_IS_GRAYSCALE
#define MODEL_INPUT_WIDTH 96
#define MODEL_INPUT_HEIGHT 96
#define MAX_BOXES 10
#define MODEL_DOWNSCALING_FACTOR 8.0f
#define MODEL_OUTPUT_ROWS ((int) MODEL_DATA_INPUT_HEIGHT/(int)MODEL_DOWNSCALING_FACTOR)
#define MODEL_OUTPUT_COLS ((int) MODEL_DATA_INPUT_WIDTH/(int)MODEL_DOWNSCALING_FACTOR)
#define MODEL_CLASS_COUNT 3
#define MAX_LABEL_LENGTH 32
#define MAX_INFERENCE_DECODE_LENGTH (1024 * 1000)

namespace InferenceUtil
{
    constexpr char classes[MODEL_CLASS_COUNT][MAX_LABEL_LENGTH] = {"bg", "cat", "human"};
    constexpr float thresholds[] = {1, 0.75, 0.5};
    constexpr uint8_t catIndex = 1;
    constexpr uint8_t humanIndex = 2;
    inline unsigned long currentStartTimer = 0;

    static const IMAGE_UTIL::BGR classColors[MAX_BOXES + 1] = {
        {  0,   0,   0 },   // 0 - unused

        {255, 255,   0 },   // 1 - Blue
        {  0, 255,   0 },   // 2 - Green
        {  0,   0, 255 },   // 3 - Red
        { 255,   0,   0 },  // 4 - Cyan
        {255,   0, 255 },   // 5 - Magenta
        {  0, 255, 255 },   // 6 - Yellow

        {128,   0, 255 },   // 7 - Purple
        {255, 128,   0 },   // 8 - Orange
        {  0, 128, 255 },   // 9 - Light Red / Pink
        {128, 255,   0 }    // 10 - Lime
    };

#ifdef INFERENCE_ENABLE_LOG
    constexpr size_t currentOutputLen = 1024;
    inline char currentOutput[currentOutputLen]{};
    inline size_t currentOutputOffset = 0;
    inline bool currentOutputTruncated = false;
#else
    inline char currentOutput[1]{};
#endif

    struct InferenceValues
    {
        int classId = -1;
        char label[MAX_LABEL_LENGTH]{};
        float value = 0;
        float x = 0;
        float y = 0;
    };

    struct InferenceOutput
    {
        InferenceValues foundValues[MAX_BOXES]{};
        size_t count = 0;
        int status = 0; // 0 = OK, less than 0 = Error
        unsigned long totalLatency = 0;

        bool add(const InferenceValues& val)
        {
            if (count < 10)
            {
                foundValues[count++] = val;
                return true;
            }

            return false;
        }

        void clear()
        {
            count = 0;
        }
    };

    inline void initOutputStr()
    {
#ifdef INFERENCE_ENABLE_LOG
        memset(currentOutput, 0, currentOutputLen);
        currentOutputTruncated = false;
        currentOutputOffset = 0;
#endif

        currentStartTimer = millis();
    }

#ifdef INFERENCE_ENABLE_LOG
    inline void printError(const char* error, const int& errorNum, InferenceOutput& output)
    {
        currentOutputOffset = snprintf(currentOutput, currentOutputLen, "%s: %d", error, errorNum);
        MLOGF("%s: %d\n", error, errorNum);
        output.status = errorNum;
    }

    inline void printLog(const char* log, const bool ln = true, ...)
    {
        va_list args;
        va_start(args, log);

        va_list args_copy;
        va_copy(args_copy, args);

        size_t bufSize = vsnprintf(nullptr, 0, log, args_copy) + 2;
        if (currentOutputOffset + bufSize <= currentOutputLen)
        {
            char buf[bufSize]{};
            vsnprintf(buf, bufSize, log, args_copy);

            size_t msgLen = strlen(buf);
            buf[msgLen] = ';';
            buf[msgLen + 1] = '\0';

            MVLOGF(log, args);
            if (ln)
            {
                MLOGN();
            }

            strncpy(currentOutput + currentOutputOffset, buf, bufSize);
            currentOutputOffset += bufSize - 1;
        }
        else if (!currentOutputTruncated)
        {
            constexpr char truncatedStr[] = "...(truncated)";
            constexpr size_t truncatedStrLen = strlen(truncatedStr);

            size_t outLen = strlen(currentOutput);
            size_t insertPos = (outLen > truncatedStrLen) ? (outLen - truncatedStrLen) : 0;
            strcpy(currentOutput + insertPos, truncatedStr);
            currentOutputTruncated = true;
        }

        va_end(args_copy);
        va_end(args);

        // Serial.printf("Current timer: %lu\n", millis() - currentStartTimer);
    }
#endif

    inline bool keep_cell(
        const uint8_t* grid,
        int rows,
        int cols,
        int r,
        int c,
        int class_idx,
        float value
    )
    {
        constexpr int K = 3;
        constexpr int R = K / 2;
        constexpr float EPSS = 1e-6f;

        // Global priority (lower = higher priority)
        int my_prio = r * cols + c;

        for (int dr = -R; dr <= R; dr++)
        {
            int rr = r + dr;
            if (rr < 0 || rr >= rows) continue;

            for (int dc = -R; dc <= R; dc++)
            {
                int cc = c + dc;
                if (cc < 0 || cc >= cols) continue;

                if (rr == r && cc == c) continue;

                float other = ModelUtil::unquantizeValue(grid[(rr * cols + cc) * MODEL_CLASS_COUNT + class_idx]);

                // 1) Stronger neighbor → suppress
                if (other > value + EPSS)
                {
                    return false;
                }

                // 2) Equal value → global tie-break
                if (fabsf(other - value) < EPSS)
                {
                    int other_prio = rr * cols + cc;
                    if (other_prio < my_prio)
                    {
                        return false;
                    }
                }
            }
        }
        return true;
    }

    inline int runClassifierAndExtractInfo(const uint8_t* imageBuffer, InferenceOutput& output)
    {
        INFERENCE_LOG_FN("Running inference.");

        uint8_t* outputBuffer = nullptr;
        int resultStatus = ModelUtil::runInference(&outputBuffer, [imageBuffer](uint8_t* dst)
        {
            for (int i = 0; i < MODEL_DATA_INPUT_WIDTH * MODEL_DATA_INPUT_HEIGHT; i++)
            {
                uint8_t b = imageBuffer[3 * i + 0];
                uint8_t g = imageBuffer[3 * i + 1];
                uint8_t r = imageBuffer[3 * i + 2];

                dst[3 * i + 0] = r; // BGR → RGB
                dst[3 * i + 1] = g;
                dst[3 * i + 2] = b;
            }
        });

        if (resultStatus != ModelUtil::OK)
        {
            INFERENCE_ERROR_FN("Error running inference", resultStatus, *output);
            return resultStatus;
        }

        INFERENCE_LOG_FN("Extracting object positions.", true);
        for (size_t i = 0; i < MODEL_OUTPUT_ROWS; i++)
        {
            for (size_t j = 0; j < MODEL_OUTPUT_COLS; j++)
            {
                uint8_t* row = outputBuffer + (i * MODEL_OUTPUT_COLS + j) * MODEL_CLASS_COUNT;
                for (size_t ci = 1; ci < MODEL_CLASS_COUNT; ci++)
                {
                    float threshold = thresholds[ci];
                    float value = ModelUtil::unquantizeValue(row[ci]);
                    if (value < threshold)
                    {
                        continue;
                    }

                    if (!keep_cell(outputBuffer, MODEL_OUTPUT_ROWS, MODEL_OUTPUT_COLS, i, j, ci, value))
                    {
                        continue;
                    }

                    InferenceValues values{};
                    values.classId = ci;
                    values.value = value;
                    values.x = (j + 0.5f) * MODEL_DOWNSCALING_FACTOR;
                    values.y = (i + 0.5f) * MODEL_DOWNSCALING_FACTOR;
                    snprintf(values.label, MAX_LABEL_LENGTH, "%s", classes[ci]);

                    INFERENCE_LOG_FN(
                        "Found object of class %d at cell with value %f at row/col=%d/%d, calculated x/y=%f/%f",
                        true,
                        ci,
                        value,
                        i,
                        j,
                        values.x,
                        values.y);

                    output.add(values);
                }
            }
        }

        output.status = ModelUtil::OK;
        return ModelUtil::OK;
    }

    inline void runInferenceFromImage(
        InferenceOutput& output,
        uint8_t* image,
        size_t imageLen,
        uint8_t** outProcessed = nullptr,
        size_t* outSize = nullptr)
    {
        initOutputStr();

        INFERENCE_LOG_FN("Converting picture");
        IMAGE_UTIL::ImageDimensions dimensions;
        bool imageDimensionsResult = IMAGE_UTIL::jpegGetSize(image, imageLen, dimensions);
        if (!imageDimensionsResult)
        {
            INFERENCE_ERROR_FN("Error opening image", -imageDimensionsResult, output);
            return;
        }

        Serial.printf("Extracted input dimensions are (w/h): %d / %d\n", dimensions.width, dimensions.height);
        size_t decodeBufferSize = dimensions.width * dimensions.height * 3;
        if (decodeBufferSize > MAX_INFERENCE_DECODE_LENGTH)
        {
            INFERENCE_ERROR_FN("Image too large.", -66, output);
            return;
        }

        auto decodeBuffer = (uint8_t*)ps_malloc(decodeBufferSize);
        if (decodeBuffer == nullptr)
        {
            INFERENCE_ERROR_FN("Failed to allocate decode buffer.", -55, output);
            return;
        }

        memset(decodeBuffer, 0, decodeBufferSize);
        bool decodeResult = fmt2rgb888(image, imageLen, PIXFORMAT_JPEG, decodeBuffer);
        if (!decodeResult)
        {
            INFERENCE_ERROR_FN("Error decoding image.", -1, output);
            return;
        }

        if (dimensions.width > MODEL_DATA_INPUT_WIDTH || dimensions.height > MODEL_DATA_INPUT_HEIGHT)
        {
            IMAGE_UTIL::crop_resize_square_bgr_inplace(
                decodeBuffer,
                dimensions.width,
                dimensions.height,
                MODEL_DATA_INPUT_WIDTH,
                MODEL_DATA_INPUT_HEIGHT);
        }

        runClassifierAndExtractInfo(decodeBuffer, output);
        output.totalLatency = millis() - currentStartTimer;
        INFERENCE_LOG_FN("Total time taken: %lu", true, output.totalLatency);

        if (outProcessed != nullptr && outSize != nullptr)
        {
            *outProcessed = decodeBuffer;
            *outSize = decodeBufferSize;
        }
        else
        {
            free(decodeBuffer);
        }
    }

    inline float triggerCertainty(const InferenceOutput& output)
    {
        if (output.count == 0)
        {
            return 0.0f;
        }

        float maxCatConfidence = 0.0f;
        float maxHumanConfidence = 0.0f;

        for (size_t i = 0; i < output.count; i++)
        {
            const InferenceValues &values = output.foundValues[i];

            if (strcmp(values.label, classes[catIndex]) == 0)
            {
                maxCatConfidence = std::max(maxCatConfidence, values.value);
            }
            else if (strcmp(values.label, classes[humanIndex]) == 0)
            {
                maxHumanConfidence = std::max(maxHumanConfidence, values.value);
            }
        }

        if (maxCatConfidence == 0.0f)
            return 0.0f;

        // Trigger certainty:
        // high when cat is strong and human is weak
        return maxCatConfidence * (1.0f - maxHumanConfidence); // [0.0, 1.0]
    }

    inline void drawMarkers(const InferenceOutput &output, uint8_t *img)
    {
        for (size_t i = 0; i < output.count; i++)
        {
            const InferenceValues &values = output.foundValues[i];
            if (values.classId > 0 && values.value >= thresholds[values.classId])
            {
                IMAGE_UTIL::BGR c = classColors[values.classId];
                IMAGE_UTIL::draw_cross(img, MODEL_INPUT_WIDTH, MODEL_INPUT_HEIGHT, values.x, values.y, c, 8);
                IMAGE_UTIL::draw_circle(img, MODEL_INPUT_WIDTH, MODEL_INPUT_HEIGHT, values.x, values.y, c, 3);
            }
        }
    }
}

#endif //INFERENCE_UTIL_H
