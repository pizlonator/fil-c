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

// two paired gotchas around an already-expired absolute deadline:
//   - a FREE lock must still be acquired (a timed lock may never report a
//     timeout when it could have locked without waiting), and
//   - a HELD lock must report the timeout promptly instead of blocking
//     forever on a deadline that has already passed.

static mtx_t mtx;

static int contender(void *arg) {
  struct timespec past = {1, 0};
  if (mtx_timedlock(&mtx, &past) != thrd_timedout)  // held + expired
    exit(20);
  return 0;
}

int main() {
  thrd_t t;
  struct timespec past = {1, 0};
  if (mtx_init(&mtx, mtx_timed) != thrd_success)
    exit(10);

  // free lock with an expired deadline must be acquired anyway
  if (mtx_timedlock(&mtx, &past) != thrd_success)
    exit(11);

  // now it's held; another thread must time out at once, not hang
  if (thrd_create(&t, contender, 0) != thrd_success)
    exit(12);
  if (thrd_join(t, 0) != thrd_success)
    exit(13);

  if (mtx_unlock(&mtx) != thrd_success)
    exit(14);
  mtx_destroy(&mtx);
}
