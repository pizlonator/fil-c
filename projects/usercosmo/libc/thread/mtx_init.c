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
#include "libc/assert.h"
#include "libc/intrin/atomic.h"
#include "libc/isystem/stdalign.h"
#include "libc/str/str.h"
#include "libc/thread/thread.h"
#include "libc/thread/threads.h"
#include "third_party/nsync/mu.h"

/**
 * Initializes mutex.
 *
 * A mutex provides mutual exclusion: at most one thread holds it at a
 * time, so the critical sections it guards never overlap, e.g.
 *
 *     mtx_t lock;
 *     mtx_init(&lock, mtx_plain);
 *     mtx_lock(&lock);
 *     // ... touch shared state ...
 *     mtx_unlock(&lock);
 *     mtx_destroy(&lock);
 *
 * The `type` is a bitwise combination of:
 *
 *   - `mtx_plain` for an ordinary mutex
 *
 *   - `mtx_timed` to additionally support mtx_timedlock(); cosmopolitan
 *     also allows mtx_timedlock() on a plain mutex, so this flag costs
 *     nothing
 *
 *   - `mtx_recursive` (OR'd with one of the above) to let the owning
 *     thread relock the mutex; it must then unlock once per lock. A
 *     non-recursive mutex relocked by its owner deadlocks
 *
 * Every mutex must be initialized exactly once before use — there's no
 * static initializer — and passed to mtx_destroy() when done. A `mtx_t`
 * is 24 bytes and, in the common non-recursive private case, is backed
 * by Mike Burrows' *NSYNC, the same fast engine used by the pthread
 * mutexes.
 *
 * This API is part of the C11 standard. It's comparable to
 * pthread_mutex_init() with an attribute selecting
 * `PTHREAD_MUTEX_NORMAL` or `_RECURSIVE`.
 *
 * @param mtx is the uninitialized mutex, which is output
 * @param type is one of `mtx_plain`, `mtx_timed`,
 *     `mtx_plain|mtx_recursive`, or `mtx_timed|mtx_recursive`
 * @return `thrd_success` on success, or `thrd_error` if `type` is
 *     invalid
 * @see mtx_lock()
 * @see mtx_unlock()
 * @see mtx_timedlock()
 * @see mtx_trylock()
 * @see mtx_destroy()
 */
int mtx_init(mtx_t *mtx, int type) {
  static_assert(sizeof(mtx_t) >= sizeof(nsync_mu));
  static_assert(alignof(mtx_t) == alignof(nsync_mu));
  switch (type) {
    case mtx_plain:
    case mtx_timed:
    case mtx_plain | mtx_recursive:
    case mtx_timed | mtx_recursive:
      atomic_store_explicit(&mtx->_nsync, 0, memory_order_relaxed);
      mtx->_waiters = 0;
      atomic_store_explicit(&mtx->_word,
                            (type & mtx_recursive) ? PTHREAD_MUTEX_RECURSIVE
                                                   : PTHREAD_MUTEX_NORMAL,
                            memory_order_relaxed);
      return thrd_success;
    default:
      return thrd_error;
  }
}
