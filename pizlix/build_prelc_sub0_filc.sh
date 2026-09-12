#!/bin/bash

set -e
set -x

ulimit -c unlimited

test "x$FILCSRC" != "x"
test -d $FILCSRC
test -d $FILCSRC/projects

test $EUID -eq `stat -c %u $FILCSRC`

cd $FILCSRC

rm -vf projects/*/pizlonated-*.tar.gz
./package-source.sh projects/linux-6.10.5 pizlonated-linux
filc/projeny package projects/yolo-util-linux.projeny projects/yolo-util-linux/pizlonated-yolo-util-linux.tar.gz
