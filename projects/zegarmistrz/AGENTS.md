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
  `--no-stack-trace`, `--no-core`/`--passthrough-signals`,
  `--var-time-op=`, `--history[=VALUE]`), Linux-ABI initial stack
  (argc/argv/envp/auxv, 16-byte aligned), main-thread CPU, interpreter
  loop. `SIGUSR1` dumps a guest backtrace (debug aid).
- `src/elf_loader.{h,cpp}` — ELF64 loader: maps each object's `PT_LOAD`s,
  loads `PT_INTERP` first, exposes entry/`AT_PHDR`/`AT_ENTRY`/`AT_BASE`.
- `src/mem.{h,cpp}` — the one memory pipeline + the per-byte poison map +
  host-fault recovery (`sigsetjmp`-based). The taint sidecar types also
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
  self-checking guests are gated on runtime CPUID so they run everywhere).
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
- **Native-vs-emulator mirror guests** (avx512/vex/aes smoke,
  insn-fixes, insn-fixes-avx512, poly1305-kat, dec-cache,
  malloc-churn, cpuid/zeglib `-native`) must produce byte-identical
  output natively and under the emulator. `poly1305-kat` additionally
  needs OpenSSL dev headers + libcrypto and is SKIPPED (not failed) when
  that compile fails. `filc-hello` runs the repo-root Fil-C `./hello`
  (skipped if it is not an ELF, e.g. a Cosmopolitan APE image).
- `core-gdb` runs `gdb -batch -ex bt` on a produced core and asserts the
  backtrace shows the guest's `boom` → `main` and **no** interpreter
  frames. `stack-trace*` check the banner/trace/core ordering;
  `stack-trace-shlib` additionally asserts a fault inside a shared
  library the guest ld.so mapped symbolicates with the .so's function
  name + source file, and `stack-trace-inline` asserts the DWARF inline
  chain expands (innermost inlined function first, marked `(inline)`);
  `no-core` checks `--no-core`.
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
   directory, then the `zegarmistrz: [core: <path>]` line. With
   `--no-core`/`--passthrough-signals`: `[core dumps disabled]`, no
   file.

The banner and trace print **before** the core is written, so the trace
is visible even if the core write is interrupted. The process exits
with SIGSEGV (shell status 139) for poisoning faults.

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

## Unsupported instructions (clean guest errors)

These fail as `unimplemented <mnemonic>` guest errors (in
`--var-time-op=everything` mode a tainted operand reports var-time
first): AMX tiles, CET shadow stack (`RSTORSSP`/`SAVEPREVSSP` would
`#UD` natively without CET), Intel TSX (no host RTM), AVX512-FP16
(`...PH`/`...SH`), AVX512 VNNI follow-ons (`VPDP*`, `VPMULTISHIFTQB`,
`VPERMI2/T2`, `VRANGE*`, `VSCALEF*`, `VFIXUPIMM*`), SHA512, `XSAVE`
supervisor states beyond AVX, `fork`, guest signal delivery. Run
`./check-isa.sh` after adding guests to catch new gaps early.
