# Fuzzers

These are fuzzers designed for use with `libFuzzer` or `afl`. They can
be used to run on Google's OSS-Fuzz (https://github.com/google/oss-fuzz/).

The convention used here is that the initial values for each parser fuzzer
are taken from the $NAME.in directory ($NAME is the name of the fuzzer, e.g.
'gnutls_x509_parser_fuzzer').

Crash reproducers from OSS-Fuzz are put into $NAME.repro directory for
regression testing with top dir 'make check'.

The script `get_ossfuzz_corpora` downloads the corpora from OSS-Fuzz for
the given fuzzer. It puts those files together with the local ones and performs
a 'merge' step to remove superfluous corpora. The next step would be to add
changed/new corpora to the git repository.

Example:
```
./get_ossfuzz_corpora gnutls_x509_parser_fuzzer
git add gnutls_x509_parser_fuzzer.in/*
git commit -a -m "Update OSS-Fuzz corpora"
(create a branch and push if something changed)
(create a MR)
```

Since there are quite a few fuzzers now, you can update all their corpora
in one step with `./get_all_corpora`. Do this from time to time to stay
in sync with OSS-Fuzz. Whenever library code or fuzzers change, there might
me new corpora after 1-2 days.


# Running a fuzzer using clang and libFuzzer

Use the following commands on top dir:
```
export CC=clang
export CXX=clang++
export CFLAGS="-O1 -g -fno-omit-frame-pointer -gline-tables-only -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION -fsanitize=undefined,integer,nullability,bool,alignment,null,enum,address,leak,nonnull-attribute -fno-sanitize-recover=all -fsanitize-recover=unsigned-integer-overflow -fsanitize-address-use-after-scope -fsanitize=fuzzer-no-link"
export CXXFLAGS="$CFLAGS"
./configure --enable-fuzzer-target --enable-static --disable-doc --disable-gcc-warnings --disable-hardware-acceleration
make clean
make
cd fuzz

# build and run gnutls_base64_decode_fuzzer
export UBSAN_OPTIONS=print_stacktrace=1:report_error_type=1
export ASAN_SYMBOLIZER_PATH=/usr/bin/llvm-symbolizer
./run-clang.sh gnutls_base64_decoder_fuzzer
```


# Running a fuzzer using AFL and clang

Use the following commands on top dir:

```
$ CC=afl-clang-fast ./configure --disable-doc --enable-fuzzer-target
$ make -j$(nproc) clean all
$ cd fuzz
$ ./run-afl.sh gnutls_base64_decoder_fuzzer
```

# Running a fuzzer using AFL and gcc

Use the following commands on top dir:

```
$ CC=afl-gcc ./configure --disable-doc --enable-fuzzer-target
$ make -j$(nproc) clean all
$ cd fuzz
$ ./run-afl.sh gnutls_base64_decoder_fuzzer
```

# Fuzz code coverage using the corpus directories *.in/

Code coverage reports currently work best with gcc+lcov+genhtml.

In the top directory:
```
./configure --enable-code-coverage --disable-doc
make clean
make
make -C fuzz check
make code-coverage-capture
xdg-open <URI given by previous command>
# repeat the last three steps after changing *fuzzer.c
```

Each fuzzer target has its own files/functions to cover, e.g.
`gnutls_base64_decode_fuzzer` covers lib/x509_b64.c.

To work on corpora for better coverage, `cd fuzz` and use e.g.
`./view-coverage.sh gnutls_base64_decode_fuzzer`.


# Enhancing the testsuite for issues found

Whenever you fix an issue from OSS-Fuzz (or an issue found via local fuzzing)
please download the corpus (data file) that triggers that issue and put it
into $NAME.repro. 'git add' the file and create a commit, to avoid future
regressions.
