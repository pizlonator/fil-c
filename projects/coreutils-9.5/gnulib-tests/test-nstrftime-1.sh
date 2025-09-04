#!/bin/sh

LC_ALL=C ${CHECKER} ./test-nstrftime${EXEEXT} || exit 1

exit 0
