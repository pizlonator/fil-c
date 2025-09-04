/*
 * Copyright 2022-2024 Free Software Foundation, Inc.
 * Written by Paul Eggert.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#include <config.h>

#include <timespec.h>

#include <stdio.h>

int
main (void)
{
  long int res = gettime_res ();
  printf ("gettime_res returned %ld ns\n", res);

  if (res <= 0)
    {
      fprintf (stderr, "gettime_res value %ld not positive\n", res);
      return 1;
    }

  if (res < TIMESPEC_HZ)
    {
      if (TIMESPEC_HZ % res != 0)
        {
          fprintf (stderr,
                   ("gettime_res value %ld ns is smaller than %d"
                    " but does not divide it\n"),
                   res, TIMESPEC_HZ);
          return 1;
        }
    }
  else
    {
      if (res % TIMESPEC_HZ != 0)
        {
          fprintf (stderr,
                   ("gettime_res value %ld ns is larger than %d"
                    " but is not a multiple of it\n"),
                   res, TIMESPEC_HZ);
          return 1;
        }
    }

  int saw_res = 0;

  for (int i = 0; i < 100000; i++)
    {
      struct timespec t = current_timespec ();
      if (res < TIMESPEC_HZ
          ? t.tv_nsec % res != 0
          : t.tv_nsec != 0 || t.tv_sec % (res / TIMESPEC_HZ) != 0)
        {
          fprintf (stderr,
                   ("current_timespec returned %lld.%09ld which is not"
                    " a multiple of the resolution, %ld ns\n"),
                   (long long) t.tv_sec, t.tv_nsec, res);
          return 1;
        }
      if (res < TIMESPEC_HZ
          ? (t.tv_nsec / res % 2 != 0
             || t.tv_nsec / res % 5 != 0)
          : (t.tv_sec / (res / TIMESPEC_HZ) % 2 != 0
             || t.tv_sec / (res / TIMESPEC_HZ) % 5 != 0))
        saw_res = 1;
    }

  if (saw_res == 0)
    fprintf (stderr,
             ("warning: all timestamps had coarser"
              " resolution than %ld ns\n"),
             res);

  return 0;
}
