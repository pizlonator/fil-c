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

#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <time.h>

// a plain (untimed) pthread_mutex_lock is not a cancelation point and
// is not interruptible. on semaphore-based backends a signal makes the
// underlying wait return EINTR, which a naive implementation would
// surface or assert on. the blocked thread must instead ignore the
// signals, stay blocked while the lock is held, and acquire only once
// it is released.

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static atomic_int acquired;

static void on_signal(int sig) {
  (void)sig;
}

static void *contender(void *arg) {
  if (pthread_mutex_lock(&mu))  // blocks until main unlocks
    exit(20);
  atomic_store_explicit(&acquired, 1, memory_order_release);
  if (pthread_mutex_unlock(&mu))
    exit(21);
  return 0;
}

int main() {
  pthread_t t;
  struct sigaction sa = {0};
  struct timespec gap = {.tv_nsec = 50000000};  // 50ms

  sa.sa_handler = on_signal;  // no SA_RESTART: signals interrupt the syscall
  if (sigaction(SIGUSR1, &sa, 0))
    exit(10);

  if (pthread_mutex_lock(&mu))
    exit(11);
  if (pthread_create(&t, 0, contender, 0))
    exit(12);

  for (int i = 0; i < 6; ++i) {
    nanosleep(&gap, 0);
    pthread_kill(t, SIGUSR1);  // must neither crash nor wake it into the lock
  }
  if (atomic_load_explicit(&acquired, memory_order_acquire))
    exit(13);  // it acquired while we still held the mutex!

  if (pthread_mutex_unlock(&mu))
    exit(14);
  if (pthread_join(t, 0))
    exit(15);
  if (!atomic_load_explicit(&acquired, memory_order_acquire))
    exit(16);  // it never acquired after release
}
