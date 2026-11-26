#!/bin/bash

set -e
set -x

# librsvg-via-Fil-Rust port:
#   The cargo stage builds the librsvg workspace for Pizlix with
#     the two-compiler dispatch (RUSTC_WRAPPER shim); gate artifact is the
#     staticlib liblibrsvg_c.a in the warm target dir
#     /hostbuild/librsvg-target/x86_64-unknown-linux-gnu/release/.
#   The autotools stage (this script): the autotools build on top -- ./configure + make +
#     make install produce librsvg-2.so (the Fil-C link of the S2 staticlib),
#     Rsvg-2.0.gir + .typelib (g-ir-scanner), and rsvg-convert (a Fil-C
#     binary straight out of cargo); gates verify exports, ldd, and SVG->PNG
#     rendering.
#   The gdk-pixbuf stage (this script): the gdk-pixbuf SVG loader (libpixbufloader-svg.so,
#     plain C) built+installed by the same make, the loaders.cache refresh,
#     and the end-to-end gdk_pixbuf_new_from_file smoke (normal + symbolic
#     SVG).
#
# Source: /sources/pizlonated-librsvg.tar.gz (the projeny package of
# projects/librsvg.projeny -- its patch carries the Makefile.am /
# Makefile.in Fil-C fixes: unconditional CARGO_TARGET_ARGS=--target +
# pinned RUST_TARGET/RUST_TARGET_SUBDIR, environment-honoring
# CARGO_TARGET_DIR, and the -export-symbols-regex extended to the
# pizlonated_* names; since the GType-as-pointer port it also carries the
# workspace-level [patch.crates-io] section).  Runs INSIDE the chroot via
# build_chroot_late.sh.
#
# The cargo invocation here uses an explicit --target x86_64-unknown-linux-gnu
# so the dispatch shim sees --target units (Fil-C rustc) vs host units
# (upstream rustc).  --locked pins Cargo.lock; --offline + the CARGO_HOME
# vendored-sources replacement (written by
# build_postlc4_chroot_project_rust_dispatch.sh, pointing at
# /sources/vendor-librsvg) keeps the build off the network.
#
# Fil-C-patched crates: glib-sys, gobject-sys, glib and string_cache arrive
# as their own projeny tarballs (sub1_packaging packages them; the stage
# driver copies them into /sources).  They are unpacked NEXT TO the librsvg
# tree so the workspace's [patch.crates-io] relative paths (../glib-sys
# etc.) resolve, and cargo builds them as part of the librsvg graph -- and it is
# how the Fil-C GType-as-pointer fix reaches the build: glib-sys declares
# GType as a pointer, so GTypes crossing the C<->Rust FFI keep their filc
# capabilities and glib itself needs no changes (it was built in postlc3 and
# is NOT rebuilt here).

# Extract the package (idempotent; the stage driver copies the tarball into
# /sources with the shadow-tarball pattern).
rm -rf /sources/pizlonated-librsvg
tar -xf /sources/pizlonated-librsvg.tar.gz -C /sources
cd /sources/pizlonated-librsvg

# --- the patched crates ([patch.crates-io] sources) ---------------------------

for crate in glib-sys gobject-sys glib string_cache; do
    rm -rf /sources/$crate
    tar -xf /sources/$crate.tar.gz -C /sources
done

# Gates: the [patch] wiring and the patched sources must actually be here.
# If the wiring silently regressed, cargo would build against the UNPATCHED
# crates.io sources -- the integer GType -- and the failure would only show
# up at RUNTIME (a filc panic on the first Rust-registered GType), so check
# for it explicitly.
test -f /sources/pizlonated-librsvg/Cargo.toml
grep -q '^\[patch\.crates-io\]' /sources/pizlonated-librsvg/Cargo.toml
for crate in glib-sys gobject-sys glib string_cache; do
    grep -q "path = \"../$crate\"" /sources/pizlonated-librsvg/Cargo.toml
    test -f /sources/$crate/Cargo.toml
done
grep -q 'pub struct GType(pub \*const GTypeOpaque);' /sources/glib-sys/src/manual.rs
grep -q 'pub const G_TYPE_STRING: GType = GType((16 << G_TYPE_FUNDAMENTAL_SHIFT) as \*const GTypeOpaque);' \
    /sources/gobject-sys/src/lib.rs
grep -q 'dynamic_entry: \*mut Entry,' /sources/string_cache/src/atom.rs
grep -q 'use std::sync::Mutex;' /sources/string_cache/src/dynamic_set.rs
grep -q 'as usize' /sources/glib/src/subclass/signal.rs
echo "patched-crate gates: OK"

# --- the dispatch environment (S2) ------------------------------------------------
export RUSTC_WRAPPER=/usr/bin/rustc-dispatch
export CARGO_HOME=/sources/cargo-home
export CARGO_TARGET_DIR=/hostbuild/librsvg-target
export CARGO_NET_OFFLINE=true
export HOME=/root
# pkg-config resolution for the gtk-rs -sys build scripts and for configure
# (glib/cairo/pango/gdk-pixbuf/libxml2 are the Fil-C builds installed under
# /usr).  PKG_CONFIG + PKG_CONFIG_ALLOW_CROSS MUST match the autotools cargo
# recipe below (Makefile.am invokes cargo with PKG_CONFIG='$(PKG_CONFIG)'
# PKG_CONFIG_ALLOW_CROSS=1): the -sys crates' build scripts re-run when these
# change, so keeping them identical between this warm-up build and the make
# build keeps the S2 fingerprints warm.
export PKG_CONFIG=/usr/bin/pkg-config
export PKG_CONFIG_ALLOW_CROSS=1
export PKG_CONFIG_PATH=/usr/lib/pkgconfig:/usr/share/pkgconfig

# --- S2 gate: warm re-check of the staticlib ---------------------------------------
# Must fingerprint-match the S2 build (same cargo argv, same env, same
# physical target dir) and finish in seconds; if cargo decides to rebuild
# everything (~16 min) the build still succeeds -- the S3 report notes it.
time cargo build --offline --locked --target x86_64-unknown-linux-gnu \
     --release --package librsvg-c

test -e $CARGO_TARGET_DIR/x86_64-unknown-linux-gnu/release/liblibrsvg_c.a
test -e $CARGO_TARGET_DIR/x86_64-unknown-linux-gnu/release/liblibrsvg_c.rlib
ls -la $CARGO_TARGET_DIR/x86_64-unknown-linux-gnu/release/liblibrsvg_c.*

echo "S2 librsvg cargo gate: OK"

# --- S3: the autotools build --------------------------------------------------------
# CC: cc/gcc -> clang-20 -> the Fil-C clang in this chroot, so the C glue,
# the libtool .so link, g-ir-scanner's dump program, and the tests-c binaries
# are all Fil-ABI and consistent with the staticlib's pizlonated_rsvg_*
# symbols.  RST2MAN is absent (warning only, no man page).  gi-docgen is
# absent (auto -> docs skipped).  vala is absent (auto -> vapi skipped).
CC="gcc" CFLAGS="-g -O2" ./configure \
    --prefix=/usr \
    --enable-introspection

time make

time make install

# --- S3 gates ------------------------------------------------------------------------

echo "=== gate: librsvg-2.so installed ==="
ls -la /usr/lib/librsvg-2.so*
test -e /usr/lib/librsvg-2.so.2
test -e /usr/lib/librsvg-2.so

echo "=== gate: dynamic exports ==="
# After pizlonation the API symbols are named pizlonated_rsvg_* (the
# upstream plain rsvg_* names no longer exist in the dynamic table -- that
# is exactly why the -export-symbols-regex patch was needed).  The public
# C API is 53 functions in librsvg 2.58.3; assert the pizlonated set is
# non-empty and print both counts for the report.
echo -n "nm -D plain ' rsvg_' count (expected 0, renamed): "
nm -D /usr/lib/librsvg-2.so.2 | grep -c ' rsvg_' || true
echo -n "nm -D ' pizlonated_rsvg_' count (the C API, expected 53): "
nm -D /usr/lib/librsvg-2.so.2 | grep -c ' pizlonated_rsvg_'
echo -n "nm -D pizlonated Rust-mangled count (workspace internals): "
nm -D /usr/lib/librsvg-2.so.2 | grep -c ' pizlonated__Z'

echo "=== gate: ldd (Fil-C runtime + GNOME libs only, no 'not found') ==="
ldd /usr/lib/librsvg-2.so.2
if ldd /usr/lib/librsvg-2.so.2 2>&1 | grep -q 'not found'; then
    echo "FATAL: undefined/shared-lib resolution failure" >&2
    exit 1
fi

echo "=== gate: GIR + typelib installed ==="
ls -la /usr/share/gir-1.0/Rsvg-2.0.gir /usr/lib/girepository-1.0/Rsvg-2.0.typelib
g-ir-inspect --print-shlib-gir-path librsvg-2 2>/dev/null || true

echo "=== gate: rsvg-convert renders an SVG (Fil-C binary) ==="
file /usr/bin/rsvg-convert
/usr/bin/rsvg-convert --version
/usr/bin/rsvg-convert -o /tmp/s3-rsvg-convert.png \
    /usr/share/icons/hicolor/scalable/apps/emacs.svg
ls -la /tmp/s3-rsvg-convert.png
test "$(head -c 8 /tmp/s3-rsvg-convert.png | od -An -tx1 | tr -d ' \n')" \
    = "89504e470d0a1a0a"
echo "PNG magic OK"

echo "S3 librsvg gate: OK"

# --- S4: the gdk-pixbuf SVG loader ---------------------------------------------------

echo "=== gate: libpixbufloader-svg.so installed ==="
ls -la /usr/lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-svg.so

echo "=== gate: loaders.cache refreshed ==="
# make install already ran gdk-pixbuf-query-loaders via the install-data-hook
# (DESTDIR empty); refresh explicitly to be sure, then check the svg line.
/usr/bin/gdk-pixbuf-query-loaders --update-cache
grep -n 'svg' /usr/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache

echo "S4 cache gate: OK"

# --- C smoke 1: librsvg API ----------------------------------------------------------
# rsvg_handle_new_from_file() on a real SVG -> render to a cairo image
# surface -> dimensions > 0 and non-uniform pixels; also
# rsvg_handle_get_intrinsic_size_in_pixels().  Lives in /sources so it can
# be re-run by hand via build_chroot_late.sh.

cat > /sources/rsvg-smoke.c <<'EOF'
#include <librsvg/rsvg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main (int argc, char **argv)
{
    const char *path = argv[1];
    const char *out = argv[2];
    GError *err = NULL;
    RsvgHandle *handle;
    RsvgDimensionData dim;
    gdouble pxw = 0.0, pxh = 0.0;
    cairo_surface_t *surf;
    cairo_t *cr;
    RsvgRectangle viewport;
    unsigned char *data;
    size_t n, i;
    int distinct = 0;
    unsigned long long last = 0, first = 0;
    gboolean have_first = FALSE;

    handle = rsvg_handle_new_from_file (path, &err);
    if (handle == NULL) {
        fprintf (stderr, "FAIL: rsvg_handle_new_from_file: %s\n",
                 err ? err->message : "?");
        return 1;
    }

    if (rsvg_handle_get_intrinsic_size_in_pixels (handle, &pxw, &pxh)) {
        printf ("intrinsic size in pixels: %gx%g\n", pxw, pxh);
    } else {
        printf ("intrinsic size in pixels: no intrinsic size, using default\n");
        pxw = 100.0; pxh = 100.0;
    }
    /* The legacy dimension API: cross-checks the intrinsic geometry. */
    rsvg_handle_get_dimensions (handle, &dim);
    printf ("dimensions (legacy API): %dx%d em=%g\n",
            dim.width, dim.height, dim.em);

    if (pxw < 1 || pxh < 1 || pxw > 8192 || pxh > 8192) {
        fprintf (stderr, "FAIL: absurd intrinsic pixel size %gx%g\n", pxw, pxh);
        return 1;
    }

    surf = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                       (int) pxw, (int) pxh);
    cr = cairo_create (surf);

    viewport.x = 0.0;
    viewport.y = 0.0;
    viewport.width = pxw;
    viewport.height = pxh;

    if (!rsvg_handle_render_document (handle, cr, &viewport, &err)) {
        fprintf (stderr, "FAIL: rsvg_handle_render_document: %s\n",
                 err ? err->message : "?");
        return 1;
    }
    cairo_surface_flush (surf);

    printf ("surface: %dx%d\n", (int) pxw, (int) pxh);
    if (pxw <= 0 || pxh <= 0) {
        fprintf (stderr, "FAIL: zero surface dimensions\n");
        return 1;
    }

    data = cairo_image_surface_get_data (surf);
    n = (size_t) (int) pxw * (size_t) (int) pxh * 4;
    for (i = 0; i + 4 <= n; i += 4) {
        unsigned long long px = ((unsigned long long) data[i] << 24)
            | ((unsigned long long) data[i + 1] << 16)
            | ((unsigned long long) data[i + 2] << 8)
            | (unsigned long long) data[i + 3];
        if (!have_first) {
            first = px;
            have_first = TRUE;
        } else if (px != last || px != first) {
            distinct = 1;
        }
        last = px;
    }
    if (!have_first) {
        fprintf (stderr, "FAIL: no pixels\n");
        return 1;
    }
    if (!distinct) {
        fprintf (stderr, "FAIL: rendered pixels are uniform\n");
        return 1;
    }
    printf ("pixels: non-uniform OK\n");

    cairo_destroy (cr);
    if (cairo_surface_write_to_png (surf, out) != CAIRO_STATUS_SUCCESS) {
        fprintf (stderr, "FAIL: write_to_png\n");
        return 1;
    }
    printf ("smoke: wrote %s\n", out);

    g_object_unref (handle);
    cairo_surface_destroy (surf);
    printf ("rsvg smoke: SUCCESS\n");
    return 0;
}
EOF

cc -o /sources/rsvg-smoke /sources/rsvg-smoke.c \
    $(pkg-config --cflags --libs librsvg-2.0)

/sources/rsvg-smoke /usr/share/icons/hicolor/scalable/apps/emacs.svg \
    /tmp/s3-smoke.png
head -c 8 /tmp/s3-smoke.png | od -An -tx1

# --- C smoke 2: gdk-pixbuf end-to-end (S4) --------------------------------------------
# gdk_pixbuf_new_from_file() on a normal SVG and on a SYMBOLIC SVG
# (the GNOME case; recoloring happens at the GTK layer, the loader must
# still produce a pixbuf), then gdk_pixbuf_save to PNG.

cat > /sources/pixbuf-smoke.c <<'EOF'
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdio.h>
#include <stdlib.h>

static int
try (const char *path, const char *out)
{
    GError *err = NULL;
    GdkPixbuf *pb;
    int w, h, rowstride, nchan, i;
    unsigned char *d;
    int nonzero = 0;

    pb = gdk_pixbuf_new_from_file (path, &err);
    if (pb == NULL) {
        fprintf (stderr, "FAIL: %s: %s\n", path,
                 err ? err->message : "?");
        return 1;
    }
    w = gdk_pixbuf_get_width (pb);
    h = gdk_pixbuf_get_height (pb);
    nchan = gdk_pixbuf_get_n_channels (pb);
    rowstride = gdk_pixbuf_get_rowstride (pb);
    d = gdk_pixbuf_get_pixels (pb);
    for (i = 0; i < h; i++) {
        int j;
        for (j = 0; j < w * nchan; j++)
            if (d[i * rowstride + j] != 0)
                nonzero++;
    }
    printf ("%s: %dx%d, %d channels, %d nonzero bytes\n", path, w, h, nchan,
            nonzero);
    if (w < 1 || h < 1 || w > 8192 || h > 8192) {
        fprintf (stderr, "FAIL: absurd pixbuf dimensions\n");
        return 1;
    }
    if (!gdk_pixbuf_save (pb, out, "png", &err, NULL)) {
        fprintf (stderr, "FAIL: gdk_pixbuf_save %s: %s\n", out,
                 err ? err->message : "?");
        return 1;
    }
    printf ("wrote %s\n", out);
    g_object_unref (pb);
    return 0;
}

int
main (int argc, char **argv)
{
    if (try (argv[1], argv[2]))
        return 1;
    if (try (argv[3], argv[4]))
        return 1;
    printf ("gdk-pixbuf smoke: SUCCESS\n");
    return 0;
}
EOF

cc -o /sources/pixbuf-smoke /sources/pixbuf-smoke.c \
    $(pkg-config --cflags --libs gdk-pixbuf-2.0)

/sources/pixbuf-smoke \
    /usr/share/icons/hicolor/scalable/apps/emacs.svg /tmp/s4-pixbuf.png \
    /usr/share/icons/Adwaita/symbolic/actions/address-book-new-symbolic.svg \
    /tmp/s4-pixbuf-symbolic.png
head -c 8 /tmp/s4-pixbuf.png | od -An -tx1
head -c 8 /tmp/s4-pixbuf-symbolic.png | od -An -tx1

# --- no filc panics anywhere ----------------------------------------------------------
for f in /tmp/s3-rsvg-convert.png /tmp/s3-smoke.png /tmp/s4-pixbuf.png /tmp/s4-pixbuf-symbolic.png; do
    test -s $f
done

echo "S3+S4 librsvg: ALL GATES GREEN"