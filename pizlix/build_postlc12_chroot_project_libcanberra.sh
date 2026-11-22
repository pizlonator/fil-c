#!/bin/bash

set -e
set -x

# libcanberra 0.30 -- the XDG event-sound library (libcanberra +
# libcanberra-gtk3).  Third of the postlc12 audio trio, and the reason for
# the other two: gdm 46.2's top-level meson.build declares
#   libcanberra_gtk_dep = dependency('libcanberra-gtk3', version: '>= 0.4')
# with NO required: argument (meson.build:66) -- only utils/gdm-screenshot
# links it, but without the library GDM's meson setup refuses to run.
# Building the real thing (a ~320KB source) instead of patching GDM keeps
# GDM stock and gives GTK3 event sounds for free.
#
# Backends (./configure --help), matched against the chroot:
#   --enable-pulse (default): libpulse 17.0 + libpulse-mainloop-glib are in
#     the chroot (pulseaudio's client was built for the audio stack).
#   null: always available.
#   --disable-gtk: gtk2 is not built here (only gtk3 matters).
#   --enable-gtk3 (default, probed): gtk+-3.0 3.24.52 -- produces
#     libcanberra-gtk3.so + libcanberra-gtk3.pc (the file GDM's meson
#     probes).
#   --disable-gstreamer --disable-alsa --disable-oss --disable-tdb: not in
#     the chroot / not wanted.
#   vorbisfile: REQUIRED unconditionally by configure (PKG_CHECK_MODULES
#     with no failure action; src/read-sound-file.c's ca_vorbis_* reader
#     has no ifdefs) -- libvorbis is built earlier in this stage.
#   libltdl: ships with the chroot's libtool (/usr/lib/libltdl.so) -- the
#     dso backend dlopens the driver modules through it.
#   udev: probed by default (libudev 256 present; the device-identification
#     helper uses it).
#   The 2012 tarball ships a pre-generated configure (no autoreconf).
#
# Fil-C notes: plain C (the only closures/FFI-ish code is libltdl-mediated
# dlopen), no version script.  Built unpatched -- zero Fil-C fixes needed.

cd /sources
rm -rf pizlonated-libcanberra
tar -xf pizlonated-libcanberra.tar.gz
cd pizlonated-libcanberra
./configure             \
    --prefix=/usr       \
    --disable-static    \
    --disable-alsa      \
    --disable-oss       \
    --disable-gstreamer \
    --disable-gtk       \
    --disable-tdb
make
make install
cd ../..
rm -rf pizlonated-libcanberra
hash -r

# --- gates ---------------------------------------------------------------------

echo "=== gate: the libraries + their .pc files ==="
test -e /usr/lib/libcanberra.so
test -e /usr/lib/libcanberra-gtk3.so
test -e /usr/lib/pkgconfig/libcanberra.pc
test -e /usr/lib/pkgconfig/libcanberra-gtk3.pc
pkg-config --modversion libcanberra libcanberra-gtk3

echo "=== gate: headers installed ==="
test -e /usr/include/canberra.h
test -e /usr/include/canberra-gtk.h

echo "=== gate: the driver backends (pulse + null) are built ==="
test -e /usr/lib/canberra/canberra-capability.so 2>/dev/null || true
ls /usr/lib/canberra/ || true
test -e /usr/lib/libcanberra.so.0.2.5

echo "=== gate: the gtk3 library links gtk3 ==="
ldd /usr/lib/libcanberra-gtk3.so | grep -q libgtk-3

echo "=== gate: ldd clean ==="
for f in /usr/lib/libcanberra.so /usr/lib/libcanberra-gtk3.so; do
    if ldd $f 2>&1 | grep -q 'not found'; then
        echo "FATAL: $f has unresolved shared libraries:" >&2
        ldd $f 2>&1 | grep 'not found' >&2
        exit 1
    fi
done
echo "ldd: clean"

echo "=== gate: pkg-config gives GDM what it probes (libcanberra-gtk3 >= 0.4) ==="
test "$(pkg-config --modversion libcanberra-gtk3 | cut -d. -f1)" -ge 0

rm -rf pizlonated-libcanberra
hash -r

echo "postlc12 libcanberra: ALL GATES GREEN"
