#!/bin/bash

set -e
set -x

ulimit -c unlimited

cd /sources

# Sprint S1 of the librsvg-via-Fil-Rust port: the Rust toolchains.
# pizlonated-filrustc (the Fil-C rustc, exposed as /usr/bin/rustc) and
# rust-yolo-1.90.0 (upstream rustc + cargo at /usr/lib/rust/yolo, unexposed;
# the Sprint S2 dispatch shim decides the compiler dispatch).
#
# Sprint S2: the two-compiler dispatch (the /yolo link-time sysroot
# completion, the cargo exposure, the /usr/bin/rustc-dispatch shim, and the
# dispatch smoke gates).  Needs vendor-librsvg.tar.gz in /sources (the
# shadow-tarball pattern; build_postlc9.sh copies it explicitly).

./build_postlc9_chroot_project_rust.sh
hash -r
./build_postlc9_chroot_project_rust_dispatch.sh
hash -r