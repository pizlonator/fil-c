set ROOT=../src/
cl ^
    %ROOT%core/DecoderLut.cpp ^
    %ROOT%core/EncoderLut.cpp ^
    %ROOT%buffer/BaseBufferProcessor.cpp ^
    %ROOT%buffer/AllProcessors.cpp ^
    %ROOT%message/MessageConverter.cpp ^
    %ROOT%tests/CorrectnessTests.cpp ^
    /I"../src" /Fe"CorrectnessTests_msvc.exe" ^
    /D _CRT_SECURE_NO_DEPRECATE ^
    /O2 /Oi /W2 /EHsc /FAs /Zi /MD /link/opt:ref
