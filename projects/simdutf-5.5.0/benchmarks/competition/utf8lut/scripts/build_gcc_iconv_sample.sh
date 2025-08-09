ROOT=../src/
gcc \
    $ROOT"tests/iconv_sample.c" \
    iconv_u8l_gcc.so \
    -I"../src" -o"iconv_sample_gcc" \
    -O3

cp ../data/test_minimal.txt data_utf8.txt
