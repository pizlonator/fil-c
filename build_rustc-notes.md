# Fil-C rustc — build state & known issues

`build_rustc.sh` (repo root) builds a stage-1 rustc that pizlonates everything
it compiles, using the Fil-C LLVM and linking binaries with `filcc`.  It is a
thin entry point that calls `do_build_rustc clean` in `build_rustc_impl.sh`:
a clean build + test by default (it nukes `rust-build/` and rebuilds from
scratch; see "Build scripts" below).  `build_rustc_incremental.sh` is the
same thing without the nuke.  Output: `rust-build/rustc` (the compiler, a
symlink to the stage-1 rustc binary) and `rust-build/filrustc` (a symlink to
the same rustc; both spellings work and are equivalent -- the Fil-C behavior
is baked into the compiler).  Keep Fil-C's `build/bin` on PATH when using it
(`filcc` is the linker).  `filrustc --version` prints
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
gcc/g++/make, cmake, ninja, ruby (for the test suite) and the usual Fil-C
build prerequisites -- all of which the enter_container.sh image installs
(see "Container audit" below).  (An earlier revision of this note claimed a
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


## Build scripts (build_rustc.sh / build_rustc_incremental.sh / build_rustc_impl.sh)

The build is split into a shared library plus two thin entry points:

* `build_rustc_impl.sh` -- the shared library (not meant to be executed
  directly).  It resolves the repo root from its own `$BASH_SOURCE` (so
  sourcing it works regardless of the caller's CWD), defines the
  `check_grep` helper and `do_build_rustc <clean|incremental>`, and carries
  ALL of the build logic: the extraction of the patched Rust sources from the
  projeny checkout, the Fil-C LLVM swap (with its `.filc-llvm-stamp`), the
  ~24 `check_grep` fail-fast verifications of the projeny patch, the
  `FILC_VERSION` composition (plus the `.filc-version-stamp` handling), the
  config.toml generation, the `x.py build --stage 1` run, and the creation of
  the `rust-build/rustc` and `rust-build/filrustc` symlinks.
* `build_rustc.sh` -- thin entry: license header, `set -e -o pipefail -x`,
  source the impl, call `do_build_rustc clean`.  This is the default: it
  nukes `rust-build/` first and does the whole build from scratch.
* `build_rustc_incremental.sh` -- thin entry calling
  `do_build_rustc incremental`: identical, except it does NOT nuke
  `rust-build/`.  Extraction, the LLVM swap and the version stamp only redo
  work when what they produce is missing or changed, and x.py/cargo/ninja
  resume incrementally, so a re-run is usually a fast no-op; a missing
  `rust-build/` is bootstrapped from scratch, so the incremental script also
  doubles as a first build.

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
  optional companion `<name>.manifest` YAML file.  Manifests use the same
  vocabulary as a `filc/tests/<name>/manifest`: `return` (one of `success`,
  `failure`, `crash`, `dontCrash`, `exit13`, `compileFailure`,
  `compileOnly`, `linkFailure` -- the last is the same as `compileFailure`
  since rustc compiles and links in one step), `output-includes` /
  `output-excludes` (fixed strings that must / must not appear in the
  output; `grep -F` semantics), `optFlags` (extra rustc flags; default is
  the debug build, e.g. declare `-O` to test the optimized pipeline),
  `extraArgs` (extra rustc arguments), `extra-env` (extra run-time
  environment), `skip`, `slow` (run first, like filc), and
  `only-on-platform` (regexp against RUBY_PLATFORM).  A missing manifest
  means `return: success` with no output assertions.
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
  which the x86_64-unknown-linux-gnu target uses as the linker) is missing,
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
the path Rust's `abort()` takes, e.g. after a safe-Rust panic message).
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
| safe_panic | safe-Rust `v[10]` index panic | failure; Rust "panicked at" + "index out of bounds", "filc panic: user thwarted themselves."; output-excludes "Not implemented" | 133 (message, then clean abort) | 42 |
| threads | 8 threads x 1000 `Arc<Mutex<u64>>` increments | success, "sum = 8000" | exit 0 | exit 0 |
| vec_string | Vec/String/HashMap/iter/collect/format!/sort | success, three expected lines | exit 0 | exit 0 |
| hello-opt | the `hello` program compiled with `optFlags: -O` | success, output-includes "hello filc rust" | exit 0 | exit 0 |
| oob_read-opt | the `oob_read` unsafe OOB read compiled with `optFlags: -O` | failure; "filc safety error", "cannot read pointer with ptr >= upper.", "filc panic: thwarted..." | 133 (SIGTRAP, core) | 42 |

The two `-opt` tests are byte-for-byte copies of `hello`/`oob_read` whose
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
the exit code.

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
| build_rustc_impl.sh | bash, coreutils (cd/rm/cp/mkdir/ln/tee/cat/sed/awk/grep/touch), uname, nproc | ubuntu base + build-essential + gawk |
| projeny extract | `filc/projeny` (repo binary; needs libcurl.so.4 + libstdc++), rsync | libcurl4-openssl-dev (pulls libcurl4), rsync |
| LLVM swap | cp -a, git rev-parse | coreutils, git |
| patch verification | grep, awk, python3 (sha256) | grep/gawk, python3 |
| version composition | sed, git (remote get-url / rev-parse) | sed, git |
| x.py / bootstrap.py | python3 (>= 3.6; 22.04 has 3.10), curl (downloads stage0 from static.rust-lang.org), tar, gzip, xz, uname, sha256sum | python3, curl, tar, gzip, xz-utils |
| stage0 bootstrap build | the downloaded stage0 rustc/cargo (no host toolchain needed) | -- (downloaded) |
| LLVM build | cmake, ninja, g++/gcc, make, ar/ld/as/objcopy/strip/ranlib (binutils), pkg-config, python3, zlib1g-dev (optional system zlib), perl | cmake, ninja-build, gcc-12/g++-12 (default gcc/g++), build-essential + binutils 2.47 in /usr/local, make, pkg-config, python3, zlib1g-dev, perl |
| libunwind step | filcc/fil++ (repo binaries in build/bin) | -- (repo) |
| linking rust programs | filcc -> pizfix runtime (repo artifacts) | -- (repo) |
| filrustc/run-tests | ruby (yaml/pathname/etc are stdlib), make (the -j nproc driver), nproc, sh, grep, cat, coreutils | ruby, make/build-essential |
| rust test binaries at run time | pizfix/lib/ld-fil1-x86_64.so + pizfix libs (baked-in absolute interpreter path; repo artifacts) | -- (repo) |

No missing package was found, so no change was made to enter_container.sh.
(Run `./enter_container.sh -n` if your container predates any image change;
the image tag hashes the generated Dockerfile, so stale containers are
flagged with a warning on attach.)
