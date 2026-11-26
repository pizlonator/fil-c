#!/bin/bash

set -e
set -x

# mutter 46.9 (Wayland compositor and X11 window manager used by GNOME).
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
# -D introspection=false: nothing in this stage needs mutter's girs.
# -D docs=false is already the default (no man option exists in 46.9).
# udev stays enabled (default): the native backend requires it; libudev
# comes from elogind and libgudev is already installed.
# X11 client support is unconditional in mutter 46 (have_x11 = true), which
# is why this stage first rebuilds xkbcommon with xkbcommon-x11.
rm -rf pizlonated-mutter
tar -xf pizlonated-mutter.tar.gz
cd pizlonated-mutter
# The tarball is packaged from projects/mutter.projeny, which already carries
# the Fil-C fixes: cogl's CPU-caps detection no longer uses raw XGETBV inline
# asm and the bitmap premultiply path no longer uses inline asm with pointer
# arguments (both are rejected by the Fil-C safety validator; the C fallbacks
# are used instead).
mkdir -v build
cd build
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
      -D introspection=false    \
      -D docs=false
ninja
ninja install
cd ../..
rm -rf pizlonated-mutter
hash -r
