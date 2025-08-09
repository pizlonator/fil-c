ROOT=../src/
g++ \
    $ROOT"base/Timing.cpp" \
    $ROOT"core/DecoderLut.cpp" \
    $ROOT"core/EncoderLut.cpp" \
    $ROOT"buffer/BaseBufferProcessor.cpp" \
    $ROOT"message/MessageConverter.cpp" \
    $ROOT"tests/Example.cpp" \
    -I"../src" -o"Example_gcc" \
    -D NDEBUG -D TIMING \
    -std=c++11 -mssse3 -O3

strip Example_gcc
