#!/bin/bash

set -e
set -x

# pulseaudio 17.0 — CLIENT-ONLY build (libpulse + libpulse-mainloop-glib) for
# the vendored gvc (libgvc) subproject that BOTH gnome-control-center
# (postlc7, its meson.build executes subproject('gvc') before any other
# dependency check) and gnome-shell (postlc8) pull in.  They only ever
# speak the client API (they connect to a pipewire-pulse/daemon over the
# native protocol), so -Ddaemon=false skips the whole server side (which
# would pull in alsa/webrtc-audio-processing/orc and a much bigger port).
#
# Built in postlc7 (moved from postlc8, which built it after g-c-c): g-c-c
# hard-requires libpulse + libpulse-mainloop-glib at meson setup, so the
# client libraries must exist before this stage's gnome-control-center
# build.  Its own hard deps are all earlier-or-in-stage: glib
# (-Dglib=enabled; postlc3), libsndfile (the dependency('sndfile') probe
# is unconditional even for the client build; built just before this
# script), dbus-1 (auto feature; postlc7's dbus build runs first in the
# stage), x11-xcb (auto; postlc6).
#
# Flags (checked against meson_options.txt):
# -D daemon=false: client libraries only.
# -D client=true (default).
# -D doxygen=false -Dman=false -Dtests=false: docs/tests off.
# -D glib=enabled: libpulse-mainloop-glib (gvc needs libpulse-mainloop-glib).
# Everything else is a 'feature' with value auto: with daemon=false the
# daemon-only deps (alsa, bluez5, orc, jack, lirc, fftw, soxr, speex,
# webrtc-aec, gstreamer, systemd, udev, avahi...) are not even probed; x11
# is probed and enabled (X11 libs are installed; libpulse only uses it for
# pax11publish/properties), which is harmless.
#
# Fil-C fixes (projects/pulseaudio.projeny):
# - src/pulse/map-file: + pizlonated*; (consumers link the pizlonated_ thunks;
#   "local: *" would hide them and break gvc/pactl/g-ir dumper).
cd /sources
rm -rf pizlonated-pulseaudio
tar -xf pizlonated-pulseaudio.tar.gz
cd pizlonated-pulseaudio
mkdir -v filc-build
cd filc-build
meson setup ..                  \
      --prefix=/usr             \
      --buildtype=debugoptimized \
      -Ddaemon=false            \
      -Dclient=true             \
      -Ddoxygen=false           \
      -Dman=false               \
      -Dtests=false             \
      -Dglib=enabled
ninja
ninja install
cd ../..
rm -rf pizlonated-pulseaudio
hash -r
