#!/bin/bash

set -x

export LSAN_OPTIONS=verbosity=1:log_threads=1

builddir="_build"
srcdir=`pwd`

meson --prefix /usr "$@" $builddir $srcdir || exit $?

ninja -C $builddir || exit $?
meson test -C $builddir || {
        res=$?
        cat $builddir/meson-logs/testlog.txt
        exit $res
}
