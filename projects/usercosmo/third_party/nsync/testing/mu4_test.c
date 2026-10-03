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
#include "libc/errno.h"
#include "libc/sysv/consts/clock.h"
#include "third_party/nsync/counter.h"
#include "third_party/nsync/timed.h"
#include "third_party/nsync/testing/mu_test.inc"

/* State shared between the test thread and the lock-holding thread. */
struct timedlock_state {
	nsync_mu mu;
	nsync_counter locked;  /* holder decrements to 0 once it holds *mu */
	nsync_counter release; /* test decrements to 0 to free the holder */
	nsync_counter done;    /* holder decrements to 0 after it unlocks */
};

/* Acquire *mu, announce it, and hold it until told to let go.  This stands in
   for a thread that holds the lock "basically forever" from the point of view
   of a contender with a short deadline. */
static void timedlock_holder (struct timedlock_state *s) {
	nsync_mu_lock (&s->mu);
	nsync_counter_add (s->locked, -1); /* announce: *mu is held */
	nsync_counter_wait (s->release, NSYNC_CLOCK, nsync_time_no_deadline);
	nsync_mu_unlock (&s->mu);
	nsync_counter_add (s->done, -1);
}

CLOSURE_DECL_BODY1 (holder, struct timedlock_state *)

/* The simple case: another thread holds *mu indefinitely, so a clocklock()
   with a 500ms deadline must give up and return ETIMEDOUT at roughly the
   deadline, without ever acquiring the lock. */
static void test_mu_timedlock_timeout (testing t) {
	struct timedlock_state s;
	nsync_time start;
	nsync_time end;
	nsync_time deadline;
	int result;

	nsync_mu_init (&s.mu);
	s.locked = nsync_counter_new (1);
	s.release = nsync_counter_new (1);
	s.done = nsync_counter_new (1);

	closure_fork (closure_holder (&timedlock_holder, &s));

	/* Wait until the holder actually owns the lock before we contend. */
	nsync_counter_wait (s.locked, NSYNC_CLOCK, nsync_time_no_deadline);

	/* Attempt to lock with a 500ms deadline; we expect to time out. */
	start = nsync_time_now (NSYNC_CLOCK);
	deadline = nsync_time_add (start, nsync_time_ms (500));
	result = nsync_mu_clocklock (&s.mu, NSYNC_CLOCK, deadline);
	end = nsync_time_now (NSYNC_CLOCK);

	if (result != ETIMEDOUT) {
		TEST_ERROR (t, ("nsync_mu_clocklock() returned %d, expected "
				"ETIMEDOUT (%d)", result, ETIMEDOUT));
	}

	/* It must not return appreciably before the deadline. */
	if (nsync_time_cmp (end, nsync_time_sub (deadline, nsync_time_ms (50))) < 0) {
		TEST_ERROR (t, ("nsync_mu_clocklock() timed out too early"));
	}

	/* Nor wildly after it. */
	if (nsync_time_cmp (nsync_time_add (deadline, nsync_time_ms (500)), end) < 0) {
		TEST_ERROR (t, ("nsync_mu_clocklock() timed out too late"));
	}

	/* Release the holder and wait for it to finish. */
	nsync_counter_add (s.release, -1);
	nsync_counter_wait (s.done, NSYNC_CLOCK, nsync_time_no_deadline);

	nsync_counter_free (s.locked);
	nsync_counter_free (s.release);
	nsync_counter_free (s.done);
}

/* clocklock() must honor its clock argument, not just CLOCK_REALTIME.  Verify
   that a CLOCK_MONOTONIC deadline times out at roughly the right time while
   another thread holds the lock. */
static void test_mu_clocklock_monotonic (testing t) {
	struct timedlock_state s;
	nsync_time start;
	nsync_time end;
	nsync_time deadline;
	int result;

	nsync_mu_init (&s.mu);
	s.locked = nsync_counter_new (1);
	s.release = nsync_counter_new (1);
	s.done = nsync_counter_new (1);

	closure_fork (closure_holder (&timedlock_holder, &s));
	nsync_counter_wait (s.locked, NSYNC_CLOCK, nsync_time_no_deadline);

	start = nsync_time_now (CLOCK_MONOTONIC);
	deadline = nsync_time_add (start, nsync_time_ms (200));
	result = nsync_mu_clocklock (&s.mu, CLOCK_MONOTONIC, deadline);
	end = nsync_time_now (CLOCK_MONOTONIC);

	if (result != ETIMEDOUT) {
		TEST_ERROR (t, ("clocklock(CLOCK_MONOTONIC) returned %d, expected "
				"ETIMEDOUT (%d)", result, ETIMEDOUT));
	}
	if (nsync_time_cmp (end, nsync_time_sub (deadline, nsync_time_ms (10))) < 0) {
		TEST_ERROR (t, ("clocklock(CLOCK_MONOTONIC) timed out too early"));
	}
	if (nsync_time_cmp (nsync_time_add (deadline, nsync_time_ms (300)), end) < 0) {
		TEST_ERROR (t, ("clocklock(CLOCK_MONOTONIC) timed out too late"));
	}

	nsync_counter_add (s.release, -1);
	nsync_counter_wait (s.done, NSYNC_CLOCK, nsync_time_no_deadline);

	nsync_counter_free (s.locked);
	nsync_counter_free (s.release);
	nsync_counter_free (s.done);
}

/* Over many iterations against a held lock, a timed lock must never return
   before its deadline, and only rarely much after it.  The iteration count
   adapts to the requested test length so it stays quick on slow machines. */
static void test_mu_clocklock_deadline_precision (testing t) {
	struct timedlock_state s;
	int i;
	int iters;
	int too_late;
	nsync_time per;
	nsync_time too_early;
	nsync_time too_late_slop;

	nsync_mu_init (&s.mu);
	s.locked = nsync_counter_new (1);
	s.release = nsync_counter_new (1);
	s.done = nsync_counter_new (1);

	closure_fork (closure_holder (&timedlock_holder, &s));
	nsync_counter_wait (s.locked, NSYNC_CLOCK, nsync_time_no_deadline);

	iters = 25;
	if (testing_longshort (t) < 0) {
		iters = 8;
	} else if (testing_longshort (t) > 0) {
		iters = 60;
	}
	per = nsync_time_ms (60);
	too_early = nsync_time_ms (10);
	too_late_slop = nsync_time_ms (100);
	too_late = 0;
	for (i = 0; i != iters; i++) {
		nsync_time start = nsync_time_now (NSYNC_CLOCK);
		nsync_time deadline = nsync_time_add (start, per);
		int result = nsync_mu_clocklock (&s.mu, NSYNC_CLOCK, deadline);
		nsync_time end = nsync_time_now (NSYNC_CLOCK);
		if (result != ETIMEDOUT) {
			TEST_FATAL (t, ("clocklock returned %d, expected ETIMEDOUT "
					"while the lock was held", result));
		}
		/* Returning before the deadline would be a correctness bug
		   regardless of machine load. */
		if (nsync_time_cmp (end, nsync_time_sub (deadline, too_early)) < 0) {
			TEST_ERROR (t, ("clocklock returned before its deadline"));
		}
		/* Returning late is only a soft failure (scheduling jitter). */
		if (nsync_time_cmp (nsync_time_add (deadline, too_late_slop), end) < 0) {
			too_late++;
		}
	}
	if (too_late > iters / 5) {
		TEST_ERROR (t, ("clocklock returned too late %d of %d times",
				too_late, iters));
	}

	nsync_counter_add (s.release, -1);
	nsync_counter_wait (s.done, NSYNC_CLOCK, nsync_time_no_deadline);

	nsync_counter_free (s.locked);
	nsync_counter_free (s.release);
	nsync_counter_free (s.done);
}

int main (int argc, char *argv[]) {
	testing_base tb = testing_new (argc, argv, 0);

	TEST_RUN (tb, test_mu_timedlock_timeout);
	TEST_RUN (tb, test_mu_clocklock_monotonic);
	TEST_RUN (tb, test_mu_clocklock_deadline_precision);

	return (testing_base_exit (tb));
}
