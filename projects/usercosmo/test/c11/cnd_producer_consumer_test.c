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

// bounded-buffer torture test. producers and consumers coordinate entirely
// through two condition variables; every produced item must be consumed
// exactly once. a lost wakeup deadlocks, and broken mutual exclusion makes
// the final count disagree.

#define PRODUCERS    4
#define CONSUMERS    4
#define PER_PRODUCER 25000
#define CAP          16
#define TOTAL        ((long)PRODUCERS * PER_PRODUCER)

static mtx_t mtx;
static cnd_t not_full;
static cnd_t not_empty;
static int count;     // items currently buffered
static long consumed; // items removed so far

static int producer(void *arg) {
  for (int i = 0; i < PER_PRODUCER; ++i) {
    if (mtx_lock(&mtx) != thrd_success)
      exit(20);
    while (count == CAP)
      if (cnd_wait(&not_full, &mtx) != thrd_success)
        exit(21);
    ++count;
    if (cnd_signal(&not_empty) != thrd_success)
      exit(22);
    if (mtx_unlock(&mtx) != thrd_success)
      exit(23);
  }
  return 0;
}

static int consumer(void *arg) {
  for (;;) {
    if (mtx_lock(&mtx) != thrd_success)
      exit(30);
    while (count == 0 && consumed < TOTAL)
      if (cnd_wait(&not_empty, &mtx) != thrd_success)
        exit(31);
    if (count == 0) {  // implies consumed == TOTAL: nothing left, ever
      if (mtx_unlock(&mtx) != thrd_success)
        exit(32);
      return 0;
    }
    --count;
    ++consumed;
    if (cnd_signal(&not_full) != thrd_success)
      exit(33);
    if (consumed == TOTAL)  // wake idle consumers so they can exit
      if (cnd_broadcast(&not_empty) != thrd_success)
        exit(34);
    if (mtx_unlock(&mtx) != thrd_success)
      exit(35);
  }
}

int main() {
  thrd_t p[PRODUCERS], c[CONSUMERS];
  if (mtx_init(&mtx, mtx_plain) != thrd_success)
    exit(10);
  if (cnd_init(&not_full) != thrd_success)
    exit(11);
  if (cnd_init(&not_empty) != thrd_success)
    exit(12);
  for (int i = 0; i < CONSUMERS; ++i)
    if (thrd_create(&c[i], consumer, 0) != thrd_success)
      exit(13);
  for (int i = 0; i < PRODUCERS; ++i)
    if (thrd_create(&p[i], producer, 0) != thrd_success)
      exit(14);
  for (int i = 0; i < PRODUCERS; ++i)
    if (thrd_join(p[i], 0) != thrd_success)
      exit(15);
  for (int i = 0; i < CONSUMERS; ++i)
    if (thrd_join(c[i], 0) != thrd_success)
      exit(16);
  if (consumed != TOTAL)
    exit(17);
  if (count != 0)
    exit(18);
  cnd_destroy(&not_empty);
  cnd_destroy(&not_full);
  mtx_destroy(&mtx);
}
