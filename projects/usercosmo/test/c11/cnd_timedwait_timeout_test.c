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

// cnd_timedwait must return thrd_timedout when no signal arrives before the
// deadline. crucially, posix requires the mutex to be re-acquired on return
// even after a timeout, so the subsequent mtx_unlock must be legal.

static mtx_t mtx;
static cnd_t cnd;

int main() {
  struct timespec d;
  if (mtx_init(&mtx, mtx_plain) != thrd_success)
    exit(10);
  if (cnd_init(&cnd) != thrd_success)
    exit(11);
  if (mtx_lock(&mtx) != thrd_success)
    exit(12);
  timespec_get(&d, TIME_UTC);
  d.tv_nsec += 100000000L;  // +100ms
  if (d.tv_nsec >= 1000000000L) {
    d.tv_sec += 1;
    d.tv_nsec -= 1000000000L;
  }
  // nobody ever signals => must time out
  if (cnd_timedwait(&cnd, &mtx, &d) != thrd_timedout)
    exit(13);
  // mutex must still be held by us
  if (mtx_unlock(&mtx) != thrd_success)
    exit(14);
  cnd_destroy(&cnd);
  mtx_destroy(&mtx);
}
