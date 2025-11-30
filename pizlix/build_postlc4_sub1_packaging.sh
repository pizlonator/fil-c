#!/bin/bash

set -e
set -x

ulimit -c unlimited

test $EUID -ne 0

test "x$FILCSRC" != "x"
test -d $FILCSRC
test -d $FILCSRC/projects

cd $FILCSRC

rm -vf projects/*/pizlonated-*.tar.gz
./package-source.sh projects/pygobject-3.48.2 pizlonated-pygobject
./package-source.sh projects/graphene-1.10.8 pizlonated-graphene
./package-source.sh projects/mesa-24.1.5 pizlonated-mesa
./package-source.sh projects/pango-1.54.0 pizlonated-pango
./package-source.sh projects/gdk-pixbuf-2.42.12 pizlonated-gdk-pixbuf
./package-source.sh projects/gtk-4.14.5 pizlonated-gtk4

