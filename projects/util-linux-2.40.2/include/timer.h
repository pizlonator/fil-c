/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#ifndef UTIL_LINUX_TIMER_H
#define UTIL_LINUX_TIMER_H

#include <signal.h>
#include <sys/time.h>

#ifdef HAVE_TIMER_CREATE
struct ul_timer {
	timer_t t_id;
};
#else
struct ul_timer {
	struct itimerval old_timer;
	struct sigaction old_sa;
};
#endif

extern int setup_timer(struct ul_timer *timer, struct itimerval *timeout,
		       void (*timeout_handler)(int, siginfo_t *, void *));
extern void cancel_timer(struct ul_timer *timer);

#endif /* UTIL_LINUX_TIMER_H */
