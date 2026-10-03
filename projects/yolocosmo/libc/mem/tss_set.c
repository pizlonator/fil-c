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
 * Sets calling thread's value for thread-specific storage key.
 *
 * This stores `val` in the calling thread's private slot for `tss_key`. It
 * affects only this thread; other threads' values for the same key are
 * unchanged, and tss_get(tss_key) in this thread will subsequently return
 * `val`. Storing a non-NULL value is what later triggers the key's
 * destructor (if any) when this thread exits.
 *
 * Note that overwriting a value with tss_set() does NOT invoke the
 * destructor on the previous value; if it owns a resource, fetch and free it
 * yourself first. This API is part of the C11 standard, and is nearly
 * identical to pthread_setspecific(), which has further documentation.
 *
 * @param tss_key must have been created by tss_create()
 * @param val is the pointer-sized value to store for the calling thread
 * @return `thrd_success` on success, or `thrd_error` on error
 * @see tss_get(), tss_create(), tss_delete()
 */
int tss_set(tss_t tss_key, void *val) {
  if (pthread_setspecific(tss_key, (const void *)val))
    return thrd_error;
  return thrd_success;
}
