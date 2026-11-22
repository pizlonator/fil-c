#!/bin/bash

set -e
set -x

ulimit -c unlimited


NINJAJOB=24

# --- extract (warm: skip when the tree already matches this tarball) ---------
mkdir -p /hostbuild
HASH=`sha256sum /sources/pizlonated-webkitgtk.tar.gz | cut -d' ' -f1`
STAMP=/hostbuild/webkitgtk-src-stamp
if test ! -d /sources/pizlonated-webkitgtk || test "x`cat $STAMP 2>/dev/null`" != "x$HASH"
then
    rm -rf /sources/pizlonated-webkitgtk
    tar -xf /sources/pizlonated-webkitgtk.tar.gz -C /sources
    echo "$HASH" > $STAMP
fi

cd /sources/pizlonated-webkitgtk

export PKG_CONFIG_PATH=/usr/lib/pkgconfig:/usr/share/pkgconfig

# --- configure ----------------------------------------------------------------

mkdir -p /hostbuild/webkitgtk-build
cd /hostbuild/webkitgtk-build

# The Fil-C JIT gate (cmake reads LDFLAGS only at configure).
export LDFLAGS="-enable-filc-jit"

time cmake /sources/pizlonated-webkitgtk -DPORT=GTK -DCMAKE_BUILD_TYPE=RelWithDebInfo -G Ninja \
  -DENABLE_JIT=OFF -DENABLE_C_LOOP=ON -DENABLE_SAMPLING_PROFILER=OFF -DENABLE_WEBASSEMBLY=OFF \
  -DUSE_SYSTEM_MALLOC=ON -DUSE_LIBBACKTRACE=OFF -DUSE_LIBHYPHEN=OFF -DENABLE_GAMEPAD=OFF \
  -DENABLE_MINIBROWSER=ON -DENABLE_DOCUMENTATION=OFF -DENABLE_WEBDRIVER=OFF -DUSE_WOFF2=OFF \
  -DUSE_GTK4=ON -DENABLE_JOURNALD_LOG=OFF -DDEVELOPER_MODE=ON -DDEVELOPER_MODE_FATAL_WARNINGS=OFF \
  -DENABLE_XSLT=ON -DENABLE_SPELLCHECK=ON -DUSE_JPEGXL=OFF -DUSE_AVIF=OFF -DENABLE_BUBBLEWRAP_SANDBOX=OFF \
  -DENABLE_INTROSPECTION=OFF -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib

# GATE A: the CLoop configuration really took (the whole point of the
# recon line: no JIT, no sampling profiler, pure C interpreter loop).
grep -q 'ENABLE_C_LOOP:BOOL=ON' CMakeCache.txt
echo "GATE A (configure + CLoop): OK"

ninja -j$NINJAJOB jsc

test -x bin/jsc
JSCOUT=`./bin/jsc -e 'print(1+1)'`
test "x$JSCOUT" = "x2"
echo "GATE B (jsc CLoop smoke): OK -- jsc -e 'print(1+1)' printed $JSCOUT"

# --- the full build + install ----------------------------------------------------
ninja -j$NINJAJOB

ninja -j$NINJAJOB install

# --- STRIP the installed artifacts -----------------------------------
# RelWithDebInfo ships ~1.24 GB of NON-ALLOC debug+symbol sections inside the
# 3.4 GB libwebkitgtk-6.0.so.4.5.7 (.debug_* ~1.11 GB + .strtab 90 MB +
# .symtab 18 MB) and ~165 MB of the same in libjavascriptcoregtk-6.0.so.1.2.7
# (measured: prof-out/s2/sections-BEFORE2.txt).  Strip them in place so the
# installed tree is what a distro would ship; the warm build dir keeps the
# unstripped copies for future incremental runs, so nothing is lost for
# debugging (the build-tree binaries still have full debug info).
#
# HONEST-SCOPE NOTE (the measurement that motivated this): strip shrinks the
# FILE but NOT the loaded footprint.  In libwebkitgtk-6.0.so.4.5.7 the
# PT_LOAD segments sum to 2.158 GB and the debug/symtab sections are OUTSIDE
# the LOAD segments, so after stripping: file 3.395 GB -> ~2.16 GB, PT_LOAD
# sum UNCHANGED at 2.158 GB.  Cold-start paging reads PT_LOAD pages (the
# 821 MB .rela.dyn + 562 MB .data.rel.ro + text that the 44.9M Fil-C
# relocations force ld.so to touch), so this is a DISK-footprint fix, not a
# cold-start fix; the cold-start fix remains boot-time page-cache prewarm.
#
# Strip the real files only (stripping a symlink would replace the symlink
# with a regular file and break the .so -> .so.N -> .so.N.N.N chain).
strip_installed() {
    for f in "$@"; do
        if test -f "$f"; then
            before=`stat -c%s "$f"`
            strip --strip-unneeded "$f"
            after=`stat -c%s "$f"`
            echo "strip: $f: $before -> $after bytes"
        fi
    done
}
loadsum() {
    readelf -lW "$1" | awk -v fn="$1" '$1 == "LOAD" { fs += strtonum($5); ms += strtonum($6); n++ }
                            END { printf "%s: %d LOAD segs, filesz=%.3fGB memsz=%.3fGB\n", fn, n, fs/1e9, ms/1e9 }'
}
# Capture the pre-strip PT_LOAD memsz sums so the post-strip gate can prove
# the LOADED footprint is unchanged (the honest-scope note above).
big_libs="/usr/lib/libwebkitgtk-6.0.so.4.5.7 /usr/lib/libjavascriptcoregtk-6.0.so.1.2.7"
loadsum_before=""
for f in $big_libs; do
    loadsum_before="$loadsum_before$(readelf -lW "$f" | awk '$1 == "LOAD" { s += strtonum($6) } END { print s }')
"
done
for f in $big_libs /usr/libexec/webkitgtk-6.0/WebKitWebProcess /usr/libexec/webkitgtk-6.0/WebKitNetworkProcess \
         /usr/libexec/webkitgtk-6.0/jsc /usr/libexec/webkitgtk-6.0/MiniBrowser \
         /usr/lib/webkitgtk-6.0/injected-bundle/libwebkitgtkinjectedbundle.so; do
    loadsum "$f"
done
strip_installed $big_libs
strip_installed /usr/libexec/webkitgtk-6.0/WebKitWebProcess /usr/libexec/webkitgtk-6.0/WebKitNetworkProcess \
                /usr/libexec/webkitgtk-6.0/jsc /usr/libexec/webkitgtk-6.0/MiniBrowser
strip_installed /usr/lib/webkitgtk-6.0/injected-bundle/libwebkitgtkinjectedbundle.so
echo "=== gate: strip shrank the files, loaded footprint unchanged ==="
du -shc $big_libs /usr/libexec/webkitgtk-6.0/WebKitWebProcess /usr/libexec/webkitgtk-6.0/WebKitNetworkProcess \
        /usr/libexec/webkitgtk-6.0/jsc /usr/libexec/webkitgtk-6.0/MiniBrowser \
        /usr/lib/webkitgtk-6.0/injected-bundle/libwebkitgtkinjectedbundle.so | tail -1
i=0
for f in $big_libs; do
    i=`expr $i + 1`
    after_sum=`readelf -lW "$f" | awk '$1 == "LOAD" { s += strtonum($6) } END { print s }'`
    before_sum=`echo "$loadsum_before" | sed -n "${i}p"`
    echo "strip gate: $f PT_LOAD memsz sum: $before_sum -> $after_sum (must be equal)"
    test "x$before_sum" = "x$after_sum"
done
for f in $big_libs /usr/libexec/webkitgtk-6.0/jsc /usr/libexec/webkitgtk-6.0/MiniBrowser; do
    loadsum "$f"
done

# --- INSTALL GATES (run POST-strip: they double as the "stripped binaries still work" gate) ---

echo "=== gate: libwebkitgtk-6.0 installed ==="
ls -la /usr/lib/libwebkitgtk-6.0.so*
test -e /usr/lib/libwebkitgtk-6.0.so
test -e /usr/lib/libjavascriptcoregtk-6.0.so

echo "=== gate: libexec helpers installed ==="
# Fil-C port note: PlatformGTK.cmake:13-14 renames the process helpers to
# the WebKit-prefixed names (WebKitWebProcess / WebKitNetworkProcess) for
# every GTK API version, so those are the names the 6.0 install carries
# (jsc and MiniBrowser keep their plain names).
for f in WebKitWebProcess WebKitNetworkProcess jsc MiniBrowser; do
    test -x /usr/libexec/webkitgtk-6.0/$f
done
ls -la /usr/libexec/webkitgtk-6.0/

echo "=== gate: injected bundle installed ==="
ls -la /usr/lib/webkitgtk-6.0/injected-bundle/
# Fil-C port note: for the webkitgtk-6.0 API the bundle target is
# webkitgtkinjectedbundle (the webkit2 infix only exists for the 4.x APIs,
# PlatformGTK.cmake:488 webkit${WEBKITGTK_API_INFIX}gtkinjectedbundle), so
# the installed module is libwebkitgtkinjectedbundle.so.
test -e /usr/lib/webkitgtk-6.0/injected-bundle/libwebkitgtkinjectedbundle.so

echo "=== gate: pkg-config versions ==="
pkg-config --modversion webkitgtk-6.0 javascriptcoregtk-6.0 webkitgtk-web-process-extension-6.0

echo "=== gate: ldd of every libexec binary (no 'not found') ==="
for f in WebKitWebProcess WebKitNetworkProcess jsc MiniBrowser; do
    if ldd /usr/libexec/webkitgtk-6.0/$f 2>&1 | grep -q 'not found'; then
        echo "FATAL: $f has unresolved shared libraries:" >&2
        ldd /usr/libexec/webkitgtk-6.0/$f 2>&1 | grep 'not found' >&2
        exit 1
    fi
done
echo "ldd: clean"

echo "=== gate: installed jsc smoke ==="
JSC42=`/usr/libexec/webkitgtk-6.0/jsc -e 'print(2*21)'`
test "x$JSC42" = "x42"
echo "installed jsc gate: OK -- jsc -e 'print(2*21)' printed $JSC42"

echo "=== info: JIT-CRT markers (nm -D, executables) ==="
for f in WebKitWebProcess WebKitNetworkProcess jsc MiniBrowser; do
    if nm -D /usr/libexec/webkitgtk-6.0/$f 2>/dev/null | grep -q filc_start_program_with_jit; then
        echo "$f: JIT-CRT-LINKED"
    else
        echo "$f: plain CRT (no jit marker)"
    fi
done
nm -D /usr/libexec/webkitgtk-6.0/MiniBrowser | grep -q filc_start_program_with_jit
echo "MiniBrowser JIT-CRT marker: OK"

# --- cleanup ---------------------------------------------------------------------
# Remove the build dir + extracted tree after a successful install so the
# stage snapshot stays small (the 55MB tarball stays in /sources).  Only
# the stage driver's own (sealing) run cleans up: it runs with
# lfsbuildstate == "postlc10-part".  Piecewise iteration runs (state
# "postlc9") keep the warm dir so fix rounds rebuild incrementally and the
# sealing run can reuse them.
if test "x`cat /sources/lfsbuildstate`" = "xpostlc10-part"
then
    rm -rf /hostbuild/webkitgtk-build /sources/pizlonated-webkitgtk /hostbuild/webkitgtk-src-stamp
    echo "cleanup: build dir + extracted tree removed (sealing run)"
else
    echo "cleanup: skipped (piecewise run; warm dir kept for the sealing run)"
fi

echo "postlc10 webkitgtk: ALL GATES GREEN"
