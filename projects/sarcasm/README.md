# sarcasm — SAfe Runtime Capability-enforced Assembler

sarcasm is an assembler (written in Luau, run via `lute`) that takes ARM64 or X86_64
assembly written to link with **Yolo-C** and rewrites it into **memory-safe** assembly
that links with **Fil-C**. It performs the GIMSO transformation on pointers, changes the
pointer representation to invisicaps, reallocates registers with Iterated Register
Coalescing, and emits code that follows the Fil-C ABI (calling convention, safepoints,
stack-overflow checks, GC roots). It supports **ARM64** (aarch64) and **X86_64** (both
AT&T and Intel input syntax; the target is auto-detected from the input), and rejects
anything it cannot prove safe rather than passing unsafe code through.

## Usage

```
./sarcasm.sh [-o OUT.o] [-S|--no-assemble] [--as CMD] INPUT.s
```
- Default: writes a temporary `INPUT.yolo.s` next to the output and runs `as` to produce
  `OUT.o` (like `as`). `--as CMD` overrides the assembler (default `as`).
- `-S` / `--no-assemble`: emit assembly only. Without `-o`, writes `INPUT.yolo.s`.

Input assembly must carry `;!` annotations:
- on each exported function label: its Fil-C signature, e.g. `hash: ;! unsigned(ptr)`;
- on each instruction that loads a pointer from memory: `;! load ptr` (`;! store ptr` for stores);
- on each call: the callee's signature, e.g. `bl foo ;! int(ptr, size_t)`;
- on stack allocations: `;! alloca size (x)` on the `sub sp,...` and `;! alloca result (x)`
  on the instruction that yields the pointer (dynamic), or `;! alloca result size=N` for a
  fixed-size buffer. These become GC allocations (`filc_allocate`), not real stack memory.

Annotations are mandatory and checked: an unannotated call, an unknown or misplaced
annotation (e.g. a signature on a non-call, or an annotation on a directive, blank line,
or mid-body label), or a `;! load/store ptr` on anything but a 64-bit scalar access is a
compile-time error, as is anything else sarcasm cannot prove safe — indirect calls or
branches through raw registers, tail calls, globals/data sections/literal pools, and
floating-point types in signatures (FP values are not marshalled). See **DESIGN.md**'s
"Limitations" section for the full list.

See `tests/test-spasm.s`, `tests/test2-spasm.s`, `tests/test3-spasm.s`, `tests/test3-spasm0.s`
for ARM64 examples, and `tests/test-spasm-x86.s` / `tests/test-spasm-x86-intel.s` for x86_64.

## Pipeline (`sarcasm/`)
Shared, architecture-independent modules:
- `detect.luau`  — auto-detects the target (arm64 vs x86_64; att vs intel syntax).
- `sig.luau`     — Fil-C signature encoding (`1 + Ret + 133*Arg`).
- `frame.luau`   — drops the input frame; virtualizes stack slots as locals.
- `lift.luau`    — lifts to a virtual-register IR via reaching-definition **register webs**.
- `ptrflow.luau` — pointer-flow analysis: which temps are pointers + their `lower` temps.
- `transform.luau` — the GIMSO transform: intval/lower doubling, invisicap loads and
  **stores (with aux allocation + FUGC store barriers)**, offset- and
  **register-index-aware** access checks, **null-capability trapping** for non-pointer
  heap accesses, Fil-C calls (marshalling + exception propagation), pollchecks at loop
  headers, SOV check, frame push/pop, GC roots.
- `regalloc.luau` — Iterated Register Coalescing (Appel & George) over the GPR file.
- `emit.luau`    — renders IR with the allocation; synthesizes prologue/epilogue.
- `build.luau`   — constructors for synthesized IR nodes.
- `sarcasm.luau` — the driver (function splitting, orchestration, `as` invocation).

Per-architecture backends (`arm64_*` / `x86_64_*` pairs behind a common interface):
- `*_parse.luau`   — GNU/clang assembly parser (+ `;!` annotations); x86_64 handles both
  AT&T and Intel syntax.
- `*_isa.luau`     — instruction semantics: register def/use, control flow, RMW.
- `*_frame.luau`   — per-arch frame policy for the shared frame preprocessor.
- `*_codegen.luau` — per-arch instruction emitters used by the shared transform.
- `*_render.luau`  — per-arch rendering + prologue/epilogue synthesis for emit.
- `*_glue.luau`    — getter, FO object, generic entrypoint thunk, origins, alias, and the
  **weak callsite resolver thunk** for called externals.

## Safety model for memory accesses
- **Frame accesses** (`sp`/`x29`-relative on ARM64, `rsp`/`rbp`-relative on X86_64,
  within the input frame) are virtualized as register-allocated locals — no capability
  needed. Accesses outside the frame, or writes into the caller's argument area, are
  **rejected at compile time**. Taking the address of the stack frame is also rejected,
  as are register-indexed frame accesses, base-writeback forms outside the callee-saved
  prologue/epilogue pairs, dynamic sp moves without an alloca annotation, and FP/SIMD
  registers in frame ops. Sub-width frame accesses ARE supported (virtualized with
  explicit extension/insertion, e.g. `strb` → `bfi`, `ldrsb` → `sxtb` on arm64).
- **Heap accesses** are bounds-checked against a capability: the base's `lower` if the
  base is a pointer, else the index's `lower` (base wins if both are pointers), else a
  **null capability** — which traps at runtime (`cannot ... with null object`).
- **Pointer stores** additionally reject read-only/special objects, ensure the aux
  capability array exists, and run the FUGC store barrier when marking is active.

## Testing
`tests/verify.sh` (ARM64) and `tests/verify-x86.sh` (X86_64) are the comprehensive
suites (run from the repo root; they use `lute` on the host and `as`/clang via docker):
they compile every yolo input with sarcasm, assemble with **GNU `as`**, link with Fil-C
`main`s, and check results, out-of-bounds/null-cap **traps**, the pointer-store
capability round-trip, register-indexed access, the callsite resolver, the presence of
every inserted check (structural), and every compile-time rejection. The X86_64 suite
exercises every behavioral case in BOTH AT&T and Intel syntax. `tests/run-all.sh` is the
original four-file ARM64 subset. The compile-time rejections are also covered, on BOTH
architectures, by `filc/tests/sarcasm-reject-*-arm` / `-att` / `-int` (run via
`filc/run-tests`).

`tests/roundtrip-test.luau` checks that lift + identity-coloring reproduces the input
byte-for-byte (validates the register-web machinery); `tests/detect-test.luau` covers
the arch/syntax auto-detection and `tests/cleanup-test.luau` the spill reload
elimination.

See `ABI-NOTES.md` / `ABI-NOTES-x86.md` for the decoded Fil-C ABIs and `DESIGN.md` for
the architecture.
