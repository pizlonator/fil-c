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

// historical gotcha (glibc/musl both had variants): a condition wait whose
// absolute deadline is already in the past must return thrd_timedout right
// away. early implementations computed a relative timeout and, given a past
// deadline, either blocked forever or slept for a garbage interval. this must
// finish effectively instantly -- the test harness will kill a hang.

static mtx_t mtx;
static cnd_t cnd;

int main() {
  struct timespec past = {1, 0};  // ~1970, definitely in the past
  if (mtx_init(&mtx, mtx_plain) != thrd_success)
    exit(10);
  if (cnd_init(&cnd) != thrd_success)
    exit(11);
  if (mtx_lock(&mtx) != thrd_success)
    exit(12);
  if (cnd_timedwait(&cnd, &mtx, &past) != thrd_timedout)
    exit(13);
  if (mtx_unlock(&mtx) != thrd_success)  // mutex still owned after timeout
    exit(14);
  cnd_destroy(&cnd);
  mtx_destroy(&mtx);
}
