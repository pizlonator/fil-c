#pragma once

#include "../buffer/BaseBufferProcessor.h"

// Base class for all plugins (they can be attached to BaseBufferProcessor).
class BasePlugin {
public:
    virtual ~BasePlugin() {}
    //callback called before conversion inside 'Process' method
    virtual void Pre() {}
    //callback called after conversion inside 'Process' method
    virtual void Post() {}
};

//plugins intended to simplify setting input data
class InputPlugin : public BasePlugin {};
//plugins intended to simplify setting output data
class OutputPlugin : public BasePlugin {};
//Note that you can independently choose one input plugin and one output plugin



// A plugin which helps to convert a given contiguous byte array in memory.
// It maintains a pointer to the beginning of yet unconverted data.
// Before each 'Process' call, it installs a chunk (starting at the pointer) as input buffer.
// So, contiguous calls to 'Process' would result in chunk-by-chunk conversion of the byte array.
//
//                     (chunk)
//          +----------|------|------------+
//          |..........|%%%%%%|%%%%%%%%%%%%|    (before)
//          +----------|------|------------+      ||
//                     >-->-->|                   || one 'Process' call
//          +-----------------|------------+      \/
//          |.................|%%%%%%%%%%%%|    (after)
//          +-----------------|------------+
//     
class ContiguousInput : public InputPlugin {
    BaseBufferProcessor *processor;
    const char *srcBuffer;
    long long srcSize;
    long long srcDone;
    int chunkSize;
    bool finished;

public:
    //takes pointer and size of contiguous input buffer to be converted
    ContiguousInput(BaseBufferProcessor &owner, const char *buffer, long long size) : processor(&owner) {
        srcBuffer = buffer;
        srcSize = size;
        srcDone = 0;
        chunkSize = processor->GetInputBufferRecommendedSize();
        finished = (srcSize == 0);
        processor->AddPlugin(*this);
    }
    virtual void Pre() {
        assert(!finished);
        int bytes = GetNextChunkSize();
        processor->SetInputBuffer(srcBuffer + srcDone, bytes);
        finished = (srcDone + bytes == srcSize);
        processor->SetHint(finished);
    }
    virtual void Post() {
        srcDone += processor->GetInputDoneSize();
    }
    //returns size of already processed prefix (i.e. converted successfully)
    long long GetProcessedInputSize() const {
        return srcDone;
    }
    //returns size of the remaining suffix (i.e. not converted yet)
    long long GetRemainingDataSize() const {
        return srcSize - srcDone;
    }
    //returns the size of the whole input buffer (as given by user)
    long long GetBufferSize() const {
        return srcSize;
    }
    //returns the size of the chunk which would be processed during the next 'Process' call.
    int GetNextChunkSize() const {
        long long bytes = srcSize - srcDone;
        if (bytes > chunkSize)
            bytes = chunkSize;
        return int(bytes);
    }
    //returns true iff the conversion is finished
    //note that some bytes may still remain unprocessed (i.e. incomplete code point)
    bool Finished() const {
        return finished;
    }
};


// A plugin which helps to interactively put input data into internal buffer.
// It owns a dynamically allocated buffer where the user puts his input data.
// During each 'Process' call, all the contents of the buffer are processed.
// Unconverted data (i.e. incomplete code point) at the end of the buffer is
// automatically moved to the beginning of the buffer.
//
//          +-|-----------------------+
//          |a|.......................|    (before)
//          +-|-----------------------+      ||
//             (new data added)              || user adds new data "b-r"
//          +------------------|------+      ||
//          |abcdefghijklmnopqr|......|   ---++
//          +------------------|------+      ||
//          >-->-->-->-->-->|                || 'Process' call
//          +---------------|--|------+      ||
//          |...............|qr|......|   ---++
//          +---------------|--|------+      ||
//                                           || unprocessed data "qr" moved
//          +--|----------------------+      \/
//          |qr|......................|    (after)
//          +--|----------------------+
//
// The following sample shows how to append new input data into buffer (from file f):
//      input.GetBuffer(ptr, size);
//      int added = fread(ptr, 1, size, f);
//      input.ConfirmInputBytes(added);
//
class InteractiveInput : public InputPlugin {
    BaseBufferProcessor *processor;
    char *inputBuffer;
    int inputSize;
    int inputSet;
    bool lastBlock;
    long long totalSizeDone;

public:
    InteractiveInput(BaseBufferProcessor &owner) : processor(&owner) {
        inputSize = processor->GetInputBufferRecommendedSize();
        inputBuffer = new char[inputSize];
        inputSet = 0;
        totalSizeDone = 0;
        processor->AddPlugin(*this);
    }
    ~InteractiveInput() {
        delete[] inputBuffer;
    }
    //returns pointer and size of a subbuffer where new data must be placed
    //you can write to there K bytes of new input data (for any K <= "size")
    //then you must call ConfirmInputBytes with "bytes" = K
    void GetBuffer(char *&buffer, int &size) const {
        buffer = inputBuffer + inputSet;
        size = inputSize - inputSet;
    }
    //informs that exactly "bytes" bytes of input were appended to the buffer
    //argument "isLastBlock" would be passed to method SetHint of processor (see docs there)
    void ConfirmInputBytes(int bytes, bool isLastBlock = true) {
        inputSet += bytes;
        lastBlock = isLastBlock;
    }
    virtual void Pre() {
        processor->SetInputBuffer(inputBuffer, inputSet);
        processor->SetHint(lastBlock);
    }
    virtual void Post() {
        int inputDone = processor->GetInputDoneSize();
        int remains = inputSet - inputDone;
        if (remains > 0)
            memmove(inputBuffer, inputBuffer + inputDone, remains);
        inputSet = remains;
        totalSizeDone += inputDone;
    }
    //returns how many bytes are currently within the buffer (and are still unprocessed)
    int GetRemainingDataSize() const {
        return inputSet;
    }
    //returns how many bytes were processed in total during the whole life of the plugin
    long long GetProcessedInputSize() const {
        return totalSizeDone;
    }
};


// A plugin which helps to save result of conversion into a given contiguous byte array in memory.
// It maintains a pointer to the end of the already saved output data.
// Before each 'Process' call, it installs the remaining part of the byte array as output buffer.
// So, contiguous calls to 'Process' would result in saving output in the array chunk-by-chunk.
//
//                     (chunk)
//          +----------|------|------------+
//          |##########|......|............|    (before)
//          +----------|------|------------+      ||
//                     >-->-->|                   || one 'Process' call
//          +-----------------|------------+      \/
//          |#################|............|    (after)
//          +-----------------|------------+
//
// If multi-stream processor is used, then conversion results are first saved into internal buffers,
// and then automatically copied into the user's byte array.
// Note that this means minor performance loss due to additional memcpy of the output data.
// You do not need to do anything special to handle this case, no difference is visible from the outside.
//     
// Note that the static method ContiguousOutput::GetMaxOutputSize can be used without plugin
// in order to see which size of output buffer is needed to surely accomodate result of conversion.
//
class ContiguousOutput : public OutputPlugin {
    BaseBufferProcessor *processor;
    char *dstBuffer;
    long long dstSize;
    long long dstDone;
    int maxSize;

    int streamsCnt, streamOutputSize;
    char *multiBuffer[BaseBufferProcessor::MaxStreamsCount];

public:
    //returns minimal size of contiguous output buffer, such that for any input data of size "inputSize":
    //  1. output buffer would be large enough to hold converted result
    //  2. during conversion, there would always be enough empty space remaining in the buffer
    //     (i.e. lower limit returned by GetOutputBufferMinSize() is not violated)
    static long long GetMaxOutputSize(const BaseBufferProcessor &processor, long long inputSize) {
        return processor.GetStreamsCount() * processor.GetOutputBufferMinSize(inputSize);
    }
    //takes pointer and size of contiguous output buffer where the result of conversion must be saved
    ContiguousOutput(BaseBufferProcessor &owner, char *buffer, long long size) : processor(&owner) {
        dstBuffer = buffer;
        dstSize = size;
        dstDone = 0;
        streamsCnt = processor->GetStreamsCount();
        streamOutputSize = 0;
        maxSize = processor->GetBufferMaxSize();
        if (streamsCnt > 1) {
            streamOutputSize = (int)processor->GetOutputBufferMinSize(processor->GetInputBufferRecommendedSize());
            for (int i = 0; i < streamsCnt; i++)
                multiBuffer[i] = new char[streamOutputSize];
        }
        processor->AddPlugin(*this);
    }
    ~ContiguousOutput() {
        if (streamsCnt > 1)
            for (int i = 0; i < streamsCnt; i++)
                delete[] multiBuffer[i];
    }
    virtual void Pre() {
        if (streamsCnt > 1) {
            for (int i = 0; i < streamsCnt; i++)
                processor->SetOutputBuffer(multiBuffer[i], streamOutputSize, i);
        }
        else {
            long long remains = dstSize - dstDone;
            if (remains > maxSize)
                remains = maxSize;
            processor->SetOutputBuffer(dstBuffer + dstDone, int(remains));
        }
    }
    virtual void Post() {
        if (streamsCnt > 1) {
            for (int i = 0; i < streamsCnt; i++) {
                int done = processor->GetOutputDoneSize(i);
                assert(dstDone + done <= dstSize);
                memcpy(dstBuffer + dstDone, multiBuffer[i], done);
                dstDone += done;
            }
        }
        else
            dstDone += processor->GetOutputDoneSize();
    }
    //returns how many bytes of output data already saved (in the prefix of the buffer)
    long long GetFilledOutputSize() const {
        return dstDone;
    }
    //returns the size of the whole output buffer (as given by user)
    long long GetBufferSize() const {
        return dstSize;
    }
};


// A plugin which helps to interactively consume conversion results from internal buffer(s).
// It owns dynamically allocated buffer (or buffers) which receive output data.
// During each 'Process' call, the conversion results are stored into this buffer(s).
// User must read all the contents from this buffer(s) prior to next 'Process' call.
//
//         single-stream case                                   multi(4)-stream case
//    +-------------------------+                        +-----+ +-----+ +-----+ +-----+
//    |.........................|         (before)       |.....| |.....| |.....| |.....|
//    +-------------------------+            ||          +-----+ +-----+ +-----+ +-----+
//    >-->-->-->-->-->-->|          'Process'|| call     >-->|   >--->|  >->|    >-->|
//    +------------------|------+            \/          +---|-+ +----|+ +--|--+ +---|-+
//    |##################|......|         (after)        |###|.| |####|| |##|..| |###|.|
//    +------------------|------+                        +---|-+ +----|+ +--|--+ +---|-+
//        (output data)                                     \       |      |       /
//     (to be ovewritten)                                (output data, to be ovewritten)
//
// The following sample shows how to receive conversion results from buffers (and dump them to file f):
//        for (int k = 0; k < output.GetStreamsCount(); k++) {
//            output.GetBuffer(ptr, size, k);
//            fwrite(ptr, 1, size, f);
//        }
//
class InteractiveOutput : public OutputPlugin {
    BaseBufferProcessor *processor;
    int streamsCnt, streamOutputSize;
    char *multiBuffer[BaseBufferProcessor::MaxStreamsCount];
    long long totalSizeDone;

public:
    InteractiveOutput(BaseBufferProcessor &owner) : processor(&owner) {
        streamsCnt = processor->GetStreamsCount();
        streamOutputSize = (int)processor->GetOutputBufferMinSize(processor->GetInputBufferRecommendedSize());
        for (int i = 0; i < streamsCnt; i++)
            multiBuffer[i] = new char[streamOutputSize];
        totalSizeDone = 0;
        processor->AddPlugin(*this);
    }
    ~InteractiveOutput() {
        for (int i = 0; i < streamsCnt; i++)
            delete[] multiBuffer[i];
    }
    virtual void Pre() {
        for (int i = 0; i < streamsCnt; i++)
            processor->SetOutputBuffer(multiBuffer[i], streamOutputSize, i);
    }
    virtual void Post() {
        for (int i = 0; i < streamsCnt; i++)
            totalSizeDone += processor->GetOutputDoneSize(i);
    }
    //returns the number of output streams = the number of separate output buffers
    int GetStreamsCount() const {
        return streamsCnt;
    }
    //returns pointer and size of the output data obtained
    //by "index"-th stream during the last 'Process' call
    //Note that you have to concatenate outputs of all the streams in their order
    //to get the real output of the last 'Process' call.
    void GetBuffer(const char *&buffer, int &size, int index = 0) const {
        buffer = multiBuffer[index];
        size = processor->GetOutputDoneSize(index);
    }
    //returns total number of bytes written to output buffer(s) during the whole life of the plugin
    long long GetFilledOutputSize() const {
        return totalSizeDone;
    }
};
