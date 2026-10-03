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

// two threads hand a token back and forth many times through one condition
// variable. each handoff depends on the previous wakeup actually arriving, so
// a single lost or misdirected signal stalls the whole exchange forever.

#define ROUNDS 100000

static mtx_t mtx;
static cnd_t cnd;
static int turn;  // 0 => ping may go, 1 => pong may go
static long pings, pongs;

static int ping(void *arg) {
  for (int i = 0; i < ROUNDS; ++i) {
    if (mtx_lock(&mtx) != thrd_success)
      exit(20);
    while (turn != 0)
      if (cnd_wait(&cnd, &mtx) != thrd_success)
        exit(21);
    ++pings;
    turn = 1;
    if (cnd_signal(&cnd) != thrd_success)
      exit(22);
    if (mtx_unlock(&mtx) != thrd_success)
      exit(23);
  }
  return 0;
}

static int pong(void *arg) {
  for (int i = 0; i < ROUNDS; ++i) {
    if (mtx_lock(&mtx) != thrd_success)
      exit(30);
    while (turn != 1)
      if (cnd_wait(&cnd, &mtx) != thrd_success)
        exit(31);
    ++pongs;
    turn = 0;
    if (cnd_signal(&cnd) != thrd_success)
      exit(32);
    if (mtx_unlock(&mtx) != thrd_success)
      exit(33);
  }
  return 0;
}

int main() {
  thrd_t a, b;
  if (mtx_init(&mtx, mtx_plain) != thrd_success)
    exit(10);
  if (cnd_init(&cnd) != thrd_success)
    exit(11);
  if (thrd_create(&a, ping, 0) != thrd_success)
    exit(12);
  if (thrd_create(&b, pong, 0) != thrd_success)
    exit(13);
  if (thrd_join(a, 0) != thrd_success)
    exit(14);
  if (thrd_join(b, 0) != thrd_success)
    exit(15);
  if (pings != ROUNDS || pongs != ROUNDS)
    exit(16);
  cnd_destroy(&cnd);
  mtx_destroy(&mtx);
}
