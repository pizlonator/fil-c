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

// PTHREAD_MUTEX_ERRORCHECK turns the classic silent-deadlock and
// stray-unlock mistakes into reported errors: relocking a mutex the calling
// thread already holds must return EDEADLK (not hang), and unlocking a mutex
// the thread doesn't hold must return EPERM (not corrupt state). the mutex
// lives in static storage, which cosmo requires for error checking.

static pthread_mutex_t mu;

int main() {
  pthread_mutexattr_t attr;
  if (pthread_mutexattr_init(&attr))
    exit(10);
  if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK))
    exit(11);
  if (pthread_mutex_init(&mu, &attr))
    exit(12);
  if (pthread_mutexattr_destroy(&attr))
    exit(13);

  // unlocking a mutex that was never locked => EPERM
  if (pthread_mutex_unlock(&mu) != EPERM)
    exit(14);

  if (pthread_mutex_lock(&mu))
    exit(15);

  // the owner relocking it => EDEADLK instead of a hang
  if (pthread_mutex_lock(&mu) != EDEADLK)
    exit(16);

  if (pthread_mutex_unlock(&mu))
    exit(17);

  // unlocking again, now that it's free => EPERM
  if (pthread_mutex_unlock(&mu) != EPERM)
    exit(18);

  pthread_mutex_destroy(&mu);
}
