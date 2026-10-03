/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2025 Justine Alexandra Roberts Tunney                              │
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
#include "libc/thread/thread.h"
#include "libc/thread/threads.h"

/**
 * Allocates thread-specific storage key.
 *
 * A `tss_t` key names a slot that holds a separate `void *` value in each
 * thread. Every thread initially sees NULL for a freshly created key, and one
 * thread's value is invisible to others, e.g.
 *
 *     tss_t key;
 *     tss_create(&key, free);          // destructor called at thread exit
 *     tss_set(key, malloc(32));        // this thread's private pointer
 *     void *p = tss_get(key);          // reads back what this thread set
 *
 * If `destructor` is non-NULL, then when a thread exits with a non-NULL value
 * stored under this key, the destructor is called with that value. The C11
 * standard repeats this up to `TSS_DTOR_ITERATIONS` times for keys whose
 * destructor re-sets a non-NULL value, after which any remaining value is
 * abandoned. Destructors run on thread termination, not at normal process
 * exit.
 *
 * Keys are a finite resource; release them with tss_delete() when done. This
 * API is part of the C11 standard, and is nearly identical to
 * pthread_key_create(), which has further documentation.
 *
 * @param tss_key is output parameter for the new key
 * @param destructor is called on each thread's non-NULL value at thread exit,
 *     or may be NULL for no cleanup
 * @return `thrd_success` on success, or `thrd_error` on error, e.g. if too
 *     many keys exist
 * @see tss_get(), tss_set(), tss_delete()
 */
int tss_create(tss_t* tss_key, tss_dtor_t destructor) {
  static_assert(sizeof(tss_t) == sizeof(pthread_key_t), "");
  static_assert(TSS_DTOR_ITERATIONS == PTHREAD_DESTRUCTOR_ITERATIONS, "");
  if (pthread_key_create(tss_key, destructor))
    return thrd_error;
  return thrd_success;
}
