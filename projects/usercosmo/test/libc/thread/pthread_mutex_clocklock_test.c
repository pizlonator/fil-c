/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2026 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/calls/struct/timespec.h"
#include "libc/cosmotime.h"
#include "libc/errno.h"
#include "libc/sysv/consts/clock.h"
#include "libc/testlib/testlib.h"
#include "libc/thread/thread.h"
#include "libc/thread/thread2.h"

pthread_mutex_t mu;

void *timedlock_expect_timeout(void *arg) {
  struct timespec d = timespec_add(timespec_real(), timespec_frommillis(100));
  ASSERT_EQ(ETIMEDOUT, pthread_mutex_timedlock(&mu, &d));
  return 0;
}

void *clocklock_mono_expect_timeout(void *arg) {
  struct timespec d = timespec_add(timespec_mono(), timespec_frommillis(100));
  ASSERT_EQ(ETIMEDOUT, pthread_mutex_clocklock(&mu, CLOCK_MONOTONIC, &d));
  return 0;
}

// POSIX: "Under no circumstance shall the function fail with a timeout
// if the mutex can be locked immediately."
TEST(pthread_mutex_timedlock, freeLock_acquiresEvenWhenDeadlinePassed) {
  struct timespec past = {1, 0};  // long ago
  ASSERT_EQ(0, pthread_mutex_init(&mu, 0));
  ASSERT_EQ(0, pthread_mutex_timedlock(&mu, &past));
  ASSERT_EQ(0, pthread_mutex_unlock(&mu));
  ASSERT_EQ(0, pthread_mutex_destroy(&mu));
}

TEST(pthread_mutex_timedlock, heldLock_timesOut) {
  pthread_t th;
  ASSERT_EQ(0, pthread_mutex_init(&mu, 0));
  ASSERT_EQ(0, pthread_mutex_lock(&mu));
  ASSERT_EQ(0, pthread_create(&th, 0, timedlock_expect_timeout, 0));
  ASSERT_EQ(0, pthread_join(th, 0));
  ASSERT_EQ(0, pthread_mutex_unlock(&mu));
  ASSERT_EQ(0, pthread_mutex_destroy(&mu));
}

TEST(pthread_mutex_clocklock, monotonic_heldLock_timesOut) {
  pthread_t th;
  ASSERT_EQ(0, pthread_mutex_init(&mu, 0));
  ASSERT_EQ(0, pthread_mutex_lock(&mu));
  ASSERT_EQ(0, pthread_create(&th, 0, clocklock_mono_expect_timeout, 0));
  ASSERT_EQ(0, pthread_join(th, 0));
  ASSERT_EQ(0, pthread_mutex_unlock(&mu));
  ASSERT_EQ(0, pthread_mutex_destroy(&mu));
}

TEST(pthread_mutex_timedlock, recursive) {
  pthread_mutexattr_t attr;
  struct timespec d = timespec_add(timespec_real(), timespec_frommillis(100));
  ASSERT_EQ(0, pthread_mutexattr_init(&attr));
  ASSERT_EQ(0, pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE));
  ASSERT_EQ(0, pthread_mutex_init(&mu, &attr));
  ASSERT_EQ(0, pthread_mutexattr_destroy(&attr));
  ASSERT_EQ(0, pthread_mutex_timedlock(&mu, &d));
  ASSERT_EQ(0, pthread_mutex_timedlock(&mu, &d));  // recursive re-entry
  ASSERT_EQ(0, pthread_mutex_unlock(&mu));
  ASSERT_EQ(0, pthread_mutex_unlock(&mu));
  ASSERT_EQ(0, pthread_mutex_destroy(&mu));
}

TEST(pthread_mutex_timedlock, nullDeadline_blocksLikeLock) {
  ASSERT_EQ(0, pthread_mutex_init(&mu, 0));
  ASSERT_EQ(0, pthread_mutex_timedlock(&mu, 0));  // no deadline => plain lock
  ASSERT_EQ(0, pthread_mutex_unlock(&mu));
  ASSERT_EQ(0, pthread_mutex_destroy(&mu));
}

// process-shared mutexes can't use nsync, so they exercise the drepper futex
// path; make sure timed locking and both clocks still behave there

static void pshared_init(void) {
  pthread_mutexattr_t attr;
  ASSERT_EQ(0, pthread_mutexattr_init(&attr));
  ASSERT_EQ(0, pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED));
  ASSERT_EQ(0, pthread_mutex_init(&mu, &attr));
  ASSERT_EQ(0, pthread_mutexattr_destroy(&attr));
}

TEST(pthread_mutex_timedlock, psharedHeldLock_timesOut) {
  pthread_t th;
  pshared_init();
  ASSERT_EQ(0, pthread_mutex_lock(&mu));
  ASSERT_EQ(0, pthread_create(&th, 0, timedlock_expect_timeout, 0));
  ASSERT_EQ(0, pthread_join(th, 0));
  ASSERT_EQ(0, pthread_mutex_unlock(&mu));
  ASSERT_EQ(0, pthread_mutex_destroy(&mu));
}

TEST(pthread_mutex_clocklock, psharedMonotonicHeldLock_timesOut) {
  pthread_t th;
  pshared_init();
  ASSERT_EQ(0, pthread_mutex_lock(&mu));
  ASSERT_EQ(0, pthread_create(&th, 0, clocklock_mono_expect_timeout, 0));
  ASSERT_EQ(0, pthread_join(th, 0));
  ASSERT_EQ(0, pthread_mutex_unlock(&mu));
  ASSERT_EQ(0, pthread_mutex_destroy(&mu));
}

TEST(pthread_mutex_timedlock, psharedFreeLock_acquiresEvenWhenExpired) {
  struct timespec past = {1, 0};
  pshared_init();
  ASSERT_EQ(0, pthread_mutex_timedlock(&mu, &past));
  ASSERT_EQ(0, pthread_mutex_unlock(&mu));
  ASSERT_EQ(0, pthread_mutex_destroy(&mu));
}
