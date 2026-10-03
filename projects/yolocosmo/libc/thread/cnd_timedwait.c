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
 * Blocks on condition variable until notified or deadline elapses.
 *
 * This behaves exactly like cnd_wait() — atomically releasing `mtx` and
 * sleeping on `cnd`, then re-acquiring `mtx` before returning — except
 * that the wait is abandoned with `thrd_timedout` once `abstime`
 * passes. The predicate must still be re-checked in a loop, since a
 * normal return may be spurious and a timeout may race a genuine
 * notification:
 *
 *     struct timespec ts;
 *     timespec_get(&ts, TIME_UTC);
 *     ts.tv_sec += 5;  // give up after five seconds
 *     mtx_lock(&mu);
 *     while (!ready)
 *       if (cnd_timedwait(&cv, &mu, &ts) == thrd_timedout)
 *         break;       // ready may still be false here; handle the timeout
 *     mtx_unlock(&mu);
 *
 * The mutex is re-acquired and held by the calling thread on EVERY
 * return path, including `thrd_timedout` and `thrd_error`, so it's
 * always safe (and required) to unlock it afterward.
 *
 * This API is part of the C11 standard and behaves like
 * pthread_cond_timedwait().
 *
 * @param cnd is the condition variable, which must be initialized
 * @param mtx must be held by the calling thread on entry, and is held again
 *     on return regardless of outcome
 * @param abstime is an absolute `CLOCK_REALTIME` (`TIME_UTC`) deadline, or
 *     NULL to wait forever as cnd_wait() does
 * @return `thrd_success` on a notification (re-check the predicate),
 *     `thrd_timedout` if `abstime` elapsed first, `thrd_canceled` if cancelled
 *     in masked mode, or `thrd_error` on failure such as a malformed `abstime`
 *     whose `tv_nsec` isn't in `[0,1000000000)`
 * @see cnd_wait(), cnd_signal(), cnd_broadcast()
 * @cancelationpoint
 */
int cnd_timedwait(cnd_t *cnd, mtx_t *mtx, const struct timespec *abstime) {
  errno_t err;
  BEGIN_CANCELATION_POINT;
  err = nsync_cv_wait_with_deadline(
      (nsync_cv *)cnd, (nsync_mu *)mtx, CLOCK_REALTIME,
      abstime ? *abstime : nsync_time_no_deadline, 0);
  END_CANCELATION_POINT;
  switch (err) {
    case 0:
      return thrd_success;
    case ETIMEDOUT:
      return thrd_timedout;
    case ECANCELED:
      return thrd_canceled;
    default:
      return thrd_error;
  }
}
