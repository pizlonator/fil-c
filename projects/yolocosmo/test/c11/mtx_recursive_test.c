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

// a recursive c11 mutex must let its owner relock it many times, demand a
// matching number of unlocks, and still exclude other threads the entire
// time it is held even once.

#define DEPTH 50

static mtx_t mtx;
static int saw_busy;

static int contender(void *arg) {
  // owner is holding it recursively; we must not be able to grab it
  if (mtx_trylock(&mtx) != thrd_busy)
    exit(20);
  saw_busy = 1;
  return 0;
}

int main() {
  thrd_t t;
  if (mtx_init(&mtx, mtx_plain | mtx_recursive) != thrd_success)
    exit(10);
  for (int i = 0; i < DEPTH; ++i)
    if (mtx_lock(&mtx) != thrd_success)
      exit(11);
  if (thrd_create(&t, contender, 0) != thrd_success)
    exit(12);
  if (thrd_join(t, 0) != thrd_success)
    exit(13);
  if (!saw_busy)
    exit(14);
  for (int i = 0; i < DEPTH; ++i)
    if (mtx_unlock(&mtx) != thrd_success)
      exit(15);
  mtx_destroy(&mtx);
}
