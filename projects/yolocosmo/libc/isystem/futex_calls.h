#ifndef COSMOPOLITAN_LIBC_ISYSTEM_FUTEX_CALLS_H_
#define COSMOPOLITAN_LIBC_ISYSTEM_FUTEX_CALLS_H_
#include "libc/calls/struct/timespec.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fil-C runtime (libpizlo) pass-through futex primitives.

   These are the yolo-libc counterparts of the zsys_futex_* system calls that
   the Fil-C runtime implements; see projects/yolomusl/src/thread/futex_calls.c
   for the musl-flavored versions with the same signatures and semantics.

   Only the portable subset is provided: these three are implemented on top of
   cosmo's per-OS futex machinery (libc/intrin/cosmo_futex.c), so they work on
   Linux, Windows, XNU, and the BSDs.  The PI and requeue operations are
   Linux-only, so cosmo doesn't provide them and the Fil-C runtime's
   zsys_futex_lock_pi/unlock_pi/requeue forwarders panic under PAS_COSMO.

   The `priv` argument is nonzero for futexes shared between threads of the
   same process (FUTEX_PRIVATE_FLAG) and zero for process-shared futexes. */

void yolo_futex_wake(volatile int *addr, int cnt, int priv);
void yolo_futex_wait(volatile int *addr, int val, int priv);

/* yolo_futex_timedwait takes an absolute `timeout` measured against `clock_id`
   and returns 0 on success or the errno as a positive value (it does not set
   errno). */
int yolo_futex_timedwait(volatile int *addr, int val, int clock_id, const struct timespec *timeout, int priv);

#ifdef __cplusplus
}
#endif

#endif /* COSMOPOLITAN_LIBC_ISYSTEM_FUTEX_CALLS_H_ */
