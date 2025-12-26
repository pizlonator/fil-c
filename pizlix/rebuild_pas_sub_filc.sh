#!/bin/bash

set -e
set -x

test "x$FILCSRC" != "x"
test -d $FILCSRC
test -d $FILCSRC/libpas

test $EUID -eq `stat -c %u $FILCSRC`

cd $FILCSRC

./build_runtime.sh
