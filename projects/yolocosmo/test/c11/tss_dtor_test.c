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

#include <stdatomic.h>
#include <stdlib.h>
#include <threads.h>

// when a thread finishes, the tss destructor must run for keys whose value is
// non-NULL, and must NOT run for keys left NULL. here one thread sets a value
// and one leaves it unset, so the destructor must fire exactly once.

static tss_t key;
static atomic_int dtor_runs;

static void dtor(void *p) {
  if (p != NULL)
    atomic_fetch_add_explicit(&dtor_runs, 1, memory_order_relaxed);
}

static int setter(void *arg) {
  if (tss_set(key, (void *)42) != thrd_success)
    exit(20);
  return 0;
}

static int leaver(void *arg) {
  (void)tss_get(key);  // touches the key but never sets it
  return 0;
}

int main() {
  thrd_t a, b;
  if (tss_create(&key, dtor) != thrd_success)
    exit(10);
  if (thrd_create(&a, setter, 0) != thrd_success)
    exit(11);
  if (thrd_create(&b, leaver, 0) != thrd_success)
    exit(12);
  if (thrd_join(a, 0) != thrd_success)
    exit(13);
  if (thrd_join(b, 0) != thrd_success)
    exit(14);
  if (atomic_load_explicit(&dtor_runs, memory_order_relaxed) != 1)
    exit(15);
  tss_delete(key);
}
