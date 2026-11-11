# Fil-C rustc — build state & known issues

`build_rustc.sh` (repo root) builds a stage-1 rustc that pizlonates everything
it compiles, using the Fil-C LLVM and linking binaries with `filcc`.  It is a
thin entry point that calls `do_build_rustc clean` in `build_rustc_impl.sh`:
a clean build + test by default (it nukes `rust-build/` and rebuilds from
scratch; see "Build scripts" below).  `build_rustc_incremental.sh` is the
same thing without the nuke.  Output: `rust-build/rustc` (the compiler, a
symlink to the stage-1 rustc binary) and `rust-build/filrustc` (a symlink to
the same rustc; both spellings work and are equivalent -- the Fil-C behavior
is baked into the compiler).  The compiler finds its `filcc` linker on its
own (baked directory, checkout discovery, /opt/fil/bin, then PATH -- see
"How the built rustc finds filcc" below), so nothing extra has to be on
PATH, though keeping Fil-C's `build/bin` on PATH still works.
`filrustc --version` prints
`rustc 1.90.0 (1159e78c4 2025-09-14) (built from a source tarball)`
followed by the Fil-C parenthetical ` (Fil-C 0.687
git@github.com:pizlonator/fil-c.git <fil-c HEAD hash>)` -- mirroring what clang
appends to `clang --version` (see "Fil-C version identification" below).

The build is wired into the Fil-C flavor build scripts as
`./build_rustc.sh` in `build_glibc_extras.sh` (glibc flavor only; see
"Flavor check" below for why it is not in `build_all_slow.sh`).

Prerequisite: no host Rust toolchain is needed.  x.py's bootstrap downloads
its pinned stage0 compiler (and cargo) from static.rust-lang.org and builds
everything with those, so a fresh container only needs python3, curl,
gcc/g++/make, ruby (for the test suite) and the usual Fil-C build
prerequisites -- all of which the enter_container.sh image installs (see
"Container audit" below).  Since rustc links the Fil-C LLVM from build/ (no
LLVM is built here), cmake/ninja are not needed by the rust build itself
(they remain prerequisites of the Fil-C compiler build that produces
llvm-config).  (An earlier revision of this note claimed a
host rustup was required; that was wrong -- the container has no
rustc/cargo/rustup at all and the build works.)

This file is the tracked (git-added) copy of `rust-build/NOTES.md`; the
rust-build copy is kept in sync for the disposable build dir.

## Where the Fil-C changes live

ALL Fil-C changes to the Rust sources live in the projeny patch
(`projects/rust.projeny`); the checkout is `projects/rust/`.  To change the
sources: edit `projects/rust/...` and run `filc/projeny commit
projects/rust.projeny` (then `git add projects/rust.projeny`).  The build
script applies no source tweaks of its own; it only *verifies* that every
expected Fil-C change is present in the extracted tree (the `check_grep`
calls in `build_rustc_impl.sh` -- 82 of them; 9 of those were added for the
ARM64 bring-up, see "ARM64 (aarch64) bring-up" below, and 14 for the
Fil-Rust test-suite fixes, counted as
`grep -c '^\s*check_grep ' build_rustc_impl.sh`; the two other mentions of
the name are the comment above the helper and its definition) and fails
fast with a clear
"projeny patch missing filc change X" error otherwise.

The patch consists of the following changes:

1. `compiler/rustc_llvm/llvm-wrapper/PassWrapper.cpp` — DataLayout-after-FilC
   setup at the top of LLVMRustOptimize (`setDataLayoutAfterFilC`), plus the
   registration of LLVM's Fil-C pass pipeline via
   `RegisterFilPasses(PB, nullptr)` (LLVM's
   `llvm/lib/Passes/RegisterFilPasses.cpp` — the same entry point
   `clang/lib/CodeGen/BackendUtil.cpp` calls with no extra arguments, as does
   filc's LLVM JIT shim), and
   `llvm::FilCAddPipelineStartPasses(MPM, OptLevel)` +
   `DeleteRedundantPollchecksPass()` applied directly to the fresh pipeline in
   the NoPrepopulatePasses path (no PassBuilder-built pipeline runs there, so
   the extension-point callbacks would never fire — allocator-shim modules
   must still pizlonate).  rustc's own callbacks (CFI, verify, lint, instr
   profiling, sanitizers) stay in rustc's callback vectors and apply around
   the Fil-C passes in the same order as on a PassBuilder-built pipeline.
   The pipeline itself (KillUB -> pre-pizlonation simplification and
   inlining -> coro lowering -> FilPizlonator, plus OptimizerLast
   DeleteRedundantPollchecks) lives in LLVM now, and every frontend runs it
   in full: the IsRust parameter that used to gate the pre-pizlonation
   simplification+inlining half off for rustc is gone (see known-issues #1
   for the two rustc-shaped miscompiles it existed for -- the spawn+join
   shape, fixed by the rustc-side typed allocas, and the mem::swap class,
   fixed in std itself -- and for the abandoned SROA-side alternative).
   Every pipeline path pizlonates.
2. `compiler/rustc_target/src/spec/targets/x86_64_unknown_linux_gnu.rs` —
   data_layout with `-ni:0` (before-Fil-C), StackProbeType::None,
   linker = "filcc", `-normalcrt` pre-link arg (full Fil-C CRT under
   -nodefaultlibs), and sanitizers/xray off.  The panic strategy is the
   default (unwind): Fil-C Rust unwinding is implemented on the Fil-C
   runtime's unwinder (see "Panics" below).  (Since the ARM64 bring-up this
   spec has an aarch64 twin, `aarch64_unknown_linux_gnu.rs`, with the same
   Fil-C shape -- see "ARM64 (aarch64) bring-up" below.)
3. `library/compiler-builtins/compiler-builtins/src/probestack.rs` — the
   naked-asm stack-probe helper is compiled out exactly for
   `all(target_arch = "x86_64", target_os = "linux", target_env = "gnu")`,
   mirroring the target spec (the Fil-C targets: x86_64-unknown-linux-gnu
   and, since the ARM64 bring-up, aarch64-unknown-linux-gnu -- the cfg
   widens to include `target_arch = "aarch64"` there; see "ARM64 (aarch64)
   bring-up" below).
4. `compiler/rustc_codegen_llvm/src/context.rs` — accept the target spec's
   before-FilC data_layout ("-ni:0") in the MismatchedDataLayout validation
   (LLVM's TargetMachine default is the same string minus "-ni:0").
5. `library/std_detect/src/detect/os/x86.rs` and
   `library/compiler-builtins/libm/src/math/arch/x86/detect.rs` — Fil-C-safe
   CPU feature detection.  Upstream detects x86 features with the `cpuid`
   instruction (plus `xgetbv` for the OS XSAVE state), which rustc emits as
   Intel-dialect inline assembly (`asm!` defaults to Intel; LLVM records the
   call as `asm sideeffect inteldialect`), and the Fil-Pizlonator only
   accepts AT&T dialect inline assembly — so `is_x86_feature_detected!`
   aborted at run time under Fil-C ("filc safety error: cannot handle
   inline asm (only AT&T dialect inline assembly is supported)").  A pure
   AT&T rewrite is impossible from Rust: rustc refuses to name `ebx` as an
   inline-asm operand or clobber on x86_64 ("rbx is used internally by LLVM
   and cannot be used as an operand for inline asm"; the register is
   LLVM's potential base pointer), so `cpuid`'s EBX output can never be
   declared the way the pizlonator's `cpuid` whitelist requires ("cpuid
   output ebx not covered by output constraint or clobber"), and the
   upstream mov/xchg scratch-register trick (which saves/restores `ebx`
   without declaring it) is rejected for the same reason ("literal register
   %ebx not covered by output constraint or clobber").  Therefore:
   * `std_detect`'s x86 backend parses /proc/cpuinfo instead (needs the
     `std_detect_file_io` std feature, which build_rustc.sh's config.toml
     enables): the kernel has already merged CPUID, the OS-enabled XCR0
     state and CR4.OSXSAVE into the `flags` line, so the result matches the
     CPUID-based computation — for the AVX-512/AMX OS-support checks it is
     even more accurate, because the kernel clears the flags when the OS
     does not enable the state.  The `FLAG_TO_FEATURE` table covers every
     `@FEATURE`-queryable feature (74 entries; kernel names are identity
     except for the aliases it documents, e.g. sse3=`pni`,
     cmpxchg16b=`cx16`, sha=`sha_ni`, ermsb=`erms`, lzcnt=`abm`,
     kl=`keylocker`, widekl=`wide_kl`, avx512bf16=`avx512_bf16`,
     avx512fp16=`avx512_fp16`).
   * libm's detector reports no CPU features (libm is `no_std` with no libc
     dependency, so it has no file-IO fallback): its `fma.rs` dispatch then
     always selects the portable software implementations (correct, just
     slower); the `vfmadd*` inline-asm bodies there are unreachable under
     Fil-C.
   * libm's `sqrtf`/`sqrt` (`libm/src/math/arch/x86.rs`) keep their
     `sqrtss`/`sqrtsd` inline asm but now pass `options(att_syntax)` — the
     pizlonator accepts that AT&T-dialect form (verified).
   * `core::arch::x86::__cpuid`/`__cpuid_count` themselves are NOT fixed:
     they still abort at run time under Fil-C (same inline-asm limitation;
     see known-issues #7).
6. `compiler/rustc_codegen_ssa/src/back/write.rs` — allocator shim modules get
   a real opt level (upstream leaves opt_level None, which skips
   LLVMRustOptimize entirely, so the shim would never be pizlonated and its
   DataLayout never flipped; the backend then asserts).
7. `compiler/rustc_codegen_ssa/src/back/linker.rs` — the auto-generated LD
   version script for dylibs exports pizlonated names:
   `pizlonatedFI*_<sym>` / `pizlonatedFIP*_<sym>` per symbol (the pizlonator
   renames every symbol; `--no-undefined-version` would otherwise fail on the
   original names), plus a blanket `pizlonated*;` wildcard that also covers
   the `pizlonated_<name>` getter for every exported function/global and the
   pizlonated1ET*/pizlonated2ET* weak entry thunks.  Cross-DSO callers
   reference those getters, so without the wildcard every dylib was silently
   broken for dylib consumers.  This mirrors Fil-C's own convention (e.g. the
   `pizlonated*;` wildcard in projects/dbus.projeny's version script; Fil-C's
   clang driver does the same rewrite in clang/lib/Driver/ToolChains/Gnu.cpp).
8. `compiler/rustc_codegen_ssa/src/back/link.rs` — the synthetic `symbols.o`
   (undefined references that pull rlib archive members and wire up the
   weak-lang-item core/std circular dependency) references
   `pizlonated_<sym>` getter names instead of the original symbol names.
   After pizlonation the original names no longer exist, so the original-name
   references silently turned into phantom undefined imports in the dynamic
   symbol table of every dylib, which then failed any executable linking that
   dylib (`--no-allow-shlib-undefined` is the default for executables).
   Getters live in the same object files as the renamed definitions, so
   archive pull-in keeps working; the getters are exported (see #7) so they
   are GC roots; and symbols provided by the allocator shim/panic runtime
   resolve at final-link time.  All entries become Text (a getter is always a
   plain function, even for a thread-local global).  Exception:
   `rust_metadata_<crate>` symbols keep their original names -- they live in
   the raw metadata object, which is not compiled through LLVM and is
   therefore never pizlonated.
9. `src/bootstrap/src/core/build_steps/llvm.rs` — libunwind.a built with
   filcc/fil++ (currently unused: see unwind below).
10. `library/unwind/src/lib.rs` — link NOTHING for unwinding on linux-gnu.
    pizfix has no libgcc_s, and LLVM libunwind cannot be built under Fil-C
    (Fil-C's assembler "sarcasm" rejects libunwind's .S files:
    "instruction outside any function (only annotated functions are compiled)").
    Instead libpizlo (linked by filcc) exports the full _Unwind_* API under the
    pizlonator's renamed names.
11. `library/unwind/src/libunwind.rs` and
    `library/backtrace/src/backtrace/libunwind.rs` — Rust-side stubs for the
    `_Unwind_*` entry points that libpizlo lacks:
    `_Unwind_GetDataRelBase`/`_Unwind_GetTextRelBase` (return null) and
    `_Unwind_GetIPInfo` (wrapper around `_Unwind_GetIP`, ip_before_insn = 0)
    in the unwind crate, and `_Unwind_FindEnclosingFunction` in both the
    unwind crate and the vendored backtrace crate (libpizlo does not provide
    it at all; the backtrace crate's helper returns the passed-in ip, which
    mirrors upstream's own macOS workaround).  Without the backtrace stub the
    symbol was a phantom undefined import in every libstd.so dylib.
    `libunwind.rs` additionally declares the pizlonated landing-pad LSDA API
    (`pizlonated_eh_landing_pad_get_num_actions`/`_get_encoded_action`/
    `_get_caught_type_by_encoded_action`/`_get_filter_by_encoded_action` and
    the two `pizlonated_eh_filter_*` accessors, all exported by libpizlo as
    `pizlonated_pizlonated_eh_*`), which the Fil-C personality routine walks
    instead of DWARF; see "Panics" below.
12. `library/backtrace/src/symbolize/gimli/libs_dl_iterate_phdr.rs` +
    `parse_running_mmaps_unix.rs` — enumerate loaded libraries from
    /proc/self/maps instead of dl_iterate_phdr (also absent from pizfix).
13. `vendor/libc-0.2.174/src/unix/linux_like/linux/gnu/mod.rs` —
    `pub type pthread_t = *mut c_void;` (pizfix headers define
    `typedef void *pthread_t`; under the Fil-C ABI an integer typedef corrupts
    the thread handle across FFI).  Vendored checksum updated.
14. `library/std/src/sys/pal/unix/stack_overflow.rs` — linux-gnu now uses the
    no-op `imp` module (the one std already provides for miri/unsupported
    platforms).  std's SIGSEGV guard-page machinery cannot work under Fil-C:
    pizfix has no `sigaltstack` (calling it is a "filc internal error: sigaltstack
    not supported" abort) and Fil-C checks every memory access itself, so the
    guard page is never touched (Fil-C reports stack overflows through its own
    runtime instead).
15. `compiler/rustc_session/src/session.rs` — `Session::lto()` defaults to
    `Lto::No` instead of auto-enabling ThinLTO (`Lto::ThinLocal`) for optimized
    multi-CGU compiles.  See "ThinLTO" under known issues below.  An explicit
    `-Clto=...` still overrides the default.
16. `compiler/rustc_driver_impl/src/lib.rs` — `rustc --version` (and the first
    line of `rustc -vV`) appends " (Fil-C <version> <repo-url> <hash>)" from a
    compile-time `option_env!("FILC_VERSION")`, mirroring clang's version
    parenthetical (see "Fil-C version identification" below).  When the
    variable is unset (a vanilla, non-Fil-C build of this tree) nothing is
    appended.
17. `library/std/src/sys/personality/gcc.rs` — the Fil-C personality routine
    (the `eh_personality` lang item).  Under
    `all(target_arch = "x86_64", target_os = "linux", target_env = "gnu")`
    (the Fil-C target; the same cfg convention the patch uses in
    compiler-builtins and stack_overflow.rs) it reads the pizlonated
    landing-pad data through the accessors from #11 and implements the
    Fil-C two-phase flow (search phase: catch-alls only claim Rust's own
    exception class `*b"MOZ\0RUST"`, so foreign exceptions keep unwinding;
    cleanup phase: Drop glue installs everywhere, catch pads only at the
    frame the search phase found, no `_Unwind_SetIP`).  The upstream DWARF
    path is kept unchanged for other targets (compiled out here).
    See "Panics" below for the full model.
18. `library/std/src/panicking.rs` — `__rust_panic_cleanup` is declared
    `unsafe extern "Rust"` instead of `unsafe extern "C"` (see "Panics"
    below: rustc's C-ABI lowering represents a fat-pointer return as
    `{i64, i64}`, which loses the capability of the vtable half under
    Fil-C).
19. `library/panic_unwind/src/lib.rs` and
    `library/panic_abort/src/lib.rs` — the matching Rust-ABI definitions of
    `__rust_panic_cleanup`.  Nothing else in panic_unwind changes: its
    exception object (`class = u64::from_ne_bytes(*b"MOZ\0RUST")` +
    canary + payload Box) works unchanged, because the Fil-C runtime only
    touches the first 16 bytes of the exception object.
20. `compiler/rustc_codegen_ssa/src/back/link.rs` — `filc_resolve_linker`:
    when the linker is the bare name `filcc` (what the target spec sets, see
    #2), resolve it to an absolute path at run time instead of relying on
    PATH; see "How the built rustc finds filcc" below for the resolution
    order.  Only the exact bare name `filcc` is resolved, so `-C linker=...`
    and absolute paths are untouched.  Consumes the `FILC_LINKER_DIR`
    directory that build_rustc_impl.sh bakes in via `option_env!` (unset --
    the arm simply absent -- in a vanilla build of the tree).

21. `compiler/rustc_codegen_llvm/src/type_of.rs` +
    `compiler/rustc_codegen_llvm/src/builder.rs` +
    `compiler/rustc_codegen_llvm/src/context.rs` +
    `compiler/rustc_codegen_ssa/src/traits/builder.rs` +
    `compiler/rustc_codegen_ssa/src/mir/{place,rvalue,block}.rs` — **typed
    allocas with all-pointer struct types for aggregates that may contain
    pointers** (the rustc-side fix for the GIMSO/SROA miscompile of
    known-issues #1; clang's union-representation hack from
    CGRecordLayoutBuilder.cpp, commit 4a54ae4076b4, applied to rustc's stack
    slots).  `filc_may_contain_pointers` (type_of.rs) is a cached transitive
    predicate over the layout's fields and enum/coroutine variants;
    `filc_ptr_struct_type` (type_of.rs) builds the `{ ptr x N }` packed struct
    for exactly-sized 8-aligned pointer-maybe aggregates; `alloca_for_layout`
    (a new `BuilderMethods` trait method whose default is the plain untyped
    alloca) is overridden by the LLVM backend to emit that struct as the
    alloca's type; since `PlaceValue::alloca_size`/`PlaceRef::alloca`
    (mir/place.rs) route through `bx.alloca_for_layout`, that one default-impl
    override covers EVERY MIR codegen alloca (~20 `PlaceRef::alloca` call
    sites: locals, arguments, return places, debuginfo spill slots, va_list
    slots, rvalue/call temps), plus the direct `alloca_for_layout` calls for
    the `RValue::Cast` transmute temp (rvalue.rs) and the two
    `make_space_for_arg` call-argument scratch allocas (block.rs).  See
    known-issues #1 for why this exists, why it is done at
    the alloca only, and why it cannot slow down numeric code.  SINCE THE
    KNOWN-ISSUE-#13 FIX (change #29) the all-pointer struct type is
    belt-and-braces rather than load-bearing: `alloca_for_layout` also
    annotates every pointer-maybe aggregate alloca with a `zhas_union` call
    (clang parity), which makes the alloca an opaque escape for SROA, so
    SROA never partitions these allocas at all (see change #29).
22. `library/core/src/ptr/mod.rs` — **pointer-typed swap chunk buffers**
    (the std-side fix for the mem::swap class of known-issues #1(b); the
    user-libc house pattern "memory that may hold pointers must be
    pointer-typed" applied to std's own byte shuffling).
    `swap_nonoverlapping_bytes` now takes `align_of::<T>()` and dispatches
    the chunk type of `swap_nonoverlapping_chunks`/`swap_chunk`: when the
    swapped regions are pointer-aligned (`align_of::<T>() >= CHUNK_SIZE`
    where `CHUNK_SIZE = size_of::<*const ()>()`), pointer-sized chunks are
    swapped as `MaybeUninit<*const ()>` so the copies are pointer-typed
    (capability-preserving under GIMSO); otherwise the upstream
    `MaybeUninit<[u8; CHUNK_SIZE]>` byte-chunk path is kept.  Since
    `size_of::<T>()` is always a multiple of `align_of::<T>()`,
    pointer-aligned swaps never have a sub-pointer-sized tail, so the
    byte-typed `swap_nonoverlapping_short` tail path is only reachable for
    sub-pointer-aligned types (where losing a pointer's capability is the
    documented pre-existing Fil-C gap for byte-granularity access).  A sweep
    of `library/core`/`library/alloc`/`library/std` for other numeric-typed
    block buffers that move potentially-pointer bytes found none besides this
    one: `slice::rotate`'s `type BufType = [usize; 32]` scratch
    (slice/rotate.rs) is memcpy-only -- every move into and out of it is a
    T-typed `ptr::copy`/`ptr::copy_nonoverlapping`, and an alloca with only
    memintrinsic uses is never promoted or widened by SROA, so capabilities
    travel with the memcpys (its gcd-rotate temporary is a T-typed
    `MaybeUninit`-free `T`, and its swap path goes through the now-fixed
    `ptr::swap_nonoverlapping`) -- while the other array-typed `MaybeUninit`
    buffers (`iter::adapters::map_windows`' `MaybeUninit<[T; N]>` and
    `slice::sort`'s `MaybeUninit<[&mut I::Output; N]>`) store real T-typed
    values, so their allocas are typed (all-pointer, via change #21) whenever
    T may contain pointers.  Verified at `-O`: a 40-`String` `Vec`
    `rotate_left(20)` (which dispatches to rotate's Algorithm 3, i.e. the
    fixed `ptr::swap_nonoverlapping` at 24-byte element size) prints the
    rotated sequence and clones cleanly, exit 0.  See
    known-issues #1(b) for the mechanism and the before/after evidence.

23. `compiler/rustc_target/src/callconv/mod.rs` — **the Rust ABI stops
    passing pointer-maybe small aggregates as integer immediates** (the
    rustc-ABI-side fix for the union class of known-issues #10 -- the
    in-memory half of that class, the mem::swap side, was already fixed by
    change #22).  `adjust_for_rust_abi`'s `BackendRepr::Memory` branch used
    to `cast_to(Reg { kind: RegKind::Integer, size })` every sized
    Memory-repr aggregate of at most pointer size ("we want to pass small
    aggregates as immediates ... so we pick an appropriately sized integer
    type instead") -- the mode a pointer-bearing `union` lands in, because
    its overlapping fields give it a Memory repr even when it holds a
    pointer (rustc's union ABI optimization only forwards a field's scalar
    repr when ALL fields share it, and `*mut u8` vs `u64` do not).  Under
    GIMSO an integer-typed value in flight carries only the payload: the
    pointer word inside the union crossed the call boundary without its
    capability and "filc safety error: cannot read pointer with null
    object" followed on first dereference.  The fix adds
    `filc_layout_may_contain_pointers` (a TyAbiInterface-level twin of
    codegen's `filc_may_contain_pointers`, change #21: same recursion over
    fields and enum/coroutine variants) and keeps pointer-maybe small
    Memory-repr aggregates in the default `PassMode::Indirect` mode -- the
    move becomes a by-reference memory copy, which moves payload and
    capability together.  This applies to arguments AND return values (the
    same loop walks both).  Scalar/ScalarPair layouts are untouched (their
    pointer halves already travel in ptr-typed registers), numeric small
    aggregates keep the integer immediate (zero effect on ordinary
    by-value passes), and the FOREIGN ABI paths (`adjust_for_foreign_abi`)
    are deliberately NOT touched -- the SysV by-value hole for
    `extern "C"`-passed pointer-bearing unions remains documented
    (known-issues #10), because changing rustc's extern "C" behavior here
    would desynchronize it from clang-compiled C callers/callees, which
    would need matching LLVM-side work.  Validity-proven in BOTH
    directions: with only this change's predicate guard reverted (the rest
    of the patch intact), `filrustc/tests/union_memory_moves` at `-O` dies
    with "filc safety error: cannot read pointer with null object" in its
    Vec section and the by-value-return probe (`make_u(p) -> U` from an
    `#[inline(never)]` fn) dies the same way; with the guard restored
    everything passes.  The `MaybeUninit<[usize; 32]>`-scratch suspicion
    for `slice::rotate_left` (its `[usize; 32]` memmove scratch, the last
    numeric-typed buffer that receives potentially-pointer bytes) was
    investigated and is NOT reachable: `slice::rotate_left` is a `const fn`
    without `#[inline]`, so it is never inlined across CGUs and its
    scratch memcpys always keep runtime lengths (unsplittable for SROA);
    see the notes under the test table for the full sweep.

The projeny patch reproduces the build tree byte-identically: after
`filc/projeny commit`, `filc/projeny extract projects/rust.projeny /tmp/pc`
followed by `diff -rq /tmp/pc rust-build/rust` (excluding `src/llvm-project`
-- untouched: rustc no longer builds it, see "Rustc links the Fil-C LLVM from
build/" below -- and the generated `config.toml`, `build/`, `.cargo`
cargo-lock state and `__pycache__`) reports zero differences.

## Rustc links the Fil-C LLVM from build/ (no second LLVM build)

`build_rustc_impl.sh` does NOT swap Fil-C's LLVM into `src/llvm-project` (an
earlier revision did; there is no `.filc-llvm-stamp` any more) and bootstrap
does NOT build LLVM.  Instead the generated config.toml sets

    [target.x86_64-unknown-linux-gnu]
    llvm-config = "<absolute path to>/build/bin/llvm-config"
    llvm-has-rust-patches = true

With `llvm-config` configured for the host target, bootstrap's
`prebuilt_llvm_config()` returns `AlreadyBuilt` and never configures or
compiles `src/llvm-project` (which now stays exactly as the pristine tarball
shipped it); it only checks the version (needs >= 19; the Fil-C LLVM is
20.1.8) and hands the path to `rustc_llvm`'s build script, which queries
`llvm-config --cxxflags/--link-static --libs/--system-libs/--ldflags` and
links the Fil-C LLVM **statically** into `librustc_driver.so` from the 103
archives in `build/lib`.  The component query resolves the full closure
including the two MCA archives (`-lLLVMMCA -lLLVMX86TargetMCA`), which is why
`build_clang.sh` now also runs
`ninja bin/llvm-config lib/libLLVMMCA.a lib/libLLVMX86TargetMCA.a` after the
clang build (verified target names via `ninja -C build -t targets`).  The
exact query rustc_llvm makes,
`build/bin/llvm-config --link-static --libs --system-libs ipo bitreader
bitwriter linker asmparser lto coverage instrumentation x86 aarch64`,
prints 65 `-lLLVM*` archives plus `-lrt -ldl -lm` and exits 0 — note there is
no `-lz` (the Fil-C LLVM build has LLVM_ENABLE_ZLIB=OFF).  Dropping
`[llvm] targets = "X86"` is safe: that key only feeds LLVM_TARGETS_TO_BUILD
when bootstrap builds LLVM itself, which it no longer does.

The hard requirement — `patchelf --print-needed` on the built compiler shows
only libc-family libraries — is met:

* `librustc_driver-*.so`: `libm.so.6`, `libc.so.6`, `ld-linux-x86-64.so.2`
  (libz, libstdc++ and libgcc_s are GONE; the pre-external-LLVM build needed
  libz.so.1, libstdc++.so.6 and libgcc_s.so.1 in addition).
* `rust-build/rustc`: `librustc_driver-*.so`, `libc.so.6` (unchanged —
  rustc dynamically linking its own driver .so is fine).

How each offender was removed:

* **libz** — the Fil-C LLVM build already has LLVM_ENABLE_ZLIB=OFF and
  LLVM_ENABLE_ZSTD=OFF, so `llvm-config --system-libs` prints `-lrt -ldl
  -lm` and no zlib ever enters the link (the old build pulled libz because
  bootstrap built LLVM with its own defaults, LLVM_ENABLE_ZLIB=ON).
* **libstdc++** — `[llvm] static-libstdcpp = true` in config.toml makes
  bootstrap set LLVM_STATIC_STDCPP; `rustc_llvm`'s build script then emits
  `cargo:rustc-link-lib=static=stdc++` (resolving to
  `/usr/lib/gcc/x86_64-linux-gnu/13/libstdc++.a`), mirroring the Fil-C LLVM
  build's LLVM_STATIC_LINK_CXX_STDLIB=ON.
* **libgcc_s** — the stage0 std's libunwind rlib metadata carries an explicit
  `-lgcc_s` that lands in the `-nodefaultlibs` rustc link; `-static-libgcc`
  does NOT remove it (proven: it only suppresses the compiler driver's own
  implicit -lgcc_s).  The working fix is a redirect directory,
  `rust-build/gcc-redirect/`, containing a `libgcc_s.so` **linker script**
  that GROUPs the static libgcc archive(s) (`cc --print-file-name` paths) —
  `GROUP ( <libgcc_eh.a> )` on x86_64 hosts, `GROUP ( <libgcc.a>
  <libgcc_eh.a> )` on aarch64 hosts, in that order.  The GROUP contents are
  arch-aware, mirroring gcc's own `-lgcc -lgcc_eh` static-libgcc link spec:
  aarch64's libgcc_eh.a has undefined LSE outline-atomics helpers
  (`__aarch64_swp8_acq_rel` et al) that only libgcc.a defines, and every
  host-side link (bootstrap build scripts) failed with "undefined reference
  to `__aarch64_swp8_acq_rel`" until libgcc.a was listed first; archive
  semantics (members are pulled only for still-undefined symbols) make the
  order duplicate-safe.  On x86_64 the redirect is UNCHANGED (byte-identical
  single-entry GROUP with libgcc_eh.a only) because x86_64's libgcc_eh.a is
  self-contained -- see "ARM64 (aarch64) bring-up" below for the evidence.
  The `-lgcc_s` then resolves against the static libgcc archive(s), which
  are linked into librustc_driver.so with no libgcc_s.so.1 NEEDED.  Getting
  the directory
  searched *first* needs two flags, because neither alone is sufficient:
  `RUSTFLAGS="-L<dir> -Clink-arg=-B<dir>"`.  The `-L` must come from
  RUSTFLAGS directly (rustc emits `-Clink-arg`s at the very END of the linker
  command — after the `-L/usr/lib/gcc/...` that rustc_llvm's build script
  adds for the static libstdc++ — and the gcc driver builds its library
  search path in command-line order, so a -B placed there was searched too
  late; verified by strace'ing the actual `cc` link of librustc_driver.so).
  The `-B` is kept as belt-and-suspenders for links that do not echo rustc's
  -L args.  A missing libgcc_eh.a (or, on aarch64, libgcc.a) fails the
  build fast with a clear message.

Other config.toml changes the external-LLVM path required (both are [rust]
keys; without them bootstrap refuses to run):

* `codegen-tests = false` — with an externally configured LLVM, bootstrap's
  sanity check demands a FileCheck binary next to llvm-config
  (`build/bin/FileCheck`), which the Fil-C build does not produce.  We do not
  run rustc's LLVM-IR codegen testsuite (filrustc/run-tests is the Fil-C test
  suite), so this turns the requirement off.
* `llvm-tools = false` — bootstrap installs the LLVM tools (llvm-cov,
  llvm-objdump, ...) into the stage sysroot by copying them out of
  llvm-config's bindir; build/bin holds the Fil-C clang, not the LLVM tool
  suite, so that copy panics.  We do not need the sysroot llvm-tools
  component.

`rust.lld = false` stays REQUIRED (bootstrap has no in-tree lld to build with
an external llvm-config).  `llvm-has-rust-patches = true` declares that this
user-provided LLVM is a patched one (it carries the Fil-C passes); it only
flips bootstrap's LLVM_RUSTLLVM knob, which the rustc LLVM wrappers do not
read, so it is inert here but honest.

`libpas/build_llvmjit.sh` is unaffected by all of this: it globs
`../build/lib/libLLVM*.a` with `--start-group`, so the two additive MCA
archives are simply scanned and skipped (nothing references MCA symbols),
its targeted ninja invocation builds only its own JIT archives, and the
cmake configuration (RTTI/EH/zlib settings) is untouched.  The same LLVM
build tree now serves clang, the LLVM JIT shim, and rustc.

Consequence for build time: the old flow spent ~4.5 of its 8:21 clean build
in a 2668-step bootstrap LLVM build.  With the external llvm-config there is
no LLVM step at all (the build log has none): `x.py build --stage 1` alone
went 8:21 → ~3:21-3:48, and the whole `./build_rustc.sh` clean run
(extraction + stage0 download + build + test suite) is ~6 minutes wall on the
32-core container.

Size note: `librustc_driver-*.so` is ~236 MB (was ~225 MB), because the
RelWithDebInfo Fil-C LLVM archives carry full debug info; the Fil-C compiler
binaries are likewise unstripped.  Do not strip it.

## Fil-C version identification (--version)

`filrustc --version` self-identifies, clang-style:

```
rustc 1.90.0 (1159e78c4 2025-09-14) (built from a source tarball) (Fil-C 0.687 git@github.com:pizlonator/fil-c.git 47ea0dd0b9e767cf4b8d93ebe9a9903378981ac4)
```

The parenthetical mirrors clang/lib/Basic/Version.cpp, which appends
`(Fil-C 0.687 git@github.com:pizlonator/fil-c.git <hash>)` to
`clang version ...`.  Mechanism (see change #16):

* build_rustc.sh reads the Fil-C version NUMBER from clang's
  `#define FILC_VERSION "0.687"` in clang/lib/Basic/Version.cpp (the single
  source of truth for the Fil-C version; versioning-checklist.md lists every
  place that must change on a bump), composes
  `Fil-C <version> <git origin URL> <HEAD hash>` (the URL/hash come from git,
  the same information clang bakes in via its VCSVersion.inc), and exports it
  as the `FILC_VERSION` environment variable before invoking x.py.
* `compiler/rustc_driver_impl/src/lib.rs` (in the projeny patch) reads it with
  `option_env!("FILC_VERSION")` and appends ` (Fil-C ...)` to the version line
  printed by `version_at_macro_invocation` — the choke point for both
  `rustc --version` and the first line of `rustc -vV`.  With the variable
  unset (a vanilla build of the tree) nothing is appended.
* Because cargo does not notice when the *value* of an environment variable
  changes, build_rustc.sh keeps a stamp (rust-build/.filc-version-stamp) with
  the last-built value; when it differs, it touches
  compiler/rustc_driver_impl/src/lib.rs to force cargo to recompile that one
  crate and relink rustc, then records the new value.  An unchanged value is a
  fast no-op.

## How the built rustc finds filcc

The Fil-C host target spec sets the linker to the bare name
`filcc` (the Fil-C clang driver): x86_64-unknown-linux-gnu, or its aarch64
twin aarch64-unknown-linux-gnu since the ARM64 bring-up (see that section
below).  Upstream rustc resolves a bare-name linker
through the PATH, which used to mean every `filrustc` invocation needed
Fil-C's `build/bin` on PATH.  The built rustc now resolves `filcc` itself (see
change #20, `filc_resolve_linker` in
`compiler/rustc_codegen_ssa/src/back/link.rs`), trying in order:

1. **The directory baked in at build time** — `option_env!("FILC_LINKER_DIR")`.
   build_rustc_impl.sh composes it right after putting
   `$FILC_ROOT/build/bin` on PATH, so in the normal **pizfix mode** checkout
   it is `<fil-c-checkout>/build/bin`; in **optfil mode** (filcc installed at
   `/opt/fil/bin`) it is `/opt/fil/bin`; in **pizlix mode** (filcc only on
   PATH in the chroot) it is wherever the chroot had filcc when the compiler
   was built.  `<dir>/filcc` is used if it exists (plain `.exists()`: an
   existing-but-not-executable candidate is still returned, because the spawn
   error rustc then reports is more informative than silently falling through
   to another filcc).  The variable is unset in a vanilla build of the tree,
   in which case this arm is simply absent.
2. **Checkout discovery** — walk up from the running rustc binary
   (`std::env::current_exe()`, which canonicalizes the `rust-build/rustc`
   symlink to the real stage1 binary) and at each ancestor directory `D` use
   `D/build/bin/filcc` if it exists.  This is the Fil-C checkout layout
   (filcc lives in `<checkout>/build/bin`, next to the `<checkout>/pizfix` it
   drives -- the "../build/bin relative to pizfix" rule), so a moved or
   reinstalled checkout is found regardless of where `rust-build` lives under
   it and without rebuilding the compiler.
3. **`/opt/fil/bin/filcc`** — the optfil install location.
4. **PATH** — if nothing above matched, the bare name `filcc` falls through to
   rustc's normal PATH lookup: that is **pizlix mode**, or rustc's usual
   `error: linker `filcc` not found` if there is no filcc anywhere.

The resolution happens in `link_natively` right after the linker path is
determined, so everything downstream sees the absolute path: the actual
spawn, `--print link-args`, and the linker error messages.

Mode table:

| Mode    | filcc lives at                    | How rustc finds it                          |
| ------- | --------------------------------- | ------------------------------------------- |
| pizfix  | `<checkout>/build/bin`            | baked `FILC_LINKER_DIR` (or the walk in #2) |
| optfil  | `/opt/fil/bin`                    | baked `FILC_LINKER_DIR` (or #3)             |
| pizlix  | wherever PATH says                | fall-through to PATH (#4)                   |

**Precedence when several filcc candidates exist:** a baked or discovered
filcc wins over whatever is on PATH.  That is correct by construction -- the
baked filcc is the one from the checkout that built this rustc and the std it
links against -- but it means a compiler built in pizfix or optfil mode
ignores a pizlix-style PATH-provided filcc (verified: with
`/tmp/path-filc/filcc` on PATH and the baked dir present, `--print link-args`
shows the baked `<checkout>/build/bin/filcc`).  Use
`-C linker=/path/to/filcc` to override explicitly; any `-C linker=` value
other than the exact bare name `filcc` is never rewritten.  A compiler built
without `FILC_LINKER_DIR` (or after its baked filcc disappeared) discovers
the checkout via the walk in #2, then `/opt/fil/bin`, then PATH.

The same stamping caveat as for `FILC_VERSION` applies (option_env! is
expanded when the rustc_codegen_ssa crate compiles and cargo does not notice
value changes), so build_rustc_impl.sh keeps a parallel stamp
(rust-build/.filc-linker-dir-stamp): when the value differs from the last
build (or the stamp is missing) it touches
`compiler/rustc_codegen_ssa/src/back/link.rs` to force a recompile and
relink, and when filcc is not on PATH at all it drops any previously baked
value the same way.

## The rustc driver statically embeds the Fil-C LLVM (version skew)

`librustc_driver-<hash>.so` is linked against `llvm-config --link-static`:
the LLVM code the shipped rustc runs is a **snapshot of `build/lib` taken at
link time** -- the same archives `build/bin/clang` runs, but frozen into the
driver (`stage1/bin/rustc` is a tiny launcher that dlopens
`$ORIGIN/../lib/librustc_driver-*.so`).  Cargo does not fingerprint those
archives: only crate sources and build-script outputs are fingerprinted, and
the LLVM libraries are neither -- they reach the link through llvm-config at
rustc_llvm's build-script run time.  So `./build_rustc_incremental.sh`
happily relinks nothing after a rebuild of the Fil-C LLVM, and the shipped
rustc keeps executing **stale LLVM code** -- including stale Fil-C
miscompile fixes -- while `build/bin/clang-20` next to it runs the fresh
one.

That actually happened (found in review round three of the SROA-side
miscompile-fix attempt, which was later abandoned -- see known-issues #1 for
that history): the driver had been linked at 08:47,
`build/lib/libLLVMScalarOpts.a` was rebuilt at 13:15 with a changed SROA, and
`./build_rustc_incremental.sh` did not relink.  The shipped driver still
contained upstream SROA while `build/bin/clang-20` next to it ran the fresh
one, so rustc and clang disagreed about LLVM behavior -- a hazard that is
general to ANY change in the archives the driver embeds (today that includes
the RegisterFilPasses pipeline itself, i.e. "every frontend runs the full
Fil-C pipeline-start block": an incremental `./build_rustc_incremental.sh`
after a `./build_clang.sh` that rebuilt those archives would otherwise keep
shipping a rustc whose pipeline differs from what its sources say).

Two defenses now, both around build_rustc_impl.sh:

* **The LLVM-freshness stamp (rust-build/.filc-llvm-libs-stamp).**  Right
  after the llvm-config existence check, the script computes the LLVM input
  state the driver was last linked against: the newest `libLLVM*.a` archive
  in `build/lib` as `%T@:%s` (mtime:size; only `libLLVM*.a` counts -- the
  driver links the LLVM component archives plus the MCA ones, while clang's
  own static libraries churn on every clang-side edit and must not trigger
  rustc relinks), plus the newest header mtime under `llvm/include` and
  `build/include` (header changes change the LLVM API the rustc_llvm
  wrappers compile against).  When the stamp is missing or its recorded
  state differs, the script touches
  `rust-build/rust/compiler/rustc_llvm/llvm-wrapper/PassWrapper.cpp`
  (forcing cargo to recompile rustc_llvm and relink the driver), rewrites
  the stamp, and prints
  `Fil-C LLVM libraries changed: relinking the rustc driver against the updated LLVM archives ...`.
  An unchanged LLVM tree makes this a fast no-op: the two `find` scans take
  ~20ms.  A missing stamp counts as changed, so the first run of this
  script revision relinks once; from then on the stamp tracks the tree.
* **Manual verification recipe.**  With llvm/lib/Transforms/Scalar/SROA.cpp
  back at upstream (see known-issues #1), there are no Fil-C SROA
  LLVM_DEBUG literals to grep for anymore; the SROA-side literals
  ("Fil-C: re-typing memcpy-only partition [", "Fil-C: keeping memcpys for
  memcpy-only partition [", "Fil-C: keeping memtransfer; the access ",
  "Fil-C: keeping memset; the access type ") must be absent from BOTH
  clang-20 and the shipped driver, which is what makes
  `strings -a stage1/lib/librustc_driver-*.so | grep -c 'Fil-C:'` == 0 a
  usable (weak, negative) skew check.  The load-bearing checks are
  source-level: the `check_grep` assertions in build_rustc_impl.sh verify
  the rustc-side fix and the std swap fix in the extracted tree, and
  `strings -a stage1/lib/librustc_driver-*.so | grep -c
  FILCRUST_FILCINLINE` must be 0 now that the IsRust gate is REMOVED from
  RegisterFilPasses.cpp (a nonzero count would mean a driver linked against
  a stale LLVM that still contained the gate's `getenv("FILCRUST_FILCINLINE")`
  probe; the string is gone from llvm/lib/Passes/RegisterFilPasses.cpp).
  The behavioral guards for the LLVM the driver actually embedded are the
  `threads-opt` test (the spawn+join shape, which the rustc-side typed
  allocas keep correct) and the `swap_nodes` test (the mem::swap shape,
  which the std pointer-typed swap chunk buffers keep correct) -- both run
  through the shipped rustc at -O, where the full pre-pizlonation block now
  runs for rustc too.

To force a relink by hand (e.g. when iterating on LLVM changes that touch
neither headers nor archives the stamp sees): `touch
rust-build/rust/compiler/rustc_llvm/llvm-wrapper/PassWrapper.cpp` and re-run
`./build_rustc_incremental.sh` -- or delete the stamp and re-run, which the
mechanism itself turns into the same relink.

## Verification matrix (all from /tmp; `filrustc` alone suffices)

Though Fil-C build/bin on PATH still works -- see "How the built rustc finds
filcc" above.

* `filrustc --version` → `rustc 1.90.0 (1159e78c4 2025-09-14) (built from a
  source tarball) (Fil-C 0.687 git@github.com:pizlonator/fil-c.git <fil-c HEAD
  hash>)`; the first `-vV` line is the same and the `LLVM version: 20.1.8`
  line is still present.
* Dynamic dependencies: `patchelf --print-needed
  rust-build/rust/build/x86_64-unknown-linux-gnu/stage1/lib/librustc_driver-*.so`
  → exactly `libm.so.6 libc.so.6 ld-linux-x86-64.so.2`; `patchelf
  --print-needed rust-build/rustc` → `librustc_driver-*.so libc.so.6`.  See
  "Rustc links the Fil-C LLVM from build/" above for how libz/libstdc++/
  libgcc_s were removed.
* `llvm-config` queries (the exact ones rustc_llvm's build script makes):
  `build/bin/llvm-config --version` → 20.1.8; `--components` lists x86 and
  aarch64 (plus the non-target components); `--link-static --libs
  --system-libs ipo bitreader bitwriter linker asmparser lto coverage
  instrumentation x86 aarch64` → 65 `-lLLVM*` archives (including
  `-lLLVMMCA -lLLVMX86TargetMCA`) plus `-lrt -ldl -lm` (no -lz), and every
  archive exists in build/lib; `--cxxflags` has no `stdlib=libc++` (so
  LLVM_STATIC_STDCPP's assert holds) and `--host-target` is
  x86_64-unknown-linux-gnu.
* `hello.rs` (`println!`) — debug and `-O` (plain rustc spells --release as
  `-C opt-level=3`/`-O`): prints "hello filc rust", exit 0 both.
* Unsafe OOB read (`*ptr.add(1000000000)` past a 3-element `Vec<i32>` wrapped
  in `black_box`) — debug and `-O`: "filc safety error: cannot read pointer
  with ptr >= upper." + "filc panic: thwarted a futile attempt to violate
  memory safety." + Fil-C semantic-origin backtrace; exit 133 (SIGTRAP, core
  dumped — Fil-C's normal behavior, same as C).
* Safe panic (`v[10]`) and `unwrap()` on `Err` — both print the full Rust
  panic message ("thread 'main' panicked at ...: index out of bounds: the len
  is 3 but the index is 10" / "called `Result::unwrap()` on an `Err` value:
  \"boom\"") and then unwind out of `main` on the Fil-C runtime's unwinder;
  `lang_start_internal`'s `catch_unwind` catches it and std exits with the
  Rust panic exit code **101**.  NO abort spew, NO "filc user error: Not
  implemented", and no `_Unwind_GetIP` anywhere.
* `-C panic=abort` — links and runs against the unwind std (the sysroot
  carries both `libpanic_unwind-*.rlib` and `libpanic_abort-*.rlib`; rustc
  injects panic_abort for `-C panic=abort`): hello exits 0; a safe panic
  prints its message and aborts through Fil-C's normal user-error path
  ("filc user error: abort(3) called" + "filc panic: user thwarted
  themselves.", exit 133).
* `-C panic=unwind` — this is now the DEFAULT (the target spec no longer
  forces abort); an explicit `-C panic=unwind` flag compiles and runs like
  the default.
* `catch_unwind` works (debug and `-O`): a panicking closure is caught and
  its payload downcasts back to the original `&'static str` message; Drop
  glue runs for every frame the panic unwinds through (innermost guard
  first); a panicking spawned thread is joined as `Err` with the payload
  intact; a panic out of `main` exits 101.  See "Panics" below and the
  filrustc test suite (catch_unwind, catch_unwind-opt, drop_unwind,
  thread_panic_join, safe_panic, safe_panic_abort).
* Mixed-stack exceptions (spot checks, from /tmp with a Rust `staticlib` +
  `clang++`): a Rust panic declared `extern "C-unwind"` unwinds into C++ and
  a C++ `catch (...)` catches it (execution reaches `__cxa_end_catch`); the
  program then aborts with upstream Rust's documented
  "fatal runtime error: Rust panics must be rethrown, aborting" — exactly
  what upstream Rust does when FFI catches a Rust panic without rethrowing.
  Conversely, a C++ exception thrown through a Rust frame is NOT caught by
  Rust's `catch_unwind` (Fil-C phase-1 decisions are final, so foreign
  exceptions keep unwinding — see "Panics" below); it propagates to the C++
  handler above the Rust frame, which catches it and execution continues
  normally.  Plain `extern "C"` (not `C-unwind`) Rust functions abort a
  panic at the boundary with `panic_cannot_unwind`, as upstream does.
* Cross-DSO: `filrustc --crate-type dylib --edition 2021 lib.rs --crate-name
  mylib -o libmylib.so -C prefer-dynamic` links; `nm -D libmylib.so | grep -c
  pizlonated_` = 5: `pizlonated__ZN5mylib3add17h052cc356082cdec9E` and
  `pizlonated__ZN5mylib10get_answer17h43de340481e07935E` are DEFINED (T),
  and the three remaining pizlonated_ entries are undefined references to
  libstd getter symbols (`pizlonated__RNvCsugZpHxZGsu_7___rustc17rust_begin_unwind`,
  `pizlonated__ZN4core9panicking11panic_const24panic_const_add_overflow...`,
  and `pizlonated_rust_eh_personality`)
  that resolve at load time from the exported getters of `libstd-*.so` —
  there are no phantom undefined imports (references to symbols that exist
  nowhere).  The client
  (`fn main() { println!("{}", mylib::add(2, 40)); println!("{}",
  mylib::get_answer()); }`) compiled with `--extern
  mylib=libmylib.so -C prefer-dynamic` links and runs, printing "42" twice,
  exit 0.  Finding libstd-*.so at run time needs either
  `-C link-arg=-Wl,-rpath,<sysroot>/lib/rustlib/x86_64-unknown-linux-gnu/lib`
  or LD_LIBRARY_PATH pointing there (ld-fil1 honors both; it does not search
  the sysroot by default).  `libstd-*.so` itself now exports 2070
  `pizlonated_*` symbols (including `pizlonated_rust_eh_personality`) and has
  zero phantom undefined imports.
* Threads regression: 8 threads x 1000 increments via `Arc<Mutex<u64>>` prints
  "sum = 8000", debug and `-O`, exit 0 — the spawn+join shape stays correct
  because of the rustc-side typed allocas (verified by hand at `-O` and
  `-C opt-level=3`, and by the `threads` and
  `threads-opt` tests), including at `-O`, where the full pre-pizlonation
  block now runs for rustc (see known-issues #1).
* `-C no-prepopulate-passes` still pizlonates: a hello-sum program compiled
  with it links, runs and prints the right answer, exit 0 — proving the
  `FilCAddPipelineStartPasses` direct-MPM path (the NoPrepopulatePasses
  branch of LLVMRustOptimize) applies the Fil-C passes including the
  FilPizlonator when no PassBuilder pipeline is built.
* A normal build compiles and runs fine for pizlonator-friendly code (hello
  at -O prints and exits 0): above O0 the Fil-C rustc runs the same
  pipeline-start block as clang -- KillUB -> pre-pizlonation
  simplification+inlining -> coro lowering -> FilPizlonator -- before the
  regular optimization pipeline (see known-issues #1).  The FILCRUST_FILCINLINE
  environment knob is gone together with the IsRust gate that used to keep
  the pre-pizlonation block off for rustc; the block now runs unconditionally
  for every frontend.
* CPU feature detection (`is_x86_feature_detected!`) — debug and `-O`, exit 0,
  no filc safety error.  A sweep program queries ALL 74 `@FEATURE`-queryable
  names of `library/std_detect/src/detect/arch/x86.rs` (plus the 4
  `@BIND_FEATURE_NAME` aliases `abm`, `avx512gfni`, `avx512vaes`,
  `avx512vpclmulqdq`, which route to the same Feature bits) and every answer
  cross-checks against the `flags` line of `grep -m1 flags /proc/cpuinfo`,
  both positives and negatives (on the test machine, AMD Ryzen AI MAX+ 395:
  48 yes / 26 no, zero mismatches — including the round-3 additions
  tsc/gfni/vaes/vpclmulqdq: yes, and the negative avx512fp16: no, the
  machine has no `avx512_fp16`).  See known-issues #7 for the history.

## config.toml (written by build_rustc_impl.sh)

change-id = 144675; [build] target/extended/tools/optimized-compiler-builtins
as before; [llvm] download-ci-llvm = false, optimize = true, assertions =
true, ninja = true, **static-libstdcpp = true** (links libstdc++ statically —
the dynamic libstdc++.so.6 must not appear in librustc_driver.so's NEEDED),
and NO `targets` key (that only feeds LLVM_TARGETS_TO_BUILD when bootstrap
builds LLVM itself, which it no longer does); [rust] llvm-libunwind = "no",
lld = false (required with an external llvm-config), jemalloc = false,
optimize = true, debug = false, lto = "off", std-features =
["panic-unwind", "std_detect_file_io"] (std_detect_file_io is what lets
std_detect's x86 backend read /proc/cpuinfo — note that setting
std-features REPLACES bootstrap's default `["panic-unwind"]`, so panic-unwind
is repeated), **codegen-tests = false** and **llvm-tools = false** (both
required by the external-LLVM path — see "Rustc links the Fil-C LLVM from
build/" above); and the appended

    [target.x86_64-unknown-linux-gnu]
    llvm-config = "<FILC_ROOT>/build/bin/llvm-config"   (absolute)
    llvm-has-rust-patches = true

(The [target] section is keyed on the host triple, which build_rustc_impl.sh
derives from `uname -m`: `[target.x86_64-unknown-linux-gnu]` on x86_64
hosts, `[target.aarch64-unknown-linux-gnu]` on aarch64 hosts; see "ARM64
(aarch64) bring-up" below.)

The impl script writes the [target] section separately because the heredoc
for the rest is quoted and the llvm-config path must expand (and MUST be
absolute: bootstrap joins relative [target] paths onto the rust source
root).

## Panics: unwinding on the Fil-C runtime's unwinder

The target spec keeps the default (unwind) panic strategy: **Rust panics
unwind on the Fil-C runtime's unwinder**.  libpizlo, the Fil-C runtime that
`filcc` links into every binary, implements `_Unwind_RaiseException` itself:
it walks the filc frames, runs each frame's personality routine in a search
phase to find the handler, then re-runs it at each frame's landing pad in a
cleanup phase.  The personality routine is `rust_eh_personality`
(`library/std/src/sys/personality/gcc.rs`), implemented against libpizlo's
pizlonated landing-pad LSDA API (`filc/include/pizlonated_eh_landing_pad.h`):
the Fil-Pizlonator lowers rustc's classic GNU EH IR (`invoke`,
`landingpad { i8*, i32 }` with clauses, `resume`) into that form, so the
personality walks a flat action list (`pizlonated_eh_landing_pad_get_*`)
instead of parsing DWARF bytes and doing IP math.  Details:

* **No `_Unwind_SetIP`.**  The runtime enters the landing pad itself; the
  pad's `landingpad` instruction returns the two values the personality
  installed with `_Unwind_SetGR` (element 0 = exception object, element 1 =
  selector).  Installing a pad = set the two GRs + `_URC_INSTALL_CONTEXT`.
  The stubs for `_Unwind_GetIP`/`_Unwind_SetIP`/`_Unwind_GetRegionStart`/
  `_Unwind_Backtrace` in libpizlo abort with `zerror`, and the personality
  never touches them (the DWARF path in the same file is kept for other
  targets but is compiled out here).
* **Phase-1 decisions are final.**  libpizlo FILC_CHECK-fails if a frame that
  the search phase found tries to `resume` instead of installing in phase 2,
  and there is no rethrow-from-a-handler path.  Two consequences:
  - `catch_unwind`'s catch-all pad (`landingpad catch ptr null`, in
    `__rust_try`) only claims exceptions whose `exception_class` is Rust's
    own (`u64::from_ne_bytes(*b"MOZ\0RUST")`, the same class panic_unwind
    stamps).  A **foreign exception (e.g. a C++ one) keeps unwinding past
    Rust catch-alls**, so a C++ handler further up the stack catches it.
    Documented deviation: `catch_unwind` does not catch foreign exceptions
    (upstream cannot honor them either — its `__rust_foreign_exception`
    path rtaborts, which under Fil-C could not even be reached from a
    handler frame, since a phase-1-found frame must install).
  - In the cleanup phase, catch clauses only install at the frame the search
    phase found (`_UA_HANDLER_FRAME`); cleanups (Drop glue) install at every
    frame, for foreign exceptions too; filters (the empty-filter terminate
    pads rustc emits at `extern "C"` boundaries) install except during forced
    unwinding.
* **Panic exit codes.**  A panic unwinding out of `main` is caught by
  `lang_start_internal`'s `catch_unwind` and std exits **101**.  With
  `-C panic=abort` the message is printed and the abort goes through Fil-C's
  normal user-error path ("filc user error: abort(3) called" + "filc panic:
  user thwarted themselves.", exit 133, 42 under the test harness).  A panic
  with no handler anywhere still reaches `rtabort!("failed to initiate
  panic, error 5")` (the `_URC_END_OF_STACK` path), but normal programs never
  hit it since `main` is always wrapped.
* **`RUST_BACKTRACE=1` is still unsupported**: std's backtrace machinery
  calls `_Unwind_Backtrace`, which libpizlo stubs with
  `zerror("Not implemented.")` (`filc/src/unwind.c`) — the program prints
  "filc user error: Not implemented." and dies (133).  Panics still print
  Fil-C's own semantic-origin backtrace, which is better anyway.  (Also
  documented in known-issues #4/#6 below.)
* **`__rust_panic_cleanup` uses the Rust ABI.**  Upstream declares it
  `extern "C"` with a fat-pointer return (`*mut (dyn Any + Send)`, behind
  `#[allow(improper_ctypes)]`).  Under Fil-C that cannot work: rustc lowers
  improper types in `extern "C"` signatures to their C-ABI representation
  (`{i64, i64}` for a two-pointer fat pointer), so the vtable half of the
  caught payload loses its capability and reading the payload's type id trips
  a filc safety check ("cannot read pointer with null object").  The projeny
  patch changes `__rust_panic_cleanup` to the Rust ABI on all three sides
  (std's declaration, panic_unwind's and panic_abort's definitions); the Rust
  ABI keeps the real two-pointer type, which the Fil-Pizlonator passes with
  capabilities intact.  This is an internal `rustc_std_internal_symbol`
  (std <-> panic runtime), so nothing external depends on the ABI.  Note the
  general rule this implies: **fat pointers cannot cross `extern "C"`
  boundaries under Fil-C** — that is true of any Rust/C FFI code, not just
  the panic machinery (C never has fat pointers, so only
  `#[allow(improper_ctypes)]` code is affected).
* `-C panic=abort` still links and runs (the panic_abort rlib is in the
  sysroot; rustc injects it for the abort strategy) — covered by the
  `safe_panic_abort` test.
* Mixed-stack exception interop was spot-checked by hand (see the
  verification matrix): a Rust panic → C++ `catch (...)` works through the
  unwinder and then hits upstream's "Rust panics must be rethrown" abort when
  C++ fails to rethrow, and a C++ exception passing through Rust frames is
  not claimed by any Rust pad.  Plain `extern "C"` Rust functions abort a
  panic at the boundary (`panic_cannot_unwind`), as upstream does.

## Startup: how Fil-C Rust binaries start

The old blocker "std startup trips a filc safety check in pthread_getattr_np"
is FIXED.  Root cause: rustc always links with `-nodefaultlibs`, which makes
Fil-C's driver (filcc) pick `pizfix/lib/filc_mincrt.o` instead of
`filc_crt.o`.  The mincrt `main` calls
`filc_start_program(argc, argv, NULL, pizlonated_main)`, i.e. glibc's
`__libc_start_main` never runs, so the main thread's Fil-C thread state is
never initialized.  The fix is in the target spec: `-normalcrt` is added to
the pre-link args, which tells filcc to use the full `filc_crt.o` even under
`-nodefaultlibs`.  Binaries start through `start_program` ->
`__libc_start_main` -> `__libc_start_call_main` -> `main`, and `pthread_self()`,
`pthread_getattr_np`, threads (`std::thread::spawn`), etc. all work.

The second startup blocker was std's stack-overflow machinery calling
`sigaltstack`, which pizfix rejects -- fixed by the stack_overflow.rs no-op
tweak listed above.

## Cross-DSO (dylib) linking

Fixed by #7 + #8 + #11 above.  A dylib (`--crate-type dylib`) now exports the
pizlonated getters (`pizlonated_<mangled>` for every exported function/global
plus the FI/FIP/ET variants) and contains no phantom undefined imports, so a
separately compiled executable can link it with
`--extern mylib=libmylib.so -C prefer-dynamic` and run.  This also fixed
`libstd-*.so` in the sysroot for `-C prefer-dynamic` consumers (which need
`-C link-arg=-Wl,-rpath,...` or LD_LIBRARY_PATH to find it at run time, since
ld-fil1 does not search rustc's sysroot by default).

## Known issues / limitations

1. **The pre-pizlonation simplification + inlining pipeline miscompiled
   rustc-shaped IR.**  This used to be why the pipeline was disabled under
   rustc via an IsRust parameter in llvm/lib/Passes/RegisterFilPasses.cpp
   (`if (Level != OptimizationLevel::O0 && FilCOptimize &&
   (!IsRust || getenv("FILCRUST_FILCINLINE")))`).  Both manifestations are
   now fixed at their sources, the IsRust parameter is GONE, and the full
   pipeline-start block (KillUB -> pre-pizlonation
   simplification+inlining -> coro lowering -> FilPizlonator) runs for every
   frontend at every level above O0:

   **(a) FIXED ON THE RUSTC SIDE: the spawn+join / pthread_t shape.**  The
   fix is change #21 in the list above: the Fil-C rustc gives pointer-maybe
   aggregates typed allocas whose LLVM type is an all-pointer struct, so
   SROA's type-driven partitioning inherits pointer types for the partitions
   that carry those bytes.  llvm/lib/Transforms/Scalar/SROA.cpp is
   byte-identical to HEAD, i.e. upstream SROA plus the pre-existing Fil-C
   Skew hack (an earlier, abandoned fix rewrote SROA itself, and a later
   SROA-side guard for known-issue #13 was REJECTED by the user and
   reverted -- see known-issue #13 and change #29 for why the fix lives on
   the rustc side instead: "clang does the right thing ... it's a rustc
   bug, and the fix is to do what clang does").  With the clang-parity
   union handling of change #29 in place, SROA never even sees the
   pointer-maybe allocas (they are escaped with zhas_union before the
   optimizer runs), so the typed allocas are belt-and-braces for the
   partitioning machinery rather than its input.  Evidence that this shape is
   closed: `threads-opt` -- the exact formerly-miscompiling program
   (`std::thread::spawn` + `join`, 8 threads x 1000 `Arc<Mutex>`
   increments) at `-O` -- passes, and with
   `-C llvm-args=--debug-only=sroa` (the pre-pizlonation block now runs for
   rustc unconditionally; the traces below are how the typed allocas feed
   SROA in that block), the 24-byte
   `io::Result<JoinHandle<()>>` allocas appear as
   `alloca <{ ptr, ptr, ptr }>, align 8` (packed all-pointer struct --
   `filc_ptr_struct_type` output) with the pthread-word partition split to
   `Rewriting alloca partition [16,24) to: alloca ptr` and the 24-byte
   memcpy widened to a capability-carrying `store ptr`.

   **(b) FIXED IN STD: the mem::swap class.**  Independently
   reproduced by the reviewer (3/3) and re-verified on this tree: a Vec of
   40-byte `Node { name: String, next: *mut u8, id: u32 }` values built
   from a `#[inline(never)]` constructor, shuffled pairwise with
   `std::mem::swap` (`#[inline(never)]` wrapper), then cloned and read:
   at `-O` the program used to die in the Vec::clone -> String::clone memmove
   with "filc safety error: cannot read source pointer with ptr < lower"
   (exit 133, or exit 42 under `FILC_EXIT_ON_PANIC=1` -- the Fil-C panic
   exit codes), while `-C opt-level=0` and `-O -C llvm-args=-filc-optimize=0`
   both printed the correct output and exited 0.  Even a bare
   `std::mem::swap::<String>` miscompiled at `-O` (the swapped String's
   data pointer was later read with a destroyed capability), so this was the
   mem::swap class, not one element shape.  Mechanism: std's
   `swap_nonoverlapping` -> `swap_nonoverlapping_bytes`
   (library/core/src/ptr/mod.rs) moved the element bytes through
   `swap_chunk` on `MaybeUninit<[u8; N]>` block buffers, where
   `N = CHUNK_SIZE = size_of::<*const ()>()` = 8 -- NUMERIC-typed allocas.
   Upstream SROA promoted those allocas and widened their typed byte
   load/stores into integer (i64) load/store pairs; under GIMSO semantics
   integer loads/stores move only the payload, so the capabilities of the
   pointers inside the swapped element were dropped at the swap (the
   pizlonator itself is correct: aggregate/byte copies move payload+shadow,
   integer moves touch only payload).  The rustc-side typed-allocas fix (a)
   could not cover this class because `filc_may_contain_pointers` correctly
   declines NUMERIC types -- `MaybeUninit<[u8; 8]>` is one -- so those
   allocas stayed untyped byte arrays.  The fix is in std itself now
   (library/core/src/ptr/mod.rs, change #22 in the list above):
   `swap_nonoverlapping_bytes` takes `align_of::<T>()` and, whenever the
   swapped regions are pointer-aligned (`align >= CHUNK_SIZE`; the
   function's precondition guarantees both pointers are aligned to
   `align_of::<T>()`), swaps pointer-sized chunks as
   `MaybeUninit<*const ()>` instead of `MaybeUninit<[u8; CHUNK_SIZE]>` --
   the copies become pointer-typed, and the pizlonator moves each word's
   capability alongside its payload for pointer-typed accesses, so the
   swap stays capability-preserving however far SROA/instcombine reshape
   it (pointer-typed accesses are never integer-widened).  This is the
   Fil-C house pattern for scratch memory that receives whole elements --
   the user libc retypes exactly such buffers as arrays of pointers (musl's
   qsort `unsigned char tmp[256]` became a pointer-typed scratch, and the
   int/pointer unions in pthread types were split so pointer slots never
   alias int slots).  Because `size_of::<T>()` is always a multiple of
   `align_of::<T>()`, pointer-aligned swaps never have a sub-8-byte tail,
   so every byte of such a swap moves through a pointer-typed chunk; types
   with a smaller alignment (e.g. `repr(packed)`) keep the upstream
   byte-chunk path, where a pointer losing its capability remains the
   documented pre-existing Fil-C gap for byte-granularity access (see
   `#[repr(packed)]` below).  Re-landing the SROA-side guard that also
   closes the class (without the std change) was still rejected for
   performance (see "Why the SROA-side fix was abandoned" below: ~1.4x on
   simdutf, because it pointer-types the memcpy-only partitions of NUMERIC
   aggregates too, making the pizlonator emit pointer checks for plain
   bytes); that guard lives on the `fil/fixed_sroa` branch and is NOT in
   this tree.  The regression guard for the std fix is the `swap_nodes`
   test (see the test table below), which passes now that the
   pre-pizlonation block runs for rustc, and was verified in BOTH
   directions: with the std fix temporarily reverted (byte-identical to
   upstream) and the gate removed, `swap_nodes` at `-O` dies with "filc
   safety error: cannot read source pointer with ptr < lower"; with the fix
   restored it prints the right answer and exits 0.

   Where the fix's shape comes from (the user-libc house patterns): the
   Fil-C adaptation of musl (projects/usermusl -- the initial commit is
   stock musl, every later commit is a Fil-C adaptation) rewrites every
   place that moves potentially-pointer bytes through integer-typed or
   byte-typed memory so that the bytes stay pointer-typed in the IR.  The
   patterns, in descending relevance to this fix: **(P1)** may-hold-pointer
   scratch that receives whole elements is retyped as pointer storage --
   musl's qsort `unsigned char tmp[256]` byte scratch became a
   `void *tmp = zgc_alloc(width)` with one whole-element memcpy
   (src/stdlib/qsort.c), because a whole-element memcpy moves
   payload+shadow while byte/word-chunked copies through integer-typed
   memory do not; **(P2)** int/pointer unions are split so pointer slots
   never alias int slots -- `pthread_mutex_t`/`pthread_cond_t`/`barrier`
   got dedicated `__p[]` pointer arrays disjoint from the `__i[]` int
   arrays (include/alltypes.h.in), and `jmp_buf` became `{ void* __p; }`;
   **(P3)** every byte copy of possibly-pointer data goes through
   `__builtin_memcpy`/`zmemmove_builtin`, which the pizlonator lowers to
   shadow-propagating memmove (src/string/memcpy.c deleted its
   `__may_alias__ u32` word-copy body; arch memcpy.S files deleted);
   **(P4)** pointers that must travel as data live in pointer-typed fields
   with runtime accessors (`void* zthread`, `zthread_self_cookie()`), never
   laundered through `uintptr_t`.  The std swap fix uses P1/P2: the swap's
   chunk scratch (`MaybeUninit<[u8; 8]>`, the std equivalent of qsort's
   `tmp[256]`) becomes `MaybeUninit<*const ()>` whenever the swapped
   regions are pointer-aligned, so the moved words are pointer-typed and
   their capabilities travel with them.  The remaining user-libc patterns
   (byte buffers the KERNEL fills, address-as-number bookkeeping like
   `pthread_attr_getstack`'s `_a_stackaddr`, `zsys_*` marshalling below the
   capability boundary) stay byte/integer-typed on purpose -- those bytes
   genuinely never hold Fil-C pointers.

   What the spawn+join shape looked like: Fil-C clang runs an extra
   pre-pizlonation pipeline above O0 (early SimplifyCFG/SROA/EarlyCSE, then
   a full inliner with
   SROA/EarlyCSE/JumpThreading/CVP/SimplifyCFG/InstCombine/GVN/SCCP/ADCE) to
   create inlining opportunities for the pizlonator's check elimination.  A
   plain `std::thread::spawn` + `join` compiled at -O handed `pthread_join` a
   thread handle whose flight-pointer capability was null ("filc safety
   error: cannot read pointer with null object" inside
   `__pthread_clockjoin_ex`, exit 133).  Every one of the
   pre-pizlonation passes reproduced it *independently* once the inliner's own
   inner SROA was accounted for, because none of them is the culprit.

   Root cause (mechanism; SROA is upstream and still does this, which is why
   the fix is on the rustc side): **SROA re-typed memcpy-only alloca
   partitions as integers.**  rustc's `Builder::spawn_unchecked::<F, T>`
   (monomorphized into the crate's CGU) returns `io::Result<JoinHandle<T>>`
   (24 bytes; the `pthread_t` sits at offset 16) through a chain of five
   `llvm.memcpy`s, with ptr-typed accesses only at offsets 0 and 8 (the
   `Option` niche check and the Err path).  SROA's `rewritePartition` types a
   partition that has *no* typed load/store uses -- i.e. bytes only ever
   moved by memcpy/memmove + lifetime intrinsics -- with the integer fallback
   `Type::getIntNTy(P.size() * 8)`, and rewrites the memcpys into
   integer-typed load/store pairs (bit-preserving for LLVM).  Under GIMSO
   semantics that is NOT semantics-preserving: the Fil-Pizlonator lowers
   memcpy/memmove as payload+shadow moves (a pointer at rest carries its
   capability in the shadow), while integer loads/stores touch only the
   payload.  The `pthread_t` word's capability was therefore dropped at the
   copy, and `JoinInner::<T>::join`'s `load ptr` at offset 16 read
   {payload, null capability} -- exactly the observed failure
   (gimso_semantics.md:61, "Loading a pointer from a location that never had
   a pointer stored to it yields a pointer with a null capability").  The
   pizlonator itself is correct per the GIMSO spec and SROA is correct per
   LLVM's own semantics: the bug is that the *composition* (running SROA
   before pizlonation) changes GIMSO-observable behavior.  It is not
   rustc-specific -- any frontend IR with partially-typed memcpy-copied
   structs could hit it.

   Why clang-shaped IR never trips it: clang's allocas are *typed*, and
   commit 4a54ae4076b4 ("Have clang convert unions that have any pointers
   into pointer structs") makes clang represent any union which may contain
   pointers as an all-pointer struct.  SROA partitions a struct-typed alloca
   by the struct's field types, so the partitions of a clang alloca that
   holds pointer words are already pointer-typed: memcpy-only slices over
   them get ptr-typed copies (capability-preserving) instead of the integer
   fallback.  rustc's allocas, by contrast, are untyped byte arrays
   (`alloca [N x i8]`) -- there are no field types for SROA to inherit, so
   every memcpy-only partition collapses to iN and drops the capabilities.
   A unions-only rustc fix would not have been enough: the `pthread_t` that
   starts this whole story lives in a *struct* field of the *enum*
   `io::Result<JoinHandle<()>>`, and Rust enums are unions in all but name
   (each variant is a disjoint view of the same bytes), so the predicate must
   cover every aggregate that may contain pointers, not just `union`s.

   The abandoned SROA-side attempt (history; the code lives on the
   `fil/fixed_sroa` branch and is NOT in this tree): the first fix put the
   classification inside LLVM (llvm/lib/Transforms/Scalar/SROA.cpp,
   `SROA::rewritePartition`): under `DL.isFilC()`, a partition whose uses are
   *only*
   memcpy/memmove/lifetime/droppable intrinsics (no typed accesses; memsets,
   PHIs and selects disqualify; every memtransfer must be word-aligned at the
   piece offset on both sides; the partition must be a whole number of aligned
   words; the alloca must live in address space 0, which is all the
   pizlonator models) is typed as `ptr` (one word) or `[N x ptr]`
   (multi-word) instead of an integer.  A single-word partition then gets
   ptr-typed copies -- bit-identical to the integer ones but
   capability-preserving -- and a multi-word partition keeps its memcpys
   (`EmitMemCpy` stays true because the array type is not a single value
   type).  This composes with SROA's existing Fil-C machinery:
   `canConvertValue` already refuses i64<->ptr conversions under the `-ni:0`
   (non-integral-pointer) datalayout, and the pre-existing Fil-C "Skew" hack's
   FIXME ("A better way of detecting that it's got no type. Probably, it
   should be based on whether all uses are mem transfers.") describes exactly
   this classification.

   Round two (the reviewer found two more holes, both closed by a
   rewriter-level guard plus a conjunction rewrite of the classification
   gate -- all on the abandoned branch):

   * **Mixed typed/memcpy (and memset/memcpy) partitions still miscompiled.**
     A partition containing BOTH a typed access (`store i64 0`) and memcpys
     that move pointer bytes was disqualified from the memcpy-only re-typing
     (typed access ⇒ not memcpy-only), fell back to `alloca i64`, and the
     rewriter then widened BOTH memcpys into integer load/store pairs --
     dropping the capability.  The memset variant (a `llvm.memset` instead of
     the typed store) died the same way: the memset also disqualifies the
     memcpy-only predicate, the partition got the integer re-typing, and the
     memcpys got widened.  The old "memset-mixed partitions keep the integer
     path (benign)" claim was FALSE whenever pointer bytes flow through the
     memcpys.  **The whole class was closed at the rewriter level**
     (llvm/lib/Transforms/Scalar/SROA.cpp, AllocaSliceRewriter::
     visitMemTransferInst/visitMemSetInst): under `DL.isFilC()`, a
     memtransfer/memset use is widened into typed accesses ONLY when the
     emitted access type is pointer-typed (`Ty->isPtrOrPtrVectorTy()`, i.e.
     the capability-preserving ptr-words path); otherwise the use stays a real
     memory intrinsic on the re-typed alloca (adjusted pointer), exactly like
     the original IR.  (Round three tightened both halves of this rule: the
     memset was ALWAYS kept, and "pointer-typed" became "a single pointer",
     `isPointerTy()` -- see the round-three bullet below.)  An alloca with any memintrinsic use is never promoted
     (`isAllocaPromotable` refuses memory intrinsics), so the alloca memory
     survives the rewrite and the typed uses (real loads/stores) and the
     intrinsics (real memcpys/memsets) operate on it with original GIMSO
     semantics -- correct by construction for any mix of uses.  One upstream
     invariant had to be relaxed for this: the rewriter asserts
     `CanSROA` whenever integer/vector widening was planned (`VecTy || IntTy`)
     and a use was not eliminated; a deliberately-kept memintrinsic now sets a
     flag (`FilCKeptMemIntrinsic`) that relaxes exactly that assert, since the
     partition intentionally does not get promoted.  The partition-level
     machinery is kept as defense in depth: memcpy-only partitions whose
     memcpys fail the alignment gates (unaligned, including merely sub-word
     `align 4` annotations, which LLVM IR permits) get pointer words when
     whole-word aligned and an i8 array otherwise -- an array is not a single
     value type, so the rewriter leaves every use as a memcpy, and the
     pizlonator's memcpy/memmove lowering preserves capabilities for all
     alignments.
   * **The memcpy-only predicate was order-dependent, not a conjunction.**
     The old `MemTransferOnly` boolean was last-writer-wins (every memtransfer
     set it true, every disqualifier set it false), so identical use sets
     classified differently depending on slice visitation order: for the use
     set {memset [0,8), memcpy [0,8), memcpy [0,16)} one partition logged
     "keeping memcpys" (true despite a memset) while a same-shaped partition
     took the non-memcpy-only path -- a compile-order-dependent soundness
     outcome.  The gate is now a real conjunction of monotone flags:
     `FilCMemTransferOnly = SawUse && NoTypedAccesses && SawMemTransfer &&
     !SawDisqualifier`, where `NoTypedAccesses`/`MemTransferAligned` are
     clear-only (once false, never set back to true) and `SawMemTransfer`/
     `SawDisqualifier` are set-only (once true, never cleared), so no flag can
     be overridden by a later slice and the outcome is independent of use
     order and SROA iteration order.  Verified on the abandoned branch with
     the exact shape above as two allocas in one function (`mixed_order.ll`,
     one of the now-removed .ll tests): both partitions took
     the identical path in every SROA iteration (`-mllvm
     -debug-only=sroa`).  Scope caveat on that test: at -O2 `stash` is inlined
     into `main`, and the decisive SROA runs there see the memsets already
     converted to `store i64 0` (the surviving uses take the typed-mixed
     rewriter path, "Fil-C: keeping memtransfer; the access type i64"), so the
     test's pass does NOT by itself discriminate the old last-writer-wins
     predicate from the conjunction -- with the rewriter-level guard in place
     both classifications produce correct code for that shape.  The
     order-independence was verified from the debug traces (both IR orders
     classify identically), and the memset-in-the-classifier variant of the
     shape was covered by `mixed_memset_memcpy.ll` (also removed with the
     branch).

   * **The round-two memset guard was incomplete for pointer-vector slices
     (legal IR aborted the compiler), and vector-of-pointer slices could
     still widen their memtransfers.**  Both round-two guards keyed on "not
     pointer-typed" via `isPtrOrPtrVectorTy()`, so a `<2 x ptr>`-typed slice
     slipped through both.  For a memset that was fatal in a second way: the
     slice's `VecTy` widening path in `visitMemSetInst`'s `CanContinue`
     returns true unconditionally (bypassing `canConvertValue`) and then
     builds an integer splat for the pointer-vector slice --
     `convertValue`'s `canConvertValue(DL, OldTy, NewTy) && "Value not
     convertable to type"` assertion fired, because under the `-ni:0`
     datalayout an integer does not convert to a non-integral pointer.  A
     reviewer-reproduced ~35-line module (a partition covered by a
     `llvm.memset` plus a `<2 x ptr>` load) aborted `clang -O2 -x ir` on
     exactly that assert (`SROA.cpp:2058`) -- the shape is documented here
     because it is a property of upstream SROA's vector-widening path under
     `-ni:0`, not of the abandoned patch.  The rules on the abandoned branch
     were (SROA.cpp, `visitMemSetInst`/`visitMemTransferInst`): under
     `DL.isFilC()` a **memset use is ALWAYS kept whole** -- never widened
     into a typed store, whatever the slice type -- because GIMSO semantics
     make a memset zero both the payload and the shadow, and no typed store
     can express that: an integer store leaves stale shadow behind (words the
     memset was supposed to null out stay live in the shadow), while a
     pointer/pointer-vector store cannot represent the integer splat under
     `-ni:0` at all (`canConvertValue` refuses it -- and the fast-path return
     above would otherwise skip that check and trip the assert).  Keeping the
     intrinsic is the only uniformly correct choice for every access type,
     and it is also the fast one: the pizlonator's `filc_memset` lowering
     zeroes payload and shadow together, which is exactly the path Fil-C
     always intended memsets to take.  And a **memtransfer use is widened
     only when the emitted access type is a single pointer**
     (`isPointerTy()`, the ptr-words path): the ptr-words machinery only ever
     re-types memcpy-only partitions as `ptr` or `[N x ptr]` -- never as a
     vector -- so a vector-of-pointer slice has no proven
     capability-preserving widening story and keeps its memcpys whole too.
     (The `[N x ptr]` array form already took the `EmitMemCpy` path, since an
     array is not a single value type, so it was never affected.)

   About which partitions reach the integer fallback at all (upstream-SROA
   mechanism, relevant both to why the miscompile happened and to why the
   rustc-side fix is shaped the way it is): under the Fil-C datalayout, the
   native-integer spec is `n8:16:32:64`, and `DL.isLegalInteger` is governed
   by that spec -- the `i128:128` entry in the same string is only an
   alignment entry, not a legality one.  So only partitions of at most 8
   bytes reach the `getIntNTy` fallback; a 16-byte partition (128 "bits",
   which are not a legal integer here) keeps its original array type via
   getTypePartition instead, and its uses stay memory ops either way.  The
   8-byte `pthread_t` word at offset 16 of the 24-byte
   `io::Result<JoinHandle<()>>` is exactly an iN-reachable partition, which
   is why the miscompile fired there.

   **Why the SROA-side fix was abandoned.**  It was correct (the filrustc
   suite passed with it), but it regressed simdutf ~1.4x: the classification
   ptr-typed every word-aligned memcpy-only partition *in the whole
   compiler*, including those of purely NUMERIC aggregates (clang allocas
   for `uint64_t`-bearing structs are typed but their memcpy-only partitions
   still hit the integer fallback path the patch rewrote).  Pointer-typed
   partitions make the pizlonator emit pointer checks (null checks,
   capability moves) for data that is just bytes -- so numeric code got
   slower across the board.  The rustc-side fix has no such blast radius: it
   types only the allocas of aggregates that *may contain pointers*
   (`filc_may_contain_pointers`), so numeric aggregates keep their untyped
   byte-array allocas and upstream SROA's integer widening of their
   partitions -- which is semantics-preserving for data with no capabilities
   in the shadow -- is untouched.  The SROA-side fix lives on the
   `fil/fixed_sroa` branch (including its three review rounds and its .ll
   tests); this branch's SROA delta is limited to the pre-existing Fil-C
   Skew hack -- llvm/lib/Transforms/Scalar/SROA.cpp is byte-identical to
   HEAD (the known-issue #13 fix moved to the rustc side, change #29, so
   NUMERIC aggregates' integer widening and every well-aligned pointer
   aggregate see exactly upstream SROA behavior).

   The rustc-side fix (change #21 above; all in the projeny patch):

   * `filc_may_contain_pointers` (rustc_codegen_llvm/src/type_of.rs): a
     conservative transitive predicate over the memory representation --
     scalars that are pointers (raw pointers, references, function pointers,
     `Box`) make it true, `ScalarPair` if either half is a pointer, SIMD
     vectors never, and memory-backed aggregates recurse over their fields;
     for multi-variant enums/coroutines it also checks every variant's
     layout, since a variant's payload fields do not appear in the
     top-level field list.  Unsized layouts answer false (they cannot be
     alloca'd, and `TyAndLayout::field` misbehaves on shapes like
     `dyn Trait`).  Results are cached per `CodegenCx` (`filc_ptr_maybe`,
     keyed like `type_lowering` by `Ty` plus variant index -- a variant's
     answer must not poison the enum's or a sibling variant's), with the
     RefCell copied out before recursion so re-entrant calls cannot
     conflict.
   * `filc_ptr_struct_type` (same file): returns the LLVM alloca type for a
     stack slot of `layout` -- `{ ptr x N }` **packed** (clang's exact
     construction from CGRecordLayoutBuilder.cpp, minus a byte-array tail
     that is always empty here) -- iff the allocation is exactly the value's
     size, non-zero, a multiple of 8 bytes, 8-byte ABI-aligned, and the
     predicate above says pointer-maybe.  Otherwise `None` and the caller
     falls back to the untyped byte array.  The guards keep the clang hack's
     safety argument: a typed alloca smaller/larger than the value would
     make SROA's out-of-type slices ill-defined or lose tail bytes;
     `repr(packed)` aggregates with pointers at non-8-aligned offsets are
     excluded (SROA would produce nonsense partitions; upstream falls back
     to integers for those slices anyway).  The struct is size- and
     alignment-preserving (`words * 8 == size` asserted).
   * `alloca_for_layout` (new `BuilderMethods` trait method,
     rustc_codegen_ssa/src/traits/builder.rs): the DEFAULT does the plain
     untyped `alloca(size, align)` -- so every other backend (and every
     non-typed case) is byte-for-byte unchanged -- and the LLVM backend
     overrides it (rustc_codegen_llvm/src/builder.rs) to emit the all-ptr
     struct alloca when `filc_ptr_struct_type` returns a type, mirroring
     `alloca`'s entry-block placement and alignment handling exactly.
     Because it is a `BuilderMethods` default-impl override, ONE hook covers
     every MIR-codegen alloca: `PlaceValue::alloca_size`
     (rustc_codegen_ssa/src/mir/place.rs -- which `PlaceRef::alloca` calls)
     routes through `bx.alloca_for_layout`, so all ~20 `PlaceRef::alloca`
     call sites are covered -- MIR locals and temps (mir/mod.rs), function
     arguments and their indirect-scratch slots (mir/block.rs
     `make_space_for_arg` et al), return places (block.rs), debuginfo spill
     slots (mir/debuginfo.rs), va_list slots (mir/mod.rs), and the assorted
     rvalue/call/intrinsic temps -- plus the direct `alloca_for_layout`
     calls for the `RValue::Cast` transmute temp (mir/rvalue.rs, typed with
     `operand.layout`, so an enlarging cast keeps the untyped alloca via the
     exact-size guard) and the two `make_space_for_arg` argument scratch
     allocas that need `required_align` (mir/block.rs).  Everything else
     (constants, statics, the aggregate's own `llvm_type`) is deliberately
     untouched: the alloca type is consulted only by SROA's type-driven
     partitioning -- all field accesses are byte-offset GEPs plus per-field
     typed loads/stores through opaque pointers, and memcpys are
     address-based -- which is exactly the clang hack's mechanism.

   Why this is GIMSO-correct: SROA types partitions from the ALLOCA's struct
   fields (each field is a `ptr`, so a memcpy-only slice over field
   boundaries gets `ptr`-typed or stays a memcpy of the whole struct), and
   ptr-typed copies preserve capabilities by GIMSO's definition (the
   pizlonator moves payload+shadow for pointer copies).  For a multi-word
   pointer struct that stays one partition, the array/struct type is not a
   single value type, so the memcpys stay memcpys entirely -- also
   capability-preserving.  Numeric aggregates fail the predicate, keep the
   untyped alloca, and get upstream behavior: zero impact.

   Evidence (all re-verified on this branch): `./filrustc/run-tests` is
   18/18 SUCCESS including `threads-opt` -- exactly the formerly-miscompiling
   program (`std::thread::spawn` + `join`, 8 threads x 1000 `Arc<Mutex>`
   increments) at `-O`, plus the same program by hand at `-O` and
   `-C opt-level=3` ("sum = 8000", exit 0) -- and including `swap_nodes`,
   the reviewer's mem::swap repro at `-O`, which now passes with the
   pre-pizlonation block RUNNING for rustc (verified in both directions:
   with the std fix of change #22 temporarily reverted -- byte-identical to
   upstream -- and the gate removed, it fails with "filc safety error:
   cannot read source pointer with ptr < lower"; see (b) above).  With
   `-C llvm-args=--debug-only=sroa` (the pre-pizlonation block now runs for
   rustc unconditionally -- the traces below are how the typed allocas feed
   SROA in that block), the 24-byte
   `io::Result<JoinHandle<()>>` allocas appear as
   `alloca <{ ptr, ptr, ptr }>, align 8` (packed all-pointer struct --
   `filc_ptr_struct_type` output) and their partitions split to ptr-typed
   allocas: `JoinInner::<()>::join`'s pthread-word partition is
   `Rewriting alloca partition [16,24) to: ... = alloca ptr, align 8`, the
   24-byte memcpy
   into it is widened to `store ptr` (capability-carrying), and the
   flight-pointer load stays `load ptr`; the corresponding
   `spawn_unchecked` scratch allocas keep whole-struct partitions for their
   memcpy-only slices.  There are no "Fil-C:" SROA debug lines at all
   (upstream SROA has none) -- the fix is entirely rustc-side.  Numeric
   aggregates still get untyped allocas (the trace is full of
   `[16 x i8]`/`i8 x N`/`i64`/`i32` partitions), and simdutf -- the
   benchmark that exposed the abandoned fix's regression -- benchmarks at
   its usual numbers with this toolchain (the rustc-side fix does not touch
   LLVM, and clang allocas were already typed via 4a54ae4076b4, so
   clang-compiled code cannot see any difference by construction).

2. **ThinLTO is unsupported.**  The ThinLTO pre-link (bitcode writer) +
   post-link round-trip hollows out pizlonated codegen units because rustc's
   export-symbol lists handed to LTO still use pre-pizlonation names while the
   module's symbols are renamed (`pizlonated_*`).  Symptom: `.text 0x0` CGU
   objects, "undefined reference to `pizlonated_main`".  Plain compiles never
   use LTO any more (the `Session::lto()` default is `Lto::No`, and the
   Fil-Pizlonator already runs a full inliner inside every pipeline, so no-LTO
   codegen is still fully optimized).  An explicit `-C lto=thin|fat` overrides
   the default and fails at link time with exactly the error above -- that is
   the expected, documented failure, not a crash.  A real fix would translate
   the export list (and/or make the allocator module join the LTO unit) -- or
   make the pizlonator ThinLTO-round-trip stable.
3. **Fat pointers cannot cross `extern "C"` boundaries** (rustc lowers
   improper types in C-ABI signatures to `{i64, i64}`-style representations,
   and the capability halves are lost).  The one place std itself did this
   (`__rust_panic_cleanup`) was switched to the Rust ABI in the projeny patch
   (see "Panics" above).  User FFI code with `#[allow(improper_ctypes)]`
   fat-pointer signatures would hit the same wall.  Unwinding itself —
   `-C panic=unwind` (the default), `catch_unwind`, Drop glue during
   unwinding, and thread-panic join — all work (see "Panics" above), with
   the documented deviation that `catch_unwind` does not catch foreign
   exceptions (they keep unwinding, which is what makes Rust/C++ mixed-stack
   exception handling work at all).
4. **dl_iterate_phdr / backtrace**: std's backtrace crate enumerates loaded
   libraries from /proc/self/maps (pizfix has no dl_iterate_phdr).  Panics
   print Fil-C's own semantic-origin backtrace, which is better anyway.
5. **Fil-Pizlonator crashes on `llvm.x86.xgetbv` with a non-constant
   argument** (pre-existing Fil-C bug, NOT worked around any more — the
   detectors no longer use `xgetbv`, see change #5).  Pizlonating a
   function whose `xgetbv` call receives a run-time value trips a
   use-after-destroy assertion in the Fil-Pizlonator (`Value::~Value`:
   "materialized_use_empty").  Exact repro with plain Fil-C clang — note
   the `#include <immintrin.h>`, without which this is just a plain C
   error (undeclared `_xgetbv`) and NOT a pizlonator bug:
   `printf '#include <immintrin.h>\nunsigned long long f(unsigned i){return _xgetbv(i);}' | build/bin/clang -O2 -mxsave -x c - -c`
   crashes at -O1/-O2/-O3 (not at -O0).  The same crash hits Fil-C rustc
   on `core::arch::x86_64::_xgetbv(0)` at any optimization level (rustc
   does not inline core's `#[inline]` wrapper at O0, so the intrinsic
   call's operand is a function parameter).  With a CONSTANT argument the
   C path is fine: `_xgetbv(0)` with the header compiles cleanly at
   -O0..-O3, links, runs, and prints the correct XCR0 value, and the
   `Intrinsic::x86_xgetbv` case is explicitly whitelisted in the
   pizlonator.  Fixing the pizlonator
   (llvm/lib/Transforms/Instrumentation/FilPizlonator.cpp) remains the
   real fix.
6. **libpizlo is missing** `_Unwind_GetIPInfo`, `_Unwind_GetDataRelBase`,
   `_Unwind_GetTextRelBase`, `_Unwind_FindEnclosingFunction`,
   `dl_iterate_phdr`, and it stubs `_Unwind_GetIP`, `_Unwind_SetIP`,
   `_Unwind_GetRegionStart` and `_Unwind_Backtrace` with aborting
   `zerror("Not implemented.")` calls.  The unwinder-related gaps are all
   worked around above: the Fil-C personality routine is written against the
   pizlonated landing-pad API and never needs IP math (the runtime enters
   landing pads itself), and `RUST_BACKTRACE=1` still fails inside the
   `_Unwind_Backtrace` stub (known issue #4).  The remaining
   `libunwind.rs`/backtrace stubs exist so other (non-Fil-C) target paths
   still compile.
7. **`is_x86_feature_detected!` / std_detect**: WORKS under Fil-C.  It was
   previously broken: the upstream CPUID/xgetbv inline asm is Intel
   dialect, which the pizlonator rejects, so every feature query aborted at
   run time ("filc safety error: cannot handle inline asm (only AT&T
   dialect inline assembly is supported)").  It is now implemented by
   parsing /proc/cpuinfo (see change #5), and its answers cross-check
   against /proc/cpuinfo's `flags` line including negatives.  Coverage is
   COMPLETE: the `FLAG_TO_FEATURE` table maps EVERY one of the 74
   `@FEATURE`-queryable features of `detect/arch/x86.rs` — there are no
   intentionally-unmapped ones (round 3 added the last four: `tsc`, `gfni`,
   `vaes`, `vpclmulqdq`, whose kernel flag names are identical to the
   feature names).  The kernel flags are identity names except for the
   documented aliases in the table (sse3=`pni`, cmpxchg16b=`cx16`,
   ermsb=`erms`, sha=`sha_ni`, lzcnt=`abm`, kl=`keylocker`,
   widekl=`wide_kl`, avx512er=`avx512_er`, avx512pf=`avx512_pf`,
   avx512ifma=`avx512ifma`, avx512vbmi=`avx512vbmi`, avx512vbmi2=
   `avx512_vbmi2`, avx512vnni=`avx512_vnni`, avx512vpopcntdq=
   `avx512_vpopcntdq`, avx512bitalg=`avx512_bitalg`, avx512bf16=
   `avx512_bf16`, avx512fp16=`avx512_fp16`, avx512vp2intersect=
   `avx512_vp2intersect`, avxvnni=`avx_vnni`, avxvnniint8=`avx_vnni_int8`,
   avxvnniint16=`avx_vnni_int16`, avxifma=`avx_ifma`, avxneconvert=
   `avx_ne_convert`; the amx_* entries are identity).  A feature the kernel
   does not report (because the CPU lacks it or the OS did not enable its
   XCR0 state) simply answers "no" — under-reporting, never over-reporting.
   The generic
   `core::arch::x86::__cpuid`/`__cpuid_count` intrinsics remain unusable
   (they still abort at run time — the pizlonator's cpuid whitelist
   requires EBX to be declared, which rustc forbids on x86_64), and other
   `core::arch` intrinsics that expand to inline asm may have the same
   limitation.  Actual vector codegen under the Fil-C ABI is Fil-C's
   business and untested beyond that.
8. Temporary debug prints were removed; if you need to trace DL/optimization
   issues, the two useful hooks are: print at the top of `LLVMRustOptimize`
   (compiler/rustc_llvm/llvm-wrapper/PassWrapper.cpp) and
   `MachineFunction::init`'s isCompatibleDataLayout assert
   (llvm/lib/CodeGen/MachineFunction.cpp).  After touching LLVM sources:
   ./build_clang.sh rebuilds the Fil-C LLVM (llvm-config/MCA archives
   included), then force the rustc relink with
   `touch rust-build/rust/compiler/rustc_llvm/llvm-wrapper/PassWrapper.cpp`
   (cargo does not track the LLVM archives) and re-run
   ./build_rustc_incremental.sh.
9. **proc-macro crates don't work.**  `--crate-type proc-macro` builds (with
   a warning that `panic=abort` is being overridden for the cdylib; in our
   minimal stage1 sysroot the `proc_macro` crate is not found implicitly —
   pass `--extern proc_macro=<sysroot>/lib/rustlib/x86_64-unknown-linux-gnu/lib/libproc_macro-*.rlib`),
   but the resulting cdylib has no `.rustc` metadata section (`readelf -S`
   finds none), so any attempt to *use* it fails with "invalid metadata"
   errors.  Additionally, a pizlonated cdylib likely could not be dlopen'd
   by the non-Fil-C host rustc anyway (it would need the pizfix runtime, and
   its pizlonated symbol names would not match what a stock rustc's
   proc-macro server expects).  Consequence: proc-macro/derive crates must
   be built with the host toolchain.  That is harmless for Fil-C targets:
   proc macros run at COMPILE time on the host, not inside the pizlonated
   program.  (Where the missing `.rustc` section comes from has not been
   investigated.)

24. `compiler/rustc_ty_utils/src/layout.rs` — **word-aligned variant
    payloads for pointer-maybe direct-tagged enums** (the std::process
    spawn fix; see known-issue #11 below for the full mechanism and the
    validity evidence).  `layout_of_uncached` computes the layout with the
    upstream repr; if the result is direct-tagged, pointer-maybe (the
    `filc_layout_may_contain_pointers` predicate, a `TyAbiInterface`-level
    twin of codegen's `filc_may_contain_pointers` from change #21), and the
    first payload byte of any variant is not word-aligned, the layout is
    recomputed with `repr.int = Fixed(I64, true)` (which also inhibits the
    niche optimization -- moot, the enum is already direct-tagged) and
    `repr.align = 8`, so the FIRST payload byte of every variant starts on a
    word boundary.  That is exactly what the misalignment check inspects
    (`filc_payload_misaligned` looks at each variant's `offset(0)` payload
    field only), and it is deliberately NOT a claim about later fields: a
    multi-field variant whose non-first fields need only 4-byte alignment
    still has them at 4 mod 8 after the recompute
    (`#[repr(u8)] enum { A(Box<String>), B(u32, u32) }` has an 8-byte tag with
    B's fields at 8 and 12, and
    `#[repr(u8)] enum { A(Box<String>), B(u64, u32, u32) }` is not even
    recomputed -- its first payload byte is already word-aligned -- leaving
    field .2 at 20).  That is safe because those trailing fields are only ever
    accessed with INTEGER-typed loads and stores, and under GIMSO integer-typed
    accesses neither need nor touch capabilities: an integer-typed store moves
    only the payload (it cannot fabricate a capability, and it leaves whatever
    capability rests in the shadow untouched), and an integer-typed load reads
    only the payload.  The rewriter cannot even widen such an access into a
    pointer-typed one: under the `-ni:0` non-integral data layout,
    SROA's `canConvertValue` refuses every `iN -> ptr` conversion, so the
    rewriter keeps the original integer-typed memory op on the kept alloca
    instead of re-typing it -- which is what preserves GIMSO's
    integer-lane/pointer-lane split (this is the load-bearing reason the
    misaligned trailing fields, and integer-typed accesses inside
    pointer-typed alloca partitions generally, are benign; probed empirically
    through the `?`-desugaring, sret returns, struct splits, and Vec moves at
    both optimization levels).  What the check MUST prevent is a POINTER
    landing at 4 mod 8: a pointer-typed access requires a word-aligned
    capability slot, which is what the first-payload-byte check guarantees
    for every variant (niche layouts put the pointer AT the niche, also
    word-aligned).
    WHICH enums get the word-sized tag on WHICH path: reprs without an
    explicit integer tag (repr(Rust), and repr(C) with no `int`) get it on
    the FIRST computation -- the `get_discriminant_type` closure widens the
    tag to `I64` there, which is consistent with typeck because such reprs
    typeck as pointer-sized (`ReprOptions::discr_type()` is
    `IntegerType::Pointer`) -- so the common `io::Result`-shaped enums pay
    no second layout computation.  Explicit-int-repr pointer-maybe enums
    (`#[repr(u8)]`, `#[repr(i16)]`, `#[repr(u32)]`, `#[repr(C, u8)]`,
    `#[repr(u64)]`, ...) keep their chosen tag on the first computation and
    get the word-sized tag from the recompute above exactly when that
    computation lands on a misaligned payload (they are always
    direct-tagged, so the misalignment check sees them).  The closure
    decides the tag against the repr the current computation is using (a
    `Cell<ReprOptions>`; `ReprOptions` is `Copy`) -- deciding it against
    `def.repr()` instead is WRONG twice over: on the first computation it
    trips rustc_abi's "layout decided on a larger discriminant type (I64)
    than typeck (I8/I16/I32)" assert (the calculator compares the closure's
    tag against `from_attr(repr.discr_type())` of the repr IT was given,
    and explicit-int reprs typeck as the narrow int), and in the recompute
    the discriminant range it is handed is derived from the bumped repr's
    interpretation of the raw `Discr.val` bits (e.g. `-5i16` is stored as
    `0xFFFB` and is re-interpreted at 64-bit width as 65531), which
    overflows the narrow repr's range and trips
    "Integer::repr_discr: `#[repr]` hint too small".  With the closure
    against the current repr both are consistent: the tag is never larger
    than typeck's, and the recomputed tag's valid range covers the raw bit
    patterns that codegen's SwitchInt compares against.  A repr whose
    payload is already word-aligned (e.g. `#[repr(i16)]` with a String
    payload at 8, or `#[repr(C, u8)]` whose tag prefix grows to the 8-byte
    field alignment) is left exactly as upstream laid it out -- no recompute,
    no ABI drift.  Only direct-tagged enums are touched: niche layouts
    already keep the pointer *at* the niche (word-aligned), packed enums
    keep their layout (the repr(packed) hole), and pure-numeric enums are
    unaffected (the predicate requires pointer-maybe fields), so there is
    no impact on numeric code.  The bump recompute cannot make a layout
    unsized (the bump only widens the discriminant and the alignment; the
    calculator hardcodes `sized: true` for every multi-variant enum repr),
    so upstream's sanity assert -- `if !maybe_unsized && layout.is_unsized()
    { bug!("got unsized layout for type that cannot be unsized ...") }` --
    holds for the recomputed layout too, and this round RESTORED it after
    the recompute (it had been dropped when the recompute was added; the
    full suite plus a fresh rustc+std build with assertions on pass with it
    back in place, which is the validity proof for the assert itself).
    Regression guards: `repr_enum_u8`, `repr_enum_c_u8`, `repr_enum_i16`,
    `repr_enum_u32` plus their `-opt` twins (see their rows in the test
    table and the coverage table) pin the explicit-int-repr shapes -- they
    fail to COMPILE (the rustc_abi assert, exit 101) if the closure bump is
    ever again applied to explicit reprs on the first computation, and they
    RUN both variants of each enum through by-value moves and print the
    payloads, proving the integer-only payloads at 4 mod 8 do not
    pizlonator-panic.

25. `library/std/src/io/mod.rs` + `library/std/src/fs.rs` +
    `library/std/src/sys/fd/unix.rs` — **the fs::read / read_buf capability
    fix** (see known-issue #12 below).  THREE std files are touched:

    * `io/mod.rs`: `default_read_to_end` reads through a fixed-size plain
      stack buffer (`[0u8; DEFAULT_BUF_SIZE]` per chunk) and appends with
      `extend_from_slice` (a memcpy moves payload + capability shadow)
      instead of reading through the vector's spare capacity, whose pointer
      loses its Fil-C capability on the way through this function's
      aggregate moves (the kernel write through it reported "cannot write
      pointer with null object", and large `read_to_string` silently
      returned an empty string).
    * `fs.rs`: `fs::read::inner` uses the infallible `Vec::with_capacity`
      instead of `Vec::try_with_capacity` (whose try-reserve aggregate move
      carried the vector pointer as a bare integer); under Fil-C the
      allocator aborts on OOM anyway, so the graceful allocation-failure
      path is moot.
    * `sys/fd/unix.rs`: `File::read_buf` no longer hands the kernel the
      cursor's raw `MaybeUninit` slice (the cursor's data pointer travels
      through the `BorrowedBuf`/`BorrowedCursor` aggregate moves and the
      vector's spare capacity, and its capability does not survive the
      trip: the read syscall reported "cannot write pointer with null
      object" from `zsys_read`).  It now reads through a fixed-size plain
      byte scratch buffer (`FILC_READ_BUF_SCRATCH = 8192`) and
      `cursor.append()`s the bytes -- a memcpy, which moves payload +
      shadow -- into the cursor.  The contract that follows: every
      `read_buf` call on a `File` transfers AT MOST 8192 bytes even when
      the cursor's capacity is larger (capping the transfer is within the
      `Read` contract -- reads may legitimately return fewer bytes than
      requested), so callers must LOOP; std's own `io::copy` loops (the
      stack-buffer loop and the BufWriter loop) already do, and the
      `fs_big_rw` suite test pins the cap from stable code (8 chunks of
      exactly 8192 for a 64 KiB file).  The extra copy per read_buf call
      is the price of the capability-preserving rewrite: the Fil-C version
      needs a fixed-size, known-typed staging buffer whose pointer comes
      straight from the stack, because the cursor's pointer is exactly
      what loses its capability.

    BufReader over a File never needed the fix (it reads through the plain
    `read` path).

26. `filrustc/tests/*.rs` + `filrustc/tests/*.manifest` — **the std coverage
    suite**: 40 new `.rs` tests (see the std coverage section below for the
    module map; the follow-up round added 7 more coverage-table rows --
    the `repr_enum_*` change-#24 guards and the `io_terminal`/
    `chown_fchown`/`backtrace_disabled` gap-closers -- bringing the table
    to 47 rows, and the round-3 sort-miscompile fix added the
    `sort_by_key_enum`/`sort_by_comparator` guards for 49), all runnable
    since known-issue #13 was fixed (the rustc-side clang-parity fix of
    change #29 un-skipped `net_tcp`/`net_unix` -- the skip notes became
    root-cause comments in their manifests), and the known-issue-#13 round
    added the six union-mirror tests for 55 `.rs` rows (see the coverage
    table).  Each test compiles at exactly ONE optimization
    level because the harness's `optFlags` REPLACES the default flags
    (recountable programmatically from the manifests)
    (before the `-opt` twins of this round, 12 of the 30 runnable
    coverage tests compiled at `-O` and 18 at the default O0 -- and the
    earlier revision of this entry wrongly claimed "30 runnable at both the default opt level and
    `-O`").  Every bug-guard test now has BOTH levels via `-opt` twins
    (byte-identical `.rs` copies whose manifests add `optFlags: -O`):
    `process_ops`/`process_ops-opt` (the known-issue #11 spawn-shape guard)
    and `fs_ops`/`fs_ops-opt` + `fs_big_rw`/`fs_big_rw-opt` (the
    known-issue #12 read-path guards) -- both bugs fired at BOTH
    optimization levels before their fixes, so both levels must be guarded.
    All tests are hermetic: filesystem tests work under a per-test
    `/tmp/filc-...-<pid>` directory created and removed by the test itself,
    networking is loopback-only with ephemeral ports, timing assertions use
    generous margins, and outputs are deterministic (sorted where hashing
    or concurrency is involved).  The bug-guard twins this round:
    `process_ops`/`process_ops-opt` (known-issue #11), `fs_ops`/
    `fs_ops-opt` + `fs_big_rw`/`fs_big_rw-opt` (known-issue #12), and
    `option_result_combinators`/`option_result_combinators-opt`
    (known-issue #14 -- its `?`-desugaring shape only miscompiles at `-O`,
    so the `-O` twin is the regression guard and the O0 original keeps the
    combinator coverage).

27. `library/core/src/result.rs` — **`?` on a `Result` with a
    pointer-bearing error type keeps the error payload's capability**
    (the `?`-desugaring fix; see known-issue #14 below for the full
    mechanism and the validity evidence).  `Result::branch` and
    `Result::from_residual` (the two `try_trait_v2` methods the `?`
    desugaring calls) are `#[inline(never)]` instead of `#[inline]`: when
    they were inlined, the inlined copies became locals whose shared
    niche-overlap partition SROA promotes to an integer register, and the
    `Err` payload's pointer then travels through `ptrtoint`/`inttoptr`,
    arriving with its Fil-C capability dropped ("filc safety error: cannot
    read pointer with null object" on the first deref of an `Err(String)`
    payload propagated by `?`, at `-O` only).  As real calls, the residual's
    aggregate moves are memcpy-across-a-call, which moves payload +
    capability together.  `?` on `Option<T>` needs no guard (its residual
    `Option<Infallible>` is uninhabited and zero-sized, so no
    pointer-bearing payload ever shares a partition with the `Continue`
    value -- verified empirically), and `from_output` stays `#[inline]`
    (it handles only the `Ok` path).  The cost is two small real calls per
    `Result`-typed `?` (a discriminant switch plus one aggregate copy).

28. `library/core/src/slice/sort/shared/smallsort.rs` +
    `library/core/src/slice/sort/unstable/quicksort.rs` — **`#[inline(never)]`
    on the sort machinery's element save/restore boundaries keeps
    user-closure-driven sorts capability-safe** (the `sort_by_key`
    miscompile fix; see known-issue #15 below for the full mechanism and the
    validity evidence).  Five attributes, all the change-#27 house pattern
    ("keep the library function that performs the critical data movement out
    of the body that also contains the inlined user closure"):
    `insert_tail` (the insertion-sort step whose `ManuallyDrop` tmp saves and
    restores the sifted element), `partition_hoare_branchy_cyclic` and
    `partition_lomuto_branchless_cyclic` (the ipnsort partitions whose
    `GapGuard`/`PartitionState` save the gap element), and the
    `GapGuard<T>::drop`/`GapGuardRaw<T>::drop` impls that restore the saved
    gap element.  Without them, the key-extraction (or comparator) closure
    inlined into those bodies lets the Fil-C pre-pizlonation pipeline collapse
    the element save/restore memcpys into integer load/store pairs that move
    only the payload, dropping the capabilities of the pointers inside the
    moved elements ("filc safety error: cannot read pointer with null object"
    when the moved element is later dropped or read, at `-O` only).  The
    attributes are EACH load-bearing: removing the two partition-level ones
    (with the drop-level ones still in place) re-breaks
    `sort_unstable_by_key` on 100 pointer-bearing elements, and removing the
    drop-level ones (with the partition-level ones in place) breaks it again
    (the standalone partition still scalarizes its gap value when the guard
    can be inlined).  `heapsort` (which moves via `ptr::swap_nonoverlapping`,
    i.e. change #22's pointer-typed chunk machinery), the driftsort merge and
    stable partition (all-memcpy moves), the stable quicksort's pivot copy
    (a read-only `ManuallyDrop`), and `swap_if_less` (the Copy-type sort
    network; reachable only for `CopyMarker` types, whose bare-pointer
    elements stay pointer-typed through the save/restore -- verified with a
    `Vec<*const u8>` and a `{ *const u8, u32 }` Copy-struct probe at `-O`)
    needed no changes.  The cost: the small-sort step and each partition
    become real calls per sort, and two more calls per partition invocation
    on the panic-recovery drop paths.

29. `compiler/rustc_codegen_llvm/src/builder.rs` +
    `compiler/rustc_codegen_llvm/src/type_of.rs` +
    `compiler/rustc_codegen_ssa/src/traits/builder.rs` +
    `compiler/rustc_codegen_ssa/src/mir/{intrinsic,statement,block}.rs` —
    **the Fil-C rustc handles pointer-maybe allocas exactly the way clang
    handles union-carrying ones: `zhas_union` on the alloca,
    `zmemmove_builtin` for the copies, and the change-#21 all-pointer-struct
    alloca type kept as belt-and-braces** (the known-issue #13 fix; this
    change REPLACES the SROA-side fix that used to be change #29, which was
    rejected by the user -- "it's 100% definitely not an SROA bug and not a
    pizlonator bug -- it's a rustc bug, and the fix is to do what clang
    does" -- and `llvm/lib/Transforms/Scalar/SROA.cpp` is back to being
    byte-identical to HEAD, i.e. upstream SROA plus the pre-existing Fil-C
    Skew hack; llvm/ carries NO diff on this branch beyond what was already
    committed).  clang's machinery, which this mirrors:

    * `zhas_union` -- clang annotates EVERY temp alloca of a union-carrying
      type of at least 8 bytes with a `call void @zhas_union(ptr)`
      (CodeGenFunction::CreateTempAlloca, fed by `Ty.hasUnion()`, plus
      EmitVarAnnotations for named locals).  The call is a no-op at run time
      (the pizlonator erases it and its escape analysis skips it --
      FilPizlonator.cpp), but it is an OPAQUE CALL USE of the alloca pointer,
      which stops SROA from partitioning, promoting or re-typing the alloca
      AT ALL: an alloca that might hold pointers must never be split into
      pieces whose types the optimizer chose, because GIMSO memory transfers
      move payload + capability shadow together while typed (especially
      integer-typed) accesses move only the payload, and a pointer-typed
      widening could manufacture capability accesses at non-word-aligned
      offsets (the exact mechanism of the UnixListener::bind/
      TcpListener::accept panic below).  This is also what made clang-compiled
      code FAST (qjs got ~1.3x faster when zhas_union landed, commit
      475a8b221ef1): the escaping is what keeps the pizlonator's own escape
      analysis cheap and the optimizer from re-typing memory.
    * `zmemmove_builtin` -- clang emits `call void @zmemmove_builtin(ptr dst,
      ptr src, i64 count)` for every `memcpy`/`memmove`/`mempcpy` builtin
      call (CGBuiltin.cpp) instead of an llvm.memcpy/memmove intrinsic.  The
      runtime function is a memmove (it handles overlap), it moves payload +
      capability shadow together, and being an opaque call it is untouchable
      by the pre-pizlonation pipeline -- no pass can widen it into
      integer-typed load/store pairs (which drop capabilities) or
      pointer-typed accesses at non-word-aligned offsets.  The pizlonator
      lowers it like any other memmove (lowerMemmoveCall) and inlines
      constant-count calls itself (isCallToInlineableMemmove), so small
      constant copies still scalarize -- capability-correctly -- at
      pizlonation time.
    * `zmemmove_union` -- the third member of the family (declared in
      filc_test_support.h: "the memmove that we use for union assignments"),
      recognized and lowered by the pizlonator with the identical signature.
      clang does NOT auto-emit it anywhere (EmitAggregateCopy emits a plain
      llvm.memcpy decorated with TBAA; the zmemmove_union callers in the tree
      are the C tests and the generated memmoves suites).  The rustc fix does
      not need it either: `memcpy_maybe_unions` (below) emits
      `zmemmove_builtin`, which has the same memmove semantics and the same
      pizlonator lowering; zmemmove_union stays a public C API with C test
      coverage.  (Decision: NOT emitted by rustc; nothing in the union paths
      needed it once zhas_union + zmemmove_builtin were in.)

    The rustc implementation:

    * `filc_may_contain_pointers` (type_of.rs, pre-existing from change #21)
      is the union-carrying analog of clang's `Ty.hasUnion()`: Rust enums are
      unions in all but name, so every aggregate that may contain pointers
      gets clang's union treatment.  It now answers `Variants::Empty`
      (uninhabited) layouts FALSE without descending into fields
      (`TyAndLayout::field` panics on them -- "there is no field in
      Variants::Empty types" -- which the old predicate never noticed because
      `filc_ptr_struct_type`'s zero-size guard rejected those layouts before
      the predicate ran; the zhas_union annotation queries the predicate for
      EVERY alloca, including uninhabited locals, so the guard moved inside
      the predicate).
    * `alloca_for_layout` (builder.rs) now does BOTH legs for pointer-maybe
      aggregates: the all-pointer-struct typed alloca (change #21, kept) AND
      the `zhas_union` annotation -- emitted when the layout is pointer-maybe,
      NOT a Scalar (clang's hasUnion only fires for aggregates; a scalar
      pointer's alloca must stay promotable by SROA -- a pointer VALUE in a
      register carries its capability under GIMSO), and at least 8 bytes
      (clang's `getTypeAllocSize(Ty) >= 8`).  The call is inserted past the
      entry block's static allocas (`LLVMRustPositionBuilderPastAllocas`), so
      it necessarily follows the alloca it annotates on either construction
      path.
    * `memcpy_maybe_unions` (new `BuilderMethods` method, default impl =
      plain `memcpy` so every other backend is unchanged; the LLVM backend
      overrides it to emit the `zmemmove_builtin` call) plus
      `filc_may_contain_pointers` (new `BuilderMethods` method, default
      false) route every copy of pointer-maybe bytes through the runtime:
      `typed_place_copy_with_flags` (the compiler-generated aggregate-copy
      path -- OperandValue::Ref stores, indirect arguments, typed swaps),
      `copy_intrinsic` (Rust's `ptr::copy`/`ptr::copy_nonoverlapping`, which
      is rustc's analog of clang's memcpy/memmove builtins), the MIR
      `CopyNonOverlapping` statement, and the PassMode::Cast spill/reload in
      mir/block.rs.  The runtime call's pointer arguments are also plain uses
      of the endpoint allocas, so an alloca participating in a pointer-maybe
      copy escapes SROA even when its own LLVM type does not reveal
      pointer-ness.

    Why this fixes known-issue #13 where the SROA-side guard only papered
    over it: the panic came from a MULTI-RUN interaction -- a run's
    widened ptr-typed store became a later run's partition evidence
    ("Rewriting alloca partition [4,12) to: alloca ptr"), propagating
    misaligned pointer accesses further (the second-level shape loaded a ptr
    from the path-string constant at +2) -- and it was REMOVABLE in both
    directions: re-bisecting the original repro against the reverted clang
    showed that deleting ONLY the final sret aggregate copy from the repro
    (the upstream leg of the chain) made the panic disappear, i.e. the shape
    needs real aggregate-copy chains feeding several SROA runs.  With
    zhas_union, pointer-maybe allocas never enter SROA at all, so no run of
    the pipeline can start the chain; with zmemmove_builtin, the copies that
    would seed it are opaque calls.  llvm/ carries no change for this --
    upstream SROA still widens exactly the shapes it always did (pinned by
    the NEGATIVE witness .ll below), and clang-compiled C never hits them
    because clang escapes these allocas before the optimizer sees them.

    Validity evidence:
    * The negative witness `filrustc/tests/sroa_misaligned_ptr_piece_
      unescaped.ll` (the pre-fix IR shape: typed all-pointer-struct alloca,
      no zhas_union, plain llvm.memcpy payload copy + sret copy chain)
      PANICS through the reverted clang with "filc safety error: alignment
      requirement of 8 bytes not met ... expected 110 bytes with ptr aligned
      to 8 bytes at offset 4" (exit 42 under FILC_EXIT_ON_PANIC=1) --
      re-verified twice during this fix (once by reverting SROA and
      rebuilding clang, once when re-deriving the shape).
    * The positive guard `filrustc/tests/sroa_misaligned_ptr_piece.ll` is
      the same shape WITH the post-fix annotations (zhas_union call on the
      alloca, zmemmove_builtin for the payload copy and the sret copy): it
      passes byte-exact ("ok") through the same `clang -O2 -x ir` command.
    * net_unix/net_tcp at -O run green with the full stack (see the honest
      caveat under known-issue #13: they had ALREADY stopped reproducing the
      panic on this tree before this change -- the change-#24 layout fix and
      the std changes moved the shapes -- so they are end-to-end coverage,
      not discriminators).
    * The new .rs mirrors: `union_mixed_members` (the wackyunion mirror),
      `union_struct_config` (the uniontest/PostgreSQL config_var_value
      mirror), `align4_numeric_moves` (the bug294 mirror -- numeric align-4
      aggregates must keep untyped allocas and SROA integer widening),
      `misaligned_copy_into_aggregate` (the #13 byte-copy-into-aggregate
      shape through real rustc codegen; passes with the annotation -- and,
      recorded honestly, also passes with the annotation disabled, because
      the single-function shape does not reproduce the multi-run
      interaction; the .ll witnesses are the discriminators),
      `union_abi_carrier_fail` (the unioncarrierfail mirror -- NEGATIVE: the
      extern "C" by-value union carrier must lose the capability and the
      read must panic with "filc safety error: cannot read pointer with null
      object", which is the remaining documented known-issue #10 gap) and
      `packed_pointer_fail` (the unionpackedpointerfail mirror -- NEGATIVE:
      a pointer at offset 7 in a repr(packed) struct must panic with
      "alignment requirement of 8 bytes not met ... ptr aligned to 8 bytes
      at offset 7").
    * The full filrustc suite is green with the final rustc (120 tests, 0
      skips, SUCCESS).
    * Perf: the change is rustc-only, so clang-compiled code is untouched BY
      CONSTRUCTION; the reverted SROA.cpp makes the shipped clang
      byte-equivalent to HEAD's.  simdutf (README.md dataset, -I 2000, 3
      runs, medians over 52 procedures) rebuilt with the reverted clang
      (77/77 ctest pass): geomean median 14.6 GB/s, top entries
      count_utf8+icelake 171.7 GB/s, validate_utf8+haswell 107.9 GB/s --
      consistent with the pre-change-#29 parity measurements recorded above
      (the SROA-side guard's geomean ratio was 1.013 vs its pre-fix binary;
      the revert restores that pre-fix binary's codegen exactly).  The
      filrustc suite wall time and the std build are the rustc-side
      consumers of the extra zhas_union/zmemmove_builtin calls; no
      regression was observed in either (std builds in the same incremental
      time, and the suite passes at both optimization levels).

10. **Aggregate-ABI holes** (they fire at `opt-level=0` AND `-O` alike, so
   they are not pipeline-dependent -- they are rustc lowering / Fil-C ABI
   gaps, documented per the review round that surfaced them).  Of the two
   holes below, the FIRST is now FIXED on this branch (change #23: the Rust
   ABI keeps pointer-maybe small aggregates indirect); only the SECOND
   remains a live gap:

   * **A `#[repr(C)]` union with a pointer field, passed by value at the
     RUST ABI, used to lose the pointer's capability -- FIXED by change
     #23.**  Original repro (exit 133 at both opt levels before the fix): a
     `#[repr(C)] union U { p: *mut u8, i: u64 }` passed BY VALUE to (or
     returned by value from) an `#[inline(never)]` fn that reads `*u.p`
     died with "filc safety error: cannot read pointer with null object."
     -- the by-value ABI move of the 8-byte union carried only the payload.
     Mechanism: the union's overlapping fields (`*mut u8` vs `u64`) give it
     a `BackendRepr::Memory` repr (rustc's union ABI optimization only
     forwards a field's scalar repr when ALL fields share it), and
     `adjust_for_rust_abi` casted every sized Memory-repr aggregate of at
     most pointer size into an integer immediate (`cast_to(Reg { Integer,
     size })`), so the union crossed the call boundary as an `i64`.  Under
     GIMSO an integer in flight carries no capability.  The same cast also
     dropped capabilities for `Vec::push` of a pointer-bearing union (the
     argument to `Vec::push` crosses the same boundary) and for `Box::new`
     of one.  Change #23 keeps pointer-maybe small Memory-repr aggregates
     in the default indirect (by-reference) mode, which moves the value as
     a memory copy (payload + capability together).  In-memory union moves
     (swap/replace/Vec-element writes) were always memcpy-based and were
     additionally covered by the typed allocas (change #21: a union's
     alloca is an all-pointer struct because `filc_may_contain_pointers`
     recurses into union fields) and the pointer-typed swap chunks
     (change #22); `filrustc/tests/union_memory_moves` exercises all of
     those (and deliberately does NOT exercise the remaining hole below --
     the bare-union-by-value case through `extern "C"`).
     The **`extern "C"` (SysV) by-value variant of this hole is still
     open**: `adjust_for_foreign_abi` -> x86_64 `compute_abi_info`
     classifies a pointer-bearing 8-byte aggregate as one Int eightbyte and
     `cast_to`s it into an `i64` register, dropping the capability.  That
     path is deliberately NOT fixed here: changing rustc's extern "C"
     handling would desynchronize it from clang-compiled C callers/callees,
     which would need matching LLVM-side (clang) work.  Any FFI that hands
     a pointer-bearing union across `extern "C"` by value still loses the
     capability.

   * **A `#[repr(packed)]` struct containing a pointer is unusable through
     Fil-C's aligned-pointer model.**  Repro (verified on this tree, exit
     133 at both opt levels): `#[repr(packed)] struct P { a: u16, p: *mut
     u8 }` with the pointer at offset 2, read via
     `addr_of!(p.p).read_unaligned()` in an `#[inline(never)]` fn, dies
     with "filc safety error: alignment requirement of 8 bytes not met."
     + "expected 10 writable bytes with ptr aligned to 8 bytes at offset
     2."  Fil-C pointers are 8-byte-aligned capabilities (and pointer
     loads/stores go through capability-carrying moves that require that
     alignment), so a pointer at a non-8-aligned offset cannot be
     represented.  This is also exactly why the `layout.align.abi.bytes() <
     8` guard in `filc_ptr_struct_type` (type_of.rs) is MANDATORY: giving
     such an aggregate a typed all-pointer struct alloca would make SROA
     produce nonsense partitions, so `repr(packed)` pointer-bearing
     aggregates fall back to the untyped byte-array alloca.  (The std swap
     fix of change #22 makes the same alignment distinction:
     `swap_nonoverlapping_bytes` uses pointer-typed chunks only when
     `align_of::<T>() >= CHUNK_SIZE` and keeps the upstream byte-chunk path
     otherwise, so packed types swap capability-blind exactly as before.)
     A real fix
     would have to teach rustc to reject `#[repr(packed)]`-with-pointers on
     the Fil-C target (or give Fil-C sub-word capability moves); neither is
     attempted here.

11. **Pointer-maybe enums with a small discriminant start their integer
   payloads at 4 mod 8 (FIXED on the layout side; std::process::Command::spawn
   is the regression guard).**  `io::Result<(Process, StdioPipes), io::Error>`
   (and every `io::Result<T, io::Error>` with a non-pointer-bearing `T`) is
   laid out as a 4-byte tag followed directly by the payload, because the Ok
   variant's fields need only 4-byte alignment while the Err variant's
   `io::Error` is a single niche-encoded pointer that lands at offset 8.
   The Fil-C rustc gives the allocas (and sret slots) of pointer-maybe
   aggregates all-pointer struct types (change #21), so the tag-plus-payload
   aggregate is sliced into 8-byte pointer-typed partitions: the Err
   variant's real pointer legitimately occupies partition [8,16), and the Ok
   variant's *integer* bytes copied through that partition come out of the
   typed slot as pointer-typed VALUES.  When those values flow into a
   destination whose byte offsets differ by 4 (the `?`-desugaring's
   field-wise stores into a differently-shaped sret, SROA's decomposition of
   the 28-byte tuple memcpy in `Command::spawn`), the pizlonator sees a
   pointer-typed store at an address that is 4 mod 8.  A pointer under GIMSO
   can only rest in an 8-byte-aligned word (its capability needs a shadow
   slot), so the pizlonator requires word alignment for every pointer-typed
   access (`buildChecksRecurse` in FilPizlonator.cpp:
   `T == RawPtrTy -> buildCheck(WordSize, WordSize, ...)`), and the check is
   unsatisfiable: `Command::status()`/`output()`/`spawn()` died at every
   optimization level with "filc safety error: alignment requirement of 8
   bytes not met. ... expected 12 writable bytes with ptr aligned to 8 bytes
   at offset 4." (exit 42/133).  The fix is in the layout (change #24 in the
   list above): `layout_of_uncached` in rustc_ty_utils computes the enum's
   layout, and when the result is direct-tagged, pointer-maybe
   (`filc_layout_may_contain_pointers`, the rustc_ty_utils twin of the
   codegen predicate), and the first payload byte is not word-aligned, it
   recomputes the layout with a word-sized discriminant
   (`repr.int = Fixed(I64, true)` -- which also inhibits the niche
   optimization, which is fine because the enum is already direct-tagged --
   plus `repr.align = 8`), moving the first payload byte of every variant to a
   word boundary (what the `filc_payload_misaligned` check actually inspects;
   later fields of multi-field variants may still sit at 4 mod 8, which is
   safe because they are only ever touched with integer-typed accesses and
   SROA's rewriter cannot widen those into pointer-typed ops under the
   `-ni:0` non-integral data layout -- `canConvertValue` refuses `iN -> ptr`
   -- see change #24 for the full explanation).
   Niche layouts already keep the pointer *at* the niche (word-aligned), and
   packed enums keep their layout (the repr(packed) hole above).  Explicit
   integer reprs (`#[repr(u8)]`-style) ride the same recompute: widening
   their tag on the FIRST computation would trip rustc_abi's
   "layout decided on a larger discriminant type" assert, so the tag-deciding
   closure tracks the repr each computation uses and the recompute is where
   their tag widens (see change #24 for the full mechanism, including the
   discriminant-range subtlety with raw `Discr.val` bits, and the
   `repr_enum_*` ICE guards).  This is
   the usermusl house pattern of never letting pointer slots alias integer
   slots at sub-word granularity (pthread_mutex_t's `__p[]` vs `__i[]`
   split).  Validity evidence, both directions: with the fix reverted
   (upstream layout), `Command::new("/bin/echo").arg("x").status()` dies
   with the alignment error above at O0 AND -O (captured repeatedly);
   with the fix restored it prints "x / status = true" and exits 0, at both
   opt levels, and the full filrustc suite passes.

12. **`std::fs::read` / large `read_to_string` lose the buffer capability
   (FIXED in std; `fs_ops` + `fs_ops-opt` + `fs_big_rw` + `fs_big_rw-opt`
   are the regression guards).**
   `fs::read::inner` built the destination vector with
   `Vec::try_with_capacity` and `io::default_read_to_end` read through the
   `BorrowedBuf`/`BorrowedCursor` machinery into the vector's spare
   capacity.  On this branch the vector's data pointer loses its Fil-C
   capability on the way through those aggregate moves (the pointer is
   stored as a bare integer in the try-reserve path and re-derived as an
   inttoptr in the pizlonated code), so the kernel write through it --
   and even `extend_from_slice`'s memcpy destination -- reported
   "filc safety error: cannot write pointer with null object" (or
   "cannot write destination pointer with null object") from `zsys_read`/
   memmove; `read_to_string` on files larger than the 32-byte probe also
   silently returned an empty string.  THREE std-side fixes (change #25 in
   the list above): `io::default_read_to_end` now reads through a
   fixed-size plain stack buffer and appends with `extend_from_slice`
   (a memcpy moves payload + shadow, so the vector only ever grows through
   properly-typed code paths -- the usermusl house pattern for bytes the
   KERNEL fills), `fs::read::inner` uses the infallible
   `Vec::with_capacity` (under Fil-C the allocator aborts on OOM anyway, so
   the try variant's graceful failure path is moot; more importantly the
   vector's pointer stays in plain typed storage), and
   `sys/fd/unix.rs`'s `File::read_buf` reads through the plain 8192-byte
   `FILC_READ_BUF_SCRATCH` buffer and `cursor.append()`s the bytes.  The
   read_buf rewrite's contract: every `read_buf` call on a `File` returns
   AT MOST 8192 bytes regardless of the cursor's capacity (capping the
   transfer is within the `Read` contract -- reads may legitimately return
   fewer bytes than requested), so callers must loop; the extra copy per
   call is the price of a fixed-size, known-typed staging buffer (the
   capability-preserving rewrite needs a pointer that comes straight from
   the stack, because the cursor's pointer is exactly what loses its
   capability on the way through the `BorrowedBuf`/`BorrowedCursor`
   aggregate moves).  Validity evidence,
   both directions: with the fixes reverted (upstream io/mod.rs + fs.rs),
   `fs::read` dies with the null-object errors above and `read_to_string`
   returns 0 bytes for a 200-byte file; with the fixes restored, a
   10-byte and a 600-byte file read correctly through `fs::read`,
   `fs::read_to_string`, and `File::read_to_end`, and the full filrustc
   suite passes.  `fs_big_rw` extends the guard to the >8 KiB regime this
   round: a 64 KiB file must read back byte-identically through `fs::read`,
   `read_to_string` and `File::read_to_end`, and io::copy's internal
   read_buf loop must deliver exactly 8 chunks of 8192 bytes (the cap) for
   it.  BufReader over a `File` never needed the fix (it
   reads through the plain `read` path).

13. **`TcpListener::accept` / `UnixListener::accept` (FIXED ON THE RUSTC
   SIDE -- change #29, the clang-parity union handling; `net_tcp`/`net_unix`
   run un-skipped, `sroa_misaligned_ptr_piece` (+ its `_unescaped` negative
   witness) are the direct `.ll` guards and the new union-mirror `.rs` tests
   are the source-level guards).**  Both hit the same aggregate-write shape
   as known-issue #11 in a form the layout fix does not cover: the accept
   result `io::Result<(TcpStream, SocketAddr)>` copies aggregate data (the
   110-byte align-2 `sockaddr_un`, the Err variant's io::Error pointer word)
   through pointer-maybe allocas whose typed (all-pointer-struct)
   representations from change #21 let upstream SROA widen crossing memcpy
   pieces into `load ptr`/`store ptr` AT THE OTHER SIDE of the copy with the
   slice's pointer type -- a 4-mod-8 or 2-mod-8 address -- and the
   pizlonator's inline word-alignment check for RawPtrTy accesses (which is
   independent of the instruction's own alignment annotation:
   `buildCheck(WordSize, WordSize, ...)` in FilPizlonator.cpp's
   buildChecksRecurse) failed at run time ("alignment requirement of 8
   bytes not met. ... expected 110 writable bytes with ptr aligned to 8
   bytes at offset 4", exit 42/133) for any `bind()`/`accept()` call.  The
   fix is change #29's clang parity (zhas_union escaping of pointer-maybe
   allocas + zmemmove_builtin for pointer-maybe copies; see change #29 for
   the full mechanism, the multi-SROA-run propagation -- one run's bogus
   ptr store became the next run's partition evidence "Rewriting alloca
   partition [4,12) to: alloca ptr", the second-level shape loading a ptr
   from the path-string constant at +2, and the re-bisect that showed the
   panic needs the sret copy chain -- and why llvm/ carries NO change for
   this).  HONEST CAVEAT about the end-to-end reproducibility: on the
   current tree, with SROA.cpp reverted to HEAD and std rebuilt, BOTH
   `net_unix` and `net_tcp` pass -- the change-#24 layout fix and the
   std-side changes (the swap chunks, the inline(never) guards, the fs
   rewrites) had already moved the real std shapes away from the
   misaligned-widening interaction, so these tests are end-to-end coverage
   of the bind/accept paths, not discriminators for this fix.  The
   discriminators are the .ll witnesses (which compile the exact IR shape
   -- a shape that cannot be expressed in Rust or C source, since clang
   escapes may-contain-pointer allocas before SROA sees them: the positive
   one passes only with the annotations, the negative one keeps panicking
   with upstream SROA) and the rustc-side check_grep markers (see
   build_rustc_impl.sh).

14. **`?` on a `Result` whose error type carries pointers used to lose the
   payload's capability at `-O` (FIXED on the std side; change #27,
   `option_result_combinators` at `-O` is the regression guard).**
   Repro (exit 42/133 at `-O` only, before the fix):
   `fn h() -> Result<i32, String> { Err("boom".to_string()) }`
   `fn f() -> Result<i32, String> { let n = h()?; Ok(n) }` +
   `println!("{:?}", f())` printed the len of the error string correctly
   but died reading it: "filc safety error: cannot read pointer with null
   object" inside `<str as Debug>::fmt`, with the pointer's ADDRESS intact
   (0x7...) and its capability null -- the classic integer-lane signature.
   O0 was always fine, and the same enums moved by hand (`match h() {
   Ok(n) => Ok(n), Err(e) => Err(e) }`) were fine at every level.  The
   trigger is the `?` desugaring itself: `Try::branch` consumes the
   `Result` and returns `ControlFlow<Result<Infallible, E>, T>`, and
   `FromResidual::from_residual` moves the `E` payload back out.  Both
   functions were `#[inline]`, the `ControlFlow` is niche-encoded in `E`'s
   non-zero words exactly like `Result<T, E>` itself (so the `Continue`
   variant's `T` payload and the `Break` variant's `E` payload occupy the
   SAME bytes), and after the pre-pizlonation inliner inlined the two
   calls, SROA promoted that shared partition to a single IR type -- the
   integer, whenever `T` is an integer (`Result<i32, String>`,
   `Result<u64, String>`, `Result<u16, String>`, `Result<i32, Vec<u8>>`
   all reproduce; `Result<(), String>` does NOT, because its `Ok` variant
   has no payload to mix into the partition) -- so `E`'s pointer moved
   through `ptrtoint`/`inttoptr` and lost its capability.  Two things
   masked the hole until this round: `#[inline(never)]` on the user's own
   `?`-chaining functions keeps the desugaring calls out of the inliner
   (the `enum_large_payloads` test's `?`-chain is exactly this shape and
   passes), and `?` on `Option<T>` is immune (its residual
   `Option<Infallible>` is uninhabited and zero-sized, so no
   pointer-bearing payload shares a partition with the `Continue` value --
   `Option<Vec<String>>` and `Option<Big{String,u64}>` verified immune).
   The fix is change #27: `#[inline(never)]` on `Result::branch` and
   `Result::from_residual`, which leaves the residual's aggregate moves as
   memcpy-across-a-call (payload + capability together).  Validity
   evidence, both directions: with the `#[inline]` attributes restored
   (byte-identical to upstream), a 15-program repro matrix of the shapes
   above (`Result<T, String>` for T in i32/u64/i64/u16, `Vec<u8>`,
   `Box<str>`, `Vec<String>`, plus map_err/`?`/Display/Debug variants)
   fails 15 of its 46 compile+run cells with the null-object error at
   `-O`; with the fix in place all 46 pass, and the full filrustc suite
   passes.  The residual risk class is now narrow: any OTHER code shape
   that gets an integer and a pointer into the same niche-overlap
   partition of an inlined aggregate move would hit the same SROA
   promotion -- none was found in the combinator chains (`and_then`,
   `map_err`, `or_else`, `inspect`, `transpose`, `flatten`, `zip` over
   `Result<i32, String>` all pass at `-O`), and the manual-move shapes
   (`match`, `ControlFlow` built and destructured by hand) pass too.

15. **`sort_by_key`/`sort_unstable_by_key`/`sort_by` on pointer-maybe
   elements used to lose the moved elements' capabilities at `-O` (FIXED on
   the std side; change #28, `sort_by_key_enum` + `sort_by_comparator` and
   their `-opt` twins are the regression guards).**  Repro (exit 42/133 at
   `-O` only, before the fix):
   `#[repr(u64)] enum E { A(Box<String>), B(u32, u32) }` +
   `let mut v: Vec<E> = vec![E::B(1,2), E::A(Box::new("x".into())), E::B(3,4)];`
   + `v.sort_by_key(|e| match e { E::A(_) => 0, E::B(a, _) => *a });` +
   `drop(v)` died in the drop with "filc safety error: cannot read pointer
   with null object", the pointer's ADDRESS intact and its capability null
   (the failing read is the Box pointer of the MOVED `A` element: probed
   directly, the failing pointer value equals the String struct's address,
   and dropping only element 0 after the sort panics while
   `mem::forget`-ing the whole Vec does not).  O0 was always fine, and
   the same vector without the sort is fine at every level.  The mechanism
   (pizlonator-source + IR-verified): `E`'s aggregate IR type is
   `{ i64, [1 x i64] }` -- an integer payload partition (upstream
   `uncached_llvm_type` fills a variant-payload union with
   `type_padding_filler`, so a payload union of `{Box}` and `{u32, u32}`
   becomes an i64 array) -- and under GIMSO integer-typed load/store pairs
   move only the payload (the destination's shadow/aux is left stale --
   `storeValueRecurseAfterCheck` emits a payload store and never touches
   aux, per gimso_semantics.md "loading a pointer from a location that
   never had a pointer stored to it yields a pointer with a null
   capability"), while memcpy/memmove ALWAYS move payload + shadow.  At O0
   the element moves are real memcpys through the typed
   (all-pointer-struct, change #21) `ManuallyDrop` allocas and every
   aggregate access is materialized payload+aux, so capabilities travel.
   At `-O`, rustc's MIR inliner merges `insert_tail` into
   `insertion_sort_shift_left`, the key-extraction closure (and, for the
   comparator APIs, the `compare == Less` closure) is inlined into the
   merged body, and the Fil-C pre-pizlonation pipeline (the early
   SimplifyCFG/SROA/EarlyCSE plus the inliner's per-CGSCC
   SROA/EarlyCSE/GVN/InstCombine chain) collapses the save/restore memcpys
   into integer load/store pairs -- the saved element travels as
   `load i64 <tag>` + `load i32 <payload lo>` + `load i32 <payload hi>`
   and is re-stored with the corresponding integer stores
   (`store i64`/`store i32`/`store i32`), which is exactly where an
   `A` element's Box capability dies: the i32 halves of the Box pointer
   are written back as plain integers and the shadow at the destination is
   never set.  Proof that the pipeline (not codegen, not the pizlonator)
   introduces it: `rustc -C opt-level=2 -C no-prepopulate-passes
   -C llvm-args=--filc-optimize=0 -C llvm-args=--filc-killub=0` (pristine
   codegen IR + pizlonator only) PASSES the repro, and pristine-O2 codegen
   emits memcpys for the element save/restore; the same program through the
   standard `-O` pipeline panics.  The fix is change #28's five
   `#[inline(never)]` attributes, which keep the element save/restore
   boundaries separate from the inlined user closure so the memcpys survive
   to pizlonation (verified at pizlonation time with
   `-C llvm-args=--print-before-all -C llvm-args=-filter-print-funcs=...`:
   `insert_tail` keeps `memcpy`-based save/restore through its typed
   `<{ ptr, ptr }>` alloca with only integer READS from the inlined key
   closure; the partition keeps its gap copies as memcpys).  Validity
   evidence, both directions: with all five attributes removed (projeny
   patch byte-identical to upstream in those two files), the reviewer's
   repro exits 42 with FILC_EXIT_ON_PANIC=1 (133 as SIGTRAP otherwise),
   `sort_unstable_by_key` on 100 pointer-bearing elements exits 133
   (its corruption surfaces in `<&T as Display>::fmt` reading a moved
   String header), and the two new suite tests exit 133 at `-O`; with the
   attributes in place all of these exit 0 at O0/O1/O2/O3 and the full
   filrustc suite passes.  Iteration log: `#[inline(never)]` on
   `insert_tail` alone fixed the 3-element stable-sort case but not the
   unstable 100-element case; adding the two partition attributes alone
   (still-inlinable guard drops) left the standalone partition
   scalarizing its gap value into `i64`/`i32` loads plus integer stores;
   adding `#[inline(never)]` to `GapGuard::drop`/`GapGuardRaw::drop`
   (so the gap storage escapes and every copy touching it stays a memcpy)
   fixed it; removing the partition attributes again (keeping the drop
   ones) re-broke it -- hence all five are load-bearing.  Residual risk
   class: any OTHER generic machinery that saves and restores a
   pointer-maybe element in the same function as an inlined user closure
   could hit the same collapse; the sort machinery was swept function by
   function (heapsort: `ptr::swap_nonoverlapping`; driftsort merge and
   stable partition: all memcpys; stable quicksort's pivot copy: read-only
   `ManuallyDrop`; `swap_if_less`: CopyMarker-only, bare-pointer elements
   stay pointer-typed -- a `Vec<*const u8>` and a `{ *const u8, u32 }`
   Copy-struct probe pass at `-O` even before the fix) and nothing else
   matched.

## Std coverage suite (the coverage-round tests)

The coverage rounds added 40 `.rs` tests whose goal is "a test for each major
std function": every public std module gets at least one test, and the
major methods of each module are exercised.  The follow-up round added 7
more coverage tests (the four `repr_enum_*` change-#24 ICE guards and the
gap-closers `io_terminal`, `chown_fchown`, `backtrace_disabled`, rows at
the bottom of the table below), for 47 coverage-table rows total, and the
round-3 sort-miscompile fix added the `sort_by_key_enum` and
`sort_by_comparator` guards (the last two rows of the table), for 49
coverage-table rows total.  47 run,
and `net_tcp`/`net_unix` run un-skipped since known-issue #13 was fixed on
the rustc side (change #29).  The known-issue-#13 round added 6 more rows
(the union-mirror tests `union_mixed_members`, `union_struct_config`,
`align4_numeric_moves`, `misaligned_copy_into_aggregate`,
`union_abi_carrier_fail` (negative) and `packed_pointer_fail` (negative))
for 56 coverage-table rows total (the -opt twin is a row of its own), plus
the negative-witness .ll
`sroa_misaligned_ptr_piece_unescaped` which is listed in the
current-tests table below (next to `sroa_misaligned_ptr_piece`).  ALL of
it runs: `filrustc/run-tests` prints 121 dots, no "Skipping" lines, and
SUCCESS (113 runnable at the start of the round -- the 49 coverage rows,
the other std/tooling tests and `sroa_misaligned_ptr_piece.ll`, with
`net_tcp`/`net_unix` already un-skipped -- plus the 6 new `.rs` tests, the
`-opt` twin, and the new negative `.ll`).  The manifests are recountable programmatically
(do NOT count by eye):
take the first column of each
`| test (new) | ... |` row in the table below, strip ` (skip)`, read
`filrustc/tests/<name>.manifest`, and count the rows whose `optFlags`
contains `-O` (12) vs not (37) -- note
that the harness's `optFlags` REPLACES the default compile flags rather than
adding to them, so each test compiles and runs at exactly ONE optimization
level; a test that needs both levels needs a `-opt` twin (a byte-identical
`.rs` copy whose manifest adds `optFlags: -O`), which is the harness
convention for the bug-guard tests (see the test-table notes below).  All
tests are deterministic and hermetic: fs tests work under a per-test
`/tmp/filc-...-<pid>` directory created and removed by the test itself,
networking is loopback-only with ephemeral ports, timing assertions use
generous margins, and outputs are sorted wherever hashing or concurrency
would otherwise show through.  All tests use `extraArgs: ["--edition",
"2021"]`.  Every new test was hand-compiled AND hand-run at BOTH `-O` and
the default O0 before its manifest was written (the manifest then picks one
of the two levels).

Intentional coverage gaps (audited, not oversights): an ENABLED backtrace
capture -- `RUST_BACKTRACE=1` -- is not exercised because the RUST_BACKTRACE
machinery is known to be unsupported under Fil-C: a real capture calls
`_Unwind_Backtrace`, which libpizlo does not provide (verified: a
`RUST_BACKTRACE=1` run of `Backtrace::capture()` aborts with "filc user
error: Not implemented. (libpizlo.so) ../filc/src/unwind.c:96:5:
_Unwind_Backtrace"), and frame-walking the pizlonated stack is not
supported.  The backtrace crate's Fil-C stubs live in `library/backtrace`
(see the check_greps in build_rustc_impl.sh: library enumeration from
/proc/self/maps, no `_Unwind_FindEnclosingFunction`), and the DISABLED
capture surface -- what every program hits when `RUST_BACKTRACE` is unset,
i.e. the stable API's default path -- DOES work and is pinned by the
`backtrace_disabled` test.  Everything
else audited as "missing" was actually coverable and IS now covered:
`io::IsTerminal` (works under Fil-C -- isatty through pizfix; the
`io_terminal` test) and `os::unix::fs::{chown,fchown}` (runnable for any
runner by chowning to the runner's own uid/gid -- the `chown_fchown`
test).

| test (new) | module(s) / functions covered |
|---|---|
| vecdeque_ops | `VecDeque`: push_front/push_back, front/back, insert/remove, pop_front, iter/iter_mut, rotate_left, make_contiguous, swap_remove_back, into_iter (front/back), resize, shrink_to_fit, with_capacity, extend, contains/get; `Box<String>` elements (aggregate moves) |
| linkedlist_ops | `LinkedList`: push_front/push_back, front/back, len, append (consuming), split_off, clone, pop_front/pop_back, iter; `Box<String>` elements |
| btreemap_ops | `BTreeMap`: insert, first_key_value/last_key_value, entry API (Occupied/Vacant, get_mut/insert), range, get_key_value, values_mut, append, split_off, pop_first; `BTreeSet`: insert/contains/take; `Box<u64>` values |
| btreeset_ops | `BTreeSet`: union/intersection/difference/symmetric_difference, insert/get/replace/remove, range, is_subset/is_superset, iter; `Box<String>` elements; dedup over split_whitespace |
| hashset_ops | `HashSet`: insert/contains/remove/len, union/intersection/difference/symmetric_difference (sorted for determinism), retain, drain, take; `HashSet<Box<String>>` aggregate moves |
| binaryheap_ops | `BinaryHeap`: push/pop/peek/peek_mut (in-place sift), into_sorted_vec, `Reverse` min-heap, drain; `(u64, Box<String>)` pairs |
| iter_adapters | `Iterator`: chain, zip, rev, scan, fold, flat_map, peekable+peek, take_while, skip_while, step_by, partition, position, find_map, max_by_key, sum/product, try_fold, by_ref+take, filter_map; `iter::from_fn`, `successors`, `once`, `repeat`; collect into `Vec`, `HashMap`, `VecDeque<Box<String>>` |
| str_methods | `str`: split/splitn/rsplitn, matches/rmatches/match_indices, trim/trim_start/trim_end_matches, find/rfind, char_indices, encode_utf16, split_whitespace/split_ascii_whitespace, strip_prefix/strip_suffix, repeat, replace/replacen, split_terminator, lines, chars, to_uppercase, escape_debug; `char`: to_digit/len_utf8/is_alphabetic/to_uppercase; as_bytes/from_utf8/concat/eq_ignore_ascii_case/starts_with/ends_with |
| parse_nums | `FromStr`/parse for i32/u64/i64/i128/u128/usize/isize/f32/f64/bool, from_str_radix (16/2), error cases (non-numeric, float-to-int, overflow, empty, leading +), "inf"/"NaN" parsing, sign handling |
| int_methods | integer ops: wrapping/saturating/checked add-sub-mul, pow/checked_pow, abs/checked_abs, signum, div_euclid/rem_euclid, leading/trailing_zeros, count_ones, swap_bytes, rotate_left, to/from_le_bytes/to_be_bytes, i128/u128 arithmetic, rem, MIN/MAX consts, `u8::from(bool)`, `TryFrom`, `as` casts, abs_diff, is_power_of_two/next_power_of_two, ilog2/ilog10 |
| float_methods | `f64`/`f32`: to_bits/from_bits, round/fract/trunc, sqrt/powi/powf/abs/exp/ln, consts (PI/E/SQRT_2/MAX/MIN_POSITIVE), NaN/INFINITY (is_nan/is_infinite/is_sign_negative, max/min with NaN), clamp, rem_euclid, floor/ceil, total_cmp sorting, formatting (`{:.3}`/`{:.0}`/`{:e}`/`{:E}`/`{:+.2}`/width/fill), as-casts, recip/cbrt/hypot/sin/cos, mul_add |
| io_cursor | `Cursor<Vec<u8>>`: write_all/writeln!/write!/position, read_to_string, read_line, read_exact, seek (Start/End), read_to_end, BufRead::split/bytes/read_until, `io::copy` |
| io_buf | `BufReader`: read_line/lines, read_exact, buffer(), fill_buf; `BufWriter`: write!/writeln!/flush/into_inner; `take`, `by_ref`, `chain`, `Read::read_to_string`; BufReader over a slice |
| io_error | `io::Error`: from_raw_os_error/kind/raw_os_error, `Error::other`, downcast_ref, `Box<dyn Error>` chains with `source()` traversal, `Send + Sync` boxes, ErrorKind list, Box downcast, `From<&str>`/`From<String>` |
| fs_ops | `fs`/`File` under a per-test tmp dir: create, write_all/writeln!, sync_all, seek, read_to_string, fs::read/read_to_string/write, metadata (len/is_file/permissions), set_permissions, OpenOptions (append/read+write), create_dir/remove_dir, read_dir (sorted), canonicalize, rename, symlink+read_link+read-through-link, remove_file/remove_dir_all, cleanup |
| net_udp | `UdpSocket`: bind (ephemeral), local_addr, send_to/recv_from (loopback), connect + send, try_clone, read timeouts |
| process_ops | `process`: Command::new/arg/args/env/current_dir, output(), status() (success/failure/exit codes), spawn + stdin piping + wait_with_output, missing-program error (NotFound), `process::id` |
| time_ops | `time`: Instant (elapsed, add), SystemTime (UNIX_EPOCH, duration_since, checked_add, elapsed), Duration (as_secs/subsec_millis/as_millis/as_nanos, mul/div, checked_mul/checked_add, saturating_sub, ZERO, from_micros, as_secs_f64/f32) |
| thread_ops | `thread`: park/unpark, yield_now, tiny sleeps, available_parallelism, Builder::name/stack_size, is_finished, join, `Thread::name`, scope (borrowing threads, panic observation), many short threads |
| sync_primitives | `sync`: Mutex (poison + `Arc::try_unwrap` + into_inner), RwLock (multiple readers, exclusive write, cross-thread write, poison), Condvar (wait/notify_one/wait_timeout.timed_out), Barrier (3 threads x 2 phases) |
| once_race | `sync`: Once::call_once from 4 racing threads (exactly-once via Barrier), OnceLock::get_or_init race (all observe one value), LazyLock (Box<String> vec), OnceLock set/get semantics |
| atomics | every stable `Atomic*` type of 1.90 (AtomicI8/U8/I16/U16/I32/U32/I64/U64/Isize/Usize via a uniform macro probe, plus AtomicBool and AtomicPtr with their own op sets; AtomicI128/AtomicU128 are `#[unstable(integer_atomics)]` in this tree and are NOT exercised): store/load under Relaxed/Release/Acquire, swap under SeqCst, fetch_add (AcqRel)/fetch_sub (Acquire)/fetch_and/fetch_or/fetch_xor/fetch_max/fetch_min (SeqCst), compare_exchange strong (SeqCst/Acquire) + weak (AcqRel/Relaxed, bounded-retry), fetch_update, for every type that has each op; Release/Acquire message passing (data + flag across a spawned thread), 4-thread SeqCst fetch_add loop.  The manifest asserts ALL TWELVE per-type uniform lines (the `exercise_int!` loop's "uniform = true add_old = ... sub_old = ... cas = ... weak = ... update = ..." line for each of the ten integer atomics plus AtomicBool's and AtomicPtr's), so a miscompile confined to one type cannot pass silently |
| os_fd_traits | `std::os::fd` on `File`: AsRawFd/AsFd/BorrowedFd (three views of one descriptor), try_clone dup semantics (different fd number, shared open file description: writes through one handle visible through the other, shared cursor), IntoRawFd/FromRawFd round trip (release + re-adopt), `OwnedFd` round trip (File -> OwnedFd -> File keeps the fd); per-test tmp dir |
| unix_fs_ext | `std::os::unix::fs`: FileExt (read_at/write_at/read_exact_at/write_all_at at multiple offsets, cursor independence), MetadataExt (uid/gid via an owner cross-check so it holds for any runner, size, dev, ino, atime, mtime, nlink), PermissionsExt (set_mode/set_permissions + from_mode round trips at 0o600/0o640, readonly), OpenOptionsExt (custom_flags with benign O_CLOEXEC, mode(), append through the ext trait); per-test tmp dir |
| ip_addr_methods | `IpAddr`/`Ipv4Addr`/`Ipv6Addr`: constants (LOCALHOST/BROADCAST/UNSPECIFIED), new/octets/segments/from_bits/to_bits (round trips), predicates (is_loopback/is_broadcast/is_private/is_link_local/is_multicast/is_unspecified/is_unique_local), v4<->v6 (to_ipv6_mapped/to_ipv6_compatible/to_ipv4/to_ipv4_mapped/to_canonical), the `IpAddr` enum (From/predicates/equality with concrete types), FromStr parsing (valid + invalid), Display. Pure computation, no networking |
| cow_typeid | `Cow` (Borrowed stays borrowed, to_mut materializes, into_owned, Cow<'a, str> from &str and from String, Cow<'a, [u32]> slice pattern), `TypeId::of` equality/inequality, `Any` (is/downcast_ref/downcast_mut/downcast over a few concrete types, type_id on the dyn itself, &dyn Any downcast) |
| slice_search | slice search: binary_search (hit/miss/head/tail/absent-low/absent-high), binary_search_by, binary_search_by_key (hit + miss), contains, starts_with/ends_with (str slices + byte slices), sort + binary_search integration, sort_unstable re-check, first/last (+ empty), windows/chunks/iter sums |
| char_conversions | `char`: from_u32 (valid/surrogate/beyond-max/nul/max), the safe-wrapper pattern for from_u32_unchecked, from_digit (valid/over-radix/max radix), to_digit round trips (16 and 36, invalid), escape_debug/escape_default, is_control, to_uppercase (multi-char 'ß' -> "SS")/to_lowercase |
| option_result_combinators | `Option`: transpose (some/none/err), flatten, map_or, ok_or, and_then, or_else, filter, zip, iter, cloned/copied, inspect, unwrap_or_else; `Result`: map_or, inspect, and_then, or_else, map_err, map, iter, cloned, flatten (ok/err), transpose; `?` chains inside a helper fn (parse error mapping + negative check) |
| fs_big_rw | the >8 KiB multi-chunk regime of the fixed default_read_to_end (change #25) and the File::read_buf cap contract (change #25's sys/fd/unix.rs piece): a 64 KiB patterned file read back byte-identically through fs::read, read_to_string and File::read_to_end; io::copy into a counting sink drives File::read_buf repeatedly until 0 and pins the 8192-bytes-per-call cap (8 chunks of exactly 8192 for 64 KiB -- the trait method itself is unstable on the release channel, so std's own stable io::copy loops are the driver); io::copy into a 16384-capacity BufWriter (cursor capacity above the cap, so std must loop); tail read; per-test tmp dir.  `fs_big_rw-opt` is the -O twin.  NOTE: the "chunks = 8 max chunk = 8192" assertion is COUPLED to std's `generic_copy` stack-buffer size (the read_buf cap, `FILC_READ_BUF_SCRATCH`, is the Fil-C side of that coupling) -- a future upstream std change to the copy buffer size would break this test for non-Fil-C reasons (that coupling is the point of the known-issue #12 guard: the test exists to notice exactly such a drift) |
| alloc_ops | `alloc`: Box::into_raw/from_raw (incl. arrays), std::alloc::alloc/dealloc/realloc with Layout + write_bytes/read/write, Vec::with_capacity/reserve/shrink_to_fit/len/capacity, `Vec::leak` |
| mem_nonzero_pin | `mem`: discriminant, size_of/align_of, swap/replace on enum-with-Box; `NonZero{U8,U16,U32,U64,I8,I32,I64,Usize}` (get/new-none/conversions/leading_zeros); `Pin` (Pin::new, get_ref, DerefMut on Unpin, `Box::pin`, `Pin<Box<dyn Future>>` polled via a helper) |
| ffi_ops | `ffi`: CString::new/into_raw/from_raw, CStr::to_bytes/to_bytes_with_nul/from_ptr/as_ptr, NUL-error, empty; `OsString`/`OsStr`: from/from_vec/as_bytes/to_string_lossy/Borrow/Ord/is_empty; `Path`/`PathBuf`: parent/file_name/extension/components/ancestors/starts_with/strip_prefix/push/set_extension/join/file_stem/is_absolute/iter/as_os_str |
| env_ops | `env`: set_var/var/var_os/remove_var, current_dir, temp_dir, args, join_paths/split_paths |
| panic_hook | `panic`: set_hook/take_hook (counting), catch_unwind (payload + non-panicking path), panic_any (String/struct payloads + downcast), silent hook, thread panic -> join Err |
| error_chains | `Error` trait: source() chains, Display for error structs, `Box<dyn Error>` From conversions (&str, custom), TryFrom/TryInto (u8::try_from, slice->array), ParseIntError kinds (InvalidDigit/PosOverflow/Empty), downcast_ref, map_err |
| hash_ops | `hash`: Hash/Hasher via DefaultHasher (determinism within a process), composite Hash impls, HashMap/HashSet with custom Hash keys (`Box<Key>`), BuildHasherDefault/with_hasher/build_hasher, order-independent container hashing |
| fmt_ops | `fmt`: Display/Debug for a custom struct (incl. `{:#?}`), width/fill/alignment (`{:5}`/`{:05}`/`{:<5}`/`{:>5}`/`{:^5}`/`{:*^7}`), sign/alternate (`{:+}`/`{:#x}`/`{:#o}`/`{:#b}`), precision (`{:.2}`/`{:.0}`), string width/precision, `{{}}` escapes, positional/named args, Debug for Vec/Option/Result/String/char/tuple/Range/slice, Display for Path/SocketAddr, `fmt::write` into a String |
| net_tcp | `TcpListener::bind` (ephemeral), local_addr, accept+echo over threads, set_nodelay, peek, shutdown, connect-refused.  Was `skip`-marked for known-issue #13; runs again since the fix (change #29, the rustc-side clang-parity union handling).  HONEST NOTE (see known-issue #13): on the current tree this test passes even with SROA.cpp reverted, because the change-#24 layout fix and the std changes moved the real shapes -- it is end-to-end coverage of the bind/accept path at `-O`, not a discriminator; the discriminators are the .ll witnesses |
| net_unix | `UnixListener`/`UnixStream` (bind/connect/echo), `UnixStream::pair`, `SocketAddr` parsing, `ToSocketAddrs`.  Was `skip`-marked for known-issue #13; runs again since the fix (change #29) -- same honest note as net_tcp |
| union_mixed_members | the filc/tests/wackyunion mirror: unions whose members mix integers, data pointers and FUNCTION POINTERS, whole-array aggregate copies through an `#[inline(never)]` fn, `slice::swap` of union elements, dispatch through a union-restored function pointer, writes and reads through a union-restored data pointer, unions into a Vec and back out (the capability of every member, including the code pointer, must survive the moves) | success, output-includes "4242 21 99 99", "12345 297 555 555" | exit 0 | exit 0 |
| union_struct_config | the filc/tests/uniontest mirror (PostgreSQL's config_var_value): a struct holding a union of bool/i64/*mut u8 members plus an extra pointer, field-by-field updates through a dispatch, by-value returns of union-carrying structs, Vec push + mem::replace aggregate moves, and string clones through chained by-value moves that read through the union pointer (capability must survive each move) | success, output-includes "bool = false", "int = 500", "int2 = 766", "str = v7 2" | exit 0 | exit 0 |
| align4_numeric_moves | the filc/tests/bug294 mirror: align-4, non-multiple-of-8 float aggregates (Vec3 12 / Quat 16 / Transform 28 bytes) copied by value at 4-mod-8 offsets through `#[inline(never)]` chains -- both the whole-aggregate assignment and the field-by-field variant -- plus a Vec round trip; numeric aggregates keep untyped allocas and upstream SROA integer widening, and every copy preserves the values (sink = -1.3 by both paths) | success, output-includes "sizes 12 16 28", "aligns 4 4 4", "sink = -1.30000 -1.30000", "ok"; output-excludes "bad" | exit 0 | exit 0 |
| misaligned_copy_into_aggregate (+ -opt twin) | the known-issue #13 shape through REAL rustc codegen: a pointer-maybe aggregate whose pointer word gets typed store+load receives a 108-byte copy of an align-2 buffer at a 4-mod-8 offset (crossing the pointer word), then the copied data bytes are verified byte-exact; exercises the zhas_union-escaped typed-alloca path end-to-end at BOTH optimization levels (the -opt twin guards that the annotation survives the whole pre-pizlonation pipeline above O0) (HONEST NOTE: passes with the annotation disabled too -- the single-function shape does not reproduce the multi-SROA-run interaction, see known-issue #13; the .ll witnesses are the discriminators) | success, output-includes "ok"; output-excludes "bad" | exit 0 | exit 0 |
| union_abi_carrier_fail | the filc/tests/unioncarrierfail mirror (NEGATIVE): a pointer-bearing union carrying a one-past-the-end pointer passed BY VALUE across an `extern "C"` boundary loses the capability in the ABI carrier staging (the remaining documented known-issue #10 gap), so the subsequent read must panic and nothing may print | return: failure, output-includes "filc safety error", output-excludes "nono" | exit 42 | exit 42 |
| packed_pointer_fail | the filc/tests/unionpackedpointerfail mirror (NEGATIVE): a pointer at offset 7 inside a `#[repr(packed)]` struct stored through `addr_of_mut!(...).write_unaligned` -- ABI carrier staging must not relax actual pointer-access alignment, so the store must panic with the alignment error and nothing may print | return: failure, output-includes "filc safety error", "alignment requirement of 8 bytes not met"; output-excludes "nono" | exit 42 | exit 42 |
| repr_enum_u8 | the `#[repr(u8)]` pointer-maybe enum surface (change #24's ICE guard): both variants of `enum E { A(Box<String>), B(u32) }` constructed, moved by value (sret returns, Vec growth, mem::swap/mem::replace), matched, and printed (the Box contents + the u32 payload).  Fails to COMPILE (rustc_abi's "layout decided on a larger discriminant type (I64) than typeck (I8)" assert, exit 101) if the Fil-C discriminant bump is ever again applied to explicit reprs on the first layout computation, and RUNS to prove the integer-only variant payloads at 4 mod 8 do not pizlonator-panic at runtime (the recompute widens the tag: 8-byte discriminant, payloads at 8).  `repr_enum_u8-opt` is the -O twin |
| repr_enum_c_u8 | the C-FFI tagged-enum pattern `#[repr(C, u8)] enum Tagged { Num(u64), Ptr(Box<String>) }` plus a misaligned-payload twin `#[repr(C, u8)] enum Mixed { Small(u32), Big(Box<String>) }`: both variants of both enums moved by value (Vec, swap, replace), matched, printed.  Same ICE guard as repr_enum_u8 (any `#[repr(C, u8)]` pointer-maybe enum fails to compile if the closure bump returns); Tagged's payloads are already word-aligned (repr(C) grows the tag prefix to the field alignment, so its 1-byte tag is kept) while Mixed's u32 payload must go through the recompute.  `repr_enum_c_u8-opt` is the -O twin |
| repr_enum_i16 | `#[repr(i16)] enum Wide { A(String), B }` (a signed 2-byte tag whose String payload is already word-aligned at 8 -- kept as upstream laid it out) plus `#[repr(i16)] enum Signed { Small(u32) = 3, Big(Box<String>), Neg = -5 }` (the u32 payload lands at 4 mod 8, so it goes through the recompute over a NEGATIVE discriminant).  Both moved by value, matched, printed; same ICE guard as repr_enum_u8.  `repr_enum_i16-opt` is the -O twin |
| repr_enum_u32 | `#[repr(u32)] enum E { A(Vec<u8>), B }` (4-byte tag kept, Vec payload at 8) plus `#[repr(u32)] enum Mixed { Small(u32), Big(Box<String>) }` (recompute: the u32 payload lands at 4 mod 8) and `#[repr(u32)] enum Large { Hi = 4_000_000_000, Lo = 1, Unit }` (near-u32::MAX discriminant).  All moved by value, matched, printed; same ICE guard as repr_enum_u8.  `repr_enum_u32-opt` is the -O twin |
| io_terminal | `io::IsTerminal` (isatty-style queries) over the generic trait surface: `stdin()`/`stdout()`/`stderr()` and `File::is_terminal` through a generic `T: IsTerminal` helper; the harness always pipes the output, so every query is deterministically false and a broken syscall path would panic or report true |
| chown_fchown | `std::os::unix::fs::{chown, fchown}`: path- and fd-based owner/group changes observed through MetadataExt's uid()/gid(), the None (leave-side-untouched) form, and a 0o640 mode round trip; the chown targets the runner's own uid/gid (read from the per-test tmp dir), so the assertions hold for root and non-root runners alike; per-test tmp dir |
| backtrace_disabled | `std::backtrace::Backtrace`'s DISABLED capture path (what every program hits when RUST_BACKTRACE is unset): `capture()` returns a Disabled backtrace that formats as "disabled backtrace".  An ENABLED capture (RUST_BACKTRACE=1) is the documented intentional gap -- it calls `_Unwind_Backtrace`, which libpizlo does not provide |
| sort_by_key_enum | `slice::sort_by_key` + `sort_unstable_by_key` over pointer-maybe enums (the change-#28 round-3 miscompile guard): the reviewer's `#[repr(u64)] {A(Box<String>), B(u32,u32)}` vector through the key closure `match e { A(_) => 0, B(a,_) => *a }` at 3 and 100 elements, the same through `#[repr(u8)]` and `#[repr(Rust)]` twins, post-sort payloads printed (String contents + integer fields), every key unique so the unstable order is determined.  `sort_by_key_enum-opt` is the -O twin |
| sort_by_comparator | the `sort_by` sibling of the change-#28 guard: the same `#[repr(u64)]` pointer-maybe enums through the comparator API (`sort_by(|a, b| key(a).cmp(&key(b)))`) at 3 and 100 elements (driftsort's quicksort + merge), plus a descending re-sort and an ascending re-sort of the sorted vector (the already-sorted detection paths), exact sorted output asserted.  `sort_by_comparator-opt` is the -O twin |

Not covered on purpose (the shapes fail by design -- known-issue #10):
`extern "C"` by-value unions and `#[repr(packed)]` structs containing
pointers.  Not covered because the APIs are unstable on 1.90 (would need
feature gates): `intersperse`, `LinkedList::retain`/cursors,
`div_ceil`/`next_multiple_of`, `linked_list_cursors`, and the
`Read::read_buf` trait method itself (which is why `fs_big_rw` drives the
patched `File::read_buf` through std's own stable `io::copy` loops instead
of calling it directly -- see that test's note in the table above).  Modules
covered
through the pre-existing tests: `sync::mpsc` (mpsc_large_messages),
`thread::scope` (thread_scoped_big), `thread_local` (thread_local_cells),
`char`/`str` formatting (string_ops_roundtrip, vec_string), `collections`
aggregates (hashmap_aggregates), `io::Stdin`-adjacent formatting via
println!.

## Verified demos (from /tmp; `filrustc` alone suffices)

See "How the built rustc finds filcc" above.

* `fn main() { println!("hello filc rust"); }` -- debug and `-O`, exit 0,
  prints "hello filc rust".
* Out-of-bounds `unsafe` read (`*ptr.add(1000000000)` past a 3-element
  `Vec<i32>`, pointer wrapped in `std::hint::black_box`) -- debug and `-O`,
  both exit 133 with "filc safety error: cannot read pointer with ptr >= upper"
  and "filc panic: thwarted a futile attempt to violate memory safety", plus a
  Fil-C semantic-origin backtrace pointing at the user's `main`.
* `readelf -l` shows interpreter `pizfix/lib/ld-fil1-x86_64.so`; NEEDED =
  libc.so.6666, libpizlo.so, libyolocimpl.so, ld-fil1-x86_64.so.
* `std::thread::spawn` + join works at every optimization level (O0-O3;
  known-issues #1 documents the SROA/GIMSO miscompile that used to break
  `-O` and its rustc-side fix); 8 threads with Arc<Mutex>,
  HashMap/Vec/String/sort/trait objects all work (debug and -O).
* `is_x86_feature_detected!` works (debug and `-O`): a sweep queries every
  one of the 74 `@FEATURE`-queryable names (plus the `abm`/`avx512gfni`/
  `avx512vaes`/`avx512vpclmulqdq` aliases) and reports every feature the
  CPU + kernel actually support and "no" for the rest, all cross-checked
  against `/proc/cpuinfo` (see the verification matrix and known-issues #7).
* `panic!`/`unwrap` on failure prints the Rust panic message and unwinds out
  of `main` (exit 101, the Rust panic exit code; see "Panics" above).
  `catch_unwind`, Drop glue during unwinding, thread-panic join and
  `-C panic=abort` (message + abort, exit 133) all work too.
* Cross-DSO: `--crate-type dylib` + client with `--extern`/`-C prefer-dynamic`
  links and runs (see "Cross-DSO" above).


## Build scripts (build_rustc.sh / build_rustc_incremental.sh / build_rustc_impl.sh)

The build is split into a shared library plus two thin entry points:

* `build_rustc_impl.sh` -- the shared library (not meant to be executed
  directly).  It resolves the repo root from its own `$BASH_SOURCE` (so
  sourcing it works regardless of the caller's CWD), defines the
  `check_grep` helper and `do_build_rustc <clean|incremental>`, and carries
  ALL of the build logic: the extraction of the patched Rust sources from the
  projeny checkout, the check that `$FILC_ROOT/build/bin/llvm-config` exists
  (the Fil-C rustc links the LLVM build from build/ — no LLVM is built here;
  run ./build_clang.sh first — see "Rustc links the Fil-C LLVM from build/"
  above), the 82 `check_grep` fail-fast verifications of the projeny patch,
  the `FILC_VERSION` composition (plus the `.filc-version-stamp` handling),
  the `FILC_LINKER_DIR` resolution + export (plus the parallel
  `.filc-linker-dir-stamp` handling; see "How the built rustc finds filcc"),
  the libgcc_s redirect (gcc-redirect dir + RUSTFLAGS; the GROUP contents
  are arch-aware, see the LLVM section and "ARM64 (aarch64) bring-up"
  below),
  the config.toml generation, the `x.py build --stage 1` run, and the
  creation of the `rust-build/rustc` and `rust-build/filrustc` symlinks.
* `build_rustc.sh` -- thin entry: license header, `set -e -o pipefail -x`,
  source the impl, call `do_build_rustc clean`.  This is the default: it
  nukes `rust-build/` first and does the whole build from scratch.
* `build_rustc_incremental.sh` -- thin entry calling
  `do_build_rustc incremental`: identical, except it does NOT nuke
  `rust-build/`.  Extraction, the llvm-config check and the version stamp
  only redo work when what they produce is missing or changed, and
  x.py/cargo resume incrementally, so a re-run is usually a fast no-op; a
  missing `rust-build/` is bootstrapped from scratch, so the incremental
  script also doubles as a first build.  NOTE: cargo does not notice changes
  to the LLVM archives rustc_llvm links against (build/lib/*.a), so after
  rebuilding clang's LLVM with a changed pipeline (e.g. after editing
  RegisterFilPasses.cpp), force the relink with
  `touch rust-build/rust/compiler/rustc_llvm/llvm-wrapper/PassWrapper.cpp`
  before the incremental build.

Both entry points end identically: after the symlinks are created, the
Fil-Rust test suite (`./filrustc/run-tests`) runs, tee'd to
`rust-build/tests.log`.  A test failure fails the build script (the entries
run under `set -e -o pipefail`, and the runner exits nonzero unless every
test passed).  Export `SKIP_RUST_TESTS=1` to skip the suite (e.g.
`SKIP_RUST_TESTS=1 ./build_rustc_incremental.sh`).

## Flavor check: musl vs. glibc (why build_glibc_extras.sh, not build_all_slow.sh)

`build_rustc.sh` was checked against the default (musl) flavor of Fil-C --
the `build_all.sh` configuration, i.e. what `./build_base.sh` builds with no
`setup_glibc.sh` -- to decide where the build belongs (item: "if it works in
the musl version put it in build_all_slow.sh, else put it in
build_glibc_extras.sh").  Result: **the build succeeds under musl but the
rustc it produces cannot link std-using Rust programs there, so the build
was placed in `build_glibc_extras.sh`** (appended after `./build_php.sh`
with a comment summarizing the LFS64 situation).  It is deliberately NOT in
`build_all_slow.sh` (the musl flavor's slow-build list).

Evidence (musl pizfix built from scratch by `./build_base.sh` -- which wipes
and rebuilds `pizfix/` for the default flavor -- then `./build_rustc.sh`
clean; ~10.5 min wall on a 32-core container, of which `x.py` itself took
7:49):

* The clean build succeeds end to end under musl: the projeny extraction,
  the Fil-C LLVM swap, the `check_grep` verification and
  `x.py build --stage 1` ("Build completed successfully in 0:07:49") all
  pass, and the resulting compiler runs (`rustc --version` prints the same
  Fil-C parenthetical as the glibc build).  Expected: the compiler binary
  itself is a host-toolchain program in BOTH flavors (bootstrap's CMake
  builds the swapped-in LLVM with the host `cc`/`c++`, the stage0-downloaded
  beta rustc builds the compiler, and the host linker links it against the
  host glibc -- its interpreter is `/lib64/ld-linux-x86-64.so.2` either way).
  The Fil-C-ness lives in the modules rustc *emits* and in `filcc` linking
  them against pizfix.  std is likewise flavor-independent: both flavors
  produce the identically-named stage1 rlib
  `libstd-a04c0eb3de138d45.rlib` (same cargo metadata hash).
* Every std-using program then fails to LINK, in the test suite (0/8 pass at
  the time of the check -- the suite has since grown from 8 to 10 tests, and
  under musl the result would be 0/10, since every test uses std and every
  std-using link fails there; each "FAIL: couldn't compile <name>" being a
  link failure) and by hand
  (`filrustc -o hello hello.rs`):
  `undefined reference to 'pizlonated_fstat64'`, and the same for
  `pizlonated_stat64`, `pizlonated_lseek64`, `pizlonated_mmap64` and
  `pizlonated_open64`, referenced from libstd's `std.*.rcgu.o`.  The vendored
  libc crate (`vendor/libc-0.2.174`, linux-gnu backend) imports glibc's
  LFS64 symbol names, and the musl user libc exports none of them
  (`nm -D pizfix/lib/libc.so` has zero `pizlonated_*64` entries; it does
  export the plain musl spellings `pizlonated_fstat`, `pizlonated_stat`,
  `pizlonated_lseek`, `pizlonated_mmap`, `pizlonated_open`, ...).  The glibc
  flavor's libc.so.6666 does export the LFS64 names, which is why the same
  rlibs link and the whole suite passes there.  (Soname observation: the two
  flavors install differently-named user libcs into pizfix -- the glibc
  flavor's `pizfix/lib/libc.so.6666` (~24 MB, "GNU C Library" banner) vs.
  the musl flavor's `pizfix/lib/libc.so` (~7 MB, musl strings, no glibc
  banner) -- with the same `ld-fil1-x86_64.so` interpreter either way.  A
  script that greps pizfix for the libc banner (as this flavor check did)
  must look at the flavor-appropriate name; `libc.so.6666` simply does not
  exist under musl.)
* What still works under musl: `rustc --version` (the compiler binary), and
  even a `#![no_std]` program links, producing a 16 KB binary -- the
  rustc -> filcc -> pizfix link pipeline per se is flavor-agnostic; only
  libstd's FFI symbol set is glibc-specific.  (A raw `-nostartfiles` no_std
  binary is not a supported runtime configuration and its run behavior is
  not evidence either way.)
* A real musl-flavor fix would need either a musl variant of the target spec
  (wiring e.g. x86_64-unknown-linux-musl up to filcc plus a Fil-C-patched
  vendored-libc backend) or teaching Fil-C's musl libc to also export the
  LFS64 names glibc exports.  Neither is attempted here; if one lands, move
  the `./build_rustc.sh` invocation to `build_all_slow.sh` per the
  build_glibc_extras.sh header convention.

## Fil-Rust test suite (filrustc/run-tests)

`filrustc/run-tests` is the Fil-Rust analog of `filc/run-tests` -- same
structure, same capabilities, same output protocol:

* Tests live in `filrustc/tests/` as flat `<name>.rs` files, each with an
  optional companion `<name>.manifest` YAML file.  `<name>.ll` files are ALSO
  supported by the runner (the suite now has one: `sroa_misaligned_ptr_piece`,
  the known-issue #13 LLVM-side regression guard -- a shape that cannot be
  expressed in Rust source, since it needs SROA to see a hand-built
  all-pointer-typed alloca whose crossing memcpy is partitionable, which
  clang-compiled C cannot produce because clang escapes may-contain-pointer
  allocas with `zmemmove_builtin`/`zhas_union` calls before the optimization
  pipeline; this is also why the fixed_sroa branch's .ll tests are not on
  this branch): they are compiled with Fil-C's clang itself, always at
  -O2 (`build/bin/clang -O2 -x ir <src> -o <bin>`), because some regressions
  cannot be expressed in Rust (or even C) source -- clang lowers memcpy
  intrinsics to pizlonated builtins before its optimization pipeline runs, so
  a test that must feed hand-written memcpys *through* that pipeline has to
  arrive at the IR level.  The -O2 is
  the point of a .ll test: it is what makes SROA and the rest of the
  pre-pizlonation pipeline run over the hand-written IR.  A .ll test must
  declare BOTH datalayouts: `target datalayout` must be the pre-FilC
  datalayout including `-ni:0` (what the pipeline sees -- `canConvertValue`
  and the ptr-words machinery depend on pointers being non-integral), and
  `target datalayout_after_filc` is MANDATORY: clang asserts in
  MachineFunction::init() without it.  Manifests use the same
  vocabulary as a `filc/tests/<name>/manifest`: `return` (one of `success`,
  `failure`, `crash`, `dontCrash`, `exit13`, `exit101`, `compileFailure`,
  `compileOnly`, `linkFailure` -- the last is the same as `compileFailure`
  since rustc compiles and links in one step; `exit101` asserts the Rust
  panic exit code for a panic unwinding out of `main`), `output-includes` /
  `output-excludes` (fixed strings that must / must not appear in the
  output; `grep -F` semantics), `optFlags` (extra rustc flags; default is
  the debug build, e.g. declare `-O` to test the optimized pipeline),
  `extraArgs` (extra rustc arguments), `extra-env` (extra run-time
  environment), `skip`, `slow` (run first, like filc), and
  `only-on-platform` (regexp against RUBY_PLATFORM).  A missing manifest
  means `return: success` with no output assertions.  (`optFlags` is a rustc
  concept and is ignored for .ll tests, whose compile command is fixed.)
* For each test the runner generates shell scripts under
  `filrustc/test-output/<name>/` -- `compile.sh` (rustc invocation, compiler
  output captured to `compilerOutput.txt`), `justRun.sh` (the binary with
  `LD_LIBRARY_PATH=pizfix/lib_test`, mirroring filc's test library flavor),
  `subRun.sh` (runs the binary with the FILC harness environment, checks the
  exit code, then checks the output inclusions/exclusions), and `run.sh`
  (compile + run) -- and drives them with
  `make -k -f filrustc/test-output/testmakefile -j <nproc>`.  That is
  exactly filc/run-tests' concurrency approach: each test is one make
  target, so compiles and runs of different tests happen in parallel (up to
  nproc at a time) while each test's own compile-then-run stays serialized.
* Output protocol: one '.' per passing test (or `<script>: success` with
  `-v`), `FAIL: ...` spew plus the failing program's complete output on
  failure, `SUCCESS` at the end, and exit code 0 iff all tests passed.
  Options: `-t <name>` runs exactly one test (supplying `-t` multiple times
  means only the last test runs), `-f <regexp>` filters test names, `-n`
  generates the scripts without running anything, `-v` is verbose.  Unlike
  filc/run-tests, a `-t`/`-f` that selects zero tests is an error (so a
  typo'd test name cannot masquerade as a pass), and the runner works from
  any CWD (it cds to the repo root it derives from its own path).
* The compiler is `rust-build/filrustc`; if it (or Fil-C's `build/bin/filcc`,
  which the Fil-C host target spec -- x86_64-unknown-linux-gnu, or its
  aarch64 twin -- uses as the linker) is missing,
  the runner exits with "Run ./build_rustc.sh first" / "Build Fil-C first"
  instead of failing every compile.

### The FILC environment mechanism

Every program runs under `FILC_EXIT_ON_PANIC=1 FILC_DUMP_SETUP=1`, exactly
like filc/run-tests' sub-run scripts.  The Fil-C runtime reads
`FILC_EXIT_ON_PANIC` in `libpas/src/libpas/filc_runtime.c`
(`filc_get_bool_env("FILC_EXIT_ON_PANIC", &filc_exit_on_panic)`), and
`filc_finish_panicking()` (same file) then prints
`[<pid>] filc panic: <kind>` and `_exit(42)` -- the distinctive panic exit
code -- instead of raising the SIGTRAP that the core dump machinery would
turn into a core file.  ALL filc panics funnel through
`filc_finish_panicking`: safety panics ("filc safety error: ..." ->
"thwarted a futile attempt to violate memory safety."), internal errors, and
user errors ("filc user error: ..." -> "user thwarted themselves.", which is
the path Rust's `abort()` takes, e.g. with `-C panic=abort` -- a default
panic instead unwinds and exits 101, which is not a filc panic at all).
`FILC_DUMP_SETUP=1` makes every run print the filc setup dump (harmless for
the output checks, and invaluable spew on failure).

So a `return: failure` test asserts exit code 42, which proves both that the
memory-safety violation was caught AND that it went through the panic
machinery rather than crashing the process.  Without the harness environment
the same programs exit 133 (SIGTRAP, core dumped) -- Fil-C's normal
trap-based panic, same as C.

### The current tests (all verified empirically with/without the env)

| test | exercises | manifest | without FILC env | under the harness |
|---|---|---|---|---|
| hello | println! | success, output-includes "hello filc rust" | exit 0 | exit 0 |
| oob_read | unsafe `*p.add(1000000000)` past a `black_box`ed 3-element `Vec<i32>` | failure; "filc safety error", "cannot read pointer with ptr >= upper.", "filc panic: thwarted..." | 133 (SIGTRAP, core) | 42 |
| oob_write | unsafe OOB `*p.add(1000000000) = 666` | failure; "...cannot write pointer with ptr >= upper."; output-excludes "Should not get here." | 133 | 42 |
| null_deref | volatile read of a `#[inline(never)]`-hidden null | failure; "...cannot read pointer with null object" | 133 | 42 |
| use_after_free | read through a raw pointer to a dropped `Box` | failure; "...cannot read pointer to free object" | 133 | 42 |
| safe_panic | safe-Rust `v[10]` index panic unwinding out of `main` | exit101; Rust "panicked at" + "index out of bounds"; output-excludes "Not implemented" and "user thwarted" | exit 101 (message, then clean unwinding) | exit 101 |
| safe_panic_abort | the `safe_panic` program with `extraArgs: -C panic=abort` | failure; "panicked at" + "index out of bounds" + "filc panic: user thwarted themselves."; output-excludes "Not implemented" | 133 (message, then clean abort) | 42 |
| catch_unwind | `panic::catch_unwind` catches `panic!("boom")`, payload downcasts to the message; non-panicking closure returns Ok | success; "panicked at", "boom", "caught: boom", "ok path"; excludes "Not implemented" | exit 0 | exit 0 |
| catch_unwind-opt | the `catch_unwind` program compiled with `optFlags: -O` | success; same output assertions | exit 0 | exit 0 |
| drop_unwind | Drop glue runs while a panic unwinds through two `#[inline(never)]` frames (innermost first), caught by `catch_unwind` | success; "panicked at", "deep panic", "drops: drop inner; drop outer", "drop ok" | exit 0 | exit 0 |
| thread_panic_join | a spawned thread panics; `join()` returns `Err` carrying the payload; a healthy thread joins `Ok` | success; "thread panicked as expected: thread boom", "healthy thread ok" | exit 0 | exit 0 |
| threads | 8 threads x 1000 `Arc<Mutex<u64>>` increments | success, "sum = 8000" | exit 0 | exit 0 |
| threads-opt | the `threads` program compiled with `optFlags: -O` (the exact spawn+join shape that used to hit the SROA/GIMSO miscompile -- now the rustc-side typed-allocas fix's regression guard, see known-issues #1(a)) | success, "sum = 8000" | exit 0 | exit 0 |
| no_prepopulate | Vec::push sum compiled with `-C no-prepopulate-passes` (proves the direct-MPM pizlonation path) | success, "no-prepopulate sum = 42" | exit 0 | exit 0 |
| vec_string | Vec/String/HashMap/iter/collect/format!/sort | success, three expected lines | exit 0 | exit 0 |
| hello-opt | the `hello` program compiled with `optFlags: -O` | success, output-includes "hello filc rust" | exit 0 | exit 0 |
| oob_read-opt | the `oob_read` unsafe OOB read compiled with `optFlags: -O` | failure; "filc safety error", "cannot read pointer with ptr >= upper.", "filc panic: thwarted..." | 133 (SIGTRAP, core) | 42 |
| swap_nodes | 16 pointer-containing `Node {String, *mut u8, u32}` values built by an `#[inline(never)]` constructor, pairwise-shuffled with `#[inline(never)]` `std::mem::swap`, then cloned and read (the reviewer's mem::swap repro; `optFlags: -O` is the point -- the miscompile only fires above O0, see known-issues #1(b); guarded by the std fix of change #22, verified in both directions) | success, output-includes "vals = [10, 20, 30, 40, 10, 20, 30, 40, 10, 20, 30, 40, 10, 20, 30, 40]" | exit 0 | exit 0 |
| swap_small_structs | mem::swap family on SMALL (<=24B) pointer-containing aggregates: two locals, whole arrays (stack allocas), through a Vec, 16-byte `Option<&str>` and tuples, plus a repeated swap loop (regression guard for change #22's pointer-chunk swap in the <=24B regime) | success, output-includes "pair: beta:2 alpha:1", "array: f:6 e:5 d:4 c:3", "vec: h:8 g:7", 'opts: Some("two") Some("one")', "tuples: (two, 22) (one, 11)", "loop: y:10 x:9" | exit 0 | exit 0 |
| swap_large_structs | mem::swap of 72-byte `{String, *mut u8, u32, [u64; 4]}` structs (with a raw pointer whose pointee gets freed by Drop): two locals, within a stack array, through Vec references, and a 12-swap shuffle; every value (including the boxed pointee behind the raw pointer) must survive bit-for-bit | success, output-includes "size = 72", "pair: bb#2*2[2,3,4,5] aa#1*1[1,2,3,4]", "shuffled: ii#9*9[9,10,11,12] | jj#10*10[10,11,12,13] | kk#11*11[11,12,13,14] | ll#12*12[12,13,14,15]" | exit 0 | exit 0 |
| swap_box_and_refs | mem::swap of 8-byte pointer ELEMENTS: in-place reversal of a `Vec<Box<String>>` via pairwise `split_at_mut` swaps, swap of Box locals, swap through `&mut T` references, 8-byte `Option<&str>` array elements, and 16-byte `Box<dyn Fn>` fat pointers -- the smallest size class where an integer-typed swap chunk would widen to a single i64 (guards change #22 at the 8-byte size) | success, output-includes "rev: e11 e10 e09 e08 e07 e06 e05 e04 e03 e02 e01 e00", "boxes: second first", "refs: ref-two ref-one", "opts: p3 p2 p1 p0", "closures: cb-b cb-a" | exit 0 | exit 0 |
| mem_replace_take | mem::replace/mem::take on String/Vec/Box payloads, Option take/replace of large payloads, Vec::swap_remove, and mem::replace of a 24-byte enum (whole-aggregate typed moves) | success, output-includes "new = two[2,3,4;20], old = one[1,2,3;10]", 'taken string = "held", s = ""', "opt now true, prev = \"three[3,4,5;30]\"", "swap_remove -> six[6,7,8;60], rest = eight[8,9,10;80] | seven[7,8,9;70]" | exit 0 | exit 0 |
| rotate_small_arrays | slice::rotate_left/rotate_right across the risky regimes: constant-folded rotate(1)/rotate(2)/rotate(3) on small FIXED arrays of 8-byte Box elements, 40-byte `[u8; 40]` elements spanning the `[usize; 32]` rotate-scratch threshold, and runtime-count rotates on `Vec<String>`/`Vec<Box<String>>` (see the rotate-scratch investigation note below the table: the scratch is unreachable by constant folding) | success, output-includes "rl1 = b,c,d,e,a", "rr1 = a,b,c,d,e", "rl2 = 3,4,1,2", "rl3 = 2,3,4,1", "bytes40 = c,d,e,f,a,b tails 234501", "rl(3) = v3,v4,v5,v6,v0,v1,v2", "rr(2) = s3,s4,s0,s1,s2" | exit 0 | exit 0 |
| rotate_vecdeque | VecDeque<Box<String>> push_front/push_back, `rotate_left`/`rotate_right`, `make_contiguous`, pops from both ends and wrap-around indexing after rotation (the deque's internal ring-buffer rotations over pointer elements) | success, output-includes "dq = f0 v0 v1 v2 v3 v4 v5 v6 v7 f2 f1", "after rr(4) = v6 v7 f2 f1 f0 v0 v1 v2 v3 v4 v5", "popped: v6 / v5", "len = 9", "idx0 = wrap", "idxlast = v4" | exit 0 | exit 0 |
| sort_stable_boxes | slice::sort (driftsort) of 60 `Box<String>` (8-byte elements: the small-sort's element-typed scratch AND the quicksort/stable_partition byte-typed `[MaybeUninit<u8>; 4096]` scratch with per-element constant-size copies), sort_by descending + re-sort, the <=32-element small-sort path, and 40 `String`s (24-byte elements) | success, output-includes "sorted = 0 1 10 11 12 13 14 15 16 17 18 19 2 20 21 22 23 25 26 27 28 29 3 30 31 32 33 34 35 36 37 38 39 4 40 41 42 43 44 45 46 47 48 49 5 50 51 52 53 54 55 56 57 58 59 6 60 7 8 9", "check = true", "small = 0,0,1,2,3,4,5,6,7,8,9,10", "strs = s00 s01 ... s42" | exit 0 | exit 0 |
| sort_unstable_strings | slice::sort_unstable (ipnsort) of 100 Strings and 50 Box<String>s, plus sort_unstable_by_key of 30 structs containing String+Box (in-place quicksort partitioning moves of pointer-containing elements) | success, output-includes "first = s0 last = s96 len = 100", "matches stable = true", "recs = n0 n0 n1 n10 ... n9", "stable-ish = true" | exit 0 | exit 0 |
| sort_large_structs | slice::sort_by/sort_by_key/sort_unstable_by of 80 96-byte structs `{String, Box<String>, [*mut u8; 2], [u64; 8]}` (sizes beyond the 4096-byte stack scratch threshold), asserting ascending order and intact boxed values | success, output-includes "sorted = big00:bx0;big00:bx3;...", "ascending = true", "bykey = bx0;bx0;bx0;", "first = big00 last = big30" | exit 0 | exit 0 |
| vec_ops_aggregates | Vec insert/remove/drain/splice/append/extend_from_slice/truncate+shrink_to_fit/retain of String and Box<String> elements (heap memmoves around pointer words) | success, output-includes "after insert: a,ins,b,c", "removed c -> a,ins,b", "drained: ins,b rest: a", "spliced: x,s1,s2,z extracted: y", "cv: c0,c1,...,c9", "retained: c0,c1,...,c7" | exit 0 | exit 0 |
| vec_roundtrip_iters | Vec<->iterator round-trips of Box elements: IntoIter collect, rev, front/back splits, zip of two pointer-bearing vecs, windows/chunks to_vec, chain+map, flat_map | success, output-includes "roundtrip: it0,it1,it2,it3,it4,it5", "rev: it5,...,it0", "front = it0 back = it1,...,it5", "zip: 1=one 2=two 3=three", "windows: w0+w1+w2 | ...", "chain: 10,20,30,40", "flat: n1,n2,n3" | exit 0 | exit 0 |
| string_ops_roundtrip | String push_str/insert_str/replace/drain/into_bytes/from_utf8 round-trips, 50-chunk growth forcing Vec memmoves, split/collect/join, Box<str> round-trip, multi-byte chars | success, output-includes "s = hello filc (cap >= 8: true)", "inserted = >>hello filc", "replaced = >>hello rust", 'drained = "he" left = ">>llo rust"', "utf8 = >>hello filc same = true", "boxed roundtrip same = true", "uni = h\u{e9}llo upper = H\u{c9}LLO chars = 5" | exit 0 | exit 0 |
| hashmap_aggregates | HashMap<String, Vec<Box<String>>> entry/or_default/get/remove with sorted-key deterministic output, BTreeMap<String, Box<Node>> ordered iteration and remove, and HashMap<&str, Box<dyn Any>> downcasts of String/u32/Vec<String> payloads | success, output-includes "size = 4", "key0 -> v0,v4,v8", "has key3 = true", "removed key3 = true", "only = made,added", "bt[n0] = node0#0", "bt removed n6 = node2", "any: string payload 42 l1+l2" | exit 0 | exit 0 |
| box_closure_dyn | Box<dyn FnOnce()> called, Box<dyn Fn> objects in a Vec, closures capturing large pointer-bearing structs moved through filter/map/collect, an owning FnMut closure used twice, and a stateful Box<dyn FnMut> | success, output-includes "fnonce = cap:cap-d0cap-d1cap-d2cap-d3", "fns = 11,20,7", "picked = iter#ITER-D0 iter#ITER-D2", "pushed = len=5 last=x1 len=6 last=x2", "counted = 1 3" | exit 0 | exit 0 |
| enum_large_payloads | Large multi-variant enum payloads: mem::replace/take of List/Pair variants, compare (PartialEq over [String; 3]), by-value consumption through #[inline(never)] calls, and Result<Shape, String> propagation with `?` | success, output-includes "taken empty = Empty taken = true", "prev pair = L R", "eq = true neq = true", "list x+y+z", "pair pq", "words w1|w2|w3", "ok = list deep", "err = bad input" | exit 0 | exit 0 |
| array_moves_index | `[LargeStruct; 3]` moved into/out of an #[inline(never)] transform, element swaps via split_at_mut, each_mut/each_ref, array::map to `[Box<String>; 3]`, into_iter collect, `[Option<Box<u64>>; 4]` swaps, and a 2D `[[Box<String>; 2]; 2]` | success, output-includes "arr = n1 n2 n3", "after = n3 999 n1", "each = n3!,n2!,n1!", "pointers = 40,20,-,10", "grid = r0c0 r0c1 r1c0 r1c1", "flat = r0c0,r0c1,r1c0,r1c1" | exit 0 | exit 0 |
| nested_containers | Vec<Vec<String>> deep-clone + mutate, Vec<Option<Box<Node>>> with a None hole, Branch{Vec<Leaf>, [u64;3]} clone-and-compare, by-value grow through #[inline(never)], Option<Branch> replace | success, output-includes "vv total = 6 mid = e", "clone paths = ab/C/def", "2 = none", "branch clone same = true", "grown = L1,L2,L3 107 3", "opt none = true prev leaves = 3" | exit 0 | exit 0 |
| maybe_uninit_roundtrip | SAFE MaybeUninit patterns over pointer-bearing aggregates: new/assume_init round-trip, write+assume_init for a struct, `[MaybeUninit<Box<String>>; 4]` filled then element-wise assume_init, ManuallyDrop-guarded reads of a partially-initialized buffer, field copies out of a ManuallyDrop wrapper | success, output-includes "roundtrip = combo 7 true", "written = written 88", "array = box0,box1,box2,box3", "guarded = combo second", "manual = manual+read" | exit 0 | exit 0 |
| rc_refcell_graph | Rc<RefCell<TreeNode>> acyclic graph with parent Weak links, strong/weak counts before and after moves into a Vec, RefCell borrow-scoped mutation of Box payloads, Weak::upgrade after drop | success, output-includes "root strong = 1 weak = 2", "left strong = 2", "child left payload 2 parent root kids left-child", "moved = left,right", "left still strong = 3", "mut = M1,m2,m3", "weak alive after drop = true", "root weak count = 2" | exit 0 | exit 0 |
| arc_mutex_large | 4 threads each capturing a large (String + 5-Box) struct by value, extending a shared Arc<Mutex<Vec<String>>>; deterministic sorted final print of all 24 entries | success, output-includes "n = 24 all = c0-0,c0-1,...,capture-3" | exit 0 | exit 0 |
| mpsc_large_messages | mpsc::channel (3 senders x 4 messages) and sync_channel (bounded, 2 senders) carrying 24-byte+ `{u64, u32, Vec<Box<String>>}` messages; deterministic via seq-number sorting | success, output-includes "count = 12", "first3: 0: s0-m0-p0+s0-m0-p1+s0-m0-p2 from 0", "last = 3: s2-m3-p0+s2-m3-p1+s2-m3-p2 from 2", "sync = 0,1,2,10,11" | exit 0 | exit 0 |
| thread_scoped_big | std::thread::scope: 3 threads borrowing a shared Vec<Box<String>> while owning large locals, returning Strings joined in sorted order, plus a scoped thread that panics and its join().is_err() observation | success, output-includes "scoped boom", "panic caught = true", "res = t0 shared8:shared-0 local4:local0-0,...", "shared intact = true" | exit 0 | exit 0 |
| thread_local_cells | thread_local! RefCell<Vec<String>> and RefCell<Box<String>> cells populated from 2 threads plus main, deterministic per-thread prints | success, output-includes "thread0 sees: t0-0,t0-1,t0-2", "thread1 sees: t1-0,t1-1,t1-2", "main sees: main,main2", "main drop cell: dropped" | exit 0 | exit 0 |
| coro_async_moves | Coroutines: async fns with Box<String>/Box<Vec<String>> captures moved across await points, polled by a hand-rolled executor (RawWaker vtable), at `-O` with `--edition 2021` (the coro-split codegen path at every level) | success, output-includes "done = [[seed+ab]!+c]!" | exit 0 | exit 0 |
| spawn_return_big | thread::spawn joining a LARGE (80-byte `{String, Box<u64>, [u64; 5], Box<u64>}`) return value from 4 threads -- the spawn+join flight-path shape that change #21's typed allocas guard, stressed with a big pointer-bearing payload | success, output-includes "size = 80", "joined = ret-0 0 [0, 1, 2, 3, 4] 0", "joined = ret-3 3000 [3, 4, 5, 6, 7] 6" | exit 0 | exit 0 |
| panic_payload_big | Panic payloads: panic_any of a large `{String, Box<u64>, Vec<String>}` struct + catch_unwind + Box<dyn Any> downcast (twice), a formatted String payload downcast, and a &'static str payload downcast (the default hook's spew is deterministic) | success, output-includes "down = payload-12#84:l12-a+l12-b", "orig = payload-11#77:l11-a+l11-b", "msg = plain message", "static = static panic text" | exit 0 | exit 0 |
| heap_ops | BinaryHeap<Box<String>> push/pop (element-typed Hole scratch), into_sorted_vec of Box<u32>, peek_mut in-place mutation and re-sift | success, output-includes "len = 10", "popped = h9,h8,h7,h6,h5,h4,h3,h2,h1,h0", "sorted = 0,1,2,3,5,6,7,8", "top = aaa" | exit 0 | exit 0 |
| slice_concat_join | slice concat/flatten/join of Box elements into fresh heap allocations, deep Vec clones, Box<str> joins, and a prefix-heavy sort of Box<str>s | success, output-includes "flattened = c1,c2,c3,c4,c5", "joined = s1-s2-s3", "concat = a|b|c", "cloned = c1,c2,c3,c4,c5 same-ptr = false", "repeated = rprprp", "names = aa,ab,ba,bb" | exit 0 | exit 0 |
| once_lock_big | OnceLock<Config> (Vec<String> + Box<u32> + Vec<Vec<String>>) initialized via get_or_init from 4 racing threads, exactly-once value observed deterministically | success, output-includes "t0: 4242 cfg-name-0 nested=n2", "t3: 4242 cfg-name-3 nested=n2", "final = 4242 6" | exit 0 | exit 0 |
| union_memory_moves | `#[repr(C)]` unions with pointer fields moved through MEMORY: inline-init + mem::swap, reference reads (#[inline(never)]), mem::replace, a struct wrapping a union passed by value, and unions shuffled inside a Vec -- the regression guard for change #23 (the Rust-ABI by-value union fix) plus the typed-alloca/pointer-chunk coverage of in-memory union moves; deliberately does NOT exercise the extern "C" by-value hole (known-issues #10) | success, output-includes "before: 11 22", "after: 22 11", "via ref: 22 11", "replace: 44 33", "consume = 66", "extra = 6600", "vec vals: 30,10,20,0 true" | exit 0 | exit 0 |
| aligned_aggregates | `#[repr(align(16))]`/`#[repr(align(32))]` aggregates holding pointers: Vec push/swap/pop, by-value pass-through chain, and fixed arrays (the typed-alloca path over over-aligned layouts) | success, output-includes "sizes 48 64", "aligns 16 32", "item = a3:3:3:m3", "popped = a1:1:1:m1", "passed = a1:1:1001:m1", "arr = a4:4:4:m4" | exit 0 | exit 0 |
| drop_unwind-opt | the `drop_unwind` program compiled with `optFlags: -O` | success; same output assertions as `drop_unwind` | exit 0 | exit 0 |
| thread_panic_join-opt | the `thread_panic_join` program compiled with `optFlags: -O` | success; same output assertions | exit 0 | exit 0 |
| use_after_free-opt | the `use_after_free` program compiled with `optFlags: -O` | failure; same output assertions as `use_after_free` | 133 | 42 |
| oob_write-opt | the `oob_write` program compiled with `optFlags: -O` | failure; same output assertions as `oob_write` | 133 | 42 |
| repr_enum_u8 (+ -opt twin) | `#[repr(u8)] enum E { A(Box<String>), B(u32) }`: both variants constructed, moved by value (sret returns, Vec growth, mem::swap/mem::replace), matched, payloads printed -- the change-#24 explicit-int-repr ICE guard (change #24's text has the mechanism); fails to COMPILE (rustc_abi assert, exit 101) if the closure bump is applied to explicit reprs on the first computation, and RUNS to prove the 4-mod-8 integer payloads do not pizlonator-panic (the recompute widens the tag) | success, output-includes "a = A(hello) b = B(3735928559)", "vec = A(vec-a), B(7), A(vec-c)", "swapped x = B(11) y = A(swap-me)", "replaced prev = A(replace-me) now = B(22)", "iter A len = 6", "iter B = 33", "eq = true neq = true", "debug = B(44)" | exit 0 | exit 0 |
| repr_enum_c_u8 (+ -opt twin) | the C-FFI tagged-enum pattern `#[repr(C, u8)] { Num(u64), Ptr(Box<String>) }` plus a misaligned twin `#[repr(C, u8)] { Small(u32), Big(Box<String>) }`, through the same by-value moves; same ICE guard as repr_enum_u8 | success, output-includes "n = Num(43981) p = Ptr(ffi-box)", "vec = Num(1), Ptr(v2), Num(18446744073709551615)", "swapped x = Num(77) y = Ptr(swap-ptr)", "replaced prev = Ptr(replace-me) now = Ptr(replaced)", "m1 = Small(12648430) m2 = Big(mixed-box)", "mixed swap = Big(mixed-swap) Small(5)", "mixed replace = Small(6) now = Big(m-repl)", "mixed vec = Small(9), Big(mv)", "eq = true", "debug = Num(3) Small(44)" | exit 0 | exit 0 |
| repr_enum_i16 (+ -opt twin) | `#[repr(i16)] { A(String), B }` (2-byte signed tag kept, String payload already word-aligned) plus `#[repr(i16)] { Small(u32) = 3, Big(Box<String>), Neg = -5 }` (recompute over a NEGATIVE discriminant), through the same by-value moves; same ICE guard as repr_enum_u8 | success, output-includes "a = A(i16-string) b = B", "vec = A(vec-a), B, A(vec-c)", "swapped x = B y = A(swap-me)", "replaced prev = A(replace-me) now = A(replaced)", "s1 = Small(3405705229) s2 = Big(signed-box) s3 = Neg", "signed vec = Small(9), Neg, Big(sv)", "signed swap = Big(signed-swap) Small(1)", "signed replace = Small(2) now = Neg", "eq = true", "debug = A(\"dbg\") Small(44)" | exit 0 | exit 0 |
| repr_enum_u32 (+ -opt twin) | `#[repr(u32)] { A(Vec<u8>), B }` plus a misaligned twin `#[repr(u32)] { Small(u32), Big(Box<String>) }` and a near-u32::MAX discriminant enum, through the same by-value moves; same ICE guard as repr_enum_u8 | success, output-includes "a = A([1, 2, 3, 4]) b = B", "vec = A([9, 8]), B, A([7, 6, 5, 4, 3, 2, 1, 0])", "swapped x = B y = A([42])", "replaced prev = A([100]) now = A([200])", "m1 = Small(4207849484) m2 = Big(u32-box)", "mixed swap = Big(u32-swap) Small(5)", "mixed replace = Small(6) now = Big(m-repl)", "mixed vec = Small(9), Big(mv)", "large = 4000000000 1 unit", "eq = true", "debug = A([1]) Small(44) Hi" | exit 0 | exit 0 |
| sort_by_key_enum (+ -opt twin) | the round-3 reviewer miscompile, EXTENDED: `#[repr(u64)]`/`#[repr(u8)]`/`#[repr(Rust)]` pointer-maybe enums `{A(Box<String>), B(u32,u32)}` sorted with the reviewer's key closure (`match e { A(_) => 0, B(a,_) => *a }`) via `sort_by_key` AND `sort_unstable_by_key`, at 3 elements (the reviewer's exact vector) and 100 elements (unique keys, so the unstable order is determined), post-sort payloads printed (String contents + integer fields) and every element's capability exercised by the drops -- fails with the null-object panic (exit 42) at `-O` if any of change #28's five `#[inline(never)]` boundaries is removed | success, output-includes "sbk3   = A(x),B(1,2)/B(3,4)/", "sbk100 = A(hundred),B(1,7)/... (full sorted 100)", "subk3  = A(x),B(1,2)/B(3,4)/", "subk100= A(hundred),B(1,7)/... (full sorted 100)", "e8sbk  = A(eight),B(1,2)/B(3,4)/", "e8subk = A(eight),B(1,2)/B(3,4)/", "ersbk  = A(rust),B(1,2)/B(3,4)/", "ersubk = A(rust),B(1,2)/B(3,4)/", "sort_by_key_enum OK" | exit 0 | exit 0 |
| sort_by_comparator (+ -opt twin) | the sort_by sibling of the round-3 miscompile: the same `#[repr(u64)]` pointer-maybe enums through the comparator API (`sort_by(|a, b| key(a).cmp(&key(b)))`), 3 elements, 100 elements through driftsort's quicksort+merge, plus a descending re-sort of the sorted vector and an ascending re-sort (the already-sorted detection paths) -- fails at `-O` if the element-move boundaries of change #28 are removed | success, output-includes "sby3   = A(x),B(1,2)/B(3,4)/", "sby100 = A(hundred),B(1,7)/... (full sorted 100)", "sbydesc= B(99,693)/.../A(hundred), (full descending)", "sbyasc = A(hundred),B(1,7)/... (full sorted 100)", "sort_by_comparator OK" | exit 0 | exit 0 |
| sroa_misaligned_ptr_piece (.ll) | the known-issue #13 POST-fix guard: the same 120-byte all-pointer-typed alloca (`<{ ptr x 15 }>`, the io::Result shape) with a typed ptr store at [8,16) and a 110-byte align-2 sockaddr_un-shaped copy into the payload at offset 4 (4 mod 8) crossing that partition, plus the Err-path pointer round trip -- as the POST-FIX rustc emits it: a `call void @zhas_union(ptr %res)` annotation on the alloca (clang's exact escape, CreateTempAlloca) and `zmemmove_builtin` for the payload copy and the sret copy (clang's exact memcpy/memmove builtin handling, CGBuiltin.cpp); compiled with the fixed `clang -O2 -x ir` command at the .ll convention.  The un-annotated version of this exact module is `sroa_misaligned_ptr_piece_unescaped` below, which panics with upstream SROA -- a shape that cannot be expressed in Rust or C source (clang escapes may-contain-pointer allocas before SROA sees them) | success, output-includes "ok"; output-excludes "bad" | exit 0 | exit 0 |
| sroa_misaligned_ptr_piece_unescaped (.ll) | the known-issue #13 NEGATIVE witness: the PRE-fix rustc IR shape of the exact module above -- typed all-pointer-struct alloca with NO zhas_union annotation, plain llvm.memcpy payload copy + sret copy chain (whose removal from the repro makes the panic vanish: the multi-SROA-run interaction is real) -- must KEEP panicking with upstream SROA's misaligned pointer-typed widening ("alignment requirement of 8 bytes not met ... expected 110 bytes with ptr aligned to 8 bytes at offset 4", exit 42 under FILC_EXIT_ON_PANIC=1; re-verified twice while deriving this fix) | return: failure, output-includes "filc safety error", "alignment requirement of 8 bytes not met" | exit 42 | exit 42 |

That is **121 test files** -- all running tests (the 113 of the previous
round with `net_tcp`/`net_unix` un-skipped and
`sroa_misaligned_ptr_piece.ll` present, plus the six known-issue-#13
union-mirror `.rs` tests (`union_mixed_members`, `union_struct_config`,
`align4_numeric_moves`, `misaligned_copy_into_aggregate` with its `-opt`
twin, `union_abi_carrier_fail`, `packed_pointer_fail`) and the
negative-witness `sroa_misaligned_ptr_piece_unescaped.ll` -- which the
`.ll` convention compiles with the fixed `clang -O2 -x ir` command) --
verified green with the final rustc build (recountable:
`filrustc/run-tests` prints 121 dots, no "Skipping" lines, and SUCCESS;
recount after any change)
back-to-back and again after the `build_all_fast_glibc.sh` gate rebuilt
Fil-C from scratch: the 53 tests of the earlier
rounds, the 32 std-coverage tests of the first coverage round, and the 8
std-coverage tests plus 4 `-opt` twins the previous round added, plus this
round's 4 `repr_enum_*` ICE guards + 4 `-opt` twins and the 3 gap-closer
coverage tests (`io_terminal`, `chown_fchown`, `backtrace_disabled`) (47
coverage-table
tests total, each with a companion `.manifest`; within the coverage table 12
of the 47 running coverage-table tests compile at `-O` and 35 at the default O0, and the
bug-guard tests got `-opt` twins so both levels are guarded -- an earlier
revision of this note wrongly claimed "6 of the new tests also run at `-O`":
in reality `optFlags` REPLACES the default flags, so a test's manifest picks
exactly one level, and the twins are how a test gets both levels), and the
round-3 sort-miscompile fix added `sort_by_key_enum` and
`sort_by_comparator` with their `-opt` twins (49 coverage-table tests
total; within the coverage table 12 of the 49 running tests have base
manifests compiling at `-O` and 37 at the default O0 -- `net_tcp`/`net_unix`
joined the runnable set (both at `-O`) when known-issue #13 was fixed on
the rustc side), and the known-issue-#13 round added the six union-mirror
tests (`union_mixed_members`, `union_struct_config`,
`align4_numeric_moves`, `misaligned_copy_into_aggregate` with its `-opt`
twin, `union_abi_carrier_fail`, `packed_pointer_fail`) for 56 `.rs`
coverage-table rows total (their opt levels: recountable from the
manifests -- all six run at the default O0 and
`misaligned_copy_into_aggregate-opt` is its `-O` twin; the `.ll` sibling
witnesses compile at the fixed `clang -O2 -x ir` level), and the
earlier rounds' `-opt` copies keep doing the same for their siblings.  Six .ll tests were REMOVED along with
the abandoned SROA-side fix (`sroa_memcpy_noalign`, `sroa_memcpy_small`,
`mixed_typed_memcpy`, `mixed_memset_memcpy`, `mixed_order`,
`sroa_memset_vector`): they existed to pin the behavior of the Fil-C
pointer re-typing that the abandoned patch added to
llvm/lib/Transforms/Scalar/SROA.cpp (the memcpy-only-partition fallback, the
rewriter-level keep-memintrinsics guard, the order-independent conjunction
gate, the always-keep-memsets rule -- see the "abandoned SROA-side attempt"
history in known-issues #1).  With SROA back at upstream, hand-written
untyped-alloca IR would simply trip upstream SROA's integer widening again
(the very behavior the rustc-side fix sidesteps by escaping the allocas), so
those IR-level tests are only meaningful against the fixed_sroa branch --
where they live -- and would fail or be vacuous here.  A LATER SROA-side
guard (the change-#29 predecessor: keep a memcpy piece when the widened
type carries pointers and the other side is sub-word-aligned) was also
REJECTED by the user and reverted -- same reason: the bug is rustc's, not
SROA's.  The regression coverage for the rustc-side fix is `.rs`-level via
`threads-opt` -- the exact formerly-miscompiling spawn+join shape, which
the typed-alloca path keeps correct, now with the pre-pizlonation block
running for rustc at `-O` -- and for the std fix (change #22) via
`swap_nodes`, which passes only while std swaps pointer-aligned regions in
pointer-typed `MaybeUninit<*const ()>` chunks (with the std fix temporarily
reverted -- byte-identical to upstream -- and the gate removed, it dies
with "filc safety error: cannot read source pointer with ptr < lower";
verified in both directions for this review round).  The suite's .ll tests
today are the known-issue #13 pair: `sroa_misaligned_ptr_piece` (the
POST-fix rustc IR shape -- typed all-pointer-struct alloca WITH the
zhas_union annotation and zmemmove_builtin copies -- passes byte-exact
through the fixed `clang -O2 -x ir` command) and
`sroa_misaligned_ptr_piece_unescaped` (the PRE-fix shape -- no annotation,
plain llvm.memcpy copies, including the sret copy chain whose removal from
the repro makes the panic vanish, i.e. the multi-SROA-run interaction is
real -- which MUST KEEP FAILING with upstream SROA's misaligned
pointer-typed widening; if it ever stops failing, upstream SROA changed
and the rustc-side annotations could be re-examined).  Both pin behavior
of UPSTREAM-plus-Skew SROA (llvm/ carries no Fil-C SROA delta), so neither
trips a nonexistent Fil-C guard nor is vacuous.  The `.ll` runner support
in `filrustc/run-tests` is a harness capability for regressions whose
shape cannot be expressed in Rust or C (see the runner description
above).

`no_prepopulate` proves the NoPrepopulatePasses pizlonation path,
`threads-opt` guards the rustc-side GIMSO fix from known-issues #1(a), and
`swap_nodes` guards the std fix for known-issues #1(b): the mem::swap
class is exactly what the std change exists for, and `swap_nodes` runs
that shape at `-O`.  The `exit101` return kind is new (added to
`filrustc/run-tests` alongside `exit13`): the generated `subRun.sh` asserts
that the exit code is exactly 101 -- the code std exits with when a panic
unwinds out of `main` and `lang_start_internal`'s `catch_unwind` catches it.
The unwinding tests are the suite-level guard for Fil-C Rust unwinding: the
panic must print its message, unwind through the pizlonated landing pads on
libpizlo's unwinder, run Drop glue in the right order, deliver a downcastable
payload to `catch_unwind`, and observe the standard Rust exit codes -- at
both optimization levels.  `oob_read`/`oob_write`/`null_deref`/
`use_after_free` are Fil-C safety panics (not Rust panics): they never
unwind, so their manifests stay `return: failure` (exit 42 under the
harness's `FILC_EXIT_ON_PANIC=1`).

Two investigations from the test-expansion round are worth recording
because they shape which tests are meaningful:

* **The `slice::rotate` memmove scratch (`type BufType = [usize; 32]`) is
  NOT reachable by the SROA integer-widening bug class.**  It is the last
  numeric-typed buffer in `library/core`/`library/alloc`/`library/std` that
  receives potentially-pointer bytes via memcpys, and it was a prime suspect
  (the rotate memcpys into it start at a constant offset 0, so a
  constant-size copy would be splittable and -- for an 8-byte pointer
  element -- its partition would hit SROA's `getIntNTy` fallback).  The
  blocker is inlining: `slice::rotate_left` (slice/mod.rs) is a `pub const
  fn` with NO `#[inline]`, so its IR is not exported to other CGUs and the
  pre-pizlonation inliner cannot inline it into user code (verified with
  `-C llvm-args=--debug-only=sroa`: `rotate_left` remains a separate
  function at `-O`, and a `rotate_left(1)` over `[Box<String>; 5]` still
  passes).  Inside the core CGU the parameters are runtime values, so the
  scratch memcpys have runtime lengths and are unsplittable for SROA (the
  rewriter only pointer-adjusts them).  Callers inside std
  (`VecDeque::make_contiguous`) also pass runtime lengths.  The
  `rotate_small_arrays` test pins this shape as a regression guard: if
  rotate ever becomes inlinable (or the scratch gets constant-size copies),
  the 8-byte-element rotates in that test are exactly the shape that would
  drop capabilities.  The same sweep of numeric-typed scratch buffers
  (done again for this round, beyond change #22's sweep) found nothing
  else: `filter_map::next_chunk`'s buffer is `[MaybeUninit<B>; N]`
  (element-typed) with runtime-index writes, `map_windows`' buffer is
  element-typed, `clone/uninit`'s copies are element-typed, sip-hash and
  fmt/num buffers move only bytes, and the sort scratches are element-typed
  (`MaybeUninit<[T; N]>`) or byte-typed with runtime offsets (which abort
  SROA's analysis for the alloca).

* **The pointer-bearing `union` bug class (change #23) was found by the
  `union_memory_moves` test and root-caused from `--debug-only=sroa`
  traces.**  The failing IR shape was rustc's own emission (it reproduced
  with `-C llvm-args=-filc-optimize=0`, i.e. without the pre-pizlonation
  block, and at O0): a `#[repr(C)] union U { p: *mut u8, i: u64 }` gets
  `BackendRepr::Memory` from the union ABI optimization (its two fields
  have different scalar reprs), so `adjust_for_rust_abi` casted the 8-byte
  union to an `i64` immediate at every by-value call boundary (`Vec::push`,
  `Box::new`, returns) -- the pointer crossed as a bare integer.  Validity
  evidence, both directions: reverting ONLY the change #23 guard (upstream
  cast restored) makes `union_memory_moves` die with "filc safety error:
  cannot read pointer with null object" right after its "extra = 6600"
  line (in the Vec section), and makes the by-value-return probe die in
  the same way; restoring the guard makes all 53 tests pass (twice).
  A sibling sweep confirmed the guard lives in the single generic path:
  `adjust_for_rust_abi`'s Memory branch is the only rule that turns small
  Memory-repr aggregates into integer immediates, it runs for every arch
  (the arch-specific `compute_rust_abi_info` pre-passes only adjust float
  soft-float/sign-extension details), and Scalar/ScalarPair -- where
  pointer halves already travel in ptr-typed registers -- are untouched.
  The `extern "C"` (SysV) variant of the same shape is verified to still
  drop the capability (an `extern "C" fn read_val(u: U)` dies with the
  same safety error) and stays documented as the remaining known-issues
  #10 hole, because fixing it would desynchronize rustc's extern "C" ABI
  from clang-compiled C code.

The twelve `-opt` tests (`hello-opt`, `catch_unwind-opt`, `threads-opt`,
`oob_read-opt`, `oob_write-opt`, `drop_unwind-opt`,
`thread_panic_join-opt`, `use_after_free-opt`, and this round's
`process_ops-opt`, `fs_ops-opt`, `fs_big_rw-opt`,
`option_result_combinators-opt`) are byte-for-byte copies
of their non-opt siblings whose
manifests add `optFlags: -O` (plain rustc's spelling of `-C opt-level=2`;
the runner interpolates the flag verbatim into the rustc command line), so
the same programs are compiled and run through the optimized pipeline with
the same expectations.  This is what guards the "the Fil-Pizlonator runs in
every pipeline, at every optimization level" claim at the suite level: the
optimized codegen must still catch the OOB read (the safety checks survive
optimization) and still produce correct output, and since the regular
optimization pipeline runs after the pizlonator (known-issues #1), `-O`
codegen is fully optimized rather than debug-only.  The harness's
`output-includes` checks are enforced for `return: failure` tests too (the
generated `subRun.sh` greps the captured output after the exit-code check),
so `oob_read-opt` proves the "filc safety error" spew specifically, not just
the exit code.  The three new twins exist because their originals' bugs
fired at BOTH optimization levels before their fixes: `process_ops` (the
known-issue #11 spawn-shape guard, whose manifest used to carry
`optFlags: -O` itself -- it now runs at O0 and the twin covers `-O`),
`fs_ops` (the known-issue #12 read-path guard, same treatment), and
`fs_big_rw` (the >8 KiB read_buf-cap guard added this round).  `swap_nodes`
is another `-O`-flagged test, but not a copy of
a non-opt sibling: it exists only at `-O`, because the mem::swap miscompile
it guards against only fires above O0 (known-issues #1(b)) -- at the debug
opt-level the program is correct even with the std fix reverted, so an O0
`swap_nodes` would be vacuous.

Notes on two tests that needed care:

* `null_deref.rs` must hide the null from rustc's own null-pointer check
  (`#[inline(never)]` helper returning `std::ptr::null()`, and
  `<*const i32>::read_volatile` for the load).  A plain `*p` on a null raw
  pointer panics in Rust ("null pointer dereference occurred") before the
  load ever reaches Fil-C; the volatile read through the opaque pointer
  reaches the pizlonated code and Fil-C reports "cannot read pointer with
  null object".
* `use_after_free.rs` reads through a raw pointer obtained before
  `drop(b)`: Fil-C's runtime recognizes the pointer as pointing to a freed
  object ("cannot read pointer to free object").  (`std::mem::forget` would
  leak, keeping the allocation live -- that would NOT panic.)

## Container audit (enter_container.sh vs. what a Fil-Rust build needs)

Audited: the generated container already installs everything a Fil-Rust
build needs -- enter_container.sh was NOT changed.  The Dockerfile heredoc
that `enter_container.sh -n` generates (ubuntu:22.04) was checked against
every external command that build_rustc_impl.sh, x.py/bootstrap.py, the
cmake/ninja LLVM build, and filrustc/run-tests invoke, and against the
actual running container (`command -v` sweep; everything found):

| needed by | commands | provided by (Dockerfile heredoc) |
|---|---|---|
| build_rustc_impl.sh | bash, coreutils (cd/rm/cp/mkdir/ln/tee/cat/sed/awk/grep/touch/printf), uname, nproc, `cc` (for `--print-file-name=libgcc_eh.a` — plus `libgcc.a` on aarch64 hosts — in the gcc-redirect) | ubuntu base + build-essential + gawk |
| projeny extract | `filc/projeny` (repo binary; needs libcurl.so.4 + libstdc++), rsync | libcurl4-openssl-dev (pulls libcurl4), rsync |
| LLVM for rustc | `build/bin/llvm-config` + `build/lib/libLLVM*.a` (incl. libLLVMMCA.a, libLLVMX86TargetMCA.a), produced by ./build_clang.sh — no LLVM is built by the rust build (the Fil-C LLVM build itself needs cmake/ninja/binutils and, on a build tree configured with valgrind detection, libvalgrind's headers — the `valgrind` package — or ninja mass-rebuilds LLVMSupport and fails on `valgrind/valgrind.h`; also: this build tree ships without most `.d` depfiles, so `touch` the sources whose headers changed to force recompiles) | -- (repo build tree) |
| patch verification | grep, awk, python3 (sha256) | grep/gawk, python3 |
| version composition | sed, git (remote get-url / rev-parse) | sed, git |
| x.py / bootstrap.py | python3 (>= 3.6; 22.04 has 3.10), curl (downloads stage0 from static.rust-lang.org), tar, gzip, xz, uname, sha256sum | python3, curl, tar, gzip, xz-utils |
| stage0 bootstrap build | the downloaded stage0 rustc/cargo (no host toolchain needed) | -- (downloaded) |
| libunwind step | filcc/fil++ (repo binaries in build/bin) | -- (repo) |
| linking rust programs | filcc -> pizfix runtime (repo artifacts) | -- (repo) |
| filrustc/run-tests | ruby (yaml/pathname/etc are stdlib), make (the -j nproc driver), nproc, sh, grep, cat, coreutils | ruby, make/build-essential |
| rust test binaries at run time | pizfix/lib/ld-fil1-x86_64.so + pizfix libs (baked-in absolute interpreter path; repo artifacts) | -- (repo) |

No missing package was found, so no change was made to enter_container.sh.
(Run `./enter_container.sh -n` if your container predates any image change;
the image tag hashes the generated Dockerfile, so stale containers are
flagged with a warning on attach.)

## ARM64 (aarch64) bring-up (2026-11-10)

The port now works on BOTH x86_64 and aarch64.  Everything above was
verified on x86_64 unless marked otherwise; this section records what the
ARM64 bring-up changed (nothing about the x86_64 behavior changes: on an
x86_64 host the generated config.toml and gcc-redirect are byte-identical to
the pre-ARM64 revision).

### What changed in the projeny patch (all ARM64-only or ARM64-parallel)

* NEW target spec
  `compiler/rustc_target/src/spec/targets/aarch64_unknown_linux_gnu.rs` —
  the aarch64 twin of the patched x86_64 spec (change #2 above): linker
  "filcc", pre-link "-normalcrt" (aarch64 needs no -m64),
  StackProbeType::None, SanitizerSet::empty(), unwind panic strategy, and
  the before-Fil-C data layout
  "e-m:e-ni:0-p270:32:32-p271:32:32-p272:64:64-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128-Fn32".
  The layout rationale: a probe of Fil-C clang `-emit-llvm` on aarch64 shows
  the after-FilC layout identical minus "-ni:0", and PassWrapper's
  `setDataLayoutAfterFilC` requires the module to carry `isFilC()` -- i.e.
  the "-ni:0" marker -- so the spec must carry it, exactly like the x86_64
  spec does (changes #2/#4; after pizlonation the layout is the same string
  minus "-ni:0", which is also LLVM's AArch64 TargetMachine default and is
  accepted by the MismatchedDataLayout validation).  features = "+v8a":
  upstream's "+outline-atomics" is deliberately DROPPED so atomics lower
  inline (IR-level atomicrmw/cmpxchg, the same lowering Fil-C clang produces
  for C atomics on this target) instead of through compiler-builtins'
  naked-asm LL/SC outline helpers -- with +outline-atomics every atomic
  operation in every Rust program would route into helpers the Pizlonator
  cannot accept (see the aarch64_linux exclusion below).
* cfg gates widened to include `target_arch = "aarch64"` alongside x86_64
  in: `library/compiler-builtins/compiler-builtins/src/probestack.rs` (the
  naked-asm stack-probe helper is compiled out on aarch64 too), and
  `library/std/src/sys/personality/gcc.rs` (the Fil-C personality is now
  used on aarch64 too -- three cfg sites; the personality body itself is
  target-independent: it only walks the pizlonated landing-pad data and
  installs pads by setting the two GRs, UNWIND_DATA_REG, which is X0/X1 on
  aarch64 instead of RAX/RDX).  `library/std/src/sys/pal/unix/stack_overflow.rs`
  needed NO change: it was already arch-neutral (its no-op `imp` module is
  selected by `not(all(target_os = "linux", target_env = "gnu"))`).
* `library/compiler-builtins/compiler-builtins/src/lib.rs`: the
  `aarch64_linux` module is compiled out with `#[cfg(any())]`.  Its
  `__aarch64_*` outline-atomics helpers are `naked_asm!`, which rustc lowers
  to MODULE-LEVEL asm (.pushsection/.weak/.type directives) that the
  Fil-Pizlonator cannot parse ("Invalid directive: .pushsection"), so
  compiler_builtins failed to build with it.  Nothing calls these helpers
  under Fil-C, since the target spec drops +outline-atomics (above).
* `library/core/src/hint.rs`: aarch64 `spin_loop!` no longer issues
  `__isb(SY)`, which becomes the `llvm.aarch64.isb` intrinsic -- unhandled
  by the Pizlonator, so every contended futex mutex/condvar/channel (whose
  slow path spins via lock_contended) aborted at run time with "filc safety
  error: Unhandled intrinsic: ... @llvm.aarch64.isb(i32 15)" (the threads,
  thread_local_cells and mpsc_large_messages tests all failed on this before
  the fix).  `spin_loop!` is a pure performance hint, so it is now a pure
  no-op on aarch64.  x86_64's `_mm_pause` arm is untouched (the Pizlonator
  handles llvm.x86.pause).
* `compiler/rustc_codegen_ssa/src/back/link.rs`: doc comments only (they now
  mention both target specs).

### What changed in build_rustc_impl.sh (Fil-C side, outside the patch)

* The script now derives HOST_TRIPLE from `uname -m` (x86_64 ->
  x86_64-unknown-linux-gnu, aarch64 -> aarch64-unknown-linux-gnu, anything
  else fails fast with a clear error) and keys config.toml's [build] target,
  the [target.<triple>] llvm-config section, the stage1 paths and the native
  libunwind cache path on it.  x86_64 output is byte-identical to before.
* THE LIBGCC REDIRECT CHANGED ON AARCH64: `rust-build/gcc-redirect/`
  libgcc_s.so on aarch64 is now `GROUP ( <libgcc.a> <libgcc_eh.a> )` instead
  of just libgcc_eh.a.  Reason: on aarch64, GCC's libgcc_eh.a has undefined
  LSE outline-atomics helpers (`__aarch64_swp8_acq_rel` etc.) that only
  libgcc.a defines (verified: `nm` shows libgcc.a's swp_8_4.o with
  `T __aarch64_swp8_acq_rel` and libgcc_eh.a's unwind-dw2-fde-dip.o with
  `U`); every host-side link (bootstrap build scripts) failed with
  "undefined reference to `__aarch64_swp8_acq_rel`" until libgcc.a was added
  first.  The GROUP order mirrors gcc's own `-lgcc -lgcc_eh` spec, and
  archive semantics make it duplicate-safe (a libgcc.a member is pulled only
  for a symbol nothing else has defined yet).  On x86_64 the redirect is
  UNCHANGED (byte-identical GROUP with libgcc_eh.a only) because x86_64's
  libgcc_eh.a is self-contained.
* One Fil-C JIT fix was needed for the full Fil-C glibc build on aarch64
  (Fil-C side, not the rust patch): alignment threading -- aarch64
  CPU-preferred function alignment can be up to 64, so the jit heap gained
  alignment-aware allocation (SHIM_API_VERSION 15 -> 16).  Relatedly, the
  filc/run-tests jit_gather test is SKIPPED on aarch64 (its manifest now has
  only-on-platform: x86_64): that test's host side casts the JIT'd function
  pointer to a C function pointer taking <8 x i32>-style vector arguments by
  value, which only coincides with the LLVM function type the test authors
  through the JIT API on x86_64 (SysV passes such vectors by value), while
  AAPCS64 passes >128-bit vectors by reference.  The LLVM JIT API exposes
  LLVM ABI, not C ABI, and the shim performs no ABI conformance, so the test
  is only valid on targets where the two coincide.  (Because of that skip, a
  full filc/run-tests pass on aarch64 is now 5518 runs rather than the 5519
  recorded above.)

### Verification results on aarch64 (8-core aarch64, 31 GB RAM)

* Clean `./build_rustc.sh` green (clean and incremental paths both
  exercised); `./filrustc/run-tests` 53/53 SUCCESS; `rustc --version` prints
  the Fil-C parenthetical; `patchelf --print-needed` on
  `librustc_driver-*.so` shows only libm/libc/ld-linux-aarch64 (no libgcc);
  a manual multi-threaded smoke test (spawn + join + Mutex) is clean.
* On the same aarch64 machine the full Fil-C glibc build is also green:
  build_base + `filc/run-tests` (5519 runs) + `build_all_slow.sh` (61
  packages) + `build_glibc_extras.sh` (17 packages, which includes
  `./build_rustc.sh`).

### ARM64 known issues / open items

1. aarch64 inline-asm support in the Pizlonator is limited: module-level asm
   and `llvm.aarch64.*` intrinsics are rejected (that is exactly why
   `aarch64_linux` and `spin_loop!`'s `__isb` are compiled out).  Simple
   register-constrained asm (e.g. libm's aarch64 vreg asm) compiles fine.
2. `is_aarch64_feature_detected!` uses `getauxval` (present in both the
   yolo and user glibc) but is untested.
3. The arch-generic known issues above apply unchanged on aarch64: extern
   "C" fat pointers (#3), `#[repr(packed)]` aggregates with pointers at
   non-8-aligned offsets (excluded from the typed-alloca fix, change #21),
   ThinLTO (#2), proc-macro crates (#9) and RUST_BACKTRACE (#4/#6).
4. x86_64 remains the primary/verified platform; aarch64 was verified with
   the same 53-test suite plus manual smoke tests.
