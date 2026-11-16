#!/bin/bash

set -e
set -x

ulimit -c unlimited

test "x$FILCSRC" != "x"
test -d $FILCSRC
test -d $FILCSRC/projects

test $EUID -eq `stat -c %u $FILCSRC`

cd $FILCSRC

# Sprint S1 of the librsvg-via-Fil-Rust port: package the two Rust
# toolchains the chroot needs.  Neither is a projeny project: both are
# packaged straight from build products into rust-build/ (the
# untracked rust build-output directory, so the tarballs stay out of git,
# like the stage1 tree they are made from).  The postlc9 driver copies them
# into $LFS/sources with explicit cp lines (the shadow-tarball pattern).
#
# 1. pizlonated-filrustc.tar.gz: the Fil-C rustc's stage1 sysroot, as built
#    by ./build_rustc.sh into rust-build/xpy/<host>/stage1 (bin/ + lib/,
#    where lib/ holds librustc_driver-*.so and the rustlib std).  The
#    binaries are stripped (exactly like clang-20 in build_lc.sh); the
#    patchelfing into a yolo-style binary happens at INSTALL time in the
#    chroot (build_postlc9_chroot_project_rust.sh), so this tarball carries
#    the pristine tree.
#
# 2. rust-yolo-1.90.0.tar.gz: the upstream dist rustc 1.90.0 + cargo, for
#    compiling proc-macro crates (the Fil-C rustc's metadata version check
#    normalizes the " (built from a source tarball)" suffix, so same-version
#    upstream-built proc-macros load; see projects/rust.projeny,
#    rustc_metadata/src/rmeta/decoder.rs).  Assembled from the official dist
#    tarball (downloaded into rust-build/ and cached there if missing), with
#    the compat libraries the chroot cannot supply: libgcc_s.so.1, libz.so.1
#    and the glibc libdl/librt/libpthread stubs.  The yolo ld.so's only
#    default search directory is /yolo/lib, and the stub NEEDED entries
#    cannot simply be removed (patchelf --remove-needed leaves their
#    DT_VERNEED entries behind, which asserts in dl-version.c: "needed !=
#    NULL"), so the toolchain ships them; they are empty shims whose
#    functionality lives in the yolo libc.

STAGE1=rust-build/xpy/x86_64-unknown-linux-gnu/stage1
if test ! -e $STAGE1/bin/rustc; then
    # The stage1 sysroot is missing (fresh clone, or rust-build/xpy was
    # wiped, e.g. together with /mnt/lfs).  Build the Fil-C rustc before
    # tarring: this script already runs as FILCOWNER (the EUID check
    # above), which is the user build_rustc.sh expects, and the clean
    # build takes ~15 minutes at postlc9 time.  build_rustc.sh wipes and
    # rewrites rust-build/xpy plus the result symlinks (rust-build/rustc,
    # rust-build/filrustc) itself, so nothing dangles afterward.
    echo "$STAGE1/bin/rustc not found; running ./build_rustc.sh (~15 min)..." >&2
    # cwd is already $FILCSRC (cd'd at the top of this script).  FILCSRC is
    # the relative path `..`, so a second `cd $FILCSRC` here would walk UP
    # out of the repo and ./build_rustc.sh would not be found.
    ./build_rustc.sh
fi

# --- 1. the Fil-C rustc ------------------------------------------------------
rm -rf rust-build/pizlonated-filrustc
mkdir -p rust-build/pizlonated-filrustc
cp -a $STAGE1/. rust-build/pizlonated-filrustc/
strip rust-build/pizlonated-filrustc/bin/rustc
strip rust-build/pizlonated-filrustc/bin/rustdoc
strip rust-build/pizlonated-filrustc/lib/librustc_driver-*.so
rm -f rust-build/pizlonated-filrustc.tar.gz
tar -czf rust-build/pizlonated-filrustc.tar.gz \
    -C rust-build pizlonated-filrustc
rm -rf rust-build/pizlonated-filrustc

# --- 2. upstream rustc + cargo (yolo) ----------------------------------------
DISTBALL=rust-build/rust-1.90.0-x86_64-unknown-linux-gnu.tar.xz
DISTURL=https://static.rust-lang.org/dist/rust-1.90.0-x86_64-unknown-linux-gnu.tar.xz
if [ ! -e $DISTBALL ]; then
    # Retry + resume, same hardening as download_tarball in
    # build_postlc6_sub1_packaging.sh: --retry alone does not retry an
    # error like curl 92 ("HTTP/2 stream not closed cleanly"), and without
    # --continue-at - a retry restarts this ~200MB tarball from byte 0.
    curl -fL --retry 5 --retry-all-errors --continue-at - -o $DISTBALL $DISTURL
fi
rm -rf rust-build/.rust-yolo-dist
mkdir -p rust-build/.rust-yolo-dist
tar -xf $DISTBALL -C rust-build/.rust-yolo-dist
DIST=$PWD/rust-build/.rust-yolo-dist/rust-1.90.0-x86_64-unknown-linux-gnu
# $PWD here is $FILCSRC (this script cd'd there at the top).  Keep DIST
# ABSOLUTE: the vendoring step below runs cargo from inside the projeny
# checkout (cd projects/librsvg), where a relative DIST would point at a
# nonexistent nested path.  (The zero-untracked ./build.sh caught that: the
# vendor cache is missing on a fresh clone, so the vendoring actually runs,
# which no earlier run ever did -- they all reused the cached tarball.)

rm -rf rust-build/rust-yolo-1.90.0
mkdir -p rust-build/rust-yolo-1.90.0/bin
mkdir -p rust-build/rust-yolo-1.90.0/lib
cp -a $DIST/rustc/bin/rustc rust-build/rust-yolo-1.90.0/bin/
cp -a $DIST/cargo/bin/cargo rust-build/rust-yolo-1.90.0/bin/
cp -a $DIST/rustc/lib/librustc_driver-*.so rust-build/rust-yolo-1.90.0/lib/
cp -a $DIST/rustc/lib/libLLVM.so.20.1-rust-1.90.0-stable rust-build/rust-yolo-1.90.0/lib/
# libLLVM-20-rust-1.90.0-stable.so is a one-line GNU ld linker script
# (INPUT(libLLVM.so.20.1-rust-1.90.0-stable)) used at link time, not an ELF
# object: copy it as-is, and do NOT patchelf it.
cp -a $DIST/rustc/lib/libLLVM-20-rust-1.90.0-stable.so rust-build/rust-yolo-1.90.0/lib/
cp -a $DIST/rust-std-x86_64-unknown-linux-gnu/lib/rustlib rust-build/rust-yolo-1.90.0/lib/

# The compat libraries: taken from the build host (any modern glibc provides
# them; they are either version-stable shims like the stubs and libz, or a
# self-contained library like libgcc_s whose only dynamic dependency is
# libc.so.6, which the yolo loader dedups onto the already-loaded yolo
# libc).  -aL dereferences symlinks: Debian-style layouts ship libz.so.1 as
# a symlink to libz.so.1.<micro>, which would arrive broken inside the
# tarball; the copy must be a real file.
cp -aL /usr/lib/x86_64-linux-gnu/libgcc_s.so.1 \
      /usr/lib/x86_64-linux-gnu/libz.so.1 \
      /usr/lib/x86_64-linux-gnu/libdl.so.2 \
      /usr/lib/x86_64-linux-gnu/librt.so.1 \
      /usr/lib/x86_64-linux-gnu/libpthread.so.0 \
      rust-build/rust-yolo-1.90.0/lib/

rm -f rust-build/rust-yolo-1.90.0.tar.gz
tar -czf rust-build/rust-yolo-1.90.0.tar.gz \
    -C rust-build rust-yolo-1.90.0

# --- 3. the Fil-C-patched Rust crates (projeny products) -----------------------
#
# The crates the Fil-C port patches (glib-sys, gobject-sys, glib,
# string_cache) are PROJENY packages, exactly like librsvg itself:
# projects/<crate>.projeny names the crates.io .crate tarball (URL + blake3)
# and carries the Fil-C patch.  `projeny package` sets the project up
# (download + verify + unpack + apply patch) and tars the tracked files, so
# this step is what makes the patched trees exist on the build host -- which
# the vendoring step below also needs, since the librsvg workspace's
# [patch.crates-io] section points at them through relative paths
# (../glib-sys from projects/librsvg).
#
# The tarballs are named without the pizlonated- prefix on purpose: the root
# directory inside each tarball must be <crate>/ so that unpacking it into
# /sources yields /sources/<crate>/, the same relative position the
# [patch.crates-io] paths expect.  (The unpacked tree IS the Fil-C-patched
# source; the distinction lives in the .projeny patch, not the filename.)

filc/projeny package projects/glib-sys.projeny projects/glib-sys/glib-sys.tar.gz
filc/projeny package projects/gobject-sys.projeny projects/gobject-sys/gobject-sys.tar.gz
filc/projeny package projects/glib.projeny projects/glib/glib.tar.gz
filc/projeny package projects/string_cache.projeny projects/string_cache/string_cache.tar.gz

# --- 4. the librsvg projeny package -------------------------------------------
# Packaged from the (patched) checkout; the driver copies it into
# $LFS/sources and build_postlc9_chroot_project_librsvg.sh extracts it at
# /sources/pizlonated-librsvg.  Since the GType-as-pointer port, the
# checkout's patch body also carries the workspace-level [patch.crates-io]
# section that wires the four patched crates from step 3 into every cargo
# build of this workspace.
#
# This step must run BEFORE the vendoring step below: `projeny package`
# sets the checkout up, and both the vendoring (cargo resolves the workspace
# graph, including the [patch] paths ../glib-sys etc. and the pinned
# Cargo.lock) and the checkout's very existence on a fresh clone depend on
# it.  (The zero-untracked ./build.sh caught exactly that ordering bug: with
# no projects/librsvg on disk, the old vendor-first order died at its
# `test -d projects/librsvg`.)

filc/projeny package projects/librsvg.projeny projects/librsvg/pizlonated-librsvg.tar.gz

# --- 5. librsvg vendored dependencies (the S2/S3 cargo builds) ----------------
#
# The librsvg dependency closure, vendored ONCE at packaging time so every
# chroot cargo build is fully offline.  NOT a projeny product (like the
# toolchains above): the vendor tree ships as its own shadow tarball
# (rust-build/vendor-librsvg.tar.gz -> /sources/vendor-librsvg) and is kept
# OUT of the librsvg checkout on purpose -- a projeny checkout is managed
# (`projeny setup` restores pristine+patch, which would wipe untracked
# content like a vendor/ tree), and the vendored-sources replacement is
# configured at build time through CARGO_HOME instead
# (build_postlc9_chroot_project_rust_dispatch.sh), so no generated cargo
# config ever lands inside the checkout either.
#
# The vendoring runs against the projeny checkout (projects/librsvg), whose
# Cargo.lock pins the exact dependency versions, using the dist cargo from
# the same 1.90.0 toolchain (vendoring only downloads; nothing compiles).
# The Fil-C-patched crates (glib-sys, gobject-sys, glib, string_cache) are
# path dependencies through [patch.crates-io], so cargo vendor excludes them
# -- they reach the chroot as their own projeny tarballs (step 3), never
# through the vendor tree.  The result is cached in rust-build/ like the
# toolchain tarballs.

VENDOR_TARBALL=rust-build/vendor-librsvg.tar.gz
if [ ! -e $VENDOR_TARBALL ]; then
    test -d projects/librsvg
    rm -rf rust-build/.librsvg-vendor
    mkdir -p rust-build/.librsvg-vendor
    (cd projects/librsvg && \
     CARGO_HOME=$OLDPWD/rust-build/.librsvg-vendor/cargo-home \
     $DIST/cargo/bin/cargo vendor --locked \
     $OLDPWD/rust-build/.librsvg-vendor/vendor)
    tar -czf $VENDOR_TARBALL -C rust-build/.librsvg-vendor vendor
    rm -rf rust-build/.librsvg-vendor
fi

rm -rf rust-build/.rust-yolo-dist rust-build/rust-yolo-1.90.0

ls -la rust-build/pizlonated-filrustc.tar.gz rust-build/rust-yolo-1.90.0.tar.gz \
       rust-build/vendor-librsvg.tar.gz projects/librsvg/pizlonated-librsvg.tar.gz \
       projects/glib-sys/glib-sys.tar.gz projects/gobject-sys/gobject-sys.tar.gz \
       projects/glib/glib.tar.gz projects/string_cache/string_cache.tar.gz