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
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <inttypes.h>

#include "c.h"
#include "nls.h"
#include "strutils.h"
#include "timeutils.h"

#define WHITESPACE " \t\n\r"

#define streq(a,b) (strcmp((a),(b)) == 0)

static int parse_sec(const char *t, usec_t *usec)
{
	static const struct {
		const char *suffix;
		usec_t usec;
	} table[] = {
		{ "seconds",	USEC_PER_SEC },
		{ "second",	USEC_PER_SEC },
		{ "sec",	USEC_PER_SEC },
		{ "s",		USEC_PER_SEC },
		{ "minutes",	USEC_PER_MINUTE },
		{ "minute",	USEC_PER_MINUTE },
		{ "min",	USEC_PER_MINUTE },
		{ "months",	USEC_PER_MONTH },
		{ "month",	USEC_PER_MONTH },
		{ "msec",	USEC_PER_MSEC },
		{ "ms",		USEC_PER_MSEC },
		{ "m",		USEC_PER_MINUTE },
		{ "hours",	USEC_PER_HOUR },
		{ "hour",	USEC_PER_HOUR },
		{ "hr",		USEC_PER_HOUR },
		{ "h",		USEC_PER_HOUR },
		{ "days",	USEC_PER_DAY },
		{ "day",	USEC_PER_DAY },
		{ "d",		USEC_PER_DAY },
		{ "weeks",	USEC_PER_WEEK },
		{ "week",	USEC_PER_WEEK },
		{ "w",		USEC_PER_WEEK },
		{ "years",	USEC_PER_YEAR },
		{ "year",	USEC_PER_YEAR },
		{ "y",		USEC_PER_YEAR },
		{ "usec",	1ULL },
		{ "us",		1ULL },
		{ "",		USEC_PER_SEC },	/* default is sec */
	};

	const char *p;
	usec_t r = 0;
	int something = FALSE;

	assert(t);
	assert(usec);

	p = t;
	for (;;) {
		long long l, z = 0;
		char *e;
		unsigned i, n = 0;

		p += strspn(p, WHITESPACE);

		if (*p == 0) {
			if (!something)
				return -EINVAL;

			break;
		}

		errno = 0;
		l = strtoll(p, &e, 10);

		if (errno > 0)
			return -errno;

		if (l < 0)
			return -ERANGE;

		if (*e == '.') {
			char *b = e + 1;

			errno = 0;
			z = strtoll(b, &e, 10);
			if (errno > 0)
				return -errno;

			if (z < 0)
				return -ERANGE;

			if (e == b)
				return -EINVAL;

			n = e - b;

		} else if (e == p)
			return -EINVAL;

		e += strspn(e, WHITESPACE);

		for (i = 0; i < ARRAY_SIZE(table); i++)
			if (startswith(e, table[i].suffix)) {
				usec_t k = (usec_t) z * table[i].usec;

				for (; n > 0; n--)
					k /= 10;

				r += (usec_t) l *table[i].usec + k;
				p = e + strlen(table[i].suffix);

				something = TRUE;
				break;
			}

		if (i >= ARRAY_SIZE(table))
			return -EINVAL;

	}

	*usec = r;

	return 0;
}

static int parse_subseconds(const char *t, usec_t *usec)
{
	usec_t ret = 0;
	int factor = USEC_PER_SEC / 10;

	if (*t != '.' && *t != ',')
		return -1;

	while (*(++t)) {
		if (!isdigit(*t) || factor < 1)
			return -1;

		ret += ((usec_t) *t - '0') * factor;
		factor /= 10;
	}

	*usec = ret;
	return 0;
}

static const char *parse_epoch_seconds(const char *t, struct tm *tm)
{
	int64_t s;
	time_t st;
	int f, c;

	f = sscanf(t, "%"SCNd64"%n", &s, &c);
	if (f < 1)
		return NULL;
	st = s;
	if ((int64_t) st < s)
		return NULL;
	if (!localtime_r(&st, tm))
		return NULL;
	return t + c;
}

static int parse_timestamp_reference(time_t x, const char *t, usec_t *usec)
{
	static const struct {
		const char *name;
		const int nr;
	} day_nr[] = {
		{ "Sunday",	0 },
		{ "Sun",	0 },
		{ "Monday",	1 },
		{ "Mon",	1 },
		{ "Tuesday",	2 },
		{ "Tue",	2 },
		{ "Wednesday",	3 },
		{ "Wed",	3 },
		{ "Thursday",	4 },
		{ "Thu",	4 },
		{ "Friday",	5 },
		{ "Fri",	5 },
		{ "Saturday",	6 },
		{ "Sat",	6 },
	};

	const char *k;
	struct tm tm, copy;
	usec_t plus = 0, minus = 0, ret = 0;
	int r, weekday = -1;
	unsigned i;

	/*
	 * Allowed syntaxes:
	 *
	 *   2012-09-22 16:34:22 !
	 *   2012-09-22T16:34:22 !
	 *   20120922163422      !
	 *   @1348331662	 ! (seconds since the Epoch (1970-01-01 00:00 UTC))
	 *   2012-09-22 16:34	   (seconds will be set to 0)
	 *   2012-09-22		   (time will be set to 00:00:00)
	 *   16:34:22		 ! (date will be set to today)
	 *   16:34		   (date will be set to today, seconds to 0)
	 *   now
	 *   yesterday		   (time is set to 00:00:00)
	 *   today		   (time is set to 00:00:00)
	 *   tomorrow		   (time is set to 00:00:00)
	 *   +5min
	 *   -5days
	 *
	 *   Syntaxes marked with '!' also optionally allow up to six digits of
	 *   subsecond granularity, separated by '.' or ',':
	 *
	 *   2012-09-22 16:34:22.12
	 *   2012-09-22 16:34:22.123456
	 *
	 *
	 */

	assert(t);
	assert(usec);

	localtime_r(&x, &tm);
	tm.tm_isdst = -1;

	if (streq(t, "now"))
		goto finish;

	else if (streq(t, "today")) {
		tm.tm_sec = tm.tm_min = tm.tm_hour = 0;
		goto finish;

	} else if (streq(t, "yesterday")) {
		tm.tm_mday--;
		tm.tm_sec = tm.tm_min = tm.tm_hour = 0;
		goto finish;

	} else if (streq(t, "tomorrow")) {
		tm.tm_mday++;
		tm.tm_sec = tm.tm_min = tm.tm_hour = 0;
		goto finish;

	} else if (t[0] == '+') {

		r = parse_sec(t + 1, &plus);
		if (r < 0)
			return r;

		goto finish;
	} else if (t[0] == '-') {

		r = parse_sec(t + 1, &minus);
		if (r < 0)
			return r;

		goto finish;
	} else if (t[0] == '@') {
		k = parse_epoch_seconds(t + 1, &tm);
		if (k && *k == 0)
			goto finish;
		else if (k && parse_subseconds(k, &ret) == 0)
			goto finish;

		return -EINVAL;
	} else if (endswith(t, " ago")) {
		char *z;

		z = strndup(t, strlen(t) - 4);
		if (!z)
			return -ENOMEM;

		r = parse_sec(z, &minus);
		free(z);
		if (r < 0)
			return r;

		goto finish;
	}

	for (i = 0; i < ARRAY_SIZE(day_nr); i++) {
		size_t skip;

		if (!startswith_no_case(t, day_nr[i].name))
			continue;

		skip = strlen(day_nr[i].name);
		if (t[skip] != ' ')
			continue;

		weekday = day_nr[i].nr;
		t += skip + 1;
		break;
	}

	copy = tm;
	k = strptime(t, "%y-%m-%d %H:%M:%S", &tm);
	if (k && *k == 0)
		goto finish;
	else if (k && parse_subseconds(k, &ret) == 0)
		goto finish;

	tm = copy;
	k = strptime(t, "%Y-%m-%d %H:%M:%S", &tm);
	if (k && *k == 0)
		goto finish;
	else if (k && parse_subseconds(k, &ret) == 0)
		goto finish;

	tm = copy;
	k = strptime(t, "%Y-%m-%dT%H:%M:%S", &tm);
	if (k && *k == 0)
		goto finish;
	else if (k && parse_subseconds(k, &ret) == 0)
		goto finish;

	tm = copy;
	k = strptime(t, "%y-%m-%d %H:%M", &tm);
	if (k && *k == 0) {
		tm.tm_sec = 0;
		goto finish;
	}

	tm = copy;
	k = strptime(t, "%Y-%m-%d %H:%M", &tm);
	if (k && *k == 0) {
		tm.tm_sec = 0;
		goto finish;
	}

	tm = copy;
	k = strptime(t, "%y-%m-%d", &tm);
	if (k && *k == 0) {
		tm.tm_sec = tm.tm_min = tm.tm_hour = 0;
		goto finish;
	}

	tm = copy;
	k = strptime(t, "%Y-%m-%d", &tm);
	if (k && *k == 0) {
		tm.tm_sec = tm.tm_min = tm.tm_hour = 0;
		goto finish;
	}

	tm = copy;
	k = strptime(t, "%H:%M:%S", &tm);
	if (k && *k == 0)
		goto finish;
	else if (k && parse_subseconds(k, &ret) == 0)
		goto finish;

	tm = copy;
	k = strptime(t, "%H:%M", &tm);
	if (k && *k == 0) {
		tm.tm_sec = 0;
		goto finish;
	}

	tm = copy;
	k = strptime(t, "%Y%m%d%H%M%S", &tm);
	if (k && *k == 0)
		goto finish;
	else if (k && parse_subseconds(k, &ret) == 0)
		goto finish;

	return -EINVAL;

 finish:
	x = mktime(&tm);
	if (x == (time_t)-1)
		return -EINVAL;

	if (weekday >= 0 && tm.tm_wday != weekday)
		return -EINVAL;

	ret += (usec_t) x * USEC_PER_SEC;

	if (minus > ret)
		return -ERANGE;
	if ((ret + plus) < ret)
		return -ERANGE;

	ret += plus;
	ret -= minus;

	*usec = ret;

	return 0;
}

int parse_timestamp(const char *t, usec_t *usec)
{
	return parse_timestamp_reference(time(NULL), t, usec);
}

/* Returns the difference in seconds between its argument and GMT. If if TP is
 * invalid or no DST information is available default to UTC, that is, zero.
 * tzset is called so, for example, 'TZ="UTC" hwclock' will work as expected.
 * Derived from glibc/time/strftime_l.c
 */
int get_gmtoff(const struct tm *tp)
{
	if (tp->tm_isdst < 0)
		return 0;

#if HAVE_TM_GMTOFF
	return tp->tm_gmtoff;
#else
	struct tm tm;
	struct tm gtm;
	struct tm ltm = *tp;
	time_t lt;

	tzset();
	lt = mktime(&ltm);
	/* Check if mktime returning -1 is an error or a valid time_t */
	if (lt == (time_t) -1) {
		if (! localtime_r(&lt, &tm)
			|| ((ltm.tm_sec ^ tm.tm_sec)
			    | (ltm.tm_min ^ tm.tm_min)
			    | (ltm.tm_hour ^ tm.tm_hour)
			    | (ltm.tm_mday ^ tm.tm_mday)
			    | (ltm.tm_mon ^ tm.tm_mon)
			    | (ltm.tm_year ^ tm.tm_year)))
			return 0;
	}

	if (! gmtime_r(&lt, &gtm))
		return 0;

	/* Calculate the GMT offset, that is, the difference between the
	 * TP argument (ltm) and GMT (gtm).
	 *
	 * Compute intervening leap days correctly even if year is negative.
	 * Take care to avoid int overflow in leap day calculations, but it's OK
	 * to assume that A and B are close to each other.
	 */
	int a4 = (ltm.tm_year >> 2) + (1900 >> 2) - ! (ltm.tm_year & 3);
	int b4 = (gtm.tm_year >> 2) + (1900 >> 2) - ! (gtm.tm_year & 3);
	int a100 = a4 / 25 - (a4 % 25 < 0);
	int b100 = b4 / 25 - (b4 % 25 < 0);
	int a400 = a100 >> 2;
	int b400 = b100 >> 2;
	int intervening_leap_days = (a4 - b4) - (a100 - b100) + (a400 - b400);

	int years = ltm.tm_year - gtm.tm_year;
	int days = (365 * years + intervening_leap_days
		    + (ltm.tm_yday - gtm.tm_yday));

	return (60 * (60 * (24 * days + (ltm.tm_hour - gtm.tm_hour))
		+ (ltm.tm_min - gtm.tm_min)) + (ltm.tm_sec - gtm.tm_sec));
#endif
}

static int format_iso_time(const struct tm *tm, uint32_t nsec, int flags, char *buf, size_t bufsz)
{
	uint32_t usec = nsec / NSEC_PER_USEC;
	char *p = buf;
	int len;

	if (flags & ISO_DATE) {
		len = snprintf(p, bufsz, "%4ld-%.2d-%.2d",
			       tm->tm_year + (long) 1900,
			       tm->tm_mon + 1, tm->tm_mday);
		if (len < 0 || (size_t) len > bufsz)
			goto err;
		bufsz -= len;
		p += len;
	}

	if ((flags & ISO_DATE) && (flags & ISO_TIME)) {
		if (bufsz < 1)
			goto err;
		*p++ = (flags & ISO_T) ? 'T' : ' ';
		bufsz--;
	}

	if (flags & ISO_TIME) {
		len = snprintf(p, bufsz, "%02d:%02d:%02d", tm->tm_hour,
			       tm->tm_min, tm->tm_sec);
		if (len < 0 || (size_t) len > bufsz)
			goto err;
		bufsz -= len;
		p += len;
	}

	if (flags & ISO_DOTNSEC) {
		len = snprintf(p, bufsz, ".%09"PRIu32, nsec);
		if (len < 0 || (size_t) len > bufsz)
			goto err;
		bufsz -= len;
		p += len;

	} else if (flags & ISO_COMMANSEC) {
		len = snprintf(p, bufsz, ",%09"PRIu32, nsec);
		if (len < 0 || (size_t) len > bufsz)
			goto err;
		bufsz -= len;
		p += len;
	} else if (flags & ISO_DOTUSEC) {
		len = snprintf(p, bufsz, ".%06"PRIu32, usec);
		if (len < 0 || (size_t) len > bufsz)
			goto err;
		bufsz -= len;
		p += len;

	} else if (flags & ISO_COMMAUSEC) {
		len = snprintf(p, bufsz, ",%06"PRIu32, usec);
		if (len < 0 || (size_t) len > bufsz)
			goto err;
		bufsz -= len;
		p += len;
	}

	if (flags & ISO_TIMEZONE) {
		int tmin  = get_gmtoff(tm) / 60;
		int zhour = tmin / 60;
		int zmin  = abs(tmin % 60);
		len = snprintf(p, bufsz, "%+03d:%02d", zhour,zmin);
		if (len < 0 || (size_t) len > bufsz)
			goto err;
	}
	return 0;
 err:
	warnx(_("format_iso_time: buffer overflow."));
	return -1;
}

/* timespec to ISO 8601 */
int strtimespec_iso(const struct timespec *ts, int flags, char *buf, size_t bufsz)
{
	struct tm tm;
	struct tm *rc;

	if (flags & ISO_GMTIME)
		rc = gmtime_r(&ts->tv_sec, &tm);
	else
		rc = localtime_r(&ts->tv_sec, &tm);

	if (rc)
		return format_iso_time(&tm, ts->tv_nsec, flags, buf, bufsz);

	warnx(_("time %"PRId64" is out of range."), (int64_t)(ts->tv_sec));
	return -1;
}

/* timeval to ISO 8601 */
int strtimeval_iso(const struct timeval *tv, int flags, char *buf, size_t bufsz)
{
	struct timespec ts = {
		.tv_sec = tv->tv_sec,
		.tv_nsec = tv->tv_usec * NSEC_PER_USEC,
	};

	return strtimespec_iso(&ts, flags, buf, bufsz);
}

/* struct tm to ISO 8601 */
int strtm_iso(const struct tm *tm, int flags, char *buf, size_t bufsz)
{
	return format_iso_time(tm, 0, flags, buf, bufsz);
}

/* time_t to ISO 8601 */
int strtime_iso(const time_t *t, int flags, char *buf, size_t bufsz)
{
	struct tm tm;
	struct tm *rc;

	if (flags & ISO_GMTIME)
		rc = gmtime_r(t, &tm);
	else
		rc = localtime_r(t, &tm);

	if (rc)
		return format_iso_time(&tm, 0, flags, buf, bufsz);

	warnx(_("time %"PRId64" is out of range."), (int64_t)*t);
	return -1;
}

/* relative time functions */
static inline int time_is_thisyear(struct tm const *const tm,
				   struct tm const *const tmnow)
{
	return tm->tm_year == tmnow->tm_year;
}

static inline int time_is_today(struct tm const *const tm,
				struct tm const *const tmnow)
{
	return (tm->tm_yday == tmnow->tm_yday &&
		time_is_thisyear(tm, tmnow));
}

int strtime_short(const time_t *t, struct timeval *now, int flags, char *buf, size_t bufsz)
{
	struct tm tm, tmnow;
	int rc = 0;

	if (now->tv_sec == 0)
		gettimeofday(now, NULL);

	localtime_r(t, &tm);
	localtime_r(&now->tv_sec, &tmnow);

	if (time_is_today(&tm, &tmnow)) {
		rc = snprintf(buf, bufsz, "%02d:%02d", tm.tm_hour, tm.tm_min);
		if (rc < 0 || (size_t) rc > bufsz)
			return -1;
		rc = 1;

	} else if (time_is_thisyear(&tm, &tmnow)) {
		if (flags & UL_SHORTTIME_THISYEAR_HHMM)
			rc = strftime(buf, bufsz, "%b%d/%H:%M", &tm);
		else
			rc = strftime(buf, bufsz, "%b%d", &tm);
	} else
		rc = strftime(buf, bufsz, "%Y-%b%d", &tm);

	return rc <= 0 ? -1 : 0;
}

int strtimespec_relative(const struct timespec *ts, char *buf, size_t bufsz)
{
	time_t secs = ts->tv_sec;
	size_t i, parts = 0;
	int rc;

	if (bufsz)
		buf[0] = '\0';

	static const struct {
		const char * const suffix;
		int width;
		int64_t secs;
	} table[] = {
		{ "y", 4, NSEC_PER_YEAR   / NSEC_PER_SEC },
		{ "d", 3, NSEC_PER_DAY    / NSEC_PER_SEC },
		{ "h", 2, NSEC_PER_HOUR   / NSEC_PER_SEC },
		{ "m", 2, NSEC_PER_MINUTE / NSEC_PER_SEC },
		{ "s", 2, NSEC_PER_SEC    / NSEC_PER_SEC },
	};

	for (i = 0; i < ARRAY_SIZE(table); i++) {
		if (secs >= table[i].secs) {
			rc = snprintf(buf, bufsz,
				      "%*"PRId64"%s%s",
				      parts ? table[i].width : 0,
				      secs / table[i].secs, table[i].suffix,
				      secs % table[i].secs ? " " : "");
			if (rc < 0 || (size_t) rc > bufsz)
				goto err;
			parts++;
			buf += rc;
			bufsz -= rc;
			secs %= table[i].secs;
		}
	}

	if (ts->tv_nsec) {
		if (ts->tv_nsec % NSEC_PER_MSEC) {
			rc = snprintf(buf, bufsz, "%*luns",
				      parts ? 10 : 0, ts->tv_nsec);
			if (rc < 0 || (size_t) rc > bufsz)
				goto err;
		} else {
			rc = snprintf(buf, bufsz, "%*llums",
				      parts ? 4 : 0, ts->tv_nsec / NSEC_PER_MSEC);
			if (rc < 0 || (size_t) rc > bufsz)
				goto err;
		}
	}

	return 0;
 err:
	warnx(_("format_reltime: buffer overflow."));
	return -1;
}

#ifndef HAVE_TIMEGM
time_t timegm(struct tm *tm)
{
	const char *zone = getenv("TZ");
	time_t ret;

	setenv("TZ", "", 1);
	tzset();
	ret = mktime(tm);
	if (zone)
		setenv("TZ", zone, 1);
	else
		unsetenv("TZ");
	tzset();
	return ret;
}
#endif /* HAVE_TIMEGM */

#ifdef TEST_PROGRAM_TIMEUTILS

static int run_unittest_timestamp(void)
{
	int rc = EXIT_SUCCESS;
	time_t reference = 1674180427;
	static const struct testcase {
		const char * const input;
		usec_t expected;
	} testcases[] = {
		{ "2012-09-22 16:34:22"    , 1348331662000000 },
		{ "2012-09-22 16:34:22,012", 1348331662012000 },
		{ "2012-09-22 16:34:22.012", 1348331662012000 },
		{ "@1348331662"            , 1348331662000000 },
		{ "@1348331662.234567"     , 1348331662234567 },
		{ "@0"                     ,                0 },
		{ "2012-09-22 16:34"       , 1348331640000000 },
		{ "2012-09-22"             , 1348272000000000 },
		{ "16:34:22"               , 1674232462000000 },
		{ "16:34:22,123456"        , 1674232462123456 },
		{ "16:34:22.123456"        , 1674232462123456 },
		{ "16:34"                  , 1674232440000000 },
		{ "now"                    , 1674180427000000 },
		{ "yesterday"              , 1674086400000000 },
		{ "today"                  , 1674172800000000 },
		{ "tomorrow"               , 1674259200000000 },
		{ "+5min"                  , 1674180727000000 },
		{ "-5days"                 , 1673748427000000 },
		{ "20120922163422"         , 1348331662000000 },
	};

	setenv("TZ", "GMT", 1);
	tzset();

	for (size_t i = 0; i < ARRAY_SIZE(testcases); i++) {
		struct testcase t = testcases[i];
		usec_t result;
		int r = parse_timestamp_reference(reference, t.input, &result);
		if (r) {
			fprintf(stderr, "Could not parse '%s'\n", t.input);
			rc = EXIT_FAILURE;
		}

		if (result != t.expected) {
			fprintf(stderr, "#%02zu %-25s: %"PRId64" != %"PRId64"\n",
				i, t.input, result, t.expected);
			rc = EXIT_FAILURE;
		}
	}

	return rc;
}

static int run_unittest_format(void)
{
	int rc = EXIT_SUCCESS;
	const struct timespec ts = {
		.tv_sec = 1674180427,
		.tv_nsec = 12345,
	};
	char buf[FORMAT_TIMESTAMP_MAX];
	static const struct testcase {
		int flags;
		const char * const expected;
	} testcases[] = {
		{ ISO_DATE,               "2023-01-20"                       },
		{ ISO_TIME,               "02:07:07"                         },
		{ ISO_TIMEZONE,           "+00:00"                           },
		{ ISO_TIMESTAMP_T,        "2023-01-20T02:07:07+00:00"        },
		{ ISO_TIMESTAMP_COMMA_G,  "2023-01-20 02:07:07,000012+00:00" },
		{ ISO_TIME | ISO_DOTNSEC, "02:07:07.000012345" },
	};

	setenv("TZ", "GMT", 1);
	tzset();

	for (size_t i = 0; i < ARRAY_SIZE(testcases); i++) {
		struct testcase t = testcases[i];
		int r = strtimespec_iso(&ts, t.flags, buf, sizeof(buf));
		if (r) {
			fprintf(stderr, "Could not format '%s'\n", t.expected);
			rc = EXIT_FAILURE;
		}

		if (strcmp(buf, t.expected)) {
			fprintf(stderr, "#%02zu %-20s != %-20s\n", i, buf, t.expected);
			rc = EXIT_FAILURE;
		}
	}

	return rc;
}

static int run_unittest_format_relative(void)
{
	int rc = EXIT_SUCCESS;
	char buf[FORMAT_TIMESTAMP_MAX];
	static const struct testcase {
		struct timespec ts;
		const char * const expected;
	} testcases[] = {
		{{}, "" },
		{{         1 },                  "1s" },
		{{        10 },                 "10s" },
		{{       100 },              "1m 40s" },
		{{      1000 },             "16m 40s" },
		{{     10000 },          "2h 46m 40s" },
		{{    100000 },      "1d  3h 46m 40s" },
		{{   1000000 },     "11d 13h 46m 40s" },
		{{  10000000 },    "115d 17h 46m 40s" },
		{{ 100000000 }, "3y  61d 15h 46m 40s" },
		{{        60 },                  "1m" },
		{{      3600 },                  "1h" },

		{{ 1,       1 }, "1s         1ns" },
		{{ 0,       1 },            "1ns" },
		{{ 0, 1000000 },            "1ms" },
		{{ 0, 1000001 },      "1000001ns" },
	};

	setenv("TZ", "GMT", 1);
	tzset();

	for (size_t i = 0; i < ARRAY_SIZE(testcases); i++) {
		struct testcase t = testcases[i];
		int r = strtimespec_relative(&t.ts, buf, sizeof(buf));
		if (r) {
			fprintf(stderr, "Could not format '%s'\n", t.expected);
			rc = EXIT_FAILURE;
		}

		if (strcmp(buf, t.expected)) {
			fprintf(stderr, "#%02zu '%-20s' != '%-20s'\n", i, buf, t.expected);
			rc = EXIT_FAILURE;
		}
	}

	return rc;
}

int main(int argc, char *argv[])
{
	struct timespec ts = { 0 };
	char buf[ISO_BUFSIZ];
	int r;

	if (argc < 2) {
		fprintf(stderr, "usage: %s [<time> [<usec>]] | [--timestamp <str>] | [--unittest-timestamp]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (strcmp(argv[1], "--unittest-timestamp") == 0)
		return run_unittest_timestamp();
	else if (strcmp(argv[1], "--unittest-format") == 0)
		return run_unittest_format();
	else if (strcmp(argv[1], "--unittest-format-relative") == 0)
		return run_unittest_format_relative();

	if (strcmp(argv[1], "--timestamp") == 0) {
		usec_t usec = 0;

		r = parse_timestamp(argv[2], &usec);
		if (r)
			errx(EXIT_FAILURE, "Can not parse '%s': %s", argv[2], strerror(-r));
		ts.tv_sec = (time_t) (usec / USEC_PER_SEC);
		ts.tv_nsec = (usec % USEC_PER_SEC) * NSEC_PER_USEC;
	} else {
		ts.tv_sec = strtos64_or_err(argv[1], "failed to parse <time>");
		if (argc == 3)
			ts.tv_nsec = strtos64_or_err(argv[2], "failed to parse <usec>")
				     * NSEC_PER_USEC;
	}

	strtimespec_iso(&ts, ISO_DATE, buf, sizeof(buf));
	printf("Date: '%s'\n", buf);

	strtimespec_iso(&ts, ISO_TIME, buf, sizeof(buf));
	printf("Time: '%s'\n", buf);

	strtimespec_iso(&ts, ISO_DATE | ISO_TIME | ISO_COMMAUSEC | ISO_T,
		       buf, sizeof(buf));
	printf("Full: '%s'\n", buf);

	strtimespec_iso(&ts, ISO_TIMESTAMP_DOT, buf, sizeof(buf));
	printf("Zone: '%s'\n", buf);

	strtimespec_relative(&ts, buf, sizeof(buf));
	printf("Rel:  '%s'\n", buf);

	return EXIT_SUCCESS;
}

#endif /* TEST_PROGRAM_TIMEUTILS */
