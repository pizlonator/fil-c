/*
 * uptime - uptime related functions - part of libproc2
 *
 * Copyright © 2015-2023 Craig Small <csmall@dropbear.xyz>
 * Copyright © 2015-2023 Jim Warner <james.warner@comcast.net>
 * Copyright © 1998-2003 Albert Cahalan
 * Copyright © 1992-1998 Michael K. Johnson <johnsonm@redhat.com>
 * Copyright © 1993      J. Cowley
 * Copyright © ????      Larry Greenfield <greenfie@gauss.rutgers.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>
#ifdef WITH_SYSTEMD
#include <systemd/sd-daemon.h>
#include <systemd/sd-login.h>
#endif
#ifdef WITH_ELOGIND
#include <elogind/sd-daemon.h>
#include <elogind/sd-login.h>
#endif

#include "misc.h"
#include "procps-private.h"

#define UPTIME_FILE "/proc/uptime"

static __thread char upbuf[256];
static __thread char shortbuf[256];

static int count_users(void)
{
    int numuser = 0;
    struct utmp *ut;

#if defined(WITH_SYSTEMD) || defined(WITH_ELOGIND)
    if (sd_booted() > 0)
      return sd_get_sessions(NULL);
#endif

    setutent();
    while ((ut = getutent())) {
    if ((ut->ut_type == USER_PROCESS) && (ut->ut_name[0] != '\0'))
        numuser++;
    }
    endutent();

    return numuser;
}

/*
 * uptime:
 *
 * Find the uptime and idle time of the system.
 * These numbers are found in /proc/uptime
 * Unlike other procps functions this closes the file each time
 * Either uptime_secs or idle_secs can be null
 *
 * Returns: 0 on success and <0 on failure
 */
PROCPS_EXPORT int procps_uptime(
        double *restrict uptime_secs,
        double *restrict idle_secs)
{
    double up=0, idle=0;
    locale_t tmplocale;
    FILE *fp;
    int rc;

    if ((fp = fopen(UPTIME_FILE, "r")) == NULL)
        return -errno;

    tmplocale = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
    uselocale(tmplocale);
    rc = fscanf(fp, "%lf %lf", &up, &idle);
    fclose(fp);
    uselocale(LC_GLOBAL_LOCALE);
    freelocale(tmplocale);

    if (uptime_secs)
        *uptime_secs = up;
    if (idle_secs)
        *idle_secs = idle;

    if (rc < 2)
        return -ERANGE;
    return 0;
}

/*
 * procps_uptime_sprint:
 *
 * Print current time in nice format
 *
 * Returns a statically allocated upbuf or NULL on error
 */
PROCPS_EXPORT char *procps_uptime_sprint(void)
{
    int upminutes, uphours, updays, users;
    int pos;
    time_t realseconds;
    struct tm realtime;
    double uptime_secs, idle_secs;
    double av1, av5, av15;

    upbuf[0] = '\0';
    if (time(&realseconds) < 0)
        return upbuf;
    localtime_r(&realseconds, &realtime);

    if (procps_uptime(&uptime_secs, &idle_secs) < 0)
        return upbuf;

    updays  =   ((int) uptime_secs / (60*60*24));
    uphours =   ((int) uptime_secs / (60*60)) % 24;
    upminutes = ((int) uptime_secs / (60)) % 60;

    pos = sprintf(upbuf, " %02d:%02d:%02d up ",
        realtime.tm_hour, realtime.tm_min, realtime.tm_sec);

    if (updays)
        pos += sprintf(upbuf + pos, "%d %s, ", updays, (updays > 1) ? "days" : "day");

    if (uphours)
        pos += sprintf(upbuf + pos, "%2d:%02d, ", uphours, upminutes);
    else
        pos += sprintf(upbuf + pos, "%d min, ", upminutes);

    users = count_users();
    procps_loadavg(&av1, &av5, &av15);

    if (users < 0)
      pos += sprintf(upbuf + pos, " ? ");
    else
      pos += sprintf(upbuf + pos, "%2d ", users);

    pos += sprintf(upbuf + pos, "%s,  load average: %.2f, %.2f, %.2f",
        users > 1 ? "users" : "user",
        av1, av5, av15);

    return upbuf;
}

/*
 * procps_uptime_sprint_short:
 *
 * Print current time in nice format
 *
 * Returns a statically allocated buffer or NULL on error
 */
PROCPS_EXPORT char *procps_uptime_sprint_short(void)
{
    int updecades, upyears, upweeks, updays, uphours = 0, upminutes = 0;
    int pos = 3;
    int comma = 0;
    double uptime_secs, idle_secs;

    shortbuf[0] = '\0';
    if (procps_uptime(&uptime_secs, &idle_secs) < 0)
        return shortbuf;

    if (uptime_secs>60*60*24*365*10) {
      updecades = (int) uptime_secs / (60*60*24*365*10);
      uptime_secs -= updecades*60*60*24*365*10;
    }
    else {
      updecades = 0;
    }
    if (uptime_secs>60*60*24*365) {
      upyears = (int) uptime_secs / (60*60*24*365);
      uptime_secs -= upyears*60*60*24*365;
    }
    else {
      upyears = 0;
    }
    if (uptime_secs>60*60*24*7) {
      upweeks = (int) uptime_secs / (60*60*24*7);
      uptime_secs -= upweeks*60*60*24*7;
    }
    else {
      upweeks = 0;
    }
    if (uptime_secs>60*60*24) {
      updays = (int) uptime_secs / (60*60*24);
      uptime_secs -= updays*60*60*24;
    }
    else {
      updays = 0;
    }
    if (uptime_secs>60*60) {
        uphours = (int) uptime_secs / (60*60);
        uptime_secs -= uphours*60*60;
    }
    if (uptime_secs>60) {
        upminutes = (int) uptime_secs / 60;
        uptime_secs -= upminutes*60;
    }
    /*updecades =  (int) uptime_secs / (60*60*24*365*10);
    upyears =   ((int) uptime_secs / (60*60*24*365)) % 10;
    upweeks =   ((int) uptime_secs / (60*60*24*7)) % 52;
    updays  =   ((int) uptime_secs / (60*60*24)) % 7;
    uphours =   ((int) uptime_secs / (60*60)) % 24;
    upminutes = ((int) uptime_secs / (60)) % 60;
*/
    strcat(shortbuf, "up ");

    if (updecades) {
        pos += sprintf(shortbuf + pos, "%d %s",
                       updecades, updecades > 1 ? "decades" : "decade");
        comma += 1;
    }

    if (upyears) {
        pos += sprintf(shortbuf + pos, "%s%d %s",
                       comma > 0 ? ", " : "", upyears,
                       upyears > 1 ? "years" : "year");
        comma += 1;
    }

    if (upweeks) {
        pos += sprintf(shortbuf + pos, "%s%d %s",
                       comma  > 0 ? ", " : "", upweeks,
                       upweeks > 1 ? "weeks" : "week");
        comma += 1;
    }

    if (updays) {
        pos += sprintf(shortbuf + pos, "%s%d %s",
                       comma  > 0 ? ", " : "", updays,
                       updays > 1 ? "days" : "day");
        comma += 1;
    }

    if (uphours) {
        pos += sprintf(shortbuf + pos, "%s%d %s",
                       comma  > 0 ? ", " : "", uphours,
                       uphours > 1 ? "hours" : "hour");
        comma += 1;
    }

    if (upminutes || (!upminutes && uptime_secs < 60)) {
        pos += sprintf(shortbuf + pos, "%s%d %s",
                       comma > 0 ? ", " : "", upminutes,
                       upminutes != 1 ? "minutes" : "minute");
        comma += 1;
    }
    return shortbuf;
}
