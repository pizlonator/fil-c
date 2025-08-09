#include "../message/MessageConverter.h"
#include <stdio.h>

//=====================================================================================================

ConversionResult ConvertInMemory(BaseBufferProcessor &processor, const char *inputBuffer, long long inputSize, char *outputBuffer, long long outputSize) {
    ConversionResult result;
    result.status = (ConversionStatus)-1;
    result.inputSize = 0;
    result.outputSize = 0;
    
    if (inputSize < 0 || (!inputBuffer && inputSize != 0)) {
        result.status = csInputOutputNoAccess;
        return result;
    }
    if (outputSize < 0 || (!outputBuffer && outputSize != 0)) {
        result.status = csInputOutputNoAccess;
        return result;
    }

    long long reqSize = ContiguousOutput::GetMaxOutputSize(processor, inputSize);
    if (outputSize < reqSize) {
        result.status = csOverflowPossible;
        return result;
    }

    processor.Clear();
    ContiguousInput input(processor, inputBuffer, inputSize);
    ContiguousOutput output(processor, outputBuffer, outputSize);

    while (!input.Finished()) {
        //do all the work
        bool ok = processor.Process();

        //check if hard error occurred
        if (!ok) {
            result.status = csIncorrectData;
            break;
        }
    }

    result.inputSize = input.GetProcessedInputSize();
    result.outputSize = output.GetFilledOutputSize();

    if (result.status == csIncorrectData) {
        //hard error occured inside loop
    }
    else if (input.GetRemainingDataSize() != 0) {
        //some bytes remain in the input
        result.status = csIncompleteData;
    }
    else {
        //everything is OK
        result.status = csSuccess;
    }

    return result;
}
/*
long long ConvertInMemorySize(BaseBufferProcessor &processor, long long inputSize, char **outputBuffer) {
    long long reqSize = ContiguousOutput::GetMaxOutputSize(processor, inputSize);
    if (outputBuffer && *outputBuffer == 0)
        *outputBuffer = new char[reqSize];
    return reqSize;
}*/
