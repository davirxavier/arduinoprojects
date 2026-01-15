//
// Created by xav on 1/11/26.
//

#ifndef MODEL_UTIL_H
#define MODEL_UTIL_H

#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "model_data.h"

// #define MODEL_STATIC_TENSOR_ARENA
#define MODEL_USE_PSRAM

// #define MODEL_DEBUG_RAM

#ifdef MODEL_DEBUG_RAM
#include "tensorflow/lite/micro/recording_micro_interpreter.h"
#else
#include "tensorflow/lite/micro/micro_interpreter.h"
#endif

namespace ModelUtil
{
    constexpr size_t arenaSize = MODEL_DATA_MODEL_SIZE * 1.25;
    using InputCallback = std::function<void(uint8_t *inputBuffer)>;

    inline const tflite::Model *currentModel = nullptr;
    inline TfLiteTensor* currentInputTensor = nullptr;

#ifdef MODEL_DEBUG_RAM
    inline tflite::RecordingMicroInterpreter *currentInterpreter = nullptr;
#else
    inline tflite::MicroInterpreter *currentInterpreter = nullptr;
#endif

#ifdef MODEL_STATIC_TENSOR_ARENA
    inline uint8_t arena[arenaSize];
#else
    inline uint8_t *arena = nullptr;
#endif

    enum Status
    {
        OK,
        INVALID_SCHEMA_VER = -1,
        ARENA_ALLOCATION_FAILED = -2,
        TENSOR_ALLOCATION_FAILED = -3,
        MODEL_NOT_INITIALIZED = -4,
        INFERENCE_ERROR = -5,
    };

    inline float unquantizeValue(uint8_t val)
    {
        return currentInputTensor->params.scale * (val - currentInputTensor->params.zero_point);
    }

    inline int loadModel()
    {
        currentModel = tflite::GetModel(model_data::tflite);
        if (currentModel->version() != TFLITE_SCHEMA_VERSION)
        {
            MicroPrintf("Model provided is schema version %d not equal to supported version %d.", currentModel->version(), TFLITE_SCHEMA_VERSION);
            return INVALID_SCHEMA_VER;
        }

#ifndef MODEL_STATIC_TENSOR_ARENA
        if (arena == nullptr)
        {
#ifdef MODEL_USE_PSRAM
            arena = (uint8_t *) ps_malloc(arenaSize);
#else
            arena = (uint8_t *) malloc(arenaSize);
#endif
        }
#endif

        static tflite::MicroMutableOpResolver<MODEL_DATA_DISTINCT_OPS_COUNT> opResolver;
        model_data::RegisterOps(opResolver);

#ifdef MODEL_DEBUG_RAM
        currentInterpreter = new tflite::RecordingMicroInterpreter(
            currentModel,
            opResolver,
            arena,
            arenaSize);
#else
        currentInterpreter = new tflite::MicroInterpreter(
            currentModel,
            opResolver,
            arena,
            arenaSize);
#endif

        TfLiteStatus allocationStatus = currentInterpreter->AllocateTensors();
        if (allocationStatus != kTfLiteOk)
        {
            MicroPrintf("AllocateTensors() failed: %d", allocationStatus);
            return TENSOR_ALLOCATION_FAILED;
        }

#ifdef MODEL_DEBUG_RAM
        currentInterpreter->GetMicroAllocator().PrintAllocations();
#endif

        currentInputTensor = currentInterpreter->input(0);
        if (currentInputTensor == nullptr)
        {
            MicroPrintf("Could not acquire input tensor.");
            return TENSOR_ALLOCATION_FAILED;
        }

        return OK;
    }

    inline int runInference(uint8_t **output, const InputCallback &writeDataCallback)
    {
        if (currentInterpreter == nullptr || currentModel == nullptr || currentInputTensor == nullptr)
        {
            MicroPrintf("Model or interpreter is not initialized.");
            return MODEL_NOT_INITIALIZED;
        }

        writeDataCallback((uint8_t*) currentInputTensor->data.data);

        TfLiteStatus infStatus = currentInterpreter->Invoke();
        if (infStatus != kTfLiteOk)
        {
            MicroPrintf("Failed to run inference: %d", infStatus);
            return INFERENCE_ERROR;
        }

#ifdef MODEL_DEBUG_RAM
        currentInterpreter->GetMicroAllocator().PrintAllocations();
#endif

        TfLiteTensor* outputTensor = currentInterpreter->output(0);
        if (outputTensor == nullptr)
        {
            MicroPrintf("Failed to get output tensor.");
            return TENSOR_ALLOCATION_FAILED;
        }

        *output = (uint8_t*) outputTensor->data.data;
        return OK;
    }

    inline void unloadModel()
    {
        if (currentInterpreter != nullptr) {
            delete currentInterpreter;
            currentInterpreter = nullptr;
        }

#ifndef MODEL_STATIC_TENSOR_ARENA
        if (arena != nullptr) {
            free(arena);
            arena = nullptr;
        }
#endif

        currentModel = nullptr;
        currentInputTensor = nullptr;
        MicroPrintf("Model unloaded successfully.");
    }
}

#endif //MODEL_UTIL_H
