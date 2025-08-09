#!/bin/bash
#
# this is the entry point for oss-fuzz.
#
# it can be invoked locally for development purposes as well
#

set -e

fuzzer_src_files=

if [ -z $SRC ] ; then
    echo "development mode"
    set -ux

    SCRIPTDIR=$(dirname "$0")
    cd "$SCRIPTDIR/.."

    export CXX=/usr/lib/ccache/clang++-18
    export CXXFLAGS="-fsanitize=fuzzer-no-link,address,undefined -g -O1 -fsanitize-trap=undefined"
    export LIB_FUZZING_ENGINE="-fsanitize=fuzzer"
    export OUT=fuzz/out
    export WORK=fuzz/work
    mkdir -p $OUT $WORK
else
    # invoked from oss fuzz
    cd $SRC/simdutf
    if [ "$ARCHITECTURE" = "aarch64" -a $(clang++ --version |head -n1 |cut -f3 -d' ' |cut -f1 -d.) -le 14 ]; then
	# the compiler and stdlib is clang 14 which does not
	# support ranges well enough to compile the newer fuzzers
	fuzzer_src_files=fuzz/roundtrip.cpp
    fi
fi

if [ -z "$fuzzer_src_files" ] ; then
  fuzzer_src_files=$(ls fuzz/*.cpp|grep -v -E "fuzz/(reproducer.|main)")
fi


cmake -B build -S . \
      -DSIMDUTF_TESTS=Off \
      -DSIMDUTF_TOOLS=Off \
      -DSIMDUTF_FUZZERS=Off \
      -DCMAKE_BUILD_TYPE=Debug \
      -DSIMDUTF_CXX_STANDARD=20 \
      -DSIMDUTF_ALWAYS_INCLUDE_FALLBACK=On

cmake --build build -j$(nproc)
cmake --install build --prefix $WORK

CXXFLAGSEXTRA=-std=c++20

for fuzzersrc in $fuzzer_src_files ; do
    fuzzer=$(basename $fuzzersrc .cpp)

    $CXX $CXXFLAGS $CXXFLAGSEXTRA\
         -I $WORK/include \
         -c $fuzzersrc -o $fuzzer.o

    $CXX $CXXFLAGS $LIB_FUZZING_ENGINE \
         $fuzzer.o \
         $WORK/lib/libsimdutf.a \
         -o $OUT/$fuzzer
done
