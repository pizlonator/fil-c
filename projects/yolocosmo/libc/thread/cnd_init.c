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
#include "third_party/nsync/cv.h"

/**
 * Initializes condition variable.
 *
 * A condition variable lets threads sleep until shared state changes.
 * It's always paired with a mutex that guards that state, and with a
 * boolean predicate describing what the sleeper is waiting for, e.g.
 *
 *     mtx_t mu;     // guards `ready`
 *     cnd_t cv;     // notified when `ready` might have become true
 *     bool ready;
 *     mtx_init(&mu, mtx_plain);
 *     cnd_init(&cv);
 *
 *     // waiter
 *     mtx_lock(&mu);
 *     while (!ready)            // loop: a wakeup doesn't prove the predicate,
 *       cnd_wait(&cv, &mu);     // and wakeups are permitted to be spurious
 *     mtx_unlock(&mu);
 *
 *     // notifier
 *     mtx_lock(&mu);
 *     ready = true;
 *     cnd_signal(&cv);          // wake one waiter (cnd_broadcast wakes all)
 *     mtx_unlock(&mu);
 *
 * A condition variable must be initialized before use; there's no
 * static initializer, so this call is mandatory. Pass it to
 * cnd_destroy() once no thread is using it. A `cnd_t` occupies only 16
 * bytes, since it's backed by Mike Burrows' *NSYNC, the same engine
 * behind `mtx_t`.
 *
 * This API is part of the C11 standard. It's similar to
 * pthread_cond_init() with a null attribute: the condition is
 * process-private and its timed waits are measured against
 * `CLOCK_REALTIME`.
 *
 * @param cnd is the uninitialized condition variable, which is output
 * @return `thrd_success`, which is the only possible result
 * @see cnd_wait(), cnd_timedwait(), cnd_signal(), cnd_broadcast()
 * @see cnd_destroy(), mtx_init()
 */
int cnd_init(cnd_t *cnd) {
  static_assert(sizeof(cnd_t) >= sizeof(nsync_cv));
  static_assert(alignof(cnd_t) == alignof(nsync_cv));
  atomic_store_explicit(&cnd->_nsync, 0, memory_order_relaxed);
  cnd->_waiters = 0;
  return thrd_success;
}
