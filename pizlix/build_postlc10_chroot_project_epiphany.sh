#!/bin/bash

set -e
set -x

ulimit -c unlimited

# Sprint W3 of the Epiphany-on-Pizlix port: Epiphany 46.5 (GNOME Web).
#
# The browser itself.  Built with meson against the webkitgtk-6.0 API
# (2.44.3, the postlc10 WebKitGTK build -- USE_GTK4=ON) + gtk4/libadwaita/
# gcr-4/nettle/libarchive/gsettings-desktop-schemas/iso-codes/... (every
# .pc dependency probed in the chroot before this script was written:
# gtk4-unix-print 4.14.5, gcr-4 4.3.0, nettle+hogweed 3.10, libarchive
# 3.7.4, json-glib 1.10.6, libsecret 0.21.4, libsoup-3.0 3.4.4, sqlite3
# 3.46.1, iso-codes 4.16.0, gstreamer-1.0 1.24.7).  libportal-gtk4 comes
# from the libportal build earlier in this stage (build_postlc10_
# chroot_project_libportal.sh); desktop-file-validate (an optional epiphany
# test runner) from build_postlc10_chroot_project_desktop_file_utils.sh.
#
# LDFLAGS="-enable-filc-jit": the Fil-C JIT gate (house pattern from the
# gnome-text-editor build, build_postlc7_chroot_project_gnome_text_
# editor.sh).  GTK4's GSK GL renderer loads swrast_dri.so and runs
# llvmpipe shader compilation in-process, so the GUI executables must
# carry the JIT-enabled Fil-C CRT (filc_jit_crt.o swaps in at link time).
# The flag applies to executables; shared libraries warn-and-ignore it
# (verified driver behavior from the mutter/gnome-shell relink sprint), so
# exporting it for the whole build is safe: the headless helpers (ephy-
# profile-migrator, epiphany-search-provider, epiphany-webapp-provider)
# get the JIT CRT harmlessly, libephymain.so / the web-process-extension
# modules ignore it.  meson only reads LDFLAGS at setup.
#
# Fil-C notes (see projects/epiphany.projeny -- the 3-hunk patch applied by
# projeny setup):
# 1. meson.build: GLIB_VERSION_MAX_ALLOWED pinned to GLIB_VERSION_2_80
#    (house pattern: GType is a pointer in the pizlonated glib; the
#    gtype.h boilerplate then routes G_DEFINE_TYPE through the pointer-
#    based _g_type_once_init_* forms, and the GLib 2.80
#    g_once_init_enter_pointer used by hunk 3 is declared).
# 2. meson.build: subdir('help') dropped (itstool is not in the chroot;
#    the docs are not needed).
# 3. lib/ephy-flatpak-utils.c: the once-init pair converted to
#    g_once_init_enter_pointer / g_once_init_leave_pointer (the plain
#    forms do integer-typed stores that break the pointer location under
#    the pizlonated glib).
# No version scripts (.map files) ship with epiphany, so no symbol-table
# surgery is needed (contrast gcr in postlc7).

cd /sources
rm -rf pizlonated-epiphany
tar -xf pizlonated-epiphany.tar.gz
cd pizlonated-epiphany
export LDFLAGS="-enable-filc-jit"
mkdir -v build
cd build
meson setup ..                   \
      --prefix=/usr              \
      --buildtype=debugoptimized \
      -D unit_tests=disabled
ninja
ninja install
cd ../..

# --- gates ---------------------------------------------------------------------

echo "=== gate: the browser binary ==="
test -x /usr/bin/epiphany
ls -la /usr/bin/epiphany

echo "=== gate: web-process-extensions ==="
test -d /usr/lib/epiphany/web-process-extensions
test -e /usr/lib/epiphany/web-process-extensions/libephywebprocessextension.so
test -e /usr/lib/epiphany/web-process-extensions/libephywebextension.so
ls -la /usr/lib/epiphany/web-process-extensions/

echo "=== gate: libephymain ==="
test -e /usr/lib/epiphany/libephymain.so

echo "=== gate: desktop file ==="
test -e /usr/share/applications/org.gnome.Epiphany.desktop
ls -la /usr/share/applications/org.gnome.Epiphany.desktop

echo "=== gate: desktop-file-validate on the installed desktop file ==="
desktop-file-validate /usr/share/applications/org.gnome.Epiphany.desktop
echo "desktop-file-validate: OK"

echo "=== gate: gschema installed + compiled ==="
test -e /usr/share/glib-2.0/schemas/org.gnome.epiphany.gschema.xml
test -e /usr/share/glib-2.0/schemas/gschemas.compiled
# the compiled gvdb must know the schema (grep the binary for the id)
grep -q org.gnome.Epiphany /usr/share/glib-2.0/schemas/gschemas.compiled
ls -la /usr/share/glib-2.0/schemas/gschemas.compiled

echo "=== gate: helper binaries ==="
# Fil-C port note: ephy-profile-migrator installs to pkglibexecdir
# (/usr/libexec/epiphany), but epiphany-search-provider and
# epiphany-webapp-provider install to the plain libexecdir (/usr/libexec)
# -- src/meson.build uses libexecdir (not pkglibexecdir) for those two.
test -x /usr/libexec/epiphany/ephy-profile-migrator
test -x /usr/libexec/epiphany-search-provider
test -x /usr/libexec/epiphany-webapp-provider
ls -la /usr/libexec/epiphany/ /usr/libexec/epiphany-search-provider /usr/libexec/epiphany-webapp-provider

echo "=== gate: dbus service files + search provider ==="
test -e /usr/share/dbus-1/services/org.gnome.Epiphany.SearchProvider.service
test -e /usr/share/dbus-1/services/org.gnome.Epiphany.WebAppProvider.service
test -e /usr/share/gnome-shell/search-providers/org.gnome.Epiphany.SearchProvider.ini

echo "=== gate: ldd of every installed binary (no 'not found') ==="
for f in /usr/bin/epiphany /usr/libexec/epiphany-search-provider /usr/libexec/epiphany-webapp-provider \
         /usr/libexec/epiphany/ephy-profile-migrator \
         /usr/lib/epiphany/web-process-extensions/libephywebprocessextension.so; do
    if ldd $f 2>&1 | grep -q 'not found'; then
        echo "FATAL: $f has unresolved shared libraries:" >&2
        ldd $f 2>&1 | grep 'not found' >&2
        exit 1
    fi
done
echo "ldd: clean"

echo "=== gate: epiphany links the webkitgtk-6.0 API ==="
ldd /usr/bin/epiphany | grep -q libwebkitgtk-6.0.so
echo "libwebkitgtk-6.0.so: linked"

echo "=== gate: JIT-CRT marker on the GUI binary (the llvmpipe gate) ==="
nm -D /usr/bin/epiphany | grep -q filc_start_program_with_jit
echo "epiphany: JIT-CRT-LINKED"
for f in /usr/libexec/epiphany-search-provider /usr/libexec/epiphany-webapp-provider /usr/libexec/epiphany/ephy-profile-migrator; do
    if nm -D $f 2>/dev/null | grep -q filc_start_program_with_jit; then
        echo "$f: JIT-CRT-LINKED"
    else
        echo "$f: plain CRT (no jit marker; headless helper, harmless)"
    fi
done

rm -rf pizlonated-epiphany
hash -r

echo "postlc10 epiphany: ALL GATES GREEN"
