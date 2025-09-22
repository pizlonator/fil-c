/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *
 * i386 CMOS starts out with 14 bytes clock data alpha has something
 * similar, but with details depending on the machine type.
 *
 * byte 0: seconds		0-59
 * byte 2: minutes		0-59
 * byte 4: hours		0-23 in 24hr mode,
 *				1-12 in 12hr mode, with high bit unset/set
 *					if am/pm.
 * byte 6: weekday		1-7, Sunday=1
 * byte 7: day of the month	1-31
 * byte 8: month		1-12
 * byte 9: year			0-99
 *
 * Numbers are stored in BCD/binary if bit 2 of byte 11 is unset/set The
 * clock is in 12hr/24hr mode if bit 1 of byte 11 is unset/set The clock is
 * undefined (being updated) if bit 7 of byte 10 is set. The clock is frozen
 * (to be updated) by setting bit 7 of byte 11 Bit 7 of byte 14 indicates
 * whether the CMOS clock is reliable: it is 1 if RTC power has been good
 * since this bit was last read; it is 0 when the battery is dead and system
 * power has been off.
 *
 * Avoid setting the RTC clock within 2 seconds of the day rollover that
 * starts a new month or enters daylight saving time.
 *
 * The century situation is messy:
 *
 * Usually byte 50 (0x32) gives the century (in BCD, so 19 or 20 hex), but
 * IBM PS/2 has (part of) a checksum there and uses byte 55 (0x37).
 * Sometimes byte 127 (0x7f) or Bank 1, byte 0x48 gives the century. The
 * original RTC will not access any century byte; some modern versions will.
 * If a modern RTC or BIOS increments the century byte it may go from 0x19
 * to 0x20, but in some buggy cases 0x1a is produced.
 */
/*
 * A struct tm has int fields
 *   tm_sec	0-59, 60 or 61 only for leap seconds
 *   tm_min	0-59
 *   tm_hour	0-23
 *   tm_mday	1-31
 *   tm_mon	0-11
 *   tm_year	number of years since 1900
 *   tm_wday	0-6, 0=Sunday
 *   tm_yday	0-365
 *   tm_isdst	>0: yes, 0: no, <0: unknown
 */

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "c.h"
#include "nls.h"
#include "pathnames.h"

/* for inb, outb */
#ifdef HAVE_SYS_IO_H
# include <sys/io.h>
#elif defined(HAVE_ASM_IO_H)
# include <asm/io.h>
#else
# error "no sys/io.h or asm/io.h"
#endif	/* HAVE_SYS_IO_H, HAVE_ASM_IO_H */

#include "hwclock.h"

#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)
#define BIN_TO_BCD(val) ((val)=(((val)/10)<<4) + (val)%10)

#define IOPL_NOT_IMPLEMENTED -2

/*
 * POSIX uses 1900 as epoch for a struct tm, and 1970 for a time_t.
 */
#define TM_EPOCH 1900

static unsigned short clock_ctl_addr = 0x70;
static unsigned short clock_data_addr = 0x71;

/*
 * Hmmh, this isn't very atomic. Maybe we should force an error instead?
 *
 * TODO: optimize the access to CMOS by mlockall(MCL_CURRENT) and SCHED_FIFO
 */
static unsigned long atomic(unsigned long (*op) (unsigned long),
			    unsigned long arg)
{
	return (*op) (arg);
}

/*
 * We only want to read CMOS data, but unfortunately writing to bit 7
 * disables (1) or enables (0) NMI; since this bit is read-only we have
 * to guess the old status. Various docs suggest that one should disable
 * NMI while reading/writing CMOS data, and enable it again afterwards.
 * This would yield the sequence
 *
 *  outb (reg | 0x80, 0x70);
 *  val = inb(0x71);
 *  outb (0x0d, 0x70);  // 0x0d: random read-only location
 *
 * Other docs state that "any write to 0x70 should be followed by an
 * action to 0x71 or the RTC will be left in an unknown state". Most
 * docs say that it doesn't matter at all what one does.
 *
 * bit 0x80: disable NMI while reading - should we? Let us follow the
 * kernel and not disable. Called only with 0 <= reg < 128
 */

static inline unsigned long cmos_read(unsigned long reg)
{
	outb(reg, clock_ctl_addr);
	return inb(clock_data_addr);
}

static inline unsigned long cmos_write(unsigned long reg, unsigned long val)
{
	outb(reg, clock_ctl_addr);
	outb(val, clock_data_addr);
	return 0;
}

static unsigned long cmos_set_time(unsigned long arg)
{
	unsigned char save_control, save_freq_select, pmbit = 0;
	struct tm tm = *(struct tm *)arg;

/*
 * CMOS byte 10 (clock status register A) has 3 bitfields:
 * bit 7: 1 if data invalid, update in progress (read-only bit)
 *         (this is raised 224 us before the actual update starts)
 *  6-4    select base frequency
 *         010: 32768 Hz time base (default)
 *         111: reset
 *         all other combinations are manufacturer-dependent
 *         (e.g.: DS1287: 010 = start oscillator, anything else = stop)
 *  3-0    rate selection bits for interrupt
 *         0000 none (may stop RTC)
 *         0001, 0010 give same frequency as 1000, 1001
 *         0011 122 microseconds (minimum, 8192 Hz)
 *         .... each increase by 1 halves the frequency, doubles the period
 *         1111 500 milliseconds (maximum, 2 Hz)
 *         0110 976.562 microseconds (default 1024 Hz)
 */
	save_control = cmos_read(11);	/* tell the clock it's being set */
	cmos_write(11, (save_control | 0x80));
	save_freq_select = cmos_read(10);	/* stop and reset prescaler */
	cmos_write(10, (save_freq_select | 0x70));

	tm.tm_year %= 100;
	tm.tm_mon += 1;
	tm.tm_wday += 1;

	if (!(save_control & 0x02)) {	/* 12hr mode; the default is 24hr mode */
		if (tm.tm_hour == 0)
			tm.tm_hour = 24;
		if (tm.tm_hour > 12) {
			tm.tm_hour -= 12;
			pmbit = 0x80;
		}
	}

	if (!(save_control & 0x04)) {	/* BCD mode - the default */
		BIN_TO_BCD(tm.tm_sec);
		BIN_TO_BCD(tm.tm_min);
		BIN_TO_BCD(tm.tm_hour);
		BIN_TO_BCD(tm.tm_wday);
		BIN_TO_BCD(tm.tm_mday);
		BIN_TO_BCD(tm.tm_mon);
		BIN_TO_BCD(tm.tm_year);
	}

	cmos_write(0, tm.tm_sec);
	cmos_write(2, tm.tm_min);
	cmos_write(4, tm.tm_hour | pmbit);
	cmos_write(6, tm.tm_wday);
	cmos_write(7, tm.tm_mday);
	cmos_write(8, tm.tm_mon);
	cmos_write(9, tm.tm_year);

	/*
	 * The kernel sources, linux/arch/i386/kernel/time.c, have the
	 * following comment:
	 *
	 * The following flags have to be released exactly in this order,
	 * otherwise the DS12887 (popular MC146818A clone with integrated
	 * battery and quartz) will not reset the oscillator and will not
	 * update precisely 500 ms later. You won't find this mentioned in
	 * the Dallas Semiconductor data sheets, but who believes data
	 * sheets anyway ... -- Markus Kuhn
	 */
	cmos_write(11, save_control);
	cmos_write(10, save_freq_select);
	return 0;
}

static int hclock_read(unsigned long reg)
{
	return atomic(cmos_read, reg);
}

static void hclock_set_time(const struct tm *tm)
{
	atomic(cmos_set_time, (unsigned long)(tm));
}

static inline int cmos_clock_busy(void)
{
	return
	    /* poll bit 7 (UIP) of Control Register A */
	    (hclock_read(10) & 0x80);
}

static int synchronize_to_clock_tick_cmos(const struct hwclock_control *ctl
					  __attribute__((__unused__)))
{
	int i;

	/*
	 * Wait for rise. Should be within a second, but in case something
	 * weird happens, we have a limit on this loop to reduce the impact
	 * of this failure.
	 */
	for (i = 0; !cmos_clock_busy(); i++)
		if (i >= 10000000)
			return 1;

	/* Wait for fall.  Should be within 2.228 ms. */
	for (i = 0; cmos_clock_busy(); i++)
		if (i >= 1000000)
			return 1;
	return 0;
}

/*
 * Read the hardware clock and return the current time via <tm> argument.
 * Assume we have an ISA machine and read the clock directly with CPU I/O
 * instructions.
 *
 * This function is not totally reliable.  It takes a finite and
 * unpredictable amount of time to execute the code below. During that time,
 * the clock may change and we may even read an invalid value in the middle
 * of an update. We do a few checks to minimize this possibility, but only
 * the kernel can actually read the clock properly, since it can execute
 * code in a short and predictable amount of time (by turning of
 * interrupts).
 *
 * In practice, the chance of this function returning the wrong time is
 * extremely remote.
 */
static int read_hardware_clock_cmos(const struct hwclock_control *ctl
				    __attribute__((__unused__)), struct tm *tm)
{
	unsigned char status = 0, pmbit = 0;

	while (1) {
		/*
		 * Bit 7 of Byte 10 of the Hardware Clock value is the
		 * Update In Progress (UIP) bit, which is on while and 244
		 * uS before the Hardware Clock updates itself. It updates
		 * the counters individually, so reading them during an
		 * update would produce garbage. The update takes 2mS, so we
		 * could be spinning here that long waiting for this bit to
		 * turn off.
		 *
		 * Furthermore, it is pathologically possible for us to be
		 * in this code so long that even if the UIP bit is not on
		 * at first, the clock has changed while we were running. We
		 * check for that too, and if it happens, we start over.
		 */
		if (!cmos_clock_busy()) {
			/* No clock update in progress, go ahead and read */
			tm->tm_sec = hclock_read(0);
			tm->tm_min = hclock_read(2);
			tm->tm_hour = hclock_read(4);
			tm->tm_wday = hclock_read(6);
			tm->tm_mday = hclock_read(7);
			tm->tm_mon = hclock_read(8);
			tm->tm_year = hclock_read(9);
			status = hclock_read(11);
			/*
			 * Unless the clock changed while we were reading,
			 * consider this a good clock read .
			 */
			if (tm->tm_sec == hclock_read(0))
				break;
		}
		/*
		 * Yes, in theory we could have been running for 60 seconds
		 * and the above test wouldn't work!
		 */
	}

	if (!(status & 0x04)) {	/* BCD mode - the default */
		BCD_TO_BIN(tm->tm_sec);
		BCD_TO_BIN(tm->tm_min);
		pmbit = (tm->tm_hour & 0x80);
		tm->tm_hour &= 0x7f;
		BCD_TO_BIN(tm->tm_hour);
		BCD_TO_BIN(tm->tm_wday);
		BCD_TO_BIN(tm->tm_mday);
		BCD_TO_BIN(tm->tm_mon);
		BCD_TO_BIN(tm->tm_year);
	}

	/*
	 * We don't use the century byte of the Hardware Clock since we
	 * don't know its address (usually 50 or 55). Here, we follow the
	 * advice of the X/Open Base Working Group: "if century is not
	 * specified, then values in the range [69-99] refer to years in the
	 * twentieth century (1969 to 1999 inclusive), and values in the
	 * range [00-68] refer to years in the twenty-first century (2000 to
	 * 2068 inclusive)."
	 */
	tm->tm_wday -= 1;
	tm->tm_mon -= 1;
	if (tm->tm_year < 69)
		tm->tm_year += 100;
	if (pmbit) {
		tm->tm_hour += 12;
		if (tm->tm_hour == 24)
			tm->tm_hour = 0;
	}

	tm->tm_isdst = -1;	/* don't know whether it's daylight */
	return 0;
}

static int set_hardware_clock_cmos(const struct hwclock_control *ctl
				   __attribute__((__unused__)),
				   const struct tm *new_broken_time)
{
	hclock_set_time(new_broken_time);
	return 0;
}

# if defined(HAVE_IOPL)
static int i386_iopl(const int level)
{
	return iopl(level);
}
# else
static int i386_iopl(const int level __attribute__ ((__unused__)))
{
	extern int ioperm(unsigned long from, unsigned long num, int turn_on);
	return ioperm(clock_ctl_addr, 2, 1);
}
# endif

static int get_permissions_cmos(void)
{
	int rc;

	rc = i386_iopl(3);
	if (rc == IOPL_NOT_IMPLEMENTED) {
		warnx(_("ISA port access is not implemented"));
	} else if (rc != 0) {
		warn(_("iopl() port access failed"));
	}
	return rc;
}

static const char *get_device_path(void)
{
	return NULL;
}

static const struct clock_ops cmos_interface = {
	N_("Using direct ISA access to the clock"),
	get_permissions_cmos,
	read_hardware_clock_cmos,
	set_hardware_clock_cmos,
	synchronize_to_clock_tick_cmos,
	get_device_path,
};

/*
 * return &cmos if cmos clock present, NULL otherwise.
 */
const struct clock_ops *probe_for_cmos_clock(void)
{
	return &cmos_interface;
}
