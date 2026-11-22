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

./package-source.sh projects/webkitgtk-2.44.3 pizlonated-webkitgtk

filc/projeny package projects/epiphany.projeny projects/epiphany/pizlonated-epiphany.tar.gz
filc/projeny package projects/libportal.projeny projects/libportal/pizlonated-libportal.tar.gz
filc/projeny package projects/desktop-file-utils.projeny projects/desktop-file-utils/pizlonated-desktop-file-utils.tar.gz

ls -la projects/webkitgtk-2.44.3/pizlonated-webkitgtk.tar.gz \
       projects/epiphany/pizlonated-epiphany.tar.gz \
       projects/libportal/pizlonated-libportal.tar.gz \
       projects/desktop-file-utils/pizlonated-desktop-file-utils.tar.gz
