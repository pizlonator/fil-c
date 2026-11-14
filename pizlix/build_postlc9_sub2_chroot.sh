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

# Sprint S3 support: rebuild glib with the GType capability-repair patch
# (Rust-registered GTypes cross the FFI as bare integers; glib's
# lookup_type_node_I must repair the TypeNode capability or the first
# dereference panics).  Must run BEFORE the librsvg build; needs
# pizlonated-glib.tar.gz in /sources (copied by the stage driver).
./build_postlc9_chroot_project_glib_gtyperepair.sh

# Sprint S2: the librsvg cargo build (the S2 staticlib gate; since S3 the
# same script continues with the autotools build, GIR, rsvg-convert, the
# gdk-pixbuf loader and the smoke gates).
./build_postlc9_chroot_project_librsvg.sh
hash -r