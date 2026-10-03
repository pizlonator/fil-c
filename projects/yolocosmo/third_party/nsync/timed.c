/*-*- mode:c;indent-tabs-mode:t;c-basic-offset:8;tab-width:8;coding:utf-8   -*-│
│ vi: set noet ft=c ts=8 sw=8 fenc=utf-8                                   :vi │
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
#include "libc/assert.h"
#include "libc/calls/blockcancel.internal.h"
#include "libc/errno.h"
#include "libc/intrin/strace.h"
#include "libc/thread/thread.h"
#include "third_party/nsync/atomic.internal.h"
#include "third_party/nsync/common.internal.h"
#include "third_party/nsync/timed.h"
#include "third_party/nsync/races.internal.h"
#include "third_party/nsync/time.h"
#include "third_party/nsync/wait_s.internal.h"

/* Back waiter *w out of *mu's waiter queue after a timeout, when
   conditional waiters are present (MU_CONDITION). Returns non-zero if
   this thread removed *w itself, in which case it now holds *mu in mode
   *l_type and the caller must release it; returns zero if a waker
   dequeued *w first.

   "remove_count" must be the value of w->remove_count observed when *w
   was inserted into the queue.

   When conditions are present we must hold both the spinlock and a
   write lock on *mu, not merely the spinlock:
   nsync_remove_from_mu_queue_() repairs the neighbours' same_condition
   lists, and nsync_mu_unlock_slow_() walks those lists while holding
   the mutex but with the spinlock released during condition evaluation.
   The write lock is what excludes that phase. See the longer rationale
   on the equivalent function in nsync_mu_wait.c.

   NB: this can block past the deadline if *mu is held continuously, but
   it is reached only when nsync_mu_wait() conditions are mixed with
   timed locking on the same mutex; the common, condition-free case
   never comes here.  */
static int mu_remove_self_via_writelock (nsync_mu *mu, lock_type *l_type,
					 waiter *w, uint32_t remove_count) {
	int removed = 0;
	unsigned spin_attempts = 0;
	uint32_t old_word = ATM_LOAD (&mu->word);
	/* Spin until we hold both the spinlock and a write lock on *mu.  */
	while ((old_word & (MU_WZERO_TO_ACQUIRE | MU_SPINLOCK)) != 0 ||
	       !ATM_CAS_ACQ (&mu->word, old_word,
			     (old_word + MU_WADD_TO_ACQUIRE + MU_SPINLOCK) &
			     ~MU_WCLEAR_ON_ACQUIRE)) {
		/* Can't acquire yet; set MU_WRITER_WAITING so readers don't
		   starve us.  It is cleared via MU_WCLEAR_ON_ACQUIRE when the
		   CAS above eventually succeeds.  Failures are ignored.  */
		if ((old_word & (MU_WRITER_WAITING | MU_SPINLOCK)) == 0) {
			ATM_CAS_RELACQ (&mu->word, old_word,
					old_word | MU_WRITER_WAITING);
		}
		spin_attempts = pthread_delay_np (mu, spin_attempts);
		old_word = ATM_LOAD (&mu->word);
	}
	/* Recheck under the lock that a waker didn't dequeue us first;
	   the remove_count test confirms *w is still governed by *mu's
	   spinlock.  */
	if (ATM_LOAD (&w->nw.waiting) != 0 &&
	    remove_count == ATM_LOAD (&w->remove_count)) {
		mu->waiters = nsync_remove_from_mu_queue_ (mu->waiters, &w->nw.q);
		ATM_STORE (&w->nw.waiting, 0);
		/* Release the spinlock, retaining the lock in mode l_type.  */
		ATM_STORE_REL (&mu->word, old_word + l_type->add_to_acquire);
		removed = 1;
	} else {
		/* Already removed by a waker; drop both spinlock and write lock.  */
		ATM_STORE_REL (&mu->word, old_word);
	}
	return (removed);
}

/* Back waiter *w out of *mu's waiter queue after its wait timed out.
   Returns non-zero if this thread removed *w itself (a genuine timeout;
   *mu is NOT held on return). Returns zero if a concurrent waker had
   already removed *w, in which case the wakeup should be honored
   instead (*w->nw.waiting will have been cleared).

   "remove_count" must be the value of w->remove_count observed when *w
   was inserted into the queue.

   In the common case there are no conditional waiters, so MU_CONDITION
   is clear and nsync_mu_unlock_slow_() never releases the spinlock
   during its walk of the waiter list. Holding the spinlock therefore
   gives us complete mutual exclusion over the queue, and we can dequeue
   without acquiring the lock itself -- which is essential, since the
   whole point of a timed lock is to give up on a mutex another thread
   may hold indefinitely. Only when MU_CONDITION is set do we fall back
   to the heavier write-lock path.  */
dontinline static int mu_remove_self_after_timeout (nsync_mu *mu, lock_type *l_type,
						    waiter *w, uint32_t remove_count) {
	int timed_out;
	uint32_t old_word;
	uint32_t clear;

	/* Acquire the spinlock; this never waits on the lock itself to be free.  */
	old_word = nsync_spin_test_and_set_ (&mu->word, MU_SPINLOCK, MU_SPINLOCK, 0, mu);

	if ((old_word & MU_CONDITION) != 0) {
		/* Conditional waiters present: dequeuing safely needs the write
		   lock, so hand off to the heavier path and release the lock it
		   leaves us holding.  */
		nsync_mu_release_spinlock_ (mu);
		if (mu_remove_self_via_writelock (mu, l_type, w, remove_count)) {
			nsync_mu_unlock (mu);
			return 1;
		}
		return 0;
	}

	/* No conditions: it is safe to dequeue under the spinlock alone. */
	clear = MU_SPINLOCK;
	if (ATM_LOAD (&w->nw.waiting) != 0 &&
	    remove_count == ATM_LOAD (&w->remove_count)) {
		mu->waiters = nsync_remove_from_mu_queue_ (mu->waiters, &w->nw.q);
		ATM_STORE (&w->nw.waiting, 0);
		if (dll_is_empty (mu->waiters)) {
			/* Queue emptied; drop the waiter-related bits, matching
			   what nsync_mu_unlock_slow_() does on an empty queue.  */
			clear |= MU_WAITING | MU_WRITER_WAITING | MU_CONDITION | MU_ALL_FALSE;
		}
		timed_out = 1;
	} else {
		/* A waker dequeued us first; honor the wakeup instead.  */
		timed_out = 0;
	}
	/* Release the spinlock (and any waiter bits cleared above).  */
	old_word = ATM_LOAD (&mu->word);
	while (!ATM_CAS_REL (&mu->word, old_word, old_word & ~clear)) {
		old_word = ATM_LOAD (&mu->word);
	}
	return timed_out;
}

/* Like nsync_mu_lock_slow_(), but gives up and returns ETIMEDOUT if *mu
   cannot be acquired by abs_deadline; returns 0 on success.  The caller does
   not hold *mu on an ETIMEDOUT return.  */
dontinline static errno_t nsync_mu_clocklock_slow (nsync_mu *mu, waiter *w, uint32_t clear,
						   lock_type *l_type, int clock,
						   nsync_time abs_deadline) {
	int result = 0;
	uint32_t zero_to_acquire;
	uint32_t wait_count;
	uint32_t long_wait;
	unsigned attempts = 0; /* attempt count; used for spinloop backoff */
	BLOCK_CANCELATION;
	w->cv_mu = NULL;      /* not a cv wait */
	w->cond.f = NULL; /* Not using a conditional critical section. */
	w->cond.v = NULL;
	w->cond.eq = NULL;
	w->wipe_mu = mu;
	w->l_type = l_type;
	zero_to_acquire = l_type->zero_to_acquire;
	if (clear != 0) {
		/* Only the constraints of mutual exclusion should stop a designated waker. */
		zero_to_acquire &= ~(MU_WRITER_WAITING | MU_LONG_WAIT);
	}
	wait_count = 0; /* number of times we waited, and were woken. */
	long_wait = 0; /* set to MU_LONG_WAIT when wait_count gets large */
	for (;;) {
		uint32_t old_word = ATM_LOAD (&mu->word);
		if ((old_word & zero_to_acquire) == 0) {
			/* lock can be acquired; try to acquire, possibly
			   clearing MU_DESIG_WAKER and MU_LONG_WAIT.  */
			if (atomic_compare_exchange_weak_explicit (&mu->word, &old_word,
								   (old_word+l_type->add_to_acquire) &
								   ~(clear|long_wait|l_type->clear_on_acquire),
								   memory_order_acquire, memory_order_relaxed)) {
				break;
			}
		} else if ((old_word&MU_SPINLOCK) == 0 &&
			   atomic_compare_exchange_weak_explicit (&mu->word, &old_word,
								  (old_word|MU_SPINLOCK|long_wait|
								   l_type->set_when_waiting) & ~(clear | MU_ALL_FALSE),
								  memory_order_acquire, memory_order_relaxed)) {
			int sem_outcome;
			uint32_t remove_count;

			/* Spinlock is now held, and lock is held by someone
			   else; MU_WAITING has also been set; queue ourselves.
			   There's no need to adjust same_condition here,
			   because w.condition==NULL.  */
			ATM_STORE (&w->nw.waiting, 1);
			/* Snapshot remove_count so we can later detect whether a
			   waker dequeued us before our timeout cleanup ran.  */
			remove_count = ATM_LOAD (&w->remove_count);
			if (wait_count == 0) {
				/* first wait goes to end of queue */
				dll_make_last (&mu->waiters, &w->nw.q);
			} else {
				/* subsequent waits go to front of queue */
				dll_make_first (&mu->waiters, &w->nw.q);
			}

			/* Release spinlock.  Cannot use a store here, because
			   the current thread does not hold the mutex.  If
			   another thread were a designated waker, the mutex
			   holder could be concurrently unlocking, even though
			   we hold the spinlock.  */
			nsync_mu_release_spinlock_ (mu);

			/* wait until awoken or until abs_deadline passes.  */
			sem_outcome = 0;
			while (ATM_LOAD_ACQ (&w->nw.waiting) != 0) { /* acquire load */
				if (sem_outcome == 0) {
					sem_outcome = nsync_mu_semaphore_p_with_deadline
						(&w->sem, clock, abs_deadline);
					if (sem_outcome != 0 && /* ETIMEDOUT or EINVAL */
					    ATM_LOAD (&w->nw.waiting) != 0) {
						/* Timed out with no wakeup.  Back ourselves
						   out of the waiter queue; on success we did
						   not acquire *mu, so report the timeout.  */
						if (mu_remove_self_after_timeout (
							    mu, l_type, w, remove_count))
							result = sem_outcome;
					}
				}
				if (ATM_LOAD (&w->nw.waiting) != 0) {
					/* Either we were dequeued by a waker but it
					   hasn't yet cleared waiting, or our timeout
					   lost the race; yield until waiting clears.  */
					attempts = pthread_delay_np (mu, attempts);
				}
			}
			if (result == ETIMEDOUT) {
				/* Timed out and already off the queue; *mu is not
				   held, so just report it.  */
				break;
			}
			wait_count++;
			/* If the thread has been woken more than this
			   many times, and still not acquired, it sets
			   the MU_LONG_WAIT bit to prevent thread that
			   have not waited from acquiring. This is the
			   starvation avoidance mechanism. The number is
			   fairly high so that we continue to benefit
			   from the throughput of not having running
			   threads wait unless absolutely necessary.  */
			if (wait_count == LONG_WAIT_THRESHOLD) { /* repeatedly woken */
				long_wait = MU_LONG_WAIT; /* force others to wait at least once */
			}

			attempts = 0;
			clear = MU_DESIG_WAKER;
			/* Threads that have been woken at least once don't care
			   about waiting writers or long waiters.  */
			zero_to_acquire &= ~(MU_WRITER_WAITING | MU_LONG_WAIT);
		}
		attempts = pthread_delay_np (mu, attempts);
	}
	ALLOW_CANCELATION;
	return result;
}

/* Block until *mu is free and then acquire it in writer mode, giving up
   if it cannot be acquired by abs_deadline. Returns 0 on success and
   ETIMEDOUT on timeout; *mu is not held on an ETIMEDOUT return.
   Equivalent to nsync_mu_clocklock with NSYNC_CLOCK.  */
errno_t nsync_mu_timedlock (nsync_mu *mu, nsync_time abs_deadline) {
	return nsync_mu_clocklock (mu, NSYNC_CLOCK, abs_deadline);
}

/* Block until *mu is free and then acquire it in writer mode, giving up
   if it cannot be acquired by abs_deadline. Returns 0 on success and
   ETIMEDOUT on timeout; *mu is not held on an ETIMEDOUT return.  */
errno_t nsync_mu_clocklock (nsync_mu *mu, int clock, nsync_time abs_deadline) {
	int result = 0;
	IGNORE_RACES_START ();
	uint32_t old_word = 0;
	if (!atomic_compare_exchange_strong_explicit (&mu->word, &old_word, MU_WADD_TO_ACQUIRE,
						      memory_order_acquire, memory_order_relaxed)) {
		if ((old_word&MU_WZERO_TO_ACQUIRE) != 0 ||
		    !atomic_compare_exchange_strong_explicit (&mu->word, &old_word,
							      (old_word+MU_WADD_TO_ACQUIRE) & ~MU_WCLEAR_ON_ACQUIRE,
							      memory_order_acquire, memory_order_relaxed)) {
			LOCKTRACE("acquiring nsync_mu_clocklock(%t)...", mu);
			waiter *w = nsync_waiter_new_ ();
			result = nsync_mu_clocklock_slow (mu, w, 0, nsync_writer_type_, clock, abs_deadline);
			nsync_waiter_free_ (w);
		}
	}
	IGNORE_RACES_END ();
	return result;
}
