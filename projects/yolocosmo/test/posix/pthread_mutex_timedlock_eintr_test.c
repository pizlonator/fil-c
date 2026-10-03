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
#include <signal.h>
#include <stdlib.h>
#include <time.h>

// posix: pthread_mutex_timedlock "shall not return an error code of
// EINTR". futex-based implementations historically let a signal
// interrupt the wait and bubbled up early. so we pepper a blocked
// timed-lock with signals and require it to ignore them, returning
// ETIMEDOUT only when its deadline genuinely passes (not cut short).

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static long elapsed_ms;
static int result;

static void on_signal(int sig) {
  (void)sig;  // do nothing; presence of a handler is enough to interrupt
}

static void *contender(void *arg) {
  struct timespec a, b, d;
  clock_gettime(CLOCK_REALTIME, &a);
  d = a;
  d.tv_nsec += 400000000L;  // +400ms
  if (d.tv_nsec >= 1000000000L) {
    d.tv_sec += 1;
    d.tv_nsec -= 1000000000L;
  }
  result = pthread_mutex_timedlock(&mu, &d);
  clock_gettime(CLOCK_REALTIME, &b);
  elapsed_ms = (b.tv_sec - a.tv_sec) * 1000 + (b.tv_nsec - a.tv_nsec) / 1000000;
  return 0;
}

int main() {
  pthread_t t;
  struct sigaction sa = {0};
  struct timespec gap = {.tv_nsec = 50000000};  // 50ms between signals

  sa.sa_handler = on_signal;  // no SA_RESTART: we want it to interrupt syscalls
  if (sigaction(SIGUSR1, &sa, 0))
    exit(10);

  if (pthread_mutex_lock(&mu))
    exit(11);
  if (pthread_create(&t, 0, contender, 0))
    exit(12);

  // hammer the waiting thread with signals well before its 400ms deadline
  for (int i = 0; i < 6; ++i) {
    nanosleep(&gap, 0);
    pthread_kill(t, SIGUSR1);
  }

  if (pthread_join(t, 0))
    exit(13);
  if (pthread_mutex_unlock(&mu))
    exit(14);

  if (result != ETIMEDOUT)  // never EINTR, never a stray success
    exit(15);
  if (elapsed_ms < 350)  // the signals must not have cut the wait short
    exit(16);
}
