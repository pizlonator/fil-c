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

// a tss destructor may re-set its own key, which makes the runtime call it
// again. c11 bounds this at TSS_DTOR_ITERATIONS passes, so a destructor that
// always re-arms must still be called at least once and no more than the
// allowed maximum (rather than looping forever).

static tss_t key;
static atomic_int calls;

static void dtor(void *p) {
  atomic_fetch_add_explicit(&calls, 1, memory_order_relaxed);
  tss_set(key, (void *)1);  // always re-arm: keep the value non-NULL
}

static int worker(void *arg) {
  if (tss_set(key, (void *)1) != thrd_success)
    exit(20);
  return 0;
}

int main() {
  thrd_t t;
  int n;
  if (tss_create(&key, dtor) != thrd_success)
    exit(10);
  if (thrd_create(&t, worker, 0) != thrd_success)
    exit(11);
  if (thrd_join(t, 0) != thrd_success)
    exit(12);
  n = atomic_load_explicit(&calls, memory_order_relaxed);
  if (n < 1)  // a non-NULL value must trigger the destructor
    exit(13);
  if (n > TSS_DTOR_ITERATIONS)  // but the runtime must stop re-running it
    exit(14);
  tss_delete(key);
}
