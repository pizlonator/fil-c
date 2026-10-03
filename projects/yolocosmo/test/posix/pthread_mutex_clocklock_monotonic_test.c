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

#define _GNU_SOURCE  // pthread_mutex_clocklock on glibc
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

// pthread_mutex_clocklock must honor its clock argument, not just realtime:
// a wait on a held lock with a CLOCK_MONOTONIC deadline must end in
// ETIMEDOUT measured against the monotonic clock.

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;

static void *contender(void *arg) {
  struct timespec d;
  clock_gettime(CLOCK_MONOTONIC, &d);
  d.tv_nsec += 100000000L;  // +100ms
  if (d.tv_nsec >= 1000000000L) {
    d.tv_sec += 1;
    d.tv_nsec -= 1000000000L;
  }
  if (pthread_mutex_clocklock(&mu, CLOCK_MONOTONIC, &d) != ETIMEDOUT)
    exit(20);
  return 0;
}

int main() {
  pthread_t t;
  if (pthread_mutex_lock(&mu))
    exit(10);
  if (pthread_create(&t, 0, contender, 0))
    exit(11);
  if (pthread_join(t, 0))
    exit(12);
  if (pthread_mutex_unlock(&mu))
    exit(13);
}
