*Migrated from: https://bitbucket.org/stgatilov/utf8lut/*

# **utf8lut**

The vectorized library for UTF-8 conversions on x86 (or x64) architecture.
It can quickly encode or decode large buffers of UTF-8 data, using SSSE3 instructions and lookup table (lut) to accelerate process.
Detailed description can be found in the [dirtyhandscoding blog](https://dirtyhandscoding.github.io/posts/utf8lut-vectorized-utf-8-converter-introduction.html).

**Note:** while this library can be used in a real world application (if you need to convert huge amounts of data super-fast), its main purpose is a proof-of-concept for vectorized UTF-8 conversion.

## Conversion settings

Four types of conversion are supported:

* from UTF-8 to UTF-16LE (decode)
* from UTF-8 to UTF-32LE (decode)
* from UTF-16LE to UTF-8 (encode)
* from UTF-32LE to UTF-8 (encode)

All these three UTF encoding are understood precisely as the Unicode standards define, in particular:

1. Only valid unicode values are allowed. Code points U+D800 through U+DFFF and the ones greater than U+10FFFF are forbidden.
2. Non-BMP characters are encoded as surrogate pairs in UTF-16.
3. Overlong encodings in UTF-8 are considered invalid.

The converter has two code paths: the **fast path** and the **slow path**.
The fast path consists of heavily templated C++ code. It allows the user to tune the converter to his particular inputs. Using non-default template arguments allows user to make the fast path even faster, but at the cost of reducing its capabilities (either in supporting long code points or in validation/checking).
The fast path works by default, but if it meets some data it cannot handle, it temporarily switches to the slow path. The slow path can handle anything, but it is slower. Moreover, the switches between the code paths cost time. A rule of thumb is to make sure that more than 99% of characters can be handled by fast path for good performance.

The first template argument (*MaxBytes*) defines the range of code points supported by fast path:

* *MaxBytes = 3*:  support [U+0000 .. U+FFFF] --- encoded as at most 3 bytes in UTF-8  (Basic Multilingual Plane).
* *MaxBytes = 2*:  support [U+0000 .. U+07FF] --- encoded as at most 2 bytes in UTF-8.
* *MaxBytes = 1*:  support [U+0000 .. U+007F] --- encoded as 1 byte in UTF-8.

The second template argument (*Mode*) controls the slow path and validation:

* *Mode = cmValidate*: full processing and validation --- invalid input causes converter to stop and return error, so arbitrary sequence of bytes can be given as input.
* *Mode = cmFull*: validation is removed --- invalid input causes undefined behavior, but any valid input is processed correctly.
* *Mode = cmFast*: slow path and validation are removed --- invalid input causes undefined behavior, and a character unsupported by fast path also causes undefined behavior.

By default, the toughest and the most versatile values are used: *MaxBytes = 3*, *Mode = cmValidate*.
Note that the fast path is capable of doing complete validation of input stream, provided that you don't disable validation. Also, it is capable of handling the whole BMP, but non-BMP characters can only be processed by slow path.
Simplest error correction can be optionally enabled in validating converters.

## Build

The utf8lut library and its applications are built using "keep it simple" principle =)
All you need to do is:

1. add the `src/` directory to C++ include path
2. add a bunch of cpp files to the build process
3. (non-MSVC): add `-mssse3 -std=c++11` to compiler parameters

CMake file is provided for testing and sample applications. You can see there which cpp files need to be compiled and linked (point 2).
Additionally, there are simple scripts (in `scripts/`) which can build the sample applications: batch scripts for MSVC and MinGW, and shell scripts for GCC. They are easier to use and read than CMake.

## Demo

There is a demo application called *FileConverter*, which allows to test and benchmark the library on various inputs and with various settings.
You can build it for instance using `scripts/build_msvc_file_conv.cmd`.

Here are some sample commands to measure performance by repeatedly converting a single array (about 1 MB in sizes) from RAM to RAM:

    FileConverter_msvc.exe [rnd1110:1000000] [hash] -k=1000
    FileConverter_msvc.exe [rnd1110:1000000] [hash] -k=1000 -b=0
    FileConverter_msvc.exe war_and_piece.fb2 [hash] -k=1000
    FileConverter_msvc.exe war_and_piece.fb2 [hash] -k=1000 -b=0

The results on Ryzen 1600 (single core) are provided below.
The numbers show how much CPU cycles are spent per one byte in UTF-8 encoding. The first number in a cell shows the exactly time of the core conversion (measured by rdtsc). The second number includes all the overhead: it is estimated from total run time, total bytes converted, and CPU frequency (3.4 GHz).
The **rnd** dataset was generated randomly, each code point had 33% probability of being 1-byte long, 2-byte long or 3-byte long. The **fb2** dataset is a famous russian book "War and Peace" in fb2 format. **decode** is conversion in UTF-8 -> UTF16 direction, while **encode** is in UTF-16 -> UTF-8 direction.

| implementation            | rnd decode   | rnd encode  | fb2 decode  | fb2 encode  |
| ------------------------- | ------------ | ----------- | ----------- | ----------- |
| max fast path (-b=3 -m=2) | 1.65 / 2.02  | 0.74 / 0.95 | 1.00 / 1.21 | 0.68 / 0.83 |
| scalar slow path (-b=0)   | 15.3 / 16.4  | 9.9 / 10.7  | 9.10 / 9.67 | 4.89 / 5.97 |

Note that utf8lut uses quite large lookup table, which barely fits CPU cache. As a result, the performance of conversion heavily depends on the particular distribution of code point lengths in the input data, with uniform random distribution being the worst case. 

More detailed performance measurements can be found in the [related blog article](https://dirtyhandscoding.github.io/posts/utf8lut-vectorized-utf-8-converter-test-results.html#performance-evaluation).

## Usage

The simplest complete usage example is present in `src/tests/Example.cpp`. Here are the most important lines from it:

    std::unique_ptr<BaseBufferProcessor> processor(ProcessorSelector<dfUtf8, dfUtf16>::WithOptions<>::Create());
    ConversionResult res = ConvertFile(*processor, "input.utf8", "output.utf16");

In the first step, a processor must be created using ProcessorSelector. Source and destination encodings and additional options (default values are used in the example) are passed as template arguments. It is recommended to read `src/buffer/ProcessorSelector.h` for more details. All processors have common base class *BaseBufferProcessor*, so C++ templates are only involved in processor creation, and polymorphism via virtual functions is used afterwards.
In the second step, you can call some message-level conversion routine. In the example, a function for complete file-to-file conversion is called. There is also a routine for memory-to-memory conversion. See `src/message/MessageConverter.h` for more details.

While most conversions can be done using message-level routines, it is also possible to go to lower level. Buffer-level routines convert data in chunks, and you can load/store data yourself in any way you want. See comments in `src/buffer/BaseBufferProcessor.h` and `src/buffer/ProcessorPlugins.h` for details. Implementation in `src/message/MessageConverter.cpp` may serve as a good example of using buffer-level routines.

Finally, it is possible to drop all the high level code and use the core-level routines directly. They are located in `src/core/` and they contain all the SSSE3 code from the fast path. No comments are available in core level, and shooting off your leg is very easy. If you are interested in understanding the main idea, this is definitely the code to look into.

## iconv interface

It is also possible to use buffer-level routines via standard *iconv* interface.
Unfortunately, the implementation does **not** exactly follow iconv specification. Please read `src/iconv/iconv.h` file, it describes the deviations from the standard.
Scripts are provided, which build a dll/so with iconv functions exported.

## Tests

Since the vectorized algorithm processes many bytes at once, ordinary tests are rather useless for it. [Fuzz testing](https://en.wikipedia.org/wiki/Fuzzing) is used to ensure that the code behaves properly on wide range of inputs.
In order to run fuzz testing, compile *CorrectnessTests* application and run it. Leave it running for at least a few minutes. If it crashes or stops writing text to console, then an error is found.
