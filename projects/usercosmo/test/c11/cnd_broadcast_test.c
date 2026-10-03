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
#include <time.h>

// cnd_broadcast must unblock every thread waiting on the condition, not just
// one. all NTHREADS waiters must observe the predicate and exit.

#define NTHREADS 8

static mtx_t mtx;
static cnd_t cnd;
static int go;
static int awake;

static int waiter(void *arg) {
  if (mtx_lock(&mtx) != thrd_success)
    exit(20);
  while (!go)
    if (cnd_wait(&cnd, &mtx) != thrd_success)
      exit(21);
  ++awake;
  if (mtx_unlock(&mtx) != thrd_success)
    exit(22);
  return 0;
}

int main() {
  thrd_t t[NTHREADS];
  if (mtx_init(&mtx, mtx_plain) != thrd_success)
    exit(10);
  if (cnd_init(&cnd) != thrd_success)
    exit(11);
  for (int i = 0; i < NTHREADS; ++i)
    if (thrd_create(&t[i], waiter, 0) != thrd_success)
      exit(12);
  thrd_sleep(&(struct timespec){.tv_nsec = 50000000}, 0);  // let them block
  if (mtx_lock(&mtx) != thrd_success)
    exit(13);
  go = 1;
  if (cnd_broadcast(&cnd) != thrd_success)
    exit(14);
  if (mtx_unlock(&mtx) != thrd_success)
    exit(15);
  for (int i = 0; i < NTHREADS; ++i)
    if (thrd_join(t[i], 0) != thrd_success)
      exit(16);
  if (awake != NTHREADS)
    exit(17);
  cnd_destroy(&cnd);
  mtx_destroy(&mtx);
}
