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

// many threads incrementing a shared counter under a c11 mutex must never
// lose an update. the final tally is exact only if the mutex delivers real
// mutual exclusion, so a flaky lock shows up as a short count.

#define NTHREADS 8
#define NITER    100000

static mtx_t mtx;
static long counter;

static int worker(void *arg) {
  for (long i = 0; i < NITER; ++i) {
    if (mtx_lock(&mtx) != thrd_success)
      exit(20);
    ++counter;
    if (mtx_unlock(&mtx) != thrd_success)
      exit(21);
  }
  return 0;
}

int main() {
  thrd_t t[NTHREADS];
  if (mtx_init(&mtx, mtx_plain) != thrd_success)
    exit(10);
  for (long i = 0; i < NTHREADS; ++i)
    if (thrd_create(&t[i], worker, 0) != thrd_success)
      exit(11);
  for (long i = 0; i < NTHREADS; ++i)
    if (thrd_join(t[i], 0) != thrd_success)
      exit(12);
  if (counter != (long)NTHREADS * NITER)
    exit(13);
  mtx_destroy(&mtx);
}
