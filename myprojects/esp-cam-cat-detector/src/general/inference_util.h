//
// Created by xav on 12/6/25.
//

#ifndef INFERENCE_UTIL_H
#define INFERENCE_UTIL_H

#include <general/cam_config.h>
#include <esp-cat-detector-testing_inferencing.h>
#include <edge-impulse-sdk/dsp/image/processing.hpp>
#include <general/image_util.h>

#define CAMERA_IMAGE_WIDTH 240
#define CAMERA_IMAGE_HEIGHT 240

namespace InferenceUtil
{
    constexpr char catLabel[] = "cat";
    constexpr char humanLabel[] = "human";
    inline float catThreshold = 0.75;
    inline float humanThreshold = 0.55;

    constexpr size_t currentOutputLen = 1024;
    inline char currentOutput[currentOutputLen]{};
    inline size_t currentOutputOffset = 0;
    inline bool currentOutputTruncated = false;

    struct InferenceOutput
    {
        float catValue = 0;
        float humanValue = 0;
        bool foundValues = false;
        int status = 0; // 0 = OK, less than 0 = Error
    };

    inline void initOutputStr()
    {
        memset(currentOutput, 0, currentOutputLen);
        currentOutputTruncated = false;
        currentOutputOffset = 0;
    }

    inline void printError(const char *error, const int &errorNum, InferenceOutput &output)
    {
        currentOutputOffset = snprintf(currentOutput, currentOutputLen, "%s: %d", error, errorNum);
        Serial.printf("%s: %d\n", error, errorNum);
        output.status = errorNum;
    }

    inline void printLog(const char *log, const bool ln = true, ...)
    {
        va_list args;
        va_start(args, log);

        va_list args_copy;
        va_copy(args_copy, args);

        size_t bufSize = vsnprintf(nullptr, 0, log, args_copy)+1;
        if (currentOutputOffset + bufSize <= currentOutputLen)
        {
            char buf[bufSize]{};
            vsnprintf(buf, bufSize, log, args_copy);

            Serial.vprintf(log, args);
            if (ln)
            {
                Serial.println();
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
    }

    inline std::function<int(size_t offset, size_t length, float *out_ptr)> getDataFromBuffer(uint8_t *inputBuffer)
    {
        // From esp32cam example
        return [inputBuffer](size_t offset, size_t length, float *out_ptr)
        {
            // we already have a RGB888 buffer, so recalculate offset into pixel index
            size_t pixel_ix = offset * 3;
            size_t pixels_left = length;
            size_t out_ptr_ix = 0;

            while (pixels_left != 0) {
                // Swap BGR to RGB here
                // due to https://github.com/espressif/esp32-camera/issues/379
                out_ptr[out_ptr_ix] = (inputBuffer[pixel_ix + 2] << 16) + (inputBuffer[pixel_ix + 1] << 8) + inputBuffer[pixel_ix];

                // go to the next pixel
                out_ptr_ix++;
                pixel_ix+=3;
                pixels_left--;
            }

            return 0;
        };
    }

    inline InferenceOutput runInference(uint8_t*& image, size_t& imageLen)
    {
        unsigned long startTimer = millis();
        InferenceOutput output{};
        initOutputStr();

        printLog("Taking picture.");
        camera_fb_t* cameraImage = esp_camera_fb_get();
        if (!cameraImage) {
            printError("Camera capture failed", -4, output);
            return output;
        }

        auto snapshotBuf = (uint8_t*) ps_malloc(CAMERA_IMAGE_WIDTH * CAMERA_IMAGE_HEIGHT * 3);
        if (snapshotBuf == nullptr)
        {
            printError("Failed to allocated snapshot buffer", -1, output);
            return output;
        }

        bool converted = fmt2rgb888(cameraImage->buf, cameraImage->len, PIXFORMAT_JPEG, snapshotBuf);
        if (!converted)
        {
            printError("Error converting jpeg to raw rgb888 bytes", -2, output);
            return output;
        }

        esp_camera_fb_return(cameraImage);
        if (ei::image::processing::crop_and_interpolate_rgb888(
            snapshotBuf,
            CAMERA_IMAGE_WIDTH,
            CAMERA_IMAGE_HEIGHT,
            snapshotBuf,
            EI_CLASSIFIER_INPUT_WIDTH,
            EI_CLASSIFIER_INPUT_HEIGHT) == false)
        {
            printError("Error resizing image for inferencing", -3, output);
            return output;
        }

        printLog("Running inference.");
        ei_impulse_result_t result{};
        signal_t classifierSignal{};
        size_t totalLengthFeatures = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
        classifierSignal.total_length = totalLengthFeatures;
        classifierSignal.get_data = getDataFromBuffer(snapshotBuf);

        EI_IMPULSE_ERROR classifierResult = run_classifier(&classifierSignal, &result, true);
        if (classifierResult != EI_IMPULSE_OK)
        {
            printError("Error running inference", classifierResult, output);
            return output;
        }

        printLog("Classification ran successfully. Time taken: DSP=%dms, INF=%dms", true, result.timing.dsp, result.timing.classification);
        for (int i = 0; i < result.bounding_boxes_count; i++)
        {
            ei_impulse_result_bounding_box_t box = result.bounding_boxes[i];
            bool isCat = strcmp(box.label, catLabel) == 0;
            bool isHuman = strcmp(box.label, humanLabel) == 0;

            if (isCat || isHuman)
            {
                printLog("Found %s: prob=%f, x=%d, y=%d, w=%d, h=%d", true, isHuman ? humanLabel : catLabel, box.value, box.x, box.y, box.width, box.height);
                output.foundValues = true;

                if (isHuman)
                {
                    output.humanValue = box.value > output.humanValue ? box.value : output.humanValue;
                }
                else
                {
                    output.catValue = box.value > output.catValue ? box.value : output.catValue;
                }
            }
        }

        image = snapshotBuf;
        imageLen = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT * 3;
        printLog("Classification ran successfully, time taken: %lu", true, millis() - startTimer);
        return output;
    }

    inline bool shouldTrigger(const InferenceOutput &inference)
    {
        return inference.status == 0 && inference.catValue >= catThreshold && inference.humanValue < humanThreshold;
    }
}

#endif //INFERENCE_UTIL_H
