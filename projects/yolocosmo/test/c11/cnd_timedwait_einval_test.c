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

// cnd_timedwait must reject a malformed deadline (tv_nsec outside
// [0, 1000000000)) with thrd_error rather than feeding garbage to the wait.
// the validation has to be enforced explicitly on every backend: a path that
// merely normalizes the value (e.g. carrying tv_nsec==1e9 into tv_sec) and
// relies on the kernel to complain would silently turn a bad argument into a
// real wait. a well-formed deadline, by contrast, must still time out, and
// every return -- error or timeout -- must leave the mutex held.

static mtx_t mtx;
static cnd_t cnd;

int main() {
  struct timespec ts;

  if (mtx_init(&mtx, mtx_plain) != thrd_success)
    exit(10);
  if (cnd_init(&cnd) != thrd_success)
    exit(11);
  if (mtx_lock(&mtx) != thrd_success)
    exit(12);

  timespec_get(&ts, TIME_UTC);
  ts.tv_sec += 1;
  ts.tv_nsec = 1000000000L;  // illegal: must be < 1e9
  if (cnd_timedwait(&cnd, &mtx, &ts) != thrd_error)
    exit(13);

  timespec_get(&ts, TIME_UTC);
  ts.tv_sec += 1;
  ts.tv_nsec = -1;  // illegal: must be >= 0
  if (cnd_timedwait(&cnd, &mtx, &ts) != thrd_error)
    exit(14);

  // control: a valid deadline is accepted and times out (mutex still held)
  timespec_get(&ts, TIME_UTC);
  ts.tv_nsec += 100000000L;  // +100ms
  if (ts.tv_nsec >= 1000000000L) {
    ts.tv_sec += 1;
    ts.tv_nsec -= 1000000000L;
  }
  if (cnd_timedwait(&cnd, &mtx, &ts) != thrd_timedout)
    exit(15);

  if (mtx_unlock(&mtx) != thrd_success)
    exit(16);
  cnd_destroy(&cnd);
  mtx_destroy(&mtx);
}
