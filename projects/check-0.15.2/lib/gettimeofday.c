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
#include <errno.h>

#if defined(_MSC_VER) || defined(__BORLANDC__)
#define EPOCHFILETIME (116444736000000000i64)
#else
#define EPOCHFILETIME (116444736000000000LL)
#endif

int gettimeofday(struct timeval *tv, void *tz)
{
#if defined(_MSC_VER)
    union
    {
        __int64 ns100;          /* time since 1 Jan 1601 in 100ns units */
        FILETIME ft;
    } now;

    GetSystemTimeAsFileTime(&now.ft);
    tv->tv_usec = (long)((now.ns100 / 10LL) % 1000000LL);
    tv->tv_sec = (long)((now.ns100 - EPOCHFILETIME) / 10000000LL);
    return (0);
#else
    /* Return that there is no implementation of this on the system */
    errno = ENOSYS;
    return -1;
#endif /* _MSC_VER */
}
