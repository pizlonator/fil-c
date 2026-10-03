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
#include "libc/thread/lock.h"
#include "libc/thread/thread.h"
#include "libc/thread/threads.h"
#include "third_party/nsync/mu.h"

dontinline static int mtx_unlock_recursive(mtx_t *mtx, uint64_t word) {
  while (MUTEX_DEPTH(word)) {
    if (atomic_compare_exchange_weak_explicit(
            &mtx->_word, &word, MUTEX_DEC_DEPTH(word), memory_order_relaxed,
            memory_order_relaxed))
      return thrd_success;
  }
  atomic_store_explicit(&mtx->_word, MUTEX_UNLOCK(word), memory_order_relaxed);
  nsync_mu_unlock((nsync_mu *)mtx);
  return thrd_success;
}

/**
 * Unlocks mutex.
 *
 * This releases `mtx`, which the calling thread must currently hold,
 * and wakes a waiting thread if any. Unlocking a mutex you don't own,
 * or one that isn't locked, is undefined behavior. For `mtx_recursive`
 * mutices this drops one level of ownership; the mutex only becomes
 * free once it's been unlocked as many times as it was locked.
 *
 * This API is part of the C11 standard and behaves like
 * pthread_mutex_unlock().
 *
 * @param mtx must already be locked by calling thread
 * @return `thrd_success` on success
 * @see mtx_lock(), mtx_trylock(), mtx_timedlock()
 */
int mtx_unlock(mtx_t *mtx) {
  uint64_t word = atomic_load_explicit(&mtx->_word, memory_order_relaxed);
  if (!(MUTEX_TYPE(word) & PTHREAD_MUTEX_RECURSIVE)) {
    nsync_mu_unlock((nsync_mu *)mtx);
    return thrd_success;
  }
  return mtx_unlock_recursive(mtx, word);
}
