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
#include "libc/thread/threads.h"
#include "third_party/nsync/cv.h"

/**
 * Wakes one thread blocked on condition variable.
 *
 * If any threads are blocked in cnd_wait() / cnd_timedwait() on `cnd`,
 * at least one of them is unblocked; it then contends for the
 * associated mutex as if it had called mtx_lock(), and only returns
 * from its wait once it holds that mutex. If no thread is currently
 * blocked, this does nothing — notifications are not saved, so a signal
 * sent before a thread waits will not wake a later wait.
 *
 * You may call this whether or not you hold the mutex, but holding it
 * across the state change and the signal gives predictable scheduling
 * and avoids the lost-wakeup pitfall. Use cnd_broadcast() instead when
 * a change may let more than one waiter proceed.
 *
 * Because the waiter must re-check its predicate anyway, a spurious
 * extra signal is harmless. This API is part of the C11 standard and
 * behaves like pthread_cond_signal().
 *
 * @param cnd is the condition variable, which must be initialized
 * @return `thrd_success`, which is the only possible result
 * @see cnd_broadcast(), cnd_wait(), cnd_timedwait()
 */
int cnd_signal(cnd_t *cnd) {
  nsync_cv_signal((nsync_cv *)cnd);
  return thrd_success;
}
