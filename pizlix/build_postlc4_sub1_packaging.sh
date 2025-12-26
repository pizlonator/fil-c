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
./package-source.sh projects/pygobject-3.48.2 pizlonated-pygobject
./package-source.sh projects/graphene-1.10.8 pizlonated-graphene
./package-source.sh projects/mesa-24.1.5 pizlonated-mesa
./package-source.sh projects/pango-1.54.0 pizlonated-pango
./package-source.sh projects/gdk-pixbuf-2.42.12 pizlonated-gdk-pixbuf
./package-source.sh projects/gtk-4.14.5 pizlonated-gtk4
./package-source.sh projects/nettle-3.10 pizlonated-nettle
./package-source.sh projects/libgudev-238 pizlonated-libgudev
./package-source.sh projects/gstreamer-1.24.7 pizlonated-gstreamer
./package-source.sh projects/gst-plugins-bad-1.24.7 pizlonated-gst-plugins-bad
./package-source.sh projects/gst-plugins-base-1.24.7 pizlonated-gst-plugins-base

