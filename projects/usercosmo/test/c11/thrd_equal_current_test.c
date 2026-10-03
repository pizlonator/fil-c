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

// thrd_current must return an identifier for the calling thread that
// thrd_equal recognizes: equal to itself, equal to the handle thrd_create
// produced, and distinct from other threads.

static thrd_t main_id;
static thrd_t worker_self;

static int worker(void *arg) {
  worker_self = thrd_current();
  return 0;
}

int main() {
  thrd_t t;
  main_id = thrd_current();
  if (!thrd_equal(main_id, thrd_current()))  // reflexive
    exit(10);
  if (thrd_create(&t, worker, 0) != thrd_success)
    exit(11);
  if (thrd_join(t, 0) != thrd_success)
    exit(12);
  if (thrd_equal(main_id, worker_self))  // a different thread is not equal
    exit(13);
  if (!thrd_equal(t, worker_self))  // create handle == that thread's own id
    exit(14);
}
