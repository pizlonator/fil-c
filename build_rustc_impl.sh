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
#
# Shared implementation of the Fil-C rustc build scripts.  This file is a
# sourced library: it is not meant to be executed directly.  Source it and
# then call:
#
#     do_build_rustc <clean|incremental>
#
# `clean` removes rust-build/ entirely and rebuilds from scratch.
# `incremental` reuses an existing rust-build/ (extraction, the compiler build
# and everything else resume incrementally; a missing rust-build/ is
# bootstrapped gracefully, so the incremental mode doubles as a first build).
# Both modes end by creating the rust-build/rustc and rust-build/filrustc
# symlinks and then running the Fil-Rust test suite (filrustc/run-tests), so
# that a build that passes is a build whose compiler is known to work.  Set
# SKIP_RUST_TESTS=1 in the environment to skip the test suite.
#
# Build Fil-C's rustc.
#
# This extracts the patched Rust 1.90.0 sources from the projeny checkout in
# projects/rust into rust-build/rust, writes a bootstrap config.toml, and then
# runs `x.py build --stage 1`.  Rust does NOT build its own LLVM: the config
# points bootstrap's [target.x86_64-unknown-linux-gnu] llvm-config at the
# Fil-C LLVM that build_clang.sh already built into build/ (bootstrap then
# treats LLVM as prebuilt and never touches src/llvm-project), and
# rustc_llvm's build script uses that llvm-config to link the Fil-C LLVM
# statically into librustc_driver.so -- with no libz (the Fil-C LLVM build has
# LLVM_ENABLE_ZLIB=OFF), with libstdc++ linked statically
# ([llvm] static-libstdcpp = true) and with the -lgcc_s that stage0 std's
# libunwind rlib metadata adds redirected to the static libgcc_eh archive via
# a -B directory holding a libgcc_s.so linker script (see below), so
# librustc_driver.so ends up needing only libm, libc and the dynamic loader.
# build_clang.sh builds the llvm-config binary and the LLVMMCA archives that
# rustc's LLVM component query needs, so run it first.  The resulting stage 1
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
# The script also bakes the directory containing `filcc` into the compiler as
# $FILC_LINKER_DIR (resolved after $FILC_ROOT/build/bin is put on PATH below,
# so in the normal pizfix-mode checkout that is <checkout>/build/bin; /opt/fil/
# bin in optfil mode; wherever the chroot has filcc in pizlix mode).  An
# option_env!("FILC_LINKER_DIR") in compiler/rustc_codegen_ssa/src/back/link.rs
# (in the projeny patch) uses it at run time to resolve the target spec's
# bare-name linker "filcc" to an absolute path, with further fallbacks (walk up
# from the rustc binary for <checkout>/build/bin/filcc, then /opt/fil/bin/filcc,
# then the PATH), so the built rustc links without anything extra on PATH and
# keeps working after the checkout is moved.  A stamp file
# (rust-build/.filc-linker-dir-stamp) forces rustc_codegen_ssa to recompile
# when the baked value changes, exactly like the FILC_VERSION stamp above.
# The rustc driver also statically embeds the LLVM archives that build_clang.sh
# built into build/lib, which cargo does not fingerprint at all; a stamp of the
# LLVM input state (rust-build/.filc-llvm-libs-stamp) forces the rustc_llvm
# recompile + relink when those archives or the LLVM headers change, so the
# shipped rustc can never keep running stale LLVM code (see below).
#
# The script is idempotent: in incremental mode, if rust-build/rust already
# exists, extraction is skipped and bootstrap resumes incrementally.
#
# The result is rust-build/rustc (symlink, also as rust-build/filrustc).
# NOTE: the built rustc finds `filcc` on its own (see above), but putting
# $FILC_ROOT/build/bin on PATH when using it is still the supported flow (and
# is how this script itself finds filcc to bake in).

# Resolve the repo root (the directory containing this file) from the script
# location itself, so that sourcing this file works regardless of the
# caller's current working directory.
FILC_RUSTC_IMPL_ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)

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

# Build Fil-C's rustc.  mode is "clean" (remove rust-build/ first) or
# "incremental" (reuse rust-build/, building only what changed; a missing
# rust-build/ is bootstrapped from scratch).
do_build_rustc() {
    local mode="${1:-clean}"
    case "$mode" in
        clean|incremental)
            ;;
        *)
            echo "ERROR: do_build_rustc: bad mode '$mode' (want clean or incremental)" >&2
            return 1
            ;;
    esac
    set -e
    set -o pipefail
    set -x
    cd -- "$FILC_RUSTC_IMPL_ROOT"
    # A clean build starts from a wiped rust-build/.  The incremental build
    # keeps it (source extraction below only happens when what it produces,
    # rust-build/rust, is missing, so re-runs are fast no-ops).
    if [ "$mode" = clean ]; then
        rm -rf rust-build
    fi

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

    # Rustc links the Fil-C LLVM that build_clang.sh already built into
    # build/, via llvm-config.  With [target.<triple>] llvm-config set,
    # bootstrap's prebuilt_llvm_config() returns AlreadyBuilt and never
    # configures or compiles src/llvm-project, so there is no second LLVM
    # build.  Fail fast with a clear message if the Fil-C LLVM is missing.
    if [ ! -x $FILC_ROOT/build/bin/llvm-config ]; then
        echo "ERROR: $FILC_ROOT/build/bin/llvm-config not found.  The Fil-C" >&2
        echo "       rustc links the Fil-C LLVM from build/ (this script does" >&2
        echo "       not build LLVM); run ./build_clang.sh first." >&2
        exit 1
    fi

    # Fil-C: rustc statically embeds the LLVM archives from build/lib (the
    # driver is linked against `llvm-config --link-static`'s library set), so
    # the LLVM code inside librustc_driver.so is whatever was in build/lib at
    # link time.  Cargo does not notice when those archives change: only
    # build-script outputs and crate sources are fingerprinted, and the LLVM
    # libraries are neither -- they are an external input resolved through
    # llvm-config at build-script run time.  A rebuilt Fil-C LLVM (say, after
    # editing llvm/lib/Transforms/Scalar/SROA.cpp and running build_clang.sh)
    # would therefore leave the shipped rustc running stale LLVM code (this
    # actually happened: a rebuilt SROA shipped in clang but not in rustc).
    # Keep a stamp of the LLVM inputs the driver was last linked against --
    # the newest archive mtime/size in build/lib plus the newest header mtime
    # under llvm/include and build/include (header changes affect the LLVM
    # C++ API the rustc wrappers compile against).  When it moves, touch the
    # rustc_llvm wrapper source, forcing cargo to recompile rustc_llvm and
    # relink librustc_driver.so against the updated archives.  An unchanged
    # LLVM tree makes this a fast no-op (two find scans, well under 100ms).
    # A missing stamp is treated as changed: the first run of this script
    # revision relinks once, and later runs compare against what it recorded.
    FILC_LLVM_STAMP=$RUST_BUILD/.filc-llvm-libs-stamp
    FILC_LLVM_STATE=
    # The newest (mtime:size) LLVM archive feeding the static link, from the
    # same directory llvm-config points at.  Only libLLVM*.a counts: the
    # driver links the LLVM component archives (llvm-config --link-static)
    # plus the MCA ones, not clang's own static libraries -- whose routine
    # churn on clang-side edits must not trigger rustc relinks.  (sed, not
    # `head -1`: this script runs under `set -e -o pipefail`, and a reader
    # that exits early SIGPIPEs the writer into a silent exit 141 -- sed
    # drains the pipeline instead.)
    if [ -d "$FILC_ROOT/build/lib" ]; then
        FILC_LLVM_STATE=`find "$FILC_ROOT/build/lib" -maxdepth 1 \
            -name 'libLLVM*.a' -printf '%T@:%s\n' \
            2>/dev/null | sort -rn | sed -n '1p'`
    fi
    # Plus the newest header mtime in the LLVM include trees the wrappers'
    # includes resolve to; a missing tree contributes nothing.
    for FILC_LLVM_INC_DIR in "$FILC_ROOT/llvm/include" "$FILC_ROOT/build/include"; do
        if [ -d "$FILC_LLVM_INC_DIR" ]; then
            FILC_LLVM_STATE="$FILC_LLVM_STATE`find "$FILC_LLVM_INC_DIR" \
                -name '*.h' -printf '%T@\n' 2>/dev/null | sort -rn | sed -n '1p'`"
        fi
    done
    if [ ! -f $FILC_LLVM_STAMP ] || [ "`cat $FILC_LLVM_STAMP 2>/dev/null`" != "$FILC_LLVM_STATE" ]; then
        touch $RUST_SRC/compiler/rustc_llvm/llvm-wrapper/PassWrapper.cpp
        echo "$FILC_LLVM_STATE" > $FILC_LLVM_STAMP
        echo "Fil-C LLVM libraries changed: relinking the rustc driver against" >&2
        echo "the updated LLVM archives (touched compiler/rustc_llvm/llvm-wrapper/PassWrapper.cpp)." >&2
    fi

    # compiler/rustc_llvm/llvm-wrapper/PassWrapper.cpp: the Fil-C pipeline
    # comes from LLVM's RegisterFilPasses -- registered on the PassBuilder
    # with no extra arguments, exactly like clang/lib/CodeGen/BackendUtil.cpp
    # and filc's LLVM JIT shim register it (which also registers the
    # OptimizerLast DeleteRedundantPollchecks), and applied directly to the
    # fresh pipeline in the NoPrepopulatePasses path (there is no
    # PassBuilder-built pipeline there to run the extension-point callbacks)
    # -- plus the "after Fil-C" DataLayout setup.  The IsRust parameter that
    # used to gate the pre-pizlonation simplification and inlining block off
    # for rustc is gone: the rustc-shaped miscompiles it existed for are
    # fixed at their sources (the rustc-side typed allocas below for the
    # spawn+join shape; the std swap fix below for the mem::swap class), so
    # the full Fil-C pipeline runs for every frontend.  See
    # llvm/lib/Passes/RegisterFilPasses.cpp.
    check_grep "RegisterFilPasses(PB, nullptr)" compiler/rustc_llvm/llvm-wrapper/PassWrapper.cpp
    check_grep "llvm::FilCAddPipelineStartPasses(MPM, OptLevel);" compiler/rustc_llvm/llvm-wrapper/PassWrapper.cpp
    check_grep "setDataLayoutAfterFilC" compiler/rustc_llvm/llvm-wrapper/PassWrapper.cpp
    check_grep "DeleteRedundantPollchecksPass()" compiler/rustc_llvm/llvm-wrapper/PassWrapper.cpp

    # compiler/rustc_codegen_llvm/src/type_of.rs + context.rs + builder.rs, and
    # compiler/rustc_codegen_ssa/src/{traits/builder,mir/place,mir/rvalue,mir/block}.rs:
    # the Fil-C all-pointer struct representation of pointer-maybe aggregates
    # for allocas (clang's union representation hack, CGRecordLayoutBuilder.cpp
    # commit 4a54ae4076b4).  This rustc-side fix closes the spawn+join shape:
    # rustc's allocas are untyped byte arrays, so unlike clang there are no
    # field types for SROA to inherit; the Fil-C rustc emits typed allocas
    # with all-pointer struct types for aggregates that may contain pointers,
    # which types the memcpy-only partitions as pointers -- so the
    # pre-pizlonation pipeline (whose pre-inliner SROA would otherwise
    # integer-widen those partitions, dropping GIMSO capabilities) cannot
    # miscompile that shape.
    check_grep "fn filc_may_contain_pointers" compiler/rustc_codegen_llvm/src/type_of.rs
    check_grep "fn filc_ptr_struct_type" compiler/rustc_codegen_llvm/src/type_of.rs
    check_grep "Packed=*/ true" compiler/rustc_codegen_llvm/src/type_of.rs
    check_grep "filc_ptr_maybe" compiler/rustc_codegen_llvm/src/context.rs
    check_grep "fn alloca_for_layout" compiler/rustc_codegen_llvm/src/builder.rs
    check_grep "filc_ptr_struct_type" compiler/rustc_codegen_llvm/src/builder.rs
    check_grep "fn alloca_for_layout" compiler/rustc_codegen_ssa/src/traits/builder.rs
    check_grep "alloca_for_layout(size, layout.align.abi, layout)" compiler/rustc_codegen_ssa/src/mir/place.rs
    check_grep "alloca_for_layout(size, align, operand.layout)" compiler/rustc_codegen_ssa/src/mir/rvalue.rs
    check_grep "alloca_for_layout(arg.layout.size, required_align, arg.layout)" compiler/rustc_codegen_ssa/src/mir/block.rs

    # library/core/src/ptr/mod.rs: the Fil-C pointer-typed swap chunk buffers.
    # std's swap_nonoverlapping_chunks used to move the bytes of swapped
    # pointer-containing elements through NUMERIC-typed MaybeUninit<[u8; N]>
    # block buffers, whose partitions upstream SROA integer-widens, dropping
    # GIMSO capabilities.  swap_nonoverlapping_bytes now takes
    # align_of::<T>() and swaps pointer-aligned regions in pointer-typed
    # MaybeUninit<*const ()> chunks (the same house pattern the Fil-C user
    # libc uses for scratch that receives whole elements), so the
    # pre-pizlonation pipeline cannot miscompile the mem::swap class either,
    # and the IsRust gate is gone -- the full Fil-C pipeline runs for every
    # frontend (see llvm/lib/Passes/RegisterFilPasses.cpp).
    check_grep "swap_nonoverlapping_chunks::<*const ()>" library/core/src/ptr/mod.rs
    check_grep "MaybeUninit<*const ()>" library/core/src/ptr/mod.rs

    # compiler/rustc_target/src/spec/targets/x86_64_unknown_linux_gnu.rs: the
    # "before Fil-C" data layout (-ni:0), filcc as the linker, -normalcrt, no
    # stack probes, no sanitizers, and the default (unwind) panic strategy --
    # unwinding is implemented on the Fil-C runtime's unwinder (see the
    # personality in library/std/src/sys/personality/gcc.rs).
    check_grep "e-m:e-ni:0-p270:32:32" compiler/rustc_target/src/spec/targets/x86_64_unknown_linux_gnu.rs
    check_grep 'base.linker = Some("filcc".into());' compiler/rustc_target/src/spec/targets/x86_64_unknown_linux_gnu.rs
    check_grep '"-m64", "-normalcrt"' compiler/rustc_target/src/spec/targets/x86_64_unknown_linux_gnu.rs
    check_grep "StackProbeType::None" compiler/rustc_target/src/spec/targets/x86_64_unknown_linux_gnu.rs
    check_grep "SanitizerSet::empty()" compiler/rustc_target/src/spec/targets/x86_64_unknown_linux_gnu.rs
    check_grep "unwinding is implemented on the Fil-C runtime's unwinder" compiler/rustc_target/src/spec/targets/x86_64_unknown_linux_gnu.rs

    # ...and the spec must NOT force the abort panic strategy any more (that
    # would break unwinding, which now works: see the personality routine).
    # This is a block-scoped inverse check, like the unwind-crate one below.
    if grep -q 'PanicStrategy' "$RUST_SRC/compiler/rustc_target/src/spec/targets/x86_64_unknown_linux_gnu.rs"; then
        echo "ERROR: projeny patch missing filc change: PanicStrategy still" >&2
        echo "       appears in compiler/rustc_target/src/spec/targets/x86_64_unknown_linux_gnu.rs." >&2
        echo "       The x86_64-unknown-linux-gnu target must keep the default (unwind)" >&2
        echo "       panic strategy: Fil-C Rust unwinding is implemented on the Fil-C" >&2
        echo "       runtime's unwinder (see library/std/src/sys/personality/gcc.rs)." >&2
        echo "       projects/rust.projeny must carry ALL Fil-C changes to the Rust" >&2
        echo "       sources; edit projects/rust/... and re-run" >&2
        echo "       'filc/projeny commit projects/rust.projeny'." >&2
        exit 1
    fi

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

    # library/unwind/src/libunwind.rs: the pizlonated landing-pad LSDA API
    # (filc/include/pizlonated_eh_landing_pad.h), which libpizlo exports as
    # `pizlonated_pizlonated_eh_*`; the Fil-C personality routine walks the
    # landing pad's action list through it.
    check_grep "pizlonated_eh_landing_pad_get_num_actions" library/unwind/src/libunwind.rs

    # library/std/src/sys/personality/gcc.rs: the Fil-C personality routine.
    # It reads the pizlonated landing-pad data (not DWARF), only claims a
    # catch-all for Rust's own exception class ("MOZ\0RUST" -- phase-1
    # decisions are final under Fil-C, so foreign exceptions must keep
    # unwinding), and only installs catch pads at the frame the search phase
    # found (_UA_HANDLER_FRAME): there is no _Unwind_SetIP under Fil-C, and a
    # catch pad entered for a foreign exception would rtabort in
    # panic_unwind's cleanup.
    check_grep "pizlonated_eh_landing_pad_get_num_actions" library/std/src/sys/personality/gcc.rs
    check_grep '*b"MOZ\0RUST"' library/std/src/sys/personality/gcc.rs
    check_grep "_UA_HANDLER_FRAME == 0" library/std/src/sys/personality/gcc.rs

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

    # compiler/rustc_codegen_ssa/src/back/link.rs: resolve the target spec's
    # bare-name linker "filcc" to an absolute path at run time: the directory
    # baked in below as FILC_LINKER_DIR, then the Fil-C checkout layout found
    # by walking up from the rustc binary (<checkout>/build/bin/filcc), then
    # /opt/fil/bin (the optfil install location), and only then the PATH
    # (pizlix mode).
    check_grep "fn filc_resolve_linker" compiler/rustc_codegen_ssa/src/back/link.rs
    check_grep 'option_env!("FILC_LINKER_DIR")' compiler/rustc_codegen_ssa/src/back/link.rs

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
# Rustc links the Fil-C LLVM from build/ via llvm-config (see the [target]
# section below), so bootstrap never builds LLVM and these [llvm] settings
# are the ones that still matter: download-ci-llvm = false keeps bootstrap
# from fetching a CI LLVM, and assertions = true matches the Fil-C LLVM's
# LLVM_ENABLE_ASSERTIONS=ON (bootstrap then compiles the LLVM C++ wrappers
# with assertions enabled, like the LLVM itself).
#
# Fil-C: link libstdc++ statically into librustc_driver.so.  The Fil-C LLVM
# is built with LLVM_STATIC_LINK_CXX_STDLIB=ON, so without this the rustc
# link would pull in the dynamic libstdc++.so.6, which is exactly what we
# must not need.
static-libstdcpp = true
# NOTE: no `targets = "X86"` here.  That setting only feeds
# LLVM_TARGETS_TO_BUILD when bootstrap builds LLVM itself, which it never
# does with an external llvm-config; the backends rustc links are whatever
# the Fil-C LLVM was configured with (X86 and AArch64), read out of
# llvm-config at build time.

[rust]
# Fil-C: no in-tree or system libunwind.  pizfix has no libgcc_s, and LLVM
# libunwind cannot be built under Fil-C anyway (Fil-C's assembler rejects its
# assembly files).  Instead, the unwind crate is patched (in the projeny
# patch) to link nothing: filcc already links the pizfix runtime (libpizlo +
# libyolounwind) into every binary, and libpizlo exports the full _Unwind_*
# API under the pizlonator's renamed symbol names.
llvm-libunwind = "no"
# Fil-C: must stay false with an external llvm-config (bootstrap has no
# in-tree lld to build, and rustc must not try to link one).
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
# Fil-C: rustc's own LLVM-IR codegen testsuite is not run here
# (filrustc/run-tests is the Fil-C test suite for this compiler).  With an
# externally configured LLVM this also lifts bootstrap's sanity-check
# requirement that a FileCheck binary exist next to llvm-config.
codegen-tests = false
# Fil-C: do not install the LLVM tools (llvm-cov, llvm-objdump, ...) into the
# stage sysroot (bootstrap's equivalent of rustup's llvm-tools-preview
# component).  With an external llvm-config bootstrap would copy them out of
# llvm-config's bindir -- build/bin, which holds the Fil-C clang, not the
# LLVM tool suite.
llvm-tools = false
EOF

    # The [target] section needs the absolute llvm-config path expanded by
    # this script, so it is written separately (the heredoc above is quoted).
    cat >> $RUST_SRC/config.toml <<EOF

[target.x86_64-unknown-linux-gnu]
# Use the Fil-C LLVM that build_clang.sh built into build/ instead of
# building a second LLVM.  With this set, bootstrap treats LLVM as prebuilt
# (prebuilt_llvm_config returns AlreadyBuilt; src/llvm-project is never
# compiled) and hands the path to rustc_llvm's build script, which queries it
# for cxxflags/libs/ldflags and links the Fil-C LLVM statically.  The path
# MUST be absolute: bootstrap joins relative [target] paths onto the rust
# source root.
llvm-config = "$FILC_ROOT/build/bin/llvm-config"
# The Fil-C LLVM is not a stock upstream LLVM (it carries the Fil-C passes,
# e.g. FilPizlonator/KillUB), so tell bootstrap that this user-provided LLVM
# is a patched one.  This only flips bootstrap's LLVM_RUSTLLVM knob for the
# rustc LLVM wrappers, which do not key on it, so it is inert here -- but it
# is the honest declaration.
llvm-has-rust-patches = true
EOF
    cat $RUST_SRC/config.toml

    # Fil-C's clang/filcc (the target spec links with `filcc`) live in build/bin.
    export PATH="$FILC_ROOT/build/bin:$PATH"

    # Fil-C: redirect the explicit -lgcc_s (from stage0 std's libunwind rlib
    # metadata, which lands in the -nodefaultlibs rustc link) to the static
    # libgcc_eh archive, so librustc_driver.so does not NEED libgcc_s.
    # -static-libgcc alone does NOT work here (proven): it only stops the
    # compiler driver from adding its own -lgcc_s, while this -lgcc_s comes
    # from the std rlibs' metadata.
    #
    # The redirect dir must be searched BEFORE /usr/lib/gcc/... (which holds
    # the real libgcc_s.so linker script).  Two mechanisms are used, because
    # neither alone is sufficient: the -L must come from RUSTFLAGS directly
    # (a -Clink-arg is emitted at the very END of the linker command, after
    # the -L/usr/lib/gcc/... that rustc_llvm's build script adds for the
    # static libstdc++, and the gcc driver builds its search path in
    # command-line order), while -B<dir> is searched for subprograms and
    # libraries alike and is kept as belt-and-suspenders for links that do
    # not echo the rustc -L args (e.g. tools linked with other drivers).
    # The libgcc_s.so linker script GROUPs the static libgcc_eh archive,
    # which is then linked statically into librustc_driver.so.  RUSTFLAGS
    # propagates to every cargo invocation bootstrap makes, so this also
    # statically satisfies -lgcc_s for the proc-macros and tools built along
    # the way (it is inert for rlib-only compilations).  Note that changing
    # RUSTFLAGS invalidates all cargo fingerprints -- a one-time churn on the
    # first build with this script revision.
    GCC_EH="$(cc --print-file-name=libgcc_eh.a)"
    if [ ! -f "$GCC_EH" ]; then
        echo "ERROR: cannot find libgcc_eh.a (cc --print-file-name=libgcc_eh.a" >&2
        echo "       printed '$GCC_EH').  It is needed to statically satisfy the" >&2
        echo "       -lgcc_s that stage0 std's libunwind rlib metadata adds to the" >&2
        echo "       rustc link." >&2
        exit 1
    fi
    mkdir -p "$RUST_BUILD/gcc-redirect"
    printf 'GROUP ( %s )\n' "$GCC_EH" > "$RUST_BUILD/gcc-redirect/libgcc_s.so"
    export RUSTFLAGS="-L$RUST_BUILD/gcc-redirect -Clink-arg=-B$RUST_BUILD/gcc-redirect"

    # Fil-C: bake the directory containing filcc into the compiler (consumed via
    # option_env!("FILC_LINKER_DIR") in rustc_codegen_ssa).  In pizfix mode this
    # is <checkout>/build/bin; in optfil mode /opt/fil/bin; in pizlix mode
    # whatever the chroot has on PATH.  Runtime fallbacks (see codegen_ssa) cover
    # relocated checkouts, so this only has to be right for the build itself.
    # If filcc is not on PATH at all, leave FILC_LINKER_DIR unset: the built
    # rustc still discovers filcc at run time (checkout walk, /opt/fil/bin, PATH).
    if command -v filcc >/dev/null 2>&1; then
        # PATH entries can be relative, and the baked candidate is exists()-checked against rustc's CWD at link time, so bake an absolute path.
        FILC_LINKER_DIR="$(cd -- "$(dirname -- "$(command -v filcc)")" && pwd -P)"
        export FILC_LINKER_DIR
    fi

    # option_env! is expanded when the rustc_codegen_ssa crate compiles, and
    # cargo does not notice when the *value* of an environment variable changes,
    # so keep a stamp recording the value the tree was last built with (same
    # mechanism as the FILC_VERSION stamp above): when it differs (or is
    # missing), touch a source file in that crate, forcing cargo to recompile
    # rustc_codegen_ssa and relink rustc with the new value.  An unchanged value
    # makes this a fast no-op.  If filcc is not on PATH, drop any previously
    # baked value the same way (recompile without FILC_LINKER_DIR).
    if [ -n "${FILC_LINKER_DIR:-}" ]; then
        FILC_LINKER_STAMP=$RUST_BUILD/.filc-linker-dir-stamp
        if [ ! -f $FILC_LINKER_STAMP ] || [ "`cat $FILC_LINKER_STAMP`" != "$FILC_LINKER_DIR" ]; then
            touch $RUST_SRC/compiler/rustc_codegen_ssa/src/back/link.rs
            echo "$FILC_LINKER_DIR" > $FILC_LINKER_STAMP
        fi
        echo "Baked-in filcc directory: $FILC_LINKER_DIR"
    elif [ -f $RUST_BUILD/.filc-linker-dir-stamp ]; then
        rm -f $RUST_BUILD/.filc-linker-dir-stamp
        touch $RUST_SRC/compiler/rustc_codegen_ssa/src/back/link.rs
    fi

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
    if [ -n "${FILC_LINKER_DIR:-}" ]; then
        FILC_LINKER_RESOLUTION="baked directory $FILC_LINKER_DIR, checkout discovery, /opt/fil/bin, then PATH"
    else
        FILC_LINKER_RESOLUTION="checkout discovery, /opt/fil/bin, or PATH"
    fi
    echo "NOTE: this rustc resolves its \`filcc\` linker itself ($FILC_LINKER_RESOLUTION),"
    echo "so nothing extra has to be on PATH; putting $FILC_ROOT/build/bin on"
    echo "PATH is still supported."

    # Run the Fil-Rust test suite (filrustc/run-tests).  This is the default:
    # a build isn't done until its compiler is known to work.  The suite
    # compiles and runs everything under rust-build/filrustc and prints
    # "SUCCESS" (and exits 0) iff every test passed; `set -e -o pipefail` turns
    # any failure into a build failure.  Export SKIP_RUST_TESTS=1 to skip it.
    cd -- "$FILC_ROOT"
    if [ "${SKIP_RUST_TESTS:-0}" = "1" ]; then
        echo "SKIP_RUST_TESTS=1: skipping the Fil-Rust test suite."
    else
        echo "Running the Fil-Rust test suite (filrustc/run-tests;"
        echo "log: $RUST_BUILD/tests.log)..."
        ./filrustc/run-tests 2>&1 | tee $RUST_BUILD/tests.log
    fi
}
