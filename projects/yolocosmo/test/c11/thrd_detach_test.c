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

// thrd_detach must succeed and let the thread run to completion on its own
// (its result is no longer joinable). we observe its work through an atomic
// flag, spinning with thrd_yield until it lands.

static atomic_int done;

static int worker(void *arg) {
  atomic_store_explicit(&done, 1, memory_order_release);
  return 0;
}

int main() {
  thrd_t t;
  if (thrd_create(&t, worker, 0) != thrd_success)
    exit(10);
  if (thrd_detach(t) != thrd_success)
    exit(11);
  while (!atomic_load_explicit(&done, memory_order_acquire))
    thrd_yield();
}
