#include "iconv/iconv.h"
#include "buffer/BaseBufferProcessor.h"
#include "buffer/ProcessorPlugins.h"
#include "buffer/BufferDecoder.h"
#include "buffer/BufferEncoder.h"
#include <assert.h>

size_t iconv(iconv_t cd, const char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft) {
    if (cd == (iconv_t)0 || cd == (iconv_t)-1) {
        errno = EBADF;
        return (size_t)-1;
    }
    BaseBufferProcessor *processor = (BaseBufferProcessor*)cd;

    if (!inbuf || !*inbuf) {
        //no state carried between calls
        return 0;
    }
    else if (!inbytesleft || !outbytesleft) {
        //error: input / output size not specified
        errno = EBADF;
        return (size_t)-1;
    }
    else if (!outbuf || !*outbuf) {
        //extension: return estimated size of output buffer
        //which is surely enough to hold all the converted data
        *outbytesleft = processor->GetOutputBufferMinSize(*inbytesleft);
        return 0;
    }

    processor->Clear();
    ContiguousInput input(*processor, *inbuf, *inbytesleft);
    ContiguousOutput output(*processor, *outbuf, *outbytesleft);

    bool overflow = false;
    bool ok;
    while (!input.Finished()) {
        //check if we surely have space for next block converted
        int inputChunk = input.GetNextChunkSize();
        int maxOutputNeeded = int(output.GetMaxOutputSize(*processor, inputChunk));
        long long freeOutputSpace = output.GetBufferSize() - output.GetFilledOutputSize();
        if (maxOutputNeeded > freeOutputSpace) {
            //note: a lot of input data may be left unconverted (up to chunk size)
            overflow = true;
            break;
        }
        //convert a block
        ok = processor->Process();
        if (!ok)
            break;
    }

    long long remains = input.GetRemainingDataSize();
    long long converted = output.GetFilledOutputSize();
    *inbuf += *inbytesleft - remains;
    *inbytesleft = remains;
    *outbuf += converted;
    *outbytesleft -= converted;

    if (overflow) {
        errno = E2BIG;
        return (size_t)-1;
    }
    else if (ok) {
        if (remains == 0) {
            //major difference: return 1 instead of number of characters converted
            return 1;
        }
        else {
            errno = EINVAL;
            return (size_t)-1;
        }
    }
    else {
        errno = EILSEQ;
        return (size_t)-1;
    }
}

int iconv_close(iconv_t cd) {
    if (cd == (iconv_t)0 || cd == (iconv_t)-1) {
        errno = EBADF;
        return -1;
    }
    BaseBufferProcessor *processor = (BaseBufferProcessor*)cd;

    delete processor;
    return 0;
}



#include "buffer/BufferDecoder.h"
#include "buffer/BufferEncoder.h"

enum Format {
    Unsupported = 0,
    Utf8,
    Utf16,
    Utf32
};
Format ParseFormat(const char *str) {
    if (strcmp(str, "UTF-8") == 0)
        return Utf8;
    if (strcmp(str, "UTF-16LE") == 0)
        return Utf16;
    if (strcmp(str, "UTF-32LE") == 0)
        return Utf32;
    return Unsupported;
}

iconv_t iconv_open(const char *tocode, const char *fromcode) {
    Format dstFormat = ParseFormat(tocode);
    Format srcFormat = ParseFormat(fromcode);
    if (srcFormat == Unsupported || dstFormat == Unsupported || (dstFormat != Utf8 && srcFormat != Utf8) || dstFormat == srcFormat) {
        errno = EINVAL;
        return (iconv_t)-1;
    }

#ifdef ICONV_SMALL
    static const int Multiplier = 1;    //compact version for small buffers
#else
    static const int Multiplier = 4;    //fastest for large buffers
#endif

    if (0);
    else if (dstFormat == Utf8 && srcFormat == Utf16)
        return new BufferEncoder<3, 2, emValidate, Multiplier>();
    else if (dstFormat == Utf8 && srcFormat == Utf32)
        return new BufferEncoder<3, 4, emValidate, Multiplier>();
    else if (dstFormat == Utf16 && srcFormat == Utf8)
        return new BufferDecoder<3, 2, dmValidate, Multiplier>();
    else if (dstFormat == Utf32 && srcFormat == Utf8)
        return new BufferDecoder<3, 4, dmValidate, Multiplier>();
    else {
        assert(0);
        return 0;
    }
}
