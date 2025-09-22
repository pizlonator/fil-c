/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *
 * Since 7a3000f7ba548cf7d74ac77cc63fe8de228a669e (v2.30) hwclock is linked
 * with parse_date.y from gnullib. This gnulib code is distributed with GPLv3.
 * Use --disable-hwclock-gplv3 to exclude this code.
 *
 * Copyright (C) 1992 Charles Hedrick, hedrick@cs.rutgers.edu
 *                    Rob Hooft <hooft@chem.ruu.nl>
 *                    Harald Koenig <koenig@nova.tat.physik.uni-tuebingen.de>
 *                    Alan Modra <alan@spri.levels.unisa.edu.au>
 *
 * Copyright (C) 2007-2023 Karel Zak <kzak@redhat.com>
 *
 * Major rewrite by Bryan Henderson <bryanh@giraffe-data.com>, 96.09.19.
 * The new program is called hwclock. New features:
 *
 *	- You can set the hardware clock without also modifying the system
 *	  clock.
 *	- You can read and set the clock with finer than 1 second precision.
 *	- When you set the clock, hwclock automatically refigures the drift
 *	  rate, based on how far off the clock was before you set it.
 *
 * Reshuffled things, added sparc code, and re-added alpha stuff
 * by David Mosberger <davidm@azstarnet.com>
 * and Jay Estabrook <jestabro@amt.tay1.dec.com>
 * and Martin Ostermann <ost@comnets.rwth-aachen.de>, aeb@cwi.nl, 990212.
 *
 * Fix for Award 2094 bug, Dave Coffin (dcoffin@shore.net) 11/12/98
 * Change of local time handling, Stefan Ring <e9725446@stud3.tuwien.ac.at>
 * Change of adjtime handling, James P. Rutledge <ao112@rgfn.epcc.edu>.
 *
 *
 */
/*
 * Explanation of `adjusting' (Rob Hooft):
 *
 * The problem with my machine is that its CMOS clock is 10 seconds
 * per day slow. With this version of clock.c, and my '/etc/rc.local'
 * reading '/etc/clock -au' instead of '/etc/clock -u -s', this error
 * is automatically corrected at every boot.
 *
 * To do this job, the program reads and writes the file '/etc/adjtime'
 * to determine the correction, and to save its data. In this file are
 * three numbers:
 *
 *	1) the correction in seconds per day. (So if your clock runs 5
 *	   seconds per day fast, the first number should read -5.0)
 *	2) the number of seconds since 1/1/1970 the last time the program
 *	   was used
 *	3) the remaining part of a second which was leftover after the last
 *	   adjustment
 *
 * Installation and use of this program:
 *
 *	a) create a file '/etc/adjtime' containing as the first and only
 *	   line: '0.0 0 0.0'
 *	b) run 'clock -au' or 'clock -a', depending on whether your cmos is
 *	   in universal or local time. This updates the second number.
 *	c) set your system time using the 'date' command.
 *	d) update your cmos time using 'clock -wu' or 'clock -w'
 *	e) replace the first number in /etc/adjtime by your correction.
 *	f) put the command 'clock -au' or 'clock -a' in your '/etc/rc.local'
 */

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#ifdef HAVE_SYS_SYSCALL_H
#include <sys/syscall.h>
#endif
#include <time.h>
#include <unistd.h>
#include <inttypes.h>

#include "c.h"
#include "closestream.h"
#include "nls.h"
#include "optutils.h"
#include "pathnames.h"
#include "hwclock.h"
#include "timeutils.h"
#include "env.h"
#include "xalloc.h"
#include "path.h"
#include "strutils.h"

#ifdef HAVE_LIBAUDIT
#include <libaudit.h>
static int hwaudit_fd = -1;
#endif

UL_DEBUG_DEFINE_MASK(hwclock);
UL_DEBUG_DEFINE_MASKNAMES(hwclock) = UL_DEBUG_EMPTY_MASKNAMES;

/* The struct that holds our hardware access routines */
static const struct clock_ops *ur;

/* Maximal clock adjustment in seconds per day.
   (adjtime() glibc call has 2145 seconds limit on i386, so it is good enough for us as well,
   43219 is a maximal safe value preventing exact_adjustment overflow.) */
#define MAX_DRIFT 2145.0

struct adjtime {
	/*
	 * This is information we keep in the adjtime file that tells us how
	 * to do drift corrections. Elements are all straight from the
	 * adjtime file, so see documentation of that file for details.
	 * Exception is <dirty>, which is an indication that what's in this
	 * structure is not what's in the disk file (because it has been
	 * updated since read from the disk file).
	 */
	int dirty;
	/* line 1 */
	double drift_factor;
	time_t last_adj_time;
	double not_adjusted;
	/* line 2 */
	time_t last_calib_time;
	/*
	 * The most recent time that we set the clock from an external
	 * authority (as opposed to just doing a drift adjustment)
	 */
	/* line 3 */
	enum a_local_utc { UTC = 0, LOCAL, UNKNOWN } local_utc;
	/*
	 * To which time zone, local or UTC, we most recently set the
	 * hardware clock.
	 */
};

static void hwclock_init_debug(const char *str)
{
	__UL_INIT_DEBUG_FROM_STRING(hwclock, HWCLOCK_DEBUG_, 0, str);

	DBG(INIT, ul_debug("hwclock debug mask: 0x%04x", hwclock_debug_mask));
	DBG(INIT, ul_debug("hwclock version: %s", PACKAGE_STRING));
}

/* FOR TESTING ONLY: inject random delays of up to 1000ms */
static void up_to_1000ms_sleep(void)
{
	int usec = random() % 1000000;

	DBG(RANDOM_SLEEP, ul_debug("sleeping ~%d usec", usec));
	xusleep(usec);
}

/*
 * time_t to timeval conversion.
 */
static struct timeval t2tv(time_t timet)
{
	struct timeval rettimeval;

	rettimeval.tv_sec = timet;
	rettimeval.tv_usec = 0;
	return rettimeval;
}

/*
 * The difference in seconds between two times in "timeval" format.
 */
double time_diff(struct timeval subtrahend, struct timeval subtractor)
{
	return (subtrahend.tv_sec - subtractor.tv_sec)
	    + (subtrahend.tv_usec - subtractor.tv_usec) / 1E6;
}

/*
 * The time, in "timeval" format, which is <increment> seconds after the
 * time <addend>. Of course, <increment> may be negative.
 */
static struct timeval time_inc(struct timeval addend, double increment)
{
	struct timeval newtime;

	newtime.tv_sec = addend.tv_sec + (time_t)increment;
	newtime.tv_usec = addend.tv_usec + (increment - (time_t)increment) * 1E6;

	/*
	 * Now adjust it so that the microsecond value is between 0 and 1
	 * million.
	 */
	if (newtime.tv_usec < 0) {
		newtime.tv_usec += 1E6;
		newtime.tv_sec -= 1;
	} else if (newtime.tv_usec >= 1E6) {
		newtime.tv_usec -= 1E6;
		newtime.tv_sec += 1;
	}
	return newtime;
}

static int
hw_clock_is_utc(const struct hwclock_control *ctl,
		const struct adjtime *adjtime)
{
	int ret;

	if (ctl->utc)
		ret = 1;	/* --utc explicitly given on command line */
	else if (ctl->local_opt)
		ret = 0;	/* --localtime explicitly given */
	else
		/* get info from adjtime file - default is UTC */
		ret = (adjtime->local_utc != LOCAL);

	if (ctl->verbose)
		printf(_("Assuming hardware clock is kept in %s time.\n"),
		       ret ? _("UTC") : _("local"));
	return ret;
}

/*
 * Read the adjustment parameters out of the /etc/adjtime file.
 *
 * Return them as the adjtime structure <*adjtime_p>. Its defaults are
 * initialized in main().
 */
static int read_adjtime(const struct hwclock_control *ctl,
			struct adjtime *adjtime_p)
{
	FILE *adjfile;
	char line1[81];		/* String: first line of adjtime file */
	char line2[81];		/* String: second line of adjtime file */
	char line3[81];		/* String: third line of adjtime file */
	int64_t last_adj_time;
	int64_t last_calib_time;

	if (access(ctl->adj_file_name, R_OK) != 0)
		return EXIT_SUCCESS;

	adjfile = fopen(ctl->adj_file_name, "r");	/* open file for reading */
	if (adjfile == NULL) {
		warn(_("cannot open %s"), ctl->adj_file_name);
		return EXIT_FAILURE;
	}

	if (!fgets(line1, sizeof(line1), adjfile))
		line1[0] = '\0';	/* In case fgets fails */
	if (!fgets(line2, sizeof(line2), adjfile))
		line2[0] = '\0';	/* In case fgets fails */
	if (!fgets(line3, sizeof(line3), adjfile))
		line3[0] = '\0';	/* In case fgets fails */

	fclose(adjfile);

	if (sscanf(line1, "%lf %"SCNd64" %lf",
			&adjtime_p->drift_factor,
			&last_adj_time,
			&adjtime_p->not_adjusted) != 3)
		warnx(_("Warning: unrecognized line in adjtime file: %s"), line1);

	if (sscanf(line2, "%"SCNd64, &last_calib_time) != 1)
		warnx(_("Warning: unrecognized line in adjtime file: %s"), line2);

	adjtime_p->last_adj_time = (time_t)last_adj_time;
	adjtime_p->last_calib_time = (time_t)last_calib_time;

	if (!strcmp(line3, "UTC\n")) {
		adjtime_p->local_utc = UTC;
	} else if (!strcmp(line3, "LOCAL\n")) {
		adjtime_p->local_utc = LOCAL;
	} else {
		adjtime_p->local_utc = UNKNOWN;
		if (line3[0]) {
			warnx(_("Warning: unrecognized third line in adjtime file\n"
				"(Expected: `UTC' or `LOCAL' or nothing.)"));
		}
	}

	if (ctl->verbose) {
		printf(_("Last drift adjustment done at %"PRId64" seconds after 1969\n"),
		       (int64_t)adjtime_p->last_adj_time);
		printf(_("Last calibration done at %"PRId64" seconds after 1969\n"),
		       (int64_t)adjtime_p->last_calib_time);
		printf(_("Hardware clock is on %s time\n"),
		       (adjtime_p->local_utc ==
			LOCAL) ? _("local") : (adjtime_p->local_utc ==
					       UTC) ? _("UTC") : _("unknown"));
	}

	return EXIT_SUCCESS;
}

/*
 * Wait until the falling edge of the Hardware Clock's update flag so that
 * any time that is read from the clock immediately after we return will be
 * exact.
 *
 * The clock only has 1 second precision, so it gives the exact time only
 * once per second, right on the falling edge of the update flag.
 *
 * We wait (up to one second) either blocked waiting for an rtc device or in
 * a CPU spin loop. The former is probably not very accurate.
 *
 * Return 0 if it worked, nonzero if it didn't.
 */
static int synchronize_to_clock_tick(const struct hwclock_control *ctl)
{
	int rc;

	if (ctl->verbose)
		printf(_("Waiting for clock tick...\n"));

	rc = ur->synchronize_to_clock_tick(ctl);

	if (ctl->verbose) {
		if (rc)
			printf(_("...synchronization failed\n"));
		else
			printf(_("...got clock tick\n"));
	}

	return rc;
}

/*
 * Convert a time in broken down format (hours, minutes, etc.) into standard
 * unix time (seconds into epoch). Return it as *systime_p.
 *
 * The broken down time is argument <tm>. This broken down time is either
 * in local time zone or UTC, depending on value of logical argument
 * "universal". True means it is in UTC.
 *
 * If the argument contains values that do not constitute a valid time, and
 * mktime() recognizes this, return *valid_p == false and *systime_p
 * undefined. However, mktime() sometimes goes ahead and computes a
 * fictional time "as if" the input values were valid, e.g. if they indicate
 * the 31st day of April, mktime() may compute the time of May 1. In such a
 * case, we return the same fictional value mktime() does as *systime_p and
 * return *valid_p == true.
 */
static int
mktime_tz(const struct hwclock_control *ctl, struct tm tm,
	  time_t *systime_p)
{
	int valid;

	if (ctl->universal)
		*systime_p = timegm(&tm);
	else
		*systime_p = mktime(&tm);
	if (*systime_p == -1) {
		/*
		 * This apparently (not specified in mktime() documentation)
		 * means the 'tm' structure does not contain valid values
		 * (however, not containing valid values does _not_ imply
		 * mktime() returns -1).
		 */
		valid = 0;
		if (ctl->verbose)
			printf(_("Invalid values in hardware clock: "
				 "%4d/%.2d/%.2d %.2d:%.2d:%.2d\n"),
			       tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			       tm.tm_hour, tm.tm_min, tm.tm_sec);
	} else {
		valid = 1;
		if (ctl->verbose)
			printf(_("Hw clock time : %4d/%.2d/%.2d %.2d:%.2d:%.2d = "
				 "%"PRId64" seconds since 1969\n"), tm.tm_year + 1900,
			       tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
			       tm.tm_sec, (int64_t)*systime_p);
	}
	return valid;
}

/*
 * Read the hardware clock and return the current time via <tm> argument.
 *
 * Use the method indicated by <method> argument to access the hardware
 * clock.
 */
static int
read_hardware_clock(const struct hwclock_control *ctl,
		    int *valid_p, time_t *systime_p)
{
	struct tm tm = { 0 };
	int err;

	err = ur->read_hardware_clock(ctl, &tm);
	if (err)
		return err;

	if (ctl->verbose)
		printf(_("Time read from Hardware Clock: %4d/%.2d/%.2d %02d:%02d:%02d\n"),
		       tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
		       tm.tm_min, tm.tm_sec);
	*valid_p = mktime_tz(ctl, tm, systime_p);

	return 0;
}

/*
 * Set the Hardware Clock to the time <newtime>, in local time zone or UTC,
 * according to <universal>.
 */
static void
set_hardware_clock(const struct hwclock_control *ctl, const time_t newtime)
{
	struct tm new_broken_time = { 0 };
	/*
	 * Time to which we will set Hardware Clock, in broken down format,
	 * in the time zone of caller's choice
	 */

	if (ctl->universal)
		gmtime_r(&newtime, &new_broken_time);
	else
		localtime_r(&newtime, &new_broken_time);

	if (ctl->verbose)
		printf(_("Setting Hardware Clock to %.2d:%.2d:%.2d "
			 "= %"PRId64" seconds since 1969\n"),
		       new_broken_time.tm_hour, new_broken_time.tm_min,
		       new_broken_time.tm_sec, (int64_t)newtime);

	if (!ctl->testing)
		ur->set_hardware_clock(ctl, &new_broken_time);
}

static double
get_hardware_delay(const struct hwclock_control *ctl)
{
	const char *devpath, *rtcname;
	char name[128 + 1];
	struct path_cxt *pc;
	int rc;

	devpath = ur->get_device_path();
	if (!devpath)
		goto unknown;

	rtcname = strrchr(devpath, '/');
	if (!rtcname || !*(rtcname + 1))
		goto unknown;
	rtcname++;

	pc = ul_new_path("/sys/class/rtc/%s", rtcname);
	if (!pc)
		goto unknown;
	rc = ul_path_scanf(pc, "name", "%128[^\n ]", name);
	ul_unref_path(pc);

	if (rc != 1 || !*name)
		goto unknown;

	if (ctl->verbose)
		printf(_("RTC type: '%s'\n"), name);

	/* MC146818A-compatible (x86) */
	if (strcmp(name, "rtc_cmos") == 0)
		return 0.5;

	/* Another HW */
	return 0;
unknown:
	/* Let's be backwardly compatible */
	return 0.5;
}


/*
 * Set the Hardware Clock to the time "sethwtime", in local time zone or
 * UTC, according to "universal".
 *
 * Wait for a fraction of a second so that "sethwtime" is the value of the
 * Hardware Clock as of system time "refsystime", which is in the past. For
 * example, if "sethwtime" is 14:03:05 and "refsystime" is 12:10:04.5 and
 * the current system time is 12:10:06.0: Wait .5 seconds (to make exactly 2
 * seconds since "refsystime") and then set the Hardware Clock to 14:03:07,
 * thus getting a precise and retroactive setting of the clock. The .5 delay is
 * default on x86, see --delay and get_hardware_delay().
 *
 * (Don't be confused by the fact that the system clock and the Hardware
 * Clock differ by two hours in the above example. That's just to remind you
 * that there are two independent time scales here).
 *
 * This function ought to be able to accept set times as fractional times.
 * Idea for future enhancement.
 */
static void
set_hardware_clock_exact(const struct hwclock_control *ctl,
			 const time_t sethwtime,
			 const struct timeval refsystime)
{
	/*
	 * The Hardware Clock can only be set to any integer time plus one
	 * half second.	 The integer time is required because there is no
	 * interface to set or get a fractional second.	 The additional half
	 * second is because the Hardware Clock updates to the following
	 * second precisely 500 ms (not 1 second!) after you release the
	 * divider reset (after setting the new time) - see description of
	 * DV2, DV1, DV0 in Register A in the MC146818A data sheet (and note
	 * that although that document doesn't say so, real-world code seems
	 * to expect that the SET bit in Register B functions the same way).
	 * That means that, e.g., when you set the clock to 1:02:03, it
	 * effectively really sets it to 1:02:03.5, because it will update to
	 * 1:02:04 only half a second later.  Our caller passes the desired
	 * integer Hardware Clock time in sethwtime, and the corresponding
	 * system time (which may have a fractional part, and which may or may
	 * not be the same!) in refsystime.  In an ideal situation, we would
	 * then apply sethwtime to the Hardware Clock at refsystime+500ms, so
	 * that when the Hardware Clock ticks forward to sethwtime+1s half a
	 * second later at refsystime+1000ms, everything is in sync.  So we
	 * spin, waiting for gettimeofday() to return a time at or after that
	 * time (refsystime+500ms) up to a tolerance value, initially 1ms.  If
	 * we miss that time due to being preempted for some other process,
	 * then we increase the margin a little bit (initially 1ms, doubling
	 * each time), add 1 second (or more, if needed to get a time that is
	 * in the future) to both the time for which we are waiting and the
	 * time that we will apply to the Hardware Clock, and start waiting
	 * again.
	 *
	 * For example, the caller requests that we set the Hardware Clock to
	 * 1:02:03, with reference time (current system time) = 6:07:08.250.
	 * We want the Hardware Clock to update to 1:02:04 at 6:07:09.250 on
	 * the system clock, and the first such update will occur 0.500
	 * seconds after we write to the Hardware Clock, so we spin until the
	 * system clock reads 6:07:08.750.  If we get there, great, but let's
	 * imagine the system is so heavily loaded that our process is
	 * preempted and by the time we get to run again, the system clock
	 * reads 6:07:11.990.  We now want to wait until the next xx:xx:xx.750
	 * time, which is 6:07:12.750 (4.5 seconds after the reference time),
	 * at which point we will set the Hardware Clock to 1:02:07 (4 seconds
	 * after the originally requested time).  If we do that successfully,
	 * then at 6:07:13.250 (5 seconds after the reference time), the
	 * Hardware Clock will update to 1:02:08 (5 seconds after the
	 * originally requested time), and all is well thereafter.
	 */

	time_t newhwtime = sethwtime;
	double target_time_tolerance_secs = 0.001;  /* initial value */
	double tolerance_incr_secs = 0.001;	    /* initial value */
	double delay;
	struct timeval rtc_set_delay_tv;

	struct timeval targetsystime;
	struct timeval nowsystime;
	struct timeval prevsystime = refsystime;
	double deltavstarget;

	if (ctl->rtc_delay != -1.0)        /* --delay specified */
		delay = ctl->rtc_delay;
	else
		delay = get_hardware_delay(ctl);

	if (ctl->verbose)
		printf(_("Using delay: %.6f seconds\n"), delay);

	rtc_set_delay_tv.tv_sec = 0;
	rtc_set_delay_tv.tv_usec = delay * 1E6;

	timeradd(&refsystime, &rtc_set_delay_tv, &targetsystime);

	while (1) {
		double ticksize;

		ON_DBG(RANDOM_SLEEP, up_to_1000ms_sleep());

		gettimeofday(&nowsystime, NULL);
		deltavstarget = time_diff(nowsystime, targetsystime);
		ticksize = time_diff(nowsystime, prevsystime);
		prevsystime = nowsystime;

		if (ticksize < 0) {
			if (ctl->verbose)
				printf(_("time jumped backward %.6f seconds "
					 "to %"PRId64".%06"PRId64" - retargeting\n"),
				       ticksize, (int64_t)nowsystime.tv_sec,
				       (int64_t)nowsystime.tv_usec);
			/* The retarget is handled at the end of the loop. */
		} else if (deltavstarget < 0) {
			/* deltavstarget < 0 if current time < target time */
			DBG(DELTA_VS_TARGET,
			    ul_debug("%"PRId64".%06"PRId64" < %"PRId64".%06"PRId64" (%.6f)",
				     (int64_t)nowsystime.tv_sec, (int64_t)nowsystime.tv_usec,
				     (int64_t)targetsystime.tv_sec,
				     (int64_t)targetsystime.tv_usec, deltavstarget));
			continue;  /* not there yet - keep spinning */
		} else if (deltavstarget <= target_time_tolerance_secs) {
			/* Close enough to the target time; done waiting. */
			break;
		} else /* (deltavstarget > target_time_tolerance_secs) */ {
			/*
			 * We missed our window.  Increase the tolerance and
			 * aim for the next opportunity.
			 */
			if (ctl->verbose)
				printf(_("missed it - %"PRId64".%06"PRId64" is too far "
					 "past %"PRId64".%06"PRId64" (%.6f > %.6f)\n"),
				       (int64_t)nowsystime.tv_sec,
				       (int64_t)nowsystime.tv_usec,
				       (int64_t)targetsystime.tv_sec,
				       (int64_t)targetsystime.tv_usec,
				       deltavstarget,
				       target_time_tolerance_secs);
			target_time_tolerance_secs += tolerance_incr_secs;
			tolerance_incr_secs *= 2;
		}

		/*
		 * Aim for the same offset (tv_usec) within the second in
		 * either the current second (if that offset hasn't arrived
		 * yet), or the next second.
		 */
		if (nowsystime.tv_usec < targetsystime.tv_usec)
			targetsystime.tv_sec = nowsystime.tv_sec;
		else
			targetsystime.tv_sec = nowsystime.tv_sec + 1;
	}

	newhwtime = sethwtime
		    + round(time_diff(nowsystime, refsystime)
			    - delay /* don't count this */);
	if (ctl->verbose)
		printf(_("%"PRId64".%06"PRId64" is close enough to %"PRId64".%06"PRId64" (%.6f < %.6f)\n"
			 "Set RTC to %"PRId64" (%"PRId64" + %d; refsystime = %"PRId64".%06"PRId64")\n"),
		       (int64_t)nowsystime.tv_sec, (int64_t)nowsystime.tv_usec,
		       (int64_t)targetsystime.tv_sec, (int64_t)targetsystime.tv_usec,
		       deltavstarget, target_time_tolerance_secs,
		       (int64_t)newhwtime, (int64_t)sethwtime,
		       (int)((int64_t)newhwtime - (int64_t)sethwtime),
		       (int64_t)refsystime.tv_sec, (int64_t)refsystime.tv_usec);

	set_hardware_clock(ctl, newhwtime);
}

static int
display_time(struct timeval hwctime)
{
	char buf[ISO_BUFSIZ];

	if (strtimeval_iso(&hwctime, ISO_TIMESTAMP_DOT, buf, sizeof(buf)))
		return EXIT_FAILURE;

	printf("%s\n", buf);
	return EXIT_SUCCESS;
}

/*
 * Adjusts System time, sets the kernel's timezone and RTC timescale.
 *
 * The kernel warp_clock function adjusts the System time according to the
 * tz.tz_minuteswest argument and sets PCIL (see below). At boot settimeofday(2)
 * has one-shot access to this function as shown in the table below.
 *
 * +-------------------------------------------------------------------------+
 * |                           settimeofday(tv, tz)                          |
 * |-------------------------------------------------------------------------|
 * |     Arguments     |  System Time  | TZ  | PCIL |           | warp_clock |
 * |   tv    |   tz    | set  | warped | set | set  | firsttime |   locked   |
 * |---------|---------|---------------|-----|------|-----------|------------|
 * | pointer | NULL    |  yes |   no   | no  |  no  |     1     |    no      |
 * | NULL    | ptr2utc |  no  |   no   | yes |  no  |     0     |    yes     |
 * | NULL    | pointer |  no  |   yes  | yes |  yes |     0     |    yes     |
 * +-------------------------------------------------------------------------+
 * ptr2utc: tz.tz_minuteswest is zero (UTC).
 * PCIL: persistent_clock_is_local, sets the "11 minute mode" timescale.
 * firsttime: locks the warp_clock function (initialized to 1 at boot).
 *
 * +---------------------------------------------------------------------------+
 * |  op     | RTC scale | settimeofday calls                                  |
 * |---------|-----------|-----------------------------------------------------|
 * | systz   |   Local   | 1) warps system time*, sets PCIL* and kernel tz     |
 * | systz   |   UTC     | 1st) locks warp_clock* 2nd) sets kernel tz          |
 * | hctosys |   Local   | 1st) sets PCIL* & kernel tz   2nd) sets system time |
 * | hctosys |   UTC     | 1st) locks warp* 2nd) sets tz 3rd) sets system time |
 * +---------------------------------------------------------------------------+
 *                         * only on first call after boot
 *
 * POSIX 2008 marked TZ in settimeofday() as deprecated. Unfortunately,
 * different C libraries react to this deprecation in a different way. Since
 * glibc v2.31 settimeofday() will fail if both args are not NULL, Musl-C
 * ignores TZ at all, etc. We use __set_time() and __set_timezone() to hide
 * these portability issues and to keep code readable.
 */
#define __set_time(_tv)		settimeofday(_tv, NULL)

#ifndef SYS_settimeofday
# ifdef __NR_settimeofday
#  define SYS_settimeofday	__NR_settimeofday
# elif defined(__NR_settimeofday_time32)
#  define SYS_settimeofday	__NR_settimeofday_time32
# endif
#endif

static inline int __set_timezone(const struct timezone *tz)
{
#ifdef SYS_settimeofday
	errno = 0;
	return syscall(SYS_settimeofday, NULL, tz);
#else
	return settimeofday(NULL, tz);
#endif
}

static int
set_system_clock(const struct hwclock_control *ctl,
		 const struct timeval newtime)
{
	struct tm broken = { 0 };
	int minuteswest;
	int rc = 0;

	localtime_r(&newtime.tv_sec, &broken);
	minuteswest = -get_gmtoff(&broken) / 60;

	if (ctl->verbose) {
		if (ctl->universal) {
			puts(_("Calling settimeofday(NULL, 0) "
			       "to lock the warp_clock function."));
			if (!( ctl->universal && !minuteswest ))
				printf(_("Calling settimeofday(NULL, %d) "
					 "to set the kernel timezone.\n"),
				       minuteswest);
		} else
			printf(_("Calling settimeofday(NULL, %d) to warp "
				 "System time, set PCIL and the kernel tz.\n"),
			       minuteswest);

		if (ctl->hctosys)
			printf(_("Calling settimeofday(%"PRId64".%06"PRId64", NULL) "
				 "to set the System time.\n"),
			       (int64_t)newtime.tv_sec, (int64_t)newtime.tv_usec);
	}

	if (!ctl->testing) {
		const struct timezone tz_utc = { 0 };
		const struct timezone tz = { minuteswest };

		/* If UTC RTC: lock warp_clock and PCIL */
		if (ctl->universal)
			rc = __set_timezone(&tz_utc);

		/* Set kernel tz; if localtime RTC: warp_clock and set PCIL */
		if (!rc && !( ctl->universal && !minuteswest ))
			rc = __set_timezone(&tz);

		/* Set the System Clock */
		if ((!rc || errno == ENOSYS) && ctl->hctosys)
			rc = __set_time(&newtime);

		if (rc) {
			warn(_("settimeofday() failed"));
			return  EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}

/*
 * Refresh the last calibrated and last adjusted timestamps in <*adjtime_p>
 * to facilitate future drift calculations based on this set point.
 *
 * With the --update-drift option:
 * Update the drift factor in <*adjtime_p> based on the fact that the
 * Hardware Clock was just calibrated to <nowtime> and before that was
 * set to the <hclocktime> time scale.
 */
static void
adjust_drift_factor(const struct hwclock_control *ctl,
		    struct adjtime *adjtime_p,
		    const struct timeval nowtime,
		    const struct timeval hclocktime)
{
	if (!ctl->update) {
		if (ctl->verbose)
			printf(_("Not adjusting drift factor because the "
				 "--update-drift option was not used.\n"));
	} else if (adjtime_p->last_calib_time == 0) {
		if (ctl->verbose)
			printf(_("Not adjusting drift factor because last "
				 "calibration time is zero,\n"
				 "so history is bad and calibration startover "
				 "is necessary.\n"));
	} else if ((hclocktime.tv_sec - adjtime_p->last_calib_time) < 4 * 60 * 60) {
		if (ctl->verbose)
			printf(_("Not adjusting drift factor because it has "
				 "been less than four hours since the last "
				 "calibration.\n"));
	} else {
		/*
		 * At adjustment time we drift correct the hardware clock
		 * according to the contents of the adjtime file and refresh
		 * its last adjusted timestamp.
		 *
		 * At calibration time we set the Hardware Clock and refresh
		 * both timestamps in <*adjtime_p>.
		 *
		 * Here, with the --update-drift option, we also update the
		 * drift factor in <*adjtime_p>.
		 *
		 * Let us do computation in doubles. (Floats almost suffice,
		 * but 195 days + 1 second equals 195 days in floats.)
		 */
		const double sec_per_day = 24.0 * 60.0 * 60.0;
		double factor_adjust;
		double drift_factor;
		struct timeval last_calib;

		last_calib = t2tv(adjtime_p->last_calib_time);
		/*
		 * Correction to apply to the current drift factor.
		 *
		 * Simplified: uncorrected_drift / days_since_calibration.
		 *
		 * hclocktime is fully corrected with the current drift factor.
		 * Its difference from nowtime is the missed drift correction.
		 */
		factor_adjust = time_diff(nowtime, hclocktime) /
				(time_diff(nowtime, last_calib) / sec_per_day);

		drift_factor = adjtime_p->drift_factor + factor_adjust;
		if (fabs(drift_factor) > MAX_DRIFT) {
			if (ctl->verbose)
				printf(_("Clock drift factor was calculated as "
					 "%f seconds/day.\n"
					 "It is far too much. Resetting to zero.\n"),
				       drift_factor);
			drift_factor = 0;
		} else {
			if (ctl->verbose)
				printf(_("Clock drifted %f seconds in the past "
					 "%f seconds\nin spite of a drift factor of "
					 "%f seconds/day.\n"
					 "Adjusting drift factor by %f seconds/day\n"),
				       time_diff(nowtime, hclocktime),
				       time_diff(nowtime, last_calib),
				       adjtime_p->drift_factor, factor_adjust);
		}

		adjtime_p->drift_factor = drift_factor;
	}
	adjtime_p->last_calib_time = nowtime.tv_sec;

	adjtime_p->last_adj_time = nowtime.tv_sec;

	adjtime_p->not_adjusted = 0;

	adjtime_p->dirty = 1;
}

/*
 * Calculate the drift correction currently needed for the
 * Hardware Clock based on the last time it was adjusted,
 * and the current drift factor, as stored in the adjtime file.
 *
 * The total drift adjustment needed is stored at tdrift_p.
 *
 */
static void
calculate_adjustment(const struct hwclock_control *ctl,
		     const double factor,
		     const time_t last_time,
		     const double not_adjusted,
		     const time_t systime, struct timeval *tdrift_p)
{
	double exact_adjustment;

	exact_adjustment =
	    ((double)(systime - last_time)) * factor / (24 * 60 * 60)
	    + not_adjusted;
	tdrift_p->tv_sec = (time_t) floor(exact_adjustment);
	tdrift_p->tv_usec = (exact_adjustment -
				 (double)tdrift_p->tv_sec) * 1E6;
	if (ctl->verbose) {
		printf(P_("Time since last adjustment is %"PRId64" second\n",
			"Time since last adjustment is %"PRId64" seconds\n",
		       ((int64_t)systime - (int64_t)last_time)),
		       ((int64_t)systime - (int64_t)last_time));
		printf(_("Calculated Hardware Clock drift is %"PRId64".%06"PRId64" seconds\n"),
		       (int64_t)tdrift_p->tv_sec, (int64_t)tdrift_p->tv_usec);
	}
}

/*
 * Write the contents of the <adjtime> structure to its disk file.
 *
 * But if the contents are clean (unchanged since read from disk), don't
 * bother.
 */
static int save_adjtime(const struct hwclock_control *ctl,
			 const struct adjtime *adjtime)
{
	char *content;		/* Stuff to write to disk file */
	FILE *fp;

	xasprintf(&content, "%f %"PRId64" %f\n%"PRId64"\n%s\n",
		  adjtime->drift_factor,
		  (int64_t)adjtime->last_adj_time,
		  adjtime->not_adjusted,
		  (int64_t)adjtime->last_calib_time,
		  (adjtime->local_utc == LOCAL) ? "LOCAL" : "UTC");

	if (ctl->verbose){
		printf(_("New %s data:\n%s"),
		       ctl->adj_file_name, content);
	}

	if (!ctl->testing) {
		int rc;

		fp = fopen(ctl->adj_file_name, "w");
		if (fp == NULL) {
			warn(_("cannot open %s"), ctl->adj_file_name);
			return EXIT_FAILURE;
		}

		rc = fputs(content, fp) < 0;
		rc += close_stream(fp);

		if (rc) {
			warn(_("cannot update %s"), ctl->adj_file_name);
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}

/*
 * Do the adjustment requested, by 1) setting the Hardware Clock (if
 * necessary), and 2) updating the last-adjusted time in the adjtime
 * structure.
 *
 * Do not update anything if the Hardware Clock does not currently present a
 * valid time.
 *
 * <hclocktime> is the drift corrected time read from the Hardware Clock.
 *
 * <read_time> was the system time when the <hclocktime> was read, which due
 * to computational delay could be a short time ago. It is used to define a
 * trigger point for setting the Hardware Clock. The fractional part of the
 * Hardware clock set time is subtracted from read_time to 'refer back', or
 * delay, the trigger point.  Fractional parts must be accounted for in this
 * way, because the Hardware Clock can only be set to a whole second.
 *
 * <universal>: the Hardware Clock is kept in UTC.
 *
 * <testing>:  We are running in test mode (no updating of clock).
 *
 */
static void
do_adjustment(const struct hwclock_control *ctl, struct adjtime *adjtime_p,
	      const struct timeval hclocktime,
	      const struct timeval read_time)
{
	if (adjtime_p->last_adj_time == 0) {
		if (ctl->verbose)
			printf(_("Not setting clock because last adjustment time is zero, "
				 "so history is bad.\n"));
	} else if (fabs(adjtime_p->drift_factor) > MAX_DRIFT) {
		if (ctl->verbose)
			printf(_("Not setting clock because drift factor %f is far too high.\n"),
				adjtime_p->drift_factor);
	} else {
		set_hardware_clock_exact(ctl, hclocktime.tv_sec,
					 time_inc(read_time,
						  -(hclocktime.tv_usec / 1E6)));
		adjtime_p->last_adj_time = hclocktime.tv_sec;
		adjtime_p->not_adjusted = 0;
		adjtime_p->dirty = 1;
	}
}

static void determine_clock_access_method(const struct hwclock_control *ctl)
{
	ur = NULL;

#ifdef USE_HWCLOCK_CMOS
	if (ctl->directisa)
		ur = probe_for_cmos_clock();
#endif
#ifdef __linux__
	if (!ur)
		ur = probe_for_rtc_clock(ctl);
#endif
	if (ur) {
		if (ctl->verbose)
			puts(ur->interface_name);

	} else {
		if (ctl->verbose)
			printf(_("No usable clock interface found.\n"));

		warnx(_("Cannot access the Hardware Clock via "
			"any known method."));

		if (!ctl->verbose)
			warnx(_("Use the --verbose option to see the "
				"details of our search for an access "
				"method."));
		hwclock_exit(ctl, EXIT_FAILURE);
	}
}

/* Do all the normal work of hwclock - read, set clock, etc. */
static int
manipulate_clock(const struct hwclock_control *ctl, const time_t set_time,
		 const struct timeval startup_time, struct adjtime *adjtime)
{
	/* The time at which we read the Hardware Clock */
	struct timeval read_time = { 0 };
	/*
	 * The Hardware Clock gives us a valid time, or at
	 * least something close enough to fool mktime().
	 */
	int hclock_valid = 0;
	/*
	 * Tick synchronized time read from the Hardware Clock and
	 * then drift corrected for all operations except --show.
	 */
	struct timeval hclocktime = { 0 };
	/*
	 * hclocktime correlated to startup_time. That is, what drift
	 * corrected Hardware Clock time would have been at start up.
	 */
	struct timeval startup_hclocktime = { 0 };
	/* Total Hardware Clock drift correction needed. */
	struct timeval tdrift = { 0 };

	if ((ctl->set || ctl->systohc || ctl->adjust) &&
	    (adjtime->local_utc == UTC) != ctl->universal) {
		adjtime->local_utc = ctl->universal ? UTC : LOCAL;
		adjtime->dirty = 1;
	}
	/*
	 * Negate the drift correction, because we want to 'predict' a
	 * Hardware Clock time that includes drift.
	 */
	if (ctl->predict) {
		hclocktime = t2tv(set_time);
		calculate_adjustment(ctl, adjtime->drift_factor,
				     adjtime->last_adj_time,
				     adjtime->not_adjusted,
				     hclocktime.tv_sec, &tdrift);
		hclocktime = time_inc(hclocktime, (double)
				      -(tdrift.tv_sec + tdrift.tv_usec / 1E6));
		if (ctl->verbose) {
			printf(_("Target date:   %"PRId64"\n"), (int64_t)set_time);
			printf(_("Predicted RTC: %"PRId64"\n"), (int64_t)hclocktime.tv_sec);
		}
		return display_time(hclocktime);
	}

	if (ctl->systz)
		return set_system_clock(ctl, startup_time);

	if (ur->get_permissions())
		return EXIT_FAILURE;

	/*
	 * Read and drift correct RTC time; except for RTC set functions
	 * without the --update-drift option because: 1) it's not needed;
	 * 2) it enables setting a corrupted RTC without reading it first;
	 * 3) it significantly reduces system shutdown time.
	 */
	if ( ! ((ctl->set || ctl->systohc) && !ctl->update)) {
		/*
		 * Timing critical - do not change the order of, or put
		 * anything between the follow three statements.
		 * Synchronization failure MUST exit, because all drift
		 * operations are invalid without it.
		 */
		if (synchronize_to_clock_tick(ctl))
			return EXIT_FAILURE;
		read_hardware_clock(ctl, &hclock_valid, &hclocktime.tv_sec);
		gettimeofday(&read_time, NULL);

		if (!hclock_valid) {
			warnx(_("RTC read returned an invalid value."));
			return EXIT_FAILURE;
		}
		/*
		 * Calculate and apply drift correction to the Hardware Clock
		 * time for everything except --show
		 */
		calculate_adjustment(ctl, adjtime->drift_factor,
				     adjtime->last_adj_time,
				     adjtime->not_adjusted,
				     hclocktime.tv_sec, &tdrift);
		if (!ctl->show)
			hclocktime = time_inc(tdrift, hclocktime.tv_sec);

		startup_hclocktime =
		 time_inc(hclocktime, time_diff(startup_time, read_time));
	}
	if (ctl->show || ctl->get) {
		return display_time(startup_hclocktime);
	}

	if (ctl->set) {
		set_hardware_clock_exact(ctl, set_time, startup_time);
		if (!ctl->noadjfile)
			adjust_drift_factor(ctl, adjtime, t2tv(set_time),
					    startup_hclocktime);
	} else if (ctl->adjust) {
		if (tdrift.tv_sec > 0 || tdrift.tv_sec < -1)
			do_adjustment(ctl, adjtime, hclocktime, read_time);
		else
			printf(_("Needed adjustment is less than one second, "
				 "so not setting clock.\n"));
	} else if (ctl->systohc) {
		struct timeval nowtime, reftime;
		/*
		 * We can only set_hardware_clock_exact to a
		 * whole seconds time, so we set it with
		 * reference to the most recent whole
		 * seconds time.
		 */
		gettimeofday(&nowtime, NULL);
		reftime.tv_sec = nowtime.tv_sec;
		reftime.tv_usec = 0;
		set_hardware_clock_exact(ctl, (time_t) reftime.tv_sec, reftime);
		if (!ctl->noadjfile)
			adjust_drift_factor(ctl, adjtime, nowtime,
					    hclocktime);
	} else if (ctl->hctosys) {
		return set_system_clock(ctl, hclocktime);
	}
	if (!ctl->noadjfile && adjtime->dirty)
		return save_adjtime(ctl, adjtime);
	return EXIT_SUCCESS;
}

/**
 * Get or set the kernel RTC driver's epoch on Alpha machines.
 * ISA machines are hard coded for 1900.
 */
#if defined(__linux__) && defined(__alpha__)
static void
manipulate_epoch(const struct hwclock_control *ctl)
{
	if (ctl->getepoch) {
		unsigned long epoch;

		if (get_epoch_rtc(ctl, &epoch))
			warnx(_("unable to read the RTC epoch."));
		else
			printf(_("The RTC epoch is set to %lu.\n"), epoch);
	} else if (ctl->setepoch) {
		if (!ctl->epoch_option)
			warnx(_("--epoch is required for --setepoch."));
		else if (!ctl->testing)
			if (set_epoch_rtc(ctl))
				warnx(_("unable to set the RTC epoch."));
	}
}
#endif		/* __linux__ __alpha__ */

#ifdef __linux__
static int
manipulate_rtc_param(const struct hwclock_control *ctl)
{
	if (ctl->param_get_option) {
		uint64_t id = 0, value = 0;

		if (get_param_rtc(ctl, ctl->param_get_option, &id, &value)) {
			warnx(_("unable to read the RTC parameter %s"),
					ctl->param_get_option);
			return 1;
		}

		printf(_("The RTC parameter 0x%jx is set to 0x%jx.\n"),
		       (uintmax_t) id, (uintmax_t) value);
		return 0;

	} else if (ctl->param_set_option) {
		if (ctl->testing)
			return 0;

		return set_param_rtc(ctl, ctl->param_set_option);
	}

	return 1;
}

static int
manipulate_rtc_voltage_low(const struct hwclock_control *ctl)
{
	if (ctl->vl_read) {
		if (rtc_vl_read(ctl))
			return 1;
	}
	if (ctl->vl_clear) {
		if (rtc_vl_clear(ctl))
			return 1;
	}
	return 0;
}
#endif

static void out_version(void)
{
	printf(UTIL_LINUX_VERSION);
}

static void __attribute__((__noreturn__))
usage(void)
{
#ifdef __linux__
	const struct hwclock_param *param = get_hwclock_params();
#endif

	fputs(USAGE_HEADER, stdout);
	printf(_(" %s [function] [option...]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, stdout);
	puts(_("Time clocks utility."));

	fputs(USAGE_FUNCTIONS, stdout);
	puts(_(" -r, --show                      display the RTC time"));
	puts(_("     --get                       display drift corrected RTC time"));
	puts(_("     --set                       set the RTC according to --date"));
	puts(_(" -s, --hctosys                   set the system time from the RTC"));
	puts(_(" -w, --systohc                   set the RTC from the system time"));
	puts(_("     --systz                     send timescale configurations to the kernel"));
	puts(_(" -a, --adjust                    adjust the RTC to account for systematic drift"));
#if defined(__linux__) && defined(__alpha__)
	puts(_("     --getepoch                  display the RTC epoch"));
	puts(_("     --setepoch                  set the RTC epoch according to --epoch"));
#endif
#ifdef __linux__
	puts(_("     --param-get <param>         display the RTC parameter"));
	puts(_("     --param-set <param>=<value> set the RTC parameter"));
	puts(_("     --vl-read                   read voltage low information"));
	puts(_("     --vl-clear                  clear voltage low information"));
#endif
	puts(_("     --predict                   predict the drifted RTC time according to --date"));
	fputs(USAGE_OPTIONS, stdout);
	puts(_(" -u, --utc                       the RTC timescale is UTC"));
	puts(_(" -l, --localtime                 the RTC timescale is Local"));
#ifdef __linux__
	printf(_(
	       " -f, --rtc <file>                use an alternate file to %1$s\n"), _PATH_RTC_DEV);
#endif
	printf(_(
	       "     --directisa                 use the ISA bus instead of %1$s access\n"), _PATH_RTC_DEV);
	puts(_("     --date <time>               date/time input for --set and --predict"));
	puts(_("     --delay <sec>               delay used when set new RTC time"));
#if defined(__linux__) && defined(__alpha__)
	puts(_("     --epoch <year>              epoch input for --setepoch"));
#endif
	puts(_("     --update-drift              update the RTC drift factor"));
	printf(_(
	       "     --noadjfile                 do not use %1$s\n"), _PATH_ADJTIME);
	printf(_(
	       "     --adjfile <file>            use an alternate file to %1$s\n"), _PATH_ADJTIME);
	puts(_("     --test                      dry run; implies --verbose"));
	puts(_(" -v, --verbose                   display more details"));

	fputs(USAGE_SEPARATOR, stdout);
	fprintf(stdout, USAGE_HELP_OPTIONS(33));

#ifdef __linux__
	fputs(USAGE_ARGUMENTS, stdout);
	fputsln(_(" <param> is either a numeric RTC parameter value or one of these aliases:"), stdout);

	while (param->name) {
		fprintf(stdout, _("   - %1$s: %2$s (0x%3$x)\n"), param->name, param->help, param->id);
		param++;
	}

	fputsln(_("   See Kernel's include/uapi/linux/rtc.h for parameters and values."), stdout);
	fputs(USAGE_ARG_SEPARATOR, stdout);
	fputsln(_(" <param> and <value> accept hexadecimal values if prefixed with 0x, otherwise decimal."), stdout);
#endif
	fprintf(stdout, USAGE_MAN_TAIL("hwclock(8)"));
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	struct hwclock_control ctl = {
			.show = 1,		/* default op is show */
			.rtc_delay = -1.0	/* unspecified */
	};
	struct timeval startup_time;
	struct adjtime adjtime = { 0 };
	/*
	 * The time we started up, in seconds into the epoch, including
	 * fractions.
	 */
	time_t set_time = 0;	/* Time to which user said to set Hardware Clock */
	int rc, c;

	/* Long only options. */
	enum {
		OPT_ADJFILE = CHAR_MAX + 1,
		OPT_DATE,
		OPT_DELAY,
		OPT_DIRECTISA,
		OPT_EPOCH,
		OPT_GET,
		OPT_GETEPOCH,
		OPT_NOADJFILE,
		OPT_PARAM_GET,
		OPT_PARAM_SET,
		OPT_VL_READ,
		OPT_VL_CLEAR,
		OPT_PREDICT,
		OPT_SET,
		OPT_SETEPOCH,
		OPT_SYSTZ,
		OPT_TEST,
		OPT_UPDATE
	};

	static const struct option longopts[] = {
		{ "adjust",       no_argument,       NULL, 'a'            },
		{ "help",         no_argument,       NULL, 'h'            },
		{ "localtime",    no_argument,       NULL, 'l'            },
		{ "show",         no_argument,       NULL, 'r'            },
		{ "hctosys",      no_argument,       NULL, 's'            },
		{ "utc",          no_argument,       NULL, 'u'            },
		{ "version",      no_argument,       NULL, 'V'            },
		{ "systohc",      no_argument,       NULL, 'w'            },
		{ "debug",        no_argument,       NULL, 'D'            },
		{ "ul-debug",     required_argument, NULL, 'd'            },
		{ "verbose",      no_argument,       NULL, 'v'            },
		{ "set",          no_argument,       NULL, OPT_SET        },
#if defined(__linux__) && defined(__alpha__)
		{ "getepoch",     no_argument,       NULL, OPT_GETEPOCH   },
		{ "setepoch",     no_argument,       NULL, OPT_SETEPOCH   },
		{ "epoch",        required_argument, NULL, OPT_EPOCH      },
#endif
#ifdef __linux__
		{ "param-get",    required_argument, NULL, OPT_PARAM_GET  },
		{ "param-set",    required_argument, NULL, OPT_PARAM_SET  },
		{ "vl-read",      no_argument,       NULL, OPT_VL_READ    },
		{ "vl-clear",     no_argument,       NULL, OPT_VL_CLEAR   },
#endif
		{ "noadjfile",    no_argument,       NULL, OPT_NOADJFILE  },
		{ "directisa",    no_argument,       NULL, OPT_DIRECTISA  },
		{ "test",         no_argument,       NULL, OPT_TEST       },
		{ "date",         required_argument, NULL, OPT_DATE       },
		{ "delay",        required_argument, NULL, OPT_DELAY      },
#ifdef __linux__
		{ "rtc",          required_argument, NULL, 'f'            },
#endif
		{ "adjfile",      required_argument, NULL, OPT_ADJFILE    },
		{ "systz",        no_argument,       NULL, OPT_SYSTZ      },
		{ "predict",      no_argument,       NULL, OPT_PREDICT    },
		{ "get",          no_argument,       NULL, OPT_GET        },
		{ "update-drift", no_argument,       NULL, OPT_UPDATE     },
		{ NULL, 0, NULL, 0 }
	};

	static const ul_excl_t excl[] = {	/* rows and cols in ASCII order */
		{ 'a','r','s','w',
		  OPT_GET, OPT_GETEPOCH, OPT_PREDICT,
		  OPT_SET, OPT_SETEPOCH, OPT_SYSTZ },
		{ 'l', 'u' },
		{ OPT_ADJFILE, OPT_NOADJFILE },
		{ OPT_NOADJFILE, OPT_UPDATE },
		{ 0 }
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;

	/* Remember what time we were invoked */
	gettimeofday(&startup_time, NULL);

#ifdef HAVE_LIBAUDIT
	hwaudit_fd = audit_open();
	if (hwaudit_fd < 0 && !(errno == EINVAL || errno == EPROTONOSUPPORT ||
				errno == EAFNOSUPPORT)) {
		/*
		 * You get these error codes only when the kernel doesn't
		 * have audit compiled in.
		 */
		warnx(_("Unable to connect to audit system"));
		return EXIT_FAILURE;
	}
#endif
	setlocale(LC_ALL, "");
#ifdef LC_NUMERIC
	/*
	 * We need LC_CTYPE and LC_TIME and LC_MESSAGES, but must avoid
	 * LC_NUMERIC since it gives problems when we write to /etc/adjtime.
	 *  - gqueri@mail.dotcom.fr
	 */
	setlocale(LC_NUMERIC, "C");
#endif
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while ((c = getopt_long(argc, argv,
				"hvVDd:alrsuwf:", longopts, NULL)) != -1) {

		err_exclusive_options(c, longopts, excl, excl_st);

		switch (c) {
		case 'D':
			warnx(_("use --verbose, --debug has been deprecated."));
			break;
		case 'v':
			ctl.verbose = 1;
			break;
		case 'd':
			hwclock_init_debug(optarg);
			break;
		case 'a':
			ctl.adjust = 1;
			ctl.show = 0;
			ctl.hwaudit_on = 1;
			break;
		case 'l':
			ctl.local_opt = 1;	/* --localtime */
			break;
		case 'r':
			ctl.show = 1;
			break;
		case 's':
			ctl.hctosys = 1;
			ctl.show = 0;
			ctl.hwaudit_on = 1;
			break;
		case 'u':
			ctl.utc = 1;
			break;
		case 'w':
			ctl.systohc = 1;
			ctl.show = 0;
			ctl.hwaudit_on = 1;
			break;
		case OPT_SET:
			ctl.set = 1;
			ctl.show = 0;
			ctl.hwaudit_on = 1;
			break;
#if defined(__linux__) && defined(__alpha__)
		case OPT_GETEPOCH:
			ctl.getepoch = 1;
			ctl.show = 0;
			break;
		case OPT_SETEPOCH:
			ctl.setepoch = 1;
			ctl.show = 0;
			ctl.hwaudit_on = 1;
			break;
		case OPT_EPOCH:
			ctl.epoch_option = optarg;	/* --epoch */
			break;
#endif
#ifdef __linux__
		case OPT_PARAM_GET:
			ctl.param_get_option = optarg;
			ctl.show = 0;
			break;
		case OPT_PARAM_SET:
			ctl.param_set_option = optarg;
			ctl.show = 0;
			ctl.hwaudit_on = 1;
			break;
		case OPT_VL_READ:
			ctl.vl_read = 1;
			ctl.show = 0;
			break;
		case OPT_VL_CLEAR:
			ctl.vl_clear = 1;
			ctl.show = 0;
			break;
#endif
		case OPT_NOADJFILE:
			ctl.noadjfile = 1;
			break;
		case OPT_DIRECTISA:
			ctl.directisa = 1;
			break;
		case OPT_TEST:
			ctl.testing = 1;	/* --test */
			ctl.verbose = 1;
			break;
		case OPT_DATE:
			ctl.date_opt = optarg;	/* --date */
			break;
		case OPT_DELAY:
			ctl.rtc_delay = strtod_or_err(optarg, "invalid --delay argument");
			break;
		case OPT_ADJFILE:
			ctl.adj_file_name = optarg;	/* --adjfile */
			break;
		case OPT_SYSTZ:
			ctl.systz = 1;		/* --systz */
			ctl.show = 0;
			ctl.hwaudit_on = 1;
			break;
		case OPT_PREDICT:
			ctl.predict = 1;	/* --predict */
			ctl.show = 0;
			break;
		case OPT_GET:
			ctl.get = 1;		/* --get */
			ctl.show = 0;
			break;
		case OPT_UPDATE:
			ctl.update = 1;		/* --update-drift */
			break;
#ifdef __linux__
		case 'f':
			ctl.rtc_dev_name = optarg;	/* --rtc */
			break;
#endif

		case 'V':			/* --version */
			print_version(EXIT_SUCCESS);
		case 'h':			/* --help */
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (argc -= optind) {
		warnx(_("too many arguments"));
		errtryhelp(EXIT_FAILURE);
	}

	if (!ctl.adj_file_name)
		ctl.adj_file_name = _PATH_ADJTIME;

	if (ctl.update && !ctl.set && !ctl.systohc) {
		warnx(_("--update-drift requires --set or --systohc"));
		exit(EXIT_FAILURE);
	}

	if (ctl.noadjfile && !ctl.utc && !ctl.local_opt) {
		warnx(_("With --noadjfile, you must specify "
			"either --utc or --localtime"));
		exit(EXIT_FAILURE);
	}

	if (ctl.set || ctl.predict) {
		if (!ctl.date_opt) {
			warnx(_("--date is required for --set or --predict"));
			exit(EXIT_FAILURE);
		}
#ifdef USE_HWCLOCK_GPLv3_DATETIME
		/* date(1) compatible GPLv3 parser */
		struct timespec when = { 0 };

		if (parse_date(&when, ctl.date_opt, NULL))
			set_time = when.tv_sec;
#else
		/* minimalistic GPLv2 based parser */
		usec_t usec;

		if (parse_timestamp(ctl.date_opt, &usec) == 0)
			set_time = (time_t) (usec / 1000000);
#endif
		else {
			warnx(_("invalid date '%s'"), ctl.date_opt);
			exit(EXIT_FAILURE);
		}
	}

#ifdef __linux__
	if (ctl.param_get_option || ctl.param_set_option) {
		if (manipulate_rtc_param(&ctl))
			hwclock_exit(&ctl, EXIT_FAILURE);

		hwclock_exit(&ctl, EXIT_SUCCESS);
	}

	if (ctl.vl_read || ctl.vl_clear) {
		if (manipulate_rtc_voltage_low(&ctl))
			hwclock_exit(&ctl, EXIT_FAILURE);

		hwclock_exit(&ctl, EXIT_SUCCESS);
	}
#endif

#if defined(__linux__) && defined(__alpha__)
	if (ctl.getepoch || ctl.setepoch) {
		manipulate_epoch(&ctl);
		hwclock_exit(&ctl, EXIT_SUCCESS);
	}
#endif

	if (ctl.verbose) {
		out_version();
		printf(_("System Time: %"PRId64".%06"PRId64"\n"),
		       (int64_t)startup_time.tv_sec, (int64_t)startup_time.tv_usec);
	}

	if (!ctl.systz && !ctl.predict)
		determine_clock_access_method(&ctl);

	if (!ctl.noadjfile && !(ctl.systz && (ctl.utc || ctl.local_opt))) {
		if ((rc = read_adjtime(&ctl, &adjtime)) != 0)
			hwclock_exit(&ctl, rc);
	} else
		/* Avoid writing adjtime file if we don't have to. */
		adjtime.dirty = 0;

	ctl.universal = hw_clock_is_utc(&ctl, &adjtime);
	rc = manipulate_clock(&ctl, set_time, startup_time, &adjtime);
	if (ctl.testing)
		puts(_("Test mode: nothing was changed."));
	hwclock_exit(&ctl, rc);
	return rc;		/* Not reached */
}

void
hwclock_exit(const struct hwclock_control *ctl
#ifndef HAVE_LIBAUDIT
	     __attribute__((__unused__))
#endif
	     , int status)
{
#ifdef HAVE_LIBAUDIT
	int ret;

	if (ctl->hwaudit_on && !ctl->testing) {
		ret = audit_log_user_message(hwaudit_fd, AUDIT_USYS_CONFIG,
					     "op=change-system-time", NULL, NULL, NULL,
					     status == EXIT_SUCCESS ? 1  : 0);
		if (ret == -1)
			warn(_("could not send audit message"));
	}
	close(hwaudit_fd);
#endif
	exit(status);
}

/*
 * History of this program:
 *
 * 98.08.12 BJH Version 2.4
 *
 * Don't use century byte from Hardware Clock. Add comments telling why.
 *
 * 98.06.20 BJH Version 2.3.
 *
 * Make --hctosys set the kernel timezone from TZ environment variable
 * and/or /usr/lib/zoneinfo. From Klaus Ripke (klaus@ripke.com).
 *
 * 98.03.05 BJH. Version 2.2.
 *
 * Add --getepoch and --setepoch.
 *
 * Fix some word length things so it works on Alpha.
 *
 * Make it work when /dev/rtc doesn't have the interrupt functions. In this
 * case, busywait for the top of a second instead of blocking and waiting
 * for the update complete interrupt.
 *
 * Fix a bunch of bugs too numerous to mention.
 *
 * 97.06.01: BJH. Version 2.1. Read and write the century byte (Byte 50) of
 * the ISA Hardware Clock when using direct ISA I/O. Problem discovered by
 * job (jei@iclnl.icl.nl).
 *
 * Use the rtc clock access method in preference to the KDGHWCLK method.
 * Problem discovered by Andreas Schwab <schwab@LS5.informatik.uni-dortmund.de>.
 *
 * November 1996: Version 2.0.1. Modifications by Nicolai Langfeldt
 * (janl@math.uio.no) to make it compile on linux 1.2 machines as well as
 * more recent versions of the kernel. Introduced the NO_CLOCK access method
 * and wrote feature test code to detect absence of rtc headers.
 *
 ***************************************************************************
 * Maintenance notes
 *
 * To compile this, you must use GNU compiler optimization (-O option) in
 * order to make the "extern inline" functions from asm/io.h (inb(), etc.)
 * compile. If you don't optimize, which means the compiler will generate no
 * inline functions, the references to these functions in this program will
 * be compiled as external references. Since you probably won't be linking
 * with any functions by these names, you will have unresolved external
 * references when you link.
 *
 * Here's some info on how we must deal with the time that elapses while
 * this program runs: There are two major delays as we run:
 *
 *	1) Waiting up to 1 second for a transition of the Hardware Clock so
 *	   we are synchronized to the Hardware Clock.
 *	2) Running the "date" program to interpret the value of our --date
 *	   option.
 *
 * Reading the /etc/adjtime file is the next biggest source of delay and
 * uncertainty.
 *
 * The user wants to know what time it was at the moment they invoked us, not
 * some arbitrary time later. And in setting the clock, they are giving us the
 * time at the moment we are invoked, so if we set the clock some time
 * later, we have to add some time to that.
 *
 * So we check the system time as soon as we start up, then run "date" and
 * do file I/O if necessary, then wait to synchronize with a Hardware Clock
 * edge, then check the system time again to see how much time we spent. We
 * immediately read the clock then and (if appropriate) report that time,
 * and additionally, the delay we measured.
 *
 * If we're setting the clock to a time given by the user, we wait some more
 * so that the total delay is an integral number of seconds, then set the
 * Hardware Clock to the time the user requested plus that integral number
 * of seconds. N.B. The Hardware Clock can only be set in integral seconds.
 *
 * If we're setting the clock to the system clock value, we wait for the
 * system clock to reach the top of a second, and then set the Hardware
 * Clock to the system clock's value.
 *
 * Here's an interesting point about setting the Hardware Clock:  On my
 * machine, when you set it, it sets to that precise time. But one can
 * imagine another clock whose update oscillator marches on a steady one
 * second period, so updating the clock between any two oscillator ticks is
 * the same as updating it right at the earlier tick. To avoid any
 * complications that might cause, we set the clock as soon as possible
 * after an oscillator tick.
 *
 * About synchronizing to the Hardware Clock when reading the time: The
 * precision of the Hardware Clock counters themselves is one second. You
 * can't read the counters and find out that is 12:01:02.5. But if you
 * consider the location in time of the counter's ticks as part of its
 * value, then its precision is as infinite as time is continuous! What I'm
 * saying is this: To find out the _exact_ time in the hardware clock, we
 * wait until the next clock tick (the next time the second counter changes)
 * and measure how long we had to wait. We then read the value of the clock
 * counters and subtract the wait time and we know precisely what time it
 * was when we set out to query the time.
 *
 * hwclock uses this method, and considers the Hardware Clock to have
 * infinite precision.
 */
