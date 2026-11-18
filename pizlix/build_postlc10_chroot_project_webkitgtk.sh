#!/bin/bash

set -e
set -x

ulimit -c unlimited

# Sprint W2 of the Epiphany-on-Pizlix port: build WebKitGTK 2.44.3 in the
# Pizlix chroot and install it (the webkitgtk-6.0 API: USE_GTK4=ON, so
# libwebkitgtk-6.0.so + the libexec helpers webkitgtk-6.0/{WebProcess,
# NetworkProcess,jsc,MiniBrowser}; Epiphany 46 links exactly this API).
#
# Source: /sources/pizlonated-webkitgtk.tar.gz (package-source.sh over the
# checked-in projects/webkitgtk-2.44.3 tree; the same 61-commit Fil-C tree
# the recon validated: configure 38.8s + the first 651 ninja steps ran
# inside this chroot).  Runs INSIDE the chroot via build_chroot_late.sh.
#
# The cmake line is the recon-validated one, with one flip: ENABLE_XSLT=ON
# (libxslt 1.1.42 was built in postlc6; the recon line had it OFF only
# because it mirrored the dev-container script do_cmake_yolo_simpler.sh).
#
# The Fil-C JIT gate: any process that loads swrast_dri.so and compiles
# llvmpipe shaders must link filc_jit_crt.o via LDFLAGS="-enable-filc-jit"
# (cmake reads LDFLAGS only at configure time; executables get it, shared
# libs warn-and-ignore it -- the verified driver behavior from the mutter/
# gnome-shell relink sprint).  GUI processes here: MiniBrowser, WebProcess;
# NetworkProcess is safe; jsc is headless so the flag is harmless there.
#
# Warm build dir: the build tree lives at /hostbuild/webkitgtk-build (the
# snapshot excludes hostbuild/*, and Fil-C TUs spike >1.5-2GB each so the
# object tree is big).  The extracted source tree and build dir survive
# re-runs (keyed on the tarball hash), so fix iterations re-run ninja
# incrementally instead of rebuilding ~2h from zero.  After the stage
# driver's own (sealing) run the script removes both -- see the cleanup
# gate on lfsbuildstate at the bottom (piecewise runs during iteration
# leave the warm dir in place for the sealing run; the sealing run is the
# one that runs with lfsbuildstate == "postlc10-part").
#
# ninja -j24 explicitly (MAKEFLAGS does not affect ninja; 59G RAM with
# Fil-C TUs spiking: j24 is the safe parallelism; drop to -j16 and resume
# if the box OOMs -- ninja resumes incrementally, no reconfigure needed).

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

# --- GATE B: the CLoop jsc smoke (in the build tree) ---------------------------
# Mirrors the port's proven state: jsc runs headless on the pure-C
# interpreter.  ~15-30 min on this box (WTF + JavaScriptCore + the LLInt
# codegen all build as dependencies of jsc).
ninja -j$NINJAJOB jsc

test -x bin/jsc
JSCOUT=`./bin/jsc -e 'print(1+1)'`
test "x$JSCOUT" = "x2"
echo "GATE B (jsc CLoop smoke): OK -- jsc -e 'print(1+1)' printed $JSCOUT"

# --- the full build + install ----------------------------------------------------
ninja -j$NINJAJOB

ninja -j$NINJAJOB install

# --- INSTALL GATES ---------------------------------------------------------------

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
# MiniBrowser is the GUI process that will load swrast_dri.so -- its JIT
# marker is the build-side gate (the llvmpipe sprint's verified marker).
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
