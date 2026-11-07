#!/bin/bash

set -e
set -x

# mutter 46.9 rebuild WITH GObject introspection (SPRINT SCRATCH / TEST of the
# gir path): gnome-shell needs the Meta/Clutter/Cally/Cogl/Mtk-14 typelibs, so
# this is the postlc7 mutter script with -Dintrospection=true instead of
# -Dintrospection=false.  Everything else is identical; this is a drop-in
# same-version rebuild (the installed mutter stays 46.9).
#
# Flags:
# -D xwayland=false: no Xwayland server in the system yet.
# -D glx=false: the pizlonated mesa is Wayland-only (built without GLX, so
#   there are no GL/glx.h headers) and with xwayland=false mutter has no
#   use for GLX anyway.
# -D remote_desktop=false: avoids pipewire (in mutter 46 remote desktop and
#   screencast are one feature gated behind libpipewire).
# -D profiler=false: no sysprof.
# -D libdisplay_info=disabled: not ported yet.
# -D tests=false: no tests (they need python-dbusmock, catch, xvfb-run, ...).
# -D sound_player=false: no libcanberra.
# -D startup_notification=false: no libstartup-notification.
# -D libwacom=false: not ported.
# -D systemd=false: mutter then falls back to libelogind for logind.
# -D introspection=true: produce the gir/typelib files gnome-shell needs.
#   g-ir-scanner builds a small dumper program against the just-built libs
#   and runs it, so introspection=true is a real build+run test of the libs.
# -D docs=false is already the default (no man option exists in 46.9).
# udev stays enabled (default): the native backend requires it; libudev
# comes from elogind and libgudev is already installed.
# NB: cd before rm: the chroot drops us in the caller's CWD, so the rm must
# happen with an absolute CWD for the relative path to hit the stale tree
# (same as the gjs/gnome-shell scripts).
cd /sources
rm -rf pizlonated-mutter
tar -xf pizlonated-mutter.tar.gz
cd pizlonated-mutter
mkdir -v build
cd build
# llvmpipe-on-JIT (sprint 3): mutter itself is a process that loads
# swrast_dri.so and runs llvmpipe shader compilation, so the mutter binary
# (and every other executable this build links) must carry the JIT-enabled
# Fil-C CRT: -enable-filc-jit at LINK time swaps filc_jit_crt.o in.  The flag
# is ignored (with a warning) for -shared links, so exporting it here is safe
# for the libmutter-14.so link steps too.  meson only reads LDFLAGS at setup.
export LDFLAGS="-enable-filc-jit"
meson setup ..                  \
      --prefix=/usr             \
      --buildtype=debugoptimized \
      -D xwayland=false         \
      -D glx=false              \
      -D remote_desktop=false   \
      -D profiler=false         \
      -D libdisplay_info=disabled \
      -D tests=false            \
      -D sound_player=false     \
      -D startup_notification=false \
      -D libwacom=false         \
      -D systemd=false          \
      -D introspection=true     \
      -D docs=false
ninja
ninja install
cd ../..
rm -rf pizlonated-mutter
hash -r
