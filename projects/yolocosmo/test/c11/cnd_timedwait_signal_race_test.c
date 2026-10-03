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

// glibc bug 25847 was a lost-wakeup where a cnd_signal raced a waiter that
// was simultaneously timing out, and the signal got "stolen"/dropped, leaving
// a permit unconsumed forever. the dangerous window is exactly a timeout
// landing at the same instant as a signal, so here every waiter uses a very
// short deadline (constantly timing out and re-queuing) while a signaller
// hands out permits one at a time. every permit must be consumed exactly
// once; a dropped signal would deadlock or leave the tally short.

#define NWAIT 6
#define PERMITS 20000

static mtx_t mtx;
static cnd_t cnd;
static long available;  // permits available to consume
static long consumed;   // permits consumed so far

static struct timespec soon(void) {
  struct timespec ts;
  timespec_get(&ts, TIME_UTC);
  ts.tv_nsec += 1000000L;  // +1ms: short enough to race signals constantly
  if (ts.tv_nsec >= 1000000000L) {
    ts.tv_sec += 1;
    ts.tv_nsec -= 1000000000L;
  }
  return ts;
}

static int consumer(void *arg) {
  for (;;) {
    if (mtx_lock(&mtx) != thrd_success)
      exit(20);
    while (available == 0 && consumed < PERMITS) {
      struct timespec d = soon();
      int rc = cnd_timedwait(&cnd, &mtx, &d);
      if (rc != thrd_success && rc != thrd_timedout)
        exit(21);
    }
    if (available == 0) {  // implies all permits handed out and consumed
      if (mtx_unlock(&mtx) != thrd_success)
        exit(22);
      return 0;
    }
    --available;
    ++consumed;
    if (consumed == PERMITS)
      if (cnd_broadcast(&cnd) != thrd_success)  // release idle consumers
        exit(23);
    if (mtx_unlock(&mtx) != thrd_success)
      exit(24);
  }
}

int main() {
  thrd_t t[NWAIT];
  if (mtx_init(&mtx, mtx_plain) != thrd_success)
    exit(10);
  if (cnd_init(&cnd) != thrd_success)
    exit(11);
  for (int i = 0; i < NWAIT; ++i)
    if (thrd_create(&t[i], consumer, 0) != thrd_success)
      exit(12);
  for (long i = 0; i < PERMITS; ++i) {
    if (mtx_lock(&mtx) != thrd_success)
      exit(13);
    ++available;
    if (cnd_signal(&cnd) != thrd_success)
      exit(14);
    if (mtx_unlock(&mtx) != thrd_success)
      exit(15);
  }
  for (int i = 0; i < NWAIT; ++i)
    if (thrd_join(t[i], 0) != thrd_success)
      exit(16);
  if (consumed != PERMITS)
    exit(17);
  if (available != 0)
    exit(18);
  cnd_destroy(&cnd);
  mtx_destroy(&mtx);
}
