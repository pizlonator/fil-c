#!/bin/bash
# Test for iconv (the program) buffer management.
# Copyright (C) 2024-2026 Free Software Foundation, Inc.
# This file is part of the GNU C Library.

# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.

# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, see
# <https://www.gnu.org/licenses/>.

# Arguments:
#   root of the build tree ($(objpfx-common))
#   test command wrapper (for running on the board/with new ld.so)
#   extra flags to pass to iconv
#   number of times to double the input files in size (default: 0)

exec 2>&1
set -e

exec {logfd}>&1

codir=$1
test_program_prefix="$2"

# Use internal converters to avoid issues with module loading.
iconv_args="-f ASCII -t UTF-8 $3"

file_size_doublings=${4-0}

failure=false

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' 0
echo ABC > "$tmp/abc"
echo DEF > "$tmp/def"
echo GGG > "$tmp/ggg"
echo HH > "$tmp/hh"
echo XY > "$tmp/xy"
echo ZT > "$tmp/zt"
echo OUT > "$tmp/out-template"
: > "$tmp/empty"
printf '\xff' > "$tmp/0xff"

# Length should be a prime number, to help with buffer alignment testing.
printf '\xc3\xa4\xe2\x80\x94\xe2\x80\x94\xc3\xa4\n' > "$tmp/utf8-sequence"

# Double all files to produce larger buffers.
for p in "$tmp"/* ; do
    i=0
    while test $i -lt $file_size_doublings; do
	cat "$p" "$p" > "$tmp/scratch"
	mv "$tmp/scratch" "$p"
	i=$(($i + 1))
    done
done

cat "$tmp/xy" "$tmp/0xff" "$tmp/zt" > "$tmp/0xff-wrapped"

run_iconv () {
    local c=0
    if test "${FUNCNAME[2]}" = main; then
       c=1
    fi
    echo "${BASH_SOURCE[$c]}:${BASH_LINENO[$c]}: iconv $iconv_args $@" >&$logfd
    $test_program_prefix $codir/iconv/iconv_prog $iconv_args "$@"
}

check_out_expected () {
    if test -x "$tmp/out" ; then
	echo "error: iconv output file is executable"
	failure=true
    fi
    if ! cmp -s "$tmp/out" "$tmp/expected" ; then
        echo "error: iconv output difference" >&$logfd
        echo "*** expected ***" >&$logfd
        cat "$tmp/expected" >&$logfd
        echo "*** actual ***" >&$logfd
        cat "$tmp/out" >&$logfd
        failure=true
    fi
}

expect_files () {
    local f
    ! test -z "$1"
    cp "$tmp/$1" "$tmp/expected"
    shift
    for f in "$@" ; do
        cat "$tmp/$f" >> "$tmp/expected"
    done
    check_out_expected
}

check_out () {
    cat > "$tmp/expected"
    check_out_expected
}

expect_exit () {
    local expected=$1
    shift
    # Prevent failure for stopping the script.
    if "$@" ; then
        actual=$?
    else
        actual=$?
    fi
    if test "$actual" -ne "$expected"; then
        echo "error: expected exit status $expected, not $actual" >&$logfd
        exit 1
    fi
}

ignore_failure () {
    set +e
    "$@"
    status=$?
    set -e
}

# Concatenation test.
run_iconv -o "$tmp/out" "$tmp/abc" "$tmp/def"
expect_files abc def

# Single-file in-place conversion.
run_iconv -o "$tmp/out" "$tmp/out"
expect_files abc def

# Multiple input files with in-place conversion.

run_iconv -o "$tmp/out" "$tmp/out" "$tmp/abc"
expect_files abc def abc

run_iconv -o "$tmp/out" "$tmp/ggg" "$tmp/out"
expect_files ggg abc def abc

run_iconv -o "$tmp/out" "$tmp/hh" "$tmp/out" "$tmp/hh"
expect_files hh ggg abc def abc hh

cp "$tmp/out-template" "$tmp/out"
run_iconv -o "$tmp/out" "$tmp/ggg" "$tmp/out" "$tmp/out" "$tmp/ggg"
expect_files ggg out-template out-template ggg

cp "$tmp/out-template" "$tmp/out"
run_iconv -o "$tmp/out" "$tmp/ggg" "$tmp/out" "$tmp/hh" "$tmp/out" "$tmp/ggg"
expect_files ggg out-template hh out-template ggg

# Empty output should truncate the output file if exists.

cp "$tmp/out-template" "$tmp/out"
run_iconv -o "$tmp/out" </dev/null
expect_files empty

cp "$tmp/out-template" "$tmp/out"
run_iconv -o "$tmp/out" - </dev/null
expect_files empty

cp "$tmp/out-template" "$tmp/out"
run_iconv -o "$tmp/out" /dev/null
expect_files empty

cp "$tmp/out-template" "$tmp/out"
expect_exit 1 run_iconv -c -o "$tmp/out" "$tmp/0xff"
expect_files empty

# But not if we are writing to standard output.

cp "$tmp/out-template" "$tmp/out"
run_iconv </dev/null >>"$tmp/out"
expect_files out-template

cp "$tmp/out-template" "$tmp/out"
run_iconv - </dev/null >>"$tmp/out"
expect_files out-template

cp "$tmp/out-template" "$tmp/out"
run_iconv /dev/null >>"$tmp/out"
expect_files out-template

# Conversion errors should avoid clobbering an existing file if
# it is also an input file.

cp "$tmp/0xff" "$tmp/out"
expect_exit 1 run_iconv -o "$tmp/out" "$tmp/out"
expect_files 0xff

cp "$tmp/0xff" "$tmp/out"
expect_exit 1 run_iconv -o "$tmp/out" < "$tmp/out"
expect_files 0xff

cp "$tmp/0xff" "$tmp/out"
expect_exit 1 run_iconv -o "$tmp/out" - < "$tmp/out"
expect_files 0xff

cp "$tmp/0xff-wrapped" "$tmp/out"
expect_exit 1 run_iconv -o "$tmp/out" "$tmp/out"
expect_files 0xff-wrapped

cp "$tmp/0xff-wrapped" "$tmp/out"
expect_exit 1 run_iconv -o "$tmp/out" < "$tmp/out"
expect_files 0xff-wrapped

cp "$tmp/0xff-wrapped" "$tmp/out"
expect_exit 1 run_iconv -o "$tmp/out" - < "$tmp/out"
expect_files 0xff-wrapped

cp "$tmp/0xff-wrapped" "$tmp/out"
expect_exit 1 run_iconv -o "$tmp/out" "$tmp/abc" "$tmp/out"
expect_files 0xff-wrapped

cp "$tmp/0xff-wrapped" "$tmp/out"
expect_exit 1 run_iconv -o "$tmp/out" "$tmp/abc" - < "$tmp/out"
expect_files 0xff-wrapped

# If errors are ignored, the file should be overwritten.

cp "$tmp/0xff-wrapped" "$tmp/out"
expect_exit 1 run_iconv -c -o "$tmp/out" "$tmp/out"
expect_files xy zt

cp "$tmp/0xff" "$tmp/out"
expect_exit 1 run_iconv -c -o "$tmp/out" "$tmp/abc" "$tmp/out" "$tmp/def"
expect_files abc def

cp "$tmp/out-template" "$tmp/out"
expect_exit 1 \
    run_iconv -c -o "$tmp/out" "$tmp/abc" "$tmp/0xff" "$tmp/def" 2>"$tmp/err"
! test -s "$tmp/err"
expect_files abc def

cp "$tmp/out-template" "$tmp/out"
expect_exit 1 run_iconv -c -o "$tmp/out" \
    "$tmp/abc" "$tmp/0xff-wrapped" "$tmp/def" 2>"$tmp/err"
! test -s "$tmp/err"
expect_files abc xy zt def

cp "$tmp/0xff-wrapped" "$tmp/out"
expect_exit 1 run_iconv -c -o "$tmp/out" "$tmp/out" "$tmp/abc" "$tmp/out" "$tmp/def"
expect_files xy zt abc xy zt def

cp "$tmp/0xff-wrapped" "$tmp/out"
expect_exit 1 run_iconv -o "$tmp/out" \
    "$tmp/out" "$tmp/abc" "$tmp/out" "$tmp/def"
expect_files 0xff-wrapped

cp "$tmp/0xff-wrapped" "$tmp/out"
expect_exit 1 run_iconv -c -o "$tmp/out" \
    "$tmp/abc" "$tmp/out" "$tmp/def" "$tmp/out"
expect_files abc xy zt def xy zt

# If the file does not exist yet, it should not be created on error.

rm "$tmp/out"
expect_exit 1 run_iconv -o "$tmp/out" "$tmp/0xff"
! test -e "$tmp/out"

expect_exit 1 run_iconv -o "$tmp/out" < "$tmp/0xff"
! test -e "$tmp/out"

expect_exit 1 run_iconv -o "$tmp/out" "$tmp/abc" "$tmp/0xff" "$tmp/def"
! test -e "$tmp/out"

expect_exit 1 run_iconv -o "$tmp/out" "$tmp/abc" - < "$tmp/0xff" "$tmp/def"
! test -e "$tmp/out"

# Listing standard input multiple times should not fail (bug 32050).

run_iconv -o "$tmp/out" "$tmp/xy" - - "$tmp/zt" < "$tmp/abc"
expect_files xy abc zt

# NB: Extra iconv args are ignored after this point.  Actual
# multi-byte conversion does not work with tiny buffers.
iconv_args="-f UTF-8 -t ASCII"

printf 'x\n\xc3' > "$tmp/incomplete"
expect_exit 1 run_iconv -o "$tmp/out" "$tmp/incomplete"
check_out <<EOF
x
EOF

# Test buffering behavior if the buffer ends with an incomplete
# multi-byte sequence.
prefix=""
prefix_length=0
while test $prefix_length -lt 12; do
    echo "info: testing prefix length $prefix_length" 2>&$logfd
    printf "%s" "$prefix" > "$tmp/prefix"
    cat "$tmp/prefix" "$tmp/utf8-sequence" > "$tmp/tmp"
    iconv_args="-f UTF-8 -t UCS-4"
    run_iconv -o "$tmp/out1" "$tmp/tmp"
    iconv_args="-f UCS-4 -t UTF-8"
    run_iconv -o "$tmp/out" "$tmp/out1"
    expect_files prefix utf8-sequence

    prefix="$prefix@"
    prefix_length=$(($prefix_length + 1))
done

if $failure ; then
    exit 1
fi
