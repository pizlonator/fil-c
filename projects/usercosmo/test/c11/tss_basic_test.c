// Copyright 2026 Justine Alexandra Roberts Tunney
//
// Permission to use, copy, modify, and/or distribute this software for
// any purpose with or without fee is hereby granted, provided that the
// above copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
// WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
// AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
// DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
// PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
// TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#include <stdlib.h>
#include <threads.h>

// thread-specific storage is per-thread: a fresh key reads NULL in every
// thread, and each thread's value is private. one thread's writes must never
// be visible through the same key in another thread.

#define NTHREADS 8

static tss_t key;

static int worker(void *arg) {
  long id = (long)arg;
  if (tss_get(key) != NULL)  // unset key reads NULL
    exit(20);
  if (tss_set(key, (void *)(id + 1)) != thrd_success)
    exit(21);
  if (tss_get(key) != (void *)(id + 1))  // reads back our own value
    exit(22);
  return 0;
}

int main() {
  thrd_t t[NTHREADS];
  if (tss_create(&key, 0) != thrd_success)
    exit(10);
  if (tss_set(key, (void *)999) != thrd_success)  // main's private value
    exit(11);
  for (long i = 0; i < NTHREADS; ++i)
    if (thrd_create(&t[i], worker, (void *)i) != thrd_success)
      exit(12);
  for (int i = 0; i < NTHREADS; ++i)
    if (thrd_join(t[i], 0) != thrd_success)
      exit(13);
  if (tss_get(key) != (void *)999)  // unchanged by the worker threads
    exit(14);
  tss_delete(key);
}
