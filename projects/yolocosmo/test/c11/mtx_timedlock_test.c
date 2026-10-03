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

// mtx_timedlock must acquire a free lock immediately, and give up with
// thrd_timedout (not deadlock, not succeed) when the lock is held past its
// absolute deadline. the deadline is on the realtime clock per c11.

static mtx_t mtx;

static struct timespec in_ms(long ms) {
  struct timespec ts;
  timespec_get(&ts, TIME_UTC);
  ts.tv_sec += ms / 1000;
  ts.tv_nsec += (ms % 1000) * 1000000L;
  if (ts.tv_nsec >= 1000000000L) {
    ts.tv_sec += 1;
    ts.tv_nsec -= 1000000000L;
  }
  return ts;
}

static int contender(void *arg) {
  struct timespec d = in_ms(100);
  if (mtx_timedlock(&mtx, &d) != thrd_timedout)
    exit(20);
  return 0;
}

int main() {
  thrd_t t;
  struct timespec d;
  if (mtx_init(&mtx, mtx_timed) != thrd_success)
    exit(10);
  d = in_ms(1000);
  if (mtx_timedlock(&mtx, &d) != thrd_success)  // free => acquire promptly
    exit(11);
  if (thrd_create(&t, contender, 0) != thrd_success)  // held => time out
    exit(12);
  if (thrd_join(t, 0) != thrd_success)
    exit(13);
  if (mtx_unlock(&mtx) != thrd_success)
    exit(14);
  mtx_destroy(&mtx);
}
