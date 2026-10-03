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
#include "libc/str/str.h"
#include "libc/thread/lock.h"
#include "libc/thread/threads.h"

/**
 * Destroys mutex.
 *
 * This releases any resources associated with `mtx`. It must be
 * unlocked and unused: destroying a mutex that is currently locked, or
 * that another thread might still lock, is undefined behavior. After
 * destruction `mtx` may not be used again until re-initialized with
 * mtx_init(). Since a `mtx_t` owns no kernel resource of its own, never
 * destroying one leaks nothing.
 *
 * This API is part of the C11 standard and behaves like
 * pthread_mutex_destroy().
 *
 * @param mtx must have been initialized by mtx_init() and not be held
 * @see mtx_init()
 */
void mtx_destroy(mtx_t *mtx) {
  // the lock must not be held on destruction: _nsync is the overlaid
  // nsync_mu word (zero when free) and _word retains only the type tag,
  // so we check there's no owner, recursion depth, or lock bit set
  unassert(MUTEX_OWNER(mtx->_word) == 0);
  unassert(MUTEX_DEPTH(mtx->_word) == 0);
  unassert(!MUTEX_LOCKED(mtx->_word));
  unassert(mtx->_waiters == 0);
  unassert(mtx->_nsync == 0);
  atomic_store_explicit(&mtx->_nsync, -1, memory_order_relaxed);
  mtx->_waiters = 0;
  atomic_store_explicit(&mtx->_word, -1, memory_order_relaxed);
}
