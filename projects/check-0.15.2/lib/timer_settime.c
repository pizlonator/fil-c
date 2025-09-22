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

int timer_settime(timer_t timerid CK_ATTRIBUTE_UNUSED,
                  int flags CK_ATTRIBUTE_UNUSED,
                  const struct itimerspec *new_value,
                  struct itimerspec *old_value CK_ATTRIBUTE_UNUSED)
{
#ifdef HAVE_SETITIMER
    /*
     * If the system does not have timer_settime() but does have
     * setitimer() use that instead of alarm().
     */
    struct itimerval interval;

    interval.it_value.tv_sec = new_value->it_value.tv_sec;
    interval.it_value.tv_usec = new_value->it_value.tv_nsec / 1000;
    interval.it_interval.tv_sec = new_value->it_interval.tv_sec;
    interval.it_interval.tv_usec = new_value->it_interval.tv_nsec / 1000;

    return setitimer(ITIMER_REAL, &interval, NULL);
#else
    int seconds = new_value->it_value.tv_sec;

    /* 
     * As the alarm() call has only second precision, if the caller
     * specifies partial seconds, we round up to the nearest second.
     */
    if(new_value->it_value.tv_nsec > 0)
    {
        seconds += 1;
    }

    alarm(seconds);

    return 0;
#endif
}
