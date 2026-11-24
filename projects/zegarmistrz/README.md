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

Dependencies: `binutils-dev` (BFD, for crash-time guest stack traces). The
Zydis decoder is vendored — the amalgamated distribution lives in `zydis/`
and is built by `make`, so no system Zydis/Zycore package is needed.

```bash
sudo apt-get install -y binutils-dev
make -j$(nproc)          # builds ./zegarmistrz
./run-tests              # builds the guests, runs the full suite in parallel
./check-isa.sh           # report mnemonics in real binaries lacking handlers

./zegarmistrz <program> [args...]        # run a guest
./zegarmistrz -v ./hello                 # disassemble each instruction
./zegarmistrz --trace-syscalls ./hello   # trace syscalls
./zegarmistrz --history ./crypto-guest   # record + dump poison provenance
./zegarmistrz --help                     # full usage + poisoning ABI + modes
```

`./run-tests` compiles the guests in `tests/` into `test-output/` and
runs every case in parallel (`xargs -P`; `JOBS=8 ./run-tests` to tune):
basic guests, native-vs-emulated SIMD differentials, the poisoning
battery (faults, laundering, clear-on-store, var-time mode gating, the
per-check-site cannot-index battery), core dumps, stack traces, and the
`--history` poison-provenance checks (trail symbolication on a fault in
every flag spelling, the deep `=N`/`=full` stack-recording modes,
default-mode invariance without the flag, bad-value rejection, semantics
unchanged in history mode).
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

The demos below live in `/tmp/zegdemo` (traces show their real paths) and
were compiled there with `gcc -O1 -g -I projects/zegarmistrz/include`,
run under a `./zegarmistrz` symlink to the built binary. Addresses and
tids vary run to run (ASLR). Each trace is gdb-style: frame 0 is the
faulting RIP and every later frame is a return address, unwound with the
guest's DWARF CFI (`.eh_frame`) the way gdb unwinds the core — so even
these `-O1` demos walk past `main` into libc's startup frames, which
symbolicate because the guest's libc was mapped from a real file whose
path + load bias the emulator records. Return-address frames symbolicate
at address - 1 (gdb's "address in block"), so the reported line is the
line the call sits on.

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
$ ./zegarmistrz ./demo_index; echo "exit=$?"
zegarmistrz: fatal guest error (tid=42131): tainted (cannot-index) value used as address index at rip=0x79a7d7f07205
Guest stack trace (tid=42131):
  #0  0x000079a7d7f07205 in main () at /tmp/zegdemo/demo_index.c:13
  #1  0x000079a816f67d90 in __libc_start_call_main () at ./csu/../sysdeps/nptl/libc_start_call_main.h:58
  #2  0x000079a816f67e40 in __libc_start_main_impl () at ./csu/../csu/libc-start.c:392
  #3  0x000079a7d7f070a5 in _start ()
zegarmistrz: [core: zegarmistrz.core.demo_index.42131]
exit=139
```

### 2. Branching on a secret (cannot-branch)

`while (len < sizeof(key) && key[len] != 0) len++;` over `key` poisoned
with `ZEGARMISTRZ_POISON_CANNOT_BRANCH`:

```console
$ ./zegarmistrz ./demo_branch; echo "exit=$?"
zegarmistrz: fatal guest error (tid=42134): tainted (cannot-branch) value used in branch condition at rip=0x7cbe2bcd5208
Guest stack trace (tid=42134):
  #0  0x00007cbe2bcd5208 in main () at /tmp/zegdemo/demo_branch.c:15
  #1  0x00007cbe6ad67d90 in __libc_start_call_main () at ./csu/../sysdeps/nptl/libc_start_call_main.h:58
  #2  0x00007cbe6ad67e40 in __libc_start_main_impl () at ./csu/../csu/libc-start.c:392
  #3  0x00007cbe2bcd50a5 in _start ()
zegarmistrz: [core: zegarmistrz.core.demo_branch.42134]
exit=139
```

### 3. Dividing on secret data (cannot-var-time-op)

`mac += secret[i] % prime;` with `secret` poisoned with
`ZEGARMISTRZ_POISON_CANNOT_VAR_TIME_OP` (64-bit DIV has operand-dependent
latency, a documented data-dependent instruction):

```console
$ ./zegarmistrz ./demo_div; echo "exit=$?"
zegarmistrz: fatal guest error (tid=42158): tainted (cannot-var-time-op) value in variable-time operand of div (data-dependent timing) at rip=0x78cfe160f214 fault_addr=0x78cfe160f214
Guest stack trace (tid=42158):
  #0  0x000078cfe160f214 in main () at /tmp/zegdemo/demo_div.c:17
  #1  0x000078d020567d90 in __libc_start_call_main () at ./csu/../sysdeps/nptl/libc_start_call_main.h:58
  #2  0x000078d020567e40 in __libc_start_main_impl () at ./csu/../csu/libc-start.c:392
  #3  0x000078cfe160f0a5 in _start ()
zegarmistrz: [core: zegarmistrz.core.demo_div.42158]
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
input taints, and flag-setting ops taint RFLAGS. A store propagates the stored value's taint bits into the destination
bytes (marking newly tainted bytes clear-on-store); a store that is clean
in a dimension whose clear-on-store bit is set clears that bit (store
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

### Kernel-write modeling: syscalls clear stale poison

Guest VA == host VA, so a host syscall that fills a guest buffer writes
guest memory **directly, invisible to the poison map**. Stale poison on
those bytes would survive the kernel's overwrite and false-positive
later: the original report was a stdout-printf path whose `fstat(fd,
&st)` landed the 144-byte `struct stat` on stack slots that deeper,
poisoned crypto frames had spilled taint into, after which the
`S_ISCHR(st.st_mode)` branch inside `__GI__IO_file_doallocate` faulted
with "tainted (cannot-branch) value used in branch condition". Every
byte the kernel overwrites is replaced with fresh, non-guest-derived
data, so after each successful passthrough syscall the emulator clears
the poison over the kernel's write extents (`syscall_kernel_writes` in
`src/syscall.cpp`; the whole step is gated on the poison map being
non-empty, so poison-free runs pay nothing and can never hit the reject
below). A failed syscall (negative return) wrote nothing and clears
nothing — with the one exception of `nanosleep`/`clock_nanosleep`
interrupted by a signal, whose remaining-time buffer the kernel does
fill.

Extent modeling is per-syscall and conservative in both directions:

- **Modeled syscalls** list exactly what the kernel writes: `read`-family
  fills `ret` bytes (`readv`/`preadv*`/`process_vm_readv`/`recvmsg`
  distribute them across the iovec array with the per-iov lengths used
  as-is — the kernel wrote exactly `ret` bytes, so clamping an `iov_len`
  would under-clear large iovs), the stat family fills
  fixed-size structs (`fstat`/`stat`/`lstat`/`newfstatat` 144,
  `statx` 256, `statfs`/`fstatfs` 120), `uname` 390, `sysinfo` 112,
  `getrusage`/`wait4` rusage 144, clocks (`clock_gettime` 16,
  `gettimeofday`, `time`, `adjtimex` 208), `getrlimit`/`prlimit64` 16,
  `poll`/`ppoll` the `revents` short of each `pollfd`, `select` the
  128-byte fd_set writebacks, `epoll_wait` 12 bytes per returned event,
  `pipe`/`socketpair` the fd pair, `getrandom`/`getcwd`/`getdents*`/
  `readlink*`/`syslog` `ret` bytes, `mincore` one byte per page,
  `getresuid`/`getresgid`/`getcpu`/`sched_getparam` 4 bytes,
  `sched_getaffinity`/`getgroups` `ret` bytes, `sched_getattr`
  `min(size, 56)`, the socket length-writeback family
  (`accept`/`getsockname`/`getpeername`/`getsockopt`/`recvfrom`/
  `recvmsg`: the buffer plus the updated `socklen_t`, clamped to
  `sockaddr_storage` where it is an address; for `recvfrom` the data
  extent is `ret` bytes at the second argument and the source address
  lands at the fifth with the `socklen_t` word at the sixth — the flags
  value is never an extent), `msgrcv` (type long +
  payload), `fcntl` only for the lock-query commands,
  `futex` only `FUTEX_LOCK_PI`'s TID word, `madvise` only
  `MADV_DONTNEED` (dropped pages come back zeroed), and `prctl`:
  `PR_GET_NAME` 16 bytes plus the six 4-byte `PR_GET_*` pointer-writers
  (`PR_GET_PDEATHSIG`/`PR_GET_UNALIGN`/`PR_GET_FPEMU`/`PR_GET_TSC`/
  `PR_GET_ENDIAN`/`PR_GET_TAGGED_ADDR_CTRL`; on x86-64 several of those
  fail with `-EINVAL` and a failed syscall writes nothing, while the
  remaining `PR_GET_*` report through RAX and write nothing).
- **ioctl uses a heuristic** (the hardest case), in priority order: a
  null third argument writes nothing; classic non-`_IOC` requests use a
  fixed table (`TCGETS` 36 / `TCGETA` 18 kernel termios/termio sizes,
  `TIOCGWINSZ` 8, the int-out requests like `FIONREAD`/`TIOCGPGRP`
  4 bytes, and the setters write nothing); every other old-style
  request is unmodelable; `_IOC`-encoded requests decode the direction
  and size fields (`dir = (req >> 30) & 3`, `size = (req >> 16) &
  0x3fff` — `_IOC_READ` confusingly means the kernel writes INTO the
  user buffer) and clear `size` bytes; anything else is unmodelable.
- **Known no-writes** are enumerated explicitly (`openat`/`close`/
  `write`/`mprotect`/`munmap`/fchmod-family/socket setup/`rseq` — whose
  ABI pages the kernel writes lazily at preemption, not at syscall time
  — and so on); everything **not** classified is rejected: when poison
  is active and the syscall succeeded, the emulator crashes with
  `cannot model kernel-write extents of syscall <name> (<nr>): extend
  syscall_kernel_writes in src/syscall.cpp` rather than silently
  laundering unknown poison. A failed syscall never crashes (it wrote
  nothing), and neither does a poison-free run.
- **Emulator-side writes are kernel-equivalent** and clear poison at the
  write site: `mmap` (a fresh mapping — zeroed or kernel-loaded), `brk`
  growth (fresh anonymous pages), `arch_prctl` GET_FS/GET_GS
  writebacks, the clone parent-tid write, and the
  `CLONE_CHILD_CLEARTID` clears. `mremap` is the one syscall that
  MOVES poison: the data at the old range relocated to the new base
  carries its poison (bits and history trails) with it
  (`mem_move_poison`), while growth beyond the old size is fresh and
  cleared.

`tests/syscall_unpoison.c` pins all of this: it poisons a buffer with
cannot-branch poison, lets the kernel write into it (`fstat`,
`ioctl(FIONREAD)`, `read`, `getrandom`, `recvfrom`'s data bytes,
sockaddr and socklen word, `timerfd_settime`'s old-value itimerspec),
and branches on the kernel-written bytes — clean under plain runs and
`--history=full`. `tests/syscall_unpoison_ctl.c` pins the soundness
half: the `timerfd_settime` `new_value` input the kernel never writes
must STAY poisoned, so branching on it faults.

### Poison provenance: --history

`--history[=VALUE]` records, for every poisoned value, which guest
operations produced or consumed it (fresh loads from poisoned memory,
moves/merges into the value's sidecar, and stores of the value where the
sidecar carrying it is handed to the store path by mutable reference;
bounded, set-union deduped — a PC already present anywhere in the trail
is never re-added, so N identical provenances merge to the distinct set
of operations and the cap keeps the most recent executions) and, when a
poison fault kills the process, prints that trail BFD-symbolicated right
after the guest stack trace. Every trail step records the global
execution sequence of the instruction that recorded it, and the dump
sorts the steps by it before printing: the trail is stored in
merge order (set-union merges assemble it out of execution order), so
the sort is what makes the printed trail a true chronological execution
order — every step consumes values produced at earlier steps, and
consecutive steps may assign the same register when the program chains
back-to-back operations on it — the later step then consumes that
register's prior value (an rmw chain; the regression guest's own trail
contains four such pairs). What the chronological ordering eliminates is
the scramble symptom from the original complaint: a trail that placed an
independent overwrite of a register between its producer and the step
that actually consumed that value. Strict per-step chaining — step I+1
using exactly step I's value — is not guaranteed when provenance forks,
since each merged trail is the union of its lineages. VALUE picks how much
each recorded step carries: bare `--history`, `=yes`, or `=1` records one
guest PC per step; `=no`, `=0`, or no flag at all records nothing; `=N`
for an integer N > 1 records N stack frames per step; `=full` or `=all`
records the whole stack per step. Any other value is rejected with an
error before any guest runs. The recorded PCs are raw and
pre-symbolication — BFD symbolication and DWARF inline reconstruction
happen at print time (`demo_index` from above under bare `--history`):

```
zegarmistrz: poison history of the offending value (2 steps, oldest first):
step 1:  #0  0x000076803dcc11f0 in zegarmistrz_client_request () at /home/pizlo/Programs/llvm-project-deluge/projects/zegarmistrz/include/zegarmistrz.h:160  (inline)
         #1  0x000076803dcc11f0 in zegarmistrz_poison_range () at /home/pizlo/Programs/llvm-project-deluge/projects/zegarmistrz/include/zegarmistrz.h:181  (inline)
         #2  0x000076803dcc11f0 in main () at /tmp/zegdemo/demo_index.c:11
step 2:  #0  0x000076803dcc1201 in main () at /tmp/zegdemo/demo_index.c:13
```

Steps are labeled `step N:` (1-based, oldest first) and every displayed
line of a step is numbered `#0`, `#1`, ... gdb-style — a continuous
per-step counter over ALL displayed lines, restarting at `#0` under the
step label — exactly like the crash-time guest stack trace numbers its
displayed lines, except the trace's one counter runs over the whole trace
instead of restarting per step. Every line carries a PC and prints
`#K  0x<PC>` plus the location (`in <func> () at <file>:<line>`), and
every line after the step's first aligns its `#K` — and so its PC and
`in` columns — under the step line's `#0` (while the per-step numbers
stay at most two digits — below `#100` the `%-2zu` pad stops and a step
exceeding 99 displayed lines shifts the PC and `in` columns). A recorded
frame prints its
symbolized chain one numbered line per entry, all repeating the frame's
own recorded PC (as the stack trace's inline frames repeat the physical
address): the innermost symbol first, the physically-containing function
last and unmarked, every inlined instance marked `  (inline)`. In the
deep modes (`=N`/`=full`) each step's further recorded caller frames
print below that, as more numbered chains under the same `step N:`
header. Recording unwinds with
the emulator's DWARF CFI unwinder (`zeg_history_record_stack` in
`src/dwarf_unwind.cpp`), so `-fomit-frame-pointer` guests record correct
stacks; deep history runs the whole engine on a third taint sidecar type
(`DeepHistoryTaint`) and is correspondingly heavier. The same demo under
`--history=full`:

```
zegarmistrz: poison history of the offending value (2 steps, oldest first):
step 1:  #0  0x0000742f124181f0 in zegarmistrz_client_request () at /home/pizlo/Programs/llvm-project-deluge/projects/zegarmistrz/include/zegarmistrz.h:160  (inline)
         #1  0x0000742f124181f0 in zegarmistrz_poison_range () at /home/pizlo/Programs/llvm-project-deluge/projects/zegarmistrz/include/zegarmistrz.h:181  (inline)
         #2  0x0000742f124181f0 in main () at /tmp/zegdemo/demo_index.c:11
         #3  0x0000742f51367d90 in __libc_start_call_main () at ./csu/../sysdeps/nptl/libc_start_call_main.h:58
         #4  0x0000742f51367e40 in __libc_start_main_impl () at ./csu/../csu/libc-start.c:392
         #5  0x0000742f124180a5 in _start ()
step 2:  #0  0x0000742f12418201 in main () at /tmp/zegdemo/demo_index.c:13
         #1  0x0000742f51367d90 in __libc_start_call_main () at ./csu/../sysdeps/nptl/libc_start_call_main.h:58
         #2  0x0000742f51367e40 in __libc_start_main_impl () at ./csu/../csu/libc-start.c:392
         #3  0x0000742f124180a5 in _start ()
```

The trail answers "where did this tainted value come from": the steps
are the operations, oldest first, and the numbered lines show where
each recorded frame sits. Each step carries the global execution
sequence of the instruction that recorded it, and the dump sorts the
steps by it before printing — the trail is stored in merge order (the
set-union merges assemble it out of execution order), so the sort is
what makes the printed trail a true chronological execution order:
every step consumes values produced at earlier steps, and consecutive
steps may assign the same register when the program chains
back-to-back operations on it — the later step then consumes that
register's prior value (an rmw chain; the regression guest's own trail
contains four such pairs). What the chronological ordering eliminates is
the scramble symptom from the original complaint: a trail that placed an
independent overwrite of a register between its producer and the step
that actually consumed that value. (Strict
per-step chaining — step I+1 using exactly step I's value — is not
guaranteed when provenance forks, since each merged trail is the union
of its lineages: in `tests/history_order.c` the chronological
`and $0x2a` step E on b is followed by step F `and %ecx,%ebx`, which
consumes a/c, not E's b.) Store PCs
are recorded into the sidecar the store site passes by mutable
reference: where the site stages a copy of the value (scalar reg->mem
stores via `op_store`'s `Val`, `push64`'s parameter, staged vector
results) the note dies with that staging object — transient by design —
while a store from a live register sidecar (vector stores from xmm
registers, e.g. the `tests/history_store.c` MOVQ) records durably. The
provenance also lives in the memory itself: every poisoned byte carries
the trail of the operations that poisoned it — the poisoning request
seeds it with the call site, a store of a tainted value rewrites the
destination bytes with the stored value's trail plus the store PC, and a
load seeds the loaded value's trail from the merged per-byte trails plus
the load PC — so every value-carrying load/store round trip (a memcpy of
poisoned key material, a spill/reload through a stack slot) is recorded
end to end and the trail never restarts at a reload; `tests/history_memcopy.c`
pins a poison → byte-copy → reload → use journey and
`tests/history_order.c` pins the chronological ordering of the printed
trail (strictly ascending source lines for a guest whose merge order
scrambles). Only the most recent
128 steps of a huge trail are printed. It is heavy (the per-value
sidecar copies its trail on every merge, every poisoned byte carries a
trail vector, and the deep modes additionally record a stack per
poisoned operation, so large poison ranges with long trails cost
significant memory) and therefore off by default;
enable it when debugging a found fault, not for steady-state runs.

## Design in brief

- **Zydis-based interpreter.** Fetch/decode/execute with the vendored Zydis
  amalgamation (`zydis/`);
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
  against the loaded objects — the executable, its interpreter, and the
  file-backed guest mmaps like the shared libraries the guest ld.so
  loads; DWARF inline frames expand via `bfd_find_inliner_info`;
  return-address frames symbolicate at address - 1 like gdb) print
  before an `ET_CORE` dump that gdb/lldb read as pure guest state. The
  walk unwinds with the guests' DWARF CFI (`.eh_frame`/`.debug_frame`,
  the same mechanism gdb uses on the core — `src/dwarf_unwind.cpp`), so
  `-fomit-frame-pointer` code unwinds exactly; the RBP-chain walk and an
  approximate stack scan remain only as fallbacks for guests without
  usable CFI.
- **CPUID presence.** The hypervisor bit (CPUID.1:ECX bit 31) plus
  hypervisor leaf `0x40000000` with vendor string `"Zegarmistrz"`;
  leaves `0x40000001`+ read zero.

## Implementation details

For the agent/contributor-facing companion — build/test mechanics, the
cannot-index enforcement map, the var-time classification scheme, the raw
poisoning ABI, and the gotchas lists — read [AGENTS.md](AGENTS.md).

