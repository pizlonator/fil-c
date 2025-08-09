set ROOT=../src/
g++ ^
    %ROOT%base/Timing.cpp ^
    %ROOT%core/DecoderLut.cpp ^
    %ROOT%core/EncoderLut.cpp ^
    %ROOT%buffer/BaseBufferProcessor.cpp ^
    %ROOT%buffer/AllProcessors.cpp ^
    %ROOT%message/MessageConverter.cpp ^
    %ROOT%tests/FileConverter.cpp ^
    -I"../src" -o"FileConverter_mingw.exe" ^
    -D NDEBUG -D TIMING ^
    -std=c++11 -mssse3 -O3

strip FileConverter_mingw.exe
