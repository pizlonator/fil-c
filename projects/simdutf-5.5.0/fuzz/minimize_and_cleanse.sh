#!/bin/sh
#
# minimizes and cleanses a crash.
# arg1 is the fuzzer
# arg2 is the crash case

usage() {
    echo "$0 fuzzer crashcase"
}

if [ $# -ne 2 ] ; then
    usage
    exit 1
fi

if [ ! -x "$1" ] ; then
    echo "fuzzer should be passes ad arg 1"
    exit 1
fi

if [ ! -e "$2" ] ; then
    echo "crash case should be passed as arg 2"
    exit 1
fi

TIMEOUT=30

REPRODUCER=reproducer.$(basename $1).cpp

rm -f minimized_crash cleaned_crash $REPRODUCER

"$1" "$2" -minimize_crash=1 -exact_artifact_path=minimized_crash -max_total_time=$TIMEOUT

"$1" minimized_crash -max_total_time=$TIMEOUT -cleanse_crash=1 -exact_artifact_path=cleaned_crash

if [ ! -e cleaned_crash ] ; then
    # in case it was not possible to clean, just copy it over
    cp minimized_crash cleaned_crash
fi

if ! which xxd >/dev/null ; then
   echo "please install xxd (sudo apt install xxd)"
   exit 1
fi

# create a reproducer
echo '#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int main() {' >$REPRODUCER
xxd --include cleaned_crash >>$REPRODUCER
echo '
LLVMFuzzerTestOneInput(cleaned_crash,cleaned_crash_len);
}' >> $REPRODUCER

CF=clang-format-18
if which $CF >/dev/null; then
    $CF -fallback-style=Mozilla -i $REPRODUCER
fi

FUZZER=$(basename $1)
mv cleaned_crash cleaned_crash.$FUZZER
rm minimized_crash

echo "find the crash data in file cleaned_crash.$FUZZER"
echo "find a reproducer source file in $REPRODUCER"

CXX=/usr/lib/ccache/clang++-18
TARGET=out/$(basename $REPRODUCER .cpp)
if [ -x $CXX ]; then
    echo compiling a reproducer binary in $TARGET
    set -x
else
    echo "compile a reproducer with the following command:"
    CXX="echo clang++"
fi
$CXX \
    -o $TARGET \
    $REPRODUCER \
    $FUZZER.cpp \
    work/lib/libsimdutf.a \
    -I work/include \
    -std=c++20 \
    -g \
    -fsanitize=address,undefined
