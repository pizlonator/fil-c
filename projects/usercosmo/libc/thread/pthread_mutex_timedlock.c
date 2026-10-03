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
#include "libc/sysv/consts/clock.h"
#include "libc/thread/thread2.h"

/**
 * Locks mutex, giving up after a `CLOCK_REALTIME` deadline.
 *
 * This is equivalent to pthread_mutex_clocklock() with a `clock` argument of
 * `CLOCK_REALTIME`.
 *
 * @param abstime is an absolute `CLOCK_REALTIME` deadline, or null to wait
 *     forever, in which case this is identical to pthread_mutex_lock()
 * @return 0 on success, or error number on failure
 * @raise ETIMEDOUT if the lock couldn't be acquired before `abstime`
 * @raise EINVAL if `abstime->tv_nsec` is not in `[0,1000000000)`
 * @see pthread_mutex_clocklock()
 * @see pthread_mutex_lock()
 */
errno_t pthread_mutex_timedlock(pthread_mutex_t *mutex,
                                const struct timespec *abstime) {
  return pthread_mutex_clocklock(mutex, CLOCK_REALTIME, abstime);
}
