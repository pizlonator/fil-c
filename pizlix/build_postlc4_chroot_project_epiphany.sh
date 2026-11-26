#!/bin/bash

set -e
set -x

ulimit -c unlimited


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

echo "postlc4 epiphany: ALL GATES GREEN"
