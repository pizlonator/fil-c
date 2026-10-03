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

// thrd_join must deliver the thread's result code, whether the thread
// returned it from its start function or passed it to thrd_exit.

static int via_return(void *arg) {
  return 42;
}

static int via_exit(void *arg) {
  thrd_exit(7);
  return 0;  // not reached
}

int main() {
  thrd_t t;
  int res;

  res = 0;
  if (thrd_create(&t, via_return, 0) != thrd_success)
    exit(10);
  if (thrd_join(t, &res) != thrd_success)
    exit(11);
  if (res != 42)
    exit(12);

  res = 0;
  if (thrd_create(&t, via_exit, 0) != thrd_success)
    exit(13);
  if (thrd_join(t, &res) != thrd_success)
    exit(14);
  if (res != 7)
    exit(15);

  // a NULL result pointer must be accepted and ignored
  if (thrd_create(&t, via_return, 0) != thrd_success)
    exit(16);
  if (thrd_join(t, 0) != thrd_success)
    exit(17);
}
