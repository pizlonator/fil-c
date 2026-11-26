#!/bin/bash

set -e
set -x

ulimit -c unlimited

# The merged postlc4 chroot script (the old postlc9 + postlc10 stages' sub2
# scripts, in order): the Rust toolchains, the librsvg dispatch, librsvg
# itself, then WebKitGTK, desktop-file-utils, libportal and Epiphany.  (The
# old postlc10 stage's GnuTLS trust-store rebuild is gone: postlc2 builds
# GnuTLS once with the pre-seeded CA bundle as its default trust store, so
# every https fetch works from the first build.)

cd /sources

# --- Rust toolchains + librsvg (the old postlc9 stage) ------------------------

./build_postlc4_chroot_project_rust.sh
hash -r
./build_postlc4_chroot_project_rust_dispatch.sh

./build_postlc4_chroot_project_librsvg.sh
hash -r

# --- WebKitGTK + Epiphany (the old postlc10 stage) -----------------------------

./build_postlc4_chroot_project_webkitgtk.sh
hash -r

./build_postlc4_chroot_project_desktop_file_utils.sh
hash -r
./build_postlc4_chroot_project_libportal.sh
hash -r
./build_postlc4_chroot_project_epiphany.sh
hash -r
