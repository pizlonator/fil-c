set ROOT=../src/
clang ^
    %ROOT%core/DecoderLut.cpp ^
    %ROOT%core/EncoderLut.cpp ^
    %ROOT%buffer/BaseBufferProcessor.cpp ^
    %ROOT%buffer/AllProcessors.cpp ^
    %ROOT%message/MessageConverter.cpp ^
    %ROOT%tests/CorrectnessTests.cpp ^
    -I"../src" -o"CorrectnessTests_clang.exe" ^
    -D _CRT_SECURE_NO_DEPRECATE ^
    -std=c++14 -mssse3 -O3
