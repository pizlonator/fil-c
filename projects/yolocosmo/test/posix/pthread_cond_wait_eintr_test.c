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

// signals delivered to a thread parked in pthread_cond_wait must not crash a
// semaphore-based backend, and must not be mistaken for a real notification:
// with the predicate still false the thread has to keep waiting. it proceeds
// only after an actual cond signal that sets the predicate.

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static int ready;
static atomic_int proceeded;

static void on_signal(int sig) {
  (void)sig;
}

static void *waiter(void *arg) {
  if (pthread_mutex_lock(&mu))
    exit(20);
  while (!ready)
    if (pthread_cond_wait(&cv, &mu))
      exit(21);
  atomic_store_explicit(&proceeded, 1, memory_order_release);
  if (pthread_mutex_unlock(&mu))
    exit(22);
  return 0;
}

int main() {
  pthread_t t;
  struct sigaction sa = {0};
  struct timespec gap = {.tv_nsec = 50000000};  // 50ms

  sa.sa_handler = on_signal;
  if (sigaction(SIGUSR1, &sa, 0))
    exit(10);
  if (pthread_create(&t, 0, waiter, 0))
    exit(11);

  for (int i = 0; i < 6; ++i) {
    nanosleep(&gap, 0);
    pthread_kill(t, SIGUSR1);  // stray signals: predicate is false
  }
  if (atomic_load_explicit(&proceeded, memory_order_acquire))
    exit(12);  // a signal must not satisfy the predicate

  if (pthread_mutex_lock(&mu))
    exit(13);
  ready = 1;
  if (pthread_cond_signal(&cv))
    exit(14);
  if (pthread_mutex_unlock(&mu))
    exit(15);
  if (pthread_join(t, 0))
    exit(16);
  if (!atomic_load_explicit(&proceeded, memory_order_acquire))
    exit(17);
}
