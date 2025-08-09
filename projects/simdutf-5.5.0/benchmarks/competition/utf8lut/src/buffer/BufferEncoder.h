#pragma once

#include <stdint.h>
#include <assert.h>
#include "../base/PerfDefs.h"
#include "../core/EncoderLut.h"
#include "../core/EncoderProcess.h"
#include "../core/ProcessTrivial.h"
#include "../base/Timing.h"
#include "../buffer/BaseBufferProcessor.h"

/**params:
 * MaxBytes = 0, 1, 2, 3
 * UnrollNum = 1, 4
 * Mode = fast, full, validate
 * InputType = 2, 4
 */

enum EncoderMode {
    emFast,     //encode only byte lengths under limit, no checks
    emFull,     //encode any UTF-8 chars (with fallback to slow version)
    emValidate, //encode any UTF-8 chars, validate input
    emAllCount, //helper
};
//Note: emValidate and emFull are completely equivalent

template<int MaxBytes, int InputType, int Mode, int UnrollNum>
class BufferEncoder : public BaseBufferProcessor {
public:
    static const int StreamsNumber = 1;

private:
    static const int InputMinChunk = 8 * InputType;
    static const int InputUnrollChunk = InputMinChunk * (UnrollNum > 0 ? UnrollNum : 1);
    static const bool ThreeBytes = (MaxBytes >= 3);

    static FORCEINLINE bool ProcessSimple(const char *&inputPtr, const char *inputEnd, char *&outputPtr, bool isLastBlock) {
        bool ok = true;
        const EncoderLutEntry *RESTRICT ptrTable = EncoderLutTable<ThreeBytes>::GetArray();
        while (inputPtr <= inputEnd - InputMinChunk) {
            ok = EncoderCore<MaxBytes, Mode != dmFast, InputType>()(inputPtr, outputPtr, ptrTable);
            if (!ok) {
                if (Mode != dmFast)
                    ok = EncodeTrivial<InputType>(inputPtr, inputPtr + InputMinChunk, outputPtr);
                if (!ok) break;
            }
        }
        if (isLastBlock)
            ok = EncodeTrivial<InputType>(inputPtr, inputEnd, outputPtr);
        return ok;
    }

public:

    BufferEncoder() {
        static_assert(MaxBytes >= 0 && MaxBytes <= 3, "MaxBytes must be between 0 and 3");
        static_assert(InputType == 2 || InputType == 4, "InputType must be either 2 or 4");
        static_assert(Mode >= 0 && Mode <= emAllCount, "Mode must be from EncoderMode enum");
        static_assert(UnrollNum == 1 || UnrollNum == 4, "UnrollNum must be 1 or 4");
        static_assert(MaxBytes > 0 || UnrollNum == 1, "UnrollNum must 1 when MaxBytes = 0");
    }


    virtual int GetStreamsCount() const {
        return StreamsNumber;
    }
    virtual int GetInputBufferRecommendedSize() const {
        return 1<<16;   //64KB
    }
    virtual long long GetOutputBufferMinSize(long long inputSize) const {
        return (inputSize / InputType) * (InputType == 2 ? 3 : 4) + 16;
    }

    virtual bool _Process() {
        TIMING_START(ENCODE);

        const char *inputPtr = inputBuffer + inputDone;
        char *outputPtr = outputBuffer[0] + outputDone[0];

        if (UnrollNum == 4) {
            const EncoderLutEntry *RESTRICT ptrTable = EncoderLutTable<ThreeBytes>::GetArray();
            for (int i = 0; i < inputSize / InputUnrollChunk; i++) {
                bool ok = 
                    EncoderCore<MaxBytes, Mode != dmFast, InputType>()(inputPtr, outputPtr, ptrTable) &&
                    EncoderCore<MaxBytes, Mode != dmFast, InputType>()(inputPtr, outputPtr, ptrTable) &&
                    EncoderCore<MaxBytes, Mode != dmFast, InputType>()(inputPtr, outputPtr, ptrTable) &&
                    EncoderCore<MaxBytes, Mode != dmFast, InputType>()(inputPtr, outputPtr, ptrTable);
                if (!ok) break;
            }
        }

        bool ok;
        if (MaxBytes > 0)
            ok = ProcessSimple(inputPtr, inputBuffer + inputSize, outputPtr, lastBlockMode);
        else
            ok = EncodeTrivial<InputType>(inputPtr, inputBuffer + inputSize, outputPtr);
        inputDone = int(inputPtr - inputBuffer);
        outputDone[0] = int(outputPtr - outputBuffer[0]);
        if (!ok) return false;

        TIMING_END(ENCODE, inputDone);
        return true;
    }
};
