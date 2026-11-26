// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# AGENTS.md

Agent/contributor-facing notes for the zegarmistrz x86-64 user-mode
emulator. `README.md` is the user-facing doc (what it does, how to build
and run it, what a fault looks like); this file carries the
implementation details, invariants, and gotchas.

## Source map

- `src/main.cpp` — CLI (`-v`, `--trace-syscalls`, `--stack-trace`/
  `--no-stack-trace`, `--no-core`, `--passthrough-signals`, `--core`,
  `--error-exitcode=N`, `--var-time-op=`, `--state-trace=FILE`,
  `--history[=VALUE]`), the
  `~/.zegarmistrz` config file (`read_config`: `coredump
  enabled|disabled`, `history ...`, `error exitcode N`, `error signal N`;
  parsed BEFORE the flags, flags win on conflicts, warnings only), core
  dumps DISABLED by default (config `coredump enabled` or `--core`
  re-enables; the setting gates only the core FILE -- the fault handler
  stays installed, so banner + stack trace + exit status still happen;
  `--no-core` is the same as the default, `--core` flips it back on, and
  the `coredump` directive never touches passthrough mode),
  `--passthrough-signals` as the only switch that removes the host signal
  handlers (it implies `--no-core`; faults kill the process like a real
  host fault), Linux-ABI initial stack
  (argc/argv/envp/auxv, 16-byte aligned), main-thread CPU, interpreter
  loop. `SIGUSR1` dumps a guest backtrace (debug aid).
- `src/elf_loader.{h,cpp}` — ELF64 loader: maps each object's `PT_LOAD`s,
  loads `PT_INTERP` first, exposes entry/`AT_PHDR`/`AT_ENTRY`/`AT_BASE`.
- `src/mem.{h,cpp}` — the one memory pipeline + the per-byte poison map +
  host-fault recovery (`sigsetjmp`-based). Passthrough mode
  (`mem_set_passthrough`, selected only by `--passthrough-signals`) is a
  separate policy from the core-dump setting: it skips installing the
  fault handler at all (so faults kill the process like a real host
  fault) while `--no-core`/the default only skip the core FILE. The
  guarded-read users (`zeg_unwind_step`, the stack-trace walk) refuse to
  walk in passthrough mode because the guards are not actually guarded
  there. The taint sidecar types also
  live here: plain `Taint`, `--history`'s `HistoryTaint` (one PC per
  step), and deep history's `DeepHistoryTaint` (recorded stacks per
  step) — every trail entry is a `ZegHistStep` (pc + recording seq +
  the recorded stack in `callers` for the deep modes; see the taint
  propagation rules + the --history section below);
  the mem API is templated on the taint sidecar type (all three
  instantiations explicitly defined in `mem.cpp`).
- `src/cpu.h` — `GPRValue` (value + sidecar), `XMMValue`
  (ZMM-width bytes + sidecar), `CPU` (gpr[16], xmm[32], opmask[8],
  x87 st[8], RFLAGS bits, fs/gs base, per-register x87/opmask taint
  sidecars, RFLAGS taint, thread bookkeeping) — all templated on the
  taint sidecar type.
- `src/decode_exec.{h,cpp}` — fetch/decode/dispatch/execute (~12k
  lines; Zydis-based). All the taint checking lives here. The engine is
  `template<typename TaintT> struct Engine` with exactly three explicit
  instantiations — plain `Taint`, `HistoryTaint` under
  `--history`/`=yes`/`=1`, and `DeepHistoryTaint` under `--history=N`
  (N > 1)/`=full` — with byte-identical bodies (only the sidecar type
  differs).
- `src/syscall.{h,cpp}` — `SYSCALL` emulation (number `RAX`, args
  `RDI,RSI,RDX,R10,R8,R9`, return negative errno in `RAX`,
  `RCX`=`RIP`/`R11`=`RFLAGS` clobber) with the virtualized list below,
  the poison-only pre-syscall buffer validation
  (`syscall_check_buffers`), and the post-syscall kernel-write extent
  modeling (`syscall_kernel_writes`, see the OS-emulation section).
- `src/core_dump.cpp` — ELF `ET_CORE` writer + the fatal-error banner.
- `src/dwarf_unwind.{h,cpp}` — self-contained DWARF CFI unwinder (no
  bfd.h; BFD stays in stack_trace.cpp): `.eh_frame`/`.debug_frame`
  parsing, the CFI instruction evaluator, a DWARF-expression subset, and
  `zeg_unwind_step` (callee register state -> caller register state; the
  primary stack-trace unwinder, see the fatal-errors section). Also
  hosts `zeg_history_record_stack`/`zeg_set_history_frames` for
  deep-history stack recording. Guest-memory reads go through
  `mem_copy_guarded`; every failure path returns false (safe in the
  crash-dump path).
- `src/stack_trace.{h,cpp}` — crash-time symbolized guest backtrace:
  the CFI-primary walk (RBP-chain + approximate-scan fallbacks), BFD
  symbolization with gdb's address-1 return-address convention, and the
  `--history` poison-history dump printer.
- `include/zegarmistrz.h` — header-only guest client library.
- `zydis/` — the vendored amalgamated Zydis distribution (`Zydis.h` +
  `Zydis.c`, Zycore inlined; see `zydis/README.md`).
- `tests/` guest sources; `test-output/` build/run artifacts (wiped by
  `make clean`); `run-tests`, `check-isa.sh`.

## Build

```bash
sudo apt-get install -y binutils-dev
make -j$(nproc)
```

- Zydis is vendored as the amalgamated distribution in `zydis/`
  (`Zydis.h`/`Zydis.c`, with Zycore inlined; v4.1.0). `make` builds it
  (`zydis/Zydis.o`; `-Izydis` plus the `-DZYDIS_STATIC_BUILD` /
  `-DZYCORE_STATIC_BUILD` static-usage defines; no `-lZydis`/`-lZycore`).
  Why: distro Zydis packages are often too old for the v4 API the emulator
  uses, which broke the build. Refresh the files wholesale on update —
  never hand-edit them (upstream generates them with
  `assets/amalgamate.py`); see `zydis/README.md`.
- **Makefile tracks `src/*.h` dependencies** (`src/%.o: ... $(wildcard
  src/*.h)`) — struct layout changes without rebuilding all objects link
  incompatible layouts (this once masqueraded as heap corruption).
- The BFD header/library pairing is a loaded footgun; see
  [the BFD pairing section](#the-bfd-headerlibrary-pairing-makefile)
  before touching anything binutils-related.

## Test

`./run-tests` (bash): builds the emulator, compiles the guests in
`tests/` into `test-output/`, then pipes the case list through
`xargs -P "$JOBS"` (default `nproc`; `JOBS=8 ./run-tests` to tune).
Output lines `PASS/FAIL/SKIP <case>` are tee'd to
`test-output/results.txt`; the runner exits nonzero on any FAIL.

How cases work:

- **Case name = guest file stem with `-` for `_`**: `poison-vt-div` runs
  `test-output/poison_vt_div` built from `tests/poison_vt_div.c`. Guest
  compile commands live at the top of `run-tests` (some guests need
  `-mavx512f` etc. for inline asm/intrinsics; instruction groups inside
  self-checking guests are gated on runtime CPUID so they run everywhere --
  EXCEPT shufpd_sdm's AVX-512 SDM-conformance battery, whose whole GUEST is
  emulator-only: the run-tests case `shufpd-sdm` never runs it natively,
  since the host may have no AVX-512 and EVEX would SIGILL; see the shufpd
  bullet in the ISA gotchas.  Corollary: a guest that RUNS NATIVELY must
  not be compiled with `-mavx512f` unless every 512-bit reach is
  CPUID-gated -- with the flag on the line, gcc may auto-vectorize any of
  the guest's plain C to 512-bit EVEX, which SIGILLs the native leg on the
  AVX-512-less hosts this suite targets (observed on megadeth's gcc with
  the old single-file shufpd guest: native-exit=132, emu-exit=0).  The
  natively-run shufpd guest is compiled `-mavx2 -msse3` only (no
  -mavx512f, so gcc cannot emit zmm at all) and run-tests objdump-audits
  it for zmm/EVEX/k-mask codegen as a build rule; insn-fixes-avx512 keeps
  its -mavx512f compile (its EVEX reach is CPUID-gated) with
  -fno-tree-vectorize as insurance against the same vectorizer path).
  This split is what resolves djb's Zen 2 report (`FAIL shufpd:
  native-exit=132 emu-exit=0` from his pre-split tree): on Zen 2 the
  split's `shufpd` guest compiles to 0 zmm/EVEX/k-mask instructions
  (266 ymm, 9 shufpd + 5 shufps at the time of writing), so the native
  leg cannot SIGILL there, while the AVX-512 battery it used to drag
  along lives in the emulator-only `shufpd-sdm` case.
- **Fault tests** (poison-load/store/branch/index/syscall, the masked
  vector ones, the var-time KNOWN ones, the poison-index-addr battery
  (cannot-index, memory-address sites), the poison-branch-ctrl battery
  (cannot-branch, control-flow sites), poison-combo,
  poison-launder-branch/index) expect **all** of: exit
  status 139 (guest errors surface as SIGSEGV), a core file
  `test-output/zegarmistrz.core.<stem>.<pid>`, and the expected reason
  substring (`cannot-index`, `var-time`, `cannot-branch`, ...) in the
  captured stderr. `poison-index-addr`, `poison-branch-ctrl`, and
  `poison-combo` cases carry an
  optional third field that **bans** a substring — used to assert the LEA
  exemption keeps `cannot-index` out of a var-time fault's stderr.
- **Clean-run poisoning tests** must exit 0 with their ok line:
  `poison-unpoison`, `poison-clear`, `poison-launder-clear`,
  `poison-index-lea`, `poison-index-nop`, and the poison-ortho
  orthogonality battery (one subtest per wrong-dimension use — a
  cannot-index value through the control-flow sites and a cannot-branch
  value through memory sites must all print `ortho-ok`).
- **Var-time mode gating tests**: `poison-vt-plausible` must be clean in
  the default mode and fault under `--var-time-op=plausible`;
  `poison-vt-mode3` (VPERMILPS, deliberately unclassified) must fault
  only under `--var-time-op=everything`; `poison-vt-exempt` must run
  clean even in `everything` mode; `poison-vt-clear` (clear-on-store)
  must exit 0.
- **Native-vs-emulator mirror guests** (avx512/aes smoke, insn-fixes,
  insn-djb, insn-forms, insn-fixes-avx512, poly1305-kat, dec-cache,
  malloc-churn, cpuid/zeglib `-native`) must produce byte-identical output
  natively and under the emulator.  `vex-smoke` is DUAL-PATH: on a host
  with AVX-512 it is a mirror (the k-mask guest runs natively); on an
  AVX-512-less host the native leg would SIGILL on the guest's k-mask
  instructions, so the case runs EMULATOR-ONLY against SDM-pinned expected
  output lines (grep -Fx each line; the pinned values were
  hardware-verified on an AVX-512 machine when the guest was written and
  the guest's inputs are constants).  `shufpd` is a mirror;
  `shufpd-sdm` is emulator-only (no native leg at all). `poly1305-kat`
  additionally needs OpenSSL dev headers + libcrypto and is SKIPPED (not
  failed) when that compile fails. `filc-hello` runs the repo-root Fil-C `./hello`
  (skipped if it is not an ELF, e.g. a Cosmopolitan APE image).
- `core-gdb` runs `gdb -batch -ex bt` on a produced core and asserts the
  backtrace shows the guest's `boom` → `main` and **no** interpreter
  frames. `stack-trace*` check the banner/trace/core ordering;
  `stack-trace-shlib` additionally asserts a fault inside a shared
  library the guest ld.so mapped symbolicates with the .so's function
  name + source file, and `stack-trace-inline` asserts the DWARF inline
  chain expands (innermost inlined function first, marked `(inline)`);
  `no-core` checks `--no-core`; `fault-diagnostics-nocore` runs the
  crash guest (unmapped write, a HOST fault class error) with an empty
  HOME and no flags: the banner, the `guest SIGSEGV (unmapped write)`
  reason and `[core dumps disabled]` must print with exit 139 and no
  core, and the same run under `--core` must write the file — the guard
  for the fault-handler-vs-core-file decoupling (a regression that skips
  installing the handler under the default kills the process with zero
  output).
- The whole suite runs with a private HOME (`test-output/config-home`,
  containing `coredump enabled`) exported before any case: the emulator
  now disables core dumps by default, and the suite config re-enables
  them so every core-expecting case sees cores exactly as before the
  default flip. HOME is exported ABSOLUTE because the emulator resolves
  `~/.zegarmistrz` against the guest process's cwd and most cases run it
  from inside `test-output/`.
- The `config-*` cases exercise `~/.zegarmistrz` directly (each sets its
  own HOME under `test-output/`): `config-default-nocore` (no config
  file: 139, the disabled message, no core), `config-coredump-enabled`
  (the directive re-enables dumps: 139 + core),
  `config-disabled-explicit` (explicit disable: same as the default),
  `config-history-all` (`history all` behaves like `--history=full`
  with no CLI flag: 139 + core + the poison-history block),
  `config-error-exitcode` (`error exitcode 42`: exit 42 + core + taint
  reason), `config-error-signal` (`error signal 9`: exit 137 + core),
  `config-cli-override` (`--error-exitcode=43` beats the config's 42),
  `config-bad-directive` (an unknown directive warns on stderr and the
  other directives still apply), and `cli-error-exitcode`
  (`--error-exitcode=42` with no config: exit 42 AND cores stay
  disabled).
- `unimplemented-exit-code` runs `tests/unimplemented.c` (raw EVEX bytes
  for VGETMANTSS, an instruction no handler covers): exit 132 (SIGILL)
  with the unimplemented reason + core under the suite HOME, still 132
  under a poison-only `error exitcode 42` config (the override must not
  touch unimplemented errors), and still 132 with cores disabled under
  an empty HOME.
- `history-math` runs the `--history` poison-provenance guest
  (`tests/history_math.c`: poison cannot-index, load, math-derive an
  address from the loaded value, index memory with it) and asserts the
  usual exit-139 + core PLUS the poison-history block with >= 2
  `in main ()` entries, at least one naming `history_math.c` — the
  history section is extracted for the count so the stack trace's own
  `in main ()` frames do not count toward the minimum.
- `history-store` runs the store-recording guest (`tests/history_store.c`:
  poison cannot-index, load through an xmm register, store back from the
  xmm — the durable store-note path — copy to a GPR, index memory with it)
  and asserts the usual exit-139 + core PLUS the poison-history block with
  >= 3 `in main ()` entries and >= 3 `history_store.c` lines (the load,
  the store and the copy PCs; the store PC present is the point — the
  section is extracted for the count as above).
- `history-memcopy` runs the memory-side provenance guest
  (`tests/history_memcopy.c`: poison a buffer, copy it byte by byte to a
  second buffer — load+store round trips — reload from the copy, index
  memory with the reloaded value) and asserts the usual exit-139 + core
  PLUS the poison-history block with >= 8 steps, >= 3 `in main ()`
  entries, >= 3 `history_memcopy.c` lines, and specific pinned lines: the
  poisoning-call line (`:22`), a copy-loop line (`:24`) and the reload
  line (`:25`) — without memory-side poison history (the per-byte trails
  in `mem.cpp`) the trail would shrink to the reload + fault-use PCs
  because the poison/copy PCs live only in the value sidecars the copy
  overwrote (the section is extracted for the counts as above). It also
  pins NON-duplication: the number of `step N:` headers must EQUAL the
  number of distinct PC addresses — in mode 1 one step header is printed
  per recorded PC (each recorded PC produces exactly one step, which may
  still print several numbered lines when its frame has an inline chain),
  and the numbered inline lines repeat only already-counted PCs, so the
  step count tracks the distinct-PC count —
  the union-dedup (`zeg_trail_add_pc`/`zeg_trail_add_stack`) must collapse
  the identical per-byte
  sub-trails of the 64 copied bytes, which consecutive-duplicate-only
  dedup concatenates into hundreds of repeated ops.
- `history-order` runs the trail-order guest (`tests/history_order.c`:
  poison a value, load it, fork the provenance into two registers via
  single-instruction asm on distinct source lines, derive an address
  from each fork, merge the forks with an OR used as a memory index —
  the stored trail is [poison-call, A, F, B, C, G, D, E, mask] in
  MERGE order (op_load stamps the current op PC into each operand
  sidecar copy before the union, so F lands before B/C and G before
  D/E), while the printed trail is the chronological
  [poison-call, A, B, C, D, E, F, G, mask]) under BOTH `--history` and
  `--history=full` and asserts, for each mode, the usual exit-139 +
  core PLUS that the first `history_order.c` line of steps 2..N
  strictly increases (the seq sort restores true chronological order —
  without the sort F prints before B/C and G prints before D/E) and
  that all seven op lines still appear (the seq fix changes only the
  printed order, never the recorded PC set).
- `history-off` runs the SAME guest WITHOUT `--history`: usual fault +
  core, and the poison-history block must NOT appear (default-mode
  invariance).
- `history-clean` re-runs the whole poison-ortho orthogonality battery
  under `--history`: semantics must be unchanged in history mode (every
  subtest exits 0 and prints `ortho-ok` with `HistoryTaint` sidecars).
- `history-syscall` mirrors `poison-syscall` with `--history` added:
  exit 139 + core, the `tainted value passed to syscall` reason, AND
  the poison-history block (the syscall boundary attaches the offending
  register's trail).
- `history-yes`/`history-one` run history-math's guest under
  `--history=yes` and `--history=1` (the explicit spellings of bare
  `--history`, mode 1): same minimums as history-math. `history-no`/
  `history-zero` run it under `--history=no`/`--history=0` (the off
  spellings, plain `Taint`): usual fault + core, and the poison-history
  block must NOT appear (like history-off).
- `history-2`/`history-full` run it under `--history=2` and
  `--history=full` (`DeepHistoryTaint`, stacks recorded per step via
  `zeg_history_record_stack`, frame cap 2 / whole stack): the history
  block must name `libc_start` in a recorded frame line (proof each step
  carries caller frames, not just the op PC) and the numbered-line count
  (recorded frame lines plus their numbered inline lines) must be at
  least 2 (history-2) / 4 (history-full — full stacks always reach the
  op frame plus `__libc_start_call_main`, `__libc_start_main_impl` and
  `_start`) per step line: every recorded frame prints its own
  PC-carrying numbered line under its `step N:` header, so a recorder
  that stopped at the op PC would break the bound.
- `history-bad` runs `--history=wat`: a usage error — exit 1 before any
  guest runs, the message names the flag, and no core is produced.

- `x86sem-suite` runs `x86sem-tests/run-x86sem-tests` (see
  `x86sem-tests/README.md`): the whole-state differential suite built
  from the kframework/X86-64-semantics single-instruction corpus (120
  imported `test.s` programs, verbatim plus an appended exit-syscall
  epilogue). Every corpus program is traced instruction-by-instruction
  on the hardware (`x86sem-tests/native-trace.c`: TF-flag self
  single-stepping, no ptrace; this container cannot ptrace so the
  gdb-script oracle cannot run here -- `ZEG_X86SEM_NATIVE=gdb` selects
  it where ptrace exists) and under the emulator
  (`--state-trace=FILE`), and `compare-traces.py` checks the whole
  architectural state (16 GPRs, the six defined flag bits, ymm0..15 as
  512-bit slots) after EVERY executed instruction. The sub-runner prints
  one PASS/FAIL/SKIP line per corpus dir plus the `x86sem:` tally and
  they all count in the run-tests tally. Make-based caching
  (`x86sem-tests/testmakefile`, regenerated per run): emu traces
  rebuild when ../zegarmistrz changes, native traces when the corpus or
  the oracle changes, cmp stamps when a trace or the comparator changes
  -- a no-change invocation costs ~0.2s. This case FAILS while emulator
  divergences remain; the running inventory is
  `x86sem-tests/TRIAGE.md` (regenerate with
  `python3 x86sem-tests/triage.py`), and per-test artifacts (traces,
  comparator detail, emu/native stderr) live under
  `x86sem-tests/test-output/x86sem/<dir>/`. The kframework-harness skip
  semantics the comparator ports (never-compared rip/rsp/rbp,
  stack-like rsi/rdi, the six flag bits, the NaN escape, the SDM
  undefined-flag table with a suppression window) are documented in
  the comparator header.  Two diagnostics to know about: (1) the native
  oracle SELF-TESTS its signal-frame vector-state extraction before every
  guest run -- all 16 ymm registers are planted with distinct known
  256-bit probe patterns and the pre-entry trap verifies the frame
  returned them bit-exactly (low half via the FXSAVE legacy xmm_space,
  upper half via the AVX xsave component gated on XSTATE_BV), dying with
  a loud "FP EXTRACTION SELF-TEST FAILED" naming the register and both
  halves otherwise, so a host/kernel whose xsave layout differs
  diagnoses itself instead of silently tracing garbage ymm slots whose
  divergences would masquerade as emulator bugs on that machine
  (NT_NO_FIXUP/NT_DIRTY_FP debug hooks skip the planting); (2) a
  divergence prints a ONE-LINE: summary in the dir's cmp.out which the
  sub-runner threads into the FAIL line -- `FAIL x86/<dir>: record N
  slot ymmK native=<hex> emu=<hex> rip=<hex> instruction='<objdump>'` --
  so remote failure reports are actionable without opening the traces
  (the multi-line detail and triage.py's parsing of it are unchanged).

`./check-isa.sh [binary...]` objdumps binaries (default: `./hello`, the
test guests, system libc/ld.so, the Fil-C loader + libc, all
`pizfix/lib/*.so`), normalizes AT&T mnemonics to Zydis style, and reports
mnemonics with no handler in `decode_exec.cpp` (it greps
`ZYDIS_MNEMONIC_*`). Run it after touching the decoder or after adding
guests.

### Debug facilities

- `ZEG_DEBUG_MOD=<base|name/needle>:<lo>-<hi>` (and `ZEG_DEBUG_MOD2`,
  `ZEG_DEBUG_OFF=<lo>-<hi>`) — enable instruction tracing inside an
  address range of a module (resolved via /proc/self/maps) or an
  absolute RIP range (see below for the needle-resolution gotcha).
- `ZEG_WATCH_TLS=<base>-<len>`, `ZEG_WATCH_VAL=<lo>-<hi>` — log stores
  into an address range / of 32-bit values in the range.
- `ZEG_TEST_CRASH_IN_TRACE=1` — test-only hook that faults inside the
  core-dump/stack-trace path (used by the `stack-trace-fault` case).
- `kill -USR1 <pid>` — dumps the guest backtrace of each thread.

### Adding a test

1. **Guest**: add `tests/<name>.c`. Convention for must-fault guests:
   print `NOTREACHED` (e.g. `write(1, "NOTREACHED\n", 11)`) right before
   the return that must never execute — a regressed check then surfaces
   as visible output / exit 0 instead of silence. Guests that run both
   natively and under the emulator must self-check (print `<case>-ok`,
   gate ISA groups on runtime CPUID, skip groups the host lacks).
2. **Compile line** at the top of `run-tests` (with the `-m` flags the
   guest needs; AVX512 intrinsics need `-mavx512f` etc. at compile time
   even though the emulator provides them at run time).
3. **Case**: `echo <case-name>` into the list and add a `case` block in
   the xargs bash snippet (copy an existing fault case or mirror case).
4. Re-run `./run-tests` (and `./check-isa.sh` if the guest uses new
   instructions).

## The memory pipeline and the poison map

Guest VA == host VA: the loader maps the guest objects at the addresses
the guest itself will use (PIE bases chosen from reservations), so a
guest pointer is directly usable as a host pointer and the fast path of
every accessor is `memcpy`. But **every** guest access — scalar
`mem_load8/16/32/64`, `mem_store*`, wide
`mem_load_bytes`/`mem_store_bytes`, string `mem_copy`, and code fetch
`mem_read_code` — goes through `src/mem.cpp`, which is where poisoning
and fault translation hook in. Do not add a `memcpy` fast path anywhere
else.

- **Poison map**: `static std::map<uint64_t,uint8_t>` keyed by byte
  address, mutex-guarded; each byte holds the 8 flag bits.
- `mem_check_load` / `mem_check_store` fault on cannot-load /
  cannot-store.
- `mem_get_taint(addr,size,&taint)` ORs the cannot-branch /
  cannot-index / cannot-var-time-op bits of a range into a `Taint`
  (loads use this to taint the loaded value).
- `mem_note_store(addr,size,taint)` implements store laundering (next
  section).
- **Fault recovery**: each emulator thread arms thread-local
  `sigsetjmp` recovery points (`FaultScope` around a raw access,
  `LoopScope` around `exec_one()`); host SIGSEGV/SIGBUS (and ILL/FPE at
  loop level) longjmp out and become `GuestError` ("guest SIGSEGV
  (unmapped read/write)") at the current guest RIP. `mem_set_dumping`
  disarms the loop recovery on the dumping thread only (see core dumps
  below). `guest_error()` throws `GuestError`, caught at the top of
  `run_cpu_loop`, which calls `dump_core`.

## Taint propagation rules

`Taint` is three bools (`cannot_branch`, `cannot_index`,
`cannot_var_time_op`), merged like OR.

- Every computation propagates the OR of its inputs' taints — integer
  ALU, vector ops, `op_load` from memory, moves. 32-bit writes zero the
  upper half but taint is a whole-register sidecar (not per-byte).
- Flag-setting ops that consume tainted inputs set `cpu->flags_taint`;
  `Jcc` checks it via `check_flags_taint`. `JCXZ`/`LOOP`/`rep` check the
  RCX counter's own taint directly (control flow = cannot-branch only;
  see the orthogonality invariant below).
- **Self-xor / self-sub clears**: `xor reg,reg` (and `sub reg,reg`, and
  the vector `vpxor xmm,xmm,xmm` family via `vec_self_op`) produce zero,
  so the result is untagged. This is load-bearing: compilers zero
  registers this way and the taint must not leak into arbitrary later
  state.
- **LEA propagates but does not check**: `exec_lea` ORs the base and
  index registers' taint into the destination while using
  `compute_ea` (see the cannot-index map below). The result of
  address arithmetic derived from a tainted register is itself tainted.
- **push/pop keep RSP's pointer taint separate from stored-value
  taint**: `push64` records the stored value's taint on the new stack
  slot but deliberately does NOT OR it into RSP (otherwise pushing a
  tainted value would taint RSP and every later RSP-relative access
  would false-positive in `resolve_mem`). `leave` copies RBP's taint
  into RSP; `enter`'s new RSP inherits old RSP + alloc-size + nesting
  taint.
- **x87 and opmask taint are per-register** (`st_taint[8]`,
  `opmask_taint[8]`), not per-byte, and **do not survive
  FXSAVE/XSAVE/FXRSTOR/XRSTOR snapshots** (those save/restore values
  only).

Under `--history` the sidecar is a history-carrying type instead of
plain `Taint` — the same three bits plus a bounded trail (the poison
provenance of the value): `HistoryTaint` (bare `--history`, `=yes`,
`=1`) trails the guest PCs of the operations; `DeepHistoryTaint`
(`--history=N` for N > 1, `=full`) trails STEPS, each step the guest
stack recorded at the operation — frame 0 is the operation's own PC from
the exec_one register snapshot (`zeg_history_pc`/`zeg_history_regs`/
`zeg_history_emu`), further frames are unwound at recording time by
`zeg_history_record_stack` (`dwarf_unwind.cpp`, the CFI unwinder),
capped at N frames (0/unlimited for `=full`, hard cap 256). Every trail
entry is a `ZegHistStep`: the operation's PC plus the global execution
SEQUENCE of the instruction that recorded it (`zeg_history_seq`, stamped
by exec_one next to `zeg_history_pc`, `zeg_history_seq_next()` bumps a
global atomic counter so sequences stay comparable across threads — a
trail can cross threads through the shared poison map's per-byte trails).
Recording rules: taint loaded from poisoned memory is seeded from
the set-unioned per-byte poison history of the loaded range plus the load
PC (`mem_get_taint`); every assignment/merge into a sidecar
(`operator=`/`operator|=`) records the move/math PC with seq-aware
set-union dedup (`zeg_trail_add_pc`/`zeg_trail_add_stack` in `mem.h`: a
PC or step already present anywhere in the trail is
not re-added — its seq is raised to the most recent execution — so loop
iterations and merges of same-provenance operands
do not inflate the trail); stores record the store PC into the sidecar
passed at each store site, by mutable reference through the engine's
`note_store_op` wrapper — where the site stages a copy of the value
(scalar `op_store`'s `Val`, `push64`'s parameter, staged vector results)
the note lands on that staging object and is transient with it (the
scalar reg->mem store path's note is transient by design: the staged
`Val` dies at the end of the handler), and a durable store note happens
only where the sidecar is a live register sidecar (vector stores from
xmm registers — `tests/history_store.c` pins this) — AND into the
destination bytes' per-byte poison history, which always survives the
staging question (see [--history: poison
provenance](#--history-poison-provenance)); `clear()` wipes the trail.
Caps: a trail holds at most 4096 entries/steps and keeps the MOST RECENT
ones (adding to a full trail evicts the entry with the SMALLEST seq — the
true oldest execution, however merges ordered the vector — so the
faulting site is never silently frozen out); the dump prints the most
recent 128. The trail VECTOR's stored order is merge order, not execution
order — merges assemble trails out of execution order (e.g. an
`op_load` sidecar copy of a tainted operand records the current op PC
into the copy before the union, so a two-operand merge interleaves the
lineages) — and the dump restores chronology by sorting the steps by
their seq before printing, so the printed trail reads in true
chronological execution order (every step consumes values produced at
earlier steps; consecutive steps may assign the same register when the
program chains back-to-back operations on it — the later step then
consumes that register's prior value (an rmw chain; the regression
guest's own trail contains four such pairs), and what the chronological
ordering eliminates is the scramble symptom from the original
complaint: a trail that placed an independent overwrite of a register
between its producer and the step that actually consumed that value;
strict per-step chaining — step I+1 using
exactly step I's value — is NOT guaranteed when provenance forks, since
each merged trail is the union of its lineages — e.g. in
`tests/history_order.c` the chronological step after the `and $0x2a` on
b consumes a/c, not that b); `tests/history_order.c` pins the
ascending-line ordering on a guest whose merge order scrambles.
Memory carries the provenance too: each poisoned byte owns a per-byte
trail (the poisoning request unions its call site into it, stores of
tainted values rewrite it, and unpoison and stores that zero a byte's
last taint bit erase it), so a value that round trips through memory
keeps the whole journey in its trail — `tests/history_memcopy.c` pins
the poison → copy → reload → use chain. The per-byte trails store
`ZegHistStep` entries in every history mode (pc + seq, with the recorded
stack in `callers` in the deep modes; `callers` is empty in
`--history=1`).

## Store laundering and clear-on-store

Precise semantics of `mem_note_store` (this is the valgrind-flavored
part; `tests/poison_launder_*.c` pin it):

- For each dimension B where the stored value is tainted, the destination
  bytes keep/gain B (and gain the matching clear-on-store bit if B was not
  already set, so a later clean store can sanitize a spill without weakening
  sticky user poison).
- For each dimension B where the stored value is clean, if the destination
  carries the matching clear-on-store bit, the store clears B there (store
  sanitizes).
- cannot-load / cannot-store / clear bits are sticky — stores never
  remove them (only `zegarmistrz_unpoison_range` does).

## The poisoning ABI

Client requests are `CALL`s to the magic address `0x1410141014101410`
with `RDI=op`, `RSI=ptr`, `RDX=size`, `RCX=flags`; result 0 in `RAX`.
`exec_call` intercepts the magic target **before** pushing a return
address and just advances RIP. (A `jmp` to the address would fault — the
guest header's opaque asm exists partly to stop the compiler turning the
call into a tail call.)

- op 0: no-op/query, returns 0.
- op 1: poison — OR `flags` (masked to 8 bits) into each byte of
  `[ptr, ptr+size)`:
  - bit0 `POISON_CANNOT_LOAD` (0x1)
  - bit1 `POISON_CANNOT_STORE` (0x2)
  - bit2 `POISON_CANNOT_BRANCH` (0x4)
  - bit3 `POISON_CANNOT_INDEX` (0x8)
  - bit4 `POISON_CLEAR_BRANCH_ON_STORE` (0x10)
  - bit5 `POISON_CLEAR_INDEX_ON_STORE` (0x20)
  - bit6 `POISON_CANNOT_VAR_TIME_OP` (0x40)
  - bit7 `POISON_CLEAR_VAR_TIME_OP_ON_STORE` (0x80)
- op 2: unpoison — erase the map entries in `[ptr, ptr+size)`.

Syscall arguments (RAX number + RDI,RSI,RDX,R10,R8,R9) reject
cannot-branch and cannot-index taint ("tainted value passed to
syscall") — including the syscall number itself. cannot-var-time-op is
deliberately not checked there: syscall timing is the kernel's.

## --history: poison provenance

`--history[=VALUE]` (`src/main.cpp` parses VALUE into
`emu.history_frames`) runs the whole engine on a history-carrying taint
sidecar instead of plain `Taint`: `main` calls `run_guest<Taint>` (no
flag, `=no`, `=0` — off), `run_guest<HistoryTaint>` (bare `--history`,
`=yes`, `=1` — one PC per step), or `run_guest<DeepHistoryTaint>`
(`=N` for N > 1 and `=full` — recorded stacks per step), and
`decode_exec.cpp`'s `template<typename TaintT> struct Engine` has
exactly three explicit instantiations (`Taint`, `HistoryTaint`,
`DeepHistoryTaint`; the bodies are unchanged — only the sidecar type
differs). `HistoryTaint` (in `src/mem.h`) is the three taint bits plus
`std::vector<ZegHistStep> history` — one step per recorded operation,
each carrying the operation's PC and its recording seq (`callers` is
empty in mode 1); `DeepHistoryTaint` is the same three bits plus
`std::vector<ZegHistStep> history` of STEPS, each step the recorded
guest stack (the operation's PC in `pc`, the rest of the recorded stack,
innermost first, in `callers`) of the operation plus its recording seq;
what
records and what does not is listed in the taint propagation rules
above. The current instruction's PC comes from the thread-local
`zeg_history_pc`, set at `exec_one` entry to the instruction's own
pre-advance RIP, and its global execution sequence comes from
`zeg_history_seq` set right next to it (history taints only; plain Taint
never touches the counter — `zeg_history_seq_next()` bumps a global
atomic so sequences from different threads compare consistently, which
matters because trails cross threads through the shared poison map);
the deep modes additionally snapshot the GPRs and
`Emulator*` there (`zeg_history_regs`/`zeg_history_emu`) for
`zeg_history_record_stack` (the stack recorder in `dwarf_unwind.cpp`,
which unwinds each step with the CFI engine), and `main` sets the
per-step frame cap once via `zeg_set_history_frames` (`=N` -> N,
`=full` -> 0/unlimited, hard cap 256).

- **What is attached at fault time**: the offending value's trail rides
  out in `GuestError::poison_history` — a list of STEPS, one per
  recorded operation (each a `ZegHistStep`: the operation's PC plus its
  recording seq, with the recorded stack in `callers` for the deep
  modes and an empty `callers` for `--history=1`) — and
  `run_cpu_loop` hands it to
  `dump_core`. The taint check sites in `decode_exec.cpp` attach it
  (via the engine's `poison_history_of` helper, which passes the
  history-carrying taints' `ZegHistStep` steps through unchanged and
  returns an empty trail for plain `Taint`, so every producer/consumer
  site is
  unchanged by the step format) and the syscall
  boundary does too (the arg check in `syscall.cpp`). The var-time
  operand report attaches its first offending operand's sidecar:
  `check_var_time` threads it out as `vt_off` — the operand scan, the
  implicit-GPR cases, the hidden x87 stack slots and the XRSTOR memory
  header all record into it, and for memory operands the trail is the
  merged per-byte poison history of the loaded range plus the load PC
  via `mem_get_taint`. The poison-map load/store faults in
  `mem.cpp` have no value to attach (the fault is about the memory, not
  a register value).
- **Memory-side provenance (per-byte poison history)**: the poison map is
  paired by byte address with a second map (`g_poison_hist`, same mutex)
  that carries each poisoned byte's trail of `ZegHistStep` entries (pc +
  seq, with the recorded stack in `callers` in the deep modes; empty
  `callers` in mode 1). The poisoning
  client request
  (op 1) unions the magic CALL's own RIP (with its execution seq) into
  every byte it poisons with
  a value-taint flag (CANNOT_BRANCH/CANNOT_INDEX/CANNOT_VAR_TIME_OP —
  CANNOT_LOAD/CANNOT_STORE-only or CLEAR_*-only ranges never produce
  value taints, so they skip the seeding): re-poisoning a byte ORs the
  new poison site into its provenance instead of replacing it (matching
  `mem_poison_range`'s bit-OR), and a byte erased by op 2 starts fresh;
  `mem_note_store` replaces a destination byte's trail with the stored
  value's trail plus the store PC whenever the store writes taint bits
  there (and erases trail + bits together when a store zeroes a byte's
  last taint bit), while a clean store leaves the trail alone (the
  sticky CANNOT_LOAD/CANNOT_STORE bits keep the original poison
  provenance meaningful); `mem_unpoison_range` erases trail with bits;
  and `mem_get_taint` seeds the loaded value's trail from the
  set-unioned per-byte trails of the loaded range plus the load PC (only
  for values that actually carry bits — a clean load from
  sticky-poisoned memory stays trail-free). The unions are seq-aware
  set-unions
  (`zeg_trail_add_pc`/`zeg_trail_add_stack`): a PC or step already present
  anywhere in a trail is not
  re-added (its seq is raised to the most recent execution), so N bytes
  with identical provenance merge to the distinct
  set of operations instead of N concatenated copies. This is what makes
  the trail survive memory round trips end to end: the key→key_copy
  memcpy in the openssl tests, a spill/reload through a stack slot, or
  any glibc internal buffer copy now records every hop.
  `tests/history_memcopy.c` pins the poison → byte-copy → reload → use
  chain. Plain mode never touches the second map (it stays empty).
- **Dump-time print**: `dump_core` calls `print_poison_history`
  (`stack_trace.cpp`) after the guest stack trace and before the core
  is written. The trail vector arrives in merge order, which is NOT
  execution order (set-union merges assemble trails out of execution
  order), so the printer first sorts the steps by their recording seq
  ascending — restoring true chronological execution order (every step
  consumes values produced at earlier steps; consecutive steps may
  assign the same register when the program chains back-to-back
  operations on it — the later step then consumes that register's prior
  value (an rmw chain; the regression guest's own trail contains four
  such pairs), and what the chronological ordering eliminates is the
  scramble symptom from the original complaint: a trail that placed an
  independent overwrite of a register between its producer and the step
  that actually consumed that value; strict per-step
  chaining — step I+1 using exactly step I's value — is NOT guaranteed
  when provenance forks, since each merged trail is the union of its
  lineages; `tests/history_order.c` pins the ordering). The block is
  `zegarmistrz: poison history of the offending value (N steps, oldest
  first):` followed by the BFD-symbolicated trail. Steps are labeled
  `step N:` (1-based over the whole SORTED trail, oldest first; when older
  entries are omitted by the 128-step cap, the printed steps keep their
  true position numbers) and EVERY displayed line of a step is numbered
  `#0`, `#1`, ... gdb-style — a continuous per-step counter over ALL
  displayed lines, restarting at `#0` under the step label — the step's
  first line opens with the step label and every further line prints
  under it, its `#K` (and so its PC and `in` columns) aligned under the
  step line's `#0` (while the per-step numbers stay at most two digits —
  below `#100` the `%-2zu` pad stops and a step exceeding 99 displayed
  lines shifts the PC and `in` columns). A recorded frame's symbolized
  chain prints one
  numbered line per entry, all repeating the frame's own recorded PC:
  the frame line prints `#%-2zu 0x0123... ` (rendered with the `%-2zu`
  pad space plus one literal space, so a 1-digit `#K` shows two spaces
  before the PC) followed by `in <func> () at <file>:<line>`, and the
  rest of the chain (its inline expansion) follows below — inline frames
  consume numbers exactly like the guest stack trace's inline frames do
  (which repeat the physical address; the trace's one counter runs over
  the whole trace instead of restarting per step) — marked `  (inline)`
  on all but the chain's last, physically-containing entry. A
  deep-history step's further recorded caller frames print below that as
  more numbered chains under the same `step N:` header (those frames are
  return addresses and symbolicate at address - 1 like the stack trace's
  caller frames; the step PC itself symbolicates unchanged). A frame
  that does not symbolicate falls back to the module+offset string on a
  numbered line that still carries the PC; a frame in no known module
  prints just its bare numbered PC prefix. Same module table and BFD
  machinery as the stack trace, capped at the most recent 128 steps with
  the omitted older count noted up front. In default mode (no
  `--history`) the block NEVER appears (`poison_history` is empty for
  plain `Taint`).
- **Performance**: `HistoryTaint` copies its trail on every merge
  (`operator=`/`operator|=` deep-copy the vector) — extremely expensive
  by design; only enable it under `--history`. `DeepHistoryTaint` (the
  deep modes) is heavier still: every poisoned operation records a stack
  through the CFI unwinder (`zeg_history_record_stack` — up to N frames
  per step, 256 for `=full`), and each step is a vector of PCs copied on
  merge. Memory cost is heavy too:
  in `--history` mode every poisoned byte carries a poison-map node plus
  a trail (of steps), so large poison ranges with long trails cost
  significant memory (debug-only — poison a narrow range, not the whole
  heap). The step trail caps at 4096; when it is saturated, adding a new
  step evicts the entry with the smallest seq (the most-recent-kept
  rule), and the dump sorts by seq and prints the most recent 128.

## The cannot-index / cannot-branch orthogonality invariant

**cannot-index and cannot-branch are ORTHOGONAL.** Each taint dimension has
exactly one job, and the check sites must not mix them:

- **cannot-index ⇔ memory addresses only**: `resolve_mem` base/index, RSP
  in push/pop/ret/call/leave (`push64`/`pop64`/`exec_ret`), RBP in
  `enter`'s level >= 2 nesting copies, RSI/RDI string ops (`check_si_di_taint`),
  BT bit offset, XLAT, VSIB index + scalar base (`vsib_base`),
  prefetch/clflush — plus syscall arguments (the documented boundary rule:
  `mem.h` says "nor passed to syscalls", and `syscall.cpp` rejects BOTH
  bits there).
- **cannot-branch ⇔ control flow only**: Jcc/LOOPE/LOOPNE flag conditions
  (`check_flags_taint`, "branch condition"), JCXZ/JECXZ/JRCXZ + LOOP
  counters (RCX), indirect JMP/CALL targets (`branch_target`, "branch
  target"), RET return address (`exec_ret`, "return address"), rep
  iteration counts (RCX, `check_rep_taint` — a count is a loop counter)
  and rep CMPS/SCAS compared data ("rep cmps" per-iteration check in
  `exec_string`).

Consequence: a cannot-index-poisoned value may be branched on, looped over,
called/returned through — and a cannot-branch-poisoned value may still be
dereferenced or used as an address. The regression case: glibc's
`__memcmp_evex_movbe` kmovd's a compare mask from poisoned ciphertext and
does `test %eax,%eax; jne` — before the orthogonality fix that trapped as
"tainted (cannot-index) value used in branch condition" and broke
`projects/simple-openssl-tests/test_aes_128_cbc --poison-variants=cannot-index`.
`tests/poison_ortho.c` pins both directions; `tests/poison_index_addr.c`
and `tests/poison_branch_ctrl.c` pin the faulting side of each dimension.

## The cannot-index enforcement map

**The invariant**: any instruction that actually touches memory goes
through `resolve_mem`, which checks the memory operand's base register
then index register for cannot-index taint and faults ("tainted
(cannot-index) value used as address base/index"), then delegates the
offset computation to the pure `compute_ea` and adds the fs/gs segment
base itself for the actual access. **LEA is the only instruction allowed
to call `compute_ea` directly** — it computes an address without ever
touching memory (compilers emit it for ordinary arithmetic like
`x = y + 42`), so it must not trip the checks, but it still propagates
taint into its destination. `compute_ea` follows real-LEA semantics and
computes ONLY the offset (base + index*scale + disp): segment overrides
are ignored, so a `lea %fs:0x10(%rax),%rcx` yields `rax+0x10`, not
`fs_base+rax+0x10`; the fs/gs base is added only in `resolve_mem` for
instructions that actually touch memory. (Zydis also normalizes LEA's
segment override away — it decodes a %fs:-prefixed LEA's operand with
segment DS — so `compute_ea`'s override-ignoring rule is behavior-
preserving twice over: real-LEA semantics by design, and the decoder
never hands LEA an FS/GS segment in the first place. `resolve_mem`'s
fs/gs addition is what matters: Zydis DOES report FS/GS for real
accesses like MOV and the prefetch family.) Anything new that touches
memory must go through `resolve_mem`; anything new that must not fault
(like the multi-byte NOPs) must bypass it like `exec_nop_like` does.

**The multi-byte-NOP exemption**: `exec_nop_like` handles NOP,
PAUSE/fences/SERIALIZE/CLAC/STAC/FNOP/FWAIT and the cache-control ops.
Long NOP encodings (`0f 1f /0` and friends) use an address encoding
purely as padding, so their decorative operand must NOT be resolved;
only CLFLUSH/CLFLUSHOPT/CLWB and the PREFETCH variants (which are ADDR
timing dependencies per the doc) resolve their address there.

**Bespoke check sites** — address components that do not flow through a
Zydis memory operand each have their own check:

- **RSP**: `push64` ("tainted RSP used by push"), `pop64` ("used by
  pop"), `exec_ret` ("used by ret"); `exec_call` pushes via `push64`;
  `exec_enter` pushes via `push64`; `exec_leave` copies RBP's taint into
  RSP and then faults in `pop64`.
- **ENTER's RBP nesting copies**: for nesting level >= 2 the copy loop
  reads `[RBP - i*8]` (a real memory access whose address depends on
  RBP), so `exec_enter` faults there on a cannot-index-tainted RBP
  ("tainted RBP used by enter" — the check sits inside the loop, so
  level 0/1 ENTER does no RBP-relative access and never fires it).
- **RSI/RDI** of string ops: `check_si_di_taint` ("tainted
  (cannot-index) RSI/RDI in string op").
- **BT bit offset** (memory form; address = m + (r >> 3)): "tainted
  (cannot-index) value used as BT bit offset".
- **XLAT** (`RBX` + zero-extended `AL`): "tainted (cannot-index) value
  used as XLAT table index/base".
- **Gather/scatter VSIB**: `vsib_base` checks BOTH sites — the index
  **vector** (XMM taint, "tainted (cannot-index) gather/scatter index")
  and the scalar base register ("tainted (cannot-index) gather/scatter
  base").
- **Syscall args**: see the ABI section above (in `syscall.cpp`); they
  reject BOTH cannot-index and cannot-branch — the deliberate boundary
  rule, not an orthogonality violation.

(Control-flow sites — branch targets, jcxz/loop/rep RCX, ret return
addresses — are NOT in this list: they belong to the cannot-branch
dimension, see the orthogonality invariant above. RCX in rep counts is a
loop counter, so `check_rep_taint` checks only cannot_branch there, then
the doc-4.4 cannot-var-time-op count rule.)

**Ordering invariant (INDEX wins for address issues)**: for an access
that is both tainted-indexed and cannot-load/cannot-store-poisoned (or
cannot-var-time-op-poisoned), the cannot-index error fires first —
`resolve_mem` runs when the address is computed, before the memory
pipeline's checks, and before a var-time fault on the same memory
operand (`var_time_operand_tainted` deliberately calls `resolve_mem`
and lets its checks fire first; see the comment at its memory-operand
case). The `poison-combo lea-div`/`mul-div` cases assert this with a
banned `cannot-index` substring.

## cannot-var-time-op: classification and enforcement

Classification uses the §4/§5/§6+§7 numbering of the x86
data-dependent-timing analysis that the code comments cite throughout
`decode_exec.cpp` (e.g. "doc 4.4", "§4.7", "§5 suspects"): §4
instructions are KNOWN data-dependent, §5 suspects are PLAUSIBLE,
§6+§7 are GUARANTEED constant-time, everything else is OTHER
(deliberately unclassified).
That analysis doc is not checked into the repo — **the live table is
`var_time_class()` in `decode_exec.cpp`, whose comment blocks are the
authoritative list** — so when you touch classification, update it
there and mirror the reasoning in a comment.

- KNOWN includes: DIV/IDIV, PDEP/PEXT (mask operand), the FMA family
  (subnormal assists), FP add/sub/mul/div/sqrt, denormal-sensitive
  conversions, the MCDT integer multiplies, gathers/scatters and
  masked-memory masks, x87 arithmetic, CPUID leaves, XSAVE-family
  bitmaps, `rep MOVS/STOS/LODS` counts (§4.4).
- PLAUSIBLE includes: RCL/RCR counts, ROUND*/MIN*/MAX*/CMP*,
  FCOM family, FLD from memory, CMPXCHG memory form (comparand RAX),
  LDMXCSR, XGETBV, INCSSP, expand-from-memory masks.
- GUARANTEED includes: RDRAND/RDSEED/PAUSE/fences/SERIALIZE/
  RDTSC/RDTSCP/RDPID, LOCK-prefixed ops, INT/UD, SYSCALL, the whole
  ALU/SIMD/crypto DOIT list, and every opmask `K*` instruction
  (`mn_is_kop`).

**`check_var_time(cpu,d)` runs in `exec_one()` before any handler
dispatch** (before the EVEX/MVEX diversion and LOCK handling), so the
var-time reason wins over an "unimplemented" error in `everything` mode.
Its order of business:

1. Branch family (Jcc/JMP/CALL/RET/JCXZ/LOOP...) returns early — CTRL
   dependencies stay on the cannot-branch checks; this also exempts the
   magic client-request CALL.
2. Form-aware carve-outs: `CMPXCHG` register form is guaranteed (§7.2)
   while the memory form is §5; `FLD` register form is data movement
   while `FLD` from memory is §5.
3. GUARANTEED → return. Then the **mode gate** (`cpu->emu->var_time_op`,
   enum in `emulator.h`: `ZG_VAR_TIME_KNOWN/PLAUSIBLE/EVERYTHING` =
   1/2/3): KNOWN traps in all modes, PLAUSIBLE needs `>= plausible`,
   OTHER only fires in `everything`.
4. Read-operand scan with Zydis visibility filtering (HIDDEN skipped,
   write-only skipped), with two skips: gathers/scatters skip their VSIB
   memory operand (a vector index is meaningless to `resolve_mem`; the
   index/base are INDEX-checked in `vsib_base` and the write mask is
   var-time-checked), and the XSAVE-side family (`XSAVE`/`XSAVEOPT`/
   `XSAVEC`/`XSAVES`) skips its memory image — it is an OUTPUT, and the
   doc scopes the memory-header VAL dependency "for XRSTOR" only.
5. Implicit operands not visible in the operand list: DIV/IDIV also
   check RDX:RAX (the dividend), CPUID checks RAX/RCX (leaf/subleaf),
   XGETBV checks RCX, the XSAVE family checks the EDX:EAX request
   bitmap (XRSTOR additionally checks the memory header
   XSTATE_BV/XCOMP_BV at `[ea+512, +16)`), CMPXCHG checks RAX,
   CMPXCHG8B RDX:RAX, CMPXCHG16B RCX:RBX vs RDX:RAX.
6. x87: ST(0) is HIDDEN to Zydis in many encodings, so `check_var_time`
   reads `st_taint[]` directly — KNOWN arithmetic checks the hidden
   ST(0) (plus ST(1) for the no-operand FPREM/FPREM1/FSCALE/FPATAN/
   FYL2X forms), the PLAUSIBLE compare set checks ST(0) when reached.

## The guest-side header library

`include/zegarmistrz.h` is static-inline and compiler-defensive:

- `is_in_zegarmistrz()` runs both CPUIDs (inline asm) on every call, no
  caching: gate on CPUID.1:ECX bit 31, then match the vendor signature
  at leaf `0x40000000`.
- `zegarmistrz_client_request()` no-ops (returns 0) unless detected;
  otherwise makes the magic call. An opaque `asm volatile` before the
  call hides the target/args from the optimizer and another one
  consumes the result after it (prevents a tail-call `jmp`, which would
  fault — the address is only meaningful as a CALL target).
- **Under `__FILC__`** the header instead declares the two entry points
  `extern`; they are provided by the Fil-C runtime (libpizlo) as natives
  `filc_native_is_in_zegarmistrz` /
  `filc_native_zegarmistrz_client_request` (registered in
  `libpas/src/libpas/generate_pizlonated_forwarders.rb`, implemented in
  `libpas/src/libpas/filc_runtime.c`). The runtime wrapper check-reads
  `[ptr, ptr+size)` before issuing the magic call from yolo code, so
  pizlonated guests use the same API safely. See the fil-c README for
  the runtime side.

## CPUID presence convention

Per the `cpuid.txt` convention (the recipe `include/zegarmistrz.h`
documents; implemented in `decode_exec.cpp`):

- `CPUID.1:ECX` bit 31 ("hypervisor present", reserved zero on real
  silicon) is set.
- Leaf `0x40000000`: `EAX` = max hypervisor leaf (`0x40000000` itself),
  `EBX:ECX:EDX` = the 12-byte vendor signature `"Zegarmistrz\0"`
  (`0x6167655A`, `0x73696D72`, `0x007A7274` — the constants live in
  `include/zegarmistrz.h` too).
- Leaves `0x40000001`–`0x4000FFFF` read zero: the host hypervisor's own
  leaves never leak through.

## Fatal errors, stack traces, and core dumps

On any fatal guest error, `guest_error` throws `GuestError`, caught in
`run_cpu_loop`, which calls `dump_core(cpu, reason, fault_addr, signo)`:

1. **Banner** to stderr: `zegarmistrz: fatal guest error (tid=%d): %s at
   rip=%#lx` plus ` fault_addr=%#lx` when nonzero.
2. **Guest stack trace** (unless `--no-stack-trace`).
3. **Core file** `zegarmistrz.core.<process-name>.<pid>` in the current
   directory, then the `zegarmistrz: [core: <path>]` line. Core dumps
   are DISABLED BY DEFAULT: `~/.zegarmistrz` `coredump enabled` or the
   `--core` flag re-enables them, and `--no-core` (the same as the
   default) or `--passthrough-signals` (or config `coredump disabled`)
   disables them again. The gate is `emu.no_core` alone -- the fault
   handler is installed regardless of it, so the banner and the trace
   print in every non-passthrough mode; only the file is skipped. With
   dumps disabled: `[core dumps disabled]`, no file.
   `--passthrough-signals` additionally removes the host signal handlers
   entirely (host-fault class errors then kill the process with zero
   diagnostics, like a real host fault).

The banner and trace print **before** the core is written, so the trace
is visible even if the core write is interrupted.

**Exit-code classification** (`core_dump.cpp`): the process exit status
encodes the reason class, so the shell can tell the failures apart:

- **139 (SIGSEGV)** — poison-class: poisoned cannot-load/store, tainted
  cannot-branch/index/var-time values, unmapped accesses, bad RIP,
  string-op taint (`zeg_reason_is_poison` keeps exactly the substrings
  `guest_fatal_signo` has always mapped to SIGSEGV).
- **132 (SIGILL)** — unimplemented-class (`zeg_reason_is_unimplemented`):
  `unimplemented <mnemonic> ...` (incl. the exec_evex fallback), decode
  failures, unsupported `bad ...`/`unsupported ...`/`cannot store`/
  `cannot determine` forms, MVEX/KNC. This was SIGABRT (134) before the
  classification landed.
- **134 (SIGABRT)** — everything else; host-signal reasons (the literal
  `SIGSEGV`/`SIGBUS`/`SIGILL`/`SIGFPE`/`SIGTRAP` substrings) keep the
  host signal. A reason matching BOTH classes is poison-class (poison is
  checked first — e.g. `bad string op` embeds the string-op taint name).

The valgrind-parity overrides apply to POISON-class errors only, so
poison stays distinguishable from unknown instructions even when they
are configured: `error exitcode N` (`--error-exitcode=N`) makes the
process exit N; `error signal N` makes it exit 128+N and records N in
the core's NT_PRSTATUS. Precedence: error_exitcode > error_signo >
natural 128+signo (`zeg_fatal_exit_code` in `core_dump.cpp`, called by
both `run_cpu_loop` catch sites — the second catch site's host-signal
reasons are other-class, so its override never fires, but it routes
through the same helper for uniformity). When BOTH overrides are
configured the exitcode override wins and the NT_PRSTATUS writeback is
gated on it NOT firing: the core then records the natural signo instead
of the unused signal override, so the recorded signal agrees with the
exit status actually used.

**Stack trace walk** (`stack_trace.cpp`): frame 0 is the exact faulting
RIP; then the DWARF CFI walk via `zeg_unwind_step` (`dwarf_unwind.cpp`)
— the same mechanism gdb uses on the core. For each frame, the module
containing its PC is looked up in the `guest_maps` snapshot (exact load
bias), that object's `.eh_frame`/`.debug_frame` is searched for an FDE
covering the PC, and the CFI is evaluated there to compute the caller's
register state (CFA = the caller's RSP, callee-saved registers restored
per the RA rule, caller RIP = the return address). Return addresses are
evaluated at address - 1 (both for the FDE lookup and later for
symbolization), so a call that is its function's last instruction still
finds the right row. The walk stops when there is no FDE for the PC, the
CFI/DWARF expression is unsupported, guest memory is unreadable, or the
unwound RA is 0. Only when CFI produces nothing beyond frame 0 (e.g. a
guest without usable `.eh_frame`/`.debug_frame`) do the legacy
fallbacks apply, in order: the guest RBP chain (`[rbp]` = caller rbp,
`[rbp+8]` = return address) via fault-guarded guest-memory reads,
validating every frame against the guest memory map (return addresses
must land in guest-executable mappings, frame pointers in read/write
mappings or the guest stack), then an approximate scan of the
guest stack for slots holding plausible return addresses — bytes before
the target look like a `call`: direct `E8 rel32` with an executable
target, or the full `FF /2` indirect-call family (optional REX, SIB,
disp8/disp32 including the RIP-relative mod==0 rm==5 form), matched by
exact encoding length; those frames print marked `(approximate)`.

**Symbolization**: BFD (`bfd_find_nearest_line`, plus `__cxa_demangle`
for C++ names) against the ELF objects recorded in `guest_maps` — the
main executable and the `PT_INTERP` interpreter (each `PT_LOAD` records
its load bias; `guest VA - bias == ELF vaddr`) plus file-backed guest
mmaps: shared libraries the guest ld.so maps (and dlopen'd objects)
record the real path + ELF load bias (`do_mmap` matches the mapping's
file offset against `p_offset - (p_vaddr & 0xfff)` of a PT_LOAD), so
those frames symbolicate like the main executable. Like gdb, frame 0
symbolicates at the faulting RIP while every later frame — a return
address, however it was unwound — symbolicates at its address minus 1
(gdb's "address in block"), so the reported line is the line the call
sits on rather than the first line after it; the printed address stays
the raw return address, and the `module+0xOFFSET` fallback keeps the
raw offset so it matches the shown address. Anonymous mappings
and the "guest-mmap"/"guest-brk"/"guest-mremap" tag entries, and file
mappings whose offset does not match a segment start (no exact bias),
still print raw — `module+0xOFFSET` is not available for them either;
addresses in a known object that do not symbolize print
`module+0xOFFSET`; stripped binaries usually print `module+0xOFFSET`
(the dynamic symbol table is a names-only last resort). Inline frames
expand: `bfd_find_nearest_line` returns the innermost inlined frame
and repeated `bfd_find_inliner_info` calls walk the DWARF chain
outward, so each physical frame prints its inline chain
innermost-first, the synthetic frames marked `(inline)` and scan-guessed
frames marked `(approximate)`.

**Core layout** (`core_dump.cpp`): `ET_CORE` with `NT_PRSTATUS` (guest
GPRs), `NT_PRPSINFO` (process name), `NT_AUXV` (the auxv placed on the
guest stack), and guest memory as `PT_LOAD`s (guest VA == host VA lets
the dumper memcpy directly). gdb/lldb show guest frames only — the
`core-gdb` test asserts no `zegarmistrz`/`run_cpu_loop`/`exec_one`
frames appear.

**Fault policy while dumping**: `dump_core` sets `mem_set_dumping(true)`
for the dumping thread, so a host fault inside the dump code itself
(e.g. in BFD) re-raises with default disposition and kills the process
immediately, instead of longjmp-ing back into the interpreter loop and
re-entering the dump forever. Fault-guarded guest-memory copies still
recover locally; other guest threads keep executing with their own
recovery. `ZEG_TEST_CRASH_IN_TRACE=1` injects such a fault for the
`stack-trace-fault` test (it must die with one banner and no core).

## The BFD header/library pairing (Makefile)

**CRITICAL**: the system `/usr/include/bfd.h` must pair with the system
`libbfd`. The Makefile:

- Force-includes the system header by absolute path
  (`-include /usr/include/bfd.h -DPACKAGE='"zegarmistrz"'
  -DPACKAGE_VERSION='"1"'`) and puts the **literal `.so` path** on the
  link line (located via `$(CXX) -print-multiarch`, with a
  `/usr/lib/*/libbfd.so` glob fallback, then `/usr/lib/libbfd.so`).
- `-isystem /usr/include` is NOT usable: it reorders the C++ header
  chain and breaks libstdc++'s `#include_next <stdlib.h>`. The forced
  `-include` wins over the default search order (where
  `/usr/local/include` comes first); both bfd.h flavors share
  `__BFD_H_SEEN__`, so the later `<bfd.h>` include in
  `stack_trace.cpp` is a no-op.
- Why the literal path: a bare `-lbfd` lets the linker's default search
  dirs (which include `/usr/local/lib`) silently bind a from-source
  `/usr/local/lib/libbfd.so` to the system header — **that combination
  compiles and links fine but symbolizes nothing at runtime**.
- Probe order: system multiarch pair first, then a from-source
  `/usr/local` pair (its own header + `/usr/local/lib/libbfd.so`) —
  that fallback is consistent and fine. Mixing the two halves is the
  bug. If BFD is missing entirely the Makefile errors with the
  `binutils-dev` hint.

## OS emulation: syscalls, threads, ELF loading

Virtualized syscalls (`syscall.cpp`):

- `brk` — per-process, capped at the main executable's reservation
  (`brk_limit == exe.reserve_end`). **Guest `brk` must never `MAP_FIXED`
  outside the reservation** (it once stomped host libc).
- `arch_prctl` `SET_FS/GET_FS/SET_GS/GET_GS` — per-CPU state, never
  touches the host.
- `clone`/`clone3` — via `pthread_create`: copies the CPU, new guest
  stack, TLS; honors `CLONE_PARENT_SETTID`/`CLONE_CHILD_SETTID`/
  `CLONE_CHILD_CLEARTID` and issues the ctid futex wake in **both**
  shared and private flavors. The child resumes **after** the syscall
  with `RAX=0` (never re-executes it).
- `exit` ends just the calling thread (ctid clear + wake +
  `pthread_exit`); `exit_group` (or `exit` from the main thread) ends
  the process. `run_cpu_loop` must **not** `catch (...)` or it eats
  `pthread_exit`'s forced unwinding.
- `rt_sigaction`/`rt_sigprocmask` — no-op success.
- `execve`/`fork`/`vfork` — `ENOSYS` (not yet).
- Everything else passes through to the host `syscall()` (futex,
  mmap/mprotect/munmap/madvise, openat/read/write/close/lseek/fstat,
  rseq/set_robust_list/set_tid_address, getrandom, prlimit64, ...).

Two poison-aware layers sit around that passthrough:

- **Pre-syscall buffer validation** (`syscall_check_buffers`): poison-only
  checks of known buffer layouts (rejects cannot-load reads and
  cannot-store writes of the syscalls it knows). Unknown syscalls stay raw
  passthrough there.
- **Post-syscall kernel-write modeling** (`syscall_kernel_writes` +
  `mem_poison_any`, gated on the poison map being non-empty so poison-free
  runs pay nothing): guest VA == host VA means a host syscall filling a
  guest buffer bypasses the poison map, so stale poison on those bytes
  would survive the overwrite and false-positive later branches (the
  original report: `fstat`'s `struct stat` landing on stack slots that
  deeper poisoned frames had spilled taint into; the `S_ISCHR` branch in
  `__GI__IO_file_doallocate` then faulted on the laundered poison). For
  every successful syscall the model clears the kernel's write extents
  (`mem_unpoison_range`): modeled syscalls list their extents exactly
  (stat family 144/256/120-byte structs, read-family `ret` bytes
  distributed across iovec arrays, poll `revents` shorts, epoll events,
  socket socklen writebacks clamped to `sockaddr_storage`, `getrandom`/
  `getcwd`/`getdents*`/`readlink*` `ret` bytes, clocks, rlimits, rusage,
  `mincore`, `PR_GET_NAME` plus the six 4-byte `PR_GET_*` pointer-writers
  (`PR_GET_PDEATHSIG`/`PR_GET_UNALIGN`/`PR_GET_FPEMU`/`PR_GET_TSC`/
  `PR_GET_ENDIAN`/`PR_GET_TAGGED_ADDR_CTRL`; the remaining `PR_GET_*`
  report through RAX and write nothing), `FUTEX_LOCK_PI`'s TID word,
  `MADV_DONTNEED`'s zeroed range, fcntl only the lock-query commands,
  ...); known no-writes
  are enumerated (`openat`/`close`/`write`/`mprotect`/`rseq` — the kernel
  writes the rseq ABI lazily at preemption, not at syscall time — etc.).
  **ioctl is a heuristic** in priority order: null arg3 writes nothing; a
  fixed table for classic non-`_IOC` requests (`TCGETS` 36 / `TCGETA` 18
  kernel struct sizes, `TIOCGWINSZ` 8, the int-out requests 4 bytes,
  setters nothing); any other old-style request is unmodelable;
  `_IOC`-encoded requests decode dir/size (`dir = (req >> 30) & 3`,
  `size = (req >> 16) & 0x3fff`; `_IOC_READ` means the kernel WRITES into
  the user buffer) and clear `size` bytes; anything else is unmodelable.
  Everything not classified REJECTS BY CRASHING — but only when poison is
  active and the syscall succeeded (a failed syscall wrote nothing): the
  message is `cannot model kernel-write extents of syscall <name> (<nr>):
  extend syscall_kernel_writes in src/syscall.cpp`. Extending the table is
  the fix; never add a guessed NONE for a syscall you are not sure writes
  nothing. Internal reads of guest control data (iovec arrays, socklen
  words, msghdr fields) go through `mem_copy_guarded` (never
  `mem_load_bytes`: the model must not fault on poison reading bytes the
  kernel already wrote), and a failed read makes the syscall unmodelable.
  Emulator-side writes are kernel-equivalent and clear poison at the write
  site: `do_mmap` (fresh mapping), `do_brk` growth (fresh anonymous
  pages), `arch_prctl` GET_FS/GET_GS writebacks, the clone parent-tid
  write, and both `CLONE_CHILD_CLEARTID` clear sites. `mremap` MOVES
  poison with the data (`mem_move_poison`: erase old range, clear
  destination, reinsert shifted with bits AND history trails intact;
  byte-granular ranges, not the page-rounded guest_maps bookkeeping), and
  its growth beyond the old size is fresh and cleared.
  `tests/syscall_unpoison.c` (the `syscall-unpoison` run-tests case, plain
  and under `--history=full`) pins the behavior: kernel-written bytes
  inside a poisoned buffer must come back clean. `tests/syscall_unpoison_ctl.c`
  (the `syscall-unpoison-ctl` run-tests case) pins the soundness half:
  the `timerfd_settime` `new_value` input the kernel never writes must
  STAY poisoned, so branching on it faults with a core.

No guest signal delivery yet (host signals are only consumed by the
fault-recovery machinery).

ELF loader (`elf_loader.cpp`):

- Each object's `PT_LOAD`s are mapped `mmap(MAP_FIXED)` anonymous +
  `pread`, then `mprotect`ed to final permissions. `ET_DYN` bases come
  from `mmap(NULL)` reservations; the main executable's reservation
  includes 1 GB of contiguous brk growth room in the same reservation
  (so virtualized `brk` can never stomp host mappings).
- `PT_INTERP` is loaded first; the loader exposes entry / `AT_PHDR` /
  `AT_ENTRY` / `AT_BASE`. `AT_PHDR` needs **file-offset→vaddr
  translation for `ET_EXEC`** static binaries — the loader's `_dl_phdr`
  comes from auxv.
- In the core file, `PT_NOTE` `p_align` must be 4 — readers align notes
  by it (the kernel uses 4); 8 breaks `readelf`/`gdb`.

## Interpreter/OS-emulation gotchas (learned the hard way)

- `SAR` and any width-sensitive logic: **widths are bytes, not bits**.
- `clone3` children resume after the syscall, not before.
- `CLONE_CHILD_CLEARTID` writes the tid at start only for
  `CLONE_CHILD_SETTID`; for `CLEARTID`-only the kernel merely clears on
  exit — writing at start clobbered musl's `detach_state` and hung
  every thread spawn.
- The thread-local **decode cache must not be instruction-visible**: it
  is `DecCacheEntry[(rip >> 2) & mask]` keyed by RIP only, so
  self-modifying code that rewrites a RIP between calls must pick up
  the new bytes (`dec-cache` test enforces this; a stale entry keyed by
  anything finer would break it).
- `ZEG_DEBUG_MOD` needle resolution: the host and the guest may map the
  same DSO at different bases (same path!) — resolve to the
  file-offset-0 mapping, preferring the higher base (the guest's). With
  no interp base (static binaries) `ZEG_DEBUG_OFF` ranges are absolute
  RIP ranges.
- The Makefile's `$(wildcard src/*.h)` dependency (see Build) — stale
  objects after a struct change masqueraded as heap corruption.
- Fault-guarded reads for the dumper must zero the failed tail and
  never throw: stale `guest_maps` entries must not crash `dump_core`.

## ISA/decoder gotchas (Zydis)

- **The djb supercop opcode batch** (regression-guested in
  `tests/insn_djb.c`, swept form-by-form in `tests/insn_forms.c` -- the
  `insn-djb` and `insn-forms` cases -- hardware-verified formulas):
  VCVTDQ2PD/CVTDQ2PD in all three encodings (legacy F3 0F E6, VEX, EVEX;
  the WIDENING convert's memory source is HALF the dest width: m64 for an
  xmm dest, m128 for a ymm dest -- verified on this host); the VEX variable
  shifts VPSLLVD/VPSLLVQ/VPSRLVD/VPSRLVQ/VPSRAVD (`exec_pshift_var`:
  per-lane counts, a count >= the element width yields 0/fill, NOT masked
  -- same rule as the EVEX `exec_evex_shift`); PMULHRSW/VPMULHRSW (the
  rounded-high SSSE3 multiply, not PMULHW); MOVDDUP/VMOVDDUP (each even
  64-bit lane duplicated into the odd lane); LDDQU/VLDDQU (SSE3 F2 0F F0 /
  VEX F2 0F F0: architecturally an unaligned MOVDQU-class load, the
  cache-line hint is not software-visible -- routed through
  `exec_vec_move`; the legacy form keeps the upper YMM, VEX zeroes above);
  PEXTRB/W/D/Q with a MEMORY destination (a legal form the old code
  rejected with "bad pextr form" -- store the selected lane, taint
  propagates, `tests/pextr_taint.c`); and MOVQ xmm1, xmm2 reg-reg (both
  the F3 0F 7E load and 66 0F D6 mod=11 store spellings, both zeroing the
  dest's upper 64 bits and carrying the sidecar, `tests/movd_taint.c`).

- **SHUFPD selects WITHIN one source per output qword, SHUFPS is the
  inverted-role PS twin** (the serious-bug regression): legacy SHUFPD
  (66 0F C6) and VSHUFPD (VEX.128/256/EVEX.512) compute
  `dst.q[0] = imm[0] ? DEST.q[1] : DEST.q[0]` and
  `dst.q[1] = imm[1] ? SRC.q[1] : SRC.q[0]` -- the low qword selects
  WITHIN DEST (VEX: src1) and the high qword WITHIN SRC (VEX: src2), so
  with DEST=[A,A] and SRC=[B,B] every imm8 yields [A.lo, B.lo]
  (hardware-verified; gcc -O0 folds _mm_shuffle_pd imm0 to UNPCKLPD and
  imm3 to UNPCKHPD, pinning DEST.lo / SRC.hi as the imm=0 endpoints). The
  old exec_shufpd PD branch picked the SOURCE by the imm bit instead
  (`[imm0?SRC.q0:DEST.q0, imm1?SRC.q1:DEST.q1]`), swapping halves on
  imm=1/3 -- gcc -O3 builds of djb supercop crypto_core/
  keccakf160064bits/reference1600 hit this eight times per Keccak round
  (theta column D values via punpcklqdq + shufpd $1) and silently permuted
  the whole 1600-bit state differently from bare metal, no fault. The imm
  bits index GLOBAL qwords at 256-bit (VEX.256 hardware: imm=5 gives
  [src1.q1, src2.q0, src1.q3, src2.q2] -- the upper lane uses imm bits
  2/3, so `sel = (order >> ((L / 8) + i)) & 1`; EVEX.512 follows the same
  global rule per the SDM), while SHUFPS/VSHUFPS
  RESTARTS its bit pairs per lane (imm=0x11 at 256-bit gives lane1 =
  [src1.d1, src1.d0, src2.d1, src2.d0]): output dwords 0/1 select WITHIN
  DEST by bits [1:0]/[3:2] and dwords 2/3 WITHIN SRC by bits [5:4]/[7:6]
  (the PS roles are the opposite pairing of the PD roles). Regression
  guests (SPLIT so no natively-run guest needs -mavx512f -- with that flag
  on the compile line gcc may auto-vectorize the guest's plain C to
  512-bit EVEX and SIGILL the native leg on the AVX-512-less hosts this
  suite targets; without it gcc cannot emit zmm at all, so dropping the
  flag is sufficient by construction): `tests/shufpd.c` (the `shufpd`
  case: legacy/VEX SHUFPD + SHUFPS + the VPERMIL imm/var sweeps + the
  full 24-round reference1600 permutation end to end with the FNV-1a
  hash, compiled `-O3 -mavx2 -msse3` and mirror-checked) and
  `tests/shufpd_sdm.c` (the `shufpd-sdm` case: the AVX-512 SDM
  battery, EMULATOR-ONLY -- no native leg). The battery (masked
  `{k1}`/`{k1}{z}` VSHUFPD,
  the `{1to8}`/`{1to16}` embedded broadcasts, masked+broadcast VPERMILPD,
  the EVEX VPERMILPD/PS imm forms, the widening/narrowing EVEX converts
  in both broadcast and masked forms, the masked `{k1}`/`{k1}{z}` VPSHUFD
  dword-granular merge/zeroing, and the ternlog family: the unmasked
  `vpternlogq ..., m512{1to8}, ...` broadcast form plus the masked
  VPTERNLOGQ (qword-granular) and VPTERNLOGD (dword-granular) merge/{z}
  probes, plus the have_avx512f()-gated unmasked EVEX.512 VSHUFPD imm
  group) is gated on
  `is_in_zegarmistrz()` and therefore runs ONLY under the emulator -- the
  host may have no AVX-512, so the EVEX forms would SIGILL natively, while
  the emulator implements AVX-512 in software on any host (the case
  requires exit 0 + the `shufpd-sdm-ok` line + the executed-probe count
  the guest reports on stderr, surfaced in the PASS line; 39 probes).
  GOTCHA revived by the split: the probes are basic-asm bodies, so
  -fipa-ra concludes they clobber NO caller-saved register and lets the
  caller keep values in rax/xmm across the probe call -- observed at -O2
  in the split-file layout as an expected-array store scheduled after the
  call being filled with the kmovd immediate left in %eax, and a
  (double)cvt constant hoisted into an xmm the probe then trashed (five
  checks the old single-file -O3 build had dodged by layout luck).  Every
  probe therefore ends with an asm-volatile clobber barrier listing the
  caller-saved GPRs and xmm0-15 (see the barrier comment in the guest) --
  deterministic across gcc versions and -O levels, verified O0/O2/O3.

- **Zydis 4.1 never sets `ZYDIS_ATTRIB_HAS_EVEX_B` while DECODING** -- the
  attribute is set exclusively by the encoder (see the "TODO: rename" note
  at the attribute's definition in `zydis/Zydis.h`), so an attribute test
  is always false and EVERY `{1toN}` memory operand used to be read as a
  full-width sequential vector (`vshufpd ...{1to8}` pulled all 8 qwords,
  `vpshufd ...{1to16}` all 16 dwords, etc.). Embedded-broadcast detection
  must key off `insn.avx.broadcast.mode != ZYDIS_BROADCAST_MODE_INVALID`,
  which the decoder fills from raw EVEX.B plus the per-tuple-type broadcast
  tables; `evex_broadcast_elem()` derives the broadcast ELEMENT size from
  the DECODED MEMORY OPERAND SIZE, which Zydis reports as exactly ONE
  element in bits for a broadcasting operand. Verified empirically against
  the vendored 4.1 by decoding the B=1 encodings of every broadcastable
  family (mem op.size / count -> element bytes): vshufpd m64{1to8} zmm
  64/8->8, vpshufd m512{1to16} zmm 32/16->4, vaddpd m64{1to2} xmm 64/2->8,
  vaddps m128{1to8} ymm 32/8->4, vpermilpd m64{1to8} zmm 64/8->8,
  vpermilps m512{1to16} zmm 32/16->4, vpternlogd m512{1to16} zmm 32/16->4
  and vpternlogq m512{1to8} zmm 64/8->8 (both REAL SDM forms -- see the
  ternlog bullet below), AND the half-width HV conversion
  sources whose broadcast element is the SOURCE element: vcvtdq2pd
  m256{1to8} zmm 32/8->4, vcvtps2pd m256{1to8} zmm 32/8->4,
  vcvtdq2pd m128{1to4} ymm 32/4->4, vcvtdq2pd m64{1to2} xmm 32/2->4,
  vcvtpd2ps m512{1to8} ymm 64/8->8. (An earlier revision derived the
  element as DEST vector width / mode count -- `insn.avx.vector_length` is
  the DEST length -- which works for the FV forms ONLY because their count
  x source element happens to span the whole vector; for the widening HV
  converts it fabricated the DEST element 8 where the broadcast element is
  the SOURCE element 4, tripping the "impossible EVEX broadcast element
  size" tripwire on legal code, and for vcvtps2pd it mis-replicated 8-byte
  chunks while the handler converted 16 lanes off the end of the 64-byte
  VecVal.) The broadcast mode's COUNT is used only as a sanity check on the
  pair (count * element must fit the vector). Zydis also FABRICATES
  broadcast modes for B=1 on instructions with no
  broadcast form (vpermd/vpsrld decode as 1_TO_16) -- illegal
  encodings that gas still assembles, so those silently broadcast instead
  of #UD; the SDM battery does not pin them. NOTE: vpternlogd does NOT
  belong in that fabricated list (an earlier revision of this bullet
  wrongly listed it): the SDM gives VPTERNLOGD a real m32bcst form and
  Zydis' 1_TO_16 for it is the real `{1to16}` (gas assembles
  `62 f3 7d 58 25 07 80`; the emulator executes it correctly through the
  generic broadcast path -- belem 4 matches exec_evex_ternlog's elem 4).
  The same is true of VPTERNLOGQ's m64bcst `{1to8}`.

- **EVEX perm-family element granularity**: `exec_evex_perm`'s `elem`
  governs the write-mask/{z}/broadcast granularity AND the masked memory
  read: VSHUFPD and VPERMILPD permute QWORDS (elem=8), VSHUFPS and
  VPERMILPS dwords (elem=4), and the elem must match the mnemonic -- the
  old code used dword granularity for both PD forms, so a `{k1}`-masked
  VSHUFPD merged dwords where hardware keeps whole qwords (k1=0x0D wrote
  qwords 0/2/3 hardware-side, but dwords 0/2/3 emulator-side: q0 came out
  as a mix of two dwords, q1 was wrongly overwritten and q2/q3 wrongly
  kept). The kind-4/5 tail (VPERMQ/VPERMPD imm vs VPSHUFD) had the same
  trap in reverse: it shared ONE elem (8) for both kinds, so a
  `{k1}`-masked VPSHUFD merged/zeroed whole QWORDS where the SDM is
  per-DWORD (k1 bit j controls dest dword j, what
  `_mm512_mask_shuffle_epi32`/`_mm512_maskz_shuffle_epi32` emit); the elem
  is now per-kind (8 for VPERMQ/VPERMPD, 4 for VPSHUFD).

- **EVEX ternlog element granularity**: `exec_evex_ternlog`'s `elem` is
  per-mnemonic: VPTERNLOGD dwords (elem 4), VPTERNLOGQ qwords (elem 8).
  It governs the write-mask/{z} lanes AND the `{1toN}` broadcast element
  of the memory source, and BOTH broadcast forms are real (the SDM lists
  m32bcst for VPTERNLOGD and m64bcst for VPTERNLOGQ; binutils assembles
  `62 f3 7d 58 25 07 80` and `62 f3 fd 58 25 07 80`). The old hardcoded
  elem=4 for both mnemonics tripped the "impossible EVEX broadcast element
  size" tripwire on the legal unmasked `vpternlogq ..., m512{1to8}, ...`
  form (belem 8 vs elem 4; exit 134 on valid code) and made a
  `{k1}`-masked VPTERNLOGQ merge/{z} DWORD-granular where the SDM writes
  whole qwords (k1=0x0002 with imm 0xFF: hardware sets qword 1 to all
  ones and keeps q0/q2..q7; the emulator wrote only dword 1 of q0 and
  kept q1).

- **VPERMIL imm form: VPERMILPD consumes its imm bits GLOBALLY, one per
  dest qword; VPERMILPS restarts its 2-bit pairs per 128-bit lane.**
  Hardware (i9-14900HX, differential sweep over all 16 immediates at
  256-bit): `vpermilpd $3` with src=[a0,a1,a2,a3] gives [a1,a1,a2,a2] and
  `$0xC` gives [a0,a0,a3,a3] -- dest.q[j] selects within its 128-bit
  lane's pair by imm bit j with j the GLOBAL dest qword number, so the
  upper lane uses bits 2/3 (12 of 16 immediates diverged when the
  emulator restarted at bit 0: $3 gave [a1,a1,a3,a3], $0xC gave
  [a0,a0,a2,a2]). VPERMILPS cannot go global (imm8 only holds four 2-bit
  pairs), so it restarts per lane like SHUFPS/VPSHUFD. The EVEX.512 imm
  forms follow the same split (exec_evex_perm kind 3; the pre-fix emulator
  died with "unsupported EVEX source operand" on those legal
  _mm512_permute_pd/ps forms because it loaded the imm8 as a vector).

- **VPERMIL variable-form controls read BIT 1 of each control element**
  (the imm form uses consecutive bits -- verified: `vpermilpd $3` with
  src=[a0,a1,a2,a3] gives [a1,a1,a2,a2]): VPERMILPS takes bits [1:0] of
  each control DWORD, but VPERMILPD takes BIT 1 of each control QWORD,
  per DEST qword, selecting within that 128-bit lane's qword pair
  (hardware-verified on VEX.256 ymm: bit 1 of control q0 flips ONLY
  dest.q0 to a1 while dest.q1 stays a0; bits 0 and 2 do nothing). The old
  code used the imm-form bit positions for the variable form, so PD
  controls read bit 0. Zero controls duplicate each pair's low qword
  (dest = [q0, q0, q2, q2] at 256-bit) -- the variable form is NOT the
  identity permutation.

- **EVEX convert source-vs-dest element granularity**: for the EVEX
  converts the vector length is the DEST width for the widening kinds
  (VCVTPS2PD/VCVTDQ2PD: 8-byte dest doubles from 4-byte source dwords) and
  the SOURCE width for the narrowing kind (VCVTPD2PS/VCVTPD2DQ/VCVTTPD2DQ:
  4-byte dest dwords from 8-byte source doubles), so `exec_evex_cvt` loads
  the source at sw = w/2 (widening) / w (narrowing) with the SOURCE element
  (4, or 8 for the narrowing double source) -- the masked source load's
  mask and a `{1toN}` broadcast's element both follow the SOURCE
  granularity, NOT the dest element:
  `vcvtps2pd zmm{k1}{z}, m256` with k1=0xF0 loads source dwords 4-7 and
  converts them into dest doubles 4-7 (the old code masked 4 8-byte lanes
  and loaded NOTHING -- all zeros), and `vcvtps2pd zmm, m256{1to8}`
  replicates one 4-byte float over the 32-byte source (the old code read 8
  bytes as the element and converted 16 lanes, writing 128 bytes into the
  64-byte VecVal -- a real stack OOB, ASan-confirmed). The narrowing kind
  masks over the DEST dwords with a full-width source read, so its load
  granularity is unchanged. All widening/narrowing probes live in
  tests/shufpd_sdm.c's is_in_zegarmistrz battery.

- **Narrowing packed converts have an xmm DEST regardless of the vector
  width**: `vec_width()` is the DEST operand's size, so for
  CVTPD2DQ/VCVTPD2DQ (and any other F2/66 0F E6-style narrowing convert)
  the LANE COUNT must come from the SOURCE operand's size -- the VEX.256
  form's dest is still xmm while its source is ymm (djb's supercop
  newhope512cca avx2 kernel: the first vcvtpd2dq handler converted only the
  low two lanes of every ymm and the whole NTT came out wrong, silently,
  with no fault). The handler `exec_cvtpd2dq` derives lanes from
  `ops[n-1].size`; dest bytes above the converted lanes are zero
  (hardware-verified: legacy zeroes the upper 64 bits, VEX applies the usual
  zeroing above the dest, VEX.256 zeroes the upper 128). The rounding form
  honors MXCSR.RC; NaN/inf/out-of-range give 0x80000000; the 66-prefixed
  CVTTPD2DQ truncates regardless of RC. Regression guest:
  `tests/cvtpd2dq.c` (the `cvtpd2dq` case).

- **The 0F 12/16 half-move family has reg-reg and m64 spellings under
  DIFFERENT mnemonics**: 0F 12 with a register third operand is
  (V)MOVHLPS, with memory (V)MOVLPS/(V)MOVLPD; 0F 16 reg-reg is
  (V)MOVLHPS, mem (V)MOVHPS/(V)MOVHPD. The VEX forms are ALWAYS
  3-operand -- there is no 2-operand VEX m64 load (the encoding always
  carries a vvvv source; GAS only accepts `vmovlps m64, src1, dst` and
  with src1 == dst, the spelling gcc emits for load-into-existing-
  register, the result keeps the old dst's other half and zeroes the
  upper YMM). The VEX 3-operand forms assemble the xmm dest from one
  half of each source ([x3.high, x2.high] for MOVHLPS, [x2.low, x3.low]
  for MOVLHPS, [m64, x2.high] for the l-load, [x2.low, m64] for the
  h-load -- hardware-verified) and zero the dest above 128 bits, while
  the legacy load forms keep the dest's other half AND the upper YMM.
  The m64 STORE forms (0F 13/66 0F 13 store the low half, 0F 17/66 0F 17
  the high half; VEX twins pick the same lanes) are handled by the same
  `exec_movlp` path. clang emits VMOVLHPS into the KeccakP1600times4
  lane interleave of djb's supercop newhope512cca avx2 kernel (56 sites in
  our clang -O3 build, so far only on untaken branches -- clang-19 could
  take them). Regression guests: `tests/movlhps.c` (the `movlhps` case)
  and the legacy/store forms in `tests/insn_forms.c` (the `insn-forms`
  case).

- **VPINSR* applies the VEX zeroing rule to the upper YMM**: the
  `exec_pinsr` VEX path must zero the dest's bytes above 128 -- the old
  code left stale lanes there (hardware-verified via the ymm0-scratch
  check in tests/insn_forms.c; the legacy PINSR* merge forms keep both
  the untouched lanes and the upper YMM, exactly like the reg-reg
  half-moves above). The PEXTR* GPR forms select from the LOW half only:
  legacy PEXTRW (0F C5) and the SSE4.1/VEX forms all take imm8 % 8 for
  words (% 16 bytes, % 4 dwords, % 2 qwords), and pextrq's qword[1] of
  {d0=0xCAFEBABE, d1=0x12345678, d2=3, d3=4} is 0x0000000400000003 --
  the qword index spans dword pairs.

- **EVEX write masks are explicit operands**, even `{k0}` when
  unmasked (`vpaddd` decodes as `[dst, k, src1, src2]`);
  `evex_explicit()` skips non-destination operands equal to
  `avx.mask.reg`; the authoritative mask lives in `avx.mask`
  (mode + reg). `vpcmpeqb` with a `k` destination is the
  mask-producing form (like `VPCMPB`+EQ), not the vector form —
  dispatch on destination kind.
- **Operand order is Intel order, but double-check per opcode**:
  `VPERMB/W` (and `VPERMD/Q`) compute `SRC2[SRC1]` (index in the
  *first* source; verified on HW against `_mm512_permutexvar_*`).
  EVEX `VPERMQ`/`VPERMPD` share one opcode between TWO encodings with
  SWAPPED operand roles: the imm8-control form has the data in ex[1]
  (ModRM.rm) and the imm8 in ex[2], while the vector-control form has the
  INDEX vector in ex[1] (EVEX.vvvv, register-only) and the DATA in ex[2]
  (zmm / m512 / {1to8} m64bcst) -- dispatch on `ex[2]` being an
  immediate. Both vector-form roles match VPERMD (index in vvvv, data in
  rm); the SDM pseudo-code (`TMP_DEST[63:0] := (TMP_SRC2[511:0] >>
  (SRC1[2:0] * 64))[63:0]`, SRC1 = vvvv, SRC2 = rm) gives GLOBAL mod-8
  indexing across the whole 512-bit source -- do NOT implement per-half
  ("per-lane") indexing for the vector form (that select-restart rule
  belongs to the imm8 form, whose 2-bit selects apply to BOTH 256-bit
  halves). There is NO memory-index form: vvvv is register-only, so a
  memory third operand is always the DATA (the pre-fix emulator
  kload()ed it as the imm8 and silently permuted with data[0] as the
  selects; the register-index form died with exit 132). The shufpd
  battery pins all of this, with lane-1 index elements >= 4 so the
  global and per-half rules give different vectors.
  `SHA256RNDS2` hides its `b` source (`HIDDEN` visibility; `k` is the
  explicit mem operand). `GF2P8AFFINE*` needs output bit-reversal plus
  per-64-bit-lane matrices (verified on HW), and legacy/VEX forms
  differ in which operand supplies bytes vs matrix. `INSERTPS`:
  `COUNT_S` is imm bits `[7:6]`, `COUNT_D` bits `[5:4]`. `VPTESTM/NM`
  are exact inverses (`M`: bits where `(a AND b) != 0`, `NM`: where
  `== 0`; the PTEST-CF analogy misleads).
- `vec_sources`/`vec_store_dst` assumed VEX means exactly 3 explicit
  operands; 4-operand VEX (`VSHUFPS`, `VBLENDPS`, `VCMPSS`, ...) must
  take the `n >= 3` path or the dst silently acts as a source.
- BMI `exec_bmi_simple` operand positions: `BZHI` uses the *index* as
  the index (not the value); `SHLX`/`RORX`/`PDEP` src/count/mask
  assignments are easy to swap. `ZydisMnemonicGetString` is lowercase
  (`vfmadd132ps`) and the FMA prefix parse starts at `'f'`.
- `_mm_aeskeygenassist_si128` needs a compile-time immediate: use a
  generated 256-case dispatch. `_mm_clmulepi64_si128` (not
  `_mm_clmull_epi32_si128`) on this GCC. PCLMUL intrinsics don't
  inherit function `target()` into lambdas — use a static helper.
- `ZydisMnemonicGetString` names for objdump's `vpcmpneqb` etc. are
  plain `VPCMPB` + imm — handle predicates by immediate, not by name.
- **RFLAGS**: flags via `__int128` arithmetic + parity fold;
  `INC`/`DEC` preserve `CF`; shifts set CF/OF/ZF/SF/PF per Intel (AF
  left alone where undefined); string ops honor `DF`; `ADC`/`SBB`
  implement carry/borrow chains exactly.
- **Operand visibility**: Zydis reports some real operands as
  `IMPLICIT` (e.g. `AL` in `test al,1`, `CL` in `shl eax,cl`) and
  others as `HIDDEN` (`RFLAGS`, implicit `RAX`/`RDX`, x87 stack
  operands). Handlers index the non-hidden operands in decoded order;
  `test`/`mul`/`div`/1-op-`imul` all work. Hidden operands matter for
  taint (see the x87 note in the var-time section).
- **LOCK**: a global bus mutex (`g_bus_lock`) serializes `LOCK`-prefixed
  RMWs plus implicitly-locked `XCHG`/`CMPXCHG8B`/`CMPXCHG16B` across
  guest threads. `RDRAND`/`RDSEED`/`CRC32` pass through to the host;
  `XSAVE`/`XRSTOR`/`FXSAVE` save/restore x87+SSE+AVX state (other
  components zeroed/skipped); shadow-stack reads report 0; `RDPID`
  returns 0.

- **The SSE4.2 string-compare family is emulated per the hardware, not per
  a prose summary** (PCMPxSTRx/PCMPxSTRM + the VEX twins; the semantics were
  validated bit-for-bit against the x86sem corpus's 4x128-record imm8 sweeps
  plus nonzero-length probes, and match qemu's `pcmpxstrx` helper):
  imm[1:0]=ub/uw/sb/sw, imm[3:2]=equal-any/ranges/equal-each/equal-ordered,
  imm[5:4]=polarity, imm[6]=the ITRI index direction (LSB/MSB) AND the STRM
  mask format (0 = IntRes2 bit-packed in the low bytes, 1 = expanded to
  bytes/words). SRC = the second operand (the scanned string, its explicit
  length in EDX), DEST = the first operand (the charset/ranges/needle, in
  EAX). Equal-each marks the positions past BOTH lengths as matches and the
  mixed-validity positions as non-matches; equal-ordered with an empty DEST
  marks every position, and a full needle must fit inside the valid SRC.
  Polarity -1 flips all bits, -3 flips only the bits under the SRC length;
  +2 (masked) leaves IntRes1 untouched. ZF = SRC length <= register width,
  SF = DEST length <= register width, CF = IntRes2 != 0, OF = IntRes2[0];
  AF/PF are left untouched (undefined on hardware). The explicit lengths
  clamp as `|int32|` into [0, 16/8]; the implicit lengths are the index of
  the first null element. The STRM result lives in XMM0, so the corpus dirs
  CHAIN (each instruction's SRC is the previous mask).

- **FMA NaN semantics**: hardware FMA preserves a NaN operand's sign and
  payload EXCEPT that an SNaN is quieted (bit 51/22 forced); the family
  negation (-) never flips a NaN sign, and a NaN on the product side wins
  over a NaN addend. Emulated by forwarding the first NaN multiplicand
  (quieted), then a NaN addend (quieted), and only otherwise calling the
  host's fused `__builtin_fma` with the negations applied to the finite
  operands. VFMADDSUB = even lanes subtract, odd lanes add; VFMSUBADD is the
  mirror; the fused single rounding comes from the host FMA hardware
  (`__attribute__((target("fma")))`).

- **ADD/SUB/MUL/DIV NaN delivery is resolved EXPLICITLY, never delegated
  to the host instruction**: when an FP add/sub/mul/div operand is NaN,
  the hardware delivers the FIRST source operand's NaN with sign+payload
  preserved and the quiet bit forced (SNaN quieted); otherwise the second
  operand's NaN quieted; first-NaN wins over second-NaN (hardware-probed
  on the i9-14900HX: addsd snan,qnan -> quiet(snan);
  addsd qnan,snan -> qnan verbatim; vhaddpd pair [qnan,snan] -> qnan,
  i.e. the LOWER/first element).  The emulator's C code says `x0 + x1`
  with the semantic first operand first, but gcc is free to place EITHER
  C operand in the Intel-first lane of the emitted instruction --
  observed (this repo's -O2 gcc-11 build) as exec_hadd_fp's horizontal
  pair `x0 + x1` compiled into ONE fused `addpd` whose Intel-first lane
  held x1, so the host FPU delivered x1's NaN while real hardware
  delivers x0's (the x86sem vadd corpus diverged at record 993,
  `vhaddpd %xmm4,%xmm9,%xmm0`, slot ymm0).  Delegating a NaN-carrying
  lane therefore lets the compiler BUILD decide which NaN propagates --
  a gcc-version-dependent wrong-answer landmine.  exec_packed_fp /
  exec_sse_scalar / exec_hadd_fp (add/sub/mul/div kinds only; sqrt/max/
  min keep their own delegated rules) now check the semantic operands'
  bits via is_nan_bits64/32 + quiet_nan_bits64/32 and only hand a
  non-NaN pair to the host arithmetic (finite/denormal/inf and the
  invalid-indefinite results are the same bits on any x86 host).  The
  FMA bullet above is the same rule for the fused forms.

- **RCPSS/RSQRTSS (and the packed forms) are hardware approximations, not
  IEEE ops** (qemu's exact-division emulation does NOT match hardware). On
  this host the result depends only on the exponent and the top 12 mantissa
  bits (the low 12 are ignored), reproduced bit-exactly by the generated
  `zeg_rcp_table`/`zeg_rsqrt_table_{126,127}` lookup tables plus the
  exponent rules (RCP: `253 - e`, underflowing to zero; RSQRT:
  `190 - ceil(e/2)`, odd/even exponent selects the table half) and the
  special cases (0/denormal -> inf, inf -> 0, -inf/negative finite ->
  the QNaN indefinite for RSQRT, NaN -> quieted). Calibrated by sweeping the
  hardware with the x86sem native oracle.

- **CVTSD2SI/CVTSS2SI (non-truncating)** round per MXCSR.RC -- implemented
  as round-to-nearest-even (`__builtin_roundeven`) followed by the
  out-of-range -> integer-indefinite check, sharing `exec_cvts2si` with the
  CVTT*S2SI truncating forms.

- **VCVTPS2PH honors its imm**: imm[2]=use MXCSR.RC, imm[1:0]=RNE/RD/RU/RZ,
  mapped onto `_mm_cvtps_ph`'s immediate (a compile-time dispatch since the
  intrinsic rejects a runtime immediate).

- **The x86sem native oracle must present a clean below-RSP region**: the
  TF-stepping `gogo` stub runs `pushq $0x302; popfq; ret` on the guest stack
  and leaves residue at [rsp-16, rsp) (the push's 0x302 and the popped entry
  word), which the corpus's `leaq -8(%rsp); scasw` then reads -- a
  ptrace-stepped execve (the upstream oracle) sees a clean zero stack there.
  The entry-trap handler zeroes [rsp-512, rsp) once before the first guest
  instruction; later deliveries never touch the guest stack (frames go to
  the sigaltstack).

## Unsupported instructions (clean guest errors)

These fail as `unimplemented <mnemonic>` guest errors (in
`--var-time-op=everything` mode a tainted operand reports var-time
first): AMX tiles, CET shadow stack (`RSTORSSP`/`SAVEPREVSSP` would
`#UD` natively without CET), Intel TSX (no host RTM), AVX512-FP16
(`...PH`/`...SH`), AVX512 VNNI follow-ons (`VPDP*`, `VPMULTISHIFTQB`,
`VPERMI2/T2`, `VRANGE*`, `VSCALEF*`, `VFIXUPIMM*`), SHA512, `XSAVE`
supervisor states beyond AVX, `fork`, guest signal delivery. The exit
status for all of these is **132 (SIGILL)** — the unimplemented-instruction
class, distinct from the poison violations' 139 — and the poison-only
`--error-exitcode`/`error exitcode` override never applies to them (see
the exit-code classification section). `tests/unimplemented.c` (the
`unimplemented-exit-code` case) pins this with VGETMANTSS, a real EVEX
instruction none of the handlers cover. Run `./check-isa.sh` after
adding guests to catch new gaps early.
