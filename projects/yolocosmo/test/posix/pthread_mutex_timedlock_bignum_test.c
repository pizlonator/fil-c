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

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

// implementations that convert an absolute deadline into a relative timeout
// have overflowed on far-future deadlines, wrapping to a negative/past value
// and returning a bogus immediate ETIMEDOUT. so block on a contended lock
// with a deadline a billion seconds out; when the holder releases ~100ms
// later the wait must succeed (return 0), not have falsely timed out.

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static int result;
static long elapsed_ms;

static void *contender(void *arg) {
  struct timespec a, b, d;
  clock_gettime(CLOCK_REALTIME, &a);
  d = a;
  d.tv_sec += 1000000000L;  // ~31 years out: huge, but a valid 64-bit time
  result = pthread_mutex_timedlock(&mu, &d);
  clock_gettime(CLOCK_REALTIME, &b);
  elapsed_ms = (b.tv_sec - a.tv_sec) * 1000 + (b.tv_nsec - a.tv_nsec) / 1000000;
  if (result == 0)
    pthread_mutex_unlock(&mu);
  return 0;
}

int main() {
  pthread_t t;
  struct timespec hold = {.tv_nsec = 100000000};  // 100ms

  if (pthread_mutex_lock(&mu))
    exit(10);
  if (pthread_create(&t, 0, contender, 0))
    exit(11);
  nanosleep(&hold, 0);
  if (pthread_mutex_unlock(&mu))  // hand the lock to the waiter
    exit(12);
  if (pthread_join(t, 0))
    exit(13);

  if (result != 0)  // must have acquired, not overflowed into a timeout
    exit(14);
  if (elapsed_ms < 50)  // it really did wait for the release
    exit(15);
}
