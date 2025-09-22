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

#ifdef __APPLE__
#include <mach/clock.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <CoreServices/CoreServices.h>
#include <unistd.h>
#endif

#define NANOSECONDS_PER_SECOND 1000000000



int clock_gettime(clockid_t clk_id CK_ATTRIBUTE_UNUSED, struct timespec *ts)
{

#ifdef __APPLE__
    /* Some versions of macOS and iOS do not have clock_gettime, use
     * mach_absolute_time */

    static mach_timebase_info_data_t sTimebaseInfo;
    uint64_t rawTime;
    uint64_t nanos;

    rawTime = mach_absolute_time();

    /*
     * OS X has a function to convert abs time to nano seconds: AbsoluteToNanoseconds
     * However, the function may not be available as we may not have
     * access to CoreServices. Because of this, we convert the abs time
     * to nano seconds manually.
     */

    /*
     * First grab the time base used on the system, if this is the first
     * time we are being called. We can check if the value is uninitialized,
     * as the denominator will be zero. 
     */
    if(sTimebaseInfo.denom == 0)
    {
        (void)mach_timebase_info(&sTimebaseInfo);
    }

    /* 
     * Do the conversion. We hope that the multiplication doesn't 
     * overflow; the price you pay for working in fixed point.
     */
    nanos = rawTime * sTimebaseInfo.numer / sTimebaseInfo.denom;

    /* 
     * Fill in the timespec container 
     */
    ts->tv_sec = nanos / NANOSECONDS_PER_SECOND;
    ts->tv_nsec = nanos - (ts->tv_sec * NANOSECONDS_PER_SECOND);
#else
    /* 
     * As there is no function to fall back onto to get the current
     * time, zero out the time so the caller will have a sane value. 
     */
    ts->tv_sec = 0;
    ts->tv_nsec = 0;
#endif

    return 0;
}
