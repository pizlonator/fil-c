/* uptime.c - Tell how long the system has been running.
 *
 * Copyright 2012 Elie De Brauwer <eliedebrauwer@gmail.com>
 * Copyright 2012 Luis Felipe Strano Moraes <lfelipe@profusion.mobi>
 * Copyright 2013 Jeroen van Rijn <jvrnix@gmail.com>

USE_UPTIME(NEWTOY(uptime, ">0ps", TOYFLAG_USR|TOYFLAG_BIN))

config UPTIME
  bool "uptime"
  default y
  help
    usage: uptime [-ps]

    Tell the current time, how long the system has been running, the number
    of users, and the system load averages for the past 1, 5 and 15 minutes.

    -p	Pretty (human readable) uptime
    -s	Since when has the system been up?
*/

#define FOR_uptime
#include "toys.h"

void uptime_main(void)
{
  struct sysinfo info;
  time_t t;
  struct tm *tm;
  unsigned int weeks, days, hours, minutes;
  struct utmpx *entry;
  int users = 0;

  // Obtain the data we need.
  sysinfo(&info);
  time(&t);

  // Just show the time of boot?
  if (FLAG(s)) {
    t -= info.uptime;
    tm = localtime(&t);
    strftime(toybuf, sizeof(toybuf), "%F %T", tm);
    xputs(toybuf);
    return;
  }

  // Current time
  tm = localtime(&t);
  // Uptime
  info.uptime /= 60;
  minutes = info.uptime%60;
  info.uptime /= 60;
  hours = info.uptime%24;
  days = info.uptime/24;

  if (FLAG(p)) {
    weeks = days/7;
    days %= 7;
    xprintf("up %d week%s, %d day%s, %d hour%s, %d minute%s, ",
        weeks, (weeks!=1)?"s":"",
        days, (days!=1)?"s":"",
        hours, (hours!=1)?"s":"",
        minutes, (minutes!=1)?"s":"");
  } else {
    xprintf(" %02d:%02d:%02d up ", tm->tm_hour, tm->tm_min, tm->tm_sec);
    if (days) xprintf("%d day%s, ", days, (days!=1)?"s":"");
    if (hours) xprintf("%2d:%02d, ", hours, minutes);
    else printf("%d min, ", minutes);

    // Obtain info about logged on users
    setutxent();
    while ((entry = getutxent())) if (entry->ut_type == USER_PROCESS) users++;
    endutxent();
    printf(" %d user%s, ", users, (users!=1) ? "s" : "");
  }

  printf(" load average: %.02f, %.02f, %.02f\n", info.loads[0]/65536.0,
    info.loads[1]/65536.0, info.loads[2]/65536.0);
}
