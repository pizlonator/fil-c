# sarcasm design

SAfe Runtime Capability-enforced Assembler: an ARM64 and X86_64 assembler (in Luau, run via
`lute`) that rewrites Yolo-C assembly into memory-safe, Fil-C-linkable assembly, then invokes
`as`. The target architecture — and, for X86_64, the AT&T vs Intel input syntax — is
auto-detected from the input text, or forced with the --x86_64/--arm64/--intel/--at&t
options (see "Command-line target selection" under the driver section).

## Pipeline
```
detect arch/syntax (detect.luau)  ->  parse (arm64_parse.luau / x86_64_parse.luau)
   ->  split into functions (driver)  ->  frame preprocessing (frame.luau + per-arch frame policy)
   ->  lift to IR (register webs; lift.luau)  ->  pointer-flow (ptrflow.luau)
   ->  GIMSO transform (intval/lower, ptr ld/st, calls, pollchecks, SOV; transform.luau
       + per-arch codegen)  ->  IRC regalloc (regalloc.luau)  ->  spill-slot packing (driver)
   ->  emit body + prologue/epilogue (emit.luau + per-arch render)  ->  glue (per-arch glue)
   ->  write .yolo.s  ->  as -o .o
```

The safety-critical logic lives ONCE in the shared modules; each architecture supplies a
backend bundle (`arm64_*` / `x86_64_*`: parse, isa, frame, codegen, render, glue) behind a
common interface, so both ISAs share the lift/pointer-flow/transform/regalloc/emit pipeline.

## Modules

Shared (architecture-independent):
- `detect.luau`     — auto-detects arm64 vs x86_64 (and att vs intel).
- `sig.luau`        — signature encoding (verified against clang: 1066/2529/12769).
- `frame.luau`      — frame preprocessing skeleton; per-arch policy plugs in.
- `lift.luau`       — lifts asm to IR over register webs (see Core stages).
- `ptrflow.luau`    — pointer-flow analysis over the lifted IR.
- `transform.luau`  — the GIMSO transform (see below).
- `regalloc.luau`   — IRC: CFG, liveness, interference, coalesce, spill.
  `regalloc.color()` runs a soundness verifier that rejects any mis-colored
  assignment with a clean compile error (a detection backstop, not a repair).
  Spilling is driven by the driver's `allocateWithSpilling` (re-run IRC after
  rewriting each spilled temp's defs/uses through a fresh reload temp): the
  rewrite covers EVERY node kind that feeds temps to the allocator — insns AND
  the non-insn nodes (the epilogue's myth-temp use, a rootstore's lower-temp
  use — each gets a spill reload inserted in front of it, and the epilogue
  renders its myth register from the node's own use temp). Covering only insns
  would leave a spilled non-insn use in place forever: the next round re-spills
  the same temp and the loop never converges (seen on ecp_nistz256's sqr_mont,
  whose register-saturated localcall clone squeezes the function-wide myth web
  out of the color pool).
- `emit.luau`       — shared emission: reload elimination, spill bookkeeping, self-move
  dropping; per-arch render module supplies instruction text + prologue/epilogue.
- `build.luau`      — constructors for synthesized IR nodes.
- `numlabel.luau`   — GNU-as numeric local labels (`1:` / `1f` / `2b`): resolves
  f/b references and renames definitions to unique private symbols at parse time.
- `sarcasm.luau`    — driver: function splitting, orchestration, spill-slot packing,
  `as` invocation, CLI.
- `ABI-NOTES.md` / `ABI-NOTES-x86.md` — complete ABI references.

Per-architecture backends (`arm64_*` / `x86_64_*` pairs):
- `*_parse.luau`   — operand parser + `;!`/`#!` (x86_64) / `//!` (arm64) annotations
  (see "Annotation markers" in README.md: the recommended spelling is `#!` on x86_64
  and `//!` on arm64, with `;!` supported on both for backwards compatibility).
  x86_64 parses BOTH AT&T and Intel
  syntax into the common dest-first operand model. Both parsers accept GNU-as numeric
  local labels (`1:` may be defined repeatedly; `1f`/`1b` reference the next/previous
  definition, including as a memory displacement like `1f(%rip)`): numlabel.luau renames
  each definition to a unique private symbol and rewrites the references at parse time,
  so numeric labels work in branches and loops like any named label. A reference with no
  definition in the requested direction is rejected (`unresolved numeric label
  reference`). In Intel-syntax mode an AT&T-style parens operand field
  (disp(base,index,scale)) is rejected outright — "AT&T-style operand in
  Intel-syntax input (use [bracket] memory operands)": such a field would
  otherwise fall through to parseSym and RE-RENDER verbatim as
  valid AT&T memory syntax with Intel dest-first operand order applied and NO
  capability/bounds check (`movq (%rdi), %rsi` in
  Intel mode would emit an unchecked 8-byte store). Any field containing parens
  is rejected (covering the parseSym fallthrough, the numeric-prefix immediate
  path, and whitespace-dodged forms); the only legitimate parens in an Intel
  operand are the x87 stack-register name st(N).

  Constant expressions (x86_64): gas folds integer constant expressions at
  assembly time and real-world inputs rely on it (OpenSSL perlasm emits `subq
  $64+32,%rsp`, `movl 512-128(%rcx), %eax`, `movq $1<<4, %rax`,
  `K256+512(%rip)`), so the parser folds them itself (parseDispExpr — a
  tokenizer plus recursive-descent evaluator; no loadstring). Supported:
  decimal/0x-hex literals, + - * / % << >> (integer semantics; / truncates
  toward zero, >> is arithmetic), unary -/+, and parentheses. An immediate
  (`$`-prefixed in AT&T, bare digit-led in Intel) folds to (text, value) with
  the text canonicalized to the folded decimal, so every downstream consumer
  sees the constant; a memory displacement folds into the bare dispSym plus a
  constant dispVal (the renderer re-joins them, so `K256+512(%rip)` round-trips
  and `.Lx+100-52(%rip)` renders as `.Lx+48(%rip)`). A symbol may only stand
  alone or be added to/subtracted from constants — sym*2, sym-sym, -sym are not
  representable as one displacement — and anything malformed (`$64+`, `$foo(2)`)
  is a clean compile error, never the silent 0 that tonumber's nil used to
  produce (which the frame analysis read as a 0-byte stack adjustment).
  Numeric local label references (`1f`/`2b`) pass through as symbols for
  numlabel.luau, and @PLT/@GOTPCREL decorations are split before evaluation.
  Unary bitwise NOT is supported too (`andl $~15, %r10d` — gas's `~x = -x - 1`,
  exact in integer range; aes-gcm-avx512 uses it).

  Encoding-hint pseudo-prefixes (x86_64): a leading `{vex}`/`{vex2}`/`{vex3}`/
  `{evex}`/`{np}` group (OpenSSL perlasm's `{vex} vpmadd52luq ...`, which
  forces a VEX rather than EVEX encoding of an instruction that has both) is
  STRIPPED at parse time — the hint changes only the encoding, never the
  semantics, so the renderer emits the plain spelling and gas picks an
  equivalent encoding. An unrecognized `{...}` prefix stays a parse error.

  .byte instruction decoding (decodeByteInsns): OpenSSL's perlasm emits many
  instructions as raw `.byte` sequences (legacy-gas workarounds) — every
  function ends with `.byte 0xf3,0xc3` (rep ret), CET entry points are `.byte
  243,15,30,250` (endbr64), locked RMWs are a `.byte 0xf0` line joined to the
  spelled instruction, the SHA-NI/AES-NI/movbe/mulx/vmovdqu/sha512-ni bodies
  are raw encodings, and the aesni-xts EVEX forms likewise. decodeByteInsns
  rewrites RUNS of adjacent `.byte` statements into the instructions those
  bytes encode — at parse time, by re-parsing the canonical AT&T spelling
  through parseInsnText, so a decoded instruction is indistinguishable from a
  spelled one downstream. Two matchers compete greedily (longest match wins;
  ties go to the table): a fixed table of whole instructions (plus the 0xf0
  lock join and the no-op filler prefixes 0x66/0x67/0x3e/0x2e/0x90 — 0x64/0x65
  fs/gs are never dropped, they change addressing), and a pattern decoder
  (matchPattern) that walks ModRM/SIB/displacement for the operand-varying
  families: SHA-NI (NP 0F 38 C8-CD, 0F 3A CC ib), AES-NI (66 0F 38 DB-DF, 66
  0F 3A DF ib), movbe ([REX] 0F 38 F0/F1), REX mov ([4x] 8B/89), VEX 3-byte
  (vsha512rnds2/msg1/msg2, mulx, vmovdqu), a narrow EVEX form (vpshrd*-imm),
  and the generic multi-byte NOP (0F 1F /0). RIP-relative and baseless
  absolute forms cannot be expressed as one operand (a .byte run has no
  symbol) and stay data, as does anything else unmatched — the downstream
  "data in a function body" rejection fires exactly as before.

  Annotation markers and comments (both parsers — see README.md's "Annotation
  markers" for the user-facing contract): a line is split into code + annotation
  body at the EARLIEST annotation marker outside a string literal — `;!` on both
  architectures, plus `#!` on x86_64 and `//!` on arm64 (the recommended
  spellings). Every string-aware scan honors `\"` escapes: inside a string
  literal the character after a `\` is skipped, so `"a \" ;! b"` is one
  ordinary string whose `;!` is inert (and, on arm64, a `//` inside such a
  string is not a comment); outside strings `\` is an ordinary character with
  no quoting power. The interplay with comments is per-architecture, and in
  both directions the invariant is that comment and string-literal text can
  never fabricate an annotation:
  - x86_64: a single string-aware scan (splitAnnotation) walks the line; a `#`
    NOT immediately followed by `!` starts a comment that ends the line with NO
    annotation (so `# use ;! load ptr here` stays inert), `#!` or `;!` splits the
    line, and the body is then run through the ordinary comment stripper
    (stripComment), so a trailing `#` comment inside the body is removed.
    `//!` is NOT a marker here: it stays in the code part
    and fails to parse ("cannot parse line: //! ...").
  - arm64: comments are removed from the WHOLE text before line splitting
    (stripComments: `//` to EOL, inline and multi-line `/* ... */`, both
    string-literal aware, each replaced by a single space so token separation is
    preserved, with newlines kept so line numbers still line up). `//!` is
    emitted VERBATIM by that pass — it is a marker, not a comment — so the
    subsequent marker split finds it, while a later `//` on the same line is
    still stripped (arm64 bodies are verbatim: no further comment stripping
    happens inside a body). Block comments win: `/* //! load ptr */` is a
    comment, not an annotation. `#` is NOT a comment character here (it
    introduces immediates), and `#!` is NOT a marker, so `#!` text stays in
    the code part and produces an ordinary parse error.
- `*_isa.luau`     — instruction semantics: register defs/uses, control flow. On
  x86_64 this is also where the implicit-register GPR instructions are MODELED:
  div/idiv (implicit rdx:rax dividend use, quotient/remainder def), mul and
  1-operand imul (rax use, rdx:rax product def), mulx (implicit rdx source,
  not written), cmpxchg (implicit accumulator RMW alongside the explicit
  destination RMW), cmpxchg8b/cmpxchg16b (the implicit expected-value RMW
  pair edx:eax resp. rdx:rax and the implicit new-value source ecx:ebx resp.
  rcx:rbx — all four pinned), the cqo/cdq/cwd sign-extends (rax use, rdx def),
  and the zero-explicit-operand implicit-only family (cpuid, rdtsc/rdtscp,
  xgetbv, rdpkru, wrpkru, monitor, mwait) — all name their implicit rax/rdx
  effects with implicit def/use operand tables the web
  analysis connects to the surrounding code; codegen's emitPinned then pins
  them to the physical registers with synthesized moves AROUND the instruction
  (the reaching webs of the implicit-use registers are copied into physical
  rax/rdx before it, the result webs copied back out into fresh webs after it
  — regalloc coalescing makes the copies free when the user already wrote
  rax/rdx), and the emitted instruction shows only its explicit operands (the
  implicit-only family renders as the bare mnemonic). monitor's implicit rax
  address is deliberately NOT modeled as a memory operand: MONITOR does not
  access memory, so there is nothing to bounds-check — a wild address can
  only fault, an uncatchable-signal safe halt the safety model accepts. An
  explicit operand naming a pinned register renders as the fixed physical
  register, keeping it in sync with the pin. cbw/cwde/cltq need no pinning:
  codegen rewrites them to a renameable movsxx on the rax web. xchg is modeled
  with the same pin machinery: BOTH register operands are read and written (each
  receives the other's value), so classify creates a fresh def-site table per
  register (the def of rax's web must receive rcx's value — a crossing the
  plain web/RMW model cannot express), emitPinned
  copies both webs into their physical registers, the passthrough xchg swaps the
  physical registers, and each physical register is copied back out into its
  register's fresh web; xchg with a MEMORY operand is an implicitly LOCKED
  read-modify-write in hardware and is rejected ("implicitly locked
  read-modify-write"), as is any non-GPR operand pair (rsp/xmm exchanges). bswap
  is a use+def RMW of its ONE register web passed through raw — and only in
  its r32/r64 forms:
  the 16/8-bit spellings are not encodable (an 8-bit byte swap would be a
  no-op; gas: "invalid instruction suffix for `bswap'"), so `bswapw %ax` and
  `bswapb %al` are rejected at classify ("bswap requires a 32-bit or 64-bit
  register operand (bswapw/bswapb are not encodable)") instead of dying in the
  temp .yolo.s; movbe is MODELED like an ordinary mov load/store — the
  byte-swapping load (`movbeq (%rsi), %rax`: def of the destination, the
  memory operand on the normal bounds-checked path) and store (`movbeq %rax,
  (%rsi)`: use of the source) pass through verbatim; the swap is a
  register-value transformation the web model need not see (like bswap). The
  register-to-register spelling (not a baseline x86-64 encoding) and movbeb
  (an 8-bit byte swap is a plain move; not encodable) are rejected cleanly.
  shld/shrd (double-precision shifts) are modeled exactly: the destination is
  a true RMW (the unknown-mnemonic fallback used to model it as a pure def,
  silently dropping the destination's contribution to the result), the source
  is a plain use, and the count is an immediate or %cl. A register-count
  shift/rotate (`shlq %cl, %rdx` & co, incl. shld/shrd with a %cl count and
  rcl/rcr, now in ALU2) pins the count web to physical rcx via emitPinned —
  %cl is the only register-count encoding in the ISA, and left renameable the
  allocator colored it anywhere (the emitted `shlq %sil, %rdx` failed to
  assemble — the aes-cfb-avx512 miscompile). A spelled non-%cl count register
  is rejected at classify. Multi-byte padding NOPs (nop/endbr64/nopw/nopl)
  are a zero-effect
  class: baseMnemonic maps them all to "nop" (no def/use effects), the frame
  rewrite passes them through verbatim (their operand — `nopw 0x0(%rax,%rax,1)`
  — is a dummy encoding hint, NOT a memory access, so it must not be
  bounds-checked or virtualized), codegen's lowerSpecial skips the access check
  for them, and the renderer normalizes the bare forms (which gas rejects with
  "invalid instruction suffix") to plain `nop`. The cache-line management
  family clflush/clflushopt/clwb is memory-neutral like prefetch (the fp
  table's noCheck): they move no data to or from the processor — they
  invalidate/write-back the addressed line in the cache hierarchy, which
  changes nothing the checker models — so a wild address can only fault, an
  uncatchable-signal safe halt the safety model accepts (the same reasoning as
  monitor; unlike clzero, which WRITES zeros to memory at rax and stays
  rejected). On arm64 this
  is where the NEON/FP semantics live: per-mnemonic def/use classification
  (the VEC_PURE_WRITE / VEC_DST_READ / lane-insert families, fmov's
  GPR<->vector roles, the safe unknown-mnemonic default — read all NEON
  operands, provide nothing), vecEffects for the width-liveness vector
  saves, the NZCV flag-use/flag-def tables (adcs/sbcs are load-bearing
  read+write), the SVE/SVE2 rejection, and the LSE_KIND table classifying
  the LSE RMW / cas / casp / load-store-exclusive families — including the
  cas/casp pin modeling (the compare register — and casp's data pair — are
  architectural read-modify-writes pinned to their written physical
  registers, the arm64 counterpart of x86_64's cmpxchg accumulator; the
  hardware encodes a pair as one even register number).
- `x86_64_fp.luau` — the x86_64 FP/SIMD knowledge module, consulted by the isa
  classifier (def/use modeling + rejections), the codegen (checked-access
  width/alignment), and the frame policy (stack-slot widths). Its substance is
  a set of width RULES over the SSE/AVX/AVX2/AVX512 surface, not a flat
  per-mnemonic table:
  * Per-family memory access widths/alignments cover the scalar and packed FP
    forms, vector moves, MMX, x87, AES/SHA/PCLMUL/GFNI crypto, opmask k moves,
    and fxsave/fxrstor (=512), plus the exact-width GPR classes (sgdt/sidt=10,
    sldt/str/smsw=2, lss/lfs/lgs = destination size + 2-byte selector;
    movnti = the source register's width, never a guessed default;
    lzcnt/tzcnt/popcnt and BMI/bsf/bsr memory sources = the destination GPR's
    byte width; xadd RMWs BOTH explicit operands; adcx/adox = the destination
    width; crc32 = the crc SOURCE width from the b/w/l/q suffix, source
    register, or PTR annotation, with an illegal destination/source width
    combination rejected cleanly).
  * Derived widths wherever a rule exists instead of entries: an embedded
    broadcast `{1toN}` reads one element; vpbroadcast memory sources read the
    exact element width 1/2/4/8; truncating/saturating vpmov stores read the
    source vector bytes / element ratio; vcvtps2ph/vcvtph2ps read half width
    on the ph side; widening converts read half or quarter the destination
    width. The full-width classes (vpermi2*/vpermt2*, unmasked
    expand/compress, VNNI dot products, vdbpsadbw, IFMA multiply-adds,
    vcvtpd2uqq) ARE exact entries, because a suffix rule would mis-pin them.
  * The ss/sd/ps/pd scalar-width suffix rule EXCLUDES p-stem mnemonics: after
    the optional 'v', a stem starting with 'p' is the packed-INTEGER family,
    where a trailing "sd"/"ss"/"ps"/"pd" is part of the NAME — so the whole
    "sd"-ending packed class falls through to the p-family pattern at FULL
    vector width instead of being under-checked at 4/8 bytes, closing that
    under-check class generically (scalar FMA keeps its width:
    vfmadd231sd's stem is "fmadd231" -> 8).
  * The packed AVX512-FP16 suffix ph resolves to full vector width (like
    ps/pd); the SCALAR sh forms are deliberately NOT matched — a rule-derived
    width would be wrong for the GPR-mixing scalar-half converts
    (vcvtusi2sh's integer source is m32/m64) and would leave vcvtsh2si's GPR
    destination unmodeled — so every sh form stays unknown and a
    memory-operand one rejects cleanly.
  * NARROWING converts (vcvtpd2ps & siblings) are SIZE-DRIVEN, because the
    destination register class does not determine the memory source width:
    the width comes from the Intel PTR annotation or the AT&T x/y(/z) source
    suffix; a bare unsized memory form is accepted only when the destination
    class is unambiguous (vcvtpd2ps (%rdi), %ymm0 ⇒ m512) and is otherwise
    rejected cleanly ("unsized memory operand is ambiguous; use a size suffix
    or PTR annotation") before anything gas would fail on is rendered; an
    illegal PTR size or suffix/destination combination rejects cleanly too;
    the renderer synthesizes the source-width suffix for a bare mnemonic with
    a sized memory operand.
  * GPR def/use roles of mixed GPR/vector instructions: the vpbroadcast
    GPR-source forms READ the source GPR; the vcvtusi2ss/sd GPR source and
    reverse vcvtt*/vcvt*2usi GPR destination are modeled.
  * The authoritative-width rule: the resolved ISA-fixed width never defers
    to an Intel PTR size annotation — a contradicting annotation REJECTS, on
    the heap and stack paths alike; only the size-driven widths (x87 PTR
    fallback, cvtsi2ss/sd integer source, narrowing converts) take their
    width from the annotation.
  * Masked-access metadata (fp.maskedAccessInfo): the supported {k}-masked
    vector moves, truncating stores, expand/compress forms, {k}-masked ALU
    memory SOURCES (AVX-512 fault suppression reads the memory operand only
    for lanes whose writemask bit is set, at the operation's natural element
    granularity — an EVEX integer p-family or packed-FP mnemonic with a
    trailing element letter b/w/d/q/ps/pd, e.g. rsaz-avx512's
    `vpsubq .Lmask52x4(%rip),%ymm3,%ymm3{%k1}`) and the AVX2
    vmaskmov/vpmaskmov forms get (element, lanes, vecsize, contiguous)
    metadata for the transform's mask-aware check; masked forms of anything
    else (incl. masked ALU shapes whose element is undeterminable), {%k0},
    and {k}-masked stack accesses stay rejected.
  * The unsafe-instruction reject list (each entry's reasoning lives under
    Limitations): privileged/system instructions, string ops and non-lock
    prefixes, gather/scatter, maskmovdqu, xsave/x87-state saves, implicit-GPR
    families (pcmpestri/pcmpistri, umwait/tpause), umonitor, AMX tiles, far
    control transfers, FSGSBASE/swapgs, TSX, CET notrack/setssbsy, SGX
    encls/enclu/enclv, skinit.

  Segment-register WRITES are rejected (the parser classifies
  %es/%cs/%ss/%ds/%fs/%gs as their own operand class, rc="seg", so the
  destination is visible; a bad selector raises an unconverted hardware
  fault). PUSH/POP of a segment register is rejected with its OWN message
  ("push/pop of a segment register is not supported (it transfers the segment
  selector, which the checker does not model)") — a push only READS the
  selector and a pop's selector load rides the stack push/pop machinery,
  while genuine selector writes keep the write-specific message. Plain
  selector READS (`movl %fs, %eax`) pass through: the destination web is
  defined with the selector's value, which is sound (the emitted instruction
  writes the web's allocated register); segment-QUALIFIED memory operands
  (`%fs:0x28`, Intel `fs:[0x28]`) still parse as symbolic displacements and
  keep their rejections.
- `*_frame.luau`   — frame policy: drop the input's frame setup/teardown, virtualize
  stack-pointer/frame-pointer-relative slots, reject stack-address escapes. (arm64
  also virtualizes NEON/FP stack slots into the raw-byte GPR slot webs — including
  the AAPCS callee-saved d8-d15 writeback save/restore forms — and derives the
  frame geometry that normalizes x29-relative offsets into the
  sp-relative coordinate space; see the arm64 NEON subsection of the frame
  section.)
- `*_codegen.luau` — per-arch instruction emitters (neutral micro-ops) for the transform.
  (arm64 includes the checked-access width/alignment model: NEON
  single-/multi-structure sizing with arrangement validation, pair forms, the
  LSE/LLSC/exclusive atomics at natural alignment, and the liveness/width-aware
  vector save/restore expansion around injected runtime calls.)
- `*_render.luau`  — render IR back to assembly with colors; synthesize the Fil-C
  prologue/epilogue/frame layout. (x86_64 always emits AT&T output.)
  x86_64_render additionally emits instructions the SYSTEM ASSEMBLER may not
  know as `.byte` directives (BYTE_EMIT): baselines as old as binutils 2.38
  (Ubuntu 22.04) do not know SHA512-NI (vsha512rnds2/vsha512msg1/vsha512msg2)
  or SM3/SM4 (vsm3rnds2/vsm3msg1/vsm3msg2/vsm4key4/vsm4rnds4), both of which
  OpenSSL perlasm uses. Their VEX 3-byte encodings are emitted byte-for-byte
  (deterministic — vector registers are never reallocated, and a memory
  source's base/index resolve through opTemp like any rendered operand;
  verified against the binutils 2.42 opcode table and disassembler), with the
  spelled instruction riding along as a comment. vsha512msg1/msg2 are the
  two-operand forms (vvvv encoded as ~0; msg2's source is ymm/m256, msg1's
  xmm/m128); vsm3msg1 is the NP form, vsm3msg2 66, vsm4key4 F3, vsm4rnds4 F2,
  all at 0F38 DA. The operand-shape validation rejects the vector classes the
  encoding cannot name (zmm / 16-31 registers) with a clean sarcasm error.
- `*_glue.luau`    — getter/FO/2ET/origins/access-origin/alias link & run, plus the weak
  callsite resolver thunk for called externals. (x86_64: the 2ET generic-entrypoint
  thunk skips the actual-vs-expected argument-size check for zero-argument signatures —
  `actual <= argBytes - 1` would be `actual <= -1`, i.e. unsigned-always — matching
  clang's thunk, which has no check there; the thunk is a weak symbol that must remain
  link-compatible with clang's.)

NOTE (x86_64 vs arm64 CC packing): BOTH fast-CC packings are DENSE — a scalar arg
consumes one register slot, a pointer arg two consecutive slots. x86_64 packs from rdx
across rdx,rcx,r8,r9 and passes further words on the stack as densely packed 8-byte
slots in declaration word order (word w >= 4 at `8*(w-4)(%rsp)` in the caller's
outgoing-args area), which sarcasm marshals in BOTH directions (see the entry-unpack
and callsite-marshalling notes); arm64 packs from x2 across x2..x7 (6 words max) and
keeps rejecting wider signatures —
e.g. `long(long,long,ptr)` puts arg1 in x3 and arg2's intval/lower in x4/x5.

NOTE (historical alloca regions): the `;! alloca`-driven GC-allocation regions
(fixed-size `alloca result size=N` regions and dynamic `alloca size/result` regions,
with the `regionRedirect` address-math redirection and the arm64/x86_64
alloca-base normalization) were removed with the annotations. The only region
source left is the fixed-frame escape promotion (D9), which rides the same
`regionOf`/`regionRedirect` machinery. Stack allocation is spelled with the
`.alloca` directive, which never touches sp and needs no regions.
An indexed region lea (`lea D(%rsp,%idx,s),%r` with the static displacement
in the region) redirects exactly like its non-indexed form: the value rides
the region pointer with the index preserved, and the result shares the
region's capability — the single-step spelling of the sanctioned two-step
idiom (`lea D(%rsp),%t` seeding the pointer, then `lea (%t,%idx,s),%r`),
with identical behavior. The dynamic index is covered by the runtime bounds
checks at every use (fail-closed: an out-of-region index traps there).

## Core stages

### lift.luau — asm -> IR over register "webs"
Faithful reallocation of the whole GPR file requires turning physical-register live
ranges into virtual temps. Approach (no full SSA needed):
1. Build CFG (reuse regalloc.buildCFG shape).
2. Reaching-definitions dataflow per physical GPR.
3. Union-find "webs": union each use with every def of the same reg that reaches it.
   Entry pseudo-defs for CC input regs (ARM64 example: x0=myth, x2/x3=arg0 iv/lower,
   x4/x5=arg1, ...). Each web = one temp id.
4. Emit copies at CC boundaries so precolored physical webs stay tiny:
   - entry: `vMyth = x0`, `vArg0iv = x2`, `vArg0lo = x3`, ...
   - call:  move args into the arg registers (precolored), call, move results out to
     virtuals.
   - return: move the ret virtual into the result register (+ lower if ptr), flags in
     the flags result.
5. Each IR insn keeps its mnemonic + operands, with reg operands replaced by
   `{temp=id, width=..}` refs so the emitter can substitute the assigned color.
   Spill-slot ld/st (within the input frame) become copies to/from a spill-slot temp
   ("spill slots as locals").

### frame.luau — prologue/epilogue recognition
Matches the canonical prologue (ARM64: `stp x29,x30,[sp,#-N]!` or `sub sp,sp,#N` + `stp`
saves + `mov x29,sp`; X86_64: `push %rbp`/`mov %rsp,%rbp`/`sub %rsp` + callee-saved
pushes, `leave`, `endbr64`) and the symmetric epilogue. Records N, the callee-saved set,
and the spill-slot region. On X86_64, rbp-relative displacements are normalized to
rsp-relative frame offsets via frameSize when rbp is the frame pointer (rbp = rsp +
frameSize), and slots are KEYED by that normalized offset — so `-8(%rbp)` and `8(%rsp)`
(addressing the same byte with a 16-byte frame) share one slot web, while `-8(%rbp)`
and `-8(%rsp)` (different bytes) stay distinct. Distinct-width slot accesses whose
real-memory bytes overlap (a 4-byte store at rbp-52 over an 8-byte value at rbp-56)
are modeled as independent webs — the aliasing is not modeled (compilers do not
emit overlapping slot traffic). Any remaining stack access outside the analyzed
frame [0, frameSize) is rejected (on X86_64 the range is extended down by the
128-byte SysV red zone for rsp-relative and normalized rbp-relative offsets alike —
with rbp = rsp + frameSize, an rbp-relative d in [-frameSize-128, -frameSize), e.g. a
leaf function's `-8(%rbp)` with no `subq` at all, IS the red zone), as is taking the
address of the stack frame at all
(e.g. `add xD, sp, #k` / `leaq 8(%rsp), %rax` / `movq %rsp, %rax` — safety cannot be
proven).
sarcasm SYNTHESIZES its own frame regardless (for the SOV check + filc_frame push +
callee-saved for the new allocation), discarding the input's frame ops.

#### x86_64: fast-CC stack argument words and SysV stack arguments

The x86_64 fast CC packs the first FOUR GPR argument words into rdx,rcx,r8,r9 (a
pointer arg is two words: intval + lower, and one can straddle the r9/stack
boundary); argument words beyond the fourth travel on the stack as densely packed
8-byte slots in declaration word order — word w (0-based, w >= 4) at `8*(w-4)(%rsp)`
in the caller's outgoing-args area (verified against pizlonated clang callers,
callees and 2ET thunks for 5..14-word signatures; the outgoing area is 16-byte
aligned at the call, and clang reads incoming word pairs with movaps). sarcasm
marshals those words in BOTH directions, so signatures have no GPR arity limit
(FP args stay bounded at 8: xmm0..xmm7):

- **Entry unpack (transform).** Arg k's dense word index s (FP args consume no
  dense words) selects the source: s < 4 — the dense CC register, copied into the
  arg's yolo SysV register web exactly as before; s >= 4 — an INCOMING stack word
  at `[entry_rsp + 8 + 8*(s-4)]` (above the return address, read-only), loaded by
  `cg.loadInArg`. Because the load renders after the prologue, its displacement is
  computed at RENDER time from the final layout (`layout.total + 8*#calleeSaved +
  8 + 8*(s-4)`; the node carries `inArgWord`, emit.luau hands renderInsn the rc).
  This is the double mapping the yolo ABI needs: e.g. `void(ptr,ptr,size_t)`
  delivers p1.iv=rdx, p1.lo=rcx, p2.iv=r8, p2.lo=r9, size=`[rsp+8]`, while the
  body's SysV placement wants p1=%rdi, p2=%rsi, size=%rdx — so %rdx is loaded
  from `[rsp+8]`.

- **SysV stack arguments at entry (x86_64_frame).** The 7th and later
  integer-class yolo arguments arrive at `8+8i(%rsp)` at entry — ABOVE the
  caller's frame, which the bounds check rejects. Their entry-time READS are
  redirected: a direct `%rsp`-based read at a provable rsp depth d (address
  `entry_rsp - d + disp == entry_rsp + 8 + 8i` — the prologue shapes
  `movq 8(%rsp),%reg` / `movd 8(%rsp),%xmm5`, and compiler-style reads above an
  established frame), or a read THROUGH a register that parked the entry rsp
  (`movq %rsp,%reg` / `leaq 0(%rsp),%reg` — the aesni_ocb/bn_mul_mont_gather5
  shape), which the prologue rsp-save map (smap) now records for ANY register
  when the signature has stack arguments (a caller-saved save still cannot
  recover %rsp, and any non-argument use of the aliased register stays
  rejected). GPR-class reads are rewritten to a VIRTUAL argument-slot web (a
  dedicated pseudo-register band), which the entry unpack seeds from the same
  incoming fast-CC word — including the capability lower for a pointer argument,
  so pointer flow propagates exactly like a register argument. FP/vector reads
  (they cannot name a GPR pseudo-register) instead MATERIALIZE the incoming
  words into a reserved frame area (yoloBytes in computeLayout, filled by the
  entry unpack, 16-aligned so movaps-class pair reads keep their alignment);
  analyzeFrame reserves it exactly when an FP read exists. Stores to the
  incoming argument area stay rejected, as are reads of the return-address word
  or beyond the declared arguments.

- **Callsite marshalling (transform).** Mirror image: dense words 0..3 move
  into the CC registers; words 4+ are stored into the outgoing-args area at
  `8*j(%rsp)` before the call — exactly clang's placement. The stores execute
  inside an outgoing stack-arg window (`cg.beginStackArgs(N)`/`endStackArgs(N)`,
  N a multiple of 16): rsp is dropped over the area BEFORE the stores and
  restored right after the call, BEFORE the exception-flag test (the flag rides
  %al, which add/sub do not touch), so both the normal and the exception path
  see a balanced stack, and the GC sees the same filc_frame throughout (the
  frame pointer at myth+16 never moves). Any spill reloads regalloc inserts for
  the stores' sources land INSIDE the window, so rewriteSpills stamps them with
  the shift (node.spillShift, kept unshifted in spillOff for slot packing and
  reload elimination) and renderInsn re-bases them. A 7th+ integer argument's
  VALUE comes from the body's own outgoing-args area — an ordinary frame slot
  (the compiler-style placement: outgoing argument o at `8*o(%rsp)`), connected
  to the call by reaching-definitions. A stack-resident pointer LOWER word is
  re-read from the argument's GC root slot (a fixed frame offset, 16 + 8*index,
  always current — every lower is rooted where its web is defined) instead of
  from the lower web: lower webs are no-spill, so keeping one live per pointer
  argument up to the call would run the allocator out of registers at roughly
  ten pointer arguments.

- **Glue (x86_64_glue).** The 2ET generic-entry thunk loads words 4+ from the
  buffer into the outgoing area through the same sub/add window (hand-written,
  so no spill concerns). The callsite resolver thunk stashes only the
  register-resident words in its hold pool (at most 4 — the pool's size):
  on the fast path the incoming stack words flow through to the callee IN PLACE
  (same signature, same placement, and the getter call touches no stack above
  its return address); on the generic buffer-CC path they are read back from
  the incoming stack area above the thunk's pushed saves.

#### x86_64: no mid-function stack-pointer movement
Frame geometry is sound ONLY if every stack access executes with rsp at one known
value, so on x86_64 the frame size is derived from the PROLOGUE PREFIX ONLY: the
leading run of frame-setup instructions (callee-saved pushes/pops, `movq %rsp,%rbp`,
constant `subq $imm,%rsp` allocations and net-balanced `addq $imm,%rsp` undos),
interleaved with instructions that cannot observe or modify the stack state; the
prefix ends at the first stack access, control-flow instruction, jump-target label,
or rsp/rbp-frame clobber. frameSize is the NET rsp depth at the end of the prefix —
relative to the frame pointer when one is established (so rbp = rsp + frameSize holds
for the post-prologue rsp) — hence a transient `subq $8,%rsp; addq $8,%rsp` pad or
`pushq %rbx; ...; popq %rbx` pair inside the prefix nets out and cannot inflate
frameSize (counting such pairs once, as a whole-body scan does, desyncs the rbp<->rsp
normalization AND the out-of-frame bounds check — a silent miscompile).

After the prologue, rsp must not move in ways sarcasm cannot prove safe — there is NO
mid-function stack-pointer movement: a control-flow rsp-depth analysis proves the
abstract depth (bytes below the entry rsp, or "unknown") at which every instruction
executes, and the frame rewrite rejects, with a clean `sarcasm: <file>: <msg>` error:
- constant (`subq`/`addq $imm`) or non-constant rsp adjustments that are not epilogue
  teardown — teardown being an `addq $imm,%rsp` / `movq %rbp,%rsp` leading to `ret`
  through callee-saved pops and ordinary, non-stack computation only. A teardown-verified
  `addq $imm,%rsp` (and the `ret` terminating its span) is accepted even at an UNKNOWN
  depth: the constant cannot be PROVEN to undo a dynamic alloca, but every statement in
  the span is dropped and the synthesized epilogue owns the real %rsp, so the output is
  correct either way. Also accepted (dropped): a constant mid-function rsp DECREASE
  (`addq $imm,%rsp`) at a statically known, non-negative resulting depth whose clobbered
  flags are provably dead (nothing reads them before a full flag write on any path) —
  the ecp_nistz256_point_add `addq $416,%rsp` frame reshape ahead of a shared-tail
  join. A `leave` is
  legal ONLY as epilogue teardown (the teardown proof must reach the `ret`): a
  mid-function `leave` is always a compile error ("mid-function `leave` cannot be
  proven safe (a leave that does not lead directly to a ret may observe the caller's
  frame)" — hardware `leave` also restores the CALLER's frame pointer into %rbp), and
  a `leave` with no frame pointer established is rejected with its own error. Dynamic
  allocation must use the `.alloca` directive, not raw `subq %rax, %rsp`;
- stack accesses executed while rsp is of UNKNOWN depth, and returns (or indirect
  tail jumps) with an unbalanced or unknown stack pointer. An access at a
  STATICALLY KNOWN perturbed depth keys exactly in normalized coordinates
  (disp + D0 - d) and virtualizes like any other slot — the same-address key is
  depth-invariant, the bounds check gates the frame extent, save-slot aliases
  ride the save-slot model (the early-ret-then-frame shape — an early return
  before the pushes leaves the framed path's pushes POST-prologue, as in rc4's
  epilogue mov-reloads — the slot sits at displacement (depth - save.depth)
  exactly like the transient prologue pad);
- mid-function pushes/pops of non-callee-saved operands, and pops of the frame pointer
  that are neither inside verified teardown nor provably paired with the outstanding
  save of rbp (a paired `popq %rbp` — e.g. the leaf `popq %rbp; ret` needing no frame
  cleanup — is a sound epilogue restore; the depth analysis rejects any stack access
  or unbalanced return after it);
- establishing the frame pointer (`movq %rsp,%rbp`) outside the prologue;
- `enter`/`enterq` — packed push-rbp/`mov %rsp,%rbp`/`sub $imm,%rsp` frame setup that
  sarcasm does not model; passed through it would shift rsp and clobber rbp inside the
  synthesized Fil-C frame (frame setup must use the canonical three-instruction form);
- any other `and` on the stack pointer (only `and $-N, %rsp` for a power of two
  N >= 16 is accepted, in the prologue or — with provably dead flags — mid-function:
  a vector-alignment rounding, dropped with the alignment recorded for the
  aligned-access model above). A direct rsp-relative slot access positioned BEFORE
  a mid-function `and` keys pre-and numbering and is rejected (carrier/rbp values
  are absolute and stay legal);
- vector-INDEXED memory operands (gather/scatter: an xmm-class register as the
  memory base or index) — they touch multiple discrete addresses that cannot be
  bounds-checked. FP/SIMD registers as VALUES are in scope: instructions naming
  them pass through the frame policy, and their stack-frame accesses are
  materialized rather than virtualized (see the FP/SIMD subsection below).

Balanced callee-saved push/pop save/restore pairs ARE permitted anywhere the depth
analysis stays consistent (e.g. gcc's shrink-wrapped saves behind a conditional
branch): they are dropped — sarcasm's synthesized frame preserves the callee-saved
registers it actually uses — sound exactly because the depth analysis rejects every
registers it actually uses — sound exactly because the depth analysis rejects every
stack access and return that could observe the shifted rsp. (Historically, the
`alloca`-annotated dynamic `subq %rax, %rsp` made the depth "unknown" with the
same save-stack survival; the annotations are gone — dynamic stack allocation is
spelled with the `.alloca` directive, which never perturbs the depth at all.) A
constant `addq $imm,%rsp` in an unknown-depth scope keeps the save stack too (it
pops nothing);
whether it is a legal teardown is decided by the straight-to-`ret` proof above.

Dropping a callee-saved pop is sound ONLY when the pop provably restores a matching
dropped push: alongside the depth, the analysis tracks an abstract save stack (the
outstanding pushes — register + slot depth — in push order; merges require agreement
on every path), and a pop is rejected when no outstanding push of the SAME register at
the SAME slot depth is on top (mid-function or inside a teardown region alike). At an
UNKNOWN depth the slot depth cannot be checked; there a pop pairs off the outstanding
save stack by register alone, and ONLY inside a verified teardown — everything in such
a span is dropped and the synthesized epilogue owns the real %rsp, so only the
register identity matters; a redefined save still rejects an observable lost reload.
An
unpaired pop's load would otherwise be silently lost, and the slot it actually reads
is not that register's save — an unpaired `popq %rbx` after `pushq %rbp` reads the
saved frame pointer (a live stack address) into rbx. The same pairing rule nets out
transient save pairs inside the prologue prefix, so such a pop cannot masquerade
as balancing the frame-pointer save. Epilogue restores of prologue saves, balanced
shrink-wrapped pairs, and verified-teardown pops all pair exactly and keep being
dropped.

While a prologue-pad push is OUTSTANDING (e.g. `pushq %rbx` before the first frame
touch, popped only after), the bytes it saved are NOT an ordinary frame slot: their
content is mirrored by the pushed register's web, because the matching pop — proven to
restore exactly this register — is dropped. The rewrite therefore maps every stack
access at displacement (depth - save.depth) — 0 = the most recent push, 8 = the one
before it (the FIRST push sits at the HIGHEST address) — onto that register's web
instead of a slot web: a full 8-byte GPR store defines the register's web (the dropped
pop then naturally yields the stored value, matching real x86), a read-modify-write
operates on that web, and a load of any width at the slot base reads it (before any
store that is the pushed value). Anything else that overlaps a save slot is rejected:
a partial-width store (the web model has no subregister view, while the remaining slot
bytes keep the value the dropped pop restores), a vector/x87 access (it cannot name a
GPR web), an instruction whose register form is not exactly modeled (the rewritten
instruction is re-classified; the conservative first-reg-def fallback would desync
slot and register), and any access while the save state is unprovable ("dyn" — the
paths disagree on what is pushed, so no provable mapping exists). Virtualizing such an
access into an unrelated slot web would silently miscompile instead: the store would
never reach the register and the dropped pop would resurrect the stale pre-push
value, diverging from real x86, where the pop overwrites the register with the
stored value.
The saved FRAME POINTER (a `pushq %rbp` establishing the frame) is not a value slot
and never aliases here — it sits above the frame, where the bounds check already
rejects accesses.

ACCEPTANCE GAP (conservative, deliberate — soundness first): when a pad-pop's reload
is unmodeled (a `redefined` save whose pop is dropped but whose lost reload is judged
observable — see the checkLostReload walk in x86_64_frame), the analysis rejects
instead of stopping the walk at a re-push of the same save. Stopping there would be
UNSOUND: at the next iteration's re-push, the register's web does not hold the
hardware-faithful value (hardware restored the caller's value via the pop, while the
model's web still holds the body's last value), so the re-push would write that wrong
value into the new save slot — and a later slot access, or a use before the register
is redefined after the re-push, would silently observe the stale web value instead of
the hardware value. A standard save/restore around a loop-body load
(`pushq %rbx; movq (%rdi),%rbx; ...; popq %rbx; loop`) is therefore conservatively
rejected because the lost-reload walk crosses the back edge; restructure by not
saving/restoring the same register inside the loop — let sarcasm's own callee-saved
handling preserve it across the loop.

A `movq %rsp, %reg` SAVE (prologue or post-prologue, wherever the rsp depth is
provable) is transparent frame setup: the save is
DROPPED — sarcasm synthesizes its own frame, so the parked value has no meaning in
the output (it is PHANTOM) — and a RECOVERY from it is dropped with it, reviving
the saved depth (the save rides the depth analysis unchanged through a dynamic
alloca). The saved value flows through CARRIERS: a register (`movq %rsp, %reg`,
or the recovery-address lea `leaq d(%rsp), %reg` at depth d — the ghash/aesni-gcm
shape) copied reg→reg (`movq %carrier, %reg2`), or parked in a frame slot
(`movq %carrier, off(%rsp)` / `off(%rbp)` — a save-store, keyed by slot offset in
the access's own coordinates), and re-materialized by a full-width slot load.
A recovery — `movq %reg, %rsp` OR `leaq K(%reg), %rsp` (the perlasm
`leaq (%rsi), %rsp`) — may read any carrier that provably holds the save value
and revives the parked depth (minus K). Register class is unrestricted: the
carrier discipline makes every use either dropped or rejected, so a caller-saved
register (or a caller-saved save riding through a region slot — the
sha256/sha512/wp idiom) is exactly as sound as a callee-saved one — no call
clobber can ever be observed. Any OTHER read of a register carrier, or a
redefinition of one, poisons it (the rejection names the register); a store
overlapping a slot carrier poisons the slot; a non-carrier-form LOAD from a live
slot carrier is rejected outright (it would observe the phantom value); and a
`ret` with a live carrier in the integer return register is rejected (the stack
address would escape as the return value). Epilogue restore loads THROUGH a
carrier (`movq -48(%rsi), %r15` — the perlasm movq-restore riding the recovered
pointer) are recognized like the epilogue movq-restore loads and dropped.
A stack access THROUGH a carrier register (`mov %rsp, %reg` / `lea K(%rsp),
%reg`, then `disp(%reg)` — the rsaz-avx2 gather shape) keys the normalized
frame offset disp + D0 - parked-depth (- 8 inside a clone, the +8 rule) and
virtualizes/materializes exactly like an rsp-relative access: the parked value
is absolute (entry_rsp - depth on every path), so the slot is static. Control
flow that leaves the register holding stack+offset on one path and a
heap/alloca/argument pointer (or nothing) on another is a static error at the
access (an explicit conflict mark — ordinary runtime-checked uses stay legal:
a heap-form access carrying an explicit pointer capability (`;! load/store
ptr` and the atomic variants) through a conflicted base passes through and is
bounds-checked at runtime against that capability, fail-closed — a
stack+offset value on some path has no valid lower, so that path traps rather
than accessing out of bounds; every other memory form through a conflicted
base, including plain unannotated heap loads/stores/RMWs (which would need a
static stack-or-heap decision the frame pass cannot make) and lea
address-taking, stays a static error);
returning or storing the register is an error like any other carrier read.
Exceptions: (a) a caller-saved save with the static frame-escape region based at
rsp+0 is NOT a carrier — the region redirect keeps it alive as a REAL value;
(b) when the entry signature has SysV stack arguments (see "fast-CC stack
argument words" above), the save — and its `leaq 0(%rsp), %reg` form — may go
into ANY register as an entry-rsp ALIAS for reading the incoming stack
arguments; every read of the register while the alias is live is either
redirected to the argument's slot or rejected, and the self-xor/self-sub
zeroing idiom counts as a redefinition, not a read.

A related epilogue shape is the perlasm movq-RESTORE: `movq (%rsp), %r15; movq
8(%rsp), %r14; ...; addq $K, %rsp; ret` restores the saved registers with loads
instead of pops. When a saved register was redefined inside the body (perlasm uses
it as scratch), the save-slot model cannot express the reload (the slot holds the
pre-push value, the web the redefined one) — but the loaded value is dead weight
whenever the register is never read afterwards, so the analysis proves
unobservability with the same forward walk the dropped pops use (checkLostReload,
rejecting a restore into the integer return register outright) and the rewrite
DROPS the load like the pops it already drops. The detection's save-slot alias
computation only fires at a PROVABLE rsp depth: at an unknown ("dyn") rsp depth
(the aftermath of an alloca) the raw displacement is not the slot's frame
coordinate, so a buffer load like point_double's `movq 0(%rsp), %rax` is never
mistaken for a restore load of an outstanding save slot.

While rbp is the frame pointer it is a stack register (rbp = rsp + frameSize), so
reading it as a VALUE — `movq %rbp, %rax`, arithmetic on it, or using it as a memory
INDEX register (`movq (%rdi,%rbp,1), %rax`) — is the same stack-address escape class
as reading rsp and is rejected (the frame setup/teardown paths that legitimately name
rbp — `movq %rsp,%rbp`, `movq %rbp,%rsp`, `leave`, teardown `popq %rbp` — are
unaffected). An rsp memory index (`movq (%rdi,%rsp,1), %rax`, which the encoding
forbids but the parser accepts) is rejected on the same path instead of crashing the
assembler.

#### x86_64: FP/SIMD registers, materialization, and injected-call saves
FP/SIMD registers (xmm/ymm/zmm, MMX mmN, x87 st/st(N), opmask k0-k7) are IN SCOPE.
sarcasm never register-allocates them — it needs no vector registers for the Fil-C
checks, so their register numbers would only collide with GPR numbers in the web
analysis — and the calling convention makes none of them callee-save, so no usage
validation is needed: instructions naming them pass through with their vector
operands verbatim. Only the GPR operands are modeled in defs/uses — the memory
base/index, and the GPR halves of mixed GPR/vector instructions (cvtsi2sd,
movd, cvttsd2si, movmskps, kmov, fnstsw %ax, ...) with their correct def/use
roles. A memory operand of an FP/SIMD instruction is bounds-checked by the
transform at the width the x86_64_fp.luau knowledge module assigns the
mnemonic (see the module entry for the width rules, and the Limitations
classes for the rejection side); aligned forms (movaps/movdqa-class) also
check alignment, clamped to 8 like FilPizlonator's buildCheck (the runtime's
access-check origin cannot express larger alignments — a word-aligned but not
vector-aligned address passes the check and then faults in hardware, same as
compiled code). The resolved width is AUTHORITATIVE over any Intel PTR size
annotation — a contradicting annotation is rejected on the heap and stack
paths alike, including GPR accesses materialized into an FP-tainted frame
cluster, because a lying annotation would under-check the access or under-size
a materialized stack slot (a stack-overflow-into-the-caller hole); the
genuinely size-driven widths take the annotation and also RENDER at it, with
the emitted AT&T mnemonic carrying the encoding suffix (`fld QWORD PTR` ->
`fldl`) — a bare rendering would assemble at gas's default width and silently
change the checked access width.
Full-width two-source permutes (vpermi2*/vpermt2*) and the
UNMASKED expand/compress forms (vexpand*/vcompress*/vpexpand*/vpcompress*) are
SUPPORTED at full vector width — with no {k} writemask they touch all lanes,
a contiguous full-width access; the {k}-masked forms are supported too, via
the mask-aware check (see the transform section).

A stack-frame access by an FP/SIMD instruction cannot ride the GPR slot
virtualization (a slot pseudo-register renders as a GPR, not a vector register),
so analyzeFrame runs an FP TAINT SCAN over the body's stack accesses: intervals
are sweep-merged into clusters, and every cluster an FP/vector access touches —
dragging in any GPR access overlapping it (aliasing: the overlapped bytes must
have exactly one home) — is MATERIALIZED: the instruction is kept and its memory
operand rewritten to a real disp(%rsp) into a reserved region of sarcasm's
synthesized frame. Synthesized offsets are chosen so every input-guaranteed
alignment up to 16 bytes is preserved (entry rsp ≡ 8 mod 16 fixes the residue,
uniformly for rsp-relative and normalized rbp-relative offsets), so movaps/movdqa
stack slots work. Materialization preserves AVX512 decorators on the rewritten
operand: an embedded-broadcast stack access keeps its `{1toN}` (the cluster is
sized at the broadcast element width, so broadcasts off stack slots read exactly
one element), and a `{k}`-masked stack access is rejected (masked-off lanes may
not touch memory — a virtualized slot is a pseudo-register the masked vector
instruction cannot take; heap masked accesses get the mask-aware check instead).
GC root slots keep their ABI-mandated position at the base of
the frame (the Fil-C GC reads them as frame->lowers[i]); the FP regions — the
vector-save area and the materialized clusters — sit past the root area, starting
on a 16-byte boundary (8 bytes of padding when the root count is odd) so the
clusters' mod-16 alignment residues survive the shift past the roots. FP/SIMD
stack accesses needing more than 16-byte alignment (vmovdqa32/vmovaps with
ymm/zmm to the stack) are accepted only with a proven alignment: the frame must
carry an `and $-N, %rsp` note (prologue or mid-function, N a power of two >= 16,
dropped like any other frame setup) with N >= the access width, and the input
offset must itself be aligned to the note's base — otherwise a static error
suggesting the unaligned form (vmovdqu/vmovups). The taint scan enforces this per
access; the cluster residue math then places such clusters at offsets aligned
exactly when the output frame is, and the layout dynamically aligns the output
frame (sub total+A, and $-A, pre-alignment rsp saved for the epilogue — static
sizing cannot align since the entry rsp's mod-A residue is dynamic, only mod-16
being fixed by the ABI).

The runtime calls sarcasm injects invisibly and that RETURN — the pollcheck slow
path, filc_allocate for `.alloca` and the frame-escape region, the ptr-store
aux-ensure/barrier slow paths, the atomic pointer load/store/compare-exchange calls —
would clobber the program's live xmm state (SysV makes every FP/SIMD register
caller-saved, and the Fil-C runtime is compiled SSE2-only), so the transform
wraps them in a vector save/restore into a reserved frame area above the GC
roots. The save/restore is LIVENESS- AND WIDTH-AWARE: `cg.fpSave`/`fpRestore`
emit placeholder nodes (`kind="fpsave"`/`"fprestore"`), and after the whole
function body is emitted (before the frameSlot numRoots shift)
`x86_64_codegen.expandFpSaves` runs a BACKWARD PER-REGISTER WIDTH LIVENESS over
the emitted node stream and replaces each placeholder with exactly the movs the
analysis proves live at that point: a register live only in its low 32 bits is
saved with `movss`, 64 bits with `movsd`, 128 with `movdqu`, 256 with `vmovdqu`,
512 with `vmovdqu64`; a register dead across the call emits nothing. A call
node additionally carries `fpVecUses` — the FP arguments an annotated call
consumes (the source program placed them in xmm registers per the fast CC) —
as uses at the movss=4/movsd=8-byte live width, so an FP argument web stays
live from its definition up to the call and the save/restore brackets any
injected runtime call in between.

The width semantics come from `fp.vecEffects` (x86_64_fp.luau): each instruction
reports, per vector register NUMBER (xmm/ymm/zmm alias by number), a use width
and a "provided width" — the width up to which the instruction PROVIDES values
(written, or architecturally zeroed), with bits above flowing through from
before. Backward transfer for a register: a def with providedWidth p kills
liveness at or below p; a use at width u adds it (`live = max(u, live > p and
live or 0)`, so an RMW like addsd counts its own 8-byte merge read). For
example, a movss LOAD provides 16 bytes (writes 4, zeroes [4,16)), so a later
128-bit read sees the movss's zeros — liveness records 16 bytes live and the
save/restore preserves the zeros; an addsd provides 8 while its upper bytes
flow through. VEX/EVEX destinations provide 64 bytes (upper state zeroed to
MAXVL), VEX scalar forms merge from src1 at 16 bytes, EVEX merge-masked
destinations are also uses, accumulate-style forms (FMA, VNNI vpdp*,
vpternlog*, vpermi2*/vpermt2*, ...) read their destinations, and anything
unrecognized defaults to the safe direction (no def — everything flows
through — and all vector operands used at full class width). User calls (the
pizlonatedFI* thunks and passthrough calls) KILL all vector state (SysV:
values live across a call were already broken in the source program); the
injected filc_* runtime calls are TRANSPARENT (the placeholder pair around
them preserves the state). Only xmm0-15 are ever saved: the SSE2-only runtime
cannot touch ymm/zmm bits above 128, zmm16-31, or k0-7.

The frame reservation is UNCHANGED at 16*vecBytes (vecBytes = the function's
widest vector operand, 16/32/64; slot i stays at 16+i*vecBytes pre-shift) —
the optimization only elides/narrows the movs, never the reservation, so no
layout/computeLayout changes. The reservation is also FORCED at
16*16 = 256 bytes (`x86_64_frame.VEC_SAVE_BYTES`) when any signature in play —
the entry signature's FP args/return, or a callsite annotation's — carries a
float/double class, even if the body has no vector operand at all
(x86_64_frame.analyzeFrame forces it from the entry signature, transform.luau
from callsite signatures): an FP value riding in an xmm register needs the
save area in a GPR-only body just as much. The expanded movs carry no temps (named fixed
operands), so the register allocator
never sees them.

x87/MMX state is NOT saved: the injected runtime-call paths sarcasm wraps never
execute x87 code paths (compiler-generated code never keeps x87 values live
across such points anyway — even though libpizlo.a does contain x87
instructions, the fldt/fstpt in the zmath long-double wrappers), so the
program's x87 state survives across the wrapped calls. k-opmask state is
genuinely EVEX-only, so an SSE2-only runtime build cannot clobber it (an
AVX512 runtime build would require extending this to zmm16-31 and k0-7). The
whole mechanism still emits NOTHING for GPR-only functions with GPR-only
signatures — zero cost (fpSave/fpRestore emit no placeholders at all, and
expandFpSaves is a no-op scan); the one exception is an FP signature, which
forces the 256-byte reservation above even with no SSE instruction in the
body (the saves themselves stay liveness-driven, so a dead xmm still saves
nothing).

#### arm64: NEON/FP registers, slot virtualization, and injected-call saves
NEON/FP registers are IN SCOPE on arm64. As on x86_64 they pass
through verbatim — sarcasm never register-allocates them (regalloc never
models NEON), so instructions naming them keep their vector operands
unchanged and only their GPR operands (memory base/index, the GPR halves of
fmov/scvtf/ucvtf/dup & co) are modeled in defs/uses. The differences from
x86_64 are in the frame policy and the save discipline:

- **Frame slots VIRTUALIZE instead of materializing.** x86_64 materializes an
  FP-touched stack range into a real frame region because its vector ISA
  needs aligned memory operands; arm64's NEON scalar/pair loads and stores
  are just byte movers, so a NEON stack slot virtualizes into the same
  raw-byte GPR slot webs the GPR mechanism uses, via explicit GPR<->vector
  moves: `str q0, [sp, #16]` -> `fmov xslot16, d0 ; fmov xslot24, v0.d[1]`
  (the FMOV vector-element encoding only exists for the top half, so the low
  64 bits move as the d register), `str s0` -> `fmov wslot, s0`, a narrow
  `str b0`/`str h0` -> `umov` + `bfi` (preserving the neighboring bytes,
  like memory); a scalar load zeroes the register's upper bits exactly like
  the ldr it replaces, and pairs (stp/ldp/stnp/ldnp of s/d/q) virtualize per
  element. The AAPCS callee-saved NEON prologue/epilogue writeback forms
  (`str d8, [sp, #-32]!` / `ldr d8, [sp], #32`, `stp d8, d9, ...`) — which
  CANNOT be dropped like the GPR stp/ldp boilerplate, since the caller's
  d8-d15 values must round-trip — virtualize at their final-sp-relative
  offset (tracked via a cumulative sp adjustment, since the writeback's own
  sp update is frame setup we drop). The forms that cannot be virtualized on
  a frame base — multi-structure ld2-4/st2-4, the replicate loads ld1r-4r,
  and lane-indexed single-element forms — are rejected cleanly; on a heap
  base they are modeled memory accesses at their exact width (N structures x
  register width, or N x element for lane/replicate forms; arrangement
  qualifiers validated so a malformed register list never under-checks).
- **Width-liveness vector saves.** AAPCS64 makes v0-v7 and v16-v31 fully
  caller-saved and only the low 64 bits of v8-v15 callee-saved, and the
  Fil-C runtime is built with a full NEON toolchain — so sarcasm's injected
  RETURNING runtime calls are bracketed by saves, and user calls kill
  caller-saved vector state (v8-v15 degrade to their callee-saved low 8
  bytes). Mirroring the x86_64 design, fpSave/fpRestore emit placeholder
  nodes and expandFpSaves runs a backward per-vector-register WIDTH
  liveness (semantics from arm64_isa.vecEffects: NEON loads supply their
  full registers, lane loads merge, stores read only the stored bytes, the
  lane-insert ins/element-fmov destinations read the untouched lanes, and
  an unknown mnemonic — e.g. the crypto families — reads every NEON operand
  at its width and provides nothing, a safe default that never under-saves):
  a v-reg live only in its low 4 bytes saves as sN, 8 as dN, 16 as qN, a dead
  one saves nothing, and v8-v15 save only when live at MORE than 8 bytes.
  The save area is 16 bytes per v0..v31 slot past the GC root area, reserved
  regardless of elision; a GPR-only function emits NOTHING (vecSaveBytes == 0).
- **SVE/SVE2 is rejected** (arm64_isa.checkSve, run on every instruction):
  z0-31/p0-15 registers in any operand position (including structure lists
  and memory base/index) and the SVE-only mnemonic family (rdvl/addvl/
  inc*/dec*/cnt*/while*/ptrue/...) error with "SVE is not yet supported
  (NEON/AdvSIMD only)" — variable-width vector state cannot be modeled by
  the fixed 16-byte slot/save machinery.

### ptrflow.luau — pointer-flow analysis
Seeds pointer-ness: function ptr args (from the signature), results of `;! load ptr`
(likewise `;! atomic load ptr` and the `;! atomic ptr` cmpxchg's accumulator def),
call results whose return type is ptr. Forward-propagates through `mov`/copies and
address arithmetic (`add`/`sub`/`and`/`or`/`shl`/`shr`/`sar`/`lea` and the arm64
forms: GEP keeps lower, changes intval). In particular `and` never drops a
capability: the integer value is masked but the capability pointer stays the
same (fail-closed — a result masked out of bounds traps at the access).
`lower` temp. `ptrtoint` (ptr used as int) reads only intval; `inttoptr` w/o known
origin -> null lower. A memory operand's base/index registers are consumed as
addresses, never as value sources (on x86, `addq (%rsi), %rax` adds the loaded SCALAR;
`lea` is the exception — its memory-shaped operand is the computed value). A
register-register `xchg` is modeled as the crossed pair: the first operand's def
web receives the second operand's pointer-ness and lower, and vice versa.
The fixpoint is guarded against non-convergence: one web can have several def
nodes (x86 2-operand RMW instructions, branch joins), and a web whose defs draw
on genuinely different pointer origins would flip-flop its lower forever, so a
repeated global (isPtr, lowerTemp) state proves the deterministic sharing
iteration would loop. On arm64 (and historically on x86_64) that is rejected as
a conflicting-pointer-sources error. On x86_64 the repeated state instead WIDENS
the oscillating webs to DYNAMIC lowers: a swap (keccak1600's loop-carried
`xchgq %rsi,%rdi` bounce buffer, or the equivalent three-move rotation) or a
conflicting join (`if (c) p = a; else p = b;`) has no single STATIC lower that is
correct, but a dynamic one is exact — the web gets its own fresh lower temp and
every pointer def of the iv web is mirrored by a lower def from the same source
(a lockstep copy the transform emits from ptrflow's dynCopy map; an xchg's
crossed pair is emitted atomically through a scratch). The lower temp then has
one def per iv def (a phi, exactly like the iv web), the lowerTemp assignment is
fixed at creation (the fixpoint cannot oscillate on it), and widening is monotone
(a web widens at most once), so the analysis always converges. Copies OUT of a
dynamic web widen the destination too (a snapshot: sharing would observe the
source's later swaps). Widening is sound: every capability a dynamic lower can
hold entered through a seed and every seed roots its lower at definition, so the
GC still sees every live capability; and a stale lower on a scalar-holding def
can only trap or pass for an in-bounds address, never access out of bounds.

### transform.luau — the GIMSO transform (templates in ABI-NOTES.md / ABI-NOTES-x86.md)
The walk and every invisicap sequence live here ONCE; the actual instructions come from
the per-arch codegen module (arm64_codegen / x86_64_codegen).
Every injected sequence that writes the condition flags (access checks,
pollchecks, allocations, invisicap loads/stores) is bracketed by a
save/restore of the program's flags (x86_64 pushfq/popq through a
regalloc-owned temp, arm64 mrs/msr nzcv) exactly when a backward
per-flag liveness fixpoint over the lifted CFG says a flag is live at the
insertion point (liveIn = use | (liveOut & ~def) as bitmasks: EFLAGS bits on
x86_64 via x86_64_isa's flagUseMask/flagDefMask, all-or-nothing NZCV on
arm64). A real call defines all flags (the ABI clobbers them); a local call
is transparent (hardware `call` preserves flags, so the edge runs into the
clone entry) while a local ret defines them all (the dispatch compares); a
`ret` needs nothing past it. Partial writes (inc/dec, shifts, bt, stc, ...)
are in neither mask and propagate soundly, and jcc/setcc/cmov read only
their condition's flags — so a bounds check before an `inc` feeding a `jz`
brackets nothing, while one between a `cmp` and its branch still saves:
- Represent each in-flight ptr temp as (intval temp, lower temp).
- `;! load ptr`  -> access-check(8, align 8) + non-atomic invisicap load sequence
  (offset = iv-lower; aux=[lower-8]&mask; auxentry=[aux+off]; box bit handling).
- `;! store ptr` -> access-check + aux-ensure (filc_object_ensure_aux_ptr_outline) +
  store barrier (filc_current_marking_state / filc_store_barrier_for_lower_slow) + stores.
  The barrier slow call is skipped for a null stored lower (a NULL store has no
  object to barrier — the runtime's filc_store_barrier_for_lower guards
  `marking && lower`, and its _slow entrypoint asserts non-null).
- `;! atomic load ptr` / `;! atomic store ptr` (x86_64, plain movq forms) ->
  access-check(8, align 8, CanWrite for the store) + the runtime calls
  filc_load_ptr_atomic_with_manual_tracking_outline(slot) resp.
  filc_store_ptr_atomic_outline(myth, slot, value) — exactly what filcc emits
  for C11 _Atomic pointer accesses. SysV marshalling: a filc_ptr is two
  consecutive words (intval, lower); the load returns rax=iv/rdx=lower. The
  loaded lower is rooted immediately; pointer arguments need no caller rooting
  (the runtime protects them). The callees are nounwind — NO exception-flag
  check after them (failures are fatal filc panics).
- `;! atomic ptr` on cmpxchgq (lock or not — both forms make the same call) ->
  access-check(8, align 8, CanWrite) +
  filc_strong_cas_ptr_with_manual_tracking(myth, slot, 0, expected, new), where
  expected = the implicit accumulator's (iv, lower) webs and new = the source
  operand's; new_value travels on the stack ([rsp]=iv, [rsp+8]=lower at the
  call, marshalled via the red zone + a balanced sub/add of rsp around the
  call — see pushStackPtr in x86_64_codegen.luau). The pinned-accumulator
  machinery (emitPinned) is BYPASSED: expected goes in argument registers, and
  the old value arrives rax=iv/rdx=lower to define the accumulator's def web
  (seeded as a pointer in ptrflow, lower rooted). Success <=> old.iv ==
  expected.iv (the runtime compares raw intvals only — a null-lower expected
  is a legitimate "integer guess"); flags are recomputed as (expected - old)
  so a following je/sete behaves natively.
- `;! load store ptr` on a supported 8-byte mem-RMW (add/adc/and/or/sbb/sub/
  xor, inc/dec/neg/not) -> the NON-atomic box-aware pointer load into a fresh
  (iv, lower) pair, the ALU op re-emitted as a register operation on the iv,
  then the NON-atomic pointer store of (new iv, SAME lower) — the capability
  rides through unchanged: Fil-C pointer arithmetic through memory. adc/sbb
  caveat: the op executes immediately after the injected load/check sequence,
  whose last flag-affecting instruction is a `test` (the aux/box logic), so
  the carry-in is NOT the program's — it is clobbered (deterministically CF=0
  in the current emission) — and the value STORED BACK reflects that. adc/sbb
  are supported for execution-without-trapping (and capability preservation),
  not for carry semantics; see the EFLAGS bullet below.
- `;! atomic load store ptr` on the same forms -> WITHOUT lock: an atomic
  load (runtime call), the op, an atomic store (each access atomic; the RMW
  as a whole is not). WITH lock: a compare-exchange loop — inline checks once
  before the loop; per iteration a pollcheck, an atomic load, the op on the
  iv, and filc_strong_cas_ptr_with_manual_tracking(expected=current old,
  new=old.iv OP src with the SAME lower); a mismatch retries from the atomic
  load. The per-iteration returned lower is re-rooted before each safepoint
  call. adc/sbb caveat: here the op executes right after a runtime call (the
  atomic load), so the carry-in is whatever the call left in EFLAGS —
  indeterminate — and the value stored back (or CAS'd in) reflects it, exactly
  like the non-atomic form's clobbered carry-in above but without even a
  deterministic value.
- EFLAGS for the RMW/CAS sequences: these injected sequences clobber EFLAGS
  and are deliberately NOT bracketed by the flag save/restore (their
  native-flag contract depends on it — see below), so the operation is
  re-executed on a scratch copy of the kept-alive pre-op value (and source)
  as the LAST flag-affecting step — a jcc/setcc immediately after the
  annotated instruction sees the native flags (for the CAS, the comparison
  uses a private pre-call copy of the expected intval, since expected and
  result webs can coincide in a retry loop). `not` sets no flags to recompute
  — the documented EFLAGS caveat. adc/sbb are the other caveat, and it is
  wider than the flags: they read a carry-in the injected sequences already
  clobbered, so BOTH the recomputed flags AND the stored result carry it —
  the op itself runs on the clobbered carry (CF=0 after the non-atomic
  `;! load store ptr` load sequence's closing `test`; indeterminate —
  whatever the runtime call left — for the atomic forms), and the flag
  recompute runs on whatever the store/CAS sequence left. So a flag consumer
  that does not immediately follow one of these RMW/CAS sequences still sees
  clobbered flags — but every OTHER injected sequence site (plain access
  checks, pollchecks, allocas, ptr loads/stores, masked accesses) preserves
  the program's flags via saveFlags/restoreFlags whenever the flag-liveness
  scan says they are live (the same withFlagSave machinery arm64 uses; see
  the "Flags" bullet under the arm64 atomics below).
- xadd with `;! load store ptr` / `;! atomic load store ptr` is rejected: its
  source register is also a destination and would receive the old pointer
  value (needing a pointer def + rooted lower — and its use/def web union
  would mis-seed the delta value as a pointer); cmpxchg with them is rejected
  (use `;! atomic ptr`); shifts/rotates/imul are rejected (not
  capability-preserving); all five ptr-family annotations are rejected on
  cmpxchg8b/cmpxchg16b (x86_64) and casb/cash/casp (arm64).
- rep movs*/stos* and bare movs*/stos* (x86_64): a checked block copy / fill
  over the implicit registers. Element widths b/w/l/d/q (the dword is
  `movsl`/`stosl` in AT&T, `movsd`/`stosd` in Intel — each assembler rejects
  the other's spelling, and the renderer normalizes Intel input to AT&T
  output); bare `movs`/`stos` are rejected as width-ambiguous, as are
  explicit operands, repne/repnz on movs/stos, and any annotation (none is
  needed). classify models the implicit rsi/rdi/rcx(/rax) effects with RMW-
  shared occurrences for the advanced pointers (so their capabilities flow
  through with no ptrflow rule) and a fresh scalar-0 rcx-def web; the
  transform emits a DF trap (ud2 — a set direction flag would copy/fill
  backwards, which is not modeled), a read check over [%rsi, %rsi+%rcx*elem)
  for movs and a write check over [%rdi, ...) for both (null, CanWrite for
  writes, lower, upper, then the overflow-free N > upper-ptr length test —
  all skipped when %rcx == 0, exactly like hardware, and one element for the
  bare form), then pins the webs through the physical registers around the
  verbatim instruction. Range failures panic through
  filc_check_aligned_access_fail with the dynamic byte count (a fixed-size
  optimized origin could not attribute them). `rep ret` / `rep nop` keep
  their hint; every other rep-prefixed instruction is rejected.
- ARM64 atomics. Non-pointer atomics are modeled directly as single
  checked memory accesses, needing no annotation (like x86_64's lock-prefixed
  RMWs on non-pointers): the LSE family (swp/ldadd/ldclr/ldeor/ldset/ldsmax/
  ldsmin/ldumax/ldumin + the st<op> no-result aliases, all widths and a/l/al
  ordering variants), cas/casp (+variants), and the exclusive family (ldxr/
  stxr/ldaxr/stlxr +b/h, ldxp/stxp/ldaxp/stlxp). Checks: full bounds + CanWrite
  on every write + NATURAL alignment (the ARM ARM requires it for these — the
  ldxp/stxp/casp pair forms fault on anything less in practice, and single-word
  forms do on some cores — so an unaligned atomic gets the deterministic filc
  alignment trap, never a hardware-dependent fault). Register modeling:
  swp/ld<op>'s old-value destination is a def, the source a use; stxr/stxp's
  status register is a def; cas's compare register is an architectural
  read-modify-write modeled with the pin mechanism (implicit use+def operand
  tables, like x86_64's cmpxchg accumulator) so regalloc keeps both sides in
  the written physical register; casp's compare AND data pairs are both
  pinned (the hardware encodes a pair as one even register number, so all
  four explicit registers must render at their written numbers). The
  annotated pointer forms mirror the x86_64 contracts with arm64 shapes and
  AAPCS64 marshalling (a filc_ptr is two consecutive argument words;
  filc_strong_cas_ptr_with_manual_tracking's eight argument words and
  filc_xchg_ptr_with_manual_tracking's six all fit in x0..x7 — no stack
  marshalling; results arrive x0=intval, x1=lower):
  - `;! atomic load ptr` on ldr/ldur/ldar/ldapr xN -> access-check(8, align 8)
    + filc_load_ptr_atomic_with_manual_tracking_outline(slot). `;! atomic
    store ptr` on str/stur/stlr xN -> access-check(8, align 8, CanWrite) +
    filc_store_ptr_atomic_outline(myth, slot, value). (No writeback forms —
    no atomic encoding has one.)
  - `;! atomic ptr` on cas/casa/casl/casal xA, xB, [xM] (64-bit only;
    casb/cash/casp rejected like cmpxchg8b/16b) -> access-check(8, align 8,
    CanWrite) + filc_strong_cas_ptr_with_manual_tracking(myth, slot, 0,
    expected, new). The pin tables let the transform find the compare
    register's use (expected) and def (old result, pointer-seeded, lower
    rooted) webs without emitPinned. NZCV is recomputed as (expected − old)
    against a private pre-call copy of the expected intval, so a following
    b.eq/cset behaves like x86_64's cmpxchg flags.
  - `;! atomic load store ptr` on a 64-bit LSE RMW (ldadd/ldset/ldeor/ldclr/
    swp + variants, and the st<op> aliases): the instruction is already a
    single atomic RMW. For ldadd/ldset/ldeor/ldclr (and the st<op> aliases)
    the lowering is the runtime compare-exchange loop (the x86_64 `lock`
    form's analog): inline checks once before the loop; per iteration a
    pollcheck, an atomic load, the op re-executed as a register ALU
    operation on the intval (ldadd->add, ldset->orr, ldeor->eor,
    ldclr->bic), and the CAS call with new = (old.iv OP src, SAME lower) —
    the capability rides through: Fil-C pointer arithmetic through memory.
    swp is the exception (its plan ALU is "mov"): the new value IS the
    source operand, not a function of the old value, so stamping it with
    the old value's lower would store a SOURCE pointer into a DIFFERENT
    object with the OLD object's capability (the next dereference traps
    "cannot read pointer with ptr >= upper"). swp therefore lowers to a
    single filc_xchg_ptr_with_manual_tracking(myth, slot, 0, new) call —
    the exact primitive filcc emits for C11 atomic_exchange (a direct call,
    no pollcheck; the runtime retries a weak CAS internally) — with new =
    (src.iv, src's OWN lower); a non-pointer source has no lower temp and
    is marshalled with a null capability (mirroring `;! atomic store ptr`:
    storing an integer into a pointer slot is legal, it just can't be
    dereferenced afterwards). The old-value destination register (ldadd's
    Wt / swp's Wt) is pointer-seeded and receives (old.iv, old.lo) rooted —
    exactly like an `;! atomic load ptr` result (the st<op> forms have
    none). NZCV is PRESERVED across the whole sequence (withFlagSave): the
    native LSE forms write no flags, so there is nothing to recompute. The
    min/max forms are rejected (no single capability-preserving ALU op);
    sub-word forms are rejected (pointer slots are 8 bytes).
  - `;! load store ptr` (the non-atomic form) is rejected on arm64: no
    non-atomic memory-destination RMW instruction exists — write the ldr/op/
    str sequence with separate `;! load ptr` / `;! store ptr` annotations.
  - Flags: the annotated ldar/stlr/ldapr and LSE RMW forms write no NZCV, so
    their injected check+call sequences are bracketed by withFlagSave when
    the program's flags are live — the flag-liveness scan + save/restore
    machinery runs on BOTH backends (x86_64's saveFlags/restoreFlags
    round-trip EFLAGS through pushfq and a regalloc-owned virtual temp); only
    the annotated cas has a flag contract (the recompute above).
- non-ptr load/store through a ptr temp -> access-check(size, align) then the raw ld/st.
  Whether an access is a write is decided by `x86_64_codegen.isStoreInsn` — a
  memory FIRST operand is a store, EXCEPT the read-only forms: cmp/test (they
  write only flags) and the one-operand multiply/divide family (`mulq`/`imulq`/
  `divq`/`idivq` — the memory operand is the multiplicand/divisor, the result
  goes to the implicit accumulator registers; ecp_nistz256's `mulq 0(%rsi)`
  reads a read-only table operand). Every heap WRITE (plain stores,
  memory-destination RMWs incl. locked forms,
  xadd/cmpxchg/cmpxchg8b/16b, FP/SIMD stores, movnti) additionally gets the
  not-readonly CanWrite test (aux word [lower-8] &
  ObjectFlagReadonly|ObjectFlagFree at bits 49-50), exactly like compiled
  Fil-C — the trap reports "cannot write to read-only object." via the write
  bit in the fail origin. Check order mirrors the compiler (null capability,
  alignment, CanWrite, lower, upper — the CheckKind declaration order
  FilPizlonator's canonicalizeAccessChecks stable-sorts by, verified against
  emitted IR); the origin's recorded alignment is clamped to the word size
  (the runtime asserts on wider), while the check itself tests the full
  alignment (cmpxchg16b tests 16). A full-alignment failure (word-aligned but
  not 16-aligned) could not be attributed by the optimized fail path against
  the clamped origin — every recorded test would pass and the runtime would
  hit its "Should not be reached" assert — so it goes to a dedicated stub
  calling filc_check_aligned_access_fail with the TRUE alignment (a clean
  "alignment requirement of 16 bytes not met" trap; the other failure kinds
  keep the optimized stub). The upper-bound test for a size>1 access uses the
  compiler's OVERFLOW-FREE form (FilPizlonator's buildCheck comment: "Ptr <=
  Upper - Size" is an "overflow-free way of saying Ptr + Size <= Upper"):
  the naive `hi = eff + size; hi ugt upper` WRAPS for eff in
  [2^64 - size, 2^64) (reachable: pointer base + a huge index via lea) and
  would pass a wild address through to a CPU fault instead of a clean filc
  trap; real
  uppers are < 2^48, so `upper - size` never wraps and any eff that large
  fails the compare. (The ptr-store sequence's inline upper-bound check uses
  the same form; the size==1 arm needs no form change — a direct `eff uge
  upper` cannot wrap.)
- AVX512 {k}-masked and AVX2 vmaskmov/vpmaskmov memory operands through a ptr
  temp get the MASK-AWARE access check (x86_64 only), exactly the algorithm
  the Fil-C compiler emits for the masked intrinsics (FilPizlonator's
  lowerIntrinsicAccess, verified against clang output): for a fixed-position
  access of N lanes x E bytes (V = N*E the full footprint), flight ptr
  (eff, lower), upper at [lower-16]: (1) ValidObject — lower == 0 fails EVEN
  when the mask is zero (a size=0 origin makes the runtime report "cannot
  access pointer with null object."); (2) stores get the usual CanWrite
  aux-flags test; (3) fast path: eff < lower -> below-slow, eff > upper - V
  -> above-slow (overflow-free), else execute with no mask work; (4)
  below-slow: mask == 0 -> execute (masked-off lanes touch nothing), else
  i = cttz of the N-bit-truncated mask and fail iff eff < lower - i*E —
  contiguous expand/compress forms always fail there (the access starts at
  eff); (5) above-slow: mask == 0 -> execute, else fixed-position fails iff
  eff > upper - (h+1)*E (h = the highest set mask bit), contiguous fails iff
  eff > upper - popcount(mask)*E; (6) failures call the runtime's dedicated
  masked fail functions (filc_masked_read/write_check_fail, resp.
  filc_expand_read / filc_compress_write_check_fail — new fail-stub kinds
  marshalling (iv, lower, mask, V, origin) resp. (iv, lower, mask, E, V,
  origin) with a plain 16-byte filc_origin), producing "masked read not in
  bounds (even accounting for the mask)." & co. The mask is extracted into a
  GPR with kmovw/kmovd/kmovq ({%kN}) resp. vmovmskps/vmovmskpd (the AVX2
  mask vector's per-lane sign bits) and truncated to exactly the lane count.
  Supported: {k}-masked vector moves (vmovdqu8/16/32/64, vmovups/upd, the
  aligned vmovdqa32/64 and vmovaps/pd, keeping their clamped-to-8 alignment
  check), the truncating/saturating vpmov stores (E = the memory element
  size, N = the memory lane count), vexpand*/vpexpand* loads and
  vcompress*/vpcompress* stores (contiguous), and the AVX2 vmaskmov/vpmaskmov
  forms. {%k0} is rejected (not
  a valid writemask — gas rejects it too); masked forms of anything else
  (masked ALU memory sources, gathers/scatters as ever, AMX) stay rejected;
  {k}-masked STACK accesses stay rejected (the frame rewrite virtualizes/
  materializes slots and cannot model masked-off lanes); and a {k} on a
  BROADCAST memory operand ({1toN} or vpbroadcast*/vbroadcast*) rides the
  plain element-width check — the read is unconditional, the mask only gates
  the register destination (verified against the SDM and filcc).
- annotations are validated, never silently ignored: `;! load ptr` / `;! store
  ptr` require a plain 8-byte GPR load/store of the matching direction (a
  mismatch would silently replace the instruction with an invisicap access of
  the wrong shape); a memory-destination RMW carrying one is rejected
  (pointer RMWs need `;! load store ptr`); the five ptr-family annotations
  (`;! atomic load ptr`, `;! atomic store ptr`, `;! atomic ptr`,
  `;! load store ptr`, `;! atomic load store ptr`) are shape-validated by the
  per-arch backend (`ptrAtomicShape`): the atomic load/store forms are at
  least as strict as the plain ones, `;! atomic ptr` requires an 8-byte
  memory-destination cmpxchg, the RMW forms require a supported 8-byte
  mem-RMW, everything else is rejected with a precise error (on
  cmpxchg8b/cmpxchg16b EVERY ptr-family annotation is rejected — a
  double-width CAS cannot operate on invisicaps); any other
  unrecognized annotation string on an instruction is a compile-time error.
  (Marker spelling, once, since these bullets all use the universal form: every
  `;!`-spelled annotation can equivalently be written
  `#!` on x86_64 or `//!` on arm64 — the RECOMMENDED spellings, see README's
  "Annotation markers". The annotation NAME is what the bullets document; the
  marker only introduces it, whichever of the three is used.)
- calls (`;! sig` on the call insn — REQUIRED on every direct call: an unannotated
  direct `call foo`/`bl foo` is rejected by body validation on BOTH architectures,
  see Limitations) -> marshal args into the Fil-C CC registers, call the
  callsite thunk `pizlonatedFI<sig>_foo`, test the exception flag (propagate on
  throw), read results. If `foo` is defined and annotated in the same module,
  `pizlonatedFI<sig>_foo` is a strong `.set` alias of its fast entrypoint (no
  resolver involved); otherwise sarcasm emits one weak/hidden callsite resolver
  thunk per distinct called external — it resolves the callee through
  `pizlonated_foo`, validates non-null capability, FUNCTION special type,
  canonical intval and signature (a DATA symbol panics at the special-type
  check), takes the fast entrypoint on an exact signature match, and marshals
  through the generic buffer CC on a mismatch.
- indirect calls (`call *%reg ;! sig(...)` on x86_64, `blr xN ;! sig(...)` on
  arm64 — both architectures support REGISTER-operand annotated indirect
  calls through the same arch-neutral transform code; on arm64 validateBody
  delegates to the shared annotation walk, which accepts exactly the annotated
  register-indirect shape) -> filcc's exact indirect-call sequence for the
  flight pointer (P = target intval temp, L = its lower temp): L==0 -> fail;
  aux = [L-8] must satisfy (aux & 0x780000000000000) == 0x80000000000000
  (special type FUNCTION); P must equal aux & 0xFFFFFFFFFFFF (the canonical
  entrypoint); then the signature word at [L+16] dispatches: a match calls
  the fast entrypoint at [L+0] with the direct call's fast-CC marshalling
  (rdi=myth, rsi=L, dense args; pointer args as iv,lower pairs), a mismatch
  runs an INLINED generic (buffer-CC) call through [L+8] — the arguments are
  stored into my_thread's CC buffer (data words at 128(myth), aux/lower words
  at 384(myth), zeroed aux for scalars), rdx = argument byte size, and after
  the call the returned ret_size in rdx is checked (else
  filc_cc_rets_check_failure) and the result unmarshalled from the buffer —
  exactly the callsite thunk's L_generic mismatch path, so (unlike filcc,
  which calls the weak pizlonated1ET<sig> thunk there) sarcasm has no link
  dependency on pizlonated1ET<sig>, which exists only when some C compilation
  unit contains an indirect callsite with that signature. Both paths rejoin at
  the direct call's exception-flag test
  and result unpacking (a pointer result's lower comes from rcx and is rooted
  immediately). All failure edges share one stub:
  filc_check_function_call_fail(rdi=P, rsi=L). The target register's web must
  be a known pointer value (ptrflow must know its lower) — a pointer
  argument, a `;! load ptr` result, or a ptr-returning call's result;
  anything else is a compile-time error, as are an UNANNOTATED indirect call
  (which would otherwise reach memory unchecked, with no caller-saved clobber
  modeling) and ANY memory-operand indirect call (`call
  *mem` — the loaded value has no capability; load the function pointer into
  a register first, e.g. via `;! load ptr`). The lift models the yolo
  argument registers as uses and the result register as a def for an
  annotated indirect call exactly like a direct one, and the renderer emits
  the AT&T `*` for indirect call/jmp operands (clang's integrated assembler
  rejects the star-less form). On arm64 the same sequence is emitted for
  `blr xN ;! sig(...)` through the same shared code with the arm64 CC
  registers (x0=myth, x1=L, dense args from x2, w2=argument byte size,
  x1 doubles as the ret-size register, fail stubs use plain `bl` — no @PLT
  relocations); the signature immediate uses the same movz/sub-cmp/movk
  widening as the callsite resolver. Memory-operand indirect calls do not
  exist on arm64 (`blr` is register-only), so the x86 `call *mem` rejection
  has no arm64 analog.
- loops (back-edges from the CFG) -> insert a pollcheck at the loop header.
- X86_64 implicit-counter support (`loop`/`loopq`/`loopl`, `jrcxz`, `jecxz`):
  the counter is an IMPLICIT register operand the emitted instruction always
  operates on physical rcx, so the counter's (unified use+def+RMW) web is
  PRECOLORED to physical rcx (fixedReg from classify -> temps[t].precolor in
  the transform) — the modeled decrement/branch must land in the register the
  hardware actually decrements/tests. Two invariants make the precolor SOUND:
  - The counter's fixed color is EXCLUSIVE for the whole function (transform
    collects the fixedReg webs and hands the color set to regalloc's
    `reserved` option): rcx is removed from the allocator's color pool and
    coalescing into any precolored temp OF a reserved color is blocked, so no
    other web can ever be ASSIGNED rcx or INHERIT it by aliasing an entry-unpack
    or argument-marshal move. This de-precolors the entry/dense-arg webs the
    fast CC seeds from rcx (a pointer argument's capability lower at dense
    slot 2, an int argument's intval at dense slot 2): their entry unpack move
    (rcx -> allocated web) becomes a real copy and regalloc spills/reloads them
    like any web. Without this, a counter web defined while a pointer
    argument's capability LOWER web was still live silently destroyed the
    lower (the IRC's George criterion coalesced the lower into the pinned
    physical rcx) and every later bounds check read the COUNTER as the
    capability lower. Nested loops with two counters are hardware-faithful:
    each counter def is a modeled rcx web def and all counter webs alias the
    same physical register exactly as the hardware register does.
  - Annotated CALLS in a counter function split into three classes, each with
    its own handling of physical rcx:
    * POINTER-returning annotated calls (direct and indirect, `retIsPtr`): the
      counter is NOT preserved — the call DEFINES the counter web from the
      result's LOWER web right after the call's result unpacking. Under the FIP
      CC the callee delivers a pointer result's lower in retLo = rcx, so
      hardware rcx after the call IS the returned lower; the pre-call counter
      value is gone (the callee's return destroyed it), and restoring it would
      stomp the returned lower. The wire is a plain copy (rcx already holds this
      value, so it changes no hardware behavior); it makes the MODELED web
      match the register. The lower web is the callee's modeled capability
      lower (a real object base, or zero for an unmodeled result) — sarcasm
      never delivers an arbitrary asm-written rcx value as a returned lower,
      because the caller roots every call-result lower into a GC root slot and
      the GC marks filc_object_for_lower(lower) blindly (a fabricated lower
      would crash the next safepoint).
    * INJECTED runtime calls (pollcheck slow path, filc_allocate, the aux/barrier
      slow paths, the atomic pointer operations): the counter IS preserved
      (save/restore of the counter web through a fresh scratch web, which, being
      live ACROSS the call, the allocator keeps out of every caller-saved
      register — callee-saved or spilled). Hardware has no such call at all, so
      preserving the counter around it is definitionally exact.
    * NON-pointer-returning annotated calls (direct and indirect): the counter
      IS preserved the same way. This matches plain hardware only when the
      callee leaves rcx alone — sarcasm cannot see the callee's internal rcx
      usage, and the arg marshal writes dense rcx (a call's 4th GPR argument
      word, or a pointer argument's lower at dense slot 2) BEFORE the call,
      which plain hardware does not do. This residue is documented and
      deterministic, and preserving is the closest-to-hardware choice: plain
      hardware leaves the pre-call counter alone, while not preserving would
      let the marshal's dense-rcx write reset the counter every iteration (a
      `loop` countdown whose body calls never terminates).
    Every pollcheck in a counter function preserves rcx — the counter's OWN back
    edge passes its web explicitly (loopHeaderRcx), any other header (a GC-churn
    loop whose back edge is a plain `jmp`, a CAS retry loop) falls back to the
    representative counter web; a pollcheck at a non-counter back edge otherwise
    lost rcx to filc_pollcheck_slow. The noreturn fail stubs need no save.
    Functions WITHOUT counters reserve nothing and emit nothing — their output
    is unchanged.
- X86_64 rel8-counter branches (`loop`/`loopq`/`loopl`, `jrcxz`, `jecxz`) are
  rewritten at RENDER time, unconditionally, through a rel8-reachable
  TRAMPOLINE (x86_64_render): these instructions re-emit verbatim and have NO
  encoding other than rel8, while the instrumentation (the pollcheck at a
  back-edge label, the per-access bounds checks) routinely pushes their target
  beyond +-127 bytes — gas would reject the whole file with an opaque "value
  of ... too large for field of 1 byte" from the temp .yolo.s. Sarcasm cannot
  predict the post-instrumentation displacement, so every such branch is
  emitted as `loop .Lsarctramp_<fn>_<n>t` / `jmp .Lsarctramp_<fn>_<n>s` /
  `.Lsarctramp_<fn>_<n>t:` / `jmp <original target>` / `.Lsarctramp_<fn>_<n>s:`
  — the trampoline label is 2 bytes ahead (always in rel8 reach), the skip jmp
  lands past the stub, and the stub's `jmp` can be rel32. NO flag-affecting
  instruction is added (jmp preserves all flags; loop/jcxz do not read them —
  a dec/jne rewrite is REJECTED: it would change the flags the fall-through
  path observes), fall-through semantics are unchanged, and the IR-level
  instruction plus its original target stay untouched (validateBody,
  reachability, liveness, the counter precolor and the pollcheck-at-back-edge
  machinery all still see the original branch — the trampoline sits at the
  jump site, the pollcheck at the label site). `<n>` is a per-function
  statement-order counter, bumped past any user label name anywhere in the
  FILE (the .Lsarctramp_* symbols are file-global: a user label named like
  another function's generated trampoline name otherwise collides in the
  object; generated names of different functions cannot collide, since each
  embeds its own function name), so synthesized labels are unique and
  deterministic.
- `.alloca size, alignment, result` -> a GC allocation via filc_allocate (payload
  = allocation + 16, the capability lower rooted), not real stack memory — and
  without touching %rsp/sp, so the input keeps its stack semantics. size/alignment:
  immediates, GPRs (including pseudos), or spill slots; result: a register, a
  pseudo, or a spill slot. Wider-than-16 alignments over-allocate and align the
  payload up (the lower stays the payload base, so bounds checks cover the whole
  object); a dynamic alignment is validated at runtime (nonzero power of two)
  with a pure trap otherwise. The historical `;! alloca` annotations (which
  rewrote the input %rsp math) were removed and are compile-time errors.
- pseudoregisters (`%fil_<ident>` / `fil_<ident>`): extra 64-bit GPRs with no fixed
  physical register, numbered in a dedicated band at parse time and colored by the
  register allocator like any other virtual web (def/use/kill, calls, spills,
  pointer flow). GPR-only: combined FP/vector use and malformed names are
  compile-time errors.
- dead-code elimination then deletes instructions whose modeled defs nothing
  forward-reachable reads.
- fabricate prologue: SOV check + filc_frame push (prev,origin,roots) + callee-saved.
- roots: store each live-across-safepoint pointer's lower into a frame root slot;
  origin count field = number of root slots.
- injected RETURNING runtime calls (the pollcheck slow path, filc_allocate, the
  atomic pointer load/store/compare-exchange calls, the ptr-store
  aux-ensure/barrier slow paths) -> wrap in a vector save/restore; the
  source program never saw these calls and the runtime clobbers the vector
  registers. A no-op for GPR-only functions unless a signature in play carries
  a float/double class — the full mechanism (width-aware saves, reservations,
  the x87/k-opmask exceptions) is described in the frame section's FP/SIMD
  subsection and the arm64 NEON subsection.

- Determinism invariant: transform output is deterministic across processes —
  every site whose iteration order could reach the output (or the temp/slot
  numbering) iterates a sorted key array instead of a table. Re-verify by
  running the transform on the same input several times and diffing the
  outputs: all runs must be byte-identical.

### emit.luau + sarcasm.luau (driver)
- emit: renders the IR with colors via the per-arch render module; materializes spill
  slots; emits the prologue/epilogue with the exact callee-saved set actually colored;
  appends glue via the per-arch glue module. IRC actual spills are handled by rewriting
  (reload before use / store after def into a fresh slot) and re-coloring; the shared
  emission walk also eliminates redundant reloads and no-op self-moves.
- driver: `as`-like args. `-o x.o` default -> write `x.yolo.s` (temp, same dir) then run
  `as` (configurable via `--as`, default `as`) to make `x.o`. `--no-assemble`/`-S` ->
  emit assembly; if no `-o`, write `<input>.yolo.s`. Also splits the input into
  functions, auto-detects the target (detect.luau), and packs spill slots after each
  allocation round (slots with non-overlapping live ranges share an offset).

#### Command-line target selection (--x86_64 / --arm64 / --intel / --at&t)

The target is auto-detected as above; four options force it explicitly, bypassing
detect.luau:
- `--x86_64` — select the X86_64 backend; the input syntax (AT&T vs Intel) is still
  auto-detected, so a `.intel_syntax`/`.att_syntax` directive still applies. When
  the input carries no syntax directive and no x86 syntax marker at all, the
  syntaxless forced case defaults to AT&T (GAS's x86 default): an unadorned
  x86_64 parse would silently be INTEL parsing (x86_64_parse only special-cases
  "att"), so bare `--x86_64` is pinned to AT&T. An explicit `--intel`/`--at&t`
  still wins.
- `--arm64` — select the ARM64 backend.
- `--intel` — select the X86_64 backend with Intel input syntax.
- `--at&t` — select the X86_64 backend with AT&T input syntax.

The options conflict with each other: `--x86_64` with `--arm64` (both select an
architecture), `--intel` with `--at&t` (both pin a syntax), and any architecture
selector with a syntax-pinning selector (`--x86_64` with `--intel`/`--at&t`,
because a pinned syntax already implies X86_64; `--arm64` with all three).
Conflicts are usage errors (exit code 2) naming the flag seen earlier on the
command line; repeating the same option is harmless.

## Limitations (compile-time rejections)
Assembly that cannot be proven safe is rejected with a clean `sarcasm: <file>: <msg>`
error (exit code 1). Current limitations, enforced on both architectures unless noted:
- Every function declared `.type NAME, %function` and defined in the file MUST carry a
  signature annotation on its label; an unparseable signature is likewise rejected.
- On arm64: at most 3 register arguments per function; the fixed x2..x7 arg pairs
  already cap any 3-argument signature at 6 words, and callsite signatures are bounded
  the same way (the arm64 callsite thunk's hold pool is also 6). On x86_64 there is NO
  GPR arity limit: the fast CC packs the first four argument words into rdx,rcx,r8,r9
  and passes the rest on the stack as densely packed 8-byte slots (a pointer arg
  occupies two words — one can straddle the r9/stack boundary), and sarcasm marshals
  the stack words in both directions. At ENTRY, the unpack loads words 4+ read-only
  from the caller's outgoing-args area (`[rsp + frame + 8 + 8*w]`, remapping them onto
  the SysV yolo register sequence — e.g. `void(ptr,ptr,size_t)` loads the size_t from
  `[rsp+8]` into %rdx); the SysV 7th+ integer arguments (read at `8+8i(%rsp)` at
  entry, directly or through a register that parked the entry `%rsp`) are redirected
  to argument-slot webs (GPR reads) or a materialized frame area (FP/vector reads)
  fed from the same incoming words. At a CALLSITE, dense words 0..3 marshal into the
  CC registers and words 4+ are stored into the outgoing-args area at `8*j(%rsp)`
  before the call (rsp dropped over them and restored right after), exactly like
  pizlonated clang; the callsite resolver thunk flows its incoming stack words
  through to the callee in place on the fast path and reads them back into the CC
  buffer on the generic path. FP arguments are still bounded on both architectures:
  at most 8 (xmm0..xmm7 / v0..v7).
- Indirect calls: on BOTH architectures a register-indirect call is supported
  when it carries a `;!` callsite signature and its target register's web is a
  known pointer value (`blr xN ;! sig(...)` on arm64 — validateBody delegates
  to the shared annotation walk; `call *%reg ;! sig(...)` on x86_64; see the
  transform section's indirect-call bullet for the emitted check and dispatch
  sequence). Rejected on both: an unannotated register-indirect call
  ("indirect call has no signature annotation (annotate the callsite with the
  callee's signature, e.g. `int(ptr)`)"), and an annotated call
  whose target web is not pointer-typed ("indirect call target must be a
  function pointer value..."). On x86_64 additionally ANY memory-indirect call
  `call *mem`, annotated or not ("indirect call through a memory operand
  cannot be made memory-safe (load the function pointer into a register
  first...)") — memory-operand indirect calls do not exist on arm64
  (`blr` is register-only).
- Unannotated DIRECT calls (`call foo` with no signature annotation) are
  rejected on BOTH architectures (validateBody, gated on each backend's
  `strictBodyValidation` — enabled on both: a passthrough would land the call
  in the callee's FIP body without myth marshalling and could never link
  against C callees, which filcc names `pizlonated_*`, never bare `foo`). The
  rejection message matches arm64's ("call to 'foo' has no signature
  annotation (annotate the callsite, e.g. `call foo ;! int(ptr)`)").
- Indirect BRANCHES are rejected on BOTH architectures in all raw forms: arm64
  `br xN` and `b xN`/`cbz xN, xM` with a register operand (any non-label branch
  target classifies as an indirect branch), and x86_64 `jmp *%reg` / `jmpq *%reg`
  and `jmp *mem` (both classify as indirect branches — the renderer would
  otherwise pass `jmp *%rax` through verbatim as an uncontrolled branch; memory
  targets are also caught by the moffs rejection when the operand names a symbol
  or absolute address, e.g. `jmp *myglobal`). A branch whose target operand is
  not a label at all (`jcc *%reg`, which is not even encodable, or a raw
  absolute `jmp $imm`) is rejected by the shared no-label-target check; a branch
  to a non-local LABEL is a tail call and is rejected on both (even annotated).
  Label-target branches inside the function (loop back-edges, `jmp
  .Llabel`/`1b`) keep working — all of this closes the same unvalidatable
  control-flow hole.
- (x86_64) explicit high-byte operands (%ah/%ch/%dh/%bh) are MODELED EXACTLY
  with the pin machinery (classifyHighByte in x86_64_isa.luau): the parser maps
  a high-byte name to its register's web (nums 0-3, width 8 — the SAME web as
  the low-byte al/cl/dl/bl; the web model has no subregister view), so every
  GPR operand of an instruction carrying a high-byte operand is pinned to its
  SPELLED physical register (emitPinned copies the reaching webs in, emits the
  spelled instruction, copies the results back out). A high-byte source reads
  bits 8-15 of the enclosing register's pinned physical register; a high-byte
  destination merges into bits 8-15 and preserves the rest (an RMW of the
  enclosing web); a low-byte partner destination (movb %ah, %cl) is likewise an
  RMW (its bits 8-63 flow through the pin), while a movzx/movsx destination is
  a PURE def (the instruction zero-extends). The renderer keeps the high-byte
  name for pinned operands (emitPinned tags them; build.rp alone would render
  the LOW byte). Supported forms: movb (reg-reg and imm), movzbl/movzbw/
  movsbl/movsbw reads, the byte ALU/cmp/test/inc/dec/neg/not forms, setcc, and
  xchgb. Rejected cleanly (gas also rejects them): a high-byte operand with a
  MEMORY operand (the bounds-check rewrite addresses memory through an
  allocated register that could leave the REX-free registers), any partner
  that requires a REX prefix (byte partners other than %al/%cl/%dl/%bl,
  partners numbered 8+, any 64-bit operand — so movzbq/movsbq with a high-byte
  source are out), a high-byte shift count (only %cl encodes one), and a
  high-byte movzx/movsx destination (not an encoding). lahf/sahf are unaffected
  — they stay modeled as implicit full-web RMW/use of the 64-bit rax web
  pinned to physical rax.
- A branch to the function's OWN ENTRY NAME (`jmp f` from inside `f`) is
  rejected on BOTH architectures (shared validateBody branch-target check): the
  entry label is in the body's local-label set, but the entry symbol itself is
  renamed away, so the branch would only die at link time ("undefined
  reference") — and if it did link, re-entering the prologue would re-run the
  SOV check against a perturbed rsp and grow the frame unboundedly.
- A body that can FALL OFF ITS END is rejected on BOTH architectures: a shared
  reachability worklist over the raw body's statement indices (unconditional
  label branches go only to their target; conditional branches also fall
  through; `ret` stops a path; every other statement — calls and indirect forms
  included — falls through) proves whether the index one past the last
  statement is reachable, i.e. whether some control-flow path runs past the end
  of the body without executing `ret`; the emitted FIP body would then fall
  through into sarcasm's own next emission (the generic-entry thunk or a fail
  stub) and execute through caller-garbage registers. Error: `sarcasm: function
  'f' can fall off the end of its body; every control-flow path must end with a
  ret instruction: ...`. A body ending in an infinite loop has NO reachable
  fall-off and stays accepted. This runs before the frame pass, whose
  control-flow walk only understands validated bodies. One conservative
  consequence: a call to a noreturn function is still just a call — it falls
  through — so a body whose ONLY exit is such a call is rejected; restructure
  it with a `ret` after the call (unreachable but present) or an explicit
  infinite loop.
- Top-level (inter-function) content is scanned on BOTH architectures.
  On X86_64, contiguous top-level data under `.section .rodata*`/`.data`/`.bss`
  (plus `.comm`/`.lcomm`) is COLLECTED into Fil-C data objects (DOs) — the
  global-variables feature, see the "Global variables (x86_64)" section below.
  On arm64 (and on x86_64 for content outside those sections), the historical
  uniform semantics apply: data under a global or referenced label,
  symbol/macro definitions (`.equ`/`.set`/`.macro`; on arm64 also
  `.comm`/...), instructions outside any function, and labels outside any
  function are rejected; provably dead content (an unreferenced, non-global
  local label and its data bytes) is dropped, and structural directives
  (`.text`, `.section .note.GNU-stack,"",@progbits`, `.p2align`, `.cfi_*`,
  `.type`, `.size`, ...) stay accepted+ignored. On x86_64 `.equ`/`.set`/`.macro`
  stay rejected everywhere and `.quad label`-style pointer initializers are a
  clean compile-time rejection.
- On X86_64, instructions that manipulate processor state sarcasm cannot model, or
  that touch memory the checker cannot see, are rejected with clean `sarcasm:`
  errors — the full class list with per-instruction reasoning is in the
  enter/enterq bullet below.
- Exception propagation through sarcasm x86_64 frames: NOT SUPPORTED (intentional,
  for now). The x86_64 glue emits every function origin with personality_getter = 0
  (`.quad 0`) and can_throw = can_catch = has_setjmps = 0 (`.byte 0/0/0`), so Fil-C
  unwinding (filc_native__Unwind_RaiseException, libpas/src/libpas/filc_runtime.c)
  fatals at any sarcasm x86_64 frame whose origin is !can_catch: a C++ exception
  thrown in code CALLED from sarcasm x86_64 assembly terminates the process instead
  of reaching a handler in an outer frame — it can neither be caught inside nor
  propagate through the sarcasm x86_64 frame. The arm64 glue emits can_throw =
  can_catch = 1 (personality still NULL — no landing pads, so a throw still cannot
  be CAUGHT inside sarcasm code, but it DOES propagate through sarcasm arm64
  frames). Detailed in ABI-NOTES-x86.md's "Exceptions / unwinding" section.
- Floating-point signatures: float/double are FULLY SUPPORTED in `;!`
  signatures on BOTH architectures, entry and callsite alike (see the
  "Floating-point signatures" section below). Still rejected on both:
  `long double` (no fast CC on arm64; stack-passed on x86_64) and the vector
  classes (a vec4 signature would need q-register/xmm-vector CC machinery,
  and filcc's encoding only matches sarcasm's formula for vec4 anyway).

- Taking the address of the stack frame is rejected (cannot prove safety): by
  address arithmetic off the stack register (`add xD, sp, #k` / `leaq
  8(%rsp), %rax`), by copying it (`movq %rsp, %rax`), or by storing it into a
  frame slot (`movq %rsp, (%rsp)` — the slot virtualization rewrites only the
  memory operand, so the live sp/fp value would leak into a virtual temp). The
  exceptions: address arithmetic landing INSIDE the promoted frame-escape region
  (redirected to a real pointer into the GC region) and the phantom saved-rsp carrier flow
  (every use of the parked value is dropped or rejected, so nothing observable
  escapes — see the frame section). The
  full mid-function stack-pointer-movement policy is in the frame section.
- Stack accesses outside the input frame — below it (on X86_64, below the
  128-byte SysV red zone under rsp, via rsp- or normalized rbp-relative
  offsets alike), above it, or stores into the caller's argument area — are
  rejected, as are indexed stack-relative access and a stack-relative memory
  operand with a SYMBOLIC displacement (`movq foo(%rsp), %rax`) — its target
  is unknown at compile time, so it cannot be bounds-checked or virtualized.
- `.alloca` requires exactly 3 operands (size, alignment, result) of the
  documented shapes; anything else (symbols, FP/vector registers, heap memory,
  bad immediates) is rejected. A `.alloca` in a promoted-region frame or at an
  unknown depth with spill-slot operands is rejected rather than silently
  mis-virtualizing the slot.
- Dropping the alloca's %rsp-mutating instruction also discards its flag effects: a
  body whose control flow consumes the flags of that instruction (branching on the
  allocation's `sub`/`mov` flags) observes different flags than hardware. An
  alloca-using body must not branch on the flags of its dropped stack math.
- On ARM64, memory operands on non-load/store instructions are rejected; X86_64 accepts
  them (e.g. `addq (%rsi), %rax`) — on X86_64 any memory operand is modeled as a real,
  bounds-checked access.
- On ARM64, NEON/FP (AdvSIMD) is IN SCOPE (see the arm64 NEON subsection of
  the frame section): b/h/s/d/q/v registers and structure lists pass through,
  heap accesses are checked at exact widths, frame slots virtualize into the
  GPR slot webs. Rejected: SVE/SVE2 ("SVE is not yet supported (NEON/AdvSIMD
  only)"), the NEON forms that cannot be virtualized on a FRAME base (on a
  heap base they are supported at exact widths), NEON structure pre-index
  writeback (no such encoding in the modeled subset), a memory operand on any
  non-load/store mnemonic (prfm & co), and `long double`/vector signature
  types.
- On X86_64, `enter`/`enterq` is rejected (packed frame setup sarcasm does not model —
  see the frame section). FP/SIMD code is IN SCOPE (see the FP/SIMD subsection of the
  frame section); the only rejected instructions are ones whose memory-safety effects
  cannot be checked, each with a clean `sarcasm:` error. They fall into classes:
  * PRIVILEGED / SYSTEM STATE, whose effect on memory safety is unknowable:
    syscall/sysenter/sysexit/sysret, port I/O (the in/out family), hlt,
    wrmsr/rdmsr, mov to/from cr/dr, lgdt/lidt/lldt/ltr/lmsw, skinit. The
    unprivileged counterparts ARE supported at exact widths (sgdt/sidt=10,
    sldt/str/smsw=2, lss/lfs/lgs = destination size + 2-byte selector).
  * STATE THE FIL-C RUNTIME OWNS, which a silent write corrupts without a
    fault: FSGSBASE rdfsbase/rdgsbase/wrfsbase/wrgsbase (the fs/gs
    thread-pointer bases — a canonical wrfsbase replaces fs.base with no fault
    and TLS breaks, the rd forms leak the thread pointer), swapgs, WRITES INTO
    a segment register (a bad selector raises an unconverted hardware fault;
    the parser classifies %es/%cs/%ss/%ds/%fs/%gs as their own operand class,
    rc="seg", so the destination is visible), and PUSH/POP of a segment
    register (a push only READS the selector and a pop's selector load rides
    the stack push/pop machinery — the selector transfer is what the checker
    does not model). Plain selector READS pass through soundly, and
    segment-QUALIFIED memory operands (`%fs:0x28`) keep the symbolic-address
    rejection.
  * UNMODELABLE IMPLICIT MEMORY OR CONTROL FLOW: string instructions
    (lods/scas/cmps in every size — movs/stos ARE modeled, see below) and the
    repne/repnz prefixes, rep*/repe/repz on anything but movs*/stos*, and the
    xacquire/xrelease prefixes (implicit rsi/rdi memory the checker cannot
    see),
    gather/scatter, maskmovdqu/maskmovq (implicit DS:rdi destination),
    umonitor (arms monitoring on a range whose extent cannot be
    bounds-checked), clzero/xlatb (implicit rax / rbx+al memory), lcall/ljmp
    (an invisible far-return-frame stack push), TSX xbegin/xend/xabort
    (transactional control flow and memory; on TSX-less hardware the
    encodings are #UD), the CET `notrack` prefix (changes indirect-branch
    tracking for the instruction that follows) and `setssbsy` (a shadow-stack
    write through the implicit SSP), SGX encls/enclu/enclv (implicit
    eax/rbx/rcx plus enclave-controlled memory).
  * IMPLICIT OPERANDS THE DEF/USE MODEL CANNOT EXPRESS, which regalloc could
    silently rename: the pcmpestri/pcmpistri family (the estri forms read
    rax/rdx as string lengths, the stri forms clobber rcx) and
    umwait/tpause (the implicit edx:eax TSC-deadline pair). The
    cpuid/rdtsc/rdtscp/xgetbv/rdpkru/wrpkru/monitor/mwait family IS modeled
    exactly and pinned (see the x86_64 backend notes).
  * STATE IMAGES OF UNTRACKED SIZE: the xsave/xrstor family (CPU-dependent
    image size — fxsave/fxrstor ARE supported, a fixed 512-byte image) and
    fsave/frstor/fstenv/fldenv (x87 environment/state saves).
  * FOOTPRINTS THAT ARE NOT ONE CHECKABLE ACCESS: AMX tile instructions (a
    strided, palette-configuration-dependent footprint, up to 16 rows x 64
    bytes) and the {k}-masked forms of anything outside the supported set
    (masked ALU memory sources, {%k0}, {k}-masked stack-frame accesses). The
    SUPPORTED masked forms — vector moves, truncating stores, expand/compress,
    the AVX2 vmaskmov/vpmaskmov forms — get the mask-aware check (see the
    transform section), and UNMASKED expand/compress stays supported at full
    vector width (no writemask = all lanes, a contiguous full-width access).
  * ENCODINGS WITH NO MODELED MEMORY FORM: `lock` outside the exactly-modeled
    memory-destination RMWs — classify allows it exactly on
    add/adc/and/or/sbb/sub/xor, inc/dec/neg/not, xadd, cmpxchg,
    cmpxchg8b/cmpxchg16b (the hardware locked set intersected with the modeled
    set); the locked instruction rides the normal write-classified checked
    path (including the not-readonly CanWrite test) and the renderer re-emits
    the prefix, while `lock` on a stack-frame access is rejected by the frame
    rewrite (which runs BEFORE classify) — the slot virtualizes/materializes,
    so accepting the prefix would silently elide it, a locked RMW degenerating
    into an unlocked register operation. Also xchg with a MEMORY operand (an
    implicitly LOCKED RMW that is not one of the exactly-modeled memory RMWs;
    register-to-register xchg IS modeled), the register-to-register movbe
    spelling (its only baseline encodings are byte-swapping MEMORY moves —
    the load/store forms ARE modeled, see below), and `int N` for N≠3
    (int3/`int $3` stay legal — like ud2 they only raise SIGTRAP/SIGILL,
    which Fil-C forbids installing handlers for, so a merely-trapping
    instruction is fine).
  * MEMORY FORMS WHOSE WIDTH OR ADDRESS CANNOT BE MODELED: unknown mnemonics
    with memory operands (the width is undeterminable and a guess could
    under-cover the access — enqcmd's m512 source, an unknown vector load — so
    unknown+mem REJECTS rather than guessing; unknown REGISTER-only forms pass
    through with the conservative first-register-def/rest-use model, which
    covers only genuinely-unknown register-only mnemonics, since the common
    compiler-output families have exact width/def-use entries in
    x86_64_fp.luau and the implicit-register class is pinned); and symbolic
    (RIP-relative/global) / absolute-address (moffs) operands — a bare
    (non-$-prefixed) AT&T symbol operand LOADS from the global and a bare AT&T
    numeric literal loads from that address (NOT an immediate move), while
    gas's Intel syntax gives a bare symbol the same absolute-moffs reading
    (proven against gas/objdump: `mov rsi, myglobal` emits an R_X86_64_32S
    absolute relocation — only a bare NUMBER is an immediate there) — so both
    reject as un-checkable memory accesses ("memory access with a symbolic
    address cannot be bounds-checked (if '<sym>' is a Fil-C global defined in
    another module, annotate the instruction with `#! global ptr`)" /
    "...with an absolute address..."). RIP-RELATIVE operands are the global-
    variable path, exempt when the symbol resolves to a same-file data object
    (see "Global variables (x86_64)"): a same-file rip-relative lea seeds a
    pointer, and a same-file rip-relative access materializes its capability
    from the data object and rides the ordinary checked path. The rejection
    fires for
    every operand position (load, store, ALU source, push/pop), FP/vector
    forms included, and covers indirect-branch memory operands uniformly:
    `jmp *myglobal` / `call *myglobal` / `jmp *0x600000` branch THROUGH an
    absolute memory operand (an un-checkable read of the branch target, not a
    direct branch to it), and `je *myglobal` has no indirect encoding at all
    (rendering would silently emit the DIRECT branch), so the `*` marker
    overrides the code-target exemption and rejects with the same messages.
    Exempt: direct branches (call/jmp/jcc to symbols and numeric local
    labels), register-indirect branches (`jmp *%rdi`), register-based
    memory-indirect branches (`jmp *(%rax,%rcx,8)` — checked like any memory
    operand; memory-indirect CALLS are instead rejected), lea (address
    arithmetic, no dereference), $imm, and plain segment-register moves.
  * WIDTH LIES: an Intel PTR size annotation that CONTRADICTS the
    ISA-determined memory access width — rejected on the heap and stack paths
    alike (`PTR size annotation (N bytes) contradicts the memory access width
    (W bytes) of '<mnem>'`), since a lying annotation would under-size the
    bounds check or the materialized stack slot; the rule also fires on the
    GPR heap path (`mov WORD PTR [rdi], rax` stores 8 bytes, not 2) and on GPR
    accesses materialized into an FP-tainted frame cluster. The exceptions are
    the narrowing exemptions (movzx/movsx/movsxd; no-GPR-operand forms like
    push/pop/call/jmp and mem-only inc/dec/neg/not) and the size-driven widths
    (x87 memory forms, cvtsi2ss/sd integer source, narrowing converts,
    source-register-determined movnti) — see the x86_64_fp.luau module entry.
    An x87 PTR form the ISA does not have (`fst TBYTE PTR`) rejects with
    `<mnem> has no <N>-byte memory form` rather than rendering a nonexistent
    instruction;
  * FP/SIMD stack accesses needing more than 16-byte alignment (see the frame
    section).
  `long double`/vector signature types are rejected on both architectures;
  float/double signature types ARE supported on both (see above).

- On X86_64, an AT&T-style parens operand field in Intel-syntax input is
  rejected ("AT&T-style operand in Intel-syntax input (use [bracket] memory
  operands)") — see the `*_parse.luau` module entry for the rationale (such a
  field would otherwise re-render verbatim as AT&T memory syntax with Intel
  dest-first operand order and NO capability/bounds check).
- Intel-syntax lowercase `ptr` (e.g. `qword ptr [rbp-8]`) mis-parses as a
  symbolic displacement and rejects — uppercase `PTR` is required (a known
  parser gap; gcc emits uppercase, so real compiler output is unaffected).
- X86_64 output is always AT&T syntax, even when the input is Intel syntax.
## Global variables (x86_64)

x86_64 sarcasm turns same-file data and annotated extern references into real
Fil-C globals, mirroring what FilPizlonator emits for a C global (the emission
was verified byte-for-byte against `clang -O2 -S` output). arm64 keeps the
historical rejections (the top-level data scan and the forged-address checks).

- **Data objects (DOs).** The driver collects contiguous top-level data under
  `.section .rodata*`/`.data`/`.bss` (plus `.comm`/`.lcomm`) into DOs: one DO
  per maximal contiguous block of LOCAL data (labels map to payload offsets, so
  "lea a base label and read past a sub-label" keeps working — including any
  UNLABELED data ahead of the block's first label, e.g. keccak1600's `.align
  256` + eight zero quads before `iotas`, whose leading bytes position the label
  at the exact input offset), one DO per EXPORTED (`.globl`/`.comm`) label with
  extent to the next label (C-side bounds are exact; an exported segment starts
  at its label, so leading unlabeled data drops there like before). Only scalar
  initializers: `.byte/.short/.word/.value/
  .long/.quad` (numeric), `.float/.double`, `.ascii/.asciz` (decoded and
  re-emitted as `.byte` lists so the computed payload size is exact by
  construction), `.zero/.space/.fill`, and `.p2align`/`.align` padding.
  `.quad label`-style pointer initializers (constant relocations) are a clean
  compile-time rejection; `.equ`/`.set`/`.macro` stay rejected everywhere;
  in-body data stays rejected. Truly unreferenced, non-global blocks drop (the
  dead-content rule).
- **Emission** per DO (in `.data.rel.ro` when readonly, `.data` when writable):
  `.quad <DO>+total` (upper), `.quad 0+flags` (ObjectFlagGlobal at bit 48,
  plus ObjectFlagReadonly at bit 49 for const), payload at DO+16 — the payload
  base IS the capability lower, and the statically laid-out header makes the
  access checks work with no runtime registration. An alignment wider than 16
  is handled with FilPizlonator's leading-pad trick (`.zero A-16` before the
  header; the payload keeps its alignment). A synthesized `.Lglobalbase_<sym>:`
  label marks the payload base. Writable DOs additionally get a
  `.LpizlonatedGP_<sym>` flight-pointer cache cell (`.comm …,16,16`) and a
  `.LpizlonatedGS_<sym>` slow-init function (verbatim clang shape:
  `filc_global_initialization_start(myth, origin, &GP, &DO)` registers the
  object into the GC's global root set, `filc_global_initialization_end` fills
  the cell). Exported labels additionally get the `.globl pizlonated_<sym>`
  getter (verbatim clang shape; `(rdi=myth, rsi=origin or NULL)` ->
  `rax=intval, rdx=lower`) and the unsafe-export alias `.globl <sym>` +
  `.set <sym>, pizlonatedDO_<sym>+16` (`.hidden` propagates to getter and
  alias). Readonly LOCAL data emits only the DO (a readonly object can never
  receive a pointer store, so it never needs registration). `.comm
  name,size,align` becomes its own writable DO of `size` zero bytes, exported
  (a documented semantic change: commons become strong definitions and lose
  cross-module merging).
- **Access lowering, same-file symbols** (fully automatic — inferred from the
  collected data, no annotation):
  * `leaq sym(%rip), %r` (also `sym+K`) is a pointer SEED: iv = the lea
    (rendered verbatim) and lo = a synthesized `leaq .Lglobalbase_<sym>(%rip)`
    for readonly data, or the inline-GP materialization for writable data
    (mirroring filcc's same-module fast path: load the GP cell's two words,
    test both for zero, on zero call the GS slow-init — which registers the
    object — and `leaq pizlonatedDO_<sym>+16(%rip)` into both words). Indexed
    accesses through the seeded register are ordinary checked accesses (x86 has
    no rip+index addressing, so table traffic is always lea-then-index).
  * A DIRECT rip-relative access (`movl g+4(%rip), %r`,
    `movdqa K256+512(%rip), %xmm7`, an ALU-source/RMW form, ...) keeps the
    instruction verbatim and gets a synthesized effective-address lea, a
    synthesized payload-base lea, and the ordinary access check at the
    instruction's width/alignment; a store to a readonly table traps in the
    check's CanWrite test ("cannot write to read-only object."), exactly like
    a store to a const C global.
  * `#! load ptr` on a same-file global lowers as the scalar form (an
    unregistered object's aux is 0 -> null lower, which is correct: nothing
    was ever stored into it with a capability). `#! store ptr` and the atomic
    pointer family on a WRITABLE global force REGISTRATION first (the inline-GP
    materialization), then run the standard invisicap sequences; on a readonly
    global they trap in the sequences' CanWrite check.
- **Extern symbols: `#! global ptr`.** On a rip-relative lea or any
  rip-relative memory access to a symbol NOT defined in the file, the
  annotation materializes the flight pointer by calling
  `pizlonated_<sym>@PLT` (`rdi=myth, rsi=0` -> `rax=iv, rdx=lo`) — an injected
  nounwind runtime call (caller-saved clobbers, fpSave/fpRestore, counterGuard;
  NO exception branch, NO root store: a global's lower is a link-time constant
  into static ELF memory and FUGC never moves it). The lea form seeds (iv, lo)
  as a pointer; the direct form then runs the checked access through
  (iv+disp, lo), with the memory operand REWRITTEN to the materialized
  effective address (register-indirect — the verbatim rip-relative form would
  need the defining module's unsafe-export alias at link time, while the
  getter always returns the true payload address). `#! load ptr` /
  `#! store ptr` / the atomic pointer family keep their spellings on extern
  globals; the rip-relative operand shape implies the getter materialization
  (`#! store ptr` & co. run the invisicap sequences against the registered
  object). An unannotated rip-relative reference that resolves to nothing
  rejects with a message pointing at `#! global ptr`, and so does an
  unannotated rip-relative lea to anything but a body-local code label.
- **Validation.** `#! global ptr` is shape-checked (a lea or memory operand of
  the form `sym(%rip)`, no `@reloc` decoration) and arch-gated to x86_64; on
  arm64 it is an unrecognized annotation like before. The driver's collector
  and the transform's globals table are threaded through
  `transformFunction(..., { globals = ... })` into `computeAddr`, which
  resolves same-file dispSyms to their data object's payload base; ptrflow
  needs no change (the lea seeds ride `loadPtrDefs`).

## Function references and constructor sections (x86_64)

- **`#! funcref`.** On a rip-relative lea to a FUNCTION symbol, materialize
  the function's flight pointer (the poly1305_init function-table shape). A
  raw lea of a code address is not a Fil-C flight pointer — storing it would
  fail the indirect call's FUNCTION-type check — so the lea becomes a
  capability seed in ptrflow (a `loadPtrDefs`-style seed, no GC root store:
  the lower is a link-time constant into static ELF memory). A same-file
  function (or alias entry — the driver's `fnNames` map resolves alias
  entries to their owners) retargets the lea to the function object:
  `leaq pizlonatedFO_<owner>+16(%rip)` into both the intval and the lower,
  exactly the getter's output (intval == lower == FO payload, the canonical
  entrypoint of the in-object header). An extern function goes through its
  cross-module getter (`pizlonated_<sym>`, an injected nounwind runtime call
  like the `#! global ptr` path). Shape validation (validateBody: a lea of
  the form `sym(%rip)`, no reloc/index/displacement) and target resolution
  (a data object, body-local code label or local subroutine is a clean
  rejection) happen in the transform; arm64 rejects the annotation with a
  clean "not yet supported". cmov selection chains over the seeded pointers
  are handled by ptrflow's cmov support: a cmov merging two pointer webs
  with different lowers widens the destination web to a dynamic (lockstep)
  lower and the transform emits a CONDITIONAL lower copy — a cmov on the
  lower temps with the node's own condition — immediately after the iv cmov
  (nothing between the two writes the flags).
- **`.section .init` / `.section .fini`.** Driver-level (outside functions):
  the top-level scan tracks the current section; inside `.init`/`.fini` a
  straight-line sequence of annotated DIRECT calls is collected (labels,
  data directives, non-call instructions, unannotated calls and data-object
  or local-subroutine targets are clean rejections; on arm64 the whole thing
  is "not yet supported"). Each call must annotate `void()`: the .init
  context runs from `_init` as plain SysV code with no Fil-C argument state,
  so only void() calls marshal soundly. Emission (x86_64 glue) writes the
  calls back into the same section in input order, marshalled as
  `filc_defer_or_run_global_ctor(<callee flight pointer>)` — exactly what
  filcc emits for a C constructor's `.Lfilc_ctor_forwarder`, which handles a
  .init chunk that runs before the runtime is initialized by deferring to
  before-main. A same-file function's flight pointer is
  `leaq pizlonatedFO_<owner>+16(%rip), %rdi; movq %rdi, %rsi`; an extern
  function resolves through `pizlonated_<sym>@PLT` (rax/rdx) first. The
  fragments make their calls with rsp exactly as found (`_init`'s crti
  prologue leaves rsp 16-aligned at fragment entry).

## Local subroutines (x86_64)

OpenSSL's perlasm calls file-local subroutines with custom caller/callee
register conventions (`call _aesni_encrypt2` on a non-`.globl`, non-signature
`.type` label that ends in `ret` — ~70 subroutines corpus-wide). A local call
is an unconditional jump to a per-caller CLONE of the subroutine; a local
`ret` is a multi-way branch to the continuations (the instructions after each
call). `localcall.luau` holds the shared discovery + clone augmentation; the
per-arch hooks are in x86_64_isa (classify), x86_64_frame (successors, the
ret exemptions), and the transform (retaddr temps, emission, flag treatment).

- **Discovery.** Regions are maximal spans of unconsumed top-level statements
  headed by a non-`.globl`, non-signature label, bounded like splitFunctions'
  body termination (`.size`, section switch, `.type OTHER,@function`,
  consumed content); `.byte` sequences in a region are decoded first (the
  rep-ret at the end of every OpenSSL sub is then a real `ret`). A label in a
  region that is referenced by an unannotated or `#! local` call from a
  function body — transitively, from an already-claimed subroutine — is a
  local subroutine. A call target may be a MID-LABEL inside a region
  (`.Lenc_loop6` inside `_aesni_encrypt6`): the clone range is then
  [mid-label .. the region's ret(s)]. Claimed regions are marked consumed (an
  uncalled prefix ahead of a claimed mid-label is dead content, never
  cloned); everything else keeps today's behavior — the no-signature error
  for an uncalled `.type` function (`.globl` no-signature always errors), the
  unannotated-call rejection for anything else, and the top-level scan for
  uncalled regions. `#! local` on a call that does not resolve to a local
  subroutine is a compile error. On arm64 a discovered local subroutine is a
  clean "not yet supported" error.
- **Validation at discovery.** Recursion (direct or mutual) is rejected — the
  local-call graph must be acyclic (each clone's continuations are fixed at
  compile time, so a re-entrant activation could never find its inner
  continuation). Every branch in a clone range must stay inside it (a branch
  out is a cross-function jump — a separate feature), and no control-flow
  path may fall off the range's end (the clone would fall into the next
  appended clone with a garbage retaddr; the driver-level fall-off check
  treats a fall-through INTO a clone entry as a fall-off too).
- **Augmentation (per caller, in sorted-sub-name order — determinism).** Each
  clone's labels are renamed (`.Llocalsub_<fn>_<sub>_<orig>`, bumped past
  user label names); each callsite is marked `st.localCall` (with its
  continuation label inserted after it) and each clone `ret` `st.localRet`
  (with the clone's full continuation list, in augmented-body order). Clones
  are appended to the caller's body, so lift/ptrflow/DCE/regalloc color
  caller+clones as ONE function: reaching-definitions unions the sub's
  register effects with the caller's webs across the call boundary — the
  "one function for regalloc" property (the caller's callee-saved webs flow
  through; a caller-saved register the sub clobbers reads back with the sub's
  value, exactly the hardware clobber semantics).
- **The +8 rule.** The stack pointer is never moved (no hardware push).
  Hardware `call` pushes an 8-byte return address, so a subroutine written
  against the real ABI sees the caller's `0(%rsp)` as its own `8(%rsp)` — the
  return-address-compensated convention eight OpenSSL subs use (rsaz
  `__rsaz_512_mul/mulx/reduce/reducex`, mont5
  `mul4x_internal/mulx4x_internal`, aesni-gcm `_aesni_ctr32_ghash_6x`). Clone
  statements are marked (`st.localClone`), and the frame pass keys their
  rsp-relative accesses at model depth `d` to frame offset
  `disp + D0 - d - 8` (hardware: the return-address word sits between the
  clone's rsp and the caller's frame — at the callsite depth, every
  regs-only sub, this is exactly `disp - 8`). The same bias applies in the
  frame rewrite's static-region lookup and slot keying, in the transform's
  region-lea seed scan, and in the codegen's region redirect (all riding
  `fctx.callerDepth` = D0), so the sub's `8(%rsp)` keys to the caller's slot
  0 and `leaq 8(%rsp),%rdi` (rsaz's frame-address idiom) keys to region
  offset 0, which the redirect resolves to the region pointer when the
  caller's buffer sits in the promoted frame-escape region. rbp-relative
  operands are never biased (the call does not move rbp);
  the entry-rsp-parking lea forms shift by the same 8 (a clone's
  `leaq d+8(%rsp)` parks the entry rsp). A clone may set up its own frame
  with a constant `sub` (the gf2m `_mul_1x1` tab frame): a mid-function
  constant adjustment at a statically known depth keys perturbed slots
  exactly (disp + D0 - d, biased by the same -8), drops like a top-level
  frame, and spills into the caller's synthesized frame — the local-ret
  state override below resumes the continuation at the callsite depth.
- **Emission.** At each callsite: `leaq retaddrTemp, cont(%rip)` + `jmp
  entry` (the lea is elided for a single-continuation clone, whose dispatch
  is a plain `jmp`). One shared "retaddr" temp per (caller, clone) — an
  ordinary regalloc web, never a pointer, so the return-address storage lives
  in a register or a sarcasm-owned spill slot the program cannot address
  (off-limits by construction). Each clone `ret` emits a compare-chain
  dispatch over the clone's continuations (`leaq scratch, contK(%rip); cmpq
  scratch, T; je contK` … final `jmp contLast`). Being live from the callsite
  through the whole clone, the temp survives annotated calls inside the clone
  through ordinary IRC interference.
- **classify (x86_64_isa).** A marked localCall classifies as `branch` to the
  clone entry (no fall-through); a marked localRet as `branch` to the clone's
  continuations — so validateBody's reachability (a fall-through into a clone
  entry is a fall-off), the lift's CFG/reaching-definitions, DCE, and
  regalloc's buildCFG all see the true control flow with zero changes.
- **Frame pass (x86_64_frame).** `successors` reaches through the marks
  (clone code executes at the caller's depth, so its stack traffic keys to
  the same slot webs — with the -8 bias applied by the offset math); a marked
  localRet is exempt from the depth-0-at-ret check and from the %rax-carrier
  escape check (it is not a return — the continuation resumes at the caller's
  depth, and a phantom carrier flowing to it is the caller's recovery working
  as intended); a clone's own teardown span may terminate at a localRet (the
  clone's own epilogue leads to it exactly like a top-level ret); a localCall
  inside a teardown span makes it unverifiable. **Local-ret state override:**
  a local ret resumes its continuation with the CLONE ENTRY's
  depth/saves/frame-pointer state — the clone's own frame machinery (its
  pushes, its frame sub/add) is dropped exactly like a top-level frame, so
  the real rsp never moved inside the clone, and the continuation executes at
  the callsite's depth (the smap keeps the clone's flow, so carrier
  redefinitions inside the clone still poison the caller's carriers). The
  depth fixpoint re-enqueues a clone's local rets whenever the clone entry's
  state changes, so the override always evaluates against the converged entry
  state.
- **Flags.** Hardware `call` does not write the flags, so the program's flags
  flow INTO the clone (a local call is transparent to flag liveness) and the
  clone may read them before defining them (the rsaz/mont5 carry chains do);
  a local RET clobbers them (the ret dispatch's compare chain), so consumers
  past the return do not keep incoming flags alive. The shared
  per-flag·liveness fixpoint models this: a local call's successors are the
  clone entry and the fall-through, a local ret defines all flags with the
  clone's continuations as successors.
- **Pollchecks.** A clone's ret edge to an earlier continuation is a back
  edge, so the continuation gets a pollcheck — sound (that IS the loop back
  edge for a call inside a loop) at a minor cost on non-loop callsites.

## Cross-function jumps (x86_64)

OpenSSL's perlasm tail-branches between functions: dispatcher functions
capability-check and then `jnz variant` / `jmp variant` to same-file
ISA-variant bodies or exported functions (56 entry-adjacent dispatch jumps
corpus-wide), and a handful of functions jump into the MIDDLE of a sibling's
tail, sharing register state (x25519 `.Lreduce51`/`.Lreduce64`, ecp
`.Lpoint_double_shortcut[qx]`, chacha `.Ldo_sse3_after_all`, mont5
`.Lsqr4x_sub_entry`). `tailcall.luau` implements both mechanisms plus alias
entry labels; the localcall hooks carry the subroutine-internal case. All of
it is x86_64-only: on arm64 an annotated jump reaches validateBody's
annotation walk, which rejects it with a clean "not yet supported" error, and
an unannotated one keeps the plain tail-call rejection.

- **B1 — tail branch to a signatured function entry → call + epilogue jump.**
  A direct `jmp target` / `jcc target` whose target resolves to a function
  WITH a signature — a same-file sig-annotated function label (directly or
  through an alias, below) or an EXTERN symbol carrying an inline callsite
  annotation (`jne asm_AES_cbc_encrypt #! void(ptr,ptr,size_t,ptr,ptr,int)`)
  — is rewritten before validateBody into an ordinary annotated CALL to that
  function (the full Fil-C CC marshalling of the current SysV arg-register
  webs, exactly like any annotated call — the webs trace to the jumper's own
  entry unpacking, correct by construction for the corpus dispatch shapes,
  which jump before the argument registers are repurposed), immediately
  followed by a jump to the function's shared synthetic epilogue block
  (`.Ltailret_<fn>: ret`). A conditional jump synthesizes a via block:
  `jcc .Lvia_K; <fallthrough>; .Lvia_K: call …; jmp .Ltailret_K`. The
  callee's return value becomes the jumper's return value: the call's result
  web reaches the epilogue's `ret`, whose ordinary return path moves it into
  the fast-CC return registers (pointer-returning jumpers may only tail-call
  pointer-returning callees — a clean error otherwise; every other
  return-shape combination is hardware-faithful). The jump site may sit at
  any virtual frame depth: the body's stack is virtual, so the synthesized
  epilogue (which owns the real rsp) tears down correctly regardless — the
  synthetic `ret` carries the `st.tailRet` mark, exempting the frame pass's
  depth-0-at-ret and %rax-carrier proof gates (sound, since the return value
  is the callee's result, defined by the call). Dense argument words beyond
  the four fast-CC registers travel on the stack at the call; for a tail
  call those words must be the jumper's OWN incoming SysV stack arguments (a
  hardware `jmp` passes them in place), so the synthetic call is marked
  `st.tailCall` and the marshalling sources its 7th+-argument webs from the
  yolo incoming-argument slots instead of the outgoing-argument frame slots
  (transform's `callArgSource`). validateBody's annotation walk validates the
  form (a sig-annotated jump must be a direct jump to a bare symbol) and
  reports unresolved targets clearly ("does not resolve to a function with a
  matching signature"); an unannotated jump to a non-local label keeps the
  plain tail-call rejection.
- **B2 — mid-body shared-tail join → region cloning.** A jump (conditional
  or unconditional) to a label INSIDE another same-file function's body (or
  inside a local subroutine's region) clones the region [target label .. the
  region's ret(s)] into the JUMPER, reusing the localcall cloning style:
  renamed labels appended to the jumper's body (`.Ltailjoin_<fn>_<owner>_<orig>`,
  deterministic), the jump retargeted to the clone entry, and the clone's
  rets left as PLAIN rets — a shared-tail join returns from the jumper
  (hardware: the ret pops the jumper's own return address, a `jmp` having
  pushed nothing). The region is the statements REACHABLE from the target
  label, in source order — which may include labels BEFORE the entry
  (mont5's `.Lsqr4x_sub` loop head). There is NO -8 clone bias for B2 (the
  localcall +8 return-address compensation applies to CALL-clones only): the
  clone's stack accesses key into the jumper's frame at the jumper's depth,
  so the corpus's by-construction depth matches (x25519's identical
  prologues, ecp's `addq $416,%rsp` reshape, chacha's pre-frame-setup join)
  hold, and the frame pass's ordinary checks reject genuine mismatches.
  Local calls inside the region are cloned transitively (recorded into the
  localcall discovery's callsite list). Register state is shared by
  construction — that is the point of a mid-body join. A branch out of the
  region is a nested cross-function jump and is rejected; so is a fall-off
  or a re-entry of the owner's entry label.
- **B2 from inside a local subroutine (mont5).** localcall.discover records
  a branch out of a subroutine's clone range to a label in ANOTHER
  subroutine's region as a "tailjoin" edge (any other escaping branch keeps
  the cross-function-jump rejection), computing and validating the tail
  region; localcall.augment clones the tail into each caller of the jumping
  subroutine with the jumping subroutine's clone id — the +8 context (the
  tail executes inside the subroutine's activation, whose caller DID push a
  return address) and the ret dispatch (the tail's rets resume the jumping
  subroutine's continuations — hardware pops its return address) both ride
  the jumping subroutine's. Tail regions whose statements are never CALLED
  are claimed consumed the same way; the tails' local calls join the
  cycle-detection graph and the per-caller closure.
- **Alias entry labels.** A label immediately adjacent to a sig-annotated
  function label — nothing between them but blanks, non-data directives, and
  CET/nop markers (endbr64/nop) — names the same entry
  (asm_AES_encrypt:/AES_encrypt:, .Lenc_rounds:/Camellia_EncryptBlock_Rounds:,
  sha1_block_data_order_shaext:/_shaext_shortcut:,
  gcm_init_clmul:/.L_init_clmul:). A PRECEDING alias is consumed at
  splitFunctions (never body content); a FOLLOWING local alias stays an
  ordinary body label (branches to it from within keep working) and is
  additionally recorded for cross-function resolution; a following `.globl`
  alias is consumed like a preceding one. Jumps to the alias resolve to the
  owning function (B1). A `.globl` alias additionally gets its own getter
  and direct-call (FI) symbol as plain `.set` aliases (`pizlonated_<alias>`,
  `pizlonatedFI<sig>_<alias>`, with `.hidden` propagated) so C callers of
  the alias link — its function object IS the function's.
- **Clone-source freshness.** B2 regions are extracted from PRISTINE copies
  of the owner bodies taken at context-build time: the transform rewrites
  some memory operands in place (the frame-escape-region redirect substitutes the
  region-pointer temp for the stack base register), so a body whose owner
  was already compiled is no longer a valid clone source for a later
  jumper.

## Floating-point signatures (arm64 and x86_64)
- Capability marker: BOTH codegen modules set `cgm.fpSignatures = true`, and
  the signature class check is arch-aware only in its error spelling:
  `sig.checkSupportedClasses(sd, where, { arch = target.arch })` accepts the
  float/double classes on both architectures and rejects `long double` and the
  vector classes (the error names the offending type class and the arch), at
  entry signatures (sarcasm.luau) and callsite annotations (the backends'
  `checkCallsiteSig`) alike.
- Fast paths (entry unpack, direct calls, indirect-call fast arm, returns): FP
  arguments and results PASS THROUGH the vector registers untouched — the transform
  simply skips FP-class args in the dense GPR marshalling (the yolo→dense
  mapping counts only GPR args, so GPR args are not shifted by FP args) and
  skips the integer retval move for FP returns. On arm64 the source program
  places FP args in v0..v7 per AAPCS and reads the FP result from v0 (s0/d0).
  On x86_64 they go in xmm0..xmm7 in declaration order among the FP args —
  INDEPENDENT of the dense GPR packing rdx,rcx,r8,r9 (a GPR arg's ordinal
  counts only the non-FP args before it; the entry unpack copies the internal
  dense fast-CC registers into the author-visible yolo sequence
  %rdi,%rsi,...), and an FP return rides in xmm0 with the `ret` path still
  clearing the %al exception flag (`movl $0, %eax`) even though %rax is not
  the result register (verified signatures mirror arm64's:
  `long(double,int,float,long)` reads the double in %xmm0, the int in %rdi,
  the float in %xmm1 and the long in %rsi).
- Generic/buffer paths (indirect-call generic arm, the weak callsite resolver
  thunk, the 2ET generic thunks): FP args are stored into the CC buffer at
  their declaration index — arm64's `cg.storeFpArg` emits `str sN/dN, [myth,
  #128+8i]` (a float writes the low 4 bytes of the 8-byte slot), x86_64's
  emits `movss/movsd %xmmN, 128+8i(%myth)` — each with a zero aux word at
  384+8i; the argument byte size counts every argument word. An FP return is
  loaded back from `myth+128` after the ret-size check (expected size stays 8):
  arm64 `cg.loadFpRet` (`ldr s0/d0`), x86_64 `movss/movsd 128(%myth), %xmm0`
  (the 2ET thunk additionally zeroes the aux word at `384(%rbx)` on the way
  out). The callsite thunk keeps FP args live in the vector file
  across its getter call (nothing in the thunk touches the v/xmm registers)
  and stores them into the buffer straight from there on the mismatch path.
- Frame interaction: FP values ride in vector registers even when the body
  never names one, so the vector-save reservation is FORCED whenever any
  signature in play (entry or callsite) has a float/double class — otherwise
  an injected runtime call's liveness-driven vector saves would not bracket
  the clobbered vector registers (gated on `cgm.fpSignatures`; both backends
  force it — see the frame section's reservation discussion).
- x86_64 specifics: call nodes carry `fpVecUses` so the width-aware backward
  liveness keeps an FP argument web live from its definition up to the call
  (see the frame section); and FP signatures push signature numbers past a
  32-bit immediate (eight doubles encode to 8552919316), so both
  signature-compare sites widen — the shared codegen hook
  (`cmpImmBranchWidened`) materializes the constant with `movq $imm64, %reg`
  (the movabs encoding) into a freshly allocated temp and compares registers,
  and the hand-written resolver glue does the same into the dead scratch
  `%r10` (arm64 needs none of this: its `cmp` immediate uses the existing
  chunked movz/sub-cmp/movk widening).

## Verification

Tests live under `filc/tests/` (yolo inputs named `sarcasm*`); from the repo
root, `filc/run-tests -f sarcasm` runs the whole suite and `-t <name>` runs one
test. Each input is assembled, linked with a Fil-C `main`, run, and its results
asserted; OOB and null-capability inputs must trap with the exact first-line
`filc safety error` message, and unsafe inputs must be rejected at compile
time. Extension-dependent tests carry manifest keys (`needsAVX512`,
`needsARMCrypto`, `needsLSE`, `needsFP16`) probed at startup and skip cleanly
when the hardware lacks the feature. Transform output is deterministic across
processes (see the determinism invariant in the transform section).

