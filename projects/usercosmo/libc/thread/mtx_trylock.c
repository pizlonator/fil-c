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
#include "libc/intrin/atomic.h"
#include "libc/intrin/likely.h"
#include "libc/thread/lock.h"
#include "libc/thread/thread.h"
#include "libc/thread/threads.h"
#include "libc/thread/tls.h"
#include "third_party/nsync/mu.h"

dontinline static int mtx_trylock_recursive(mtx_t *mtx, uint64_t word) {
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
  } else if (MUTEX_OWNER(word) != 0) {
    return thrd_busy;
  }
  if (!nsync_mu_trylock((nsync_mu *)mtx))
    return thrd_busy;
  word = MUTEX_UNLOCK(word);
  word = MUTEX_LOCK(word);
  word = MUTEX_SET_OWNER(word, me);
  mtx->_word = word;
  return thrd_success;
}

/**
 * Tries to lock mutex without blocking.
 *
 * This attempts to acquire `mtx` and returns immediately either way: it
 * never sleeps waiting for a contended lock. It's the building block
 * for "do other work if the lock isn't free" patterns, e.g.
 *
 *     if (mtx_trylock(&lock) == thrd_success) {
 *       // ... got it, touch shared state ...
 *       mtx_unlock(&lock);
 *     } else {
 *       // busy; go do something else
 *     }
 *
 * For a `mtx_recursive` mutex already owned by the calling thread, this
 * succeeds and bumps the recursion count (requiring a matching unlock).
 * It reports `thrd_busy` when another thread holds the lock.
 *
 * This API is part of the C11 standard and behaves like
 * pthread_mutex_trylock().
 *
 * @param mtx must have been initialized by mtx_init()
 * @return `thrd_success` if the lock was acquired, `thrd_busy` if it's
 *     currently held by another thread, or `thrd_error` if a recursive mutex
 *     exceeded its maximum lock depth
 * @see mtx_lock(), mtx_timedlock(), mtx_unlock()
 */
int mtx_trylock(mtx_t *mtx) {
  uint64_t word = atomic_load_explicit(&mtx->_word, memory_order_relaxed);
  if (!(MUTEX_TYPE(word) & PTHREAD_MUTEX_RECURSIVE)) {
    if (nsync_mu_trylock((nsync_mu *)mtx))
      return thrd_success;
    return thrd_busy;
  }
  return mtx_trylock_recursive(mtx, word);
}
