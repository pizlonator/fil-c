# this library provides main() for compilers that do not have libfuzzer (everyone except clang)
# so the source code is compiled regardless of compiler, which prevents code rot.
# also, the library provides a way to invoke the fuzzer on external data which is
# useful to run the fuzz corpus through code also when not using clang.
add_library(main STATIC main.cpp)
target_compile_features(main PRIVATE cxx_std_20)

# see if libfuzzer is supported - this gets more complicated than just checking
# for clang, because apple clang does not seem to support it and windows also
# support clang nowadays (and perhaps libfuzzer)

include(CheckSourceCompiles)
set(CMAKE_REQUIRED_LINK_OPTIONS -fsanitize=fuzzer)
check_source_compiles(CXX "
    #include <cstddef>
    #include <cstdint>
    extern \"C\" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
    { return 0;}
    " haslibfuzzer)


macro(create_fuzztest testname)
    add_executable(${testname} ${testname}.cpp)
    target_compile_features(${testname} PRIVATE cxx_std_20)
    target_link_libraries(${testname} PRIVATE simdutf)

    if(haslibfuzzer)
        target_compile_options(${testname} PUBLIC -fsanitize=fuzzer)
        target_link_options(${testname} PUBLIC -fsanitize=fuzzer)
    else()
        # supply a separate main
        target_link_libraries(${testname} PRIVATE main)
    endif()

endmacro()

create_fuzztest(conversion)
create_fuzztest(misc)
create_fuzztest(roundtrip)
create_fuzztest(base64)
