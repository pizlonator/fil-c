/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8     -*-│
│ vi: set noet ft=c ts=2 sts=2 sw=2 fenc=utf-8                             :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Fil-C port of Cosmopolitan libc: futex shims.                                │
│                                                                              │
│ Cosmo's futex layer (libc/intrin/cosmo_futex.c) bottoms out in three raw     │
│ assembly thunks:                                                             │
│                                                                              │
│   - cosmo_futex_thunk: the raw futex(2) syscall, returning -errno            │
│     (it jumps to _linret, i.e. errors come back NEGATED, unlike the          │
│     ordinary sys_* thunks which return -1 with errno set).                   │
│   - _futex_wake: a 3-argument declaration that cosmo binds to the same       │
│     thunk via an asm("cosmo_futex_thunk") label;                              │
│   - sys_futex_cp: the cancellable futex wait (also -errno convention).       │
│                                                                              │
| Under Fil-C those thunks can not exist (raw syscalls are not allowed), so    │
│ this file re-implements them on top of libpizlo's zsys_futex_* API, keeping  │
│ the exact same -errno return convention that cosmo_futex.c expects.  The     │
│ asm("...") label on the _futex_wake declaration in cosmo_futex.c has been    │
│ removed; instead this file defines _futex_wake as a regular function.        │
│                                                                              │
│ Note that libpizlo's zsys_futex_timedwait() takes an ABSOLUTE timeout        │
│ measured against a given clock, which maps perfectly onto cosmo's            │
│ FUTEX_WAIT_BITSET usage; the plain FUTEX_WAIT path (relative timeout,        │
│ monotonic clock) is converted here.                                          │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/calls/struct/timespec.h"
#include "libc/calls/struct/timespec.internal.h"
#include "libc/cosmotime.h"
#include "libc/dce.h"
#include "libc/errno.h"
#include "libc/intrin/atomic.h"
#include "libc/limits.h"
#include "libc/sysv/consts/clock.h"
#include "libc/sysv/consts/futex.h"
#include "libc/thread/tls.h"
#include <stdfil.h>
#include <pizlonated_syscalls.h>
#include <pizlonated_runtime.h>

/* libpizlo's zsys_futex_* "priv" argument follows the musl convention:
   nonzero means PTHREAD_PROCESS_PRIVATE. */
static int filc_futex_priv(int op) {
  return (op & FUTEX_PRIVATE_FLAG) ? 1 : 0;
}

static int filc_futex_wait_raw(volatile int *uaddr, int op, int val,
                               const struct timespec *timeout, int val3) {
  int base, clockid;
  struct timespec absmem;
  const struct timespec *abstimeout = 0;

  base = op & ~(FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME);
  if ((op & FUTEX_CLOCK_REALTIME) || base == FUTEX_WAIT_BITSET)
    clockid = CLOCK_REALTIME;
  else
    clockid = CLOCK_MONOTONIC;

  if (base == FUTEX_WAIT || base == FUTEX_WAIT_BITSET) {
    if (timeout) {
      if (base == FUTEX_WAIT) {
        /* plain FUTEX_WAIT uses a RELATIVE timeout on the monotonic clock;
           convert it to the absolute form zsys_futex_timedwait() wants. */
        struct timespec now;
        if (zsys_clock_gettime(CLOCK_MONOTONIC, &now))
          return -EOPNOTSUPP;
        absmem = timespec_add(now, *timeout);
      } else {
        /* FUTEX_WAIT_BITSET takes an absolute timeout already. */
        absmem = *timeout;
      }
      abstimeout = &absmem;
    }
    if (base == FUTEX_WAIT_BITSET && val3 != FUTEX_BITSET_MATCH_ANY)
      return -ENOSYS;
    /* zsys_futex_timedwait() returns the errno as a POSITIVE value (or 0). */
    int err = zsys_futex_timedwait(uaddr, val, clockid, abstimeout,
                                   filc_futex_priv(op));
    if (err > 0)
      return -err;
    return 0;
  }

  return -ENOSYS;
}

/**
 * Raw futex() wait side (cosmo's cancellable futex thunk).
 * Returns 0 on success or -errno.
 */
int sys_futex_cp(volatile int *uaddr, int op, int val,
                 const struct timespec *timeout, int *uaddr2, int val3) {
  (void)uaddr2;
  if ((op & ~(FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME)) == FUTEX_WAKE) {
    zsys_futex_wake(uaddr, val, filc_futex_priv(op));
    return 0;
  }
  return filc_futex_wait_raw(uaddr, op, val, timeout, val3);
}

/**
 * Raw futex() (cosmo's cosmo_futex_thunk). Returns 0/#woken or -errno.
 */
long cosmo_futex_thunk(volatile int *uaddr, int futex_op, int val,
                       const struct timespec *timeout, int *uaddr2,
                       int val3) {
  if ((futex_op & ~(FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME)) ==
      FUTEX_WAKE) {
    zsys_futex_wake(uaddr, val, filc_futex_priv(futex_op));
    return 0;
  }
  return filc_futex_wait_raw(uaddr, futex_op, val, timeout, val3);
}

/**
 * sched_yield(): cosmo's version is a raw syscall thunk
 * (libc/intrin/sys_sched_yield.S) returning the raw -errno; zsys_sched_yield
 * has no failure mode we care about here.
 */
int sys_sched_yield(void) {
  zsys_sched_yield();
  return 0;
}

/**
 * Wakes futex waiters (cosmo binds this name to the same thunk). Returns the
 * number woken or -errno; we conservatively return 0 (callers only check for
 * negative error values, or ignore the count entirely).
 */
long _futex_wake(volatile int *uaddr, int op, int val) {
  zsys_futex_wake(uaddr, val, filc_futex_priv(op));
  return 0;
}
