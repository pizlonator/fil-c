/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * rtc.c - Use /dev/rtc for clock access
 */
#include <asm/ioctl.h>
#include <errno.h>
#include <linux/rtc.h>
#include <linux/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "monotonic.h"
#include "strutils.h"
#include "xalloc.h"
#include "nls.h"

#include "hwclock.h"

#ifndef RTC_PARAM_GET
struct rtc_param {
	__u64 param;
	union {
		__u64 uvalue;
		__s64 svalue;
		__u64 ptr;
	};
	__u32 index;
	__u32 __pad;
};

# define RTC_PARAM_GET	_IOW('p', 0x13, struct rtc_param)
# define RTC_PARAM_SET	_IOW('p', 0x14, struct rtc_param)

# define RTC_PARAM_FEATURES		0
# define RTC_PARAM_CORRECTION		1
# define RTC_PARAM_BACKUP_SWITCH_MODE	2
#endif /* RTC_PARAM_GET */

static const struct hwclock_param hwclock_params[] =
{
	{ RTC_PARAM_FEATURES,  "features", N_("supported features") },
	{ RTC_PARAM_CORRECTION, "correction", N_("time correction") },
	{ RTC_PARAM_BACKUP_SWITCH_MODE, "bsm", N_("backup switch mode") },
	{ }
};

const struct hwclock_param *get_hwclock_params(void)
{
	return hwclock_params;
}

/*
 * /dev/rtc is conventionally chardev 10/135
 * ia64 uses /dev/efirtc, chardev 10/136
 * devfs (obsolete) used /dev/misc/... for miscdev
 * new RTC framework + udev uses dynamic major and /dev/rtc0.../dev/rtcN
 * ... so we need an overridable default
 */

/* default or user defined dev (by hwclock --rtc=<path>) */
static const char *rtc_dev_name;
static int rtc_dev_fd = -1;

static void close_rtc(void)
{
	if (rtc_dev_fd != -1)
		close(rtc_dev_fd);
	rtc_dev_fd = -1;
}

static int open_rtc(const struct hwclock_control *ctl)
{
	static const char * const fls[] = {
#ifdef __ia64__
		"/dev/efirtc",
		"/dev/misc/efirtc",
#endif
		"/dev/rtc0",
		"/dev/rtc",
		"/dev/misc/rtc"
	};
	size_t i;

	if (rtc_dev_fd != -1)
		return rtc_dev_fd;

	/* --rtc option has been given */
	if (ctl->rtc_dev_name) {
		rtc_dev_name = ctl->rtc_dev_name;
		rtc_dev_fd = open(rtc_dev_name, O_RDONLY);
	} else {
		for (i = 0; i < ARRAY_SIZE(fls); i++) {
			if (ctl->verbose)
				printf(_("Trying to open: %s\n"), fls[i]);
			rtc_dev_fd = open(fls[i], O_RDONLY);

			if (rtc_dev_fd < 0) {
				if (errno == ENOENT || errno == ENODEV)
					continue;
				if (ctl->verbose)
					warn(_("cannot open %s"), fls[i]);
			}
			rtc_dev_name = fls[i];
			break;
		}
		if (rtc_dev_fd < 0)
			rtc_dev_name = *fls;	/* default for error messages */
	}
	if (rtc_dev_fd != -1)
		atexit(close_rtc);
	return rtc_dev_fd;
}

static int open_rtc_or_exit(const struct hwclock_control *ctl)
{
	int rtc_fd = open_rtc(ctl);

	if (rtc_fd < 0) {
		warn(_("cannot open rtc device"));
		hwclock_exit(ctl, EXIT_FAILURE);
	}
	return rtc_fd;
}

static int do_rtc_read_ioctl(int rtc_fd, struct tm *tm)
{
	int rc = -1;
	struct rtc_time rtc_tm = { 0 };

	rc = ioctl(rtc_fd, RTC_RD_TIME, &rtc_tm);

	if (rc == -1) {
		warn(_("ioctl(RTC_RD_NAME) to %s to read the time failed"),
			rtc_dev_name);
		return -1;
	}

	/* kernel uses private struct tm definition to be self contained */
	tm->tm_sec   = rtc_tm.tm_sec;
	tm->tm_min   = rtc_tm.tm_min;
	tm->tm_hour  = rtc_tm.tm_hour;
	tm->tm_mday  = rtc_tm.tm_mday;
	tm->tm_mon   = rtc_tm.tm_mon;
	tm->tm_year  = rtc_tm.tm_year;
	tm->tm_wday  = rtc_tm.tm_wday;
	tm->tm_yday  = rtc_tm.tm_yday;
	tm->tm_isdst = -1;	/* don't know whether it's dst */
	return 0;
}

/*
 * Wait for the top of a clock tick by reading /dev/rtc in a busy loop
 * until we see it. This function is used for rtc drivers without ioctl
 * interrupts. This is typical on an Alpha, where the Hardware Clock
 * interrupts are used by the kernel for the system clock, so aren't at
 * the user's disposal.
 */
static int busywait_for_rtc_clock_tick(const struct hwclock_control *ctl,
				       const int rtc_fd)
{
	struct tm start_time = { 0 };
	/* The time when we were called (and started waiting) */
	struct tm nowtime = { 0 };
	int rc;
	struct timeval begin = { 0 }, now = { 0 };

	if (ctl->verbose) {
		printf("ioctl(%d, RTC_UIE_ON, 0): %s\n",
		       rtc_fd, strerror(errno));
		printf(_("Waiting in loop for time from %s to change\n"),
		       rtc_dev_name);
	}

	if (do_rtc_read_ioctl(rtc_fd, &start_time))
		return 1;

	/*
	 * Wait for change.  Should be within a second, but in case
	 * something weird happens, we have a time limit (1.5s) on this loop
	 * to reduce the impact of this failure.
	 */
	gettime_monotonic(&begin);
	do {
		rc = do_rtc_read_ioctl(rtc_fd, &nowtime);
		if (rc || start_time.tm_sec != nowtime.tm_sec)
			break;
		gettime_monotonic(&now);
		if (time_diff(now, begin) > 1.5) {
			warnx(_("Timed out waiting for time change."));
			return 1;
		}
	} while (1);

	if (rc)
		return 1;
	return 0;
}

/*
 * Same as synchronize_to_clock_tick(), but just for /dev/rtc.
 */
static int synchronize_to_clock_tick_rtc(const struct hwclock_control *ctl)
{
	int rtc_fd;		/* File descriptor of /dev/rtc */
	int ret = 1;

	rtc_fd = open_rtc(ctl);
	if (rtc_fd == -1) {
		warn(_("cannot open rtc device"));
		return ret;
	}

	/* Turn on update interrupts (one per second) */
	int rc = ioctl(rtc_fd, RTC_UIE_ON, 0);

	if (rc != -1) {
		/*
		 * Just reading rtc_fd fails on broken hardware: no
		 * update interrupt comes and a bootscript with a
		 * hwclock call hangs
		 */
		fd_set rfds;
		struct timeval tv;

		/*
		 * Wait up to ten seconds for the next update
		 * interrupt
		 */
		FD_ZERO(&rfds);
		FD_SET(rtc_fd, &rfds);
		tv.tv_sec = 10;
		tv.tv_usec = 0;
		rc = select(rtc_fd + 1, &rfds, NULL, NULL, &tv);
		if (0 < rc)
			ret = 0;
		else if (rc == 0) {
			warnx(_("select() to %s to wait for clock tick timed out"),
			      rtc_dev_name);
		} else
			warn(_("select() to %s to wait for clock tick failed"),
			     rtc_dev_name);
		/* Turn off update interrupts */
		rc = ioctl(rtc_fd, RTC_UIE_OFF, 0);
		if (rc == -1)
			warn(_("ioctl() to %s to turn off update interrupts failed"),
			     rtc_dev_name);
	} else if (errno == ENOTTY || errno == EINVAL) {
		/* rtc ioctl interrupts are unimplemented */
		ret = busywait_for_rtc_clock_tick(ctl, rtc_fd);
	} else
		warn(_("ioctl(%d, RTC_UIE_ON, 0) to %s failed"),
		     rtc_fd, rtc_dev_name);
	return ret;
}

static int read_hardware_clock_rtc(const struct hwclock_control *ctl,
				   struct tm *tm)
{
	int rtc_fd, rc;

	rtc_fd = open_rtc_or_exit(ctl);

	/* Read the RTC time/date, return answer via tm */
	rc = do_rtc_read_ioctl(rtc_fd, tm);

	return rc;
}

/*
 * Set the Hardware Clock to the broken down time <new_broken_time>. Use
 * ioctls to "rtc" device /dev/rtc.
 */
static int set_hardware_clock_rtc(const struct hwclock_control *ctl,
				  const struct tm *new_broken_time)
{
	int rc = -1;
	int rtc_fd;
	struct rtc_time rtc_tm = { 0 };

	rtc_fd = open_rtc_or_exit(ctl);

	/* kernel uses private struct tm definition to be self contained */
	rtc_tm.tm_sec   = new_broken_time->tm_sec;
	rtc_tm.tm_min   = new_broken_time->tm_min;
	rtc_tm.tm_hour  = new_broken_time->tm_hour;
	rtc_tm.tm_mday  = new_broken_time->tm_mday;
	rtc_tm.tm_mon   = new_broken_time->tm_mon;
	rtc_tm.tm_year  = new_broken_time->tm_year;
	rtc_tm.tm_wday  = new_broken_time->tm_wday;
	rtc_tm.tm_yday  = new_broken_time->tm_yday;
	rtc_tm.tm_isdst = new_broken_time->tm_isdst;

	rc = ioctl(rtc_fd, RTC_SET_TIME, &rtc_tm);

	if (rc == -1) {
		warn(_("ioctl(RTC_SET_TIME) to %s to set the time failed"),
			rtc_dev_name);
		hwclock_exit(ctl, EXIT_FAILURE);
	}

	if (ctl->verbose)
		printf(_("ioctl(RTC_SET_TIME) was successful.\n"));

	return 0;
}

static int get_permissions_rtc(void)
{
	return 0;
}

static const char *get_device_path(void)
{
	return rtc_dev_name;
}

static const struct clock_ops rtc_interface = {
	N_("Using the rtc interface to the clock."),
	get_permissions_rtc,
	read_hardware_clock_rtc,
	set_hardware_clock_rtc,
	synchronize_to_clock_tick_rtc,
	get_device_path,
};

/* return &rtc if /dev/rtc can be opened, NULL otherwise */
const struct clock_ops *probe_for_rtc_clock(const struct hwclock_control *ctl)
{
	const int rtc_fd = open_rtc(ctl);

	if (rtc_fd < 0)
		return NULL;
	return &rtc_interface;
}

#ifdef __alpha__
/*
 * Get the Hardware Clock epoch setting from the kernel.
 */
int get_epoch_rtc(const struct hwclock_control *ctl, unsigned long *epoch_p)
{
	int rtc_fd;

	rtc_fd = open_rtc(ctl);
	if (rtc_fd < 0) {
		warn(_("cannot open %s"), rtc_dev_name);
		return 1;
	}

	if (ioctl(rtc_fd, RTC_EPOCH_READ, epoch_p) == -1) {
		warn(_("ioctl(%d, RTC_EPOCH_READ, epoch_p) to %s failed"),
		     rtc_fd, rtc_dev_name);
		return 1;
	}

	if (ctl->verbose)
		printf(_("ioctl(%d, RTC_EPOCH_READ, epoch_p) to %s succeeded.\n"),
		       rtc_fd, rtc_dev_name);

	return 0;
}

/*
 * Set the Hardware Clock epoch in the kernel.
 */
int set_epoch_rtc(const struct hwclock_control *ctl)
{
	int rtc_fd;
	unsigned long epoch;

	errno = 0;
	epoch = strtoul(ctl->epoch_option, NULL, 10);

	/* There were no RTC clocks before 1900. */
	if (errno || epoch < 1900 || epoch == ULONG_MAX) {
		warnx(_("invalid epoch '%s'."), ctl->epoch_option);
		return 1;
	}

	rtc_fd = open_rtc(ctl);
	if (rtc_fd < 0) {
		warn(_("cannot open %s"), rtc_dev_name);
		return 1;
	}

	if (ioctl(rtc_fd, RTC_EPOCH_SET, epoch) == -1) {
		warn(_("ioctl(%d, RTC_EPOCH_SET, %lu) to %s failed"),
		     rtc_fd, epoch, rtc_dev_name);
		return 1;
	}

	if (ctl->verbose)
		printf(_("ioctl(%d, RTC_EPOCH_SET, %lu) to %s succeeded.\n"),
		       rtc_fd, epoch, rtc_dev_name);

	return 0;
}
#endif	/* __alpha__ */



static int resolve_rtc_param_alias(const char *alias, __u64 *value)
{
	const struct hwclock_param *param = &hwclock_params[0];

	while (param->name) {
		if (!strcmp(alias, param->name)) {
			*value = param->id;
			return 0;
		}
		param++;
	}

	return 1;
}

/* kernel uapi __u64 can be defined differently than uint64_t */
static int strtoku64(const char *str, __u64 *num, int base)
{
	return ul_strtou64(str, (uint64_t *) num, base);
}

/*
 * Get the Hardware Clock parameter setting from the kernel.
 */
int get_param_rtc(const struct hwclock_control *ctl,
		  const char *name, uint64_t *id, uint64_t *value)
{
	int rtc_fd;
	struct rtc_param param = { .param = 0 };

	/* handle name */
	if (resolve_rtc_param_alias(name, &param.param) != 0
	    && strtoku64(name, &param.param, 0) != 0) {
		warnx(_("could not convert parameter name to number"));
		return 1;
	}

	/* get parameter */
	rtc_fd = open_rtc(ctl);
	if (rtc_fd < 0) {
		warn(_("cannot open %s"), rtc_dev_name);
		return 1;
	}

	if (ioctl(rtc_fd, RTC_PARAM_GET, &param) == -1) {
		warn(_("ioctl(%d, RTC_PARAM_GET, param) to %s failed"),
		     rtc_fd, rtc_dev_name);
		return 1;
	}

	if (id)
		*id = param.param;
	if (value)
		*value = param.uvalue;

	if (ctl->verbose)
		printf(_("ioctl(%d, RTC_PARAM_GET, param) to %s succeeded.\n"),
		       rtc_fd, rtc_dev_name);

	return 0;
}

/*
 * Set the Hardware Clock parameter in the kernel.
 */
int set_param_rtc(const struct hwclock_control *ctl, const char *opt0)
{
	int rtc_fd, rc = 1;
	struct rtc_param param = { .param = 0 };
	char *tok, *opt = xstrdup(opt0);

	/* handle name */
	tok = strtok(opt, "=");
	if (resolve_rtc_param_alias(tok, &param.param) != 0
	    && strtoku64(tok, &param.param, 0) != 0) {
		warnx(_("could not convert parameter name to number"));
		goto done;
	}

	/* handle value */
	tok = strtok(NULL, "=");
	if (!tok) {
		warnx(_("expected <param>=<value>"));
		goto done;
	}
	if (strtoku64(tok, &param.uvalue, 0) != 0) {
		warnx(_("could not convert parameter value to number"));
		goto done;
	}

	/* set parameter */
	rtc_fd = open_rtc(ctl);
	if (rtc_fd < 0) {
		warnx(_("cannot open %s"), rtc_dev_name);
		goto done;
	}

	if (ioctl(rtc_fd, RTC_PARAM_SET, &param) == -1) {
		warn(_("ioctl(%d, RTC_PARAM_SET, param) to %s failed"),
		     rtc_fd, rtc_dev_name);
		goto done;
	}

	if (ctl->verbose)
		printf(_("ioctl(%d, RTC_PARAM_SET, param) to %s succeeded.\n"),
		       rtc_fd, rtc_dev_name);

	rc = 0;
done:
	free(opt);
	return rc;
}

#ifndef RTC_VL_DATA_INVALID
#define RTC_VL_DATA_INVALID     0x1
#endif
#ifndef RTC_VL_BACKUP_LOW
#define RTC_VL_BACKUP_LOW       0x2
#endif
#ifndef RTC_VL_BACKUP_EMPTY
#define RTC_VL_BACKUP_EMPTY     0x4
#endif
#ifndef RTC_VL_ACCURACY_LOW
#define RTC_VL_ACCURACY_LOW     0x8
#endif
#ifndef RTC_VL_BACKUP_SWITCH
#define RTC_VL_BACKUP_SWITCH    0x10
#endif

int rtc_vl_read(const struct hwclock_control *ctl)
{
	unsigned int vl;
	int rtc_fd;
	size_t i;
	static const struct vl_bit {
		unsigned int bit;
		const char *desc;
	} vl_bits[] = {
		{ RTC_VL_DATA_INVALID,  N_("Voltage too low, RTC data is invalid") },
		{ RTC_VL_BACKUP_LOW,    N_("Backup voltage is low") },
		{ RTC_VL_BACKUP_EMPTY,  N_("Backup empty or not present") },
		{ RTC_VL_ACCURACY_LOW,  N_("Voltage is low, RTC accuracy is reduced") },
		{ RTC_VL_BACKUP_SWITCH, N_("Backup switchover happened") },
	};

	rtc_fd = open_rtc(ctl);
	if (rtc_fd < 0) {
		warnx(_("cannot open %s"), rtc_dev_name);
		return 1;
	}

	if (ioctl(rtc_fd, RTC_VL_READ, &vl) == -1) {
		warn(_("ioctl(%d, RTC_VL_READ) on %s failed"),
		     rtc_fd, rtc_dev_name);
		return 1;
	}

	if (ctl->verbose) {
		printf(_("ioctl(%d, RTC_VL_READ) on %s returned 0x%x\n"),
		       rtc_fd, rtc_dev_name, vl);
	}

	for (i = 0; i < ARRAY_SIZE(vl_bits); ++i) {
		const struct vl_bit *vlb = &vl_bits[i];

		if (vl & vlb->bit) {
			printf("0x%02x - %s\n", vlb->bit, vlb->desc);
			vl &= ~vlb->bit;
		}
	}
	if (vl)
		printf("0x%02x - unknown bit(s)\n", vl);

	return 0;
}

int rtc_vl_clear(const struct hwclock_control *ctl)
{
	int rtc_fd;

	rtc_fd = open_rtc(ctl);
	if (rtc_fd < 0) {
		warnx(_("cannot open %s"), rtc_dev_name);
		return 1;
	}

	if (ioctl(rtc_fd, RTC_VL_CLR) == -1) {
		warn(_("ioctl(%d, RTC_VL_CLEAR) on %s failed"),
		     rtc_fd, rtc_dev_name);
		return 1;
	}

	if (ctl->verbose)
		printf(_("ioctl(%d, RTC_VL_CLEAR) on %s succeeded.\n"),
		       rtc_fd, rtc_dev_name);

	return 0;
}
