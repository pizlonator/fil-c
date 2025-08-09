#pragma once

#include <stdint.h>
#include <assert.h>
#include "../base/PerfDefs.h"
#include "../core/DecoderLut.h"
#include "../core/DecoderProcess.h"
#include "../core/ProcessTrivial.h"
#include "../base/Timing.h"
#include "../buffer/BaseBufferProcessor.h"

FORCEINLINE const char *FindUtf8Border(const char *pSource) {
    for (int i = 0; i < 4; i++) {
        uint8_t byte = pSource[i];
        if ((byte & 0x80U) == 0x00U)
            return pSource + i;
        if ((byte & 0xC0U) == 0xC0U)
            return pSource + i;
    }
    //four continuation bytes in a row
    //split at the end, so that left half is surely invalid
    return pSource + 4;
}

/**params:
 * MaxBytes = 0, 1, 2, 3
 * StreamsNum = 1, 4
 * Mode = fast, full, validate
 * OutputType = 2, 4
 */

enum DecoderMode {
    dmFast,     //decode only byte lengths under limit, no checks
    dmFull,     //decode any UTF-8 chars (with fallback to slow version)
    dmValidate, //decode any UTF-8 chars, validate input
    dmAllCount, //helper
};

template<int MaxBytes, int OutputType, int Mode, int StreamsNum>
class BufferDecoder : public BaseBufferProcessor {
public:
    static const int StreamsNumber = DMAX(StreamsNum, 1);

private:
    static const bool Validate = (Mode == dmValidate);

    static FORCEINLINE bool ProcessSimple(const char *&inputPtr, const char *inputEnd, char *&outputPtr, bool isLastBlock) {
        bool ok = true;
        const DecoderLutEntry<Validate> *RESTRICT ptrTable = DecoderLutTable<Validate>::GetArray();
        while (inputPtr <= inputEnd - 16) {
            ok = DecoderCore<MaxBytes, Mode != dmFast, Mode == dmValidate, OutputType>()(inputPtr, outputPtr, ptrTable);
            if (!ok) {
                if (Mode != dmFast)
                    ok = DecodeTrivial<OutputType>(inputPtr, inputPtr + 16, outputPtr);
                if (!ok) break;
            }
        }
        if (isLastBlock)
            ok = DecodeTrivial<OutputType>(inputPtr, inputEnd, outputPtr);
        return ok;
    }

    static FORCEINLINE void SplitRange(const char *buffer, int size, const char *splits[]) {
        splits[0] = buffer;
        splits[StreamsNumber] = buffer + size;
        for (int k = 1; k < StreamsNumber; k++)
            splits[k] = FindUtf8Border(buffer + uint32_t(k * size) / StreamsNumber);
    }
public:

    BufferDecoder() {
        static_assert(MaxBytes >= 0 && MaxBytes <= 3, "MaxBytes must be between 0 and 3");
        static_assert(OutputType == 2 || OutputType == 4, "OutputType must be either 2 or 4");
        static_assert(Mode >= 0 && Mode <= dmAllCount, "Mode must be from DecoderMode enum");
        static_assert(StreamsNum == 1 || StreamsNum == 4, "StreamsNum can be only 1 or 4");
        static_assert(MaxBytes > 0 || StreamsNum == 1, "StreamNum must be 1 when MaxBytes = 0");
    }

    virtual int GetStreamsCount() const {
        return StreamsNumber;
    }
    virtual int GetInputBufferRecommendedSize() const {
        return 1<<16;   //64KB
    }
    virtual long long GetOutputBufferMinSize(long long inputSize) const {
        return (inputSize / StreamsNumber + 4) * OutputType;
    }

    virtual bool _Process() {
        TIMING_START(DECODE);
        static const int MinBytesPerStream = 32;    //more than 16 after split
        if (StreamsNum > 1 && inputSize >= StreamsNum * MinBytesPerStream) {
            assert(StreamsNum == 4);
            const char *splits[StreamsNum + 1];
            SplitRange(inputBuffer, inputSize, splits);
            const DecoderLutEntry<Validate> *RESTRICT ptrTable = DecoderLutTable<Validate>::GetArray();
            #define STREAM_START(k) \
                const char *inputStart##k = splits[k]; \
                const char *inputEnd##k = splits[k+1]; \
                const char *inputPtr##k = inputStart##k; \
                char *outputPtr##k = outputBuffer[k]; \
                bool ok##k;
            STREAM_START(0)
            STREAM_START(1)
            STREAM_START(2)
            STREAM_START(3)
            while (1) {
                #define STREAM_CHECK(k) \
                    if (inputPtr##k > inputEnd##k - 16) \
                        break;
                STREAM_CHECK(0);
                STREAM_CHECK(1);
                STREAM_CHECK(2);
                STREAM_CHECK(3);
                #define STREAM_STEP(k) \
                    ok##k = DecoderCore<MaxBytes, Mode != dmFast, Mode == dmValidate, OutputType>()(inputPtr##k, outputPtr##k, ptrTable); \
                    if (!ok##k) goto slow;
                STREAM_STEP(0);
                STREAM_STEP(1);
                STREAM_STEP(2);
                STREAM_STEP(3);
slow:
                #define STREAM_SLOW(k) \
                    if (!ok##k && Mode != dmFast) \
                        ok##k = DecodeTrivial<OutputType>(inputPtr##k, inputPtr##k + 16, outputPtr##k); \
                    if (!ok##k) break;
                STREAM_SLOW(0);
                STREAM_SLOW(1);
                STREAM_SLOW(2);
                STREAM_SLOW(3);
            }
            #define STREAM_FINISH(k) \
                ok##k = ProcessSimple(inputPtr##k, inputEnd##k, outputPtr##k, true); \
                inputDone = int(inputPtr##k - inputBuffer); \
                outputDone[k] = int(outputPtr##k - outputBuffer[k]); \
                if (!ok##k || (k+1 < StreamsNum && inputPtr##k != inputEnd##k)) \
                    return false;
            STREAM_FINISH(0);
            STREAM_FINISH(1);
            STREAM_FINISH(2);
            STREAM_FINISH(3);
        }
        else {
            const char *inputPtr = inputBuffer + inputDone;
            char *outputPtr = outputBuffer[0] + outputDone[0];
            bool ok;
            if (MaxBytes > 0)
                ok = ProcessSimple(inputPtr, inputBuffer + inputSize, outputPtr, lastBlockMode);
            else
                ok = DecodeTrivial<OutputType>(inputPtr, inputBuffer + inputSize, outputPtr);
            inputDone = int(inputPtr - inputBuffer);
            outputDone[0] = int(outputPtr - outputBuffer[0]);
            if (!ok) return false;
        }
        TIMING_END(DECODE, inputDone);
        return true;
    }
};
