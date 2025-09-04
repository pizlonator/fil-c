/* vi: set sw=4 ts=4: */
/*
 * Busybox utility routines.
 *
 * Copyright (C) 2005 by Tito Ragusa <tito-wolit@tiscali.it>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
#include "libbb.h"

/* void FAST_FUNC bb_do_delay(unsigned seconds) { ... } - no users yet */

#ifndef LOGIN_FAIL_DELAY
#define LOGIN_FAIL_DELAY 3
#endif
void FAST_FUNC pause_after_failed_login(void)
{
#if 0 /* over-engineered madness */
	time_t end, diff;

	end = time(NULL) + LOGIN_FAIL_DELAY;
	diff = LOGIN_FAIL_DELAY;
	do {
		sleep(diff);
		diff = end - time(NULL);
	} while (diff > 0);
#else
	sleep(LOGIN_FAIL_DELAY);
#endif
}

void FAST_FUNC sleep1(void)
{
	sleep(1);
}

void FAST_FUNC msleep(unsigned ms)
{
#if 0
	/* 1. usleep(n) is not guaranteed by standards to accept n >= 1000000
	 * 2. multiplication in usleep(ms * 1000) can overflow if ms > 4294967
	 *    (sleep of ~71.5 minutes)
	 * Let's play safe and loop:
	 */
	while (ms > 500) {
		usleep(500000);
		ms -= 500;
	}
	usleep(ms * 1000);
#else
//usleep is often implemented as a call to nanosleep.
//Simply do the same to implement msleep.
//it's marginally larger, but wakes your CPU less often:
//function    old     new   delta
//msleep       45      52      +7
	struct timespec ts;
	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000;
	/*
	 * If a signal has non-default handler, nanosleep returns early.
	 * Our version of msleep doesn't return early
	 * if interrupted by such signals:
	 */
	while (nanosleep(&ts, &ts) != 0)
		continue;
#endif
}
