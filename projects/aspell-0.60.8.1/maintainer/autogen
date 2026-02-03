#!/bin/sh

set -e

test -d interfaces || mkdir interfaces
test -d interfaces/cc || mkdir interfaces/cc
cd auto/
perl -I ./ mk-src.pl
perl -I ./ mk-doc.pl
touch auto
cd ..

cd scripts/
test -e preunzip || ln -s prezip preunzip
test -e precat   || ln -s prezip precat
cd ..

autopoint -f || exit 1
libtoolize --automake || exit 1
aclocal -I m4 $ACLOCAL_FLAGS || exit 1
autoheader || exit 1
automake --add-missing --foreign || exit 1
autoconf || exit 1

