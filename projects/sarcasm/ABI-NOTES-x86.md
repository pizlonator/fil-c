# Fil-C 0.683 x86_64 ABI notes (decoded from clang output)

The ABI *concepts* are identical to ARM64 (see ABI-NOTES.md): invisicap (intval,lower),
object header at [lower-16]=upper / [lower-8]=aux, myth offsets ([+0]=stack limit,
[+8]=flags, [+16]=top frame, [+128]=CC payload buffer, [+384]=CC aux buffer), filc_frame
{prev, origin, roots}, signature encoding, FO layout, 0x83<<48 function flags, function
origins with can_throw=1/can_catch=1 (C++ exceptions propagate through sarcasm frames).
Only the registers and instructions differ.

## Register calling convention (fast entrypoint / direct call)
- `%rdi` = myth, `%rsi` = function object (FO payload ptr).
- args: packed DENSELY into `%rdx`,`%rcx`,`%r8`,`%r9` — a pointer-class arg occupies two
  consecutive registers (intval, lower), a scalar-class arg one (intval only). E.g.
  `(ptr, size_t, size_t)` = arg0 in rdx/rcx, arg1 in r8, arg2 in r9. Arguments beyond
  the fourth word are passed on the stack (verified against clang output: a 3rd pointer
  arg arrives at 8+16N(%rsp) after the callee's frame). sarcasm does not marshal stack
  arguments and rejects signatures needing more than 4 register argument words.
- return: `%al` bit0 = exception flag; `%rdx` = ret intval; `%rcx` = ret lower.

## Runtime-call argument registers (SysV: rdi, rsi, rdx, rcx, r8, r9)
- `filc_optimized_access_check_fail(rdi=intval, rsi=lower, rdx=&origin)`
- `filc_pollcheck_slow(rdi=myth, rsi=&origin)`
- `filc_allocate(rdi=myth, rsi=sizeBytes)` -> `%rax`; usable payload = `%rax + 16`
- `filc_object_ensure_aux_ptr_outline(rdi=myth, rsi=lower-16)` -> `%rax`
- `filc_store_barrier_for_lower_slow(rdi=myth, rsi=lower)`
- `filc_cc_args_check_failure(rdi=size, rsi=expected, rdx=0)` / `filc_cc_rets_check_failure`
- calls use `@PLT`; globals via `leaq sym(%rip), %reg`; return `retq`; indirect `callq *(%rsi)` / `jmpq *%rax`

## Registers
16 GPRs, encoding order: rax=0 rcx=1 rdx=2 rbx=3 rsp=4 rbp=5 rsi=6 rdi=7 r8..r15=8..15.
Sub-registers: e** (32), ** (16), *l/**l/r*b (8). Callee-saved: rbx, rbp, r12-r15.
Caller-saved: rax, rcx, rdx, rsi, rdi, r8-r11. Reserved: rsp (stack), rbp (frame ptr).

## Prologue / SOV / frame (from pizlonatedFIP)
```
pushq %rbp ; pushq %r15 ; ... ; pushq %rbx      # save used callee-saved
subq $N, %rsp                                    # frame
cmpq %rsp, (%rdi) ; jae filc_stack_overflow_failure@PLT   # SOV (rdi=myth at entry)
movq 16(%rdi), %rax ; movq %rax, (%rsp)          # frame.prev
movq %rsp, %rax ; movq %rax, 16(%rdi)            # myth->top_frame = &frame
leaq .Lfilc_origin(%rip), %rax ; movq %rax, 8(%rsp)   # frame.origin
# roots at 16(%rsp), 24(%rsp), ...
```
Epilogue: `movq (%rsp), %rax ; movq %rax, 16(%rdi)` (pop frame) ; restore ; `addq $N,%rsp` ;
`popq` in reverse ; `xorl %eax,%eax` (flags) ; `movq <ret>, %rdx` ; `retq`.

## Access check for N bytes at (intval=%iv, lower=%lo)
```
testq %lo, %lo ; je FAIL                     # null cap
cmpq %lo, %iv ; jb FAIL                        # iv < lower
cmpq -16(%lo), <iv+N> ; ja FAIL   (or: iv vs upper with jae for N==1)   # iv+N > upper
testb $(align-1), %ivb ; jne FAIL              # alignment
```
FAIL: `leaq origin(%rip),%rdx ; movq %iv,%rdi ; movq %lo,%rsi ; callq filc_optimized_access_check_fail@PLT`

## Pollcheck / store barrier (same structure as ARM64, x86 encodings)
```
testb $14, 8(%rdi) ; jne slow                  # myth flags
... callq filc_pollcheck_slow@PLT ...
```
Store barrier: `movq filc_current_marking_state@GOTPCREL(%rip),%r; cmpl $0,(%r); jne slow` etc.

## Callsite resolver (pizlonatedFI<sig>_NAME, weak/hidden)
`xorl %esi,%esi; callq pizlonated_NAME@PLT` -> rax=iv, rdx=lower of FO; validate flags
`movabsq $0x780000000000000` & `== $0x80000000000000`; intval `== [FO-8]&0xFFFFFFFFFFFF`;
signature `cmpq $SIG, 16(FO)`; direct: `movq (FO),%rax; <restore CC>; jmpq *%rax`; else
marshal to CC buffers and call generic entrypoint. Fail: filc_check_function_call_fail.

## Data directives
`.quad` (= .xword), `.long` (= .word), `.zero`, `.byte`, `.asciz` — same layout as ARM64.
`movabsq $imm, %reg` for 64-bit immediates; `endbr64` (CET marker; safe to keep/ignore).
