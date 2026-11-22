#!/bin/bash

set -e
set -x


# The Fil-C rustc (exposed as /usr/bin/rustc by build_postlc9_chroot_project_rust.sh)
# builds every TARGET crate (cargo is always invoked with an explicit
# --target x86_64-unknown-linux-gnu, which happens to be the Fil-C rustc's own
# host triple, so it accepts --target and uses its patched target spec whose
# linker is filcc).  Every HOST unit -- build scripts, proc-macros, and the
# rlibs of the proc-macro subtree (syn/quote/proc-macro2/...) -- compiles
# WITHOUT --target and is dispatched to the upstream dist rustc 1.90.0 at
# /usr/lib/rust/yolo.  Rationale:
#
# - proc-macro crates compiled by the upstream rustc are accepted by the
#   Fil-C rustc thanks to the metadata-version normalization (E0514 fix);
#   both rustcs report "1.90.0 (1159e78c4 2025-09-14)" after the strip.
# - the upstream rustc's std is glibc-ABI.  Host units must therefore link
#   against the yolo glibc, not the Fil-C libc: this script completes the
#   /yolo link-time sysroot (see below) and the dispatch shim passes
#   -C linker=/yolo/bin/gcc -C link-arg=-fno-use-linker-plugin to every
#   upstream-rustc invocation.  The resulting host binaries/dylibs are
#   yolo-linked (interpreter /yolo/lib/ld-linux-x86-64.so.2 -- via the
#   /usr/lib/ld-linux-x86-64.so.2 symlink -- and NEEDEDs from /yolo/lib) and
#   run in the chroot, like the yolo rustc/cargo themselves.
# - mixing glibc-ABI Rust objects with the Fil-C CRT would be wrong even when
#   it links: the upstream std's glibc assumptions (pthread layouts etc.) do
#   not hold on the Fil-C user glibc.
#
# This script is idempotent; it also gates the whole dispatch with three
# smoke tests (build script, proc-macro interplay, vendored registry crate).

# --- (a) complete the /yolo link-time sysroot ----------------------------------
#
# The yolo gcc (/yolo/bin/gcc, the LFS 14.2 toolchain that built the kernel)
# can link yolo binaries in this chroot once it can find:
#
# 1. libc by linker name: glibc installs a libc.so LINKER SCRIPT
#    (GROUP(libc.so.6 libc_nonshared.a AS_NEEDED(ld-linux...))); the
#    standalone /yolo/lib only shipped the DSOs.  Without the script, -lc
#    would resolve to /usr/lib/libc.so -- the FIL-C libc script -- and pull
#    the wrong (Fil-ABI) libc into a glibc-ABI link.  /yolo/lib precedes
#    /usr/lib in the gcc's library search order, so the script wins.
# 2. libm by linker name (same story).
# 3. libgcc_s.so.1: the yolo gcc only ships static libgcc, and rustc links
#    -lgcc_s for unwinding (both executables and proc-macro dylibs).  The
#    dist tarball already carries a glibc-ABI libgcc_s.so.1 for the yolo
#    rustc/cargo; a copy in /yolo/lib serves every yolo-linked binary (the
#    yolo ld's only default search directory is /yolo/lib).
#
# The linker scripts reference /usr/lib/libyoloc_nonshared.a, which
# build_lc.sh installed (the yolo glibc's libc_nonshared.a, renamed for the
# sandwich layout; an archive has no NEEDED wiring, so sharing it is safe).
# /usr/lib/ld-linux-x86-64.so.2 -> /yolo/lib/ld-linux-x86-64.so.2 completes
# the previously dangling /lib64/ld-linux-x86-64.so.2 chain: the yolo gcc's
# spec bakes /lib64/ld-linux-x86-64.so.2 as the interpreter, and this way the
# interpreter is the yolo ld (whose compiled-in default search directory,
# /yolo/lib, resolves the NEEDEDs).  Fil-C binaries are unaffected: the Fil-C
# clang bakes /lib/ld-fil1-x86_64.so.

cat > /yolo/lib/libc.so <<'EOF'
OUTPUT_FORMAT(elf64-x86-64)
GROUP ( /yolo/lib/libc.so.6 /usr/lib/libyoloc_nonshared.a  AS_NEEDED ( /yolo/lib/ld-linux-x86-64.so.2 ) )
EOF
cat > /yolo/lib/libm.so <<'EOF'
OUTPUT_FORMAT(elf64-x86-64)
GROUP ( /yolo/lib/libm.so.6 )
EOF
cp -a /usr/lib/rust/yolo/lib/libgcc_s.so.1 /yolo/lib/libgcc_s.so.1
ln -sf libgcc_s.so.1 /yolo/lib/libgcc_s.so
ln -sf /yolo/lib/ld-linux-x86-64.so.2 /usr/lib/ld-linux-x86-64.so.2

# --- (b) expose the upstream cargo + rustc, write the dispatch shim ------------
#
# /usr/bin/cargo is the upstream cargo: every pizlix cargo build goes through
# RUSTC_WRAPPER=/usr/bin/rustc-dispatch, which sends --target units to the
# Fil-C rustc and everything else to the upstream rustc.  Plain `rustc`
# (outside cargo) stays the Fil-C rustc.

ln -sf /usr/lib/rust/yolo/bin/cargo /usr/bin/cargo
ln -sf /usr/lib/rust/yolo/bin/rustc /usr/bin/rustc-yolo
ln -sf /usr/lib/rust/yolo/bin/cargo /usr/bin/cargo-yolo

cat > /usr/bin/rustc-dispatch <<'EOF'
#!/bin/bash
#
# Two-compiler dispatch shim for cargo (set RUSTC_WRAPPER=/usr/bin/rustc-dispatch).
#
# cargo invokes: $RUSTC_WRAPPER <rustc-path> <args...>.  Dispatch rule:
#
#   argv contains --target  -> the Fil-C rustc (/usr/lib/rust/filc/bin/rustc,
#                              the real /usr/bin/rustc): target crates.  The
#                              Fil-C rustc needs no linker help: its patched
#                              target spec sets linker=filcc (PATH).
#   otherwise               -> the upstream yolo rustc 1.90.0: build scripts,
#                              proc-macros, and the host rlibs.  The shim
#                              appends -C linker=/yolo/bin/gcc and
#                              -C link-arg=-fno-use-linker-plugin so the
#                              glibc-ABI host artifacts yolo-link (see
#                              build_postlc9_chroot_project_rust_dispatch.sh):
#                              - the Fil-C clang's CRT must never be mixed
#                                with upstream-std objects, and
#                              - the Fil-C /usr/bin/ld cannot dlopen the yolo
#                                gcc's glibc-ABI LTO plugin, so the plugin is
#                              disabled (rustc never uses gcc LTO at link).
#                              Flags are appended AFTER cargo's args because
#                              rustc resolves scalar -C options last-wins and
#                              the dispatch's linker choice is non-negotiable.
#
# After a successful upstream compile of a bin crate (build scripts), the
# output is patchelfed defensively (interpreter + rpath); with the linker
# flags above this is a no-op, but it keeps build scripts runnable even if a
# future cargo passes its own -C linker.
#
# Debug: set RUSTC_DISPATCH_LOG=<path> to append every dispatched argv to it.

FILC_RUSTC=/usr/lib/rust/filc/bin/rustc
Y_RUSTC=/usr/lib/rust/yolo/bin/rustc
Y_GCC=/yolo/bin/gcc
YOLO_LD=/yolo/lib/ld-linux-x86-64.so.2

args=("$@")
if [ "x$(basename "${args[0]}")" = "xrustc" ]; then
    args=("${args[@]:1}")
fi

has_target=0
crate_type=bin
out=
prev=
for a in "${args[@]}"; do
    case "$a" in
        --target|--target=*) has_target=1 ;;
        --crate-type) ;;
        --crate-type=*) crate_type="${a#--crate-type=}" ;;
        -o) ;;
        -o*) out="${a#-o}" ;;
    esac
    if [ "x$prev" = "x--crate-type" ]; then crate_type="$a"; fi
    if [ "x$prev" = "x-o" ]; then out="$a"; fi
    prev="$a"
done

if [ -n "${RUSTC_DISPATCH_LOG:-}" ]; then
    printf '%s\n' "target=$has_target type=$crate_type out=$out :: ${args[*]}" >> "$RUSTC_DISPATCH_LOG"
fi

if [ "$has_target" = 1 ]; then
    exec "$FILC_RUSTC" "${args[@]}"
fi

"$Y_RUSTC" "${args[@]}" -C linker=$Y_GCC -C link-arg=-fno-use-linker-plugin || exit $?

if [ "x$crate_type" = "xbin" ] && [ -n "$out" ] && [ -x "$out" ]; then
    patchelf --set-interpreter $YOLO_LD "$out" 2>/dev/null || true
    patchelf --set-rpath /yolo/lib "$out" 2>/dev/null || true
fi
exit 0
EOF
chmod 755 /usr/bin/rustc-dispatch

hash -r
cargo --version
/usr/bin/rustc-yolo --version
/usr/bin/cargo-yolo --version
test -x /usr/bin/rustc-dispatch

echo "rustc-dispatch shim installed"

# --- (c) gates: the dispatch under real cargo -----------------------------------
#
# A. proc-macro interplay: cargo --target builds the local proc-macro crate
#    (host unit -> upstream rustc) and the bin that uses it (target unit ->
#    Fil-C rustc, which dlopens the yolo-linked proc-macro dylib).
# B. build.rs: cargo compiles and RUNS the build script (a yolo-linked host
#    binary), and its cargo: directives reach the Fil-C target compile.
# C. vendored registry crates: thiserror (whose -impl crate is a proc-macro)
#    + unicode-bidi from /sources/vendor-librsvg, offline.

export RUSTC_WRAPPER=/usr/bin/rustc-dispatch
export CARGO_HOME=/sources/cargo-home
export CARGO_TARGET_DIR=/hostbuild/dispatch-smoke-target
export CARGO_NET_OFFLINE=true
export HOME=/root

# The vendored dependencies (the shadow-tarball pattern, like the rust
# toolchains: not a projeny product, copied into /sources by the driver).
# They live OUTSIDE the librsvg checkout on purpose: a projeny checkout is
# managed (setup restores pristine+patch), so vendor/ must never live inside
# it.  The vendored-sources replacement is configured per-build through
# CARGO_HOME (absolute directory path), which also reaches the autotools
# build's cargo invocations via the environment.

rm -rf /sources/vendor-librsvg
tar -xf /sources/vendor-librsvg.tar.gz -C /sources
# The tarball's root directory is `vendor/` (see build_postlc9_sub1_packaging.sh:
# tar -czf $VENDOR_TARBALL -C rust-build/.librsvg-vendor vendor), while every
# consumer -- the vendored-sources config below, build_postlc9_chroot_project_librsvg.sh,
# and the packaging docs -- names the tree /sources/vendor-librsvg.  Move the
# extracted tree where the config points.
mv /sources/vendor /sources/vendor-librsvg
mkdir -p $CARGO_HOME
cat > $CARGO_HOME/config.toml <<EOF
[source.crates-io]
replace-with = "vendored-sources"

[source.vendored-sources]
directory = "/sources/vendor-librsvg"
EOF

rm -rf /tmp/dispatch-smoke
mkdir -p /tmp/dispatch-smoke/pm/src /tmp/dispatch-smoke/top/src /tmp/dispatch-smoke/bs/src /tmp/dispatch-smoke/reg/src

cat > /tmp/dispatch-smoke/pm/Cargo.toml <<'EOF'
[package]
name = "pm"
version = "0.1.0"
edition = "2021"

[lib]
proc-macro = true
EOF
cat > /tmp/dispatch-smoke/pm/src/lib.rs <<'EOF'
use proc_macro::TokenStream;

#[proc_macro]
pub fn make_answer(_item: TokenStream) -> TokenStream {
    "fn answer() -> u32 { 42 }".parse().unwrap()
}
EOF

cat > /tmp/dispatch-smoke/top/Cargo.toml <<'EOF'
[package]
name = "top"
version = "0.1.0"
edition = "2021"

[dependencies]
pm = { path = "../pm" }
EOF
cat > /tmp/dispatch-smoke/top/src/main.rs <<'EOF'
pm::make_answer!();

fn main() {
    println!("answer = {}", answer());
}
EOF

cd /tmp/dispatch-smoke/top
cargo build --offline --target x86_64-unknown-linux-gnu --release
TOP_OUT=$CARGO_TARGET_DIR/x86_64-unknown-linux-gnu/release/top
test "x$($TOP_OUT)" = "xanswer = 42"
readelf -l $TOP_OUT | grep -q "/lib/ld-fil1-x86_64.so"  # target unit: Fil-C binary
PM_SO=$(ls $CARGO_TARGET_DIR/release/deps/libpm-*.so | head -1)
readelf -d $PM_SO | grep -q "libc.so.6"                 # host unit: yolo-linked dylib
echo "gate A (proc-macro interplay through cargo): OK"

cat > /tmp/dispatch-smoke/bs/Cargo.toml <<'EOF'
[package]
name = "bs"
version = "0.1.0"
edition = "2021"
build = "build.rs"
EOF
cat > /tmp/dispatch-smoke/bs/build.rs <<'EOF'
fn main() {
    println!("cargo:rustc-cfg=feeling_lucky");
    eprintln!("build-script-ran");
}
EOF
cat > /tmp/dispatch-smoke/bs/src/main.rs <<'EOF'
#[cfg(feeling_lucky)]
fn message() -> &'static str {
    "lucky"
}

#[cfg(not(feeling_lucky))]
fn message() -> &'static str {
    "unlucky"
}

fn main() {
    println!("bs = {}", message());
}
EOF

cd /tmp/dispatch-smoke/bs
# -vv: cargo captures build-script stderr unless very-verbose
cargo build --offline --target x86_64-unknown-linux-gnu --release -vv > /tmp/dispatch-smoke/bs-out.txt 2>&1
grep -q "build-script-ran" /tmp/dispatch-smoke/bs-out.txt
BS_OUT=$CARGO_TARGET_DIR/x86_64-unknown-linux-gnu/release/bs
test "x$($BS_OUT)" = "xbs = lucky"
echo "gate B (build.rs runs and its directives land): OK"

cat > /tmp/dispatch-smoke/reg/Cargo.toml <<'EOF'
[package]
name = "reg"
version = "0.1.0"
edition = "2021"

[dependencies]
thiserror = "1"
unicode-bidi = "0.3"
EOF
cat > /tmp/dispatch-smoke/reg/src/main.rs <<'EOF'
#[derive(Debug, thiserror::Error)]
enum Err {
    #[error("broke: {0}")]
    Broke(String),
}

fn main() {
    let bidi = unicode_bidi::BidiInfo::new("hello א", None);
    println!(
        "reg: {} paragraphs / {}",
        bidi.paragraphs.len(),
        Err::Broke("x".to_string())
    );
}
EOF

cd /tmp/dispatch-smoke/reg
cargo build --offline --target x86_64-unknown-linux-gnu --release
REG_OUT=$CARGO_TARGET_DIR/x86_64-unknown-linux-gnu/release/reg
REG_OUT_TEXT=$($REG_OUT)
case "$REG_OUT_TEXT" in
    "reg: 1 paragraphs / broke: x") ;;
    *) echo "unexpected reg output: $REG_OUT_TEXT" >&2; exit 1 ;;
esac
echo "gate C (vendored registry crates + thiserror proc-macro): OK"

rm -rf /tmp/dispatch-smoke

echo "rustc-dispatch smoke gates: ALL OK"