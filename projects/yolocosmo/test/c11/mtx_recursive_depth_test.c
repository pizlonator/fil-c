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

// a recursive mutex can only be relocked finitely many times. when that
// bound is reached mtx_lock must report thrd_error gracefully rather than
// overflow, deadlock, or crash. afterwards the matching unlocks must fully
// release the lock so a fresh thread can take it.

static mtx_t mtx;

static int taker(void *arg) {
  // once fully unlocked by main, this must succeed
  if (mtx_lock(&mtx) != thrd_success)
    exit(30);
  if (mtx_unlock(&mtx) != thrd_success)
    exit(31);
  return 0;
}

int main() {
  thrd_t t;
  long depth = 0;
  int rc;

  if (mtx_init(&mtx, mtx_plain | mtx_recursive) != thrd_success)
    exit(10);

  // relock until the implementation refuses; the refusal must be thrd_error
  for (;;) {
    rc = mtx_lock(&mtx);
    if (rc == thrd_success) {
      if (++depth > 1000000)  // runaway guard: it should refuse long before
        exit(11);
      continue;
    }
    if (rc != thrd_error)
      exit(12);
    break;
  }
  if (depth < 2)  // a recursive mutex should allow at least a few levels
    exit(13);

  // release exactly as many times as we acquired
  for (long i = 0; i < depth; ++i)
    if (mtx_unlock(&mtx) != thrd_success)
      exit(14);

  // the lock is free again: another thread must be able to take it
  if (thrd_create(&t, taker, 0) != thrd_success)
    exit(15);
  if (thrd_join(t, 0) != thrd_success)
    exit(16);

  mtx_destroy(&mtx);
}
