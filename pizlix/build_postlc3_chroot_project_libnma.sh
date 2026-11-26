#!/bin/bash

set -e
set -x

ulimit -c unlimited

# libnma 1.10.6 -- the NetworkManager
# UI utilities.  Two shared libraries come out of this build:
#
#   libnma      (NMA-1.0 gir/typelib, gtk3)  -- the gtk3 variant; nothing in
#              the pizlonated system links it today, but upstream's meson
#              always builds the gtk3 variant and g-c-c's libnm.pc-style
#              consumers may want it; building it costs little and keeps
#              the install faithful.
#   libnma-gtk4 (NMA4-1.0 gir/typelib, gtk4) --
#              gnome-control-center's network panel links it
#              (dependency('libnma-gtk4', version: '>= 1.10.2')), and the
#              NMA4-1.0 typelib is what gnome-shell's statically-imported
#              gi://NMA4 (js/misc/modemManager.js via js/ui/status/network.js)
#              resolves at session start -- without it the shell's network
#              status module throws at import time.
#
# Meson flags (checked against meson_options.txt):
#   libnma_gtk4=true        DEFAULT FALSE upstream -- g-c-c links
#                           libnma-gtk4, must be on.
#   gcr=true                default true anyway; the chroot has gcr-4
#                           4.3.0 (>= 3.92), so meson's cc.compiles probe
#                           of the GCK/GCR prototypes picks gcr4_deps and
#                           the certificate chooser builds against gcr-4
#                           (the nma-pkcs11-* sources compile in).
#   iso_codes=true          iso-codes 4.20.1 installed (country names for
#                           the mobile wizard's provider database).
#   mobile_broadband_provider_info=false    not installed (the wizard
#                           falls back to an empty serviceproviders.xml
#                           path; Pizlix has no modems).
#   introspection=true      NMA-1.0 + NMA4-1.0 girs/typelibs (gjs needs
#                           NMA4-1.0).
#   vapi=false, gtk_doc=false, more_asserts=0, ld_gc=false
#                           no vala in the chroot; no gtk-doc; production
#                           assert level; no -ffunction-sections
#                           --gc-sections linking (house pattern: avoid
#                           gc-sections quirks under Fil-C).
#
# Dependencies: libnm >= 1.7 (NetworkManager 1.48.4, built earlier in this
# stage -- g-c-c and gnome-shell need libnm too, which is WHY the whole
# wifi userspace stack lives in this stage), gtk+-3.0
# >= 3.12 (3.24.52), gtk4 >= 4.0 (4.14.5, postlc2's build), gio,
# gmodule-export-2.0, gcr-4/gck-2 (4.3.0), iso-codes.
#
# Fil-C changes (see the projeny patch):
# - GLIB_VERSION_MAX_ALLOWED bumped 2.38 -> 2.80 in meson.build: the
#   pizlonated glib's GType is a pointer, and G_DEFINE_TYPE only routes
#   through the pointer-safe g_once_init_enter_pointer/g_once_init_leave_
#   pointer when GLIB_VERSION_MAX_ALLOWED >= 2.80 (gobject/gtype.h gate).
# - linker-script-binary.ver + src/libnma.ver: pizlonated_*; added to the
#   global: section (house pattern: the fil-C compiler references
#   functions through pizlonated_ forwarders; with only `local: *;` they
#   were hidden and consumers failed to link).

cd /sources
rm -rf pizlonated-libnma
tar -xf pizlonated-libnma.tar.gz
cd pizlonated-libnma
mkdir -v build
cd build
meson setup ..                   \
      --prefix=/usr              \
      --buildtype=debugoptimized \
      -Dlibnma_gtk4=true         \
      -Dgcr=true                 \
      -Diso_codes=true           \
      -Dmobile_broadband_provider_info=false \
      -Dintrospection=true       \
      -Dvapi=false               \
      -Dgtk_doc=false            \
      -Dmore_asserts=0           \
      -Dld_gc=false
ninja
ninja install
cd ../..

# --- gates ---------------------------------------------------------------------

echo "=== gate: the two libraries + their .pc files ==="
test -e /usr/lib/libnma.so
test -e /usr/lib/libnma-gtk4.so
test -e /usr/lib/pkgconfig/libnma.pc
test -e /usr/lib/pkgconfig/libnma-gtk4.pc
pkg-config --modversion libnma libnma-gtk4
ls -la /usr/lib/libnma.so* /usr/lib/libnma-gtk4.so*

echo "=== gate: headers installed ==="
test -e /usr/include/libnma/nma-wifi-dialog.h
test -e /usr/include/libnma/nma-cert-chooser.h
test -e /usr/include/libnma/nma-version.h
test -e /usr/include/libnma/nma-ws-802-1x.h
ls /usr/include/libnma/ | head -30

echo "=== gate: the gir/typelib pairs (gjs imports NMA4 by name) ==="
test -e /usr/share/gir-1.0/NMA-1.0.gir
test -e /usr/lib/girepository-1.0/NMA-1.0.typelib
test -e /usr/share/gir-1.0/NMA4-1.0.gir
test -e /usr/lib/girepository-1.0/NMA4-1.0.typelib
ls -la /usr/share/gir-1.0/NMA*.gir /usr/lib/girepository-1.0/NMA*.typelib

echo "=== gate: gjs can load BOTH typelibs ==="
# gjs is a late product of THIS stage (the shell block).  libnma must
# build before gnome-control-center links libnma-gtk4, so when this
# script runs, gjs is not in the chroot yet.  Run the gates when gjs
# exists; otherwise defer them to build_postlc3_sub2_chroot.sh, which
# re-runs them right after gjs is built.
if command -v gjs >/dev/null 2>&1; then
    gjs -c "const NM = imports.gi.NM; print('NM', NM.MAJOR_VERSION, NM.MINOR_VERSION);"
    gjs -c "const NMA4 = imports.gi.NMA4; print('NMA4 loaded, WifiDialog ctor:', typeof NMA4.WifiDialog, NMA4.WifiDialog.\$gtype);"
else
    echo "gjs not in the chroot yet (built later in this stage) -- libnma typelib gates deferred to the shell block"
fi

echo "=== gate: the pizlonated_ forwarders are exported ==="
nm -D --defined-only /usr/lib/libnma-gtk4.so | grep -c pizlonated_nma_

echo "=== gate: ldd of the libraries (no 'not found') ==="
for f in /usr/lib/libnma.so /usr/lib/libnma-gtk4.so; do
    if ldd $f 2>&1 | grep -q 'not found'; then
        echo "FATAL: $f has unresolved shared libraries:" >&2
        ldd $f 2>&1 | grep 'not found' >&2
        exit 1
    fi
done
echo "ldd: clean"

rm -rf pizlonated-libnma
hash -r

echo "postlc3 libnma: ALL GATES GREEN"
