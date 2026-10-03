/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2026 Justine Alexandra Roberts Tunney                              │
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
#include "libc/calls/struct/timespec.h"
#include "libc/errno.h"
#include "libc/sysv/consts/clock.h"
#include "libc/thread/threads.h"

/**
 * Suspends calling thread for a duration.
 *
 * This sleeps for at least the relative interval `*req` (not an
 * absolute time), measured against `CLOCK_REALTIME`, e.g.
 *
 *     thrd_sleep(&(struct timespec){.tv_sec = 1}, 0);  // sleep ~1 second
 *
 * If a signal handler interrupts the sleep, this returns early with -1;
 * if `rem` is non-NULL it receives the unslept remainder, so the call
 * can be resumed by sleeping for `*rem`. The interval is relative and
 * not adjusted if the system clock is stepped while sleeping.
 *
 * This API is part of the C11 standard. It resembles nanosleep(), but
 * reports status through its return value rather than `errno`.
 *
 * @param req is the requested duration, a relative time; `tv_nsec` must
 *     be in `[0,1000000000)`
 * @param rem if non-NULL receives the time remaining when interrupted
 * @return 0 if the full interval elapsed, -1 if interrupted by a
 *     signal, -3 if thread was canceled in masked mode, otherwise some
 *     other negative value if the arguments were invalid
 * @see nanosleep(), clock_nanosleep()
 * @cancelationpoint
 * @norestart
 */
int thrd_sleep(const struct timespec *req, struct timespec *rem) {
  switch (clock_nanosleep(CLOCK_REALTIME, 0, req, rem)) {
    case 0:
      return 0;
    case EINTR:
      return -1;
    case ECANCELED:
      return -3;
    default:
      return -2;
  }
}
