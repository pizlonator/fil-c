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

// when many threads race on the same once_flag, the init function must run
// exactly once, and call_once must not return until that init has completed
// (so every thread observes its effects).

#define NTHREADS 16

static once_flag once = ONCE_FLAG_INIT;
static atomic_int runs;

static void init_fn(void) {
  atomic_fetch_add_explicit(&runs, 1, memory_order_relaxed);
}

static int worker(void *arg) {
  call_once(&once, init_fn);
  // upon return the one-time init is guaranteed complete
  if (atomic_load_explicit(&runs, memory_order_relaxed) != 1)
    exit(20);
  return 0;
}

int main() {
  thrd_t t[NTHREADS];
  for (int i = 0; i < NTHREADS; ++i)
    if (thrd_create(&t[i], worker, 0) != thrd_success)
      exit(10);
  for (int i = 0; i < NTHREADS; ++i)
    if (thrd_join(t[i], 0) != thrd_success)
      exit(11);
  if (atomic_load_explicit(&runs, memory_order_relaxed) != 1)
    exit(12);
}
