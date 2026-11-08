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

# zegarmistrz — an x86-64 user-mode emulator that catches secret-dependent timing

`zegarmistrz` ("clockmaker" in Polish) loads a Linux x86-64 ELF binary and
interprets it instruction by instruction, giving every guest memory access
a common pipeline. On top of that it enforces a valgrind-style
poison/taint system aimed at testing constant-time cryptosystems: the
guest marks memory as secret with per-byte "cannot" bits — cannot-load,
cannot-store, cannot-branch, cannot-index, cannot-var-time-op — and the
emulator faults the moment a tainted value feeds an operation whose
control flow, addressing, or timing could depend on it. A fault prints a
symbolized guest stack trace to stderr and writes a gdb-consumable ELF
core dump (exit status 139, i.e. SIGSEGV). The target use: run crypto
code with its key material poisoned, and any branch on key bytes, table
lookup indexed by key bytes, division on secret-derived values, or
secret-indexed gather becomes an immediate, located failure instead of a
possible side channel.

## Build & test

Dependencies: `libzydis-dev` + `libzycore-dev` (Zydis decoder) and
`binutils-dev` (BFD, for crash-time guest stack traces).

```bash
sudo apt-get install -y libzydis-dev libzycore-dev binutils-dev
make -j$(nproc)          # builds ./zegarmistrz
./run-tests              # builds the guests, runs the full suite in parallel
./check-isa.sh           # report mnemonics in real binaries lacking handlers

./zegarmistrz <program> [args...]        # run a guest
./zegarmistrz -v ./hello                 # disassemble each instruction
./zegarmistrz --trace-syscalls ./hello   # trace syscalls
./zegarmistrz --help                     # full usage + poisoning ABI + modes
```

`./run-tests` compiles the guests in `tests/` into `test-output/` and
runs every case in parallel (`xargs -P`; `JOBS=8 ./run-tests` to tune):
basic guests, native-vs-emulated SIMD differentials, the poisoning
battery (faults, laundering, clear-on-store, var-time mode gating, the
per-check-site cannot-index battery), core dumps, and stack traces.
`./build_zegarmistrz.sh` (repo root) rebuilds and installs the binary at
`filc/zegarmistrz`.

## Guest-side API: `include/zegarmistrz.h`

A header-only C client library: `zegarmistrz_poison_range(ptr, size,
flags)`, `zegarmistrz_unpoison_range(ptr, size)`, `zegarmistrz_query()`,
and `is_in_zegarmistrz()`. Everything is static inline and behaves as a
no-op outside the emulator, so the same binary runs natively and under
zegarmistrz. (Under Fil-C the calls become runtime natives; see the
header's comments.)

## Catching errors: three worked examples

The demos below live in `/tmp` (traces show their real paths) and were
compiled with `gcc -O1 -g -I projects/zegarmistrz/include`. Addresses and
tids vary run to run (ASLR).

### 1. Table lookup indexed by a secret byte (cannot-index)

```c
// Secret-indexed table lookup: the classic cache-timing leak.
#include <zegarmistrz.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    static const uint8_t sbox[256] = {0x63, 0x7c, 0x77, 0x7b, 0xf2}; /* AES-ish */
    uint8_t key[16], out[16];
    memset(key, 0x42, sizeof(key));
    zegarmistrz_poison_range(key, sizeof(key), ZEGARMISTRZ_POISON_CANNOT_INDEX);
    for (int i = 0; i < 16; i++)
        out[i] = sbox[key[i]];                 // data-dependent cache line
    printf("mac=%02x%02x\n", out[0], out[1]);  // NOTREACHED under zegarmistrz
    return 0;
}
```

```console
$ ./demo_index                     # natively: nothing special happens
mac=0000
$ zegarmistrz ./demo_index; echo "exit=$?"
zegarmistrz: fatal guest error (tid=4105146): tainted (cannot-index) value used as address index at rip=0x708f3f8d91f4
Guest stack trace (tid=4105146):
  #0  0x0000708f3f8d91f4 in main () at /tmp/zegdemo/demo_index.c:13
  #1  0x0000708f7e97e28b
  #2  0x0000708f3f8d90a5 in _start ()
zegarmistrz: [core: zegarmistrz.core.demo_index.4105146]
exit=139
```

### 2. Branching on a secret (cannot-branch)

`while (len < sizeof(key) && key[len] != 0) len++;` over `key` poisoned
with `ZEGARMISTRZ_POISON_CANNOT_BRANCH`:

```console
$ zegarmistrz ./demo_branch; echo "exit=$?"
zegarmistrz: fatal guest error (tid=4105122): tainted (cannot-branch) value used in branch condition at rip=0x7b17a4b121f1
Guest stack trace (tid=4105122):
  #0  0x00007b17a4b121f1 in main () at /tmp/zegdemo/demo_branch.c:15
  #1  0x00007b17e3b7e28b
  #2  0x00007b17a4b120a5 in _start ()
zegarmistrz: [core: zegarmistrz.core.demo_branch.4105122]
exit=139
```

### 3. Dividing on secret data (cannot-var-time-op)

`mac += secret[i] % prime;` with `secret` poisoned with
`ZEGARMISTRZ_POISON_CANNOT_VAR_TIME_OP` (64-bit DIV has operand-dependent
latency, a documented data-dependent instruction):

```console
$ zegarmistrz ./demo_div; echo "exit=$?"
zegarmistrz: fatal guest error (tid=4105123): tainted (cannot-var-time-op) value in variable-time operand of div (data-dependent timing) at rip=0x7ddf6bba521a fault_addr=0x7ddf6bba521a
Guest stack trace (tid=4105123):
  #0  0x00007ddf6bba521a in main () at /tmp/zegdemo/demo_div.c:17
  #1  0x00007ddfaab7e28b
  #2  0x00007ddf6bba50a5 in _start ()
zegarmistrz: [core: zegarmistrz.core.demo_div.4105123]
exit=139
```

## Poisoning semantics

Poison is a per-byte map. `zegarmistrz_poison_range(ptr, size, flags)`
ORs a bitmask into each byte of `[ptr, ptr+size)`: bit0 cannot-load,
bit1 cannot-store, bit2 cannot-branch, bit3 cannot-index, bit4
clear-branch-on-store, bit5 clear-index-on-store, bit6
cannot-var-time-op, bit7 clear-var-time-op-on-store (the
`ZEGARMISTRZ_POISON_*` constants in the header).

Loading a byte taints the loaded value with its cannot-branch /
cannot-index / cannot-var-time-op bits; computation propagates the OR of
input taints, and flag-setting ops taint RFLAGS. A store launders the
stored value's bits into the destination bytes; a store into a location
carrying the matching clear-on-store bit instead clears that bit (store
sanitizes). `xor reg,reg` (and self-sub) clears taint — the result is
zero regardless of the input. cannot-load/cannot-store/clear bits are
sticky.

### What cannot-index actually covers

cannot-index applies **only to instructions that actually touch memory**
through the tainted value: loads/stores and any other memory operand
(base or index), string ops via RSI/RDI, push/pop/ret/call/leave/enter
via RSP (and `enter`'s level >= 2 nesting copies read `[RBP - i*8]`),
XLAT (`RBX`+`AL`), BT bit offsets, prefetch/`clflush`, gather/scatter
VSIB index vectors (and their scalar base), and syscall arguments. The
dimensions are orthogonal: cannot-index never fires at control-flow
sites — branching, looping, jumping, or returning through an
address-tainted value is the cannot-branch dimension instead, and
syscall arguments are the one boundary where both bits are rejected. Two
deliberate exemptions keep ordinary compiler arithmetic from
false-positive:

- **LEA never trips it.** LEA is pure address arithmetic (compilers emit
  it for `x = y + 42`), so it computes its result without the cannot-index
  checks — but the taint still propagates into the destination, so a later
  real access through the result still faults. Like real hardware, LEA
  also ignores segment overrides: it computes only the offset
  (base + index*scale + disp); the fs/gs base is added by `resolve_mem`
  for instructions that actually touch memory.
- **Multi-byte NOPs never trip it.** The long NOP encodings (`0f 1f /0`
  and friends) use an address encoding purely as padding; the decorative
  operand is ignored.

cannot-var-time-op is the fourth dimension: it faults when a tainted
value reaches an instruction whose execution time depends on operand
values (DIV/IDIV, gathers/masked ops, FP arithmetic, CPUID leaves, x87
math, ...), other than through branching or addressing, which keep their
own bits and win when both would apply. Three modes via `--var-time-op=`:
`known` (default; only documented data-dependent instructions),
`plausible` (also undocumented suspects like RCR counts), and
`everything` (anything not provably constant-time). SYSCALL timing is the
kernel's and is never checked; syscall *arguments* reject cannot-branch
and cannot-index values.

## Design in brief

- **Zydis-based interpreter.** Fetch/decode/execute with system Zydis;
  ~full user-mode ISA through AVX512 + AES/SHA/GFNI (`./check-isa.sh`
  verifies coverage); unimplemented instructions fail cleanly.
- **One memory pipeline.** Guest VA == host VA, so the fast path is
  `memcpy`, but every load/store/code-fetch/string copy flows through
  `src/mem.cpp` — where poison checks run and host SIGSEGV/SIGBUS become
  guest core dumps.
- **Taint sidecars.** Every GPR/XMM value carries the three taint bits
  (x87 and opmask registers carry whole-register sidecars) that propagate
  like OR through ALU and vector ops; RFLAGS has its own sidecar for
  branch conditions.
- **ELF loader + virtualized syscalls.** Maps `PT_LOAD`s, loads
  `PT_INTERP`, builds the Linux initial stack, reserves `brk` growth
  room; `brk`/`arch_prctl`/`clone` (as pthreads)/`exit` vs `exit_group`
  are emulated, the rest pass through to the host; tainted syscall
  arguments are rejected.
- **Fatal errors are first-class.** Symbolized guest stack traces (BFD
  against the loaded objects; RBP-chain walk with an approximate-scan
  fallback for `-fomit-frame-pointer` code) print before an `ET_CORE`
  dump that gdb/lldb read as pure guest state.
- **CPUID presence.** The hypervisor bit (CPUID.1:ECX bit 31) plus
  hypervisor leaf `0x40000000` with vendor string `"Zegarmistrz"`;
  leaves `0x40000001`+ read zero.

## Implementation details

For the agent/contributor-facing companion — build/test mechanics, the
cannot-index enforcement map, the var-time classification scheme, the raw
poisoning ABI, and the gotchas lists — read [AGENTS.md](AGENTS.md).

