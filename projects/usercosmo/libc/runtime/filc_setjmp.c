/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8     -*-│
│ vi: set noet ft=c ts=2 sts=2 sw=2 fenc=utf-8                             :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Fil-C port of Cosmopolitan libc: memory-safe setjmp/longjmp.                 │
│                                                                              │
│ Cosmo's setjmp family is hand-written assembly (libc/nexgen32e/*.S) that     │
│ stashes callee-saved registers in the jmp_buf.  None of that is compiled     │
│ here and none of it can work under Fil-C: pizlonated code is transformed by  │
│ the FilPizlonator, where setjmp/longjmp are compiler/runtime intrinsics      │
│ (exactly like in the musl flavor — see projects/usermusl/src/setjmp/):       │
│                                                                              │
│   - Calls to setjmp/_setjmp/sigsetjmp are recognized BY NAME and lowered     │
│     into filc_jmp_buf_create() plus a real _setjmp() into the runtime's      │
│     system_buf; the resulting filc_jmp_buf* is stored into the first word    │
│     of the user's jmp_buf.  No definition of the setjmp family is needed:    │
│     the call is erased, so no object ever references the symbol.  (For this  │
│     to work the jmp_buf just needs a pointer-sized slot at offset zero,      │
│     which cosmo's `long jmp_buf[8]` / `long sigjmp_buf[11]` have.)           │
│                                                                              │
│   - zlongjmp() (libpizlo) performs the memory-safe unwind: it verifies that  │
│     the destination frame is still live and restores GC/deferral state       │
│     before jumping into the system_buf.                                      │
│                                                                              │
│ So all this file provides is the longjmp side: fish the filc_jmp_buf* out    │
│ of jmp_buf[0] and hand it to zlongjmp().  siglongjmp() already funnels into  │
│ _longjmp() (libc/runtime/siglongjmp.c), and sigsetjmp()'s sigmask saving is  │
│ done by the pizlonator itself (the `savemask` argument is the `value` of     │
│ filc_jmp_buf_create), so cosmo's __sigsetjmp_tail() stays unused.            │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/runtime/runtime.h"
#include <pizlonated_runtime.h>

_Noreturn void longjmp(jmp_buf env, int val) {
  zlongjmp(*(zjmp_buf **)env, val);
}

_Noreturn void _longjmp(jmp_buf env, int val) {
  zlongjmp(*(zjmp_buf **)env, val);
}
