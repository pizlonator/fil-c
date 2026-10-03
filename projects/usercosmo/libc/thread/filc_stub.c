/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8     -*-│
│ vi: set noet ft=c ts=2 sts=2 sw=2 fenc=utf-8                             :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Fil-C port of Cosmopolitan libc: stubs for features not ported yet.          │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/errno.h"
#include "libc/thread/thread.h"

/**
 * Requests cancellation of a thread.
 *
 * Not ported yet: cosmo's implementation sends a special signal to the
 * target thread and depends on its assembly-only cancel ack machinery.
 * libpizlo's thread API doesn't have a cancellation mechanism either.
 */
errno_t pthread_cancel(pthread_t thread) {
  (void)thread;
  return ENOSYS;
}

/**
 * Reports whether cancellation is pending (cosmo extension).
 *
 * pthread_cancel() is a no-op in this build, so cancellation is never
 * pending.  Various cosmo cancellation points poll this via _weaken().
 */
errno_t pthread_testcancel_np(void) {
  return 0;
}
