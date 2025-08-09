ROOT=../src/
g++ \
    $ROOT"core/DecoderLut.cpp" \
    $ROOT"core/EncoderLut.cpp" \
    $ROOT"buffer/BaseBufferProcessor.cpp" \
    $ROOT"iconv/iconv.cpp" \
    -I"../src" -fPIC -shared -o"iconv_u8l_gcc.so" \
    -D NDEBUG -D ICONV_UTF8LUT_BUILD \
    -std=c++11 -mssse3 -O3

strip iconv_u8l_gcc.so
