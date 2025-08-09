/* hwclock.c - get and set the hwclock
 *
 * Copyright 2014 Bilal Qureshi <bilal.jmi@gmail.com>
 *
 * No standard, but see Documentation/rtc.txt in the linux kernel source.
 *
 * TODO: get/set subsecond time
USE_HWCLOCK(NEWTOY(hwclock, ">0(fast)f(rtc):u(utc)l(localtime)t(systz)s(hctosys)r(show)w(systohc)[-ul][!rtsw]", TOYFLAG_SBIN))

config HWCLOCK
  bool "hwclock"
  default y
  help
    usage: hwclock [-rswtlu] [-f FILE]

    Get/set the hardware clock. Default is hwclock -ruf /dev/rtc0

    -f	Use specified device FILE instead of /dev/rtc0 (--rtc)
    -l	Hardware clock uses localtime (--localtime)
    -r	Show hardware clock time (--show)
    -s	Set system time from hardware clock (--hctosys)
    -t	Inform kernel of non-UTC clock's timezone so it returns UTC (--systz)
    -u	Hardware clock uses UTC (--utc)
    -w	Set hardware clock from system time (--systohc)
*/


// Bug workaround for musl commit 5a105f19b5aa which removed a symbol the
// kernel headers have. (Can't copy it here, varies wildly by architecture.)
#if __has_include(<asm/unistd.h>)
#include <asm/unistd.h>
#endif

#define FOR_hwclock
#include "toys.h"
#include <linux/rtc.h>

GLOBALS(
  char *f;
)

// Bug workaround for musl commit 2c2c3605d3b3 which rewrote the syscall
// wrapper to not use the syscall, which is the only way to set kernel's sys_tz
#ifdef _NR_settimeofday
#define settimeofday(x, tz) syscall(__NR_settimeofday, (void *)0, (void *)tz)
#else
#define settimeofday(x, tz) ((tz)->tz_minuteswest = 0)
#endif

void hwclock_main()
{
  struct timezone tz = {0};
  struct timespec ts = {0};
  struct tm tm;
  int fd = -1;

  // -t without -u implies -l
  if (FLAG(t)&&!FLAG(u)) toys.optflags |= FLAG_l;
  if (FLAG(l)) {
    // sets globals timezone and daylight from sys/time.h
    // Handle dst adjustment ourselves. (Rebooting during dst transition is
    // just conceptually unpleasant, linux uses UTC for a reason.)
    tzset();
    tz.tz_minuteswest = timezone/60 - 60*daylight;
  }

  if (!FLAG(t)) {
    fd = xopen(TT.f ? : "/dev/rtc0", O_WRONLY*FLAG(w));

    // Get current time in seconds from rtc device.
    if (!FLAG(w)) {
      xioctl(fd, RTC_RD_TIME, &tm);
      ts.tv_sec = xmktime(&tm, !FLAG(l));
    }
  }

  if (FLAG(w) || FLAG(t)) {
    if (FLAG(w)) {
      if (clock_gettime(CLOCK_REALTIME, &ts)) perror_exit("clock_gettime");
      if (!(FLAG(l) ? localtime_r : gmtime_r)(&ts.tv_sec, &tm))
        error_exit("%s failed", FLAG(l) ? "localtime_r" : "gmtime_r");
      xioctl(fd, RTC_SET_TIME, &tm);
    }
    if (settimeofday(0, &tz)) perror_exit("settimeofday");
  } else if (FLAG(s)) {
    if (clock_settime(CLOCK_REALTIME, &ts)) perror_exit("clock_settime");
  } else {
    strftime(toybuf, sizeof(toybuf), "%F %T%z", &tm);
    xputs(toybuf);
  }

  if (CFG_TOYBOX_FREE) xclose(fd);
}
