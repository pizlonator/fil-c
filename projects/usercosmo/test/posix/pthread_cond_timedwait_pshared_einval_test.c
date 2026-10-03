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

// a process-shared condition variable cannot use the nsync backend, so this
// exercises the futex "footek" path -- the one where tv_nsec validation now
// lives deep in the futex layer. two things must hold there:
//   - a malformed tv_nsec still surfaces EINVAL (not a bogus sleep or crash),
//   - because that path unlocks the mutex before waiting, the mutex must be
//     re-acquired before EINVAL returns, since a cond wait always returns with
//     the mutex held. the trailing valid wait would misbehave otherwise.

static pthread_mutex_t mu;
static pthread_cond_t cv;

int main() {
  pthread_mutexattr_t ma;
  pthread_condattr_t ca;
  struct timespec ts;

  if (pthread_mutexattr_init(&ma))
    exit(10);
  if (pthread_mutexattr_setpshared(&ma, PTHREAD_PROCESS_SHARED))
    exit(11);
  if (pthread_mutex_init(&mu, &ma))
    exit(12);
  if (pthread_mutexattr_destroy(&ma))
    exit(13);
  if (pthread_condattr_init(&ca))
    exit(14);
  if (pthread_condattr_setpshared(&ca, PTHREAD_PROCESS_SHARED))
    exit(15);
  if (pthread_cond_init(&cv, &ca))
    exit(16);
  if (pthread_condattr_destroy(&ca))
    exit(17);

  if (pthread_mutex_lock(&mu))
    exit(18);

  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += 1;
  ts.tv_nsec = 1000000000L;  // illegal
  if (pthread_cond_timedwait(&cv, &mu, &ts) != EINVAL)
    exit(19);

  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += 1;
  ts.tv_nsec = -1;  // illegal
  if (pthread_cond_timedwait(&cv, &mu, &ts) != EINVAL)
    exit(20);

  // mutex must still be ours after EINVAL: a valid wait blocks and times out
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_nsec += 100000000L;  // +100ms, valid
  if (ts.tv_nsec >= 1000000000L) {
    ts.tv_sec += 1;
    ts.tv_nsec -= 1000000000L;
  }
  if (pthread_cond_timedwait(&cv, &mu, &ts) != ETIMEDOUT)
    exit(21);

  if (pthread_mutex_unlock(&mu))
    exit(22);
  pthread_cond_destroy(&cv);
  pthread_mutex_destroy(&mu);
}
