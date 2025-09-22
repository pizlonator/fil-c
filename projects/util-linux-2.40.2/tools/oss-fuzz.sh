#!/usr/bin/env bash

set -ex

export LC_CTYPE=C.UTF-8

export CC=${CC:-clang}
export CXX=${CXX:-clang++}
export LIB_FUZZING_ENGINE=${LIB_FUZZING_ENGINE:--fsanitize=fuzzer}

SANITIZER=${SANITIZER:-address -fsanitize-address-use-after-scope}
flags="-O1 -fno-omit-frame-pointer -gline-tables-only -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION -fsanitize=$SANITIZER -fsanitize=fuzzer-no-link"

export CFLAGS=${CFLAGS:-$flags}
export CXXFLAGS=${CXXFLAGS:-$flags}

export OUT=${OUT:-$(pwd)/out}
mkdir -p $OUT

if [[ "$SANITIZER" == undefined ]]; then
    additional_ubsan_checks=alignment
    UBSAN_FLAGS="-fsanitize=$additional_ubsan_checks -fno-sanitize-recover=$additional_ubsan_checks"
    CFLAGS+=" $UBSAN_FLAGS"
    CXXFLAGS+=" $UBSAN_FLAGS"
fi

CONFIGURE_ARGS="--disable-all-programs --enable-libuuid --enable-libfdisk --enable-last --enable-fuzzing-engine --enable-libmount --enable-libblkid"

LIBC_VERSION="$(dpkg -s libc6 | grep Version | cut -d' ' -f2)"

# Ubuntu focal uses glibc 2.31 but 2.34 is necessary
if dpkg --compare-versions "$LIBC_VERSION" 'lt' '2.34'; then
	CONFIGURE_ARGS="$CONFIGURE_ARGS --disable-year2038"
fi

./autogen.sh
./configure $CONFIGURE_ARGS
make -j$(nproc) V=1 check-programs

for d in "$(dirname $0)"/../tests/ts/fuzzers/test_*_fuzz_files; do
    bd=$(basename "$d")
    fuzzer=${bd%_files}
    zip -jqr $OUT/${fuzzer}_seed_corpus.zip "$d"
done

# create seed corpus for blkid fuzzing
unxz -k "$(dirname $0)"/../tests/ts/blkid/images-*/*.xz
zip -jqrm $OUT/test_blkid_fuzz_seed_corpus.zip "$(dirname $0)"/../tests/ts/blkid/images-*/*.img

find . -maxdepth 1 -type f -executable -name "test_*_fuzz" -exec mv {} $OUT \;
find . -type f -name "fuzz-*.dict" -exec cp {} $OUT \;
