#!/bin/bash

set -e
set -x

test $EUID -ne 0

test "x$FILCSRC" != "x"
test -d $FILCSRC
test -d $FILCSRC/libpas

cd $FILCSRC

./build_runtime.sh
