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

// mtx_trylock must succeed on a free lock and return thrd_busy (never block,
// never falsely succeed) when another thread holds it.

static mtx_t mtx;

static int contender(void *arg) {
  if (mtx_trylock(&mtx) != thrd_busy)
    exit(20);
  return 0;
}

int main() {
  thrd_t t;
  if (mtx_init(&mtx, mtx_plain) != thrd_success)
    exit(10);
  if (mtx_trylock(&mtx) != thrd_success)  // free => acquire
    exit(11);
  if (thrd_create(&t, contender, 0) != thrd_success)  // held by us
    exit(12);
  if (thrd_join(t, 0) != thrd_success)
    exit(13);
  if (mtx_unlock(&mtx) != thrd_success)
    exit(14);
  mtx_destroy(&mtx);
}
