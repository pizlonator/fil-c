// Note: this source file contains instantiations of all possible processors.
// You should NOT link it in your project.
// It is used mostly for testing purposes.

#include "buffer/ProcessorSelector.h"

BaseBufferProcessor* GenerateProcessor(int srcFormat, int dstFormat, int maxBytes, int checkMode, int multiplier, int *errorCounter) {
    #define TRY_PROC(from, to, maxB, mode, mult) \
        if (srcFormat == from && dstFormat == to && maxBytes == maxB && checkMode == mode && multiplier == mult) \
            res = ProcessorSelector<from, to>::template WithOptions<mode, maxB, mult>::Create(errorCounter);

    #define TRY_OPT(from, to, maxB, mode) \
        TRY_PROC(from, to, maxB, mode, 1); \
        TRY_PROC(from, to, maxB, mode, 4);

    #define TRY_DB(from, to, maxB) \
        TRY_OPT(from, to, maxB, cmFast); \
        TRY_OPT(from, to, maxB, cmFull); \
        TRY_OPT(from, to, maxB, cmValidate);

    #define TRY_DIR(from, to) \
        TRY_PROC(from, to, 0, 2, 1); \
        TRY_DB(from, to, 1); \
        TRY_DB(from, to, 2); \
        TRY_DB(from, to, 3);

    BaseBufferProcessor *res = 0;

    TRY_DIR(dfUtf8, dfUtf16);
    TRY_DIR(dfUtf8, dfUtf32);
    TRY_DIR(dfUtf16, dfUtf8);
    TRY_DIR(dfUtf32, dfUtf8);

    return res;
}
