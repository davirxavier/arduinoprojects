//
// Created by xav on 12/6/25.
//

#ifndef INFERENCE_UTIL_H
#define INFERENCE_UTIL_H

#include <general/cam_config.h>
#include <esp-cat-detector-testing_inferencing.h>
#include <edge-impulse-sdk/dsp/image/processing.hpp>
#include <general/image_util.h>

// #define MODEL_IS_GRAYSCALE
#define MAX_BOXES 10
#define MODEL_DOWNSCALING_FACTOR 8.0
#define MODEL_OUTPUT_ROWS ((int) EI_CLASSIFIER_INPUT_HEIGHT/(int)MODEL_DOWNSCALING_FACTOR)
#define MODEL_OUTPUT_COLS ((int) EI_CLASSIFIER_INPUT_WIDTH/(int)MODEL_DOWNSCALING_FACTOR)
#define MODEL_CLASS_COUNT 3
#define MAX_LABEL_LENGTH 32

namespace InferenceUtil
{
#if EI_CLASSIFIER_OBJECT_DETECTION == 1 || EI_CLASSIFIER_FREEFORM_OUTPUT == 1
    constexpr char classes[MODEL_CLASS_COUNT][MAX_LABEL_LENGTH] = {"bg", "cat", "human"};
    constexpr float thresholds[] = {1, 0.75, 0.5};
    constexpr uint8_t catIndex = 1;
    constexpr uint8_t humanIndex = 2;
#else
    constexpr char classes[MODEL_CLASS_COUNT][MAX_LABEL_LENGTH] = {"cat", "human", "unknown"};
    constexpr float thresholds[] = {0.75, 0.5, 0};
    constexpr uint8_t catIndex = 0;
    constexpr uint8_t humanIndex = 1;
#endif

    constexpr size_t currentOutputLen = 1024;
    inline unsigned long currentStartTimer = 0;
    inline char currentOutput[currentOutputLen]{};
    inline size_t currentOutputOffset = 0;
    inline bool currentOutputTruncated = false;

    struct InferenceValues
    {
        char label[MAX_LABEL_LENGTH]{};
        float value = 0;

#if EI_CLASSIFIER_OBJECT_DETECTION == 1 || EI_CLASSIFIER_FREEFORM_OUTPUT == 1
        float x = 0;
        float y = 0;
#endif
    };

    struct InferenceOutput
    {
        InferenceValues foundValues[MAX_BOXES]{};
        size_t count = 0;
        int status = 0; // 0 = OK, less than 0 = Error

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
        memset(currentOutput, 0, currentOutputLen);
        currentOutputTruncated = false;
        currentOutputOffset = 0;
        currentStartTimer = millis();
    }

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

        Serial.printf("Current timer: %lu\n", millis() - currentStartTimer);
    }

    inline bool has_higher_priority(
        int r1,
        int c1,
        int r2,
        int c2,
        int center_r,
        int center_c
    )
    {
        int d1 = (r1 - center_r) * (r1 - center_r) +
            (c1 - center_c) * (c1 - center_c);
        int d2 = (r2 - center_r) * (r2 - center_r) +
            (c2 - center_c) * (c2 - center_c);

        if (d1 != d2) return d1 < d2;
        if (r1 != r2) return r1 < r2;
        return c1 < c2;
    }

    inline bool keep_cell(
        const float* grid,
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

                float other =
                    grid[(rr * cols + cc) * MODEL_CLASS_COUNT + class_idx];

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

    inline std::function<int(size_t offset, size_t length, float* out_ptr)> getDataFromBuffer(uint8_t* inputBuffer)
    {
        // From esp32cam example
        return [inputBuffer](size_t offset, size_t length, float* out_ptr)
        {
            size_t pixel_ix = offset * 3;
            size_t pixels_left = length;
            size_t out_ptr_ix = 0;

            while (pixels_left != 0)
            {
#ifdef MODEL_IS_GRAYSCALE
                    uint8_t b = inputBuffer[pixel_ix + 0];
                    uint8_t g = inputBuffer[pixel_ix + 1];
                    uint8_t r = inputBuffer[pixel_ix + 2];

                    uint8_t gray8 = JPEG_DECODE_UTIL::rgb_to_gray(r, g, b);
                    out_ptr[out_ptr_ix] = gray8;

                    inputBuffer[pixel_ix + 0] = gray8; // TODO don't update input buffer
                    inputBuffer[pixel_ix + 1] = gray8;
                    inputBuffer[pixel_ix + 2] = gray8;
#else
                // Swap BGR to RGB here
                // due to https://github.com/espressif/esp32-camera/issues/379
                out_ptr[out_ptr_ix] = (inputBuffer[pixel_ix + 2] << 16) + (inputBuffer[pixel_ix + 1] << 8) + inputBuffer
                    [pixel_ix];
#endif

                // go to the next pixel
                out_ptr_ix++;
                pixel_ix += 3;
                pixels_left--;
            }

            vTaskDelay(1);
            return 0;
        };
    }

    inline int runClassifierAndExtractInfo(uint8_t* imageBuffer, InferenceOutput* output)
    {
        constexpr size_t freeformBufferSize = MODEL_OUTPUT_ROWS * MODEL_OUTPUT_COLS * MODEL_CLASS_COUNT * sizeof(float);
        static float freeformBuffer[freeformBufferSize]{};

        printLog("Running inference.");
        memset(freeformBuffer, 0, freeformBufferSize);

        ei_impulse_handle_t& impulseHandle = ei_default_impulse;
        ei::matrix_t freeformOutput{MODEL_OUTPUT_ROWS * MODEL_OUTPUT_COLS, MODEL_CLASS_COUNT, freeformBuffer};
        EI_IMPULSE_ERROR res = ei_set_freeform_output(&impulseHandle, &freeformOutput, 1);
        if (res != EI_IMPULSE_OK)
        {
            printError("Set freeform output failed", res, *output);
            return res;
        }

        ei_impulse_result_t result{};
        signal_t classifierSignal{};
        size_t totalLengthFeatures = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
        classifierSignal.total_length = totalLengthFeatures;
        classifierSignal.get_data = getDataFromBuffer(imageBuffer);

        EI_IMPULSE_ERROR classifierResult = run_classifier(&classifierSignal, &result, false);
        if (classifierResult != EI_IMPULSE_OK)
        {
            printError("Error running inference", classifierResult, *output);
            return classifierResult;
        }

        printLog("Classification ran successfully. Time taken: DSP=%dms, INF=%dms", true, result.timing.dsp,
                 result.timing.classification);

#if EI_CLASSIFIER_FREEFORM_OUTPUT == 1
        printLog("Extracting object positions.", true);

        for (size_t i = 0; i < MODEL_OUTPUT_ROWS; i++)
        {
            for (size_t j = 0; j < MODEL_OUTPUT_COLS; j++)
            {
                float* row = freeformOutput.get_row_ptr(i * MODEL_OUTPUT_COLS + j);
                for (size_t ci = 1; ci < MODEL_CLASS_COUNT; ci++)
                {
                    float threshold = thresholds[ci];
                    float value = row[ci];
                    if (value < threshold)
                    {
                        continue;
                    }

                    if (!keep_cell(
                        freeformOutput.buffer,
                        MODEL_OUTPUT_ROWS,
                        MODEL_OUTPUT_COLS,
                        i,
                        j,
                        ci,
                        value))
                    {
                        continue;
                    }

                    InferenceValues values{};
                    values.value = value;
                    values.x = j * MODEL_DOWNSCALING_FACTOR;
                    values.y = i * MODEL_DOWNSCALING_FACTOR;
                    snprintf(values.label, MAX_LABEL_LENGTH, "%s", classes[ci]);
                    printLog("Found object of class %d at cell with value %f at row/col=%d/%d, calculated x/y=%f/%f",
                             true,
                             ci,
                             row[ci],
                             i,
                             j,
                             values.x,
                             values.y);

                    output->add(values);
                }
            }
        }
#elif EI_CLASSIFIER_OBJECT_DETECTION == 0
        for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++)
        {
            ei_impulse_result_classification_t classification = result.classification[i];
            printLog("Class val: %f", true, classification.value);

            if (classification.value < thresholds[i])
            {
                continue;
            }

            InferenceValues values{};
            values.value = classification.value;
            snprintf(values.label, MAX_LABEL_LENGTH, "%s", classification.label);
            output->add(values);
        }

        printLog("Found values: %s=%f, %s=%f, %s=%f", true, humanLabel, output->humanValue, catLabel, output->catValue, unknownLabel, output->unknownValue);
#else
        printLog("Bounding box count: %lu", true, result.bounding_boxes_count);
        for (int i = 0; i < result.bounding_boxes_count; i++)
        {
            ei_impulse_result_bounding_box_t box = result.bounding_boxes[i];
            printLog("Bounding box found: %s", true, box.label);

            for (int ci = 1; ci < MODEL_CLASS_COUNT; ci++)
            {
                if (box.value < thresholds[ci])
                {
                    continue;
                }

                printLog("Found %s: prob=%f, x=%d, y=%d, w=%d, h=%d", true, box.label, box.value, box.x, box.y, box.width, box.height);

                InferenceValues values{};
                values.value = box.value;
                values.x = box.x;
                values.y = box.y;

                snprintf(values.label, MAX_LABEL_LENGTH, "%s", box.label);
                output->add(values);
            }
        }
#endif

        output->status = EI_IMPULSE_OK;
        return EI_IMPULSE_OK;
    }

    inline InferenceOutput runInferenceFromImage(uint8_t* image, size_t imageLen, uint8_t** outProcessed = nullptr, size_t* outSize = nullptr)
    {
        InferenceOutput output{};
        initOutputStr();

        printLog("Converting picture");
        IMAGE_UTIL::ImageDimensions dimensions;
        int imageDimensionsResult = IMAGE_UTIL::getImageDimensions(image, imageLen, &dimensions);
        if (imageDimensionsResult != IMAGE_UTIL::OK)
        {
            printError("Error opening image", -imageDimensionsResult, output);
            return output;
        }

        size_t decodeBufferSize = dimensions.width * dimensions.height * 3;
        auto decodeBuffer = (uint8_t*)ps_malloc(decodeBufferSize);
        memset(decodeBuffer, 0, decodeBufferSize);
        bool decodeResult = fmt2rgb888(image, imageLen, PIXFORMAT_JPEG, decodeBuffer);
        if (!decodeResult)
        {
            printError("Error decoding image.", -1, output);
            return output;
        }

        if (dimensions.width != EI_CLASSIFIER_INPUT_WIDTH || dimensions.height != EI_CLASSIFIER_INPUT_HEIGHT)
        {
            int res = ei::image::processing::crop_and_interpolate_rgb888(
                decodeBuffer,
                dimensions.width,
                dimensions.height,
                decodeBuffer,
                EI_CLASSIFIER_INPUT_WIDTH,
                EI_CLASSIFIER_INPUT_HEIGHT);

            if (res != EIDSP_OK)
            {
                printError("Error resizing image", res, output);
                return output;
            }
        }

        runClassifierAndExtractInfo(decodeBuffer, &output);
        printLog("Total time taken: %lu", true, millis() - currentStartTimer);

        if (outProcessed != nullptr && outSize != nullptr)
        {
            *outProcessed = decodeBuffer;
            *outSize = decodeBufferSize;
        }
        else
        {
            free(decodeBuffer);
        }

        return output;
    }

    inline bool shouldTrigger(const InferenceOutput &output)
    {
        if (output.count == 0)
        {
            return false;
        }

        bool hasCat = false;
        bool hasHuman = false;

        for (const InferenceValues &values : output.foundValues)
        {
            if (strcmp(values.label, classes[catIndex]) == 0)
            {
                hasCat = true;
            }
            else if (strcmp(values.label, classes[humanIndex]) == 0)
            {
                hasHuman = true;
            }
        }

        return hasCat && !hasHuman;
    }
}

#endif //INFERENCE_UTIL_H
