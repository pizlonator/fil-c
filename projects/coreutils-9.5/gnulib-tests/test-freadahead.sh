#!/bin/sh

${CHECKER} ./test-freadahead${EXEEXT} 0 < "$srcdir/test-freadahead.sh" || exit 1
${CHECKER} ./test-freadahead${EXEEXT} 5 < "$srcdir/test-freadahead.sh" || exit 1
exit 0
