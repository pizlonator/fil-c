#!/bin/bash

set -e
set -x

# gjs 1.80.2 (GNOME 46 JavaScript bindings), the JS runtime for gnome-shell.
#
# This is the Fil-C port. The pizlonated tarball is packaged from
# projects/gjs.projeny, which carries the Fil-C fixes (see the projeny patch
# for the full list; at packaging time this includes at least):
#
# - gjs/engine.cpp: the explicit 1MB JS_SetNativeStackQuota is raised to 6MB
#   under __FILC__ (Fil-C frames are much larger; the mozjs port had to raise
#   the engine default from 1MB to 6MB for the same reason — with gjs's own
#   1MB override deep JS recursion would hit "over recursed" immediately).
#
# Flags (checked against meson_options.txt):
# -D installed_tests=false: no installed test harness.
# -D profiler=disabled: no sysprof-capture-4 in the system.
# -D bsymbolic_functions=false: -Bsymbolic-functions would resolve intra-library
#   calls to the local definitions; under Fil-C interposition/capability
#   semantics we keep the PLT (same reason the flag is dropped everywhere else).
# -D dtrace/systemtap (both default false): no SystemTap.
# -D skip_dbus_tests=true: we build no tests; dbus-run-session exists but the
#   test machinery (python-dbusmock etc.) does not.
# readline: enabled (auto feature; readline 8.3 + ncursesw are installed) for
#   gjs-console's interactive REPL.
# cairo: enabled (auto; cairo-gobject is installed) for gjs's cairo module.
# NB: cd before rm: the chroot drops us in /, so the rm must happen with an
# absolute CWD for the relative path to hit the stale tree (same as the mozjs
# script).
cd /sources
rm -rf pizlonated-gjs
tar -xf pizlonated-gjs.tar.gz
cd pizlonated-gjs
mkdir -v filc-build
cd filc-build
meson setup ..                  \
      --prefix=/usr             \
      --buildtype=debugoptimized \
      -Dinstalled_tests=false   \
      -Dprofiler=disabled       \
      -Dbsymbolic_functions=false \
      -Ddtrace=false            \
      -Dsystemtap=false         \
      -Dskip_dbus_tests=true    \
      -Dreadline=enabled        \
      -Dcairo=enabled
ninja
ninja install
cd ../..
rm -rf pizlonated-gjs
hash -r
