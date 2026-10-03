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
#include <time.h>

// the subtle half of call_once: while one thread is partway through a
// slow init routine, every other thread that calls call_once on the
// same flag must BLOCK until that init finishes -- not race ahead
// seeing a flag that's been marked "in progress" but not yet "done".
// each worker therefore must observe init_done == 1 the instant
// call_once returns.

#define NTHREADS 4

static once_flag once = ONCE_FLAG_INIT;
static atomic_int init_runs;
static atomic_int init_done;

static void init_fn(void) {
  atomic_fetch_add_explicit(&init_runs, 1, memory_order_relaxed);
  thrd_sleep(&(struct timespec){.tv_nsec = 200000000}, 0);  // slow: 200ms
  atomic_store_explicit(&init_done, 1, memory_order_release);
}

static int worker(void *arg) {
  call_once(&once, init_fn);
  if (!atomic_load_explicit(&init_done, memory_order_acquire))
    exit(20);  // returned before the one-time init had completed
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
  if (atomic_load_explicit(&init_runs, memory_order_relaxed) != 1)
    exit(12);
  if (!atomic_load_explicit(&init_done, memory_order_acquire))
    exit(13);
}
