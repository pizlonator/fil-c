/* Get the system clock resolution.

   Copyright 2021-2024 Free Software Foundation, Inc.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Paul Eggert.  */

#include <config.h>

#include "timespec.h"

static long int _GL_ATTRIBUTE_CONST
gcd (long int a, long int b)
{
  while (b != 0)
    {
      long int r = a % b;
      a = b;
      b = r;
    }
  return a;
}

/* Return the system time resolution in nanoseconds.  */

long int
gettime_res (void)
{
#if defined __sun && defined __sparc
  /* On Solaris 11.4/SPARC (with a SPARC-M8 CPU)
     clock_getres (CLOCK_REALTIME, ...) returns a resolution of 1000000 ns,
     and clock_gettime (CLOCK_REALTIME) returns a multiple of 5 ns with a
     probability of ca. 1 - 1/300000.  Thus the heuristic below with the
     32 samples guesses a resolution of 5 ns with a probability of ca.
     1 - 1/10000.  But occasionally clock_gettime (CLOCK_REALTIME) returns
     only a multiple of 1 ns.
     Simple guesswork is not enough to cope with this irregular behaviour.
     Therefore, here is an override.  */
  return 1;
#else
  struct timespec res;
# if defined CLOCK_REALTIME && HAVE_CLOCK_GETRES
  clock_getres (CLOCK_REALTIME, &res);
# elif defined HAVE_TIMESPEC_GETRES
  timespec_getres (&res, TIME_UTC);
# else
  /* Guess high and let the later code deduce better.  */
  res.tv_sec = 1;
  res.tv_nsec = 0;
# endif

  /* On all Gnulib platforms the following calculations do not overflow.  */

  long int hz = TIMESPEC_HZ;
  long int r = res.tv_nsec <= 0 ? hz : res.tv_nsec;
  struct timespec earlier = { .tv_nsec = -1 };

  /* On some platforms, clock_getres (CLOCK_REALTIME, ...) yields a
     too-large resolution, under the mistaken theory that it should
     return the timer interval.  For example, on AIX 7.2 POWER8
     clock_getres yields 10 ms even though clock_gettime yields 1 μs
     resolution.  Work around the problem with high probability by
     trying clock_gettime several times and observing the resulting
     bounds on resolution.  */
  int nsamples = 32;
  for (int i = 0; 1 < r && i < nsamples; i++)
    {
      /* If successive timestamps disagree the clock resolution must
         be small, so exit the inner loop to check this sample.
         Otherwise, arrange for the outer loop to exit but continue
         the inner-loop search for a differing timestamp sample.  */
      struct timespec now;
      for (;; i = nsamples)
        {
          now = current_timespec ();
          if (earlier.tv_nsec != now.tv_nsec || earlier.tv_sec != now.tv_sec)
            break;
        }
      earlier = now;

      if (0 < now.tv_nsec)
        r = gcd (r, now.tv_nsec);
    }

  return r;
#endif
}

/*
 * Hey Emacs!
 * Local Variables:
 * coding: utf-8
 * End:
 */
