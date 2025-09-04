/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 2018 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config FLOAT_DURATION
//config:	bool "Enable fractional duration arguments"
//config:	default y
//config:	help
//config:	Allow sleep N.NNN, top -d N.NNN etc.

//kbuild:lib-$(CONFIG_SLEEP)   += duration.o
//kbuild:lib-$(CONFIG_TOP)     += duration.o
//kbuild:lib-$(CONFIG_TIMEOUT) += duration.o
//kbuild:lib-$(CONFIG_PING)    += duration.o
//kbuild:lib-$(CONFIG_PING6)   += duration.o
//kbuild:lib-$(CONFIG_WATCH)   += duration.o

#include "libbb.h"

static const struct suffix_mult duration_suffixes[] ALIGN_SUFFIX = {
	{ "s", 1 },
	{ "m", 60 },
	{ "h", 60*60 },
	{ "d", 24*60*60 },
	{ "", 0 }
};

#if ENABLE_FLOAT_DURATION
duration_t FAST_FUNC parse_duration_str(char *str)
{
	duration_t duration;

	if (strchr(str, '.')) {
		double d;
		char *pp;
		int len;
		char sv;

# if ENABLE_LOCALE_SUPPORT
		/* Undo busybox.c: on input, we want to use dot
		 * as fractional separator in strtod(),
		 * regardless of current locale
		 */
		setlocale(LC_NUMERIC, "C");
# endif
		len = strspn(str, "0123456789.");
		sv = str[len];
		str[len] = '\0';
		errno = 0;
		d = strtod(str, &pp);
		if (errno || *pp)
			bb_show_usage();
		str += len;
		*str-- = sv;
		sv = *str;
		*str = '1';
		duration = d * xatoul_sfx(str, duration_suffixes);
		*str = sv;
	} else {
		duration = xatoul_sfx(str, duration_suffixes);
	}

	return duration;
}
void FAST_FUNC sleep_for_duration(duration_t duration)
{
	struct timespec ts;

	ts.tv_sec = MAXINT(typeof(ts.tv_sec));
	ts.tv_nsec = 0;
	if (duration >= 0 && duration < ts.tv_sec) {
		ts.tv_sec = duration;
		ts.tv_nsec = (duration - ts.tv_sec) * 1000000000;
	}
	/* NB: ENABLE_ASH_SLEEP requires that we do NOT loop on EINTR here:
	 * otherwise, traps won't execute until we finish looping.
	 */
	//do {
	//	errno = 0;
	//	nanosleep(&ts, &ts);
	//} while (errno == EINTR);
	nanosleep(&ts, &ts);
}
#else
duration_t FAST_FUNC parse_duration_str(char *str)
{
	return xatou_range_sfx(str, 0, UINT_MAX, duration_suffixes);
}
#endif
