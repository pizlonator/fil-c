/* struct timespec subtraction.
   Copyright (C) 1991-2026 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <https://www.gnu.org/licenses/>.
*/
#include <stdckdint.h>
#include <time.h>

/* Put into *R the difference between X and Y.
   Return true if overflow occurs, false otherwise.  */

bool
timespec_subtract (struct timespec *r,
                   struct timespec x, struct timespec y)
{
  /* Compute nanoseconds, setting @var{borrow} to 1 or 0
     for propagation into seconds.  */
  long int nsec_diff = x.tv_nsec - y.tv_nsec;
  bool borrow = nsec_diff < 0;
  r->tv_nsec = nsec_diff + 1000000000 * borrow;

  /* Compute seconds, returning true if this overflows.  */
  bool v = ckd_sub (&r->tv_sec, x.tv_sec, y.tv_sec);
  return v ^ ckd_sub (&r->tv_sec, r->tv_sec, borrow);
}
