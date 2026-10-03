#ifndef _SIGNAL_H
#define _SIGNAL_H
#include "libc/calls/calls.h"
#include "libc/calls/sigtimedwait.h"
#include "libc/calls/struct/sigaction.h"
#include "libc/calls/struct/sigaltstack.h"
#include "libc/calls/struct/siginfo.h"
#include "libc/calls/ucontext.h"
#include "libc/sysv/consts/sa.h"
#include "libc/sysv/consts/sicode.h"
#include "libc/sysv/consts/sig.h"
#include "libc/sysv/consts/ss.h"

/* Fil-C additions: describe the signals this libc reserves for its own
   internal use, and provide a sigdelset variant that doesn't refuse to delete
   them. See libc/calls/libc_internal_signals.c and the musl flavored
   equivalents in projects/yolomusl/src/signal/. The Fil-C runtime (libpizlo)
   declares these in its own sources too, but having them here means that
   `#include <signal.h>` is all that's needed to build libpizlo. */
extern int libc_internal_signals[];
extern unsigned num_libc_internal_signals;

int sigdelsetyolo(sigset_t *, int);
#endif /* _SIGNAL_H */
