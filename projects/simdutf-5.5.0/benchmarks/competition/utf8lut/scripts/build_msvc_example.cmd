set ROOT=../src/
cl ^
    %ROOT%base/Timing.cpp ^
    %ROOT%core/DecoderLut.cpp ^
    %ROOT%core/EncoderLut.cpp ^
    %ROOT%buffer/BaseBufferProcessor.cpp ^
    %ROOT%message/MessageConverter.cpp ^
    %ROOT%tests/Example.cpp ^
    /I"../src" /Fe"Example_msvc.exe" ^
    /D _CRT_SECURE_NO_DEPRECATE ^
    /D NDEBUG /D TIMING ^
    /O2 /Oi /W2 /EHsc /FAs /Zi /MD /link/opt:ref
