#!/bin/bash

set -e
set -x

# Install the Rust toolchains
# into the chroot and verify the Fil-C one end to end.
#
# (a) The Fil-C rustc (built on the host by ./build_rustc.sh from
#     projects/rust.projeny; packaged by build_postlc4_sub1_packaging.sh as
#     pizlonated-filrustc.tar.gz -- the stage1 sysroot: bin/ + lib/ with
#     librustc_driver-*.so and the rustlib std).  Installed at
#     /usr/lib/rust/filc and patchelfed yolo-style exactly like clang-20 in
#     build_lc.sh (interpreter + rpath into /yolo/lib): the Fil-C rustc,
#     like the Fil-C clang, links libstdc++ and libgcc statically and needs
#     only libm/libc, which /yolo/lib provides.  Exposed as /usr/bin/rustc;
#     it resolves its `filcc` linker from the PATH (pizlix mode:
#     /usr/bin/filcc).
#
# (b) The upstream dist rustc 1.90.0 + cargo (rust-yolo-1.90.0.tar.gz) at
#     /usr/lib/rust/yolo, patchelfed the same way, but NOT exposed on PATH:
#     cargo needs an upstream rustc for its proc-macro crates, and the
#     compiler dispatch shim decides how it is reached.  The
#     tarball ships the compat libraries the yolo loader cannot find in the
#     chroot (libgcc_s.so.1, libz.so.1, and the glibc libdl/librt/libpthread
#     stubs), because the yolo ld.so's only default search directory is
#     /yolo/lib and the stub NEEDED entries cannot be removed (their
#     DT_VERNEED entries would assert in dl-version.c).
#
# NOTE: like every postlc chroot script this must run with /proc mounted
# (the stage driver does build_mount.sh first): glibc expands $ORIGIN in the
# RUNPATH through /proc/self/exe.
#
# Verified below by compiling + running hello world with the Fil-C rustc:
# the produced binary is a Fil-C binary (linked via filcc against the
# pizlonated runtime) and prints through it.

cd /sources
rm -rf pizlonated-filrustc rust-yolo-1.90.0
tar -xf pizlonated-filrustc.tar.gz
tar -xf rust-yolo-1.90.0.tar.gz

# --- (a) the Fil-C rustc ------------------------------------------------------
rm -rf /usr/lib/rust/filc
mkdir -p /usr/lib/rust
cp -a pizlonated-filrustc /usr/lib/rust/filc
for f in /usr/lib/rust/filc/bin/rustc /usr/lib/rust/filc/lib/librustc_driver-*.so; do
    patchelf --set-rpath '/yolo/lib:$ORIGIN/../lib' $f
done
patchelf --set-interpreter /yolo/lib/ld-linux-x86-64.so.2 /usr/lib/rust/filc/bin/rustc
ln -sf /usr/lib/rust/filc/bin/rustc /usr/bin/rustc

# --- (b) upstream rustc + cargo (yolo; no /usr/bin exposure) -------------------
rm -rf /usr/lib/rust/yolo
cp -a rust-yolo-1.90.0 /usr/lib/rust/yolo
for f in /usr/lib/rust/yolo/bin/rustc /usr/lib/rust/yolo/bin/cargo \
         /usr/lib/rust/yolo/lib/librustc_driver-*.so \
         /usr/lib/rust/yolo/lib/libLLVM.so.20.1-rust-1.90.0-stable; do
    patchelf --set-rpath '/yolo/lib:$ORIGIN/../lib' $f
done
for f in /usr/lib/rust/yolo/bin/rustc /usr/lib/rust/yolo/bin/cargo; do
    patchelf --set-interpreter /yolo/lib/ld-linux-x86-64.so.2 $f
done
# The yolo rustc's libLLVM-20-rust-1.90.0-stable.so is a linker script; it
# needs no patchelf (see build_postlc4_sub1_packaging.sh).

# --- gates ---------------------------------------------------------------------
hash -r
rustc --version
rustc --version | grep -q "Fil-C 0.687" || {
    echo "ERROR: chroot rustc --version does not self-identify as Fil-C 0.687" >&2
    exit 1
}
/usr/lib/rust/yolo/bin/rustc --version
/usr/lib/rust/yolo/bin/cargo --version

cat > /tmp/filc-hello.rs <<'EOF'
fn main() {
    println!("hello from the pizlonated runtime");
}
EOF
rustc -O /tmp/filc-hello.rs -o /tmp/filc-hello
HELLO_OUT=`/tmp/filc-hello`
test "x$HELLO_OUT" = "xhello from the pizlonated runtime"
rm -f /tmp/filc-hello /tmp/filc-hello.rs

# --- cleanup --------------------------------------------------------------------
rm -rf /sources/pizlonated-filrustc /sources/rust-yolo-1.90.0

echo "Fil-C rustc hello: OK ($HELLO_OUT)"
hash -r