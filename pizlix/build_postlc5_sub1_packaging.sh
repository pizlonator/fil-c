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
./package-source.sh projects/gnutls-3.8.7.1 pizlonated-gnutls
./package-source.sh projects/glib-networking-2.80.0 pizlonated-glib-networking
./package-source.sh projects/libsoup-3.4.4 pizlonated-libsoup
./package-source.sh projects/libgpg-error-1.50 pizlonated-libgpg-error

