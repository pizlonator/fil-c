#!/bin/bash

set -e
set -x

# Sprint S2 of the librsvg-via-Fil-Rust port: build librsvg's Rust crate
# graph with cargo and the two-compiler dispatch (the autotools/librsvg-2.so
# integration is Sprint S3).  Runs INSIDE the chroot.
#
# Source: /sources/pizlonated-librsvg (the projeny package of
# projects/librsvg.projeny, extracted by the stage driver; for S2 hand-runs,
# extract /sources/pizlonated-librsvg.tar.gz).
#
# The build is the release profile of the librsvg-c package (the C API
# crate; crate-type = ["staticlib", "rlib"], i.e. the S2 gate artifact is
# liblibrsvg_c.a) with an explicit --target x86_64-unknown-linux-gnu so the
# dispatch shim sees --target units (Fil-C rustc) vs host units (upstream
# rustc).  --locked pins Cargo.lock; --offline + the CARGO_HOME
# vendored-sources replacement (written by
# build_postlc9_chroot_project_rust_dispatch.sh, pointing at
# /sources/vendor-librsvg) keeps the build off the network.

# Extract the package (idempotent; the stage driver copies the tarball into
# /sources with the shadow-tarball pattern).
rm -rf /sources/pizlonated-librsvg
tar -xf /sources/pizlonated-librsvg.tar.gz -C /sources
cd /sources/pizlonated-librsvg

export RUSTC_WRAPPER=/usr/bin/rustc-dispatch
export CARGO_HOME=/sources/cargo-home
export CARGO_TARGET_DIR=/hostbuild/librsvg-target
export CARGO_NET_OFFLINE=true
export HOME=/root
# pkg-config resolution for the gtk-rs -sys build scripts (glib/cairo/
# pango/gdk-pixbuf are the Fil-C builds installed under /usr).
export PKG_CONFIG_PATH=/usr/lib/pkgconfig:/usr/share/pkgconfig

time cargo build --offline --locked --target x86_64-unknown-linux-gnu \
     --release --package librsvg-c

# --- S2 gate --------------------------------------------------------------------
# The staticlib the autotools build (S3) will link into librsvg-2.so, plus
# the rlib.

test -e $CARGO_TARGET_DIR/x86_64-unknown-linux-gnu/release/liblibrsvg_c.a
test -e $CARGO_TARGET_DIR/x86_64-unknown-linux-gnu/release/liblibrsvg_c.rlib
ls -la $CARGO_TARGET_DIR/x86_64-unknown-linux-gnu/release/liblibrsvg_c.*

echo "S2 librsvg cargo gate: OK"