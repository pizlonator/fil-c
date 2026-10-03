/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8   -*-│
│ vi: set noet ft=c ts=8 sw=8 fenc=utf-8                                   :vi │
╞═════════════════════════════════════════════════════════════════════════════╡
│ This file is part of the Fil-C yolocosmo support.                            │
│                                                                              │
│ It implements the portable yolo futex primitives that the Fil-C runtime      │
│ (libpizlo) uses to implement its zsys_futex_* pass-through system calls.     │
│ The musl flavored equivalents live in                                        │
│ projects/yolomusl/src/thread/futex_calls.c and these functions have exactly  │
│ the same signatures and semantics.  Only the portable subset is provided:    │
│ these three are backed by cosmo's per-OS futex machinery                     │
│ (libc/intrin/cosmo_futex.c), so they work on Linux, Windows, XNU, and the    │
│ BSDs.  The Linux-only PI and requeue operations are not exposed at all; the  │
│ Fil-C runtime's zsys_futex_lock_pi/unlock_pi/requeue forwarders panic under  │
│ PAS_COSMO.                                                                   │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/calls/struct/timespec.h"
#include "libc/cosmo.h"
#include "libc/errno.h"
#include "libc/sysv/consts/clock.h"
#include "libc/thread/thread.h"

static char yolo_futex_pshare(int priv) {
  if (priv)
    return PTHREAD_PROCESS_PRIVATE;
  return PTHREAD_PROCESS_SHARED;
}

void yolo_futex_wake(volatile int *addr, int cnt, int priv) {
  /* Matches musl's __wake().  Uses the per-OS cosmo futex machinery: the raw
     sys_futex thunk is Linux-only (it fails with ENOSYS on Windows and the
     BSDs), which would leave waiters asleep forever there. */
  cosmo_futex_wake((cosmo_futex_t *)addr, cnt, yolo_futex_pshare(priv));
}

void yolo_futex_wait(volatile int *addr, int val, int priv) {
  /* Matches musl's __futexwait(): wait forever and ignore errors, since
     callers are expected to use this inside a loop that rechecks `*addr`.
     Uses the per-OS cosmo futex machinery (win32 futexes on Windows, ulocks
     on XNU, ...): the raw sys_futex thunk is Linux-only and would turn every
     blocking wait into a busy spin off-Linux.  A NULL deadline waits
     forever; like the kernel's FUTEX_WAIT, this returns immediately with
     -EAGAIN when *addr doesn't hold `val` anymore, which is exactly the
     semantics the recheck loops expect. */
  cosmo_futex_wait((cosmo_futex_t *)addr, val, yolo_futex_pshare(priv),
                   CLOCK_MONOTONIC, 0);
}

int yolo_futex_timedwait(volatile int *addr, int val, int clock_id,
                         const struct timespec *timeout, int priv) {
  /* Matches musl's __timedwait(): `timeout` is an absolute time measured
     against `clock_id`. Returns 0 on success (including the EAGAIN case where
     `*addr` already had a different value) or a positive errno. musl maps
     everything except EINTR, ETIMEDOUT, and ECANCELED to zero. */
  int rc = cosmo_futex_wait((cosmo_futex_t *)addr, val, yolo_futex_pshare(priv),
                            clock_id, timeout);
  if (rc == -EAGAIN)
    return 0;
  if (rc == -EINTR || rc == -ETIMEDOUT || rc == -ECANCELED)
    return -rc;
  if (rc == -EINVAL)
    return EINVAL;
  return 0;
}
