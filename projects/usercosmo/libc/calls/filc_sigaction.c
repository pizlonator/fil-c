/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8     -*-│
│ vi: set noet ft=c ts=2 sts=2 sw=2 fenc=utf-8                             :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Fil-C port of Cosmopolitan libc: signal handler installation.                │
│                                                                              │
│ cosmo's sigaction() routes through its own multi-OS metasigaction            │
│ machinery: rva-relative handler tables in the PIB, __sigenter_* assembly     │
│ wrappers, and the __restore_rt sigreturn trampoline.  None of that can       │
│ exist in pizlonated code.                                                    │
│                                                                              │
│ Under Fil-C, handlers are installed via libpizlo's zsys_sigaction(), which   │
│ wraps every user handler into libpizlo's signal_pizlonator trampoline        │
│ (yolo side) — the trampoline re-enters the pizlonated handler with a valid   │
│ siginfo, so no restorer or wrapper is needed on this side.  The struct       │
│ sigaction layout here is cosmo's kernel-shaped one, which is exactly what    │
│ filc_native_zsys_sigaction() expects.                                        │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/calls/calls.h"
#include "libc/calls/struct/sigaction.h"
#include "libc/dce.h"
#include "libc/errno.h"
#include <stdfil.h>
#include <pizlonated_syscalls.h>

int sigaction(int sig, const struct sigaction *act, struct sigaction *oldact) {
  return zsys_sigaction(sig, act, oldact);
}
