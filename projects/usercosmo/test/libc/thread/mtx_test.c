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
#include "libc/testlib/testlib.h"
#include "libc/thread/thread.h"
#include "libc/thread/threads.h"

#define NTHREADS 8
#define NITER    20000

mtx_t g_mtx;
long g_counter;

//////////////////////////////////////////////////////////////////////////////
// init

TEST(mtx_init, badType_returnsError) {
  mtx_t m;
  ASSERT_EQ(thrd_error, mtx_init(&m, 0x99));
}

TEST(mtx_init, goodTypes_succeed) {
  mtx_t m;
  ASSERT_EQ(thrd_success, mtx_init(&m, mtx_plain));
  ASSERT_EQ(thrd_success, mtx_init(&m, mtx_timed));
  ASSERT_EQ(thrd_success, mtx_init(&m, mtx_plain | mtx_recursive));
  ASSERT_EQ(thrd_success, mtx_init(&m, mtx_timed | mtx_recursive));
}

//////////////////////////////////////////////////////////////////////////////
// basic lock / unlock / trylock

TEST(mtx_lock, plain) {
  mtx_t m;
  ASSERT_EQ(thrd_success, mtx_init(&m, mtx_plain));
  ASSERT_EQ(thrd_success, mtx_lock(&m));
  ASSERT_EQ(thrd_success, mtx_unlock(&m));
}

TEST(mtx_trylock, uncontended_succeeds) {
  mtx_t m;
  ASSERT_EQ(thrd_success, mtx_init(&m, mtx_plain));
  ASSERT_EQ(thrd_success, mtx_trylock(&m));
  ASSERT_EQ(thrd_success, mtx_unlock(&m));
}

static void *trylock_expect_busy(void *arg) {
  ASSERT_EQ(thrd_busy, mtx_trylock(&g_mtx));
  return 0;
}

TEST(mtx_trylock, contended_returnsBusy) {
  pthread_t th;
  ASSERT_EQ(thrd_success, mtx_init(&g_mtx, mtx_plain));
  ASSERT_EQ(thrd_success, mtx_lock(&g_mtx));
  ASSERT_EQ(0, pthread_create(&th, 0, trylock_expect_busy, 0));
  ASSERT_EQ(0, pthread_join(th, 0));
  ASSERT_EQ(thrd_success, mtx_unlock(&g_mtx));
}

//////////////////////////////////////////////////////////////////////////////
// mutual exclusion under contention

static void *inc_loop(void *arg) {
  for (int i = 0; i < NITER; ++i) {
    ASSERT_EQ(thrd_success, mtx_lock(&g_mtx));
    ++g_counter;
    ASSERT_EQ(thrd_success, mtx_unlock(&g_mtx));
  }
  return 0;
}

TEST(mtx, mutualExclusion) {
  pthread_t th[NTHREADS];
  g_counter = 0;
  ASSERT_EQ(thrd_success, mtx_init(&g_mtx, mtx_plain));
  for (int i = 0; i < NTHREADS; ++i)
    ASSERT_EQ(0, pthread_create(&th[i], 0, inc_loop, 0));
  for (int i = 0; i < NTHREADS; ++i)
    ASSERT_EQ(0, pthread_join(th[i], 0));
  ASSERT_EQ((long)NTHREADS * NITER, g_counter);
}

//////////////////////////////////////////////////////////////////////////////
// recursive

TEST(mtx, recursive_reentry) {
  mtx_t m;
  ASSERT_EQ(thrd_success, mtx_init(&m, mtx_plain | mtx_recursive));
  ASSERT_EQ(thrd_success, mtx_lock(&m));
  ASSERT_EQ(thrd_success, mtx_lock(&m));
  ASSERT_EQ(thrd_success, mtx_lock(&m));
  ASSERT_EQ(thrd_success, mtx_unlock(&m));
  ASSERT_EQ(thrd_success, mtx_unlock(&m));
  ASSERT_EQ(thrd_success, mtx_unlock(&m));
}

TEST(mtx, recursive_trylockReentry) {
  mtx_t m;
  ASSERT_EQ(thrd_success, mtx_init(&m, mtx_plain | mtx_recursive));
  ASSERT_EQ(thrd_success, mtx_lock(&m));
  ASSERT_EQ(thrd_success, mtx_trylock(&m));  // re-entrant trylock
  ASSERT_EQ(thrd_success, mtx_unlock(&m));
  ASSERT_EQ(thrd_success, mtx_unlock(&m));
}

TEST(mtx, recursive_excludesOtherThread) {
  pthread_t th;
  ASSERT_EQ(thrd_success, mtx_init(&g_mtx, mtx_plain | mtx_recursive));
  ASSERT_EQ(thrd_success, mtx_lock(&g_mtx));
  ASSERT_EQ(thrd_success, mtx_lock(&g_mtx));  // depth 2
  ASSERT_EQ(0, pthread_create(&th, 0, trylock_expect_busy, 0));
  ASSERT_EQ(0, pthread_join(th, 0));
  ASSERT_EQ(thrd_success, mtx_unlock(&g_mtx));
  ASSERT_EQ(thrd_success, mtx_unlock(&g_mtx));
}

//////////////////////////////////////////////////////////////////////////////
// timedlock

TEST(mtx_timedlock, freeLock_succeeds) {
  mtx_t m;
  struct timespec d = timespec_add(timespec_real(), timespec_frommillis(100));
  ASSERT_EQ(thrd_success, mtx_init(&m, mtx_timed));
  ASSERT_EQ(thrd_success, mtx_timedlock(&m, &d));
  ASSERT_EQ(thrd_success, mtx_unlock(&m));
}

TEST(mtx_timedlock, nullDeadline_behavesLikeLock) {
  mtx_t m;
  ASSERT_EQ(thrd_success, mtx_init(&m, mtx_timed));
  ASSERT_EQ(thrd_success, mtx_timedlock(&m, 0));
  ASSERT_EQ(thrd_success, mtx_unlock(&m));
}

static void *timedlock_expect_timeout(void *arg) {
  struct timespec d = timespec_add(timespec_real(), timespec_frommillis(100));
  ASSERT_EQ(thrd_timedout, mtx_timedlock(&g_mtx, &d));
  return 0;
}

TEST(mtx_timedlock, heldLock_timesOut) {
  pthread_t th;
  ASSERT_EQ(thrd_success, mtx_init(&g_mtx, mtx_timed));
  ASSERT_EQ(thrd_success, mtx_lock(&g_mtx));
  ASSERT_EQ(0, pthread_create(&th, 0, timedlock_expect_timeout, 0));
  ASSERT_EQ(0, pthread_join(th, 0));
  ASSERT_EQ(thrd_success, mtx_unlock(&g_mtx));
}

TEST(mtx_timedlock, recursive_reentry) {
  mtx_t m;
  struct timespec d = timespec_add(timespec_real(), timespec_frommillis(100));
  ASSERT_EQ(thrd_success, mtx_init(&m, mtx_timed | mtx_recursive));
  ASSERT_EQ(thrd_success, mtx_timedlock(&m, &d));
  ASSERT_EQ(thrd_success, mtx_timedlock(&m, &d));
  ASSERT_EQ(thrd_success, mtx_unlock(&m));
  ASSERT_EQ(thrd_success, mtx_unlock(&m));
}

//////////////////////////////////////////////////////////////////////////////
// destroy (kept last: an abort here shouldn't mask the functional tests)

TEST(mtx_destroy, plain) {
  mtx_t m;
  ASSERT_EQ(thrd_success, mtx_init(&m, mtx_plain));
  ASSERT_EQ(thrd_success, mtx_lock(&m));
  ASSERT_EQ(thrd_success, mtx_unlock(&m));
  mtx_destroy(&m);
}

TEST(mtx_destroy, recursive) {
  mtx_t m;
  ASSERT_EQ(thrd_success, mtx_init(&m, mtx_plain | mtx_recursive));
  ASSERT_EQ(thrd_success, mtx_lock(&m));
  ASSERT_EQ(thrd_success, mtx_unlock(&m));
  mtx_destroy(&m);
}
