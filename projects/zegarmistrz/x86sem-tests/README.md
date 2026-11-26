# x86sem-tests: the whole-state differential suite

This directory holds the zegarmistrz differential test suite built from
the kframework/X86-64-semantics single-instruction corpus: every corpus
program runs on the REAL CPU (instruction-by-instruction whole-state
trace) and under `../zegarmistrz --state-trace=FILE` (the same trace),
and a comparator checks the comparable architectural state after EVERY
executed instruction: the 16 GPRs, the six compared flag bits
(CF/PF/AF/ZF/SF/OF) and the 16 ymm registers. x87 stack registers,
MXCSR and DF are never compared (the corpus only observes them through
results that are compared elsewhere). A test passes iff all comparable
slots of all records match. This imports the kframework harness methodology 1:1 --
but with the hardware in the K slot and the emulator in the gdb slot --
so every divergence pinpoints an emulator bug: step number, register or
flag, expected (hardware) vs actual (emulator).

## Provenance and license

- Corpus: kframework/X86-64-semantics
  (https://github.com/kframework/X86-64-semantics), commit 592380ae,
  directory tests/single-instruction-tests/. Imported VERBATIM byte for
  byte, except for the appended harness epilogue below.
- Upstream license: University of Illinois/NCSA Open Source License
  (upstream LICENSE.md, (c) 2019 University of Illinois Urbana
  Champaign). Redistribution and use with the copyright notice and
  disclaimers retained; this README carries the required notice. The
  imported test.s files are unmodified upstream test programs (plus the
  clearly-marked epilogue), so the notice above satisfies the
  license for them.
- Upstream counts 123 `test.s` files in 120 directories; the three
  extras are byte-differently-headered duplicate copies of
  `shldq_m64_r64_cl/test.s` nested inside the
  `shldq_m64_r64_imm8/`, `shrdq_m64_r64_cl/` and
  `shrdq_m64_r64_imm8/` directories (upstream accident: each differs
  from the canonical file only in the first line, `.section .text` vs
  ` .text`). They are the same instruction, same form, same values --
  zero extra coverage -- so this suite imports the 120 canonical dirs
  once each. Every instruction the kframework suite tests is tested
  here. On top of the imported corpus there are TWO hand-written dirs,
  `sbb/` (plain-GAS register-form SBB sweep with explicit CF-setup
  variants -- stc/clc/cmc and arithmetic carries -- since SBB consumes
  the borrow): the ADC/SBB OF fix had corpus coverage only on the ADC
  half, and auto-collection picks the dir up without a runner edit.
  And `vblendvpd/` (VBLENDVPS/VBLENDVPD VEX.128+VEX.256 with the mask in
  non-XMM0 registers plus memory-source forms, alongside the legacy
  implicit-XMM0 BLENDVPD/BLENDVPS/PBLENDVB forms): the emulator used to
  read the mask from the implicit XMM0 for the VEX forms too -- found
  through supercop crypto_onetimeauth/poly1305 avx, whose masks live in
  ymm6/7/8/9/15 (a mask that happens to sit in XMM0 looks correct, so a
  gcc intrinsic probe can mask the bug). Stack adjustments in this dir
  use `leaq` (not `addq $N,%rsp`) because flag-bearing rsp arithmetic
  diverges legitimately between the two oracles' different stacks.

## The harness epilogue

The corpus programs end with a trailing `nop` and then run off the end
into garbage bytes (the upstream harnesses stop when RIP leaves the K
code map or at the 10000-step cap). Our guests must terminate cleanly
on BOTH sides, so each imported test.s ends with:

```
/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
```

exit(0) from the main thread. Neither oracle traces the syscall itself
(the emulator's syscall handler exits the process before any record;
the native oracle detects the syscall before executing it and exits),
so both traces end after the same last completed instruction and align
record for record.

## Oracles

- `native-trace.c` (default): a ptrace-free hardware oracle. It loads
  the guest into its own address space, arms the x86 trap flag, and
  jumps to the guest entry; every guest instruction then raises #DB
  (SIGTRAP) and the handler dumps the state and re-arms TF. No ptrace
  is needed (this container forbids ptrace; a gdb-based oracle cannot
  run here at all). The state comes from the kernel's signal frame:
  GPRs and EFLAGS from `uc_mcontext.gregs`, the vector state from the
  frame's xsave image (`uc_mcontext.fpregs`): the low 128 bits of each
  ymm from the FXSAVE legacy `xmm_space`, the upper 128 bits from the
  AVX state component (present iff XSTATE_BV has the AVX bit). Live
  register reads inside the handler are WRONG on this kernel (the FPU
  is deactivated at delivery; the first FP instruction lazily restores
  a stale image) and glibc helpers clobber ymm0-15 -- the signal frame
  is the kernel's authoritative copy and is what a ptrace dumper would
  see. Initial guest state matches execve: zeroed GPRs, zeroed and
  init-configured FP state (the entry-trap handler force-fixes the
  signal frame to that state and hard-fails if the dumper itself
  misbehaves), a fresh deterministic stack (fixed address,
  ADDR_NO_RANDOMIZE) with a minimal argc/argv/envp/auxv image, EFLAGS
  IF|TF|1. Guest faults flush the trace and exit 3 (a truncated oracle
  trace: the test then fails with a record-count mismatch, which is
  meaningful triage data -- e.g. `div` hits a real #DE on hardware
  earlier than the emulator does).
- `gdb-state-trace.gdb`: the same line format produced by gdb
  single-stepping (adapted from upstream scripts/script_3.gdb, `si`
  first then dump). Requires ptrace, so it cannot run in this
  container; it exists as the portable oracle for machines where gdb
  works and for cross-checking the TF-stepper. Select with
  `ZEG_X86SEM_NATIVE=gdb ./run-x86sem-tests`.
- `../zegarmistrz --state-trace=FILE`: the emulator side, one
  whole-state line per executed guest instruction, written after every
  `exec_one` (rip post-advance; the exit syscall produces no record).

### Trace line format (identical on both sides)

```
S rip=<16hex> rax=<16hex> rbx=<16hex> rcx=<16hex> rdx=<16hex> rsi=<16hex> rdi=<16hex> rbp=<16hex> rsp=<16hex> r8=<16hex> ... r15=<16hex> fl=<8hex> ymm0=<128hex> ... ymm15=<128hex>
```

- rip and the GPRs are 16 lowercase hex digits, natural hex (MSB
  first), no 0x prefix. fl is the raw 32-bit EFLAGS, 8 hex digits.
- Each ymmN is the 512-bit register as its eight qwords q7..q0 (MSW
  first), each qword printed big-endian, so the register's LOW qword is
  the LAST 16 chars of the slot.
- The upper 256 bits of the slot (zmm bits 256..511) are always zero:
  the kernel zeroes zmm state at execve, no corpus instruction writes
  above 256 bits (no EVEX), and the emulator keeps them zero. Both
  oracles write zeros there by construction.

## Comparison semantics (compare-traces.py)

Records align 1:1 by index (record k = state after instruction k on
both sides). Compared and skipped slots:

- rip: never compared (the two runners may place the guest
  differently). Shown in every divergence report as context.
- rsp, rbp: never compared (different stacks, same as the kframework
  harness which skipped them by register index).
- rsi, rdi: skipped when stack-like -- a value within
  [min(rsp_seen)-65536, max(rsp_seen)+4096] of ITS OWN trace. The
  corpus only points rsi/rdi at the stack (string ops on pushed
  operands) and the two runners use different stacks. This replaces
  the kframework K-side "symloc" detection (pointer-valued register).
- flags: CF/PF/AF/ZF/SF/OF (bits 0,2,4,6,7,11) are extracted and
  compared under the per-BIT defined/undefined model below. DF/IF/TF/AC
  and fixed bit 1 are skipped, matching the kframework comparator
  (kutils.pm parses only the six symbolic flag names and drops DF).
- ymm0..15: the full 512-bit slot is compared, except that a slot whose
  hex contains a NaN pattern on BOTH sides counts as equal -- ported
  from kutils.pm compareInts/containsNaN (substring patterns 7fffffff,
  7fc00000, ffc00000, ffffffff, 7fffffffffffffff, 7ff8000000000000,
  ffffffffffffffff, fff8000000000000). DEVIATION FROM UPSTREAM: the
  last pattern is the CORRECT 16-char double-quiet-NaN spelling; the
  upstream file has a 15-char typo there that can mask payload
  differences. Float ops producing NaNs carry implementation-defined
  payloads on hardware.
- Architecturally undefined flags: the SDM marks some flag OUTPUTS
  undefined (all six after div/idiv; SF/ZF/AF/PF after mul/imul; AF
  after logical ops and shifts; OF after shifts/rotates only when the
  count is 1; ...). The kframework harness skipped these via K undef
  marks, which a hardware-vs-emulator oracle does not have.
  compare-traces.py instead keeps a PERSISTENT PER-BIT state for the
  six compared bits -- defined/undefined -- and classifies the
  instruction at each record (resolved by objdump of the test binary,
  keyed by the record's rip) against the FLAG_PROFILES table:
  - a bit the instruction writes with a defined value becomes DEFINED
    (compared from that record on);
  - a bit the instruction writes with an undefined value becomes
    UNDEFINED (mismatches tolerated and counted -- real silicon
    produces run-to-run garbage);
  - a bit the instruction does not write is PRESERVED: hardware leaves
    the bit (including its undefined garbage) untouched, so it keeps
    its previous state until some instruction redefines it. The
    update happens BEFORE the record's flags are compared, and there
    is no window: nothing is ever closed unconditionally, undefined
    bits persist exactly as long as hardware would keep them.
  The state is seeded all-DEFINED at trace start (hardware starts from
  a fixed EFLAGS image; the initial record's flags are kernel-defined).
  The shift/rotate/SHLD/SHRD profiles are count-dependent: the count
  is resolved per record from the encoding (C0/C1 immediate from the
  objdump text, D2/D3 from the PREVIOUS record's RCX -- the
  pre-instruction CL; the record's OWN RCX is the post-instruction
  value, which for a CL-form shift whose destination is RCX is the
  shifted result, not the count -- D0/D1 = 1 -- `shl $1,%reg`
  assembles to the D0/D1 opcode with NO immediate, so the count cannot
  be read from an immediate alone). Record 0 has no previous record to
  read the pre-instruction CL from, so a CL-form count there is
  treated as unknown and falls back to the OF-undefined profile. The
  count is masked by the operand size (5
  bits, or 6 for REX.W). count == 0 writes NO flag bits (all
  preserved); count == 1 defines OF for shifts and rotates; count > 1
  leaves OF undefined. Mnemonics the table does not know default to
  PRESERVED -- preserved bits still compare whenever they are defined,
  so a misclassification cannot hide a defined-flag bug -- and are
  listed once on stderr (in <dir>/cmp.err) so the table grows
  explicitly; cmp.err also carries a one-line per-bit summary (entries
  held defined/compared vs undefined/tolerated) that makes the model
  observable per dir. Ambiguous families were classified empirically
  against the hardware oracle (which flag bits CHANGE across the
  instruction in the native traces, plus bit-identical repeated
  native-trace runs to rule out run-varying garbage): PEXT/PDEP and
  BMI2 SHLX/SHRX/SARX write no flags; POPCNT's ZF stays compared (the
  SDM defines it) while its CF is SDM-UNDEFINED after POPCNT and is
  tolerated by the model -- it happens to clear on this host's
  silicon, matching the emulator, but that is an implementation
  detail of this CPU, not a guarantee; the method is documented in
  the table's block comment in compare-traces.py.

## Running

```
cd x86sem-tests && ./run-x86sem-tests          # JOBS=<n> to tune
ZEG_X86SEM_NATIVE=gdb ./run-x86sem-tests       # gdb oracle instead
```

One line per test: `PASS x86/<dir>` / `FAIL x86/<dir>: <first
divergence>` / `SKIP x86/<dir>: <assembler error>`, then
`x86sem: N passed, M failed, K skipped`. Exit nonzero iff M > 0.
`ZEG_X86SEM_QUIET=1` limits output to the verdict lines and the tally
(this is what run-tests uses). Auto-collection: every `<dir>/test.s`
is picked up automatically; new corpus imports need no runner edit.

HOST-RELATIVE: the suite is calibrated to the LOCAL host, not bit-
portable across CPUs for the approximation instructions: the emulator's
RCPSS/RSQRTSS-style polynomial tables (and any other op whose hardware
behavior is implementation-defined, e.g. the undefined-bit garbage the
model above tolerates) match THIS machine's silicon. A different CPU
may legitimately diverge in those slots.

The suite is wired into `../run-tests` as the `x86sem-suite` case.
Everything lives under `test-output/x86sem/<dir>/`: `test` (the linked
guest), `native.strace`, `emu.strace`, `cmp` (verdict stamp), `cmp.out`
(comparator detail), `emu.stderr` (the emulator's fatal-error banner --
the missing-mnemonic triage source), `native.stderr` (native oracle
fault reports). Caching: native traces rebuild when the guest binary or
the oracle changes; emu traces when the guest binary or the emulator
changes; cmp when either trace or the comparator changes. A no-change
run costs ~0.2s.

## Divergence triage workflow

1. `./run-x86sem-tests` and read the FAIL lines (each names the first
   divergence record + rip).
2. For a failing dir, `cat test-output/x86sem/<dir>/cmp.out` for the
   full first-divergence report (slot, native vs emu values, rip,
   skipped slots).
3. Resolve the instruction: `objdump -d test-output/x86sem/<dir>/test`
   and find the entry at (record rip - instruction length), or grep the
   objdump for the rip shown (it is the NEXT instruction's address).
4. Classify: an emu.stderr banner naming `unimplemented <mnemonic>` is
   a missing mnemonic; a record-count mismatch with an emu.stderr
   fatal-error banner is the emulator dying mid-guest; a value or flag
   divergence with equal record counts is a silent wrong-value bug at
   that instruction.
5. Record findings in TRIAGE.md (the running triage report for this
   batch) and fix the emulator in a later batch.
