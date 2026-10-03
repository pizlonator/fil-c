/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2022 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/calls/cp.internal.h"
#include "libc/calls/struct/timespec.h"
#include "libc/errno.h"
#include "libc/sysv/consts/clock.h"
#include "libc/thread/threads.h"
#include "third_party/nsync/cv.h"
#include "third_party/nsync/mu.h"
#include "third_party/nsync/time.h"

/**
 * Blocks on condition variable until notified.
 *
 * This atomically (a) releases `mtx`, which the calling thread must
 * hold, and (b) puts the thread to sleep on `cnd`. "Atomically" means a
 * notifier that acquires `mtx` after this releases it is guaranteed to
 * have its subsequent cnd_signal() / cnd_broadcast() seen by this wait,
 * so notifications can't be lost to a race. When the wait returns,
 * `mtx` has been re-acquired and is once again held by the calling
 * thread.
 *
 * A return does NOT prove the thing you're waiting for has happened:
 * wakeups may be spurious, and a notifier may have changed and
 * re-changed the state before you ran. Callers must therefore re-test
 * their predicate in a loop:
 *
 *     mtx_lock(&mu);
 *     while (!ready)
 *       cnd_wait(&cv, &mu);
 *     mtx_unlock(&mu);
 *
 * The same mutex must be used for every concurrent wait on a given
 * condition variable (a dynamic binding that lasts until the last
 * waiter is released); waiting with a different mutex meanwhile is
 * undefined behavior. The mutex should be a non-recursive `mtx_plain` /
 * `mtx_timed`; waiting on a recursive mutex locked more than once is
 * undefined, because only one level is dropped.
 *
 * This API is part of the C11 standard and behaves like
 * pthread_cond_wait().
 *
 * @param cnd is the condition variable, which must be initialized
 * @param mtx must be held by the calling thread on entry, and is held again
 *     on return
 * @return `thrd_success` on a normal wakeup (predicate must still be checked),
 *     `thrd_canceled` if the thread was cancelled, or `thrd_error` on failure
 * @return `thrd_canceled` happens only when the calling thread used
 *     pthread_cancel() and opted into `PTHREAD_CANCEL_MASKED` via
 *     pthread_setcancelstate(); otherwise a cancellation unwinds the stack via
 *     pthread_exit() and this never returns. `mtx` is held in either case.
 * @see cnd_timedwait(), cnd_signal(), cnd_broadcast()
 * @cancelationpoint
 */
int cnd_wait(cnd_t *cnd, mtx_t *mtx) {
  errno_t err;
  BEGIN_CANCELATION_POINT;
  err = nsync_cv_wait_with_deadline((nsync_cv *)cnd, (nsync_mu *)mtx,
                                    CLOCK_REALTIME, nsync_time_no_deadline, 0);
  END_CANCELATION_POINT;
  switch (err) {
    case 0:
      return thrd_success;
    case ECANCELED:
      return thrd_canceled;
    default:
      return thrd_error;
  }
}
