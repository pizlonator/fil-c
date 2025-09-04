/* Convert double to timespec.
   Copyright (C) 2011-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library and is also part of gnulib.
   Patches to this file should be submitted to both projects.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

/* Convert the double value SEC to a struct timespec.  Round toward
   positive infinity.  On overflow, return an extremal value.  */

#include <support/timespec.h>
#include <intprops.h>

struct timespec
dtotimespec (double sec)
{
  if (sec <= TYPE_MINIMUM (time_t))
    return make_timespec (TYPE_MINIMUM (time_t), 0);
  else if (sec >= 1.0 + TYPE_MAXIMUM (time_t))
    return make_timespec (TYPE_MAXIMUM (time_t), TIMESPEC_HZ - 1);
  else
    {
      time_t s = sec;
      double frac = TIMESPEC_HZ * (sec - s);
      long ns = frac;
      ns += ns < frac;
      s += ns / TIMESPEC_HZ;
      ns %= TIMESPEC_HZ;

      if (ns < 0)
        {
          s--;
          ns += TIMESPEC_HZ;
        }

      return make_timespec (s, ns);
    }
}
