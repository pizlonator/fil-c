set ROOT=../src/
cl ^
    %ROOT%tests/iconv_sample.c ^
    iconv_u8l_msvc.lib ^
    /I"../src" /Fe"iconv_sample_msvc.exe" ^
    /D _CRT_SECURE_NO_DEPRECATE ^
    /O2 /W2 /EHsc /Zi /MD

copy ..\data\test_minimal.txt data_utf8.txt
