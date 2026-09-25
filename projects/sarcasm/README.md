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
| `;! load ptr` / `;! store ptr` | plain 8-byte GPR pointer load/store. Also accepted on a plain 8-byte register<->frame-slot move (`movq %reg,-8(%rsp) ;! store ptr`): a virtualized slot is an ordinary GPR web, so the capability rides pointer flow exactly like a register move (see "Frames and the stack pointer") |
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
  `leaq 0(%rsp),%rdi`, which is legal only as a frame-interior carrier (a
  value use of the frame address is a compile-time error).
- `#! local` is accepted as an optional explicit marker on such calls
  (validated to resolve the same way; a mismatch is a compile error).
- A subroutine may set up its own frame with a constant `sub` (torn down
  before its `ret`): the clone accesses the caller's frame and spills like
  any other code, keyed at the perturbed depth with the same -8 bias.
- The flags flow into the subroutine (a hardware `call` preserves them) and
  are clobbered by its return (the ret dispatch compares). A subroutine that
  falls off its end (a `ud2` is not a fall-off — a trap ends the path, like a
  `ret`), a
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
  with the jumping subroutine's clone context and continuations. Frame-
  interior leas in clone statements at the jumper's prologue depth (dd ==
  D0 — the sha1-mb shared 4x/8x bodies) take the D0-interior memory-only
  carrier relaxation exactly like the jumper's own leas.

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


### Data directives in function bodies (x86_64)

A contiguous run of data directives inside a function body is DISASSEMBLED:
`.byte`, `.word`/`.short`/`.2byte`, `.long`/`.int`/`.4byte` and
`.quad`/`.8byte` runs are decoded into the x86_64 instructions they encode
(each directive contributes its values in little-endian byte order, so
`.long 0x9066A4F3` decodes to `rep movsb` plus a 2-byte `nop` pad). This is
what makes OpenSSL's perlasm output compile unmodified — it spells whole
instructions as raw bytes (`0xf3,0xc3` rep ret, `243,15,30,250` endbr64, the
AES-NI/SHA-NI/movbe families, `.long 0x9066A4F3`, ...).

- Every byte of the run must decode into exactly one instruction whose text
  re-parses like a spelled instruction; a run that cannot be fully decoded
  stays data and is rejected (`data in a function body is not supported ...`)
  — literal pools and jump tables are NOT supported. Only plain integer
  literals count (decimal / `0x`-hex / `0b`-binary, optionally negative);
  symbolic expressions (`.long .L1-.L0`) keep their data spelling.
- Coverage: the legacy one-byte map (ALU/mov/inc/dec/push/pop/test/xchg/lea/
  shifts/imul/div groups, flag ops, string ops incl. `rep` forms, setcc,
  jcc/jmp/loop/jrcxz), the 0F map (jcc near, setcc, cmovcc, movzx/movsx,
  imul, bt/bts/btr/btc, bsf/bsr/popcnt, shld/shrd, cmpxchg/xadd, bswap,
  cmpxchg8b/16b, rdrand/rdseed, rdtsc/cpuid/xgetbv/monitor/mwait, and the
  SSE/SSE2/SSE3/SSE4.1 FP and packed-integer maps with their 66/F2/F3
  prefix selectors), the 0F 38 / 0F 3A maps (movbe, crc32, ptest, palignr,
  pextr*/pinsr*, pclmulqdq, aeskeygenassist, sha1rnds4, ...), and the VEX/
  EVEX shapes sarcasm models. Instructions sarcasm does not model
  (`syscall`, `wrmsr`, ...) decode and then fail with the classifier's
  per-instruction message — the same rejection a spelled instruction gets.
- Soundness: prefixes are never dropped where they change semantics
  (`fs`/`gs` overrides and a mid-run `0x67` make the run undecodable); a
  `0xf0` lock byte joins the following instruction (in the run, or the
  spelled instruction after the run); a branch decodes only when its target
  is an instruction boundary inside the same run (a synthetic local label is
  planted there); `call`, far transfers, RIP-relative and absolute forms
  stay data (a data run has no symbol to name them).
- An annotation on the run transfers to the first decoded instruction:
  `.long 0x9066A4F3  #! stack buffer (f, %rsp, %rsp + 16)` is a decoded
  `rep movsb` carrying that annotation (validated exactly like the spelled
  form). Two annotations on one run are ambiguous and keep it data.


### Stack buffers (x86_64)

The `#! stack buffer (...)` annotation declares a byte range of the function's
own frame as a RAW BYTE BUFFER — real memory in sarcasm's synthesized frame,
addressed without capability checks:

    #! stack buffer (name)                            # short form
    #! stack buffer (name, %reg + lo, %reg + hi)      # long form

- `<name>` matches `[A-Za-z_][A-Za-z0-9_]*`. In the long form both bounds use
  the SAME base register; the offsets are integer literals (decimal or hex,
  negative allowed), and a bare `%reg` means offset 0. Both orders of
  `+`/`-` are accepted (`%rsp - 16`, `%rbp + 8`).
- The annotation is only meaningful on (a) an instruction with a
  stack-relative memory operand (base `%rsp`/`%rbp` or a live stack-alias
  register) — which enables INDEXED accesses (`movl (%rsp,%rax),%ecx`,
  scales 1/2/4/8) that are otherwise rejected — or (b) a `rep movs*` /
  `rep stos*` instruction (and its bare single-step forms). Anywhere else it
  is a compile error. It must be the instruction's only annotation.
- The long form resolves to a normalized frame range: the base register's
  stack offset at the declaring statement (%rsp at depth d: `D0 - d`; %rbp
  with an established frame pointer: `rbpNorm`; any other register must be a
  live saved-rsp stack alias) plus the written offsets. Every long form of a
  given name — across the WHOLE FILE — must resolve to the same range
  ("the offsets must match"); two long forms sharing one alias register that
  aliases the stack at different offsets are rejected.
  Two exceptions carve out B2 SHARED-TAIL CLONES (statements marked by the
  cross-jump pass, e.g. the OpenSSL ChaCha20 variants' tails cloned into
  ChaCha20_ctr32):
    * A long form on a clone statement resolves IN THE CLONE'S OWN CONTEXT —
      the clone executes at the jump site's rsp with its own `sub`/`and` as
      ordinary code, so its depth marks are the natural continuation of the
      jumper's tracking, and the same real bytes simply normalize to a
      different range in the jumper's coordinates. Clone declarations do not
      participate in the file-wide canonical table (they cannot satisfy short
      forms and cannot conflict with them), and the clone's buffer accesses
      key into the clone's own coordinate band (the same +band shift every
      other clone access gets), so a clone entered above the jumper's
      prologue never collides with the jumper's own slots. Clone
      declarations must use %rsp as their base.
    * A short form on a clone statement is REJECTED (a clone's window is
      private to its own frame context; declare the buffer with the long
      form inside the cloned region).
- The SHORT form references a buffer some long form declared: it is valid
  only if the name has a long form somewhere in the file, and the using
  function independently re-checks that the range fits ITS OWN frame.
- Alignment: an aligned access (movdqa-class) whose range falls inside a
  buffer must land on an aligned byte of the lowered region. Every aligned
  access contributes its own (offset, requirement) pair; a group is
  placeable iff its pairs are jointly congruent (one output base exists) and
  the synthesized frame's effective alignment backs the widest requirement.
  The effective alignment is the widest requirement any materialized cluster
  OR buffer group carries (16 when none needs more) — so an `and $-N, %rsp`
  frame (the perlasm dynamic-alignment idiom) admits aligned buffer traffic:
  all of a buffer's post-`and` accesses shift together with the and's
  dynamic slack, so their relative offsets carry the whole alignment story.
  A buffer in a frame whose effective alignment is only 16 rejects aligned
  >16-byte accesses (use the unaligned form); GPR traffic carries no
  alignment promise and always works. Traffic that can REACH a mid-function
  `and` (pre-and numbering) may not share bytes with a buffer: the same
  modeled offset names different real addresses on the two sides of the
  and's slack.
- Validity (compile errors otherwise): the range must sit inside the
  function's own carved frame — at or above the declaring statement's
  provably-allocated stack floor (`lo >= D0 - dDecl`, which is `lo >= 0` for
  an ordinary frame: no red zone), past the outstanding push/callee-save
  slot area, disjoint from the outgoing call-argument area, and out of any
  `.alloca` region. The floor rule admits the mid-function INNER-FRAME
  shape (the prologue scan ends in a leading half of the function, so a
  later push+sub keys as an inner frame whose bytes normalize to negative
  offsets — the aesni CBC decrypt tail): at a provable rsp depth `d` the
  function's own pushes/subs have allocated every byte from `entry_rsp - d`
  up, so a buffer over `[D0 - d, ...)` covers the function's own allocated
  bytes; anything below the floor was never allocated on that path and
  stays rejected. Overlapping (or touching) buffers are allowed and merge
  into ONE larger lowered region. The push-slot rule is checked exactly
  against the declaring statement's own outstanding saves (a clone's own
  pushes park their slots below the jumper's frame, where the transient
  prologue pad model cannot see them).
- Accesses: every static (non-indexed) stack access whose normalized byte
  range falls fully inside a buffer is automatically redirected into the
  lowered region — no annotation needed, and GPR, SIMD and alias-based
  spellings of the same bytes all hit the same memory. rsp-based accesses
  key at `disp + D0 - d` at a KNOWN perturbed depth, exactly like the
  rewrite's slot keying, so inner-frame buffer traffic is recognized. An
  access straddling a buffer boundary is a compile error (ambiguous).
  `store ptr` / `load ptr` on buffer-range accesses are rejected: buffer
  memory never holds a capability — loading from it always yields a
  null-capability scalar.
- Indexed accesses annotated with the buffer get a runtime BOUNDS CHECK on
  the index — one overflow-safe unsigned compare,
  `idx_min = ceil((lo - disp)/scale)`, `idx_max = floor((hi - size -
  disp)/scale)`, fail iff `(idx - idx_min) >=u count` — then the instruction
  runs verbatim with its displacement translated into the lowered region
  (`disp'(%rsp,%idx,s)`). An out-of-range index traps with the usual
  `filc safety error: cannot read/write pointer with ptr >= upper.`
  attribution over a `stack_optimized(offset=...,size=...)` pointer (the
  same shape the C compiler's stack checks report).
- Rep string ops: when a `rep movs*`/`rep stos*` (or bare single-step) has a
  stack-alias carrier as its source (%rsi) or destination (%rdi) — e.g. the
  aesni CBC `movdqu %xmm0,(%rsp); leaq (%rsp),%rsi; rep movsb` shape — it
  MUST carry a `#! stack buffer (id, ...)` annotation covering the alias's
  stack offset. Sarcasm emits a dynamic count check (`N <= hi - K`, with
  `N = %rcx * element` computed exactly as the hardware does) before the
  rep, materializes the alias into the physical register from the lowered
  region, and keeps the OTHER side's capability checks (a heap pointer stays
  fully checked, including the dynamic count). A count of zero copies
  nothing and traps nothing, like the hardware. The alias register's value
  after the rep is the lowered (advanced) buffer address.
- By-ADDRESS accesses (Feature A): a register may hold the ADDRESS of a
  declared buffer's bytes — materialized by a buffer-interior `leaq K(%rsp),
  %reg` whose computed offset lands inside a declared buffer's range, then
  computed on (add/sub/xor/or), stored to memory and reloaded, or cmov'd.
  Sarcasm lowers the value `leaq` into the lowered region (tagged so the
  frame-base shift finalizes it — the web's runtime value IS the output
  address of exactly the byte the input lea computed), and any access through
  such a web annotated `#! stack buffer (id)` (the short form) lowers to a
  RUNTIME BOUNDS CHECK of the address against the buffer's lowered range
  followed by the RAW instruction — no capability check (a buffer address is
  a capability-less stack address; the ordinary checked path would trap on
  it). The check is one overflow-safe unsigned compare: with `t1` the
  lowered group's base address, `disp` the access's constant displacement and
  `w` its width, it fails iff `(base - t1 + disp) >=u (span - w + 1)`,
  `span = hi - lo` of the merged group — accepting exactly the addresses
  whose `disp`-offset access lies inside the group (an access ENDING exactly
  at the group's top byte is inside; a negative `disp` folds in with
  wrapping semantics, mirroring the indexed check's modular arithmetic).
  Failure traps with the usual `filc safety error`
  attribution over a `stack_optimized(offset=...,size=...)` pointer. The
  ORIGINAL instruction is emitted verbatim (original base register, original
  displacement); a lowerable `{%kN}`-masked vector move lowers through the
  frame pass's scratch register with the FULL width bounds-checked first.
  Static rules: the base must be a general-register web (an FP/SIMD or
  %rip base has no byte address to check), the access must have no index
  register (an unbounded index cannot ride a base-only check — use the
  indexed form through %rsp), a base that provably resolves to a stack
  address (a live saved-rsp carrier, an established frame pointer) takes
  the existing static paths instead, and `store ptr`/`load ptr` stay
  rejected. A SHORT form is the by-address mode by declaration (any
  general-register web). A LONG form may also ride a computed base — the
  declaration is independent of the access's base register (it is spelled
  %rsp-relative and resolves from the statement's own depth context,
  band-aware for B2 clones) — but only when the base web is TAINTED by a
  buffer value `leaq` (the taint seeds at every value lea's destination and
  spreads through every modeled GPR definition, never dying): a long form on
  a register the buffer never touched (an argument or heap pointer) stays
  rejected, because a declaration must name the stack bytes it drives on.
  An UNANNOTATED access through a register that provably or
  possibly holds a buffer address is a compile error naming the buffer
  ("annotate the access") — redefining the register by ALU drops the
  property (an ordinary web again). The static FAST PATH is not taken:
  every by-address access is runtime-checked even when the base provably
  still holds the raw value-lea result (correctness first; the lea's
  emitted displacement and the check share the same tagged group base, so
  they always agree).
- By-address in B2 shared-tail clones (the aesni-mb dec8x shape): the
  clone's buffer-interior value lea and the long-form declarations ride the
  same banding every other clone access uses. The transfer's carrier probe
  tests a clone lea's RAW displacement against the clone's own raw declared
  ranges (in the clone's own frame coordinates, where declaration and lea
  key alike); the buffer scan re-checks the decision EXACTLY in banded
  coordinates, and a lea whose uses are all memory-only keeps the carrier
  path (the enc8x shape) — only value-use shapes fall out to the ordinary
  web.
- Absolute alignment identity for computed-base groups (the xor-toggle
  feature): when the program COMPUTES on a buffer-address web (the dec8x
  `xorq $0x80, %base` base flip between the IV and ciphertext halves), the
  computation keys ABSOLUTE address bits, so the output frame must
  reproduce the input buffer's residue mod N. When the declaring statement
  is governed by a provable `and $-N, %rsp` (the governing-and dataflow:
  the most recent and on every path, no rsp restore since), the buffer
  base's residue `res = (d_and - D0 + o_lo) mod N` is exact, the group
  carries it, and the transform pads the buffer region so the lowered
  group's base lands on exactly that residue — with the frame's effective
  alignment raised to N (the output prologue mirrors the input's and), so
  the toggled web value flips between the two halves exactly as the gas
  program's did, and every flipped state stays inside the lowered group
  (a wrong value still traps). Groups without a computed base keep
  today's placement byte for byte (the enc8x carrier shape has no
  computed web and no identity needs).
- Notes: buffers are real per-call frame memory — recursive calls get fresh
  bytes. A rep whose sides are BOTH stack aliases of the same buffer is
  accepted (each side is checked against the buffer's range independently).
  The bounds are on the BYTE range only: alignment guarantees of
  movdqa-class input accesses are preserved by the lowering, and aligned
  accesses wider than 16 bytes are rejected (use the unaligned form).


### Stack buffers in local subroutines (localcall clones)

A `#! stack buffer (id, %rsp + lo, %rsp + hi)` long form on a statement inside
a LOCAL SUBROUTINE (a per-caller-clone localcall callee — the OpenSSL gf2m
`_mul_1x1` shape, whose tab lives in the subroutine's own `sub` frame) declares
the buffer in the SUBROUTINE'S OWN frame coordinates and participates in the
file-wide canonical table from there:

- The subroutine's text registers the canonical range in its own frame
  coordinates (caller-independent), so two callers' clones re-resolve it
  consistently; a same-named long form OUTSIDE the subroutine (owner body or
  another function) at a different range hits the usual "the offsets must
  match" canonical rejection.
- Inside each caller's clone, the declaration resolves IN THE CLONE'S OWN
  CONTEXT with the +8 return-address compensation (`k = D0 - d - 8`, the same
  keying every other clone access uses) — the buffer's bytes live in the
  CLONE'S own sub-frame area: the range must stay below the phantom return
  address word of the hardware call the subroutine was written against
  (`hi <= D0 - dCloneEntry - 8`) and at or above the clone's own allocated
  floor (`lo >= D0 - d - 8`).
- Short-form accesses inside the clone resolve through the clone's own
  declaration (translated into the clone's keyed coordinates), or through a
  declaration in the enclosing function's own body (whose range clone accesses
  key into directly). A buffer declared only in some third function is out of
  reach for the clone's short forms. A caller-declared buffer used only by
  clone statements is fine — each caller's own declaration resolves for its
  own clone — but two callers using one name at DIFFERENT ranges still hit
  the canonical rejection.
- A `rep` string op carrying a `stack buffer` annotation inside a localcall
  clone is rejected (the +8 compensation would need its own rep alias-side
  model). By-address (Feature A) accesses inside clones work exactly like
  ordinary ones — the clone's value `leaq 8(%rsp), %rdi` walking-pointer shape
  (the rsaz MUL shape) seeds a web whose runtime value is the caller's lowered
  buffer byte 0, and the walking stores annotate short-form.


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
  A `leaq K(%alias),%reg2` DERIVES another alias (the perlasm rolling-cursor
  shape — sha1-avx2's X[]+K[] window: `leaq 128(%rsp),%r13` then
  `leaq 256(%r13),%r13` per schedule phase): the derived alias parks
  entry_rsp - (depth - K) and obeys the same rules; a K that would park
  above the entry rsp is unprovable and rejected at the use.
- Frame slots (x86_64 spellings; sp/x29 analogous on arm64) are
  virtualized into register-allocated locals with compile-time bounds —
  the 128-byte SysV red zone is legal. Accesses
  outside the frame, caller-argument-area writes, and taking the frame's
  address are rejected. Aligned vector accesses wider than 16 bytes
  (vmovdqa32/64) need a covering `and $-N,%rsp` note plus an aligned offset
  (the frame is then emitted aligned); otherwise they are compile errors
  suggesting the unaligned form.
- Frame slots CAN hold pointers with capabilities: `store ptr` / `load ptr`
  on a plain 8-byte register<->slot move are accepted, and the capability
  rides pointer flow exactly like a register move (a store seeds the slot
  web from the source's capability, a load hands the slot web's capability
  to the destination, and two origins stored into one slot widen it to a
  dynamic lower). A narrower scalar store to the same offset is a full
  zero-extending def that kills the slot's pointer-ness — a later `load
  ptr` then carries a null capability and traps at the deref. The rest of
  the ptr family (atomics, `load store ptr`) stays rejected on frame
  accesses: a slot is not real memory, so the invisicap sidecar-byte
  protocol has nothing to point at.
- A body that can fall off its end without `ret`, and a branch to the
  function's own entry label, are rejected. `ud2` (and `.byte 0x0f,0x0b`,
  which decodes to `ud2`) is UNCONDITIONALLY TERMINAL: it raises #UD, so a
  control-flow path through it has no successors and a body may end in a
  `ud2` with no trailing `ret` (the same applies to `int3`/`int $3`, whose
  SIGTRAP terminates, and — defensively, since they are rejected anyway —
  `hlt` and `int $N` for `N != 3`). Code after a `ud2` is only reachable via
  an explicit jump to it.

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
