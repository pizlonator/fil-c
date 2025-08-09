ROOT=../src/
clang++ \
    $ROOT"core/DecoderLut.cpp" \
    $ROOT"core/EncoderLut.cpp" \
    $ROOT"buffer/BaseBufferProcessor.cpp" \
    $ROOT"buffer/AllProcessors.cpp" \
    $ROOT"message/MessageConverter.cpp" \
    $ROOT"tests/CorrectnessTests.cpp" \
    -I"../src" -o"CorrectnessTests_clang" \
    -std=c++11 -mssse3 -O3

strip CorrectnessTests_clang

