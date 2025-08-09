#include "../buffer/BaseBufferProcessor.h"
#include "../buffer/ProcessorPlugins.h"
#include <string.h>
#include <assert.h>
#include <limits.h>

BaseBufferProcessor::BaseBufferProcessor() {
    errorCallback = 0;
    errorContext = 0;
    Clear();
}
BaseBufferProcessor::~BaseBufferProcessor() {
}

void BaseBufferProcessor::Clear() {
    inputBuffer = 0;
    inputSize = 0;
    memset(outputBuffer, 0, sizeof(outputBuffer));
    memset(outputSize, 0, sizeof(outputSize));
    inputDone = 0;
    memset(outputDone, 0, sizeof(outputDone));
    SetHint();
    pluginsCount = 0;
}

void BaseBufferProcessor::SetInputBuffer(const char *ptr, int size) {
    inputBuffer = ptr;
    inputSize = size;
}

void BaseBufferProcessor::SetOutputBuffer(char *ptr, int size, int index) {
    assert(index >= 0 && index < MaxStreamsCount);
    outputBuffer[index] = ptr;
    outputSize[index] = size;
}
#ifdef __GNUC__
__attribute__((unused))
#endif 
bool BaseBufferProcessor::CheckBuffers() const {
    int streams = GetStreamsCount();
    //check input buffer validity
    if (!inputBuffer || inputSize < 1 || inputSize > GetBufferMaxSize())
        return false;
    //check output buffers validity
    for (int i = 0; i < streams; i++)
        if (!outputBuffer[i] || outputSize[i] < 1 || outputSize[i] > GetBufferMaxSize())
            return false;
    //check for overlapping
    for (int i = -1; i < streams; i++)
        for (int j = i+1; j < streams; j++) {
            const char *leftA = (i < 0 ? inputBuffer : outputBuffer[i]);
            const char *rightA = leftA + (i < 0 ? inputSize : outputSize[i]);
            const char *leftB = outputBuffer[j];
            const char *rightB = leftB + outputSize[j];
            if (leftA >= rightB || leftB >= rightA)
                continue;
            return false;
        }
    return true;
}

int BaseBufferProcessor::GetBufferMaxSize() const {
    return INT_MAX / 8;
}

void BaseBufferProcessor::SetHint(bool isLastBlock) {
    lastBlockMode = isLastBlock;
}

bool BaseBufferProcessor::Process() {
    for (int i = 0; i < pluginsCount; i++)
        plugins[i]->Pre();

    assert(CheckBuffers());
    inputDone = 0;
    memset(outputDone, 0, sizeof(outputDone));

    bool res = _Process();
    while (!res && errorCallback) {
        const char *inputPtr = inputBuffer + inputDone;
        char *outputPtr = outputBuffer[0] + outputDone[0];
        bool fixed = errorCallback(errorContext, inputPtr, inputSize - inputDone, outputPtr, outputSize[0] - outputDone[0]);
        inputDone = inputPtr - inputBuffer;
        outputDone[0] = outputPtr - outputBuffer[0];
        assert(inputDone >= 0 && inputDone <= inputSize);
        assert(outputDone[0] >= 0 && outputDone[0] <= outputSize[0]);
        if (!fixed)
            break;
        res = _Process();
    }

    for (int i = pluginsCount-1; i >= 0; i--)
        plugins[i]->Post();

    return res;
}

int BaseBufferProcessor::GetInputDoneSize() const {
    return inputDone;
}

int BaseBufferProcessor::GetOutputDoneSize(int index) const {
    return outputDone[index];
}

void BaseBufferProcessor::AddPlugin(BasePlugin &addedPlugin) {
    assert(pluginsCount < MaxPluginsCount);
    plugins[pluginsCount++] = &addedPlugin;
}

bool BaseBufferProcessor::SetErrorCallback(pfErrorCallback callback, ctxErrorCallback context) {
    if (GetStreamsCount() > 1)
        return false;   //not supported
    errorCallback = callback;
    errorContext = context;
    return true;
}
