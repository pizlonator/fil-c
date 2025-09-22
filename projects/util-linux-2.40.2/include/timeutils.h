/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * First set of functions in this file are part of systemd, and were
 * copied to util-linux at August 2013.
 *
 * Copyright 2010 Lennart Poettering
 * Copyright (C) 2014 Karel Zak <kzak@redhat.com>
 *
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 */
#ifndef UTIL_LINUX_TIME_UTIL_H
#define UTIL_LINUX_TIME_UTIL_H

#include <stdio.h>
#include <inttypes.h>
#include <sys/time.h>
#include <stdbool.h>

typedef uint64_t usec_t;
typedef uint64_t nsec_t;

#define MSEC_PER_SEC  1000ULL
#define USEC_PER_SEC  1000000ULL
#define USEC_PER_MSEC 1000ULL
#define NSEC_PER_SEC  1000000000ULL
#define NSEC_PER_MSEC 1000000ULL
#define NSEC_PER_USEC 1000ULL

#define USEC_PER_MINUTE	(60ULL*USEC_PER_SEC)
#define NSEC_PER_MINUTE	(60ULL*NSEC_PER_SEC)
#define USEC_PER_HOUR	(60ULL*USEC_PER_MINUTE)
#define NSEC_PER_HOUR	(60ULL*NSEC_PER_MINUTE)
#define USEC_PER_DAY	(24ULL*USEC_PER_HOUR)
#define NSEC_PER_DAY	(24ULL*NSEC_PER_HOUR)
#define USEC_PER_WEEK	(7ULL*USEC_PER_DAY)
#define NSEC_PER_WEEK	(7ULL*NSEC_PER_DAY)
#define USEC_PER_MONTH	(2629800ULL*USEC_PER_SEC)
#define NSEC_PER_MONTH	(2629800ULL*NSEC_PER_SEC)
#define USEC_PER_YEAR	(31557600ULL*USEC_PER_SEC)
#define NSEC_PER_YEAR	(31557600ULL*NSEC_PER_SEC)

#define FORMAT_TIMESTAMP_MAX ((4*4+1)+11+9+4+1)	/* weekdays can be unicode */
#define FORMAT_TIMESTAMP_RELATIVE_MAX 256
#define FORMAT_TIMESPAN_MAX 64

int parse_timestamp(const char *t, usec_t *usec);
int get_gmtoff(const struct tm *tp);

/* flags and masks for strxxx_iso() functions */
enum {
	ISO_DATE		= (1 << 0),
	ISO_TIME		= (1 << 1),
	ISO_TIMEZONE		= (1 << 2),
	ISO_DOTUSEC		= (1 << 3),
	ISO_COMMAUSEC		= (1 << 4),
	ISO_DOTNSEC		= (1 << 5),
	ISO_COMMANSEC		= (1 << 6),
	ISO_T			= (1 << 7),
	ISO_GMTIME		= (1 << 8),
	ISO_TIMESTAMP		= ISO_DATE | ISO_TIME | ISO_TIMEZONE,
	ISO_TIMESTAMP_T		= ISO_TIMESTAMP | ISO_T,
	ISO_TIMESTAMP_DOT	= ISO_TIMESTAMP | ISO_DOTUSEC,
	ISO_TIMESTAMP_DOT_T	= ISO_TIMESTAMP_DOT | ISO_T,
	ISO_TIMESTAMP_COMMA	= ISO_TIMESTAMP | ISO_COMMAUSEC,
	ISO_TIMESTAMP_COMMA_T	= ISO_TIMESTAMP_COMMA | ISO_T,
	ISO_TIMESTAMP_COMMA_G	= ISO_TIMESTAMP_COMMA | ISO_GMTIME,
	ISO_TIMESTAMP_COMMA_GT  = ISO_TIMESTAMP_COMMA_G | ISO_T
};

#define CTIME_BUFSIZ	26
#define ISO_BUFSIZ	42

int strtimeval_iso(const struct timeval *tv, int flags, char *buf, size_t bufsz);
int strtm_iso(const struct tm *tm, int flags, char *buf, size_t bufsz);
int strtime_iso(const time_t *t, int flags, char *buf, size_t bufsz);
int strtimespec_iso(const struct timespec *t, int flags, char *buf, size_t bufsz);
int strtimespec_relative(const struct timespec *ts, char *buf, size_t bufsz);

#define UL_SHORTTIME_THISYEAR_HHMM (1 << 1)

int strtime_short(const time_t *t, struct timeval *now, int flags, char *buf, size_t bufsz);

#ifndef HAVE_TIMEGM
extern time_t timegm(struct tm *tm);
#endif

static inline usec_t timeval_to_usec(const struct timeval *t)
{
	return t->tv_sec * USEC_PER_SEC + t->tv_usec;
}

static inline usec_t timespec_to_usec(const struct timespec *t)
{
	return t->tv_sec * USEC_PER_SEC + t->tv_nsec / NSEC_PER_USEC;
}

static inline struct timeval usec_to_timeval(usec_t t)
{
	struct timeval r = {
		.tv_sec = t / USEC_PER_SEC,
		.tv_usec = t % USEC_PER_SEC,
	};
	return r;
}

static inline bool is_timespecset(const struct timespec *t)
{
	return t->tv_sec || t->tv_nsec;
}

#endif /* UTIL_LINUX_TIME_UTIL_H */
