/*
 * Check: a unit test framework for C
 * Copyright (C) 2001, 2002 Arien Malec
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "libcompat.h"

int timer_delete(timer_t timerid CK_ATTRIBUTE_UNUSED)
{
#ifdef HAVE_SETITIMER
    /*
     * If the system does not have timer_settime() but does have
     * setitimer() use that instead of alarm().
     */
    struct itimerval interval;

    /*
     * Setting values to '0' results in disabling the running timer.
     */
    interval.it_value.tv_sec = 0;
    interval.it_value.tv_usec = 0;
    interval.it_interval.tv_sec = 0;
    interval.it_interval.tv_usec = 0;

    return setitimer(ITIMER_REAL, &interval, NULL);
#else
    /*
     * There is only one timer, that used by alarm.
     * Setting alarm(0) will not set a new alarm, and
     * will kill the previous timer.
     */

    alarm(0);

    return 0;
#endif
}
