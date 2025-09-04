#!/bin/sh

tmpfiles=""
trap 'rm -fr $tmpfiles' HUP INT QUIT TERM

tmpfiles="$tmpfiles t-vfprintf-posix.tmp t-vfprintf-posix.out"
${CHECKER} ./test-vfprintf-posix${EXEEXT} > t-vfprintf-posix.tmp || exit 1
LC_ALL=C tr -d '\r' < t-vfprintf-posix.tmp > t-vfprintf-posix.out || exit 1

: "${DIFF=diff}"
${DIFF} "${srcdir}/test-printf-posix.output" t-vfprintf-posix.out
result=$?

rm -fr $tmpfiles

exit $result
