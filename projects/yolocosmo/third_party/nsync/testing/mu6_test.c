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
#include "libc/errno.h"
#include "libc/mem/mem.h"
#include "third_party/nsync/counter.h"
#include "third_party/nsync/timed.h"
#include "third_party/nsync/testing/mu_test.inc"

/* This test exercises the one path in timed.c that the rest of the suite never
   reaches: the MU_CONDITION write-lock fallback in mu_remove_self_after_timeout().
   That branch runs only when an nsync_mu_clocklock() waiter times out while an
   nsync_mu_wait() conditional waiter is parked on the *same* mutex. */

/* Condition for the parked nsync_mu_wait() waiter: true once *flag is set. */
static int cond_flag_set (const void *v) {
	return *(const int *)v != 0;
}

struct cond_block_state {
	nsync_mu mu;
	int flag;               /* protected by mu; release the waiter when set */
	nsync_counter c_parked; /* waiter -> 0 once it holds mu and is about to wait */
	nsync_counter c_done;   /* waiter -> 0 after it wakes and unlocks */
	nsync_counter held;     /* main   -> 0 once it holds mu */
	nsync_counter t_done;   /* timer  -> 0 after clocklock returns */
	int t_result;           /* clocklock result observed by the timer thread */
	nsync_time t_start;
	nsync_time t_end;
};

/* Park forever on a false condition; this keeps MU_CONDITION set on mu. */
static void cond_block_waiter (struct cond_block_state *s) {
	nsync_mu_lock (&s->mu);
	nsync_counter_add (s->c_parked, -1);
	while (!s->flag) {
		nsync_mu_wait (&s->mu, &cond_flag_set, &s->flag, NULL);
	}
	nsync_mu_unlock (&s->mu);
	nsync_counter_add (s->c_done, -1);
}

CLOSURE_DECL_BODY1 (cond_block_waiter, struct cond_block_state *)

/* Once main holds mu, attempt a timed lock with a 100ms deadline. */
static void cond_block_timer (struct cond_block_state *s) {
	int clock = NSYNC_CLOCK;
	nsync_counter_wait (s->held, clock, nsync_time_no_deadline);
	s->t_start = nsync_time_now (clock);
	s->t_result = nsync_mu_clocklock (&s->mu, clock,
					  nsync_time_add (s->t_start, nsync_time_ms (100)));
	s->t_end = nsync_time_now (clock);
	if (s->t_result == 0) {
		nsync_mu_unlock (&s->mu); /* we won the lock; release it */
	}
	nsync_counter_add (s->t_done, -1);
}

CLOSURE_DECL_BODY1 (cond_block_timer, struct cond_block_state *)

static void test_timedlock_condition_blocks_past_deadline (testing t) {
	struct cond_block_state s;
	nsync_time elapsed;

	nsync_mu_init (&s.mu);
	s.flag = 0;
	s.c_parked = nsync_counter_new (1);
	s.c_done = nsync_counter_new (1);
	s.held = nsync_counter_new (1);
	s.t_done = nsync_counter_new (1);

	/* Park a conditional waiter so MU_CONDITION is set on mu. */
	closure_fork (closure_cond_block_waiter (&cond_block_waiter, &s));
	nsync_counter_wait (s.c_parked, NSYNC_CLOCK, nsync_time_no_deadline);
	nsync_time_sleep (NSYNC_CLOCK, nsync_time_ms (50)); /* let it release mu and park */

	/* Hold the lock so the timed locker is forced to wait, then time out. */
	nsync_mu_lock (&s.mu);
	nsync_counter_add (s.held, -1);

	closure_fork (closure_cond_block_timer (&cond_block_timer, &s));

	/* Hold well past the timer's 100ms deadline, then release. */
	nsync_time_sleep (NSYNC_CLOCK, nsync_time_ms (300));
	nsync_mu_unlock (&s.mu);

	nsync_counter_wait (s.t_done, NSYNC_CLOCK, nsync_time_no_deadline);

	/* The timed locker either removed itself (ETIMEDOUT) or was woken as it
	   gave up (0); both are valid outcomes of the race at the deadline. */
	if (s.t_result != ETIMEDOUT && s.t_result != 0) {
		TEST_ERROR (t, ("clocklock returned %d, want 0 or ETIMEDOUT", s.t_result));
	}

	/* Because MU_CONDITION forced the write-lock cleanup path, the call could
	   not return at its 100ms deadline: that path must hold the lock to
	   dequeue, so it blocked until we released the lock ~300ms in. */
	elapsed = nsync_time_sub (s.t_end, s.t_start);
	if (nsync_time_cmp (elapsed, nsync_time_ms (150)) < 0) {
		char *e = nsync_time_str (elapsed, 2);
		TEST_ERROR (t, ("clocklock returned after %s; expected it to block "
				"past its 100ms deadline until the lock was released", e));
		free (e);
	}

	/* Wake the conditional waiter and confirm it returns cleanly -- this is
	   what proves the cleanup left the waiter queue and same_condition lists
	   intact. */
	nsync_mu_lock (&s.mu);
	s.flag = 1;
	nsync_mu_unlock (&s.mu);
	if (nsync_counter_wait (s.c_done, NSYNC_CLOCK,
				nsync_time_add (nsync_time_now (NSYNC_CLOCK),
						nsync_time_ms (5000))) != 0) {
		TEST_ERROR (t, ("conditional waiter never woke after its condition "
				"became true (waiter queue likely corrupted)"));
	}

	nsync_counter_free (s.c_parked);
	nsync_counter_free (s.c_done);
	nsync_counter_free (s.held);
	nsync_counter_free (s.t_done);
}

/* Concurrency stress for the timed-lock cleanup paths.  Several threads race
   to acquire *mu with short deadlines (so many of them time out and dequeue
   themselves) while a conditional waiter stays parked, keeping MU_CONDITION
   set so the write-lock cleanup path is exercised under real contention.  An
   "in_cs" flag detects any breach of mutual exclusion. */

struct cond_stress_state {
	nsync_mu mu;
	int flag;    /* protected by mu; set at the end to release the waiter */
	int in_cs;   /* protected by mu; mutual-exclusion overlap detector */
	long acquisitions; /* atomic */
	long timeouts;     /* atomic */
	nsync_counter parked;       /* waiter  -> 0 once parked */
	nsync_counter c_done;       /* waiter  -> 0 after it wakes */
	nsync_counter workers_done; /* workers -> 0 as each finishes */
	nsync_time stop_time;
};

static void cond_stress_waiter (struct cond_stress_state *s) {
	nsync_mu_lock (&s->mu);
	nsync_counter_add (s->parked, -1);
	while (!s->flag) {
		nsync_mu_wait (&s->mu, &cond_flag_set, &s->flag, NULL);
	}
	nsync_mu_unlock (&s->mu);
	nsync_counter_add (s->c_done, -1);
}

CLOSURE_DECL_BODY1 (cond_stress_waiter, struct cond_stress_state *)

static void cond_stress_worker (struct cond_stress_state *s) {
	int clock = NSYNC_CLOCK;
	while (nsync_time_cmp (nsync_time_now (clock), s->stop_time) < 0) {
		nsync_time deadline = nsync_time_add (nsync_time_now (clock),
						      nsync_time_ms (1));
		int result = nsync_mu_clocklock (&s->mu, clock, deadline);
		if (result == 0) {
			/* Hold the lock longer than the 1ms deadline so the
			   other workers are forced to time out and run their
			   cleanup against MU_CONDITION. */
			unassert (s->in_cs == 0);
			s->in_cs = 1;
			__atomic_fetch_add (&s->acquisitions, 1, __ATOMIC_RELAXED);
			nsync_time_sleep (clock, nsync_time_ms (2));
			s->in_cs = 0;
			nsync_mu_unlock (&s->mu);
		} else {
			unassert (result == ETIMEDOUT);
			__atomic_fetch_add (&s->timeouts, 1, __ATOMIC_RELAXED);
		}
	}
	nsync_counter_add (s->workers_done, -1);
}

CLOSURE_DECL_BODY1 (cond_stress_worker, struct cond_stress_state *)

static void test_timedlock_condition_stress (testing t) {
	struct cond_stress_state s;
	int i;
	int workers = 4;
	nsync_time dur;

	nsync_mu_init (&s.mu);
	s.flag = 0;
	s.in_cs = 0;
	s.acquisitions = 0;
	s.timeouts = 0;
	s.parked = nsync_counter_new (1);
	s.c_done = nsync_counter_new (1);
	s.workers_done = nsync_counter_new (workers);

	/* Park a conditional waiter to keep MU_CONDITION set throughout. */
	closure_fork (closure_cond_stress_waiter (&cond_stress_waiter, &s));
	nsync_counter_wait (s.parked, NSYNC_CLOCK, nsync_time_no_deadline);
	nsync_time_sleep (NSYNC_CLOCK, nsync_time_ms (50));

	dur = nsync_time_ms (500);
	if (testing_longshort (t) < 0) {
		dur = nsync_time_ms (200);
	} else if (testing_longshort (t) > 0) {
		dur = nsync_time_ms (2000);
	}
	s.stop_time = nsync_time_add (nsync_time_now (NSYNC_CLOCK), dur);
	for (i = 0; i != workers; i++) {
		closure_fork (closure_cond_stress_worker (&cond_stress_worker, &s));
	}
	nsync_counter_wait (s.workers_done, NSYNC_CLOCK, nsync_time_no_deadline);

	/* Release the conditional waiter; it must still wake, proving the
	   contended cleanups never corrupted the waiter queue. */
	nsync_mu_lock (&s.mu);
	s.flag = 1;
	nsync_mu_unlock (&s.mu);
	if (nsync_counter_wait (s.c_done, NSYNC_CLOCK,
				nsync_time_add (nsync_time_now (NSYNC_CLOCK),
						nsync_time_ms (5000))) != 0) {
		TEST_ERROR (t, ("conditional waiter never woke after stress "
				"(waiter queue likely corrupted)"));
	}

	/* Sanity: the workers really did contend (acquired and timed out). */
	if (s.acquisitions == 0) {
		TEST_ERROR (t, ("no timed lock was ever acquired"));
	}
	if (s.timeouts == 0) {
		TEST_ERROR (t, ("no timed lock ever timed out; the cleanup path "
				"under MU_CONDITION was not exercised"));
	}

	nsync_counter_free (s.parked);
	nsync_counter_free (s.c_done);
	nsync_counter_free (s.workers_done);
}

int main (int argc, char *argv[]) {
	testing_base tb = testing_new (argc, argv, 0);

	TEST_RUN (tb, test_timedlock_condition_blocks_past_deadline);
	TEST_RUN (tb, test_timedlock_condition_stress);

	return (testing_base_exit (tb));
}
