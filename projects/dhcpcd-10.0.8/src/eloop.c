/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * eloop - portable event based main loop.
 * Copyright (c) 2006-2023 Roy Marples <roy@marples.name>
 * All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* NOTES:
 * Basically for a small number of fd's (total, not max fd)
 * of say a few hundred, ppoll(2) performs just fine, if not faster than others.
 * It also has the smallest memory and binary size footprint.
 * ppoll(2) is available on all modern OS my software runs on and should be
 * an up and coming POSIX standard interface.
 * If ppoll is not available, then pselect(2) can be used instead which has
 * even smaller memory and binary size footprint.
 * However, this difference is quite tiny and the ppoll API is superior.
 * pselect cannot return error conditions such as EOF for example.
 *
 * Both epoll(7) and kqueue(2) require an extra fd per process to manage
 * their respective list of interest AND syscalls to manage it.
 * So for a small number of fd's, these are more resource intensive,
 * especially when used with more than one process.
 *
 * epoll avoids the resource limit RLIMIT_NOFILE Linux poll stupidly applies.
 * kqueue avoids the same limit on OpenBSD.
 * ppoll can still be secured in both by using SEECOMP or pledge.
 *
 * kqueue can avoid the signal trick we use here so that we function calls
 * other than those listed in sigaction(2) in our signal handlers which is
 * probably more robust than ours at surviving a signal storm.
 * signalfd(2) is available for Linux which probably works in a similar way
 * but it's yet another fd to use.
 *
 * Taking this all into account, ppoll(2) is the default mechanism used here.
 */

#if (defined(__unix__) || defined(unix)) && !defined(USG)
#include <sys/param.h>
#endif
#include <sys/time.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* config.h should define HAVE_PPOLL, etc. */
#if defined(HAVE_CONFIG_H) && !defined(NO_CONFIG_H)
#include "config.h"
#endif

/* Prioritise which mechanism we want to use.*/
#if defined(HAVE_PPOLL)
#undef HAVE_EPOLL
#undef HAVE_KQUEUE
#undef HAVE_PSELECT
#elif defined(HAVE_POLLTS)
#define HAVE_PPOLL
#define ppoll pollts
#undef HAVE_EPOLL
#undef HAVE_KQUEUE
#undef HAVE_PSELECT
#elif defined(HAVE_KQUEUE)
#undef HAVE_EPOLL
#undef HAVE_PSELECT
#elif defined(HAVE_EPOLL)
#undef HAVE_KQUEUE
#undef HAVE_PSELECT
#elif !defined(HAVE_PSELECT)
#define HAVE_PPOLL
#endif

#if defined(HAVE_KQUEUE)
#include <sys/event.h>
#if defined(__DragonFly__) || defined(__FreeBSD__)
#define	_kevent(kq, cl, ncl, el, nel, t) \
	kevent((kq), (cl), (int)(ncl), (el), (int)(nel), (t))
#else
#define	_kevent kevent
#endif
#define NFD 2
#elif defined(HAVE_EPOLL)
#include <sys/epoll.h>
#define	NFD 1
#elif defined(HAVE_PPOLL)
#include <poll.h>
#define NFD 1
#elif defined(HAVE_PSELECT)
#include <sys/select.h>
#endif

#include "eloop.h"

#ifndef UNUSED
#define UNUSED(a) (void)((a))
#endif
#ifndef __unused
#ifdef __GNUC__
#define __unused   __attribute__((__unused__))
#else
#define __unused
#endif
#endif

/* Our structures require TAILQ macros, which really every libc should
 * ship as they are useful beyond belief.
 * Sadly some libc's don't have sys/queue.h and some that do don't have
 * the TAILQ_FOREACH macro. For those that don't, the application using
 * this implementation will need to ship a working queue.h somewhere.
 * If we don't have sys/queue.h found in config.h, then
 * allow QUEUE_H to override loading queue.h in the current directory. */
#ifndef TAILQ_FOREACH
#ifdef HAVE_SYS_QUEUE_H
#include <sys/queue.h>
#elif defined(QUEUE_H)
#define __QUEUE_HEADER(x) #x
#define _QUEUE_HEADER(x) __QUEUE_HEADER(x)
#include _QUEUE_HEADER(QUEUE_H)
#else
#include "queue.h"
#endif
#endif

#ifdef ELOOP_DEBUG
#include <stdio.h>
#endif

#ifndef __arraycount
#  define __arraycount(__x)       (sizeof(__x) / sizeof(__x[0]))
#endif

/*
 * Allow a backlog of signals.
 * If you use many eloops in the same process, they should all
 * use the same signal handler or have the signal handler unset.
 * Otherwise the signal might not behave as expected.
 */
#define ELOOP_NSIGNALS	5

/*
 * time_t is a signed integer of an unspecified size.
 * To adjust for time_t wrapping, we need to work the maximum signed
 * value and use that as a maximum.
 */
#ifndef TIME_MAX
#define	TIME_MAX	((1ULL << (sizeof(time_t) * NBBY - 1)) - 1)
#endif
/* The unsigned maximum is then simple - multiply by two and add one. */
#ifndef UTIME_MAX
#define	UTIME_MAX	(TIME_MAX * 2) + 1
#endif

struct eloop_event {
	TAILQ_ENTRY(eloop_event) next;
	int fd;
	void (*cb)(void *, unsigned short);
	void *cb_arg;
	unsigned short events;
#ifdef HAVE_PPOLL
	struct pollfd *pollfd;
#endif
};

struct eloop_timeout {
	TAILQ_ENTRY(eloop_timeout) next;
	unsigned int seconds;
	unsigned int nseconds;
	void (*callback)(void *);
	void *arg;
	int queue;
};

struct eloop {
	TAILQ_HEAD (event_head, eloop_event) events;
	size_t nevents;
	struct event_head free_events;

	struct timespec now;
	TAILQ_HEAD (timeout_head, eloop_timeout) timeouts;
	struct timeout_head free_timeouts;

	const int *signals;
	size_t nsignals;
	void (*signal_cb)(int, void *);
	void *signal_cb_ctx;

#if defined(HAVE_KQUEUE) || defined(HAVE_EPOLL)
	int fd;
#endif
#if defined(HAVE_KQUEUE)
	struct kevent *fds;
#elif defined(HAVE_EPOLL)
	struct epoll_event *fds;
#elif defined(HAVE_PPOLL)
	struct pollfd *fds;
#endif
#if !defined(HAVE_PSELECT)
	size_t nfds;
#endif

	int exitcode;
	bool exitnow;
	bool events_need_setup;
	bool cleared;
};

#ifdef HAVE_REALLOCARRAY
#define	eloop_realloca	reallocarray
#else
/* Handy routing to check for potential overflow.
 * reallocarray(3) and reallocarr(3) are not portable. */
#define SQRT_SIZE_MAX (((size_t)1) << (sizeof(size_t) * CHAR_BIT / 2))
static void *
eloop_realloca(void *ptr, size_t n, size_t size)
{

	if ((n | size) >= SQRT_SIZE_MAX && n > SIZE_MAX / size) {
		errno = EOVERFLOW;
		return NULL;
	}
	return realloc(ptr, n * size);
}
#endif


static int
eloop_event_setup_fds(struct eloop *eloop)
{
	struct eloop_event *e, *ne;
#if defined(HAVE_KQUEUE)
	struct kevent *pfd;
	size_t nfds = eloop->nsignals;
#elif defined(HAVE_EPOLL)
	struct epoll_event *pfd;
	size_t nfds = 0;
#elif defined(HAVE_PPOLL)
	struct pollfd *pfd;
	size_t nfds = 0;
#endif

#ifndef HAVE_PSELECT
	nfds += eloop->nevents * NFD;
	if (eloop->nfds < nfds) {
		pfd = eloop_realloca(eloop->fds, nfds, sizeof(*pfd));
		if (pfd == NULL)
			return -1;
		eloop->fds = pfd;
		eloop->nfds = nfds;
	}
#endif

#ifdef HAVE_PPOLL
	pfd = eloop->fds;
#endif
	TAILQ_FOREACH_SAFE(e, &eloop->events, next, ne) {
		if (e->fd == -1) {
			TAILQ_REMOVE(&eloop->events, e, next);
			TAILQ_INSERT_TAIL(&eloop->free_events, e, next);
			continue;
		}
#ifdef HAVE_PPOLL
		e->pollfd = pfd;
		pfd->fd = e->fd;
		pfd->events = 0;
		if (e->events & ELE_READ)
			pfd->events |= POLLIN;
		if (e->events & ELE_WRITE)
			pfd->events |= POLLOUT;
		pfd->revents = 0;
		pfd++;
#endif
	}

	eloop->events_need_setup = false;
	return 0;
}

size_t
eloop_event_count(const struct eloop *eloop)
{

	return eloop->nevents;
}

int
eloop_event_add(struct eloop *eloop, int fd, unsigned short events,
    void (*cb)(void *, unsigned short), void *cb_arg)
{
	struct eloop_event *e;
	bool added;
#if defined(HAVE_KQUEUE)
	struct kevent ke[2], *kep = &ke[0];
	size_t n;
#elif defined(HAVE_EPOLL)
	struct epoll_event epe;
	int op;
#endif

	assert(eloop != NULL);
	assert(cb != NULL && cb_arg != NULL);
	if (fd == -1 || !(events & (ELE_READ | ELE_WRITE | ELE_HANGUP))) {
		errno = EINVAL;
		return -1;
	}

	TAILQ_FOREACH(e, &eloop->events, next) {
		if (e->fd == fd)
			break;
	}

	if (e == NULL) {
		added = true;
		e = TAILQ_FIRST(&eloop->free_events);
		if (e != NULL)
			TAILQ_REMOVE(&eloop->free_events, e, next);
		else {
			e = malloc(sizeof(*e));
			if (e == NULL) {
				return -1;
			}
		}
		TAILQ_INSERT_HEAD(&eloop->events, e, next);
		eloop->nevents++;
		e->fd = fd;
		e->events = 0;
	} else
		added = false;

	e->cb = cb;
	e->cb_arg = cb_arg;

#if defined(HAVE_KQUEUE)
	n = 2;
	if (events & ELE_READ && !(e->events & ELE_READ))
		EV_SET(kep++, (uintptr_t)fd, EVFILT_READ, EV_ADD, 0, 0, e);
	else if (!(events & ELE_READ) && e->events & ELE_READ)
		EV_SET(kep++, (uintptr_t)fd, EVFILT_READ, EV_DELETE, 0, 0, e);
	else
		n--;
	if (events & ELE_WRITE && !(e->events & ELE_WRITE))
		EV_SET(kep++, (uintptr_t)fd, EVFILT_WRITE, EV_ADD, 0, 0, e);
	else if (!(events & ELE_WRITE) && e->events & ELE_WRITE)
		EV_SET(kep++, (uintptr_t)fd, EVFILT_WRITE, EV_DELETE, 0, 0, e);
	else
		n--;
#ifdef EVFILT_PROCDESC
	if (events & ELE_HANGUP)
		EV_SET(kep++, (uintptr_t)fd, EVFILT_PROCDESC, EV_ADD,
		    NOTE_EXIT, 0, e);
	else
		n--;
#endif
	if (n != 0 && _kevent(eloop->fd, ke, n, NULL, 0, NULL) == -1) {
		if (added) {
			TAILQ_REMOVE(&eloop->events, e, next);
			TAILQ_INSERT_TAIL(&eloop->free_events, e, next);
		}
		return -1;
	}
#elif defined(HAVE_EPOLL)
	memset(&epe, 0, sizeof(epe));
	epe.data.ptr = e;
	if (events & ELE_READ)
		epe.events |= EPOLLIN;
	if (events & ELE_WRITE)
		epe.events |= EPOLLOUT;
	op = added ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
	if (epe.events != 0 && epoll_ctl(eloop->fd, op, fd, &epe) == -1) {
		if (added) {
			TAILQ_REMOVE(&eloop->events, e, next);
			TAILQ_INSERT_TAIL(&eloop->free_events, e, next);
		}
		return -1;
	}
#elif defined(HAVE_PPOLL)
	e->pollfd = NULL;
	UNUSED(added);
#else
	UNUSED(added);
#endif
	e->events = events;
	eloop->events_need_setup = true;
	return 0;
}

int
eloop_event_delete(struct eloop *eloop, int fd)
{
	struct eloop_event *e;
#if defined(HAVE_KQUEUE)
	struct kevent ke[2], *kep = &ke[0];
	size_t n;
#endif

	assert(eloop != NULL);
	if (fd == -1) {
		errno = EINVAL;
		return -1;
	}

	TAILQ_FOREACH(e, &eloop->events, next) {
		if (e->fd == fd)
			break;
	}
	if (e == NULL) {
		errno = ENOENT;
		return -1;
	}

#if defined(HAVE_KQUEUE)
	n = 0;
	if (e->events & ELE_READ) {
		EV_SET(kep++, (uintptr_t)fd, EVFILT_READ, EV_DELETE, 0, 0, e);
		n++;
	}
	if (e->events & ELE_WRITE) {
		EV_SET(kep++, (uintptr_t)fd, EVFILT_WRITE, EV_DELETE, 0, 0, e);
		n++;
	}
	if (n != 0 && _kevent(eloop->fd, ke, n, NULL, 0, NULL) == -1)
		return -1;
#elif defined(HAVE_EPOLL)
	if (epoll_ctl(eloop->fd, EPOLL_CTL_DEL, fd, NULL) == -1)
		return -1;
#endif
	e->fd = -1;
	eloop->nevents--;
	eloop->events_need_setup = true;
	return 1;
}

unsigned long long
eloop_timespec_diff(const struct timespec *tsp, const struct timespec *usp,
    unsigned int *nsp)
{
	unsigned long long tsecs, usecs, secs;
	long nsecs;

	if (tsp->tv_sec < 0) /* time wreapped */
		tsecs = UTIME_MAX - (unsigned long long)(-tsp->tv_sec);
	else
		tsecs = (unsigned long long)tsp->tv_sec;
	if (usp->tv_sec < 0) /* time wrapped */
		usecs = UTIME_MAX - (unsigned long long)(-usp->tv_sec);
	else
		usecs = (unsigned long long)usp->tv_sec;

	if (usecs > tsecs) /* time wrapped */
		secs = (UTIME_MAX - usecs) + tsecs;
	else
		secs = tsecs - usecs;

	nsecs = tsp->tv_nsec - usp->tv_nsec;
	if (nsecs < 0) {
		if (secs == 0)
			nsecs = 0;
		else {
			secs--;
			nsecs += NSEC_PER_SEC;
		}
	}
	if (nsp != NULL)
		*nsp = (unsigned int)nsecs;
	return secs;
}

static void
eloop_reduce_timers(struct eloop *eloop)
{
	struct timespec now;
	unsigned long long secs;
	unsigned int nsecs;
	struct eloop_timeout *t;

	clock_gettime(CLOCK_MONOTONIC, &now);
	secs = eloop_timespec_diff(&now, &eloop->now, &nsecs);

	TAILQ_FOREACH(t, &eloop->timeouts, next) {
		if (secs > t->seconds) {
			t->seconds = 0;
			t->nseconds = 0;
		} else {
			t->seconds -= (unsigned int)secs;
			if (nsecs > t->nseconds) {
				if (t->seconds == 0)
					t->nseconds = 0;
				else {
					t->seconds--;
					t->nseconds = NSEC_PER_SEC
					    - (nsecs - t->nseconds);
				}
			} else
				t->nseconds -= nsecs;
		}
	}

	eloop->now = now;
}

/*
 * This implementation should cope with UINT_MAX seconds on a system
 * where time_t is INT32_MAX. It should also cope with the monotonic timer
 * wrapping, although this is highly unlikely.
 * unsigned int should match or be greater than any on wire specified timeout.
 */
static int
eloop_q_timeout_add(struct eloop *eloop, int queue,
    unsigned int seconds, unsigned int nseconds,
    void (*callback)(void *), void *arg)
{
	struct eloop_timeout *t, *tt = NULL;

	assert(eloop != NULL);
	assert(callback != NULL);
	assert(nseconds <= NSEC_PER_SEC);

	/* Remove existing timeout if present. */
	TAILQ_FOREACH(t, &eloop->timeouts, next) {
		if (t->callback == callback && t->arg == arg) {
			TAILQ_REMOVE(&eloop->timeouts, t, next);
			break;
		}
	}

	if (t == NULL) {
		/* No existing, so allocate or grab one from the free pool. */
		if ((t = TAILQ_FIRST(&eloop->free_timeouts))) {
			TAILQ_REMOVE(&eloop->free_timeouts, t, next);
		} else {
			if ((t = malloc(sizeof(*t))) == NULL)
				return -1;
		}
	}

	eloop_reduce_timers(eloop);

	t->seconds = seconds;
	t->nseconds = nseconds;
	t->callback = callback;
	t->arg = arg;
	t->queue = queue;

	/* The timeout list should be in chronological order,
	 * soonest first. */
	TAILQ_FOREACH(tt, &eloop->timeouts, next) {
		if (t->seconds < tt->seconds ||
		    (t->seconds == tt->seconds && t->nseconds < tt->nseconds))
		{
			TAILQ_INSERT_BEFORE(tt, t, next);
			return 0;
		}
	}
	TAILQ_INSERT_TAIL(&eloop->timeouts, t, next);
	return 0;
}

int
eloop_q_timeout_add_tv(struct eloop *eloop, int queue,
    const struct timespec *when, void (*callback)(void *), void *arg)
{

	if (when->tv_sec < 0 || (unsigned long)when->tv_sec > UINT_MAX) {
		errno = EINVAL;
		return -1;
	}
	if (when->tv_nsec < 0 || when->tv_nsec > NSEC_PER_SEC) {
		errno = EINVAL;
		return -1;
	}

	return eloop_q_timeout_add(eloop, queue,
	    (unsigned int)when->tv_sec, (unsigned int)when->tv_sec,
	    callback, arg);
}

int
eloop_q_timeout_add_sec(struct eloop *eloop, int queue, unsigned int seconds,
    void (*callback)(void *), void *arg)
{

	return eloop_q_timeout_add(eloop, queue, seconds, 0, callback, arg);
}

int
eloop_q_timeout_add_msec(struct eloop *eloop, int queue, unsigned long when,
    void (*callback)(void *), void *arg)
{
	unsigned long seconds, nseconds;

	seconds = when / MSEC_PER_SEC;
	if (seconds > UINT_MAX) {
		errno = EINVAL;
		return -1;
	}

	nseconds = (when % MSEC_PER_SEC) * NSEC_PER_MSEC;
	return eloop_q_timeout_add(eloop, queue,
		(unsigned int)seconds, (unsigned int)nseconds, callback, arg);
}

int
eloop_q_timeout_delete(struct eloop *eloop, int queue,
    void (*callback)(void *), void *arg)
{
	struct eloop_timeout *t, *tt;
	int n;

	assert(eloop != NULL);

	n = 0;
	TAILQ_FOREACH_SAFE(t, &eloop->timeouts, next, tt) {
		if ((queue == 0 || t->queue == queue) &&
		    t->arg == arg &&
		    (!callback || t->callback == callback))
		{
			TAILQ_REMOVE(&eloop->timeouts, t, next);
			TAILQ_INSERT_TAIL(&eloop->free_timeouts, t, next);
			n++;
		}
	}
	return n;
}

void
eloop_exit(struct eloop *eloop, int code)
{

	assert(eloop != NULL);

	eloop->exitcode = code;
	eloop->exitnow = true;
}

void
eloop_enter(struct eloop *eloop)
{

	assert(eloop != NULL);

	eloop->exitnow = false;
}

/* Must be called after fork(2) */
int
eloop_forked(struct eloop *eloop)
{
#if defined(HAVE_KQUEUE) || defined(HAVE_EPOLL)
	struct eloop_event *e;
#if defined(HAVE_KQUEUE)
	struct kevent *pfds, *pfd;
	size_t i;
#elif defined(HAVE_EPOLL)
	struct epoll_event epe = { .events = 0 };
#endif

	assert(eloop != NULL);
#if defined(HAVE_KQUEUE) || defined(HAVE_EPOLL)
	if (eloop->fd != -1)
		close(eloop->fd);
	if (eloop_open(eloop) == -1)
		return -1;
#endif

#ifdef HAVE_KQUEUE
	pfds = malloc((eloop->nsignals + (eloop->nevents * NFD)) * sizeof(*pfds));
	pfd = pfds;

	if (eloop->signal_cb != NULL) {
		for (i = 0; i < eloop->nsignals; i++) {
			EV_SET(pfd++, (uintptr_t)eloop->signals[i],
			    EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
		}
	} else
		i = 0;
#endif

	TAILQ_FOREACH(e, &eloop->events, next) {
		if (e->fd == -1)
			continue;
#if defined(HAVE_KQUEUE)
		if (e->events & ELE_READ) {
			EV_SET(pfd++, (uintptr_t)e->fd,
			    EVFILT_READ, EV_ADD, 0, 0, e);
			i++;
		}
		if (e->events & ELE_WRITE) {
			EV_SET(pfd++, (uintptr_t)e->fd,
			    EVFILT_WRITE, EV_ADD, 0, 0, e);
			i++;
		}
#elif defined(HAVE_EPOLL)
		memset(&epe, 0, sizeof(epe));
		epe.data.ptr = e;
		if (e->events & ELE_READ)
			epe.events |= EPOLLIN;
		if (e->events & ELE_WRITE)
			epe.events |= EPOLLOUT;
		if (epoll_ctl(eloop->fd, EPOLL_CTL_ADD, e->fd, &epe) == -1)
			return -1;
#endif
	}

#if defined(HAVE_KQUEUE)
	if (i == 0)
		return 0;
	return _kevent(eloop->fd, pfds, i, NULL, 0, NULL);
#else
	return 0;
#endif
#else
	UNUSED(eloop);
	return 0;
#endif
}

int
eloop_open(struct eloop *eloop)
{
#if defined(HAVE_KQUEUE) || defined(HAVE_EPOLL)
	int fd;

	assert(eloop != NULL);
#if defined(HAVE_KQUEUE1)
	fd = kqueue1(O_CLOEXEC);
#elif defined(HAVE_KQUEUE)
	int flags;

	fd = kqueue();
	flags = fcntl(fd, F_GETFD, 0);
	if (!(flags != -1 && !(flags & FD_CLOEXEC) &&
	    fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == 0))
	{
		close(fd);
		return -1;
	}
#elif defined(HAVE_EPOLL)
	fd = epoll_create1(EPOLL_CLOEXEC);
#endif

	eloop->fd = fd;
	return fd;
#else
	UNUSED(eloop);
	return 0;
#endif
}

int
eloop_signal_set_cb(struct eloop *eloop,
    const int *signals, size_t nsignals,
    void (*signal_cb)(int, void *), void *signal_cb_ctx)
{
#ifdef HAVE_KQUEUE
	size_t i;
	struct kevent *ke, *kes;
#endif
	int error = 0;

	assert(eloop != NULL);

#ifdef HAVE_KQUEUE
	ke = kes = malloc(MAX(eloop->nsignals, nsignals) * sizeof(*kes));
	if (kes == NULL)
		return -1;
	for (i = 0; i < eloop->nsignals; i++) {
		EV_SET(ke++, (uintptr_t)eloop->signals[i],
		    EVFILT_SIGNAL, EV_DELETE, 0, 0, NULL);
	}
	if (i != 0 && _kevent(eloop->fd, kes, i, NULL, 0, NULL) == -1) {
		error = -1;
		goto out;
	}
#endif

	eloop->signals = signals;
	eloop->nsignals = nsignals;
	eloop->signal_cb = signal_cb;
	eloop->signal_cb_ctx = signal_cb_ctx;

#ifdef HAVE_KQUEUE
	if (signal_cb == NULL)
		goto out;
	ke = kes;
	for (i = 0; i < eloop->nsignals; i++) {
		EV_SET(ke++, (uintptr_t)eloop->signals[i],
		    EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
	}
	if (i != 0 && _kevent(eloop->fd, kes, i, NULL, 0, NULL) == -1)
		error = -1;
out:
	free(kes);
#endif

	return error;
}

#ifndef HAVE_KQUEUE
static volatile int _eloop_sig[ELOOP_NSIGNALS];
static volatile size_t _eloop_nsig;

static void
eloop_signal3(int sig, __unused siginfo_t *siginfo, __unused void *arg)
{

	if (_eloop_nsig == __arraycount(_eloop_sig)) {
#ifdef ELOOP_DEBUG
		fprintf(stderr, "%s: signal storm, discarding signal %d\n",
		    __func__, sig);
#endif
		return;
	}

	_eloop_sig[_eloop_nsig++] = sig;
}
#endif

int
eloop_signal_mask(struct eloop *eloop, sigset_t *oldset)
{
	sigset_t newset;
	size_t i;
#ifndef HAVE_KQUEUE
	struct sigaction sa = {
	    .sa_sigaction = eloop_signal3,
	    .sa_flags = SA_SIGINFO,
	};
#endif

	assert(eloop != NULL);

	sigemptyset(&newset);
	for (i = 0; i < eloop->nsignals; i++)
		sigaddset(&newset, eloop->signals[i]);
	if (sigprocmask(SIG_SETMASK, &newset, oldset) == -1)
		return -1;

#ifndef HAVE_KQUEUE
	sigemptyset(&sa.sa_mask);

	for (i = 0; i < eloop->nsignals; i++) {
		if (sigaction(eloop->signals[i], &sa, NULL) == -1)
			return -1;
	}
#endif

	return 0;
}

struct eloop *
eloop_new(void)
{
	struct eloop *eloop;

	eloop = calloc(1, sizeof(*eloop));
	if (eloop == NULL)
		return NULL;

	/* Check we have a working monotonic clock. */
	if (clock_gettime(CLOCK_MONOTONIC, &eloop->now) == -1) {
		free(eloop);
		return NULL;
	}

	TAILQ_INIT(&eloop->events);
	TAILQ_INIT(&eloop->free_events);
	TAILQ_INIT(&eloop->timeouts);
	TAILQ_INIT(&eloop->free_timeouts);
	eloop->exitcode = EXIT_FAILURE;

#if defined(HAVE_KQUEUE) || defined(HAVE_EPOLL)
	if (eloop_open(eloop) == -1) {
		eloop_free(eloop);
		return NULL;
	}
#endif

	return eloop;
}

void
eloop_clear(struct eloop *eloop, ...)
{
	va_list va1, va2;
	int except_fd;
	struct eloop_event *e, *ne;
	struct eloop_timeout *t;

	if (eloop == NULL)
		return;

	va_start(va1, eloop);
	TAILQ_FOREACH_SAFE(e, &eloop->events, next, ne) {
		va_copy(va2, va1);
		do
			except_fd = va_arg(va2, int);
		while (except_fd != -1 && except_fd != e->fd);
		va_end(va2);
		if (e->fd == except_fd && e->fd != -1)
			continue;
		TAILQ_REMOVE(&eloop->events, e, next);
		if (e->fd != -1) {
			close(e->fd);
			eloop->nevents--;
		}
		free(e);
	}
	va_end(va1);

#if !defined(HAVE_PSELECT)
	/* Free the pollfd buffer and ensure it's re-created before
	 * the next run. This allows us to shrink it incase we use a lot less
	 * signals and fds to respond to after forking. */
	free(eloop->fds);
	eloop->fds = NULL;
	eloop->nfds = 0;
	eloop->events_need_setup = true;
#endif

	while ((e = TAILQ_FIRST(&eloop->free_events))) {
		TAILQ_REMOVE(&eloop->free_events, e, next);
		free(e);
	}
	while ((t = TAILQ_FIRST(&eloop->timeouts))) {
		TAILQ_REMOVE(&eloop->timeouts, t, next);
		free(t);
	}
	while ((t = TAILQ_FIRST(&eloop->free_timeouts))) {
		TAILQ_REMOVE(&eloop->free_timeouts, t, next);
		free(t);
	}
	eloop->cleared = true;
}

void
eloop_free(struct eloop *eloop)
{

	eloop_clear(eloop, -1);
#if defined(HAVE_KQUEUE) || defined(HAVE_EPOLL)
	if (eloop != NULL && eloop->fd != -1)
		close(eloop->fd);
#endif
	free(eloop);
}

#if defined(HAVE_KQUEUE)
static int
eloop_run_kqueue(struct eloop *eloop, const struct timespec *ts)
{
	int n, nn;
	struct kevent *ke;
	struct eloop_event *e;
	unsigned short events;

	n = _kevent(eloop->fd, NULL, 0, eloop->fds, eloop->nevents, ts);
	if (n == -1)
		return -1;

	for (nn = n, ke = eloop->fds; nn != 0; nn--, ke++) {
		if (eloop->cleared || eloop->exitnow)
			break;
		e = (struct eloop_event *)ke->udata;
		if (ke->filter == EVFILT_SIGNAL) {
			eloop->signal_cb((int)ke->ident,
			    eloop->signal_cb_ctx);
			continue;
		}
		if (ke->filter == EVFILT_READ)
			events = ELE_READ;
		else if (ke->filter == EVFILT_WRITE)
			events = ELE_WRITE;
#ifdef EVFILT_PROCDESC
		else if (ke->filter == EVFILT_PROCDESC &&
		    ke->fflags & NOTE_EXIT)
			/* exit status is in ke->data.
			 * As we default to using ppoll anyway
			 * we don't have to do anything with it right now. */
			events = ELE_HANGUP;
#endif
		else
			continue; /* assert? */
		if (ke->flags & EV_EOF)
			events |= ELE_HANGUP;
		if (ke->flags & EV_ERROR)
			events |= ELE_ERROR;
		e->cb(e->cb_arg, events);
	}
	return n;
}

#elif defined(HAVE_EPOLL)

static int
eloop_run_epoll(struct eloop *eloop,
    const struct timespec *ts, const sigset_t *signals)
{
	int timeout, n, nn;
	struct epoll_event *epe;
	struct eloop_event *e;
	unsigned short events;

	if (ts != NULL) {
		if (ts->tv_sec > INT_MAX / 1000 ||
		    (ts->tv_sec == INT_MAX / 1000 &&
		     ((ts->tv_nsec + 999999) / 1000000 > INT_MAX % 1000000)))
			timeout = INT_MAX;
		else
			timeout = (int)(ts->tv_sec * 1000 +
			    (ts->tv_nsec + 999999) / 1000000);
	} else
		timeout = -1;

	if (signals != NULL)
		n = epoll_pwait(eloop->fd, eloop->fds,
		    (int)eloop->nevents, timeout, signals);
	else
		n = epoll_wait(eloop->fd, eloop->fds,
		    (int)eloop->nevents, timeout);
	if (n == -1)
		return -1;

	for (nn = n, epe = eloop->fds; nn != 0; nn--, epe++) {
		if (eloop->cleared || eloop->exitnow)
			break;
		e = (struct eloop_event *)epe->data.ptr;
		if (e->fd == -1)
			continue;
		events = 0;
		if (epe->events & EPOLLIN)
			events |= ELE_READ;
		if (epe->events & EPOLLOUT)
			events |= ELE_WRITE;
		if (epe->events & EPOLLHUP)
			events |= ELE_HANGUP;
		if (epe->events & EPOLLERR)
			events |= ELE_ERROR;
		e->cb(e->cb_arg, events);
	}
	return n;
}

#elif defined(HAVE_PPOLL)

static int
eloop_run_ppoll(struct eloop *eloop,
    const struct timespec *ts, const sigset_t *signals)
{
	int n, nn;
	struct eloop_event *e;
	struct pollfd *pfd;
	unsigned short events;

	n = ppoll(eloop->fds, (nfds_t)eloop->nevents, ts, signals);
	if (n == -1 || n == 0)
		return n;

	nn = n;
	TAILQ_FOREACH(e, &eloop->events, next) {
		if (eloop->cleared || eloop->exitnow)
			break;
		/* Skip freshly added events */
		if ((pfd = e->pollfd) == NULL)
			continue;
		if (e->pollfd->revents) {
			nn--;
			events = 0;
			if (pfd->revents & POLLIN)
				events |= ELE_READ;
			if (pfd->revents & POLLOUT)
				events |= ELE_WRITE;
			if (pfd->revents & POLLHUP)
				events |= ELE_HANGUP;
			if (pfd->revents & POLLERR)
				events |= ELE_ERROR;
			if (pfd->revents & POLLNVAL)
				events |= ELE_NVAL;
			if (events)
				e->cb(e->cb_arg, events);
		}
		if (nn == 0)
			break;
	}
	return n;
}

#elif defined(HAVE_PSELECT)

static int
eloop_run_pselect(struct eloop *eloop,
    const struct timespec *ts, const sigset_t *sigmask)
{
	fd_set read_fds, write_fds;
	int maxfd, n;
	struct eloop_event *e;
	unsigned short events;

	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);
	maxfd = 0;
	TAILQ_FOREACH(e, &eloop->events, next) {
		if (e->fd == -1)
			continue;
		if (e->events & ELE_READ) {
			FD_SET(e->fd, &read_fds);
			if (e->fd > maxfd)
				maxfd = e->fd;
		}
		if (e->events & ELE_WRITE) {
			FD_SET(e->fd, &write_fds);
			if (e->fd > maxfd)
				maxfd = e->fd;
		}
	}

	/* except_fd's is for STREAMS devices which we don't use. */
	n = pselect(maxfd + 1, &read_fds, &write_fds, NULL, ts, sigmask);
	if (n == -1 || n == 0)
		return n;

	TAILQ_FOREACH(e, &eloop->events, next) {
		if (eloop->cleared || eloop->exitnow)
			break;
		if (e->fd == -1)
			continue;
		events = 0;
		if (FD_ISSET(e->fd, &read_fds))
			events |= ELE_READ;
		if (FD_ISSET(e->fd, &write_fds))
			events |= ELE_WRITE;
		if (events)
			e->cb(e->cb_arg, events);
	}

	return n;
}
#endif

int
eloop_start(struct eloop *eloop, sigset_t *signals)
{
	int error;
	struct eloop_timeout *t;
	struct timespec ts, *tsp;

	assert(eloop != NULL);
#ifdef HAVE_KQUEUE
	UNUSED(signals);
#endif

	for (;;) {
		if (eloop->exitnow)
			break;

#ifndef HAVE_KQUEUE
		if (_eloop_nsig != 0) {
			int n = _eloop_sig[--_eloop_nsig];

			if (eloop->signal_cb != NULL)
				eloop->signal_cb(n, eloop->signal_cb_ctx);
			continue;
		}
#endif

		t = TAILQ_FIRST(&eloop->timeouts);
		if (t == NULL && eloop->nevents == 0)
			break;

		if (t != NULL)
			eloop_reduce_timers(eloop);

		if (t != NULL && t->seconds == 0 && t->nseconds == 0) {
			TAILQ_REMOVE(&eloop->timeouts, t, next);
			t->callback(t->arg);
			TAILQ_INSERT_TAIL(&eloop->free_timeouts, t, next);
			continue;
		}

		if (t != NULL) {
			if (t->seconds > INT_MAX) {
				ts.tv_sec = (time_t)INT_MAX;
				ts.tv_nsec = 0;
			} else {
				ts.tv_sec = (time_t)t->seconds;
				ts.tv_nsec = (long)t->nseconds;
			}
			tsp = &ts;
		} else
			tsp = NULL;

		eloop->cleared = false;
		if (eloop->events_need_setup)
			eloop_event_setup_fds(eloop);

#if defined(HAVE_KQUEUE)
		UNUSED(signals);
		error = eloop_run_kqueue(eloop, tsp);
#elif defined(HAVE_EPOLL)
		error = eloop_run_epoll(eloop, tsp, signals);
#elif defined(HAVE_PPOLL)
		error = eloop_run_ppoll(eloop, tsp, signals);
#elif defined(HAVE_PSELECT)
		error = eloop_run_pselect(eloop, tsp, signals);
#else
#error no polling mechanism to run!
#endif
		if (error == -1) {
			if (errno == EINTR)
				continue;
			return -errno;
		}
	}

	return eloop->exitcode;
}
