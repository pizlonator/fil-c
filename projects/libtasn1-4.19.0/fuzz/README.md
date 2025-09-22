# Fuzzers

These are fuzzers designed for use with `libFuzzer` or `afl`. They can
be used to run on Google's OSS-Fuzz (https://github.com/google/oss-fuzz/).

The convention used here is that the initial values for each parser fuzzer
are taken from the $NAME.in/ and $NAME.repro/ directories.

Crash reproducers from OSS-Fuzz are put into $NAME.repro directory for
regression testing with top dir 'make check'.


# Running a fuzzer locally using clang

Use the following commands on top dir (best with clang-9 or higher):
```
export CC=clang-9
export CFLAGS="-O1 -g -fno-omit-frame-pointer -gline-tables-only -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION -fsanitize=undefined,integer,nullability -fsanitize=address -fsanitize-address-use-after-scope -fsanitize=fuzzer-no-link"
export CFLAGS="$CFLAGS -fsanitize=bool,array-bounds,float-divide-by-zero,function,integer-divide-by-zero,return,shift,signed-integer-overflow,unsigned-integer-overflow,vla-bound,vptr -fno-sanitize-recover=bool,array-bounds,float-divide-by-zero,function,integer-divide-by-zero,return,shift,signed-integer-overflow,vla-bound,vptr"
./configure --enable-fuzzing --disable-doc --disable-gcc-warnings
make clean
make -j$(nproc)
cd fuzz

# run libtasn1_array2tree_fuzzer
UBSAN_OPTIONS=print_stacktrace=1 ASAN_SYMBOLIZER_PATH=/usr/lib/llvm-9/bin/llvm-symbolizer \
  ./run-clang.sh libtasn1_array2tree_fuzzer
```

If you see a crash, then a crash corpora is written that can be used for further
investigation. E.g.
```
==2410==ERROR: AddressSanitizer: heap-use-after-free on address 0x602000004e90 at pc 0x00000049cf9c bp 0x7fffb5543f70 sp 0x7fffb5543720
...
Test unit written to ./crash-adc83b19e793491b1c6ea0fd8b46cd9f32e592fc
```

To reproduce the crash:
```
./libtasn1_array2tree_fuzzer < ./crash-adc83b19e793491b1c6ea0fd8b46cd9f32e592fc
```

You can also copy/move that file into libtasn1_array2tree_fuzzer.repro/
and re-build the project without fuzzing for a valgrind run, if you prefer that.
Just a `./configure --enable-valgrind-tests` and a `make check` should reproduce it.


# Running a fuzzer using AFL

Use the following commands on top dir:

```
$ CC=afl-clang-fast ./configure --disable-doc
$ make -j$(nproc) clean all
$ cd fuzz
$ ./run-afl.sh libtasn1_array2tree_fuzzer
```

# Fuzz code coverage using the corpus directories *.in/

Code coverage reports currently work best with gcc+lcov+genhtml.

In the top directory:
```
CC=gcc CFLAGS="-O0 -g" ./configure --disable-doc --disable-manywarnings
make fuzz-coverage
xdg-open lcov/index.html
```

Each fuzzer target has it's own files/functions to cover, e.g.
`libtasn1_array2tree_fuzzer` covers array2tree.

To work on corpora for better coverage, `cd fuzz` and use e.g.
`./view-coverage.sh libtasn1_array2tree_fuzzer`.
