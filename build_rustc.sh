#!/bin/bash
#
# Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

# Build Fil-C's rustc.
#
# This extracts the patched Rust 1.90.0 sources from the projeny checkout in
# projects/rust into rust-build/rust, swaps Rust's bundled LLVM for the Fil-C
# LLVM (both are LLVM 20.1.8, so rustc's LLVM C++ wrapper compiles against it
# unchanged modulo our Fil-C pipeline patches), writes a bootstrap
# config.toml, and then runs `x.py build --stage 1`.  The resulting stage 1
# rustc pizlonates every module it compiles (the Fil-Pizlonator runs in every
# pipeline, at every optimization level) and the x86_64-unknown-linux-gnu
# target spec makes `filcc` the linker, so what it produces links as Fil-C.
#
# ALL Fil-C changes to the Rust sources live in the projeny patch
# (projects/rust.projeny); this script applies no source tweaks of its own.
# Instead it verifies that every expected Fil-C change is present in the
# extracted tree (the check_grep calls below) and fails fast with a clear
# error if the patch is missing one.  To change the sources: edit
# projects/rust/... then run `filc/projeny commit projects/rust.projeny`.
#
# The built rustc also self-identifies as Fil-C, clang-style: `rustc --version`
# appends " (Fil-C <version> <repo-url> <hash>)".  This script composes that
# string (version number read from clang's FILC_VERSION #define, the single
# source of truth for the Fil-C version -- see versioning-checklist.md -- plus
# the fil-c git origin URL and HEAD hash) and exports it as $FILC_VERSION; an
# option_env!("FILC_VERSION") in compiler/rustc_driver_impl/src/lib.rs (in the
# projeny patch) bakes it into the compiler, mirroring
# clang/lib/Basic/Version.cpp.
#
# The script is idempotent: if rust-build/rust already exists, extraction is
# skipped and bootstrap resumes incrementally.
#
# Usage: ./build_rustc.sh
#
# The result is rust-build/rustc (symlink, also as rust-build/filrustc).
# NOTE: $FILC_ROOT/build/bin must be on PATH whenever the built rustc is used,
# because linking goes through Fil-C's `filcc`.
set -e
set -o pipefail
set -x

cd -- "$(dirname -- "$0")"

. libpas/common.sh

FILC_ROOT=`pwd`
RUST_BUILD=$FILC_ROOT/rust-build
RUST_SRC=$RUST_BUILD/rust

# Extract the Rust sources from the projeny checkout, unless we already have
# rust-build/rust (then bootstrap just resumes incrementally).
if [ ! -d $RUST_SRC ]; then
    mkdir -p $RUST_BUILD
    (
        cd projects
        rm -rf rust/extracted-source
        ../filc/projeny extract rust.projeny rust/extracted-source
        mkdir -p $RUST_BUILD
        rsync -a --delete rust/extracted-source/ $RUST_SRC/
        rm -rf rust/extracted-source
    )
fi

# Swap in the Fil-C LLVM for the LLVM that Rust bundles.  The essential
# directories are llvm/ (the LLVM sources), cmake/ (LLVM's common CMake
# modules incl. LLVMVersion.cmake), libunwind/ (bootstrap builds it in-tree
# because rust.llvm-libunwind = "in-tree"), runtimes/, and third-party/.
# We keep a stamp file recording the Fil-C git HEAD so that we know the LLVM
# in there is ours and don't redo this on re-runs.
if [ ! -d $RUST_SRC/src/llvm-project/llvm ] || [ ! -f $RUST_SRC/src/llvm-project/.filc-llvm-stamp ]; then
    rm -rf $RUST_SRC/src/llvm-project
    mkdir -p $RUST_SRC/src/llvm-project
    cp -a llvm cmake third-party runtimes libunwind $RUST_SRC/src/llvm-project/
    git rev-parse HEAD > $RUST_SRC/src/llvm-project/.filc-llvm-stamp
fi

# Verify that the extracted tree contains every Fil-C change.  All Fil-C
# changes to the Rust sources live in the projeny patch (projects/rust.projeny)
# -- this script applies no tweaks of its own, so if one of these greps fails
# the patch is missing a change and the build would produce a broken or
# non-pizlonating compiler.  Fail fast with a clear error in that case.
check_grep() {
    local marker="$1"
    local file="$2"
    if ! grep -qF -- "$marker" "$RUST_SRC/$file"; then
        echo "ERROR: projeny patch missing filc change: '$marker' not found in" >&2
        echo "       $file" >&2
        echo "       projects/rust.projeny must carry ALL Fil-C changes to the Rust" >&2
        echo "       sources; edit projects/rust/... and re-run" >&2
        echo "       'filc/projeny commit projects/rust.projeny'." >&2
        exit 1
    fi
}

# compiler/rustc_llvm/llvm-wrapper/PassWrapper.cpp: the Fil-C pipeline
# (KillUB -> [Fil-C inline pipeline, disabled by default] -> coro lowering ->
# FilPizlonator on every path, OptimizerLast DeleteRedundantPollchecks) and
# the "after Fil-C" DataLayout setup.
check_grep "FilPizlonatorPass()" compiler/rustc_llvm/llvm-wrapper/PassWrapper.cpp
check_grep "setDataLayoutAfterFilC" compiler/rustc_llvm/llvm-wrapper/PassWrapper.cpp
check_grep "DeleteRedundantPollchecksPass()" compiler/rustc_llvm/llvm-wrapper/PassWrapper.cpp

# compiler/rustc_target/src/spec/targets/x86_64_unknown_linux_gnu.rs: the
# "before Fil-C" data layout (-ni:0), filcc as the linker, -normalcrt, no
# stack probes, no sanitizers, and panic=abort (unwinding cannot work under
# Fil-C).
check_grep "e-m:e-ni:0-p270:32:32" compiler/rustc_target/src/spec/targets/x86_64_unknown_linux_gnu.rs
check_grep 'base.linker = Some("filcc".into());' compiler/rustc_target/src/spec/targets/x86_64_unknown_linux_gnu.rs
check_grep '"-m64", "-normalcrt"' compiler/rustc_target/src/spec/targets/x86_64_unknown_linux_gnu.rs
check_grep "StackProbeType::None" compiler/rustc_target/src/spec/targets/x86_64_unknown_linux_gnu.rs
check_grep "SanitizerSet::empty()" compiler/rustc_target/src/spec/targets/x86_64_unknown_linux_gnu.rs
check_grep "base.panic_strategy = PanicStrategy::Abort;" compiler/rustc_target/src/spec/targets/x86_64_unknown_linux_gnu.rs

# library/compiler-builtins/compiler-builtins/src/probestack.rs: the
# naked-asm stack probe helper must be compiled out exactly for the Fil-C
# x86_64-unknown-linux-gnu target (the target spec sets StackProbeType::None).
check_grep 'all(target_arch = "x86_64", target_os = "linux", target_env = "gnu")' library/compiler-builtins/compiler-builtins/src/probestack.rs

# compiler/rustc_codegen_llvm/src/context.rs: accept the target spec's
# before-FilC data layout ("-ni:0") in the MismatchedDataLayout validation.
check_grep 'replace("-ni:0", "") != llvm_data_layout' compiler/rustc_codegen_llvm/src/context.rs

# library/compiler-builtins/libm/src/math/arch/x86/detect.rs and
# library/std_detect/src/detect/os/x86.rs: Fil-C's rustc emits `asm!` as
# Intel-dialect inline asm, which the Fil-Pizlonator rejects, and rustc
# refuses to name `ebx` as an inline-asm operand/clobber on x86_64, so
# `cpuid` can neither be executed nor have its EBX output declared the way
# the pizlonator requires.  std_detect therefore detects features by
# parsing /proc/cpuinfo (the kernel merges CPUID + XCR0 + CR4.OSXSAVE into
# its `flags` line) and libm reports no CPU features (its software
# fallbacks are used).
check_grep "/proc/cpuinfo" library/std_detect/src/detect/os/x86.rs
check_grep "FLAG_TO_FEATURE" library/std_detect/src/detect/os/x86.rs
check_grep "Flags::empty()" library/compiler-builtins/libm/src/math/arch/x86/detect.rs

# compiler/rustc_codegen_ssa/src/back/write.rs: allocator shim modules must go
# through the pipeline (they used to get opt_level None, skipping
# LLVMRustOptimize, so they were never pizlonated).
check_grep "opt_level_and_size = Some(sess.opts.optimize);" compiler/rustc_codegen_ssa/src/back/write.rs

# compiler/rustc_codegen_ssa/src/back/linker.rs: the auto-generated LD version
# script must export the pizlonated symbol names -- the pizlonatedFI*/
# pizlonatedFIP* wildcards and the `pizlonated*` wildcard that also covers the
# pizlonated_<name> getters (cross-DSO callers reference those).
check_grep 'pizlonatedFI*_{sym};' compiler/rustc_codegen_ssa/src/back/linker.rs
check_grep 'pizlonated*;' compiler/rustc_codegen_ssa/src/back/linker.rs

# compiler/rustc_codegen_ssa/src/back/link.rs: the synthetic symbols.o must
# reference pizlonated getter names (the original names no longer exist after
# pizlonation, which would leave phantom undefined imports in every dylib).
check_grep 'format!("pizlonated_{sym}")' compiler/rustc_codegen_ssa/src/back/link.rs

# src/bootstrap/src/core/build_steps/llvm.rs: build libunwind with filcc/fil++.
check_grep 'PathBuf::from("filcc")' src/bootstrap/src/core/build_steps/llvm.rs

# library/unwind: link nothing for unwinding on linux-gnu (filcc links the
# pizfix runtime, whose libpizlo exports the full _Unwind_* API under the
# pizlonator's renamed names), and stub the _Unwind_* entry points that
# libpizlo lacks.
check_grep "pizlonated__Unwind_RaiseException" library/unwind/src/lib.rs
check_grep "derive _Unwind_GetIPInfo from _Unwind_GetIP" library/unwind/src/libunwind.rs
check_grep '_Unwind_FindEnclosingFunction` is not provided by libpizlo' library/unwind/src/libunwind.rs

# ...and the linux-gnu/uclibc unwinding block in library/unwind/src/lib.rs
# must actually link NOTHING: no `#[link(...)]` attribute may appear between
# its `cfg(...)` and its `unsafe extern "C" {}` body.  (The greps above only
# prove that the explanatory comment is present; `#[link(name = "gcc_s",
# ...)]` legitimately appears in this file for other OSes (musl, redox,
# netbsd, freebsd, ...), so the block-scoped assertion below is what
# actually verifies that upstream's `#[link(name = "gcc_s", cfg(not(
# target_feature = "crt-static")))]` removal survived.)
UNWIND_BLOCK=`awk '/target_os = "linux",/{f=1} f{print} f && /unsafe extern "C" \{\}/{exit}' "$RUST_SRC/library/unwind/src/lib.rs"`
if ! echo "$UNWIND_BLOCK" | grep -q 'any(target_env = "gnu", target_env = "uclibc")'; then
    echo "ERROR: projeny patch missing filc change: the linux-gnu/uclibc" >&2
    echo "       unwinding cfg block was not found in library/unwind/src/lib.rs" >&2
    echo "       projects/rust.projeny must carry ALL Fil-C changes to the Rust" >&2
    echo "       sources; edit projects/rust/... and re-run" >&2
    echo "       'filc/projeny commit projects/rust.projeny'." >&2
    exit 1
fi
if echo "$UNWIND_BLOCK" | grep -q '#\[link('; then
    echo "ERROR: projeny patch missing filc change: the linux-gnu/uclibc" >&2
    echo "       unwinding block in library/unwind/src/lib.rs contains a" >&2
    echo "       '#[link(...)]' attribute; on linux-gnu the unwind crate must" >&2
    echo "       link nothing (filcc links the pizfix runtime, not libgcc_s)." >&2
    echo "       projects/rust.projeny must carry ALL Fil-C changes to the Rust" >&2
    echo "       sources; edit projects/rust/... and re-run" >&2
    echo "       'filc/projeny commit projects/rust.projeny'." >&2
    exit 1
fi

# library/backtrace: enumerate loaded libraries from /proc/self/maps instead
# of dl_iterate_phdr (absent from pizfix), and stub _Unwind_FindEnclosingFunction
# (also absent from libpizlo).
check_grep "/proc/self/maps" library/backtrace/src/symbolize/gimli/libs_dl_iterate_phdr.rs
check_grep "pub(super) fn is_executable" library/backtrace/src/symbolize/gimli/parse_running_mmaps_unix.rs
check_grep "libpizlo (the Fil-C runtime) does not provide" library/backtrace/src/backtrace/libunwind.rs

# Vendored libc: pthread_t must be a pointer (pizfix headers define
# `typedef void *pthread_t`; an integer typedef corrupts the thread handle
# across FFI under the Fil-C ABI), with the vendored checksum updated.
check_grep "pub type pthread_t = *mut crate::c_void;" vendor/libc-0.2.174/src/unix/linux_like/linux/gnu/mod.rs
LIBC_SHA=`python3 -c "import hashlib,sys; print(hashlib.sha256(open(sys.argv[1],'rb').read()).hexdigest())" "$RUST_SRC/vendor/libc-0.2.174/src/unix/linux_like/linux/gnu/mod.rs"`
check_grep "$LIBC_SHA" vendor/libc-0.2.174/.cargo-checksum.json

# library/std/src/sys/pal/unix/stack_overflow.rs: linux-gnu uses the no-op imp
# module (the SIGSEGV guard-page machinery cannot work under Fil-C).
check_grep 'not(all(target_os = "linux", target_env = "gnu"))' library/std/src/sys/pal/unix/stack_overflow.rs

# compiler/rustc_session/src/session.rs: Session::lto() defaults to Lto::No
# (ThinLTO is incompatible with the Fil-Pizlonator's symbol renames).
check_grep 'upstream would enable "auto" ThinLTO' compiler/rustc_session/src/session.rs

# compiler/rustc_driver_impl/src/lib.rs: `rustc --version` (and the first line
# of `rustc -vV`) appends " (Fil-C <version> <repo-url> <hash>)" read from the
# FILC_VERSION environment variable with option_env!, mirroring
# clang/lib/Basic/Version.cpp (which bakes the same parenthetical into
# "clang version ...").  The variable itself is composed and exported below.
check_grep 'option_env!("FILC_VERSION")' compiler/rustc_driver_impl/src/lib.rs

echo "All Fil-C source changes verified in the extracted tree."

# Compose the Fil-C version string that the compiler bakes into its
# `rustc --version` output via option_env!("FILC_VERSION") in
# compiler/rustc_driver_impl/src/lib.rs (checked above; the projeny patch
# carries the code).  It mirrors what clang bakes into "clang version ..."
# (clang/lib/Basic/Version.cpp): "(Fil-C <version> <repo-url> <hash>)".  The
# Fil-C version NUMBER is read from clang's FILC_VERSION #define -- the single
# source of truth for the Fil-C version (see versioning-checklist.md) -- and
# the repo URL and HEAD hash identify the fil-c checkout this rustc was built
# from, just like clang's VCSVersion.inc does.
FILC_VER=`sed -n 's/^#define FILC_VERSION "\(.*\)"$/\1/p' $FILC_ROOT/clang/lib/Basic/Version.cpp`
if [ -z "$FILC_VER" ]; then
    echo "ERROR: could not find '#define FILC_VERSION \"...\"' in" >&2
    echo "       clang/lib/Basic/Version.cpp; cannot compose the Fil-C rustc" >&2
    echo "       version string.  clang/lib/Basic/Version.cpp defines the Fil-C" >&2
    echo "       version number for the whole repo (see versioning-checklist.md)." >&2
    exit 1
fi
FILC_URL=`git remote get-url origin 2>/dev/null || echo git@github.com:pizlonator/fil-c.git`
FILC_HASH=`git rev-parse HEAD 2>/dev/null || echo unknown`
export FILC_VERSION="Fil-C $FILC_VER $FILC_URL $FILC_HASH"

# option_env! is expanded when the rustc_driver_impl crate compiles, and cargo
# does not notice when the *value* of an environment variable changes, so a
# re-run after a Fil-C version bump would otherwise silently keep the stale
# string.  Keep a stamp recording the value the tree was last built with: when
# it differs (or is missing), touch the one source file that reads the
# variable, forcing cargo to recompile rustc_driver_impl and relink rustc, and
# record the new value.  An unchanged value makes this a fast no-op.
FILC_STAMP=$RUST_BUILD/.filc-version-stamp
if [ ! -f $FILC_STAMP ] || [ "`cat $FILC_STAMP`" != "$FILC_VERSION" ]; then
    touch $RUST_SRC/compiler/rustc_driver_impl/src/lib.rs
    echo "$FILC_VERSION" > $FILC_STAMP
fi
echo "Baked-in Fil-C rustc version string: $FILC_VERSION"

# The libunwind step is cached based on output mtime; force a rebuild after
# switching it to the Fil-C compilers (in the projeny patch).
rm -f $RUST_SRC/build/x86_64-unknown-linux-gnu/native/libunwind/libunwind.a

# Write the bootstrap configuration.  This file is fully generated by this
# script, so rewrite it on every run: a stale config.toml (e.g. one written
# by an older revision of this script) would silently drop settings the
# current sources need.
cat > $RUST_SRC/config.toml <<'EOF'
# Bootstrap configuration for building Fil-C's rustc (written by build_rustc.sh).

change-id = 144675

[build]
target = ["x86_64-unknown-linux-gnu"]
extended = false
tools = []
# Use the pure-Rust compiler-builtins (no compiler-rt C sources, which we
# don't vendor); the Rust builtins get pizlonated like everything else.
optimized-compiler-builtins = false

[llvm]
download-ci-llvm = false
optimize = true
assertions = true
ninja = true
targets = "X86"

[rust]
# Fil-C: no in-tree or system libunwind.  pizfix has no libgcc_s, and LLVM
# libunwind cannot be built under Fil-C anyway (Fil-C's assembler rejects its
# assembly files).  Instead, the unwind crate is patched (in the projeny
# patch) to link nothing: filcc already links the pizfix runtime (libpizlo +
# libyolounwind) into every binary, and libpizlo exports the full _Unwind_*
# API under the pizlonator's renamed symbol names.
llvm-libunwind = "no"
lld = false
jemalloc = false
optimize = true
debug = false
# Fil-C: LTO is off for std.  std's ThinLTO post-link pass internalizes the
# pizlonator's `pizlonated_<name>` getter symbols (rustc's export list passed
# to LTO still uses pre-pizlonation names), which breaks separately-compiled
# modules that reference them -- notably the allocator shim module.  Without
# LTO every module keeps its pizlonated entry points and links consistently.
lto = "off"
# NOTE: setting std-features replaces bootstrap's default feature set
# (["panic-unwind"]), so panic-unwind is repeated here.  std_detect_file_io
# gives std_detect's x86 backend (which Fil-C reimplements on top of
# /proc/cpuinfo; see the projeny patch) access to the libc file-IO calls it
# needs; without it, is_x86_feature_detected! would report every feature as
# absent.
std-features = ["panic-unwind", "std_detect_file_io"]
EOF
cat $RUST_SRC/config.toml

# Fil-C's clang/filcc (the target spec links with `filcc`) live in build/bin.
export PATH="$FILC_ROOT/build/bin:$PATH"

cd $RUST_SRC
python3 x.py build --stage 1 2>&1 | tee $RUST_BUILD/build.log

RUSTC_BIN=$RUST_SRC/build/x86_64-unknown-linux-gnu/stage1/bin/rustc
if [ ! -x $RUSTC_BIN ]; then
    echo "ERROR: x.py did not produce $RUSTC_BIN" >&2
    exit 1
fi

ln -sfn $RUSTC_BIN $RUST_BUILD/rustc

# `filrustc` is a symlink to the same rustc: the Fil-C behavior is baked into
# the compiler itself.  ThinLTO would be broken under Fil-C (the pizlonator's
# symbol renames break the ThinLTO summary round-trip, which hollows out
# codegen units), but the compiler's Session::lto() default is patched to
# Lto::No in the projeny patch, so plain compiles -- at every optimization
# level -- never use LTO.  An explicit `-Clto=...` still overrides the default
# and then produces a link error (documented in build_rustc-notes.md).

ln -sfn rustc $RUST_BUILD/filrustc

echo "Fil-C rustc build succeeded."
echo "  rustc:    $RUST_BUILD/rustc"
echo "  filrustc: $RUST_BUILD/filrustc"
echo "NOTE: put $FILC_ROOT/build/bin on PATH when using this rustc; the"
echo "x86_64-unknown-linux-gnu target links with Fil-C's \`filcc\`."
