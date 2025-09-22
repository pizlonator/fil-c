#!/bin/sh

# Test with all of stdin, stdout, stderr open.
${CHECKER} ./test-xstdopen${EXEEXT} || exit 1

# The syntax for closed file descriptors in sh scripts is specified by POSIX in
# section 2.7.5 of
# https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html

# Test with stdin closed.
${CHECKER} ./test-xstdopen${EXEEXT} <&- || exit 1

# Test with stdout closed.
${CHECKER} ./test-xstdopen${EXEEXT} >&- || exit 1

# Test with stderr closed.
${CHECKER} ./test-xstdopen${EXEEXT} 2>&- || exit 1

# Test with all of stdin, stdout, stderr closed.
${CHECKER} ./test-xstdopen${EXEEXT} <&- >&- 2>&- || exit 1
