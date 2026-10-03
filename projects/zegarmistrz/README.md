<!--
Copyright (c) 2026 Filip Pizlo. All Rights Reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-->

# zegarmistrz — x86-64 user-mode emulator (phase 2: full ISA + tooling)

`zegarmistrz` ("clockmaker" in Polish) is a user-mode x86-64 emulator: it
loads a Linux ELF binary and interprets it instruction by instruction,
giving every guest memory access a common pipeline.

Phase 1 goal (done): run `./hello` (a Fil-C PIE binary with its own
dynamic loader) to print `Hello!`. Phase 2 (done): full user-mode ISA
through AVX512 + AES/SHA, valgrind-like poisoning, ELF core dumps, signal
conversion, CLI, parallel tests, `./build_zegarmistrz.sh`.

## Build & test

```bash
sudo apt-get install -y libzydis-dev libzycore-dev   # Zydis decoder
make -j$(nproc)        # builds ./zegarmistrz
./run-tests            # builds guests, runs 24 tests in parallel (xargs -P)
./check-isa.sh         # verify ISA coverage against binaries
./zegarmistrz <program> [args...]
./zegarmistrz -v ./hello                             # trace instructions
./zegarmistrz --trace-syscalls ./hello               # trace syscalls
./zegarmistrz --help                                 # full usage + poisoning ABI
```

`run-tests` (bash, `xargs -P`, `JOBS=` to tune, defaults to `nproc`)
runs: `filc-hello` (repo-root `./hello` must print `Hello!`),
`static-hi` (statically linked guest), `threads` (pthread
create/join + mutex), `args-exit` (argv passing + exit code 42),
`avx512-smoke` + `aes-smoke` + `vex-smoke` (native-vs-emulated
differentials, skipped if the host lacks the features),
`poison-{load,store,branch,index,syscall}` (each must dump core),
`poison-unpoison` + `poison-clear` (must exit 0), `cpuid-emulated` +
`cpuid-native` + `zeglib-emulated` + `zeglib-native` (CPUID presence and
the `include/zegarmistrz.h` client library, each run under the emulator
and natively), `poison-header` (header-library poison must fault under
the emulator; the native run exits 0), `core-gdb`
(`gdb -batch -ex bt` shows guest `boom` → `main`, no interpreter
frames), `no-core` (`--no-core` writes no core). It does not touch
`filc/tests`.

`./build_zegarmistrz.sh` (repo root) rebuilds and installs the binary at
`filc/zegarmistrz`.

## Layout

- `include/zegarmistrz.h` — header-only C client library for guests:
  `is_in_zegarmistrz()` (uncached detection via two inline-asm CPUIDs),
  `zegarmistrz_query()`, `zegarmistrz_poison_range()`,
  `zegarmistrz_unpoison_range()`, `zegarmistrz_client_request()`; all
  static inline, compiler-defensive (asm-opaque call target), all no-ops
  outside the emulator.
- `src/cpu.h` — `GPRValue` (`uint64_t val` + `cannot_branch` /
  `cannot_index` sidecars), `XMMValue` (64-byte ZMM + sidecars), `CPU`
  (gpr[16], rip, rflags CF/PF/AF/ZF/SF/OF/DF, xmm[32], fs/gs_base,
  mxcsr, minimal x87, thread bookkeeping).
- `src/mem.h`, `src/mem.cpp` — common memory pipeline
  (`mem_load8/16/32/64`, `mem_store*`, wide vector access,
  `mem_read_code`, `mem_copy`). Guest VA == host VA so the fast path
  is `memcpy`, but every access flows through here (poison checks +
  SIGSEGV/SIGBUS recovery via thread-local `sigsetjmp`, translated to
  guest core dumps). Poison map (`mem_poison_range`) + magic-call
  handler included.
- `src/elf_loader.h`, `src/elf_loader.cpp` — minimal ELF64 loader:
  maps each object's `PT_LOAD`s with `mmap(MAP_FIXED)` (anonymous +
  `pread`, then `mprotect` to final perms). `ET_DYN` bases come from a
  `mmap(NULL)` reservation; the main exe reserves +1 GB of contiguous
  brk growth room in the same reservation (so virtualized `brk` can
  never stomp host mappings). Loads `PT_INTERP` first, exposes entry /
  `AT_PHDR` (file-offset→vaddr translated) / `AT_ENTRY` / `AT_BASE`.
- `src/syscall.h`, `src/syscall.cpp` — `SYSCALL` emulation:
  number from `RAX`, args from `RDI,RSI,RDX,R10,R8,R9`, return in
  `RAX` (negative errno), `RCX`=`RIP`/`R11`=`RFLAGS` clobber.
  Virtualized: `brk` (per-process, capped at reservation),
  `arch_prctl` `SET/GET_FS/GS` (per-CPU, never touches host),
  `clone`/`clone3` (via `pthread_create`: copies CPU, new guest stack,
  TLS, `PARENT_SETTID`/`CHILD_CLEARTID` + futex wake; child resumes
  *after* the syscall with `RAX=0`), `exit` (ends just the thread:
  clears ctid + wakes *both* futex flavors + `pthread_exit`) vs
  `exit_group` (whole process), `rt_sigaction`/`rt_sigprocmask`
  (no-op success), `execve`/`fork`/`vfork` (`ENOSYS`). Everything else
  passes through host `syscall()` (futex, mmap/mprotect/munmap/madvise,
  openat/read/write/close/lseek/fstat, rseq/set_robust_list/
  set_tid_address, getrandom, prlimit64, ...). Tainted
  (`cannot_branch`/`cannot_index`) syscall args are rejected.
- `src/decode_exec.h`, `src/decode_exec.cpp` — fetch/decode/
  dispatch/execute. **Decoder: system Zydis** (CMake config
  `libzydis-dev`, no pkg-config file; `Makefile` falls back to
  `-lZydis -lZycore`). A global `LOCK`-prefix bus mutex (`g_bus_lock`)
  serializes `LOCK`ed RMWs plus implicitly-locked `XCHG`/`CMPXCHG8B`/
  `16B` across guest threads. `CPUID` masks AVX512/AMX/SHA (phase 1
  implements up to AVX2; guests then avoid EVEX paths); CPUID also
  reports zegarmistrz's presence (hypervisor bit + vendor leaf, see
  below). `RDRAND`/
  `RDSEED`/`CRC32` pass through to the host; `XSAVE/XRSTOR/FXSAVE`
  save/restore x87+SSE+AVX state (other components zeroed/skipped);
  shadow-stack reads report 0; `RDPID` returns 0. Unimplemented
  opcodes (EVEX/AVX512, AES, SHA, gathers, ...) fail cleanly as
  `unimplemented <mnemonic>`.
- `src/main.cpp` — CLI, Linux-ABI stack setup (argc/argv/envp/auxv
  with `AT_PHDR/ENTRY/BASE/HWCAP/RANDOM/EXECFN/...`, 16-byte aligned),
  main-thread CPU, interpreter loop. `SIGUSR1` dumps guest
  backtraces (debug aid).
- `tests/` — guest sources (`args.c`, `threads.c`, `static_hi.c`);
  `test-output/` — build/run artifacts.

## Presence (CPUID)

Per `cpuid.txt` (repo root), zegarmistrz reveals itself through the CPUID
hypervisor convention:

- `CPUID.1:ECX` bit 31 — the "hypervisor present" bit (reserved zero on
  real silicon) is set.
- Hypervisor leaf `0x40000000` returns the max hypervisor leaf in `EAX`
  (`0x40000000`) and the 12-byte vendor signature `"Zegarmistrz\0"` in
  `EBX:ECX:EDX`. Leaves `0x40000001`-`0x4000FFFF` read as zero — the
  host's own hypervisor leaves never leak through.

Guest-side detection is `is_in_zegarmistrz()` from `include/zegarmistrz.h`
(no caching; both CPUID instructions run on every call, in inline
assembly; gate on bit 31 first, then match the signature — exactly the
cpuid.txt recipe). Tests: `cpuid_vendor` runs both natively (no match
expected) and under the emulator (signature expected).

## RFLAGS / operand notes

- Flags via `__int128` arithmetic + parity fold; `INC`/`DEC` preserve
  `CF`; shifts set `CF`/`OF`/`ZF`/`SF`/`PF` per Intel (AF left alone
  where undefined); string ops honor `DF`; `ADC`/`SBB` implement
  carry/borrow chains exactly.
- Zydis reports some real operands as `IMPLICIT` (e.g. `AL` in
  `test al,1`, `CL` in `shl eax,cl`) and others as `HIDDEN`
  (`RFLAGS`, implicit `RAX/RDX`...). Handlers index the non-hidden
  operands in decoded order; `test`/`mul`/`div`/1-op-`imul` all work.
- `CALL` to `0x1410141014101410` is the poisoning client-request
  trap (request 1 = poison range, 2 = unpoison); otherwise `CALL`
  pushes the return address normally.

## Gotchas fixed in phase 1 (notes for phase 2)

- Guest `brk` must never `MAP_FIXED` outside a reservation (it once
  stomped host libc). The exe reservation now includes 1 GB of growth
  room; `do_brk` is capped at it.
- `clone3` children must resume *after* the syscall (`RAX=0`), not
  re-execute it.
- `CLONE_CHILD_CLEARTID` writes tid at start *only* for
  `CLONE_CHILD_SETTID`; for `CLEARTID`-only the kernel merely clears
  on exit — writing at start clobbered musl's `detach_state` and
  hung every thread spawn. Related: ctid wakeups must be issued in
  both shared and private flavors.
- `SYS_exit` ends one thread (`pthread_exit` after ctid
  clear+wake); only `SYS_exit_group` (or `exit` from the main thread)
  ends the process. `run_cpu_loop` must not `catch(...)` or it eats
  `pthread_exit`'s forced unwinding.
- `Makefile` tracks `src/*.h` dependencies — struct layout
  changes without a rebuild link incompatible objects (this once
  masqueraded as heap corruption).
- `SAR` (and any width logic): widths are bytes, not bits.
- Static (`ET_EXEC`) binaries need file-offset→vaddr translation for
  `AT_PHDR` (the loader's `_dl_phdr` comes from auxv).

## Gotchas fixed in phase 2 (notes for the future)

- Zydis exposes EVEX write masks (`{k}`) as explicit operands, even
  `{k0}` when unmasked (e.g. `vpaddd` decodes as
  `[dst, k, src1, src2]`). `evex_explicit()` skips non-destination
  operands equal to `avx.mask.reg`; the authoritative mask lives in
  `avx.mask` (mode + reg). Related: `vpcmpeqb` with a `k` destination
  is the mask-producing form (like `VPCMPB`+EQ), not the vector form —
  dispatch on destination kind.
- Zydis operand order is Intel order, but double-check per-opcode:
  `VPERMB/W` (and `VPERMD/Q`) compute `SRC2[SRC1]` (index in the
  *first* source) — verified on HW against the
  `_mm512_permutexvar_*` intrinsics. `SHA256RNDS2` hides its `b`
  source (`HIDDEN` visibility; `k` is the explicit mem operand) —
  verified against GCC's emission. `GF2P8AFFINE*` needs output
  bit-reversal plus per-64-bit-lane matrices (verified on HW), and
  legacy/VEX forms differ in which operand supplies bytes vs matrix.
  `INSERTPS`: `COUNT_S` is imm bits `[7:6]`, `COUNT_D` bits `[5:4]`.
  `VPTESTM/NM` are exact inverses: `M` sets bits where `(a AND b) !=
  0`, `NM` where `== 0` (verified on HW; the PTEST-CF analogy
  misleads).
- `vec_sources`/`vec_store_dst` assumed VEX means exactly 3 explicit
  operands; 4-operand VEX (`VSHUFPS`, `VBLENDPS`, `VCMPSS`, ...) silently
  took the legacy path (dst as a source). Now `n >= 3`.
- BMI `exec_bmi_simple` mixed up Intel operand positions (`BZHI` used
  the index as the value; `SHLX`/`RORX`/`PDEP` swapped src/count or
  src/mask). `ZydisMnemonicGetString` is lowercase (`vfmadd132ps`),
  and the FMA prefix parse was off by one (`p` starts at `'f'`).
- `_mm_aeskeygenassist_si128` needs a compile-time immediate: use a
  generated 256-case dispatch. `_mm_clmulepi64_si128` (not
  `_mm_clmull_epi32_si128`) on this GCC. PCLMUL intrinsics don't
  inherit function `target()` into lambdas — use a static helper.
- `ZydisMnemonicGetString` names for objdump's `vpcmpneqb` etc. are
  plain `VPCMPB` + imm (handle predicates, not names).
- Debug facility: `ZEG_DEBUG_MOD` needle resolution collided with the
  host's own libc mapping (same path!); resolve to the file-offset-0
  mapping, preferring the higher base (guest). Absolute ranges
  (`ZEG_DEBUG_OFF`) didn't work for static binaries (no interp base);
  base-0 now means absolute. ELF `PT_NOTE` `p_align` must be 4 —
  readers align notes by it (kernels use 4); 8 breaks `readelf`/`gdb`.

## Poisoning ABI (phase 2)

The canonical guest-side entry points are zegarmistrz_poison_range() /
zegarmistrz_unpoison_range() / zegarmistrz_query() from
include/zegarmistrz.h (header-only, no-ops outside the emulator); the
raw ABI below is what they emit.

Client request: `CALL` to `0x1410141014101410` with `RDI=op`,
`RSI=ptr`, `RDX=size`, `RCX=flags`; returns 0 in `RAX`:

- op 0: no-op/query, returns 0.
- op 1: poison each byte of `[ptr, ptr+size)` OR-ing the `flags`
  bitmask: bit0 `cannot-load`, bit1 `cannot-store`, bit2
  `cannot-branch`, bit3 `cannot-index`, bit4
  `clear-branch-on-store`, bit5 `clear-index-on-store`. Example:
  `(1, ptr, size, 1)` poisons cannot-load only.
- op 2: unpoison (erase entries in range).

Enforcement: `cannot-load`/`cannot-store` fault on any access through
the memory pipeline. `cannot-branch`/`cannot-index` taint loaded
`GPRValue`/`XMMValue`s (and RFLAGS taint when flag-setting ops consume
tainted inputs). Any ALU/vector computation propagates OR of inputs;
self-`xor`/`vpxor`/`xorps` (and self-`sub`) of the same register clear
taint (result is zero). Using `cannot-index` in any address
computation (base/index/scale, `SI`/`DI` of string ops, `RSP` of
push/pop, `RCX` of `rep`) faults; using `cannot-branch` as an indirect
branch/call/`ret` target or in a `Jcc`/`JCXZ`/`LOOP` condition faults;
either bit on a syscall argument faults. Stores propagate the value's
bits into the destination bytes (plus the matching clear-on-store
bit); storing (anything) to a location carrying a clear-on-store bit
clears that dimension's cannot bit (store sanitizes).
`cannot-load`/`cannot-store`/clear bits are sticky. All violations
throw guest errors → ELF core dumps.

## Status / phase 2

Works: `./hello` → `Hello!` (exit 0), static/dynamic/threaded guests,
full `run-tests` green (24 tests, parallel), zero compiler warnings.
Threads, futexes, TLS (`FS`), `rseq`, robust lists, `brk`, `mmap`,
AVX/AVX2, BMI/BMI2 (incl. `BEXTR`), x87 (+`FLDENV`/`FNSTENV`), `XSAVE`,
`RDPKRU`/`WRPKRU` — all exercised by the loader + libc + Fil-C runtime
startup.

Phase 2 additions:

- **ISA**: EVEX/AVX512 with `{k}{z}` masking (moves, logic incl.
  `VPTERNLOG`, `VPCMP*` + mask-form `VPCMPEQ*`/`VPCMPGT*`,
  `VPTESTM/NM`, arithmetic/minmax/avg/sad, shifts incl. variable,
  broadcasts incl. `MB2Q`/`MW2D`, permutes, blends, converts incl.
  saturating down-converts + `M2B`/`B2M`, `POPCNT`/`LZCNT`, `VPABS`,
  `VP2INTERSECT`, compress/expand, `VALIGN`, sub-vector insert/extract /
  shuffles, FP converts, gathers/scatters, `VMASKMOV`), opmask `K*`
  insns, AES-NI + VAES + `VPCLMULQDQ`, GFNI (C++ `GF(2^8)` + output
  bit-reversal, verified on HW), SHA1/SHA256 (host SHA-NI), FMA
  (single-rounding via `std::fma`) + `FMSUB`/`FNMADD`/`FMADDSUB`,
  F16C + BF16 (`RNE`), VEX classics (`PMADD`, `PHADD`, `PAVGB`,
  `PSADBW`, `PMULH*`, `MPSADBW`, `PCMPESTRI/M`, `EXTRACTPS`/`INSERTPS`,
  `HADD`/`ADDSUB`), `UD2`/`INT3`/`HLT` traps. `./check-isa.sh`
  confirms every mnemonic in `./hello`, test guests, system
  `libc.so.6`/`ld.so`, the Fil-C loader + `libc.so`, and all 111
  `pizfix/lib/*.so` is handled.
- **Unsupported (clean guest error, documented)**: AMX tiles, CET
  shadow-stack (`RSTORSSP`/`SAVEPREVSSP` would `#UD` natively without
  CET), Intel TSX (`XBEGIN` etc.; host has no RTM), AVX512-FP16
  (`...PH`/`...SH`), AVX512 VNNI/VP2INTERSECT-followons
  (`VPDP*`, `VPMADD52`, `VPMULTISHIFTQB`, `VPERMI2/T2`, `VRANGE*`,
  `VSCALEF*`, `VFIXUPIMM*`), SHA512, `XSAVE` supervisor states,
  `fork`, guest signal delivery.
- **Poisoning** (see ABI below): per-byte map, full taint through
  `GPRValue`/`XMMValue` + RFLAGS taint, enforced on loads/stores/
  branches/addresses/syscalls, with self-xor clearing.
- **Core dumps**: ELF `ET_CORE`
  `zegarmistrz.core.<processname>.<pid>` (`NT_PRSTATUS` guest regs,
  `NT_PRPSINFO` name, `NT_AUXV`, `PT_LOAD`s); `gdb`/`lldb` show guest
  frames only. Host `SIGSEGV`/`SIGBUS`/`SIGILL`/`SIGFPE` during guest
  execution convert to guest cores; `--no-core` /
  `--passthrough-signals` turns that off.

Not yet: `fork`, guest signal delivery, `XSAVE` compacted supervisor
states beyond AVX.
