#!/bin/sh
. "${srcdir=.}/init.sh"; path_prepend_ .

fail=0

${CHECKER} test-readtokens || fail=1

# Simplest case.
echo a:b:c: > exp || fail=1
printf a:b:c | ${CHECKER} test-readtokens : > tmp 2>&1 || fail=1
LC_ALL=C tr -d '\r' < tmp > out || fail=1
compare exp out || fail=1

# Use NUL as the delimiter.
echo a:b:c: > exp || fail=1
printf 'a\0b\0c' | ${CHECKER} test-readtokens '\0' > tmp 2>&1 || fail=1
LC_ALL=C tr -d '\r' < tmp > out || fail=1
compare exp out || fail=1

# Two delimiter bytes, and adjacent delimiters in the input.
echo a:b:c: > exp || fail=1
printf a:-:b-:c:: | ${CHECKER} test-readtokens :- > tmp 2>&1 || fail=1
LC_ALL=C tr -d '\r' < tmp > out || fail=1
compare exp out || fail=1

Exit $fail
