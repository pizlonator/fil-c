#!/bin/sh

tmpfiles=""
trap 'rm -fr $tmpfiles' HUP INT QUIT TERM

tmpfiles="$tmpfiles t-vprintf-posix.tmp t-vprintf-posix.out"
${CHECKER} ./test-vprintf-posix${EXEEXT} > t-vprintf-posix.tmp || exit 1
LC_ALL=C tr -d '\r' < t-vprintf-posix.tmp > t-vprintf-posix.out || exit 1

: "${DIFF=diff}"
${DIFF} "${srcdir}/test-printf-posix.output" t-vprintf-posix.out
result=$?

rm -fr $tmpfiles

exit $result
