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

#include <stdatomic.h>
#include <stdlib.h>
#include <threads.h>

// a notification is not a promise that the predicate holds. we deliver
// a flurry of cnd_signals while the predicate is still false; a correct
// waiter re-checks under the mutex and stays blocked. it may proceed
// only after the predicate is actually set. (this also shakes out cv
// state corruption from repeated signal/wait cycling.)

static mtx_t mtx;
static cnd_t cnd;
static int ready;
static atomic_int proceeded;

static int waiter(void *arg) {
  if (mtx_lock(&mtx) != thrd_success)
    exit(20);
  while (!ready)
    if (cnd_wait(&cnd, &mtx) != thrd_success)
      exit(21);
  atomic_store_explicit(&proceeded, 1, memory_order_release);
  if (mtx_unlock(&mtx) != thrd_success)
    exit(22);
  return 0;
}

int main() {
  thrd_t t;
  if (mtx_init(&mtx, mtx_plain) != thrd_success)
    exit(10);
  if (cnd_init(&cnd) != thrd_success)
    exit(11);
  if (thrd_create(&t, waiter, 0) != thrd_success)
    exit(12);

  // bombard with notifications while the predicate is false
  for (int i = 0; i < 50; ++i) {
    if (mtx_lock(&mtx) != thrd_success)
      exit(13);
    if (cnd_signal(&cnd) != thrd_success)
      exit(14);
    if (mtx_unlock(&mtx) != thrd_success)
      exit(15);
    thrd_yield();
  }
  if (atomic_load_explicit(&proceeded, memory_order_acquire))
    exit(16);  // none of those signals may carry the waiter past its predicate

  // now make the predicate true and notify for real
  if (mtx_lock(&mtx) != thrd_success)
    exit(17);
  ready = 1;
  if (cnd_signal(&cnd) != thrd_success)
    exit(18);
  if (mtx_unlock(&mtx) != thrd_success)
    exit(19);
  if (thrd_join(t, 0) != thrd_success)
    exit(23);
  if (!atomic_load_explicit(&proceeded, memory_order_acquire))
    exit(24);
  cnd_destroy(&cnd);
  mtx_destroy(&mtx);
}
