/* Buffer primitives for comparison operations.

   Copyright (C) 1993, 1995, 1998, 2001-2002, 2006, 2009-2013, 2015-2023 Free
   Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include "cmpbuf.h"
#include "intprops.h"

#ifndef SSIZE_MAX
# define SSIZE_MAX TYPE_MAXIMUM (ssize_t)
#endif

#undef MIN
#define MIN(a, b) ((a) <= (b) ? (a) : (b))

/* Read NBYTES bytes from descriptor FD into BUF.
   NBYTES must not be SIZE_MAX.
   Return the number of characters successfully read.
   On error, return SIZE_MAX, setting errno.
   The number returned is always NBYTES unless end-of-file or error.  */

size_t
block_read (int fd, char *buf, size_t nbytes)
{
  char *bp = buf;
  char const *buflim = buf + nbytes;
  size_t readlim = MIN (SSIZE_MAX, SIZE_MAX);

  do
    {
      size_t bytes_remaining = buflim - bp;
      size_t bytes_to_read = MIN (bytes_remaining, readlim);
      ssize_t nread = read (fd, bp, bytes_to_read);
      if (nread <= 0)
        {
          if (nread == 0)
            break;

          /* Accommodate Tru64 5.1, which can't read more than INT_MAX
             bytes at a time.  They call that a 64-bit OS?  */
          if (errno == EINVAL && INT_MAX < bytes_to_read)
            {
              readlim = INT_MAX;
              continue;
            }

          /* This is needed for programs that have signal handlers on
             older hosts without SA_RESTART.  It also accommodates
             ancient AIX hosts that set errno to EINTR after uncaught
             SIGCONT.  See <news:1r77ojINN85n@ftp.UU.NET>
             (1993-04-22).  */
          if (! SA_RESTART && errno == EINTR)
            continue;

          return SIZE_MAX;
        }
      bp += nread;
    }
  while (bp < buflim);

  return bp - buf;
}

/* Least common multiple of two buffer sizes A and B.  However, if
   either A or B is zero, or if the multiple is greater than LCM_MAX,
   return a reasonable buffer size.  */

size_t
buffer_lcm (size_t a, size_t b, size_t lcm_max)
{
  size_t lcm, m, n, q, r;

  /* Yield reasonable values if buffer sizes are zero.  */
  if (!a)
    return b ? b : 8 * 1024;
  if (!b)
    return a;

  /* n = gcd (a, b) */
  for (m = a, n = b;  (r = m % n) != 0;  m = n, n = r)
    continue;

  /* Yield a if there is an overflow.  */
  q = a / n;
  return !INT_MULTIPLY_WRAPV (q, b, &lcm) && lcm <= lcm_max ? lcm : a;
}
