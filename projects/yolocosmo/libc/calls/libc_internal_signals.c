/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8   -*-│
│ vi: set noet ft=c ts=8 sw=8 fenc=utf-8                                   :vi │
╞═════════════════════════════════════════════════════════════════════════════╡
│ This file is part of the Fil-C yolocosmo support.                            │
│                                                                              │
│ It describes the signals that this libc reserves for its own internal use,   │
│ and provides a sigdelset variant that does not refuse to delete those        │
│ signals. The Fil-C runtime (libpizlo) uses this to avoid clobbering libc     │
│ internal signals when it manipulates signal masks on behalf of user code.    │
│                                                                              │
│ The musl flavored equivalents live in                                        │
│ projects/yolomusl/src/signal/libc_internal_signals.c and sigdelset.c.        │
│                                                                              │
│ Cosmo reserves exactly one signal internally: SIGTHR (32), which is used by  │
│ the pthreads implementation (e.g. pthread_cancel).                           │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/calls/calls.h"
#include "libc/calls/struct/sigset.h"
#include "libc/errno.h"
#include "libc/sysv/consts/sig.h"

int libc_internal_signals[] = {
    SIGTHR,
};

unsigned num_libc_internal_signals = 1;

int sigdelsetyolo(sigset_t *set, int sig) {
  unsigned s = sig - 1;
  if (s >= NSIG) {
    errno = EINVAL;
    return -1;
  }
  *set &= ~(1ull << s);
  return 0;
}
