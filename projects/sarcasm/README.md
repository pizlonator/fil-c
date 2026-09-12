# sarcasm — SAfe Runtime Capability-enforced Assembler

sarcasm is a pizlonating assembler: it takes x86_64 or ARM64 assembly
annotated with Fil-C directives and rewrites it into a memory-safe object
file that links with Fil-C — invisicap pointers, capability-checked
accesses, the Fil-C ABI with safepoints and GC roots. It is clang's
default assembler for `.s` files, so annotated assembly gets full Fil-C
memory safety out of the box; `-yolo-assembler` is the escape hatch back
to the plain integrated assembler.

## Usage

The Fil-C driver invokes `pizfix/bin/sarcasm [--x86_64|--arm64] [-g]
INPUT.s -o OUT.o` for every `.s` file; standalone, the same CLI runs from
a Fil-C checkout (`projects/sarcasm/sarcasm.sh` runs the checkout copy):

    pizfix/bin/sarcasm [-o OUT.o] [-S] [--x86_64|--arm64] [--intel|--at&t] [--as CMD] INPUT.s

- `-S` / `--no-assemble` dumps the rewritten assembly instead of running
  `as` (without `-o` it writes `INPUT.yolo.s`); `--as CMD` overrides that
  assembler.
- The target and syntax are auto-detected; `--x86_64`/`--arm64` select the
  backend and `--intel`/`--at&t` pin the x86_64 syntax (default AT&T).

Annotations are mandatory and validated, never silently ignored: a wrong
direction, width, or shape is a compile-time rejection.

### Annotation markers

| Marker | Architectures |
|---|---|
| `;!` | both |
| `#!` | x86_64 only |
| `//!` | arm64 only |

- The earliest marker on the line, outside a string literal, splits the
  line into code and annotation body; text inside `"..."` never forms an
  annotation (`\"` escapes are honored). Write `#!` on x86_64 and `//!` on
  arm64, and keep `;!` where one file must assemble on either target.
- On x86_64 a `#` not followed by `!` is a plain comment (a trailing `#`
  comment is stripped from the body); on arm64 `//` without `!` is a
  plain comment, `/* ... */` wins over `//!`, and the body is verbatim.
- Annotations attach to the instruction's or label's own line (a marker on
  an otherwise-empty line inside a function is a compile error) and are
  per-architecture: `#!` in an arm64 file and `//!` in an x86_64 file are
  not markers, so the text stays in the code part and fails to parse.

### Signature annotations

Each defined function label carries its Fil-C signature in C style:

    hash: ;! unsigned(ptr)

The signature controls the internal calling convention: non-FP arguments
pack densely into GPRs (x86_64 `%rdx,%rcx,%r8,%r9` — further argument
words travel on the stack and are marshalled both ways, so signatures
are not arity-limited there; arm64 packs into six registers and keeps
its 3-argument/6-word limit), `float`/`double` arguments pass in
`%xmm0`-`%xmm7` (arm64 `v0`-`v7`) in declaration order among the FP
arguments, and a `float`/`double` result returns in `%xmm0`/`v0`. `ptr`
is the pointer type; `long double` and vector types are rejected. On
x86_64, SysV stack arguments (the 7th and later integer-class
arguments, read at `8+8i(%rsp)` at entry — directly, or through a
register that parked the entry `%rsp` with `movq %rsp,%reg` /
`leaq 0(%rsp),%reg`) are redirected to argument slots fed from the
incoming words.

### Pointer annotations

| Annotation | Applies to |
|---|---|
| `;! load ptr` / `;! store ptr` | plain 8-byte GPR pointer load/store |
| `;! atomic load ptr` / `;! atomic store ptr` | atomic pointer load/store through the Fil-C runtime, exactly like C11 `_Atomic` (x86_64 plain `movq`; arm64 `ldr`/`ldur`/`ldar`/`ldapr` and `str`/`stur`/`stlr`, 64-bit, no writeback) |
| `;! atomic ptr` | pointer compare-exchange: x86_64 `cmpxchgq` with a memory destination (`lock` allowed and irrelevant), arm64 64-bit `cas` forms |
| `;! load store ptr` | x86_64 non-atomic pointer RMW over a memory slot: 8-byte add/adc/and/or/sbb/sub/xor, inc/dec/neg/not |
| `;! atomic load store ptr` | atomic pointer RMW: x86_64 memory-destination RMW (with `lock`, a runtime compare-exchange loop); arm64 64-bit LSE RMW (`ldadd`/`ldset`/`ldeor`/`ldclr`/`swp` and aliases) |

- The annotation must match the instruction exactly; cmpxchg8b/cmpxchg16b,
  casb/cash/casp, xadd, and any shape mismatch are compile-time
  rejections.
- For `;! atomic ptr` the expected value is the accumulator (x86_64) or
  the compare register (arm64); the old value returns with its
  capability, and flags are recomputed for a following branch/`setcc`.
- adc/sbb with `;! load store ptr` run without trapping, but the carry-in
  is clobbered: carry semantics are not preserved.

### Capability save/restore (both)

Any instruction that produces a value into a register can carry
`save capability (name)` or `restore capability (name)` (`;!` on both
architectures, `#!`/`//!` also accepted on x86_64/arm64 respectively), where
`name` is any `[_A-Za-z][_A-Za-z0-9]*` variable name:

    movq %rdi, %rbx  #! save capability (tbl)
    ... pointer math that is not capability-preserving ...
    addq $0, %rbx    #! restore capability (tbl)

- `save` records the destination register's capability under `name`; a later
  `restore` re-attaches that exact capability to its own destination
  register, overriding whatever pointer flow would otherwise compute. Every
  `restore` must be dominated by its `save` (every path from function entry
  to the restore executes the save).
- The recorded capability is a snapshot of the capability flowing *into* the
  save instruction, frozen at pointer-flow convergence — clobbering math on
  the same register between the save and the restore cannot pollute it. A
  wrongly restored capability can only trap (its bounds check still guards
  every access), never access out of bounds.
- The restored value is an ordinary pointer web sharing the saved lower, so
  GC rooting, spilling, and checked accesses treat it exactly like the saved
  value — no extra emission is involved.
- The clause must stand alone as the whole annotation (combine it with
  nothing — copy the value through a `mov` first if the instruction already
  carries one). A name may only be saved once per function. Restoring a
  dynamically-tracked capability (a register merging pointers from different
  origins) is rejected, as are restores without saves, undominated restores,
  and uses on instructions with no register destination — all clean
  compile-time errors.

### Capability source selection (both)

One standalone annotation picks which capability flows where when the
default rules would pick the wrong one (`;!` on both architectures,
`#!`/`//!` also accepted on x86_64/arm64 respectively). It names a
register as an ordinary operand (`%r11` in AT&T, `r11` in Intel — the
`%` is optional); `%fil_` pseudos may be named too. A wrong choice can
only trap (its bounds check still guards every access), never access
out of bounds. (`new capability` is a deprecated alias for `use
capability` — existing perlasm using it still assembles — as are the
historical typo spellings `new capabiltiy` / `use capabiltiy`.)

| Annotation | Applies to |
|---|---|
| `use capability %reg` | address arithmetic (`add`/`sub`/`lea`/`and`/`or`) and any instruction with a memory operand |

- On address arithmetic, the result draws its capability from the NAMED
  input register instead of pointer flow's pick. It is needed when several
  inputs carry capabilities (a plain `add` keeps a single capability
  source automatically; shifts never propagate one):

      movq %rsi, %rax
      subq %rsi, %rax      # integer 0, but the web stays sticky
      addq %rax, %rdi      #! use capability %rdi

  With zero or one capability sources the annotation is unnecessary but
  harmless (it must still name a capability-carrying use — naming a
  scalar is a compile-time error).

- On any other instruction with a memory operand (loads, stores, vector
  moves like `vmovdqu`, ...), the memory access is guarded by the NAMED
  register's capability, which must be the memory operand's base or
  index. It covers the case where the assembler moved the logical base
  out of base position (x86_64-xlate.pl flips `disp(%r13,%rdi)` to
  `disp(%rdi,%r13)` when the base is `%rbp`/`%r13` — the address is
  unchanged, but the base-first selection would then guard with the
  wrong object — so `vmovdqu %xmm,(%a,%b) #! use capability %a` keeps
  guarding with the written base), and stale-base shapes:

      xorq %rdi, %rdi                         # scalar 0, stale web
      movq (%rdi,%rsi), %rax #! use capability %rsi

  The effective address is still computed from base+index+disp as
  written — only the capability source changes. Using the annotation on
  any other mnemonic, or naming a register the instruction does not use
  (or, for a memory operand, a register that is neither base nor index),
  is a compile-time error.

  On a `lea` the annotation selects the computed value's pointer
  source. An arithmetic instruction that also has a memory operand gets
  both effects (for example `add (%rsi),%rdi #! use capability %rsi`:
  the `(%rsi)` load is guarded by `%rsi`'s capability and the `%rdi`
  result draws from it too). On such a dual-effect instruction the named
  register must satisfy both rules at once: a general-register use of the
  instruction that is also the memory operand's base or index — naming a
  use that is neither (like `%rax` above) is a compile-time error, since
  the access would otherwise be guarded with the wrong capability.

### Global variables (x86_64)

Same-file data becomes real Fil-C globals automatically, and extern globals
are one annotation away:

- Contiguous top-level data under `.section .rodata*`/`.data`/`.bss` (plus
  `.comm`/`.lcomm`) is collected into Fil-C data objects mirroring the
  compiler's per-global emission: bounds, alignment and write permission are
  checked exactly like compiled code, and a store to read-only data traps.
  Scalar initializers only (`.byte/.short/.long/.quad` numeric, `.float/
  .double`, `.ascii/.asciz`, `.zero/.space/.fill`, alignment padding);
  `.quad label` pointer initializers are a compile-time rejection. A
  `.comm name,size,align` becomes a strong writable definition (documented
  semantic change: commons lose cross-module merging).
- References to same-file data need no annotation: `leaq tab(%rip),%r` seeds
  a checked pointer (indexed table traffic just works), direct accesses like
  `movl g+4(%rip),%r` or `movdqa K256+512(%rip),%xmm7` run the ordinary
  capability check, and `#! load ptr` / `#! store ptr` maintain pointers
  through globals (pointer stores into writable globals register them with
  the GC).
- `#! global ptr` — on a rip-relative lea or any rip-relative memory access
  to an EXTERN global (defined in another module): materializes the flight
  pointer by calling the global's `pizlonated_<sym>` getter, then seeds
  (lea form) or checks-and-accesses (direct form). Extern globals without
  the annotation are compile-time rejections, as is any unresolvable
  rip-relative reference. arm64 keeps the historical rejections.

### Function references (x86_64)

`#! funcref` on a rip-relative lea to a FUNCTION symbol materializes the
function's flight pointer (its function-object capability), so the register
can be stored with `#! store ptr` into a function-pointer table that C later
calls indirectly — the OpenSSL poly1305_init shape:

    leaq poly1305_blocks(%rip),%r10 #! funcref
    ...
    movq %r10,0(%rdx) #! store ptr

- A same-file function (or alias entry) materializes its function object
  directly (`leaq pizlonatedFO_<fn>+16(%rip)` into both the intval and the
  capability lower — exactly the getter's output, a link-time constant into
  static ELF memory that needs no GC rooting).
- An extern function materializes through its cross-module getter
  (`pizlonated_<sym>`, the `#! global ptr` contract — an injected nounwind
  runtime call).
- The pointer flows through copies, address arithmetic and cmov selection
  chains (ptrflow's lockstep lowers: a cmov merging two function pointers
  gets a dynamic lower maintained by a conditional lower cmov with the same
  condition, emitted immediately after the instruction's own cmov).
- A data-object, body-local-label, or local-subroutine target is a clean
  compile-time rejection (a function needs a function object to point at).
  The stored capability passes the runtime FUNCTION-type check when C calls
  it indirectly. arm64 rejects `funcref` with a clean "not yet supported".

### `.section .init` / `.section .fini` content (x86_64)

At top level, inside `.section .init` (and `.section .fini` for symmetry), a
straight-line sequence of annotated DIRECT calls is accepted — the OpenSSL
x86_64cpuid shape:

    .section .init
    call OPENSSL_cpuid_setup #! void()

Each call must carry a `void()` signature annotation (the .init context runs
from `_init` as plain SysV code — there is no Fil-C argument state, so only
void() calls marshal soundly); labels, data directives, non-call instructions
and unannotated calls are clean rejections, as is a data-object or
local-subroutine target. The calls are emitted back into the same section as
Fil-C constructor calls through `filc_defer_or_run_global_ctor` (exactly what
filcc emits for a C constructor), preserving order — a same-file function's
function object is materialized directly, an extern function resolves through
its `pizlonated_<sym>` getter. Fil-C runs .init constructors, so the calls
run at process startup. arm64 rejects .init content with a clean "not yet
supported". `.section .init_array` pointer initializers stay rejected.

### Call annotations

- Every direct call carries the callee's signature — `bl foo //! int(ptr,
  size_t)`; an unannotated direct call is rejected. It is retargeted to
  the callee's pizlonated fast entrypoint; a cross-module call goes
  through a resolver that validates the target and marshals a
  mismatched signature via the generic buffer calling convention.
- A register-indirect call carries an inline signature — `call *%rax #! ptr(int)`
  (x86_64) or `blr x8 //! ptr(int)` (arm64). The target register must hold
  a known pointer value (a pointer argument, a `;! load ptr` result, or a
  pointer-returning call); the emitted code checks FUNCTION type,
  canonical entrypoint, and signature before calling.
- Rejected: unannotated register-indirect calls, memory-indirect calls
  (`call *mem` — load the pointer into a register first; arm64 `blr` is
  register-only), and indirect branches (`jmp *%rdi`, `br xN`) — an
  uncontrolled branch cannot be made memory-safe.

### Local subroutines (x86_64)

An unannotated `call` to a file-local subroutine works with NO signature —
the OpenSSL perlasm shape: a non-`.globl`, non-signature label (with or
without `.type name,@function`) whose region contains a `ret`, called from a
function body (or from another subroutine) with a custom caller/callee
register convention. sarcasm compiles each call as an unconditional jump to a
per-caller CLONE of the subroutine and each clone `ret` as a multi-way branch
back to the continuations, so lift/regalloc color caller+clones as one
function — argument/result registers, clobbered caller-saved registers, and
the caller's frame slots all follow the ordinary web rules. Details:

- A call target may also be a label in the MIDDLE of a subroutine's body (the
  clone is then [mid-label .. the region's ret(s)]).
- Nested non-recursive local calls work (each clone gets its own
  continuations); recursion — direct or mutual — is a compile-time error.
- The stack pointer is never moved (no hardware push), so a subroutine
  written against the return-address-compensated `N+8(%rsp)` convention (the
  rsaz/mont5/aesni-gcm subs) sees its rsp-relative displacements biased by -8:
  its `8(%rsp)` keys to the caller's slot 0, and `leaq 8(%rsp),%rdi` becomes
  `leaq 0(%rsp),%rdi`, which resolves into the caller's alloca region.
- `#! local` is accepted as an optional explicit marker on such calls
  (validated to resolve the same way; a mismatch is a compile error).
- A subroutine may set up its own frame with a constant `sub` (torn down
  before its `ret`): the clone accesses the caller's frame and spills like
  any other code, keyed at the perturbed depth with the same -8 bias.
- The flags flow into the subroutine (a hardware `call` preserves them) and
  are clobbered by its return (the ret dispatch compares). A subroutine that
  falls off its end, a
  branch out of a subroutine that is not a mid-body tail join (below), and a
  `.globl` no-signature label are compile-time errors; on arm64 a discovered
  local subroutine is a clean "not yet supported" error.

### Cross-function jumps (x86_64)

Two OpenSSL perlasm shapes are supported (arm64 rejects them with a clean
"not yet supported" error):

- **Tail branches to function entries** (`jmp/jcc` to a sig-annotated
  same-file function or alias entry, or to an extern symbol carrying an
  inline signature, `jne asm_AES_cbc_encrypt #! void(...)`): rewritten into
  an ordinary annotated CALL (the full Fil-C marshalling of the current SysV
  arg-register webs — 7th+ arguments ride the jumper's own incoming
  stack-argument webs) plus a jump to the function's synthetic epilogue, at
  any frame depth. The callee's return value becomes the jumper's return
  value. An annotated jump whose target has no matching signature is a clear
  compile-time error; an unannotated jump to a non-local label keeps the
  plain tail-call rejection.
- **Mid-body shared-tail joins** (`jmp/jcc` into a label inside another
  same-file function's or subroutine's body): the region [label .. ret(s)]
  is cloned into the jumper (renamed labels, the region's rets returning
  from the jumper — no call, no return address, no clone bias), with local
  calls cloned transitively. A tail join from inside a local subroutine
  (mont5's `jmp .Lsqr4x_sub_entry`) clones the target tail into each caller
  with the jumping subroutine's clone context and continuations.

Alias entry labels (a label immediately adjacent to a sig-annotated function
label — asm_AES_encrypt:/AES_encrypt: or sha1's _shaext_shortcut:) share the
function's signature and body: jumps to them resolve to the function, and a
`.globl` alias gets its own getter/direct-call symbols so C callers link.

### Pseudoregisters (both)

Handwritten assembly sometimes needs a scratch value without sparing a physical
register. Name it `%fil_<ident>` on x86_64 (in Intel-syntax files the `%`
prefix is still required, so a bare `fil_<ident>` keeps its plain-symbol
reading) or `fil_<ident>` on arm64, where `<ident>` matches `[0-9a-zA-Z_]+`
and may start with a digit:

    movq %rdi, %fil_tmp
    addq %rsi, %fil_tmp
    movq %fil_tmp, %rax

- A pseudo is a 64-bit GPR with no fixed physical register: sarcasm
  register-allocates it exactly like a spilled GPR web (def/use/kill, calls,
  spills, pointer capabilities, `save`/`restore capability` all work — a
  pseudo holding a pointer dereferences like any GPR).
- Pseudos are GPR-only: any instruction combining a pseudo with an FP/vector
  register (`movq %fil_a, %xmm0`, `fmov d0, fil_a`) is a compile-time error,
  as is a malformed name (`%fil_`, `%fil_foo-bar`).
- On arm64 a bare `fil_<ident>` in operand position is always a pseudo, never
  a symbol — do not name globals, functions, or local labels with a `fil_`
  prefix.

### The `.alloca` directive (both)

Stack allocation is a GC allocation (`filc_allocate`), not stack memory — and
it never touches `%rsp`/`sp`, so the input's own stack math keeps its meaning:

    .alloca <size>, <alignment>, <result>

- `<size>` and `<alignment>` are each an immediate, a GPR (including a
  pseudo-register), or a frame-relative spill slot (`-8(%rbp)` / `[sp, #8]`);
  `<result>` is a register, a pseudo-register, or a spill slot. (x86_64 AT&T
  accepts `$N` or bare `N` immediates; operands stay in written order.)
- Semantics: allocate `size` bytes with `alignment`, return the buffer pointer
  in `result`. The buffer is a garbage-collected object: the pointer may
  escape and outlive the function, and freeing it is a no-op. The returned
  pointer satisfies `alignment` (which must be a positive power of two —
  immediates are checked at compile time, registers at runtime with a clean
  trap otherwise) and the whole `size` is writable through it.
- Accesses through the result are capability-checked exactly like any heap
  pointer (misaligned pointer-sized accesses trap; scalar/vector accesses
  follow the usual hardware-alignment rules).
- Alignment is consolidated in the directive: the result already satisfies
  `alignment` (wider alignments over-allocate and align up), so call sites
  use it directly for aligned traffic — no per-site `and $-16` masking and
  no `and $8` / `xor $8` rounding dances. Such masking is unnecessary, and
  `and` on a pointer preserves its capability anyway (the value is masked,
  the capability stays; an out-of-bounds result traps at the access).
- The historical `;! alloca ...` annotations (`alloca size (x)`,
  `alloca result (x)`, `alloca result size=N`) were removed: they rewrote the
  input's `%rsp` math, changing the original assembly's stack semantics. Any
  `alloca` annotation is now a compile-time error.


### Frames and the stack pointer

- The prologue is the leading run of callee-saved pushes, `movq %rsp,%rbp`,
  and `subq $imm,%rsp`; the frame geometry comes from that prefix.
  `enter` is rejected.
- %rsp writes are legal only (a) in the prologue (callee-saved pushes,
  `movq %rsp,%rbp`, `subq`/`addq $imm,%rsp`, `and $-N,%rsp` for a power of
  two N >= 16, and a `movq %rsp,%reg` save into a callee-saved register),
  (b) as the alloca allocation, (c) as %rsp recovery (`movq %rbp,%rsp`,
  `leaq N(%rbp),%rsp` with the frame pointer established, or
  `movq %reg,%rsp` from an unredefined prologue save), (d) as a proven
  mid-function constant adjustment (`addq`/`subq $imm,%rsp`,
  `leaq K(%rsp),%rsp`, or `and $-N,%rsp` at a known depth with dead flags),
  and (e) as epilogue teardown — everything else is a compile error.
- A register holding `%rsp` or `%rsp`+offset (`movq %rsp,%reg`,
  `leaq K(%rsp),%reg`) is a stack+offset alias: accesses at offsets from it
  are ordinary stack accesses at statically known offsets. Control flow that
  leaves it ambiguous (stack+offset on one path, heap/alloca/argument on
  another) is a static error at the access, as are returning or storing it.
- Frame slots (x86_64 spellings; sp/x29 analogous on arm64) are
  virtualized into register-allocated locals with compile-time bounds —
  no capability needed; the 128-byte SysV red zone is legal. Accesses
  outside the frame, caller-argument-area writes, and taking the frame's
  address are rejected. Aligned vector accesses wider than 16 bytes
  (vmovdqa32/64) need a covering `and $-N,%rsp` note plus an aligned offset
  (the frame is then emitted aligned); otherwise they are compile errors
  suggesting the unaligned form.
- A body that can fall off its end without `ret`, and a branch to the
  function's own entry label, are rejected.

## Build & install flow

sarcasm is written in Luau in this directory
(`projects/sarcasm/sarcasm/`). The Fil-C build never runs it from the
checkout: `./build_sarcasm.sh` (run from the repo root) installs the
modules into `pizfix` as a minilute entry script, `pizfix/bin/sarcasm`,
which is what clang and the test suite actually execute. Re-run it after
editing the sources; un-installed edits have no effect on clang or test
runs.

## Safety model

- Every memory access is capability-checked: bounds and alignment against
  the pointer's capability, and write permission on every heap write
  (read-only objects trap). A heap access whose base has no capability —
  an integer address — traps at runtime with a null capability.
- The stack frame is virtualized (compile-time bounds, red zone legal),
  and allocas become garbage-collected allocations.
- Pointer loads, stores, atomics, and RMWs maintain the capability through
  memory (including the aux capability array and the GC store barrier), so
  pointers round-trip and stay dereferenceable.
- A defined set of unsafe instruction classes is rejected at compile time:
  state-corrupting and privileged forms (segment-selector and FS/GS-base
  writes, `swapgs`, TSX, `syscall`/port I/O/`hlt`, MSRs, descriptor-table
  loads), anything whose memory semantics cannot be modeled (the unmodeled
  string/`rep` instructions — lods/scas/cmps, repne/repnz, `rep` anywhere but
  movs/stos/ret/nop — `xchg` with memory, gather/scatter, AMX tiles, unknown
  mnemonics with memory operands), symbolic and absolute addresses,
  indirect branches, memory-indirect calls, and `lock` outside the modeled
  memory-destination RMWs. `rep movs*` / `rep stos*` (and their bare
  single-step forms) ARE modeled: bounds-checked block copies/fills over the
  implicit registers (see DESIGN.md).
- Unknown register-only forms pass through with conservative def/use
  modeling; FP/SIMD/NEON registers pass through as written (memory
  operands checked at the exact width); non-pointer atomics (x86_64
  `lock`ed RMWs, arm64 LSE and load/store-exclusive families) are single
  checked accesses.
- Exceptions do not unwind through x86_64 sarcasm frames (a C++ throw in
  a callee terminates the process); arm64 frames propagate.

DESIGN.md has the full model, including per-access check sequences and the
complete reject lists.

## Testing

Tests live in `filc/tests/` in directories named `sarcasm*`; run
`filc/run-tests -f sarcasm` from the repo root (`-t <name>` for one). The
suite asserts results, traps, and compile rejections; extension-dependent
tests skip cleanly on unsupported hardware.

## More documentation

`DESIGN.md` has the design and full semantics; `ABI-NOTES.md` and
`ABI-NOTES-x86.md` decode the Fil-C ABIs sarcasm emits.
