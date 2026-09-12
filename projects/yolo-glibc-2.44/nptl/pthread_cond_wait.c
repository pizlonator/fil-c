/* Copyright (C) 2003-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <endian.h>
#include <errno.h>
#include <sysdep.h>
#include <futex-internal.h>
#include <pthread.h>
#include <pthreadP.h>
#include <sys/time.h>
#include <atomic.h>
#include <stdint.h>
#include <stdbool.h>

#include <shlib-compat.h>
#include <stap-probe.h>
#include <time.h>

#include "pthread_cond_common.c"


struct _condvar_cleanup_buffer
{
  uint64_t wseq;
  pthread_cond_t *cond;
  pthread_mutex_t *mutex;
  int private;
};


/* Decrease the waiter reference count.  */
static void
__condvar_confirm_wakeup (pthread_cond_t *cond, int private)
{
  /* If destruction is pending (i.e., the wake-request flag is nonzero) and we
     are the last waiter (prior value of __wrefs was 1 << 3), then wake any
     threads waiting in pthread_cond_destroy.  Release MO to synchronize with
     these threads.  Don't bother clearing the wake-up request flag.  */
  if ((atomic_fetch_add_release (&cond->__data.__wrefs, -8) >> 2) == 3)
    futex_wake (&cond->__data.__wrefs, INT_MAX, private);
}


/* Cancel waiting after having registered as a waiter previously.  SEQ is our
   position and G is our group index.
   The goal of cancellation is to make our group smaller if that is still
   possible.  If we are in a closed group, this is not possible anymore; in
   this case, we need to send a replacement signal for the one we effectively
   consumed because the signal should have gotten consumed by another waiter
   instead; we must not both cancel waiting and consume a signal.

   Must not be called while still holding a reference on the group.

   Returns true iff we consumed a signal.

   On some kind of timeouts, we may be able to pretend that a signal we
   effectively consumed happened before the timeout (i.e., similarly to first
   spinning on signals before actually checking whether the timeout has
   passed already).  Doing this would allow us to skip sending a replacement
   signal, but this case might happen rarely because the end of the timeout
   must race with someone else sending a signal.  Therefore, we don't bother
   trying to optimize this.  */
static void
__condvar_cancel_waiting (pthread_cond_t *cond, uint64_t seq, unsigned int g,
			  int private)
{
  bool consumed_signal = false;

  /* No deadlock with group switching is possible here because we do
     not hold a reference on the group.  */
  __condvar_acquire_lock (cond, private);

  uint64_t g1_start = __condvar_load_g1_start_relaxed (cond);
  if (g1_start > seq)
    {
      /* Our group is closed, so someone provided enough signals for it.
	 Thus, we effectively consumed a signal.  */
      consumed_signal = true;
    }
  else
    {
      if (g1_start + __condvar_get_orig_size (cond) <= seq)
	{
	  /* We are in the current G2 and thus cannot have consumed a signal.
	     Reduce its effective size or handle overflow.  Remember that in
	     G2, unsigned int size is zero or a negative value.  */
	  if (cond->__data.__g_size[g] + __PTHREAD_COND_MAX_GROUP_SIZE > 0)
	    {
	      cond->__data.__g_size[g]--;
	    }
	  else
	    {
	      /* Cancellations would overflow the maximum group size.  Just
		 wake up everyone spuriously to create a clean state.  This
		 also means we do not consume a signal someone else sent.  */
	      __condvar_release_lock (cond, private);
	      __pthread_cond_broadcast (cond);
	      return;
	    }
	}
      else
	{
	  /* We are in current G1.  If the group's size is zero, someone put
	     a signal in the group that nobody else but us can consume.  */
	  if (cond->__data.__g_size[g] == 0)
	    consumed_signal = true;
	  else
	    {
	      /* Otherwise, we decrease the size of the group.  This is
		 equivalent to atomically putting in a signal just for us and
		 consuming it right away.  We do not consume a signal sent
		 by someone else.  We also cannot have consumed a futex
		 wake-up because if we were cancelled or timed out in a futex
		 call, the futex will wake another waiter.  */
	      cond->__data.__g_size[g]--;
	    }
	}
    }

  __condvar_release_lock (cond, private);

  if (consumed_signal)
    {
      /* We effectively consumed a signal even though we didn't want to.
	 Therefore, we need to send a replacement signal.
	 If we would want to optimize this, we could do what
	 pthread_cond_signal does right in the critical section above.  */
      __pthread_cond_signal (cond);
    }
}

/* Clean-up for cancellation of waiters waiting for normal signals.  We cancel
   our registration as a waiter, confirm we have woken up, and re-acquire the
   mutex.  */
static void
__condvar_cleanup_waiting (void *arg)
{
  struct _condvar_cleanup_buffer *cbuffer =
    (struct _condvar_cleanup_buffer *) arg;
  pthread_cond_t *cond = cbuffer->cond;
  unsigned g = cbuffer->wseq & 1;

  __condvar_cancel_waiting (cond, cbuffer->wseq >> 1, g, cbuffer->private);
  /* FIXME With the current cancellation implementation, it is possible that
     a thread is cancelled after it has returned from a syscall.  This could
     result in a cancelled waiter consuming a futex wake-up that is then
     causing another waiter in the same group to not wake up.  To work around
     this issue until we have fixed cancellation, just add a futex wake-up
     conservatively.  */
  futex_wake (cond->__data.__g_signals + g, 1, cbuffer->private);

  __condvar_confirm_wakeup (cond, cbuffer->private);

  /* XXX If locking the mutex fails, should we just stop execution?  This
     might be better than silently ignoring the error.  */
  __pthread_mutex_cond_lock (cbuffer->mutex);
}

/* This condvar implementation guarantees that all calls to signal and
   broadcast and all of the three virtually atomic parts of each call to wait
   (i.e., (1) releasing the mutex and blocking, (2) unblocking, and (3) re-
   acquiring the mutex) happen in some total order that is consistent with the
   happens-before relations in the calling program.  However, this order does
   not necessarily result in additional happens-before relations being
   established (which aligns well with spurious wake-ups being allowed).

   All waiters acquire a certain position in a 64b waiter sequence (__wseq).
   This sequence determines which waiters are allowed to consume signals.
   A broadcast is equal to sending as many signals as are unblocked waiters.
   When a signal arrives, it samples the current value of __wseq with a
   relaxed-MO load (i.e., the position the next waiter would get).  (This is
   sufficient because it is consistent with happens-before; the caller can
   enforce stronger ordering constraints by calling signal while holding the
   mutex.)  Only waiters with a position less than the __wseq value observed
   by the signal are eligible to consume this signal.

   This would be straight-forward to implement if waiters would just spin but
   we need to let them block using futexes.  Futexes give no guarantee of
   waking in FIFO order, so we cannot reliably wake eligible waiters if we
   just use a single futex.  Also, futex words are 32b in size, but we need
   to distinguish more than 1<<32 states because we need to represent the
   order of wake-up (and thus which waiters are eligible to consume signals);
   blocking in a futex is not atomic with a waiter determining its position in
   the waiter sequence, so we need the futex word to reliably notify waiters
   that they should not attempt to block anymore because they have been
   already signaled in the meantime.  While an ABA issue on a 32b value will
   be rare, ignoring it when we are aware of it is not the right thing to do
   either.

   Therefore, we use a 64b counter to represent the waiter sequence (on
   architectures which only support 32b atomics, we use a few bits less).
   To deal with the blocking using futexes, we maintain two groups of waiters:
   * Group G1 consists of waiters that are all eligible to consume signals;
     incoming signals will always signal waiters in this group until all
     waiters in G1 have been signaled.
   * Group G2 consists of waiters that arrive when a G1 is present and still
     contains waiters that have not been signaled.  When all waiters in G1
     are signaled and a new signal arrives, the new signal will convert G2
     into the new G1 and create a new G2 for future waiters.

   We cannot allocate new memory because of process-shared condvars, so we
   have just two slots of groups that change their role between G1 and G2.
   Each has a separate futex word, a number of signals available for
   consumption, a size (number of waiters in the group that have not been
   signaled), and a reference count.

   The group reference count is used to maintain the number of waiters that
   are using the group's futex.

   To represent which intervals in the waiter sequence the groups cover (and
   thus also which group slot contains G1 or G2), we use a 64b counter to
   designate the start position of G1 (inclusive), and a single bit in the
   waiter sequence counter to represent which group slot currently contains
   G2.  This allows us to switch group roles atomically wrt. waiters obtaining
   a position in the waiter sequence.  The G1 start position allows waiters to
   figure out whether they are in a group that has already been completely
   signaled (i.e., if the current G1 starts at a later position that the
   waiter's position).  Waiters cannot determine whether they are currently
   in G2 or G1 -- but they do not have to because all they are interested in
   is whether there are available signals, and they always start in G2 (whose
   group slot they know because of the bit in the waiter sequence.  Signalers
   will simply fill the right group until it is completely signaled and can
   be closed (they do not switch group roles until they really have to to
   decrease the likelihood of having to wait for waiters still holding a
   reference on the now-closed G1).

   Signalers maintain the initial size of G1 to be able to determine where
   G2 starts (G2 is always open-ended until it becomes G1).  They track the
   remaining size of a group; when waiters cancel waiting (due to PThreads
   cancellation or timeouts), they will decrease this remaining size as well.

   To implement condvar destruction requirements (i.e., that
   pthread_cond_destroy can be called as soon as all waiters have been
   signaled), waiters increment a reference count before starting to wait and
   decrement it after they stopped waiting but right before they acquire the
   mutex associated with the condvar.

   pthread_cond_t thus consists of the following (bits that are used for
   flags and are not part of the primary value of each field but necessary
   to make some things atomic or because there was no space for them
   elsewhere in the data structure):

   __wseq: Waiter sequence counter
     * LSB is index of current G2.
     * Waiters fetch-add while having acquire the mutex associated with the
       condvar.  Signalers load it and fetch-xor it concurrently.
   __g1_start: Starting position of G1 (inclusive)
     * Modified by signalers while having acquired the condvar-internal lock
       and observed concurrently by waiters.
   __g1_orig_size: Initial size of G1
     * The two least-significant bits represent the condvar-internal lock.
     * Only accessed while having acquired the condvar-internal lock.
   __wrefs: Waiter reference counter.
     * Bit 2 is true if waiters should run futex_wake when they remove the
       last reference.  pthread_cond_destroy uses this as futex word.
     * Bit 1 is the clock ID (0 == CLOCK_REALTIME, 1 == CLOCK_MONOTONIC).
     * Bit 0 is true iff this is a process-shared condvar.
     * Simple reference count used by both waiters and pthread_cond_destroy.
     (If the format of __wrefs is changed, update nptl_lock_constants.pysym
      and the pretty printers.)
   For each of the two groups, we have:
   __g_signals: The number of signals that can still be consumed, relative to
     the current g1_start.  (i.e. g1_start with the signal count added)
     * Used as a futex word by waiters.  Used concurrently by waiters and
       signalers.
   __g_size: Waiters remaining in this group (i.e., which have not been
     signaled yet.
     * Accessed by signalers and waiters that cancel waiting (both do so only
       when having acquired the condvar-internal lock.
     * The size of G2 is always zero because it cannot be determined until
       the group becomes G1.
     * Although this is of unsigned type, we rely on using unsigned overflow
       rules to make this hold effectively negative values too (in
       particular, when waiters in G2 cancel waiting).

   A PTHREAD_COND_INITIALIZER condvar has all fields set to zero, which yields
   a condvar that has G2 starting at position 0 and a G1 that is closed.

   Because waiters do not claim ownership of a group right when obtaining a
   position in __wseq but only reference count the group when using futexes
   to block, it can happen that a group gets closed before a waiter can
   increment the reference count.  Therefore, waiters have to check whether
   their group is already closed using __g1_start.  They also have to perform
   this check when spinning when trying to grab a signal from __g_signals.
   Note that for these checks, using relaxed MO to load __g1_start is
   sufficient because if a waiter can see a sufficiently large value, it could
   have also consume a signal in the waiters group.


   Limitations:
   * This condvar isn't designed to allow for more than
     __PTHREAD_COND_MAX_GROUP_SIZE * (1 << 31) calls to __pthread_cond_wait.
   * More than __PTHREAD_COND_MAX_GROUP_SIZE concurrent waiters are not
     supported.
   * Beyond what is allowed as errors by POSIX or documented, we can also
     return the following errors:
     * EPERM if MUTEX is a recursive mutex and the caller doesn't own it.
     * EOWNERDEAD or ENOTRECOVERABLE when using robust mutexes.  Unlike
       for other errors, this can happen when we re-acquire the mutex; this
       isn't allowed by POSIX (which requires all errors to virtually happen
       before we release the mutex or change the condvar state), but there's
       nothing we can do really.
     * When using PTHREAD_MUTEX_PP_* mutexes, we can also return all errors
       returned by __pthread_tpp_change_priority.  We will already have
       released the mutex in such cases, so the caller cannot expect to own
       MUTEX.

   Other notes:
   * Instead of the normal mutex unlock / lock functions, we use
     __pthread_mutex_unlock_usercnt(m, 0) / __pthread_mutex_cond_lock(m)
     because those will not change the mutex-internal users count, so that it
     can be detected when a condvar is still associated with a particular
     mutex because there is a waiter blocked on this condvar using this mutex.
*/
static __always_inline int
__pthread_cond_wait_common (pthread_cond_t *cond, pthread_mutex_t *mutex,
    clockid_t clockid, const struct __timespec64 *abstime)
{
  int err;
  int result = 0;

  LIBC_PROBE (cond_wait, 2, cond, mutex);

  /* clockid will already have been checked by
     __pthread_cond_clockwait or pthread_condattr_setclock, or we
     don't use it if abstime is NULL, so we don't need to check it
     here. */

  /* Acquire a position (SEQ) in the waiter sequence (WSEQ).  We use an
     atomic operation because signals and broadcasts may update the group
     switch without acquiring the mutex.  We do not need release MO here
     because we do not need to establish any happens-before relation with
     signalers (see __pthread_cond_signal); modification order alone
     establishes a total order of waiters/signals.  We do need acquire MO
     to synchronize with group reinitialization in __condvar_switch_g1.  */
  uint64_t wseq = __condvar_fetch_add_wseq_acquire (cond, 2);
  /* Find our group's index.  We always go into what was G2 when we acquired
     our position.  */
  unsigned int g = wseq & 1;
  uint64_t seq = wseq >> 1;

  /* Increase the waiter reference count.  Relaxed MO is sufficient because
     we only need to synchronize when decrementing the reference count.  */
  unsigned int flags = atomic_fetch_add_relaxed (&cond->__data.__wrefs, 8);
  int private = __condvar_get_private (flags);

  /* Now that we are registered as a waiter, we can release the mutex.
     Waiting on the condvar must be atomic with releasing the mutex, so if
     the mutex is used to establish a happens-before relation with any
     signaler, the waiter must be visible to the latter; thus, we release the
     mutex after registering as waiter.
     If releasing the mutex fails, we just cancel our registration as a
     waiter and confirm that we have woken up.  */
  err = __pthread_mutex_unlock_usercnt (mutex, 0);
  if (__glibc_unlikely (err != 0))
    {
      __condvar_cancel_waiting (cond, seq, g, private);
      __condvar_confirm_wakeup (cond, private);
      return err;
    }


  while (1)
    {
      /* Now wait until a signal is available in our group or it is closed.
         Acquire MO so that if we observe (signals == lowseq) after group
         switching in __condvar_switch_g1, we synchronize with that store and
         will see the prior update of __g1_start done while switching groups
         too.  */
      unsigned int signals = atomic_load_acquire (cond->__data.__g_signals + g);
      uint64_t g1_start = __condvar_load_g1_start_relaxed (cond);

      if (seq < g1_start)
        {
          /* If the group is closed already,
             then this waiter originally had enough extra signals to
             consume, up until the time its group was closed.  */
           break;
        }

      /* If there is an available signal, don't block.
         If __g1_start has advanced at all, then we must be in G1
         by now, perhaps in the process of switching back to an older
         G2, but in either case we're allowed to consume the available
         signal and should not block anymore.  */
      if ((int)(signals - (unsigned int)g1_start) > 0)
        {
	  /* Try to grab a signal.  See above for MO.  (if we do another loop
	     iteration we need to see the correct value of g1_start)  */
	    if (atomic_compare_exchange_weak_acquire (
			cond->__data.__g_signals + g,
			&signals, signals - 1))
	      break;
	    else
	      continue;
	}

      // Now block.
      struct _pthread_cleanup_buffer buffer;
      struct _condvar_cleanup_buffer cbuffer;
      cbuffer.wseq = wseq;
      cbuffer.cond = cond;
      cbuffer.mutex = mutex;
      cbuffer.private = private;
      __pthread_cleanup_push (&buffer, __condvar_cleanup_waiting, &cbuffer);

      err = __futex_abstimed_wait_cancelable64 (
        cond->__data.__g_signals + g, signals, clockid, abstime, private);

      __pthread_cleanup_pop (&buffer, 0);

      if (__glibc_unlikely (err == ETIMEDOUT || err == EOVERFLOW))
        {
          /* If we timed out, we effectively cancel waiting.  */
          __condvar_cancel_waiting (cond, seq, g, private);
          result = err;
          break;
        }
    }

  /* Confirm that we have been woken.  We do that before acquiring the mutex
     to allow for execution of pthread_cond_destroy while having acquired the
     mutex.  */
  __condvar_confirm_wakeup (cond, private);

  /* Woken up; now re-acquire the mutex.  If this doesn't fail, return RESULT,
     which is set to ETIMEDOUT if a timeout occurred, or zero otherwise.  */
  err = __pthread_mutex_cond_lock (mutex);
  /* XXX Abort on errors that are disallowed by POSIX?  */
  return (err != 0) ? err : result;
}


/* See __pthread_cond_wait_common.  */
int
___pthread_cond_wait (pthread_cond_t *cond, pthread_mutex_t *mutex)
{
  /* clockid is unused when abstime is NULL. */
  return __pthread_cond_wait_common (cond, mutex, 0, NULL);
}

versioned_symbol (libc, ___pthread_cond_wait, pthread_cond_wait,
		  GLIBC_2_3_2);
libc_hidden_ver (___pthread_cond_wait, __pthread_cond_wait)
#ifndef SHARED
strong_alias (___pthread_cond_wait, __pthread_cond_wait)
#endif

/* See __pthread_cond_wait_common.  */
int
___pthread_cond_timedwait64 (pthread_cond_t *cond, pthread_mutex_t *mutex,
			     const struct __timespec64 *abstime)
{
  /* Check parameter validity.  This should also tell the compiler that
     it can assume that abstime is not NULL.  */
  if (! valid_nanoseconds (abstime->tv_nsec))
    return EINVAL;

  /* Relaxed MO is suffice because clock ID bit is only modified
     in condition creation.  */
  unsigned int flags = atomic_load_relaxed (&cond->__data.__wrefs);
  clockid_t clockid = (flags & __PTHREAD_COND_CLOCK_MONOTONIC_MASK)
                    ? CLOCK_MONOTONIC : CLOCK_REALTIME;
  return __pthread_cond_wait_common (cond, mutex, clockid, abstime);
}

#if __TIMESIZE == 64
strong_alias (___pthread_cond_timedwait64, ___pthread_cond_timedwait)
#else
strong_alias (___pthread_cond_timedwait64, __pthread_cond_timedwait64)
libc_hidden_def (__pthread_cond_timedwait64)

int
___pthread_cond_timedwait (pthread_cond_t *cond, pthread_mutex_t *mutex,
			    const struct timespec *abstime)
{
  struct __timespec64 ts64 = valid_timespec_to_timespec64 (*abstime);

  return __pthread_cond_timedwait64 (cond, mutex, &ts64);
}
#endif /* __TIMESIZE == 64 */
versioned_symbol (libc, ___pthread_cond_timedwait,
		  pthread_cond_timedwait, GLIBC_2_3_2);
libc_hidden_ver (___pthread_cond_timedwait, __pthread_cond_timedwait)
#ifndef SHARED
strong_alias (___pthread_cond_timedwait, __pthread_cond_timedwait)
#endif

/* See __pthread_cond_wait_common.  */
int
___pthread_cond_clockwait64 (pthread_cond_t *cond, pthread_mutex_t *mutex,
			      clockid_t clockid,
			      const struct __timespec64 *abstime)
{
  /* Check parameter validity.  This should also tell the compiler that
     it can assume that abstime is not NULL.  */
  if (! valid_nanoseconds (abstime->tv_nsec))
    return EINVAL;

  if (!futex_abstimed_supported_clockid (clockid))
    return EINVAL;

  return __pthread_cond_wait_common (cond, mutex, clockid, abstime);
}

#if __TIMESIZE == 64
strong_alias (___pthread_cond_clockwait64, ___pthread_cond_clockwait)
#else
strong_alias (___pthread_cond_clockwait64, __pthread_cond_clockwait64);
libc_hidden_def (__pthread_cond_clockwait64)

int
___pthread_cond_clockwait (pthread_cond_t *cond, pthread_mutex_t *mutex,
                          clockid_t clockid,
                          const struct timespec *abstime)
{
  struct __timespec64 ts64 = valid_timespec_to_timespec64 (*abstime);

  return __pthread_cond_clockwait64 (cond, mutex, clockid, &ts64);
}
#endif /* __TIMESIZE == 64 */
libc_hidden_ver (___pthread_cond_clockwait, __pthread_cond_clockwait)
#ifndef SHARED
strong_alias (___pthread_cond_clockwait, __pthread_cond_clockwait)
#endif
versioned_symbol (libc, ___pthread_cond_clockwait,
		  pthread_cond_clockwait, GLIBC_2_34);
#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_30, GLIBC_2_34)
compat_symbol (libpthread, ___pthread_cond_clockwait,
	       pthread_cond_clockwait, GLIBC_2_30);
#endif
