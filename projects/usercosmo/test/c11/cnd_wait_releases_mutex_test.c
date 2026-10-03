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

// cnd_wait must atomically release the mutex while blocked: another thread
// has to be able to acquire that same mutex. if cnd_wait failed to release
// it, main's mtx_lock below would deadlock and the test would hang.

static mtx_t mtx;
static cnd_t cnd;
static int go;
static int main_took_mutex;

static int waiter(void *arg) {
  if (mtx_lock(&mtx) != thrd_success)
    exit(20);
  while (!go)
    if (cnd_wait(&cnd, &mtx) != thrd_success)
      exit(21);
  if (!main_took_mutex)  // main must have held the mutex during our wait
    exit(22);
  if (mtx_unlock(&mtx) != thrd_success)
    exit(23);
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
  thrd_sleep(&(struct timespec){.tv_nsec = 50000000}, 0);  // let it enter wait
  if (mtx_lock(&mtx) != thrd_success)  // hangs if cnd_wait didn't release it
    exit(13);
  main_took_mutex = 1;
  go = 1;
  if (cnd_signal(&cnd) != thrd_success)
    exit(14);
  if (mtx_unlock(&mtx) != thrd_success)
    exit(15);
  if (thrd_join(t, 0) != thrd_success)
    exit(16);
  cnd_destroy(&cnd);
  mtx_destroy(&mtx);
}
