# Fil-C rustc — build state & known issues

`build_rustc.sh` (repo root) builds a stage-1 rustc that pizlonates everything
it compiles, using the Fil-C LLVM and linking binaries with `filcc`.  Output:
`rust-build/rustc` (the compiler, a symlink to the stage-1 rustc binary) and
`rust-build/filrustc` (a symlink to the same rustc; both spellings work and
are equivalent -- the Fil-C behavior is baked into the compiler).  Keep Fil-C's
`build/bin` on PATH when using it (`filcc` is the linker).  `filrustc --version`
prints `rustc 1.90.0 (1159e78c4 2025-09-14) (built from a source tarball)`
followed by the Fil-C parenthetical ` (Fil-C 0.687
git@github.com:pizlonator/fil-c.git <fil-c HEAD hash>)` -- mirroring what clang
appends to `clang --version` (see "Fil-C version identification" below).

Prerequisite: building requires a HOST rustup rustc/cargo (x.py's bootstrap
downloads its pinned stage0 compiler from static.rust-lang.org).  The
container has rustup 1.99.1 installed, with PATH persisted in
/etc/profile.d/zz-rust.sh.

This file is the tracked (git-added) copy of `rust-build/NOTES.md`; the
rust-build copy is kept in sync for the disposable build dir.

## Where the Fil-C changes live

ALL Fil-C changes to the Rust sources live in the projeny patch
(`projects/rust.projeny`); the checkout is `projects/rust/`.  To change the
sources: edit `projects/rust/...` and run `filc/projeny commit
projects/rust.projeny` (then `git add projects/rust.projeny`).  The build
script applies no source tweaks of its own; it only *verifies* that every
expected Fil-C change is present in the extracted tree (the `check_grep` calls
in `build_rustc.sh`) and fails fast with a clear
"projeny patch missing filc change X" error otherwise.

The patch consists of the following changes:

1. `compiler/rustc_llvm/llvm-wrapper/PassWrapper.cpp` — port of Fil-C clang's
   BackendUtil hooks: DataLayout-after-FilC setup at the top of
   LLVMRustOptimize; PipelineStartEPCallbacks (KillUB -> [Fil-C inline
   pipeline, disabled by default, see known-issues #1] -> coro lowering ->
   FilPizlonator) and OptimizerLastEPCallbacks
   (DeleteRedundantPollchecks).  Every pipeline path pizlonates.
2. `compiler/rustc_target/src/spec/targets/x86_64_unknown_linux_gnu.rs` —
   data_layout with `-ni:0` (before-Fil-C), StackProbeType::None,
   linker = "filcc", `-normalcrt` pre-link arg (full Fil-C CRT under
   -nodefaultlibs), sanitizers/xray off, and
   `base.panic_strategy = PanicStrategy::Abort` (see panic handling below).
3. `library/compiler-builtins/compiler-builtins/src/probestack.rs` — the
   naked-asm stack-probe helper is compiled out exactly for
   `all(target_arch = "x86_64", target_os = "linux", target_env = "gnu")`,
   mirroring the target spec (the only Fil-C target).
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

The projeny patch reproduces the build tree byte-identically: after
`filc/projeny commit`, `filc/projeny extract projects/rust.projeny /tmp/pc`
followed by `diff -rq /tmp/pc rust-build/rust` (excluding `src/llvm-project`
-- the swapped Fil-C LLVM -- and the generated `config.toml`, `build/`,
`.cargo` cargo-lock state and `__pycache__`) reports zero differences.

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

## Verification matrix (all from /tmp, `filrustc` + Fil-C build/bin on PATH)

* `filrustc --version` → `rustc 1.90.0 (1159e78c4 2025-09-14) (built from a
  source tarball) (Fil-C 0.687 git@github.com:pizlonator/fil-c.git <fil-c HEAD
  hash>)`; the first `-vV` line is the same and the `LLVM version: 20.1.8`
  line is still present.
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
  \"boom\"") and then terminate cleanly via abort: "filc user error: abort(3)
  called" + "filc panic: user thwarted themselves.", exit 133 — exactly what a
  plain C program calling `abort()` does under Fil-C (verified: the same C
  program exits 133).  NO "filc user error: Not implemented" and no
  `_Unwind_GetIP` anywhere.
* `-C panic=abort` — links and runs (hello: exit 0; safepanic: message +
  abort, exit 133).
* `-C panic=unwind` — fails cleanly with:
  `error: the crate `panic_unwind` does not have the panic strategy `unwind``
  (exit 1, no crash).
* Cross-DSO: `filrustc --crate-type dylib --edition 2021 lib.rs --crate-name
  mylib -o libmylib.so -C prefer-dynamic` links; `nm -D libmylib.so | grep -c
  pizlonated_` = 4: `pizlonated__ZN5mylib3add17h052cc356082cdec9E` and
  `pizlonated__ZN5mylib10get_answer17h43de340481e07935E` are DEFINED (T),
  and the two remaining pizlonated_ entries are undefined references to
  libstd getter symbols (`pizlonated__RNvCsugZpHxZGsu_7___rustc17rust_begin_unwind`,
  `pizlonated__ZN4core9panicking11panic_const24panic_const_add_overflow...`)
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
  "sum = 8000", debug and `-O`, exit 0.
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

## config.toml (written by build_rustc.sh)

change-id = 144675; [rust] llvm-libunwind = "no", lto = "off",
std-features = ["panic-unwind", "std_detect_file_io"] (std_detect_file_io is
what lets std_detect's x86 backend read /proc/cpuinfo — note that setting
std-features REPLACES bootstrap's default `["panic-unwind"]`, so panic-unwind
is repeated); [llvm] targets = "X86", download-ci-llvm = false,
assertions = true.
Note: dev/nightly channel defaults also enable experimental LLVM targets
(M68k/CSKY/Xtensa); `[llvm] experimental-targets = ""` would speed rebuilds.

## Panics: abort, not unwind

The target spec sets `panic_strategy = PanicStrategy::Abort`, and std is built
accordingly.  Rust-level unwinding cannot work under Fil-C (libpizlo only
implements a subset of the libgcc `_Unwind_*` API, and LLVM libunwind cannot
be built under Fil-C), so a panicking Rust program now prints its full panic
message and then terminates cleanly via `abort()` -- i.e. through Fil-C's
normal abort path ("filc user error: abort(3) called" + "filc panic: user
thwarted themselves", exit 133, same as a plain C program calling `abort()`).
The previous behavior died inside the unimplemented `_Unwind_GetIP`
("filc user error: Not implemented") after printing the message; that garbage
is gone.  Details:

* Plain panics (`v[10]`, `unwrap()` on Err, explicit `panic!`) print the
  message and abort (exit 133).
* `-C panic=abort` links and runs (std is an abort build).
* `-C panic=unwind` fails at link time with:
  `error: the crate `panic_unwind` does not have the panic strategy `unwind``
  (the sysroot's panic_unwind was itself built with the abort strategy; this is
  a clean error, not a crash).
* `catch_unwind` cannot catch panics (they abort); the test harness crates
  (`libtest`) are built with abort as well, so `cargo test`-style unwinding
  tests would abort on first panic.

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

1. **The Fil-C inline pipeline is disabled under rustc.**  Fil-C clang runs an
   extra pre-pizlonation pipeline above O0 (early SimplifyCFG/SROA/EarlyCSE,
   then a full inliner with SROA/EarlyCSE/JumpThreading/CVP/SimplifyCFG/
   InstCombine/GVN/SCCP/ADCE) to create inlining opportunities for the
   pizlonator's check elimination.  Under rustc, that pipeline makes the
   FilPizlonator miscompile ordinary code: `std::thread::spawn` + `join` at
   -O1 handed `pthread_join` a thread handle whose flight-pointer capability
   was reconstructed as null ("filc safety error: cannot read pointer with
   null object" inside `__pthread_clockjoin_ex`).  This reproduced with
   `-C no-prepopulate-passes`, i.e. with NO regular LLVM optimization running,
   and disappeared when the pre-pizlonation simplification was skipped, so
   the pre-pizlonation passes reshape rustc's IR into something the
   FilPizlonator cannot handle (plain Fil-C clang code runs the same pipeline
   and is unaffected -- it is an interaction specific to rustc-shaped IR).
   PassWrapper.cpp therefore runs only KillUB -> coro lowering ->
   FilPizlonator above O0; the regular optimization pipeline still runs after
   the FilPizlonator on the pizlonated IR, exactly as it does for every
   Fil-C clang compilation, so codegen is still fully optimized.  Set
   FILCRUST_FILCINLINE=1 to turn the pipeline back on (for debugging the
   pizlonator interaction).  A real fix would find the IR shape (suspects:
   rustc's noalias attributes, the inline verse-allocator fast path, or the
   load/phi structure of moved-out boxes) that breaks the pizlonator and fix
   the pizlonator (llvm/lib/Transforms/Instrumentation/FilPizlonator.cpp).
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
3. **`-C panic=unwind` and `catch_unwind` are unsupported** (see "Panics" above
   for the exact behavior).  This does not matter for the Fil-C safety story
   (Fil-C safety violations abort directly, before any unwinding).
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
   `dl_iterate_phdr` (all worked around above).
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
   (llvm/lib/CodeGen/MachineFunction.cpp).  Remember to
   `rm rust-build/rust/build/x86_64-unknown-linux-gnu/llvm/.llvm-stamp` after
   touching LLVM sources so bootstrap re-runs ninja.
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

## Verified demos (from /tmp, with Fil-C build/bin on PATH)

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
  see known-issues #1 for the pre-fix bug); 8 threads with Arc<Mutex>,
  HashMap/Vec/String/sort/trait objects all work (debug and -O).
* `is_x86_feature_detected!` works (debug and `-O`): a sweep queries every
  one of the 74 `@FEATURE`-queryable names (plus the `abm`/`avx512gfni`/
  `avx512vaes`/`avx512vpclmulqdq` aliases) and reports every feature the
  CPU + kernel actually support and "no" for the rest, all cross-checked
  against `/proc/cpuinfo` (see the verification matrix and known-issues #7).
* `panic!`/`unwrap` on failure prints the Rust panic message and aborts
  cleanly (exit 133, Fil-C's normal abort path; see "Panics" above).
* Cross-DSO: `--crate-type dylib` + client with `--extern`/`-C prefer-dynamic`
  links and runs (see "Cross-DSO" above).
