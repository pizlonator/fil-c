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

// posix: when a condition wait would block, a timespec whose tv_nsec is
// outside [0, 1000000000) must be rejected with EINVAL. implementations that
// skipped this validation passed the garbage to the kernel, producing absurd
// sleeps or undefined behavior. the mutex is held with no signaller, so each
// call would block if it weren't for the argument check.

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;

int main() {
  struct timespec ts;

  if (pthread_mutex_lock(&mu))
    exit(10);

  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += 1;
  ts.tv_nsec = 1000000000L;  // illegal: must be < 1e9
  if (pthread_cond_timedwait(&cv, &mu, &ts) != EINVAL)
    exit(11);

  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += 1;
  ts.tv_nsec = -1;  // illegal: must be >= 0
  if (pthread_cond_timedwait(&cv, &mu, &ts) != EINVAL)
    exit(12);

  // control: a well-formed deadline must NOT be rejected as EINVAL. with no
  // signaller it simply times out, proving the check is specific to malformed
  // nanoseconds rather than firing on any timed wait.
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_nsec += 100000000L;  // +100ms, a valid nanoseconds value
  if (ts.tv_nsec >= 1000000000L) {
    ts.tv_sec += 1;
    ts.tv_nsec -= 1000000000L;
  }
  if (pthread_cond_timedwait(&cv, &mu, &ts) != ETIMEDOUT)
    exit(13);

  if (pthread_mutex_unlock(&mu))  // EINVAL/ETIMEDOUT leave the mutex held
    exit(14);
}
