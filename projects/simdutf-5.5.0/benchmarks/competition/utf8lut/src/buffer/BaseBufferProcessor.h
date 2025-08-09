#pragma once

#include <string.h>
#include <assert.h>

class BasePlugin;

// BaseBufferProcessor: base class for any processor, which can convert buffers of data.
// As soon as you have created a suitable processor object,
// only this common interface is necessary to work with it.
// See ProcessorSelector.h for information about choosing and creating a processor.
//
// Most of the methods of processor are used only to get information or to configure it.
// The method 'Process' is the only one which actually performs the conversion.
// Everything must be properly configured by the time it is called.
// Among the state-configuring methods, only the following ones are mandatory (unless you use plugins):
//   SetInputBuffer, SetOutputBuffer;
// Among the info-quering methods, you will surely need the following ones:
//   GetOutputBufferMinSize, GetInputDoneSize, GetOutputDoneSize;
//
// One input buffer and one (or several) output buffer(s) must be set for conversion.
// Note that processor neither has any buffers within it, nor owns the buffers passed to it.
// The caller is responsible for managing lifetime of the buffers, processor only saves references to them.
//
// Each call of 'Process' method performs conversion of the input buffer.
// Below you can see the plan of all the buffers and how they are filled during 'Process' call.
// There is a major difference here between single-stream and multi-stream conversion.
// In multi-stream case, the input data is split into chunks, which are converted simultaneously,
// Each stream converts its own chunk and writes conversion result to its own output buffer.
//
// 
//           input buffer                                     input buffer                         ;
//      +-------------------+                 \          +-----------------|-+                     ;
//      |%%%%%%%%%%%%%%%%%%%|                  \         |.................|%|                     ;
//      +-------------------+         ----------\        +-----------------|-+                     ;
//                                      Process  >                         ^                       ;
//           output buffer            ----------/             output buffer                        ;
//   +---------------------------+             /      +------------------|--------+                ;
//   |...........................|            /       |##################|........|                ;
//   +---------------------------+                    +------------------|--------+                ;
//                                                     0123456789ABCDEFGH^
// (above: single-stream conversion)
//   '.' -- nonexisting, undefined, or no-longer-necessary data
//   '%' -- the input data, to be converted
//   '#' -- the output data, already converted
//   '^' -- a pointer showing how many bytes were read or written
//   '012...AB...' --- the order of output data (within the message)
// (below: multi-stream conversion, 4 streams)
//
//           input buffer                                     input buffer                         ;
//         +--------------+                   \             +-------------|+                       ;
//         |%%%%%%%%%%%%%%|                    \            |.............||                       ;
//         +--------------+           ----------\           +-------------|+                       ;
//                                      Process  >                        ^                        ;
//           output buffers           ----------/             output buffers                       ;
//  +-----+ +-----+ +-----+ +-----+            /     +---|-+ +--|--+ +----|+ +---|-+               ;
//  |.....| |.....| |.....| |.....|           /      |###|.| |##|..| |####|| |###|.|               ;
//  +-----+ +-----+ +-----+ +-----+                  +---|-+ +--|--+ +----|+ +---|-+               ;
//                                                    012^    34^     5678^   9AB^                 ;
//
//
// Plugins can be used to simplify setting input and output buffers,
// but their usage is completely optional.
// Each plugin has two methods: both of them are called inside 'Process' method.
// One of them is called before conversion, and one of them is called after it.
// See more details in ProcessorPlugins.h.
//
// When an encoding error is met in the input data,
// the default behavior of a validating processor is to simply stop before it.
// If you want to convert invalid input by fixing the encoding errors, then you should set an error callback.
// You can read more about it and find some ready-to-use error callbacks in ProcessorSelector.h.
// 
class BaseBufferProcessor {
public:
    //maximum possible number of output streams among all available processors
    static const int MaxStreamsCount = 4;
    //a hard cap on the number of plugins that may be attached to a processor
    static const int MaxPluginsCount = 2;

    virtual ~BaseBufferProcessor();

    // Resets processor to its default state.
    // Exception: error callback is NOT changed (see details below).
    void Clear();

    // Returns how many output buffers are used.
    // Usually a processor uses only one output buffer (i.e. single stream).
    // The only exception is 4-stream buffer decoder, which converts to 4 buffers.
    // Note: the same value is always returned for a given processor.
    virtual int GetStreamsCount() const = 0;

    // Specifies where to get input data from.
    // A pointer to an external buffer must be passed.
    // Note: here and below all buffer sizes are in bytes.
    void SetInputBuffer(const char *ptr, int size);

    // Specifies where to write output data to (for single stream).
    // A pointer to an external buffer must be passed.
    // Parameter 'index' is the index of the output stream being configured.
    // In case of single-stream processor, only one output buffer is set.
    // In case of multi-stream processor, output buffer must be set separately for each stream.
    void SetOutputBuffer(char *ptr, int size, int index = 0);

    // Checks whether all input + output buffers are configured correctly.
    // Returns false if:
    //   * one of the buffers is not set;
    //   * one of the buffers has invalid size (empty or insanely large);
    //   * the output buffers are not large enough to hold converted result;
    //   * some buffers overlap;
    bool CheckBuffers() const;

    // Returns the recommended size of an input buffer.
    // This size is considered to be good for performance.
    // Using much smaller of much larger buffer would slow down conversion.
    virtual int GetInputBufferRecommendedSize() const = 0;

    // Returns minimal allowed size of each output buffer, given the size of the input buffer.
    // In case of a multi-stream processor, every ONE output buffer
    // must have at least this size (and NOT all of them together).
    // This function does NOT depend on the state of processor,
    // and for a given processor it always returns the same number given the same argument.
    // The output depends on the input almost linearly (plus some constant overhead and some rounding).
    // Note: the argument may exceed maximal allowed buffer size.
    virtual long long GetOutputBufferMinSize(long long inputSize) const = 0;

    // Returns maximal allowed size of any buffer involved.
    // This size cap is orders of magnitude greater that the recommended size anyway.
    // The returned value always stays the same for a given processor.
    int GetBufferMaxSize() const;

    // Sets a hint whether the next block of data would be the last one or not.
    // If set to true, then maximum number of code points is always converted (a few bytes may remain).
    // If you set it to false, then processor may leave a few complete code points unconverted (only in case of success).
    // If the input data is invalid, then the hint does not matter (as if hint = true).
    // Note: the last call of 'Process' method must be done with this hint set to true!
    void SetHint(bool isLastBlock = true);

    // Runs convertion of the preconfigured input buffer.
    // The output is written into preconfigured output buffer (or bufferS in multi-stream case).
    // The method returns:
    //   'true':
    //      Some prefix of the input buffer has been successfully converted.
    //      See method SetHint for details about how the length of the prefix is chosen.
    //   'false':
    //      Some prefix of the input buffer has been successfully converted,
    //      but the data immediately after it cannot be converted (because it is invalid).
    // Note that if you set error callback (see below), then even invalid data is converted successfully.
    // In order to learn how many bytes were converted, use methods GetInputDoneSize and GetOutputDoneSize.
    bool Process();

    // Returns how many bytes from the input buffer were successfully converted during the last 'Process' call.
    int GetInputDoneSize() const;

    // Returns how many bytes were written to the output buffer during the last 'Process' call.
    // In case of multi-stream processor, you must specify 'index' of the output buffer.
    // Then number of bytes written to this specific buffer is returned.
    // If you concatenate the data written to all the output buffers (in their natural order),
    // you will obtain the real result of the conversion.
    int GetOutputDoneSize(int index = 0) const;

    // Adds a plugin to processor.
    // You should NOT call this method directly,
    // because it is usually called in plugin's constructor.
    // See ProcessorPlugins.h for more information.
    void AddPlugin(BasePlugin &addedPlugin);

    // The error callback receives an opaque value of this type.
    typedef void *ctxErrorCallback;
    // This is the type of the error callback function.
    //   context: the opaque value set alongside the function pointer
    //   srcBuffer: current pointer to the input data (everything before it has been converted)
    //   srcBytes: how many bytes of input data remain ahead (unconverted yet)
    //   dstBuffer: current pointer to the output data (the converted data is exactly before it)
    //   dstBytes: how many bytes of space are left in the output buffer
    // If true is returned, then conversion is continued as if no error happened.
    // If false is returned, then conversion stops (just like it stops if no error callback is set)
    // Both srcBuffer and dstBuffer can be updated by the callback,
    // but note that moving output pointer too far may lead to buffer overflows in future.
    typedef bool (*pfErrorCallback)(ctxErrorCallback context, const char *&srcBuffer, int srcBytes, char *&dstBuffer, int dstBytes);
    // Sets a callback which would be fired each time processor detects invalid input data.
    // The 'context' argument is passed to the callback function on every call.
    // Error correction is not supported for processors with multi-stream output.
    // Note: error callback is NOT cleared when 'Clear' method is called.
    bool SetErrorCallback(pfErrorCallback callback = 0, ctxErrorCallback context = 0);
    
private:
    virtual bool _Process() = 0;

    //plugins attached
    BasePlugin *plugins[MaxPluginsCount];
    int pluginsCount;
    //error callback (if present)
    pfErrorCallback errorCallback;
    ctxErrorCallback errorContext;

protected:  //accessed in implementations
    BaseBufferProcessor();

    //input buffer
    const char *inputBuffer;
    int inputSize;
    //output buffers (one per stream)
    char *outputBuffer[MaxStreamsCount];
    int outputSize[MaxStreamsCount];
    //the hint: would next block be last
    bool lastBlockMode;
    //how much bytes were processed
    int inputDone;
    int outputDone[MaxStreamsCount];
};
