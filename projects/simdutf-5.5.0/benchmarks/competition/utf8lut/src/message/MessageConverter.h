#pragma once

#include "../buffer/BaseBufferProcessor.h"
#include "../buffer/ProcessorPlugins.h"


enum ConversionStatus {
    csSuccess = 0,
    // Conversion performed successfully.
    // The input data is correct and complete.
    // Output was properly written.

    csOverflowPossible = 1,
    // Conversion not attempted.
    // Output buffer is not large enough to hold any potential converted output.

    csIncompleteData = 2,
    // Input data is correct but INCOMPLETE.
    // All complete code points were converted and written to the output.

    csIncorrectData = 3,
    // Input data is INCORRECT.
    // Conversion is done partially, but cannot be finished due to invalid input.
    // Maximal prefix consisting of full and valid code points was converted (and written to output).

    csInputOutputNoAccess = 4,
    // Cannot work with given input/output.
    // In case of files: cannot find or open input file, or cannot write to output file.
    // In case of buffers: some non-empty buffer has zero pointer, or some buffer has negative size.

    csNotImplemented = 5,
    // Conversion with specified settings not yet implemented on current platform.
    // For example, if you use ConvertFile with memory mapping, then OS-specific code is needed.
};

struct ConversionResult {
    //overall result of conversion (see above)
    ConversionStatus status;
    //number of bytes read from input and successfully converted
    long long inputSize;
    //number of bytes written to output being the result of conversion
    long long outputSize;
};


//======================================= In-memory conversion ====================================

// Convert data from one contiguous buffer in memory into another contiguous buffer in memory.
// The buffers must not overlap, both buffer sizes are given in bytes.
// Output buffer must be sufficiently large, otherwise csOverflowPossible error would be returned.
// You can determine a suitable size of output buffer by calling ConvertInMemorySize function (see below).
// In order to get a processor object, use ProcessorSelector from the same-named header file.
ConversionResult ConvertInMemory(BaseBufferProcessor &processor, const char *inputBuffer, long long inputSize, char *outputBuffer, long long outputSize);

// Request information about the required size of the output buffer in ConvertInMemory routine.
// Returns the minimal allowed size of the output buffer (in bytes).
// It is surely enough to contain conversion output for any possible input buffer of specified size.
// Additionally, if outputBuffer points to char* with null value, then:
//    1. a buffer of that size will be allocated (with new char[]);
//    2. pointer to this new buffer will be saved into *outputBuffer;
//long long ConvertInMemorySize(BaseBufferProcessor &processor, long long inputSize, char **outputBuffer = 0);


//===================================== File-to-file conversion ===================================

enum FileIOType {
    ftLibC = 0,
    // Use fopen + fread + fwrite + fclose.
    // Cross-platform.

    ftMemoryMapWhole = 1,
    // Map whole file into virtual address space.
    // Uses OS-specific API.

    //TODO: perhaps implement some other types?
    //ftPosix,
    //ftWinApi,
    //ftPosixAsync,
    //ftWinApiAsync,
    //...
};

struct ConvertFilesSettings {
    //which functions to use for IO
    FileIOType type;

    //TODO: should we allow to set buffer size?
    //int bufferSize;

    ConvertFilesSettings() : type(ftLibC)/*, bufferSize(0)*/ {}
};
