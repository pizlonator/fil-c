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
#include "libc/errno.h"
#include "libc/intrin/atomic.h"
#include "libc/intrin/likely.h"
#include "libc/sysv/consts/clock.h"
#include "libc/thread/lock.h"
#include "libc/thread/thread.h"
#include "libc/thread/threads.h"
#include "libc/thread/tls.h"
#include "third_party/nsync/time.h"
#include "third_party/nsync/timed.h"

dontinline static int mtx_timedlock_recursive(mtx_t *mtx,
                                              struct timespec abs_deadline,
                                              uint64_t word) {
  int me = atomic_load_explicit(&__get_tls()->tib_ptid, memory_order_relaxed);
RecursiveLock:
  if (MUTEX_OWNER(word) == me) {
    if (MUTEX_DEPTH(word) < MUTEX_DEPTH_MAX) {
      if (atomic_compare_exchange_weak_explicit(
              &mtx->_word, &word, MUTEX_INC_DEPTH(word), memory_order_relaxed,
              memory_order_relaxed))
        return thrd_success;
      goto RecursiveLock;
    } else {
      return thrd_error;
    }
  }
  switch (nsync_mu_clocklock((nsync_mu *)mtx, CLOCK_REALTIME, abs_deadline)) {
    case 0:
      word = MUTEX_UNLOCK(word);
      word = MUTEX_LOCK(word);
      word = MUTEX_SET_OWNER(word, me);
      mtx->_word = word;
      return thrd_success;
    case ETIMEDOUT:
      return thrd_timedout;
    default:  // EINVAL
      return thrd_error;
  }
}

/**
 * Locks mutex, giving up if a deadline passes.
 *
 * This behaves like mtx_lock(), except the wait for a contended mutex is
 * abandoned with `thrd_timedout` once `abs_deadline` is reached, e.g.
 *
 *     struct timespec ts;
 *     timespec_get(&ts, TIME_UTC);
 *     ts.tv_sec += 1;  // wait at most one second
 *     if (mtx_timedlock(&lock, &ts) == thrd_timedout)
 *       return;        // couldn't get the lock in time
 *
 * The deadline is an absolute `CLOCK_REALTIME` (`TIME_UTC`) time, not
 * an interval. If the mutex can be acquired without blocking, it's
 * taken even when the deadline has already passed. This is not a
 * cancelation point and is not interrupted by signals (it never returns
 * `EINTR`), nor is it safe to call from a signal handler.
 *
 * This API is part of the C11 standard and resembles
 * pthread_mutex_timedlock(), though here the deadline clock is always
 * `CLOCK_REALTIME`.
 *
 * @param mtx must have been initialized by mtx_init()
 * @param abs_deadline is an absolute `CLOCK_REALTIME` timestamp, or
 *     NULL to wait forever exactly as mtx_lock() does
 * @return `thrd_success` on acquiring the lock, `thrd_timedout` if the
 *     deadline elapsed first, or `thrd_error` if a recursive mutex
 *     exceeded its maximum lock depth or `abs_deadline` was invalid
 * @see mtx_lock(), mtx_trylock(), mtx_unlock()
 */
int mtx_timedlock(mtx_t *mtx, const struct timespec *abs_deadline) {
  struct timespec abs_deadline_value = timespec_max;
  if (abs_deadline)
    abs_deadline_value = *abs_deadline;
  uint64_t word = atomic_load_explicit(&mtx->_word, memory_order_relaxed);
  if (!(MUTEX_TYPE(word) & PTHREAD_MUTEX_RECURSIVE)) {
    switch (nsync_mu_clocklock((nsync_mu *)mtx, CLOCK_REALTIME,
                               abs_deadline_value)) {
      case 0:
        return thrd_success;
      case ETIMEDOUT:
        return thrd_timedout;
      default:  // EINVAL
        return thrd_error;
    }
  }
  return mtx_timedlock_recursive(mtx, abs_deadline_value, word);
}

/**
 * Locks mutex, blocking until it is acquired.
 *
 * If the mutex is already held by another thread, the caller blocks
 * until it becomes available. For a `mtx_recursive` mutex, the owning
 * thread may lock it again (incrementing a depth count) and must then
 * unlock it the same number of times; for a non-recursive mutex, a
 * thread that relocks a mutex it already holds deadlocks.
 *
 * This is not a cancelation point and is not interrupted by signals,
 * nor is it safe to call from a signal handler. There's no priority
 * inheritance.
 *
 * This API is part of the C11 standard and behaves like
 * pthread_mutex_lock().
 *
 * @param mtx must have been initialized by mtx_init()
 * @return `thrd_success` on success, or `thrd_error` if a recursive
 *     mutex exceeded its maximum lock depth
 * @see mtx_trylock(), mtx_timedlock(), mtx_unlock()
 */
int mtx_lock(mtx_t *mtx) {
  uint64_t word = atomic_load_explicit(&mtx->_word, memory_order_relaxed);
  if (!(MUTEX_TYPE(word) & PTHREAD_MUTEX_RECURSIVE))
    return nsync_mu_clocklock((nsync_mu *)mtx, CLOCK_REALTIME,
                              nsync_time_no_deadline);
  return mtx_timedlock_recursive(mtx, nsync_time_no_deadline, word);
}
