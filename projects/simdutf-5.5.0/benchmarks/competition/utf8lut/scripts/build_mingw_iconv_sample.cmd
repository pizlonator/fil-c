set ROOT=../src/
g++ ^
    %ROOT%tests/iconv_sample.c ^
    iconv_u8l_mingw.dll ^
    -I"../src" -o"iconv_sample_mingw.exe" ^
    -std=c++11 -mssse3 -O3

copy ..\data\test_minimal.txt data_utf8.txt
