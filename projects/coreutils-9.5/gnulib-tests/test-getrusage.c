/* Test of getting resource utilization.
   Copyright (C) 2012-2024 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Bruno Haible <bruno@clisp.org>, 2012.  */

#include <config.h>

#include <sys/resource.h>

#include "signature.h"
SIGNATURE_CHECK (getrusage, int, (int, struct rusage *));

#include <sys/time.h>

#include "macros.h"

volatile unsigned int counter;

int
main (void)
{
  struct rusage before;
  struct rusage after;
  int ret;

  ret = getrusage (RUSAGE_SELF, &before);
  ASSERT (ret == 0);

  /* Busy-loop for one second.  */
  {
    struct timeval t0;
    ASSERT (gettimeofday (&t0, NULL) == 0);

    for (;;)
      {
        struct timeval t;
        int i;

        for (i = 0; i < 1000000; i++)
          counter++;

        ASSERT (gettimeofday (&t, NULL) == 0);
        if (t.tv_sec - t0.tv_sec > 1
            || (t.tv_sec - t0.tv_sec == 1 && t.tv_usec >= t0.tv_usec))
          break;
      }
  }

  ret = getrusage (RUSAGE_SELF, &after);
  ASSERT (ret == 0);

  ASSERT (after.ru_utime.tv_sec >= before.ru_utime.tv_sec);
  ASSERT (after.ru_stime.tv_sec >= before.ru_stime.tv_sec);
  {
    /* Compute time spent during busy-looping (in usec).  */
    unsigned int spent_utime =
      (after.ru_utime.tv_sec > before.ru_utime.tv_sec
       ? (after.ru_utime.tv_sec - before.ru_utime.tv_sec - 1) * 1000000U
         + after.ru_utime.tv_usec + (1000000U - before.ru_utime.tv_usec)
       : after.ru_utime.tv_usec - before.ru_utime.tv_usec);
    unsigned int spent_stime =
      (after.ru_stime.tv_sec > before.ru_stime.tv_sec
       ? (after.ru_stime.tv_sec - before.ru_stime.tv_sec - 1) * 1000000U
         + after.ru_stime.tv_usec + (1000000U - before.ru_stime.tv_usec)
       : after.ru_stime.tv_usec - before.ru_stime.tv_usec);

    ASSERT (spent_utime + spent_stime <= 2 * 1000000U);
    /* Assume that the load during this busy-looping was less than 100.  */
    ASSERT (spent_utime + spent_stime > 10000U);
  }

  return 0;
}
