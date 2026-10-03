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
#include "libc/calls/blockcancel.internal.h"
#include "libc/calls/state.internal.h"
#include "libc/calls/struct/timespec.h"
#include "libc/calls/weirdtypes.h"
#include "libc/cosmo.h"
#include "libc/cosmotime.h"
#include "libc/dce.h"
#include "libc/errno.h"
#include "libc/intrin/atomic.h"
#include "libc/intrin/describeflags.h"
#include "libc/intrin/strace.h"
#include "libc/intrin/weaken.h"
#include "libc/runtime/internal.h"
#include "libc/sysv/consts/clock.h"
#include "libc/sysv/pib.h"
#include "libc/thread/lock.h"
#include "libc/thread/posixthread.internal.h"
#include "libc/thread/thread.h"
#include "libc/thread/thread2.h"
#include "libc/thread/tls.h"
#include "third_party/nsync/mu.h"
#include "third_party/nsync/time.h"
#include "third_party/nsync/timed.h"

#if PTHREAD_USE_NSYNC
// guarantee other posix mutex functions will dispatch to same algorithm
__static_yoink("nsync_mu_lock");
__static_yoink("nsync_mu_unlock");
__static_yoink("nsync_mu_trylock");
#endif

static errno_t pthread_mutex_lock_normal_success(pthread_mutex_t *mutex,
                                                 uint64_t word) {
  if (IsModeDbg() || MUTEX_TYPE(word) == PTHREAD_MUTEX_ERRORCHECK) {
    __deadlock_track(mutex, MUTEX_TYPE(word) == PTHREAD_MUTEX_ERRORCHECK);
    __deadlock_record(mutex, MUTEX_TYPE(word) == PTHREAD_MUTEX_ERRORCHECK);
  }
  return 0;
}

// returns true if abstime has already passed on the given clock
static bool pthread_mutex_clocklock_expired(int clock,
                                            const struct timespec *abstime) {
  struct timespec now;
  if (!abstime)
    return false;
  if (clock_gettime(clock, &now))
    return true;
  return timespec_cmp(now, *abstime) >= 0;
}

// see "take 3" algorithm in "futexes are tricky" by ulrich drepper
static errno_t pthread_mutex_clocklock_drepper(pthread_mutex_t *mutex,
                                               uint64_t word, int clock,
                                               const struct timespec *abstime) {
  int val = 0;
  bool timedout = false;
  if (atomic_compare_exchange_strong_explicit(
          &mutex->_futex, &val, 1, memory_order_acquire, memory_order_acquire))
    return pthread_mutex_lock_normal_success(mutex, word);
  LOCKTRACE("acquiring pthread_mutex_clocklock_drepper(%t)...", mutex);
  if (val == 1)
    val = atomic_exchange_explicit(&mutex->_futex, 2, memory_order_acquire);
  BLOCK_CANCELATION;
  while (val > 0) {
    int rc = cosmo_futex_wait(&mutex->_futex, 2, MUTEX_PSHARED(word), clock,
                              abstime);
    val = atomic_exchange_explicit(&mutex->_futex, 2, memory_order_acquire);
    if (val == 0)
      break;  // acquired the lock (it was released out from under the wait)
    if (rc == -ETIMEDOUT) {
      timedout = true;
      break;
    }
  }
  ALLOW_CANCELATION;
  if (timedout)
    return ETIMEDOUT;
  return pthread_mutex_lock_normal_success(mutex, word);
}

static errno_t pthread_mutex_clocklock_recursive(
    pthread_mutex_t *mutex, uint64_t word, int clock,
    const struct timespec *abstime) {
  uint64_t lock;
  int backoff = 0;
  int me = atomic_load_explicit(&__get_tls()->tib_ptid, memory_order_relaxed);
  bool once = false;
  for (;;) {
    if (MUTEX_OWNER(word) == me) {
      if (MUTEX_DEPTH(word) < MUTEX_DEPTH_MAX) {
        if (atomic_compare_exchange_weak_explicit(
                &mutex->_word, &word, MUTEX_INC_DEPTH(word),
                memory_order_relaxed, memory_order_relaxed))
          return 0;
        continue;
      } else {
        return EAGAIN;
      }
    }
    if (IsModeDbg())
      __deadlock_check(mutex, 0);
    word = MUTEX_UNLOCK(word);
    lock = MUTEX_LOCK(word);
    lock = MUTEX_SET_OWNER(lock, me);
    if (atomic_compare_exchange_weak_explicit(&mutex->_word, &word, lock,
                                              memory_order_acquire,
                                              memory_order_relaxed)) {
      if (IsModeDbg()) {
        __deadlock_track(mutex, 0);
        __deadlock_record(mutex, 0);
      }
      mutex->_pid = __get_pib()->pid;
      return 0;
    }
    if (!once) {
      LOCKTRACE("acquiring pthread_mutex_clocklock_recursive(%t)...", mutex);
      once = true;
    }
    for (;;) {
      word = atomic_load_explicit(&mutex->_word, memory_order_relaxed);
      if (MUTEX_OWNER(word) == me)
        break;
      if (word == MUTEX_UNLOCK(word))
        break;
      if (pthread_mutex_clocklock_expired(clock, abstime))
        return ETIMEDOUT;
      backoff = pthread_delay_np(mutex, backoff);
    }
  }
}

#if PTHREAD_USE_NSYNC
static errno_t pthread_mutex_clocklock_recursive_nsync(
    pthread_mutex_t *mutex, int clock, const struct timespec *abstime) {
  uint64_t word = atomic_load_explicit(&mutex->_word, memory_order_relaxed);
  int me = atomic_load_explicit(&__get_tls()->tib_ptid, memory_order_relaxed);
  for (;;) {
    if (MUTEX_OWNER(word) == me) {
      if (MUTEX_DEPTH(word) < MUTEX_DEPTH_MAX) {
        if (atomic_compare_exchange_weak_explicit(
                &mutex->_word, &word, MUTEX_INC_DEPTH(word),
                memory_order_relaxed, memory_order_relaxed))
          return 0;
        continue;
      } else {
        return EAGAIN;
      }
    }
    if (IsModeDbg())
      __deadlock_check(mutex, 0);
    errno_t err =
        nsync_mu_clocklock((nsync_mu *)mutex->_nsync, clock,
                           abstime ? *abstime : nsync_time_no_deadline);
    if (err)
      return err;  // ETIMEDOUT
    if (IsModeDbg()) {
      __deadlock_track(mutex, 0);
      __deadlock_record(mutex, 0);
    }
    word = MUTEX_UNLOCK(word);
    word = MUTEX_LOCK(word);
    word = MUTEX_SET_OWNER(word, me);
    mutex->_word = word;
    mutex->_pid = __get_pib()->pid;
    return 0;
  }
}
#endif

dontinline static errno_t pthread_mutex_clocklock_impl(
    pthread_mutex_t *mutex, int clock, const struct timespec *abstime) {
  uint64_t word = atomic_load_explicit(&mutex->_word, memory_order_relaxed);

  // handle recursive mutexes
  if (MUTEX_TYPE(word) == PTHREAD_MUTEX_RECURSIVE) {
#if PTHREAD_USE_NSYNC
    if (MUTEX_PSHARED(word) == PTHREAD_PROCESS_PRIVATE) {
      return pthread_mutex_clocklock_recursive_nsync(mutex, clock, abstime);
    } else {
      return pthread_mutex_clocklock_recursive(mutex, word, clock, abstime);
    }
#else
    return pthread_mutex_clocklock_recursive(mutex, word, clock, abstime);
#endif
  }

  // check if normal mutex is already owned by calling thread
  if (MUTEX_TYPE(word) == PTHREAD_MUTEX_ERRORCHECK ||
      (IsModeDbg() && MUTEX_TYPE(word) == PTHREAD_MUTEX_DEFAULT)) {
    if (__deadlock_tracked(mutex) == 1) {
      if (IsModeDbg() && MUTEX_TYPE(word) != PTHREAD_MUTEX_ERRORCHECK) {
        npassert(!"error: attempted to lock non-recursive mutex that's already "
                  "held by the calling thread\n");
      }
      return EDEADLK;
    }
  }

  // check if locking will create cycle in lock graph
  if (IsModeDbg() || MUTEX_TYPE(word) == PTHREAD_MUTEX_ERRORCHECK)
    if (__deadlock_check(mutex, MUTEX_TYPE(word) == PTHREAD_MUTEX_ERRORCHECK))
      return EDEADLK;

#if PTHREAD_USE_NSYNC
  // use superior mutexes if possible
  if (MUTEX_PSHARED(word) == PTHREAD_PROCESS_PRIVATE) {
    // on apple silicon we should just put our faith in ulock
    // otherwise *nsync gets struck down by the eye of sauron
    if (!IsXnuSilicon()) {
      errno_t err =
          nsync_mu_clocklock((nsync_mu *)mutex->_nsync, clock,
                             abstime ? *abstime : nsync_time_no_deadline);
      if (err)
        return err;  // ETIMEDOUT
      return pthread_mutex_lock_normal_success(mutex, word);
    }
  }
#endif

  // isc licensed non-recursive mutex implementation
  return pthread_mutex_clocklock_drepper(mutex, word, clock, abstime);
}

/**
 * Locks mutex, giving up after a deadline, e.g.
 *
 *     struct timespec ts;  // one second timeout
 *     ts = timespec_add(timespec_mono(), timespec_frommillis(1000));
 *     if (pthread_mutex_clocklock(&lock, CLOCK_MONOTONIC, &ts) == ETIMEDOUT) {
 *       // handle timeout...
 *     }
 *
 * This behaves exactly like pthread_mutex_lock(), except that the wait for a
 * contended lock is abandoned once `abstime` passes on the clock named by
 * `clock`. If the lock can be acquired immediately, `abstime` is not even
 * examined and the lock is taken regardless of whether the deadline has
 * already passed.
 *
 * @param clock should be `CLOCK_REALTIME` or `CLOCK_MONOTONIC`
 * @param abstime is an absolute deadline, or null to wait forever, in
 *     which case this is identical to pthread_mutex_lock()
 * @return 0 on success, or error number on failure
 * @raise ETIMEDOUT if the lock couldn't be acquired before `abstime`
 * @raise EDEADLK if mutex is `PTHREAD_MUTEX_ERRORCHECK` and the calling
 *     thread already holds it, or a cycle is detected in the lock graph
 * @raise EAGAIN if mutex is recursive and the maximum lock depth was hit
 * @raise EINVAL if `abstime->tv_nsec` is not in `[0,1000000000)`
 * @see pthread_mutex_lock()
 * @see pthread_mutex_timedlock()
 */
errno_t pthread_mutex_clocklock(pthread_mutex_t *mutex, int clock,
                                const struct timespec *abstime) {
  errno_t err;
  FORBIDDEN_IN_POSIX_SPAWN;
  err = pthread_mutex_clocklock_impl(mutex, clock, abstime);
  LOCKTRACE("pthread_mutex_clocklock(%t, %d) → %s", mutex, clock,
            DescribeErrno(err));
  return err;
}
