# Fuzzing

All non deprecated functions part of the public API are fuzzed.

There have been quite a few bugs uncovered by this,
showing the effectiveness of this type of testing.

The fuzzers are doing differential fuzzing where applicable, meaning the output
of the different implementations are compared against each other. Deviations
are not tolerated, unless in some special cases. For instance, if the result of
a conversion is that the input was invalid, the (possibly partially) converted
output is allowed to differ.

Because simdutf does runtime cpu dispatching, you have to run the fuzzers at
different systems to ensure all code is exercised. For instance, the icelake
code is not run if you have an older x86 cpu.

simdutf is participating in the oss-fuzz project. The upstream instructions for
how to run, debug and develop fuzzers are applicable in addition to the
instructions here.

## Useful links
 - [OSS fuzz build logs](https://oss-fuzz-build-logs.storage.googleapis.com/index.html#simdutf)
 - [OSS fuzz bugs reported](https://bugs.chromium.org/p/oss-fuzz/issues/list?sort=-id&q=proj%3Dsimdutf)
 - [fuzz introspector (requires login)](https://storage.googleapis.com/oss-fuzz-introspector/simdutf/inspector-report/20240816/fuzz_report.html)
 - [libfuzzer documentation](https://llvm.org/docs/LibFuzzer.html)
## Building the fuzzers locally

Ensure you have clang installed.

```shell
cd fuzz
./build.sh
mkdir -p corpus/base64
out/base64 corpus/base64
```

## Developing fuzzers in an IDE
Using an IDE is nice because it gives code completion, debug support etc.
Make sure the SIMDUTF_FUZZ_FUZZERS is set to on. This should work regardless
of platform, regardless if libFuzzer is available. There will however be no
instrumentation, to get that CXXFLAGS have to be set. Easiest is to use the
build.sh script outside of the IDE and have two builds, one in the IDE to work
with and the one created by the build.sh script.

## Run files through a non-fuzz enabled build
This is useful for instance to test that a gcc build has no errors
on fuzz input.

Example using a sanitized build:
```shell
export CXX=/usr/lib/ccache/g++-14
cmake -B /tmp/build-gcc -DSIMDUTF_SANITIZE=On -DSIMDUTF_SANITIZE_UNDEFINED=On -DSIMDUTF_FUZZERS=On -S . -GNinja
cmake --build /tmp/build-gcc
/tmp/build-gcc/fuzz/conversion corpus/conversion
```

Or, running through valgrind:
```shell
export CXX=/usr/lib/ccache/g++-14
cmake -B /tmp/build-gcc-for-valgrind -DSIMDUTF_FUZZERS=On -S . -GNinja
cmake --build /tmp/build-gcc-for-valgrind
/tmp/build-gcc-for-valgrind/fuzz/conversion corpus/conversion
```

## Minimizing a crash
This is easiest shown with an example:
```shell
  ./build.sh
  mkdir -p corpus/conversion
  out/conversion corpus/conversion
  # ...crashes...
  # crash-XXXX is created where XXXX is a hash of the crashing input
  ./minimize_and_cleanse.sh out/conversion crash-*
  # you now find cleaned_crash.conversion in the current directory
  # see if it reproduces (it should)
  out/conversion cleaned_crash.conversion
```

Some of the fuzzers support printing out a reproducer, which makes it easy to
convert the fuzz finding into a unit test.
