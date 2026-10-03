/*-*-mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8-*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
╞════════════════════════════════════════════════════════════════════════════╡
│ Fil-C port: C replacement for libc/calls/islinux.c (excluded from the      │
│ Fil-C build because it issues a raw `syscall` inline asm — the Fil-C        │
│ compiler panics on that mnemonic at runtime).  Nothing under Fil-C can      │
│ query seccomp support that way, and the answer only ever gates workarounds  │
│ for Linux < 2.6.23, which is ancient history; just say yes.                 │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/dce.h"

bool __is_linux_2_6_23(void) {
  /* IsLinux() is a compile-time constant with SUPPORT_VECTOR=1; the caller
     (utimens.c, backtrace2.c, cocmd.c) has already checked IsLinux(). */
  return true;
}
