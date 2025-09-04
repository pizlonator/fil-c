#!/bin/sh

tmpfiles=""
trap 'rm -fr $tmpfiles' HUP INT QUIT TERM

tmpfiles="$tmpfiles t-xprintf-posix.tmp t-xprintf-posix.out"
${CHECKER} ./test-xprintf-posix${EXEEXT} > t-xprintf-posix.tmp || exit 1
LC_ALL=C tr -d '\r' < t-xprintf-posix.tmp > t-xprintf-posix.out || exit 1

: "${DIFF=diff}"
${DIFF} "${srcdir}/test-printf-posix.output" t-xprintf-posix.out
test $? = 0 || { rm -fr $tmpfiles; exit 1; }

tmpfiles="$tmpfiles t-xfprintf-posix.tmp t-xfprintf-posix.out"
${CHECKER} ./test-xfprintf-posix${EXEEXT} > t-xfprintf-posix.tmp || exit 1
LC_ALL=C tr -d '\r' < t-xfprintf-posix.tmp > t-xfprintf-posix.out || exit 1

: "${DIFF=diff}"
${DIFF} "${srcdir}/test-printf-posix.output" t-xfprintf-posix.out
test $? = 0 || { rm -fr $tmpfiles; exit 1; }

rm -fr $tmpfiles

exit 0
