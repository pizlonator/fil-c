/* Multithread-safety test for localtime_r().
   Copyright (C) 2024 Free Software Foundation, Inc.

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

/* Written by Bruno Haible <bruno@clisp.org>, 2024.  */

#include <config.h>

/* Work around GCC bug 44511.  */
#if 4 < __GNUC__ + (3 <= __GNUC_MINOR__)
# pragma GCC diagnostic ignored "-Wreturn-type"
#endif

#if USE_ISOC_THREADS || USE_POSIX_THREADS || USE_ISOC_AND_POSIX_THREADS || USE_WINDOWS_THREADS

/* Specification.  */
#include <time.h>

#include <stdio.h>
#include <stdlib.h>

#include "glthread/thread.h"
#include "macros.h"


/* Some common time zone name.  */

#if defined _WIN32 && !defined __CYGWIN__
/* Cf. <https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/default-time-zones>
   or <https://ss64.com/timezones.html>  */
# define FRENCH_TZ  "Romance Standard Time"
#else
/* Cf. <https://en.wikipedia.org/wiki/List_of_tz_database_time_zones>  */
# define FRENCH_TZ  "Europe/Paris"
#endif


static void *
thread1_func (void *arg)
{
  for (;;)
    {

      time_t t = 1178467200; /* 2007-05-06 18:00:00 */
      struct tm tm;
      struct tm *result = localtime_r (&t, &tm);
      ASSERT (result == &tm);
      if (!(result->tm_sec == 0
            && result->tm_min == 0
            && result->tm_hour == 18
            && result->tm_mday == 6
            && result->tm_mon == 5 - 1
            && result->tm_year == 2007 - 1900
            && result->tm_wday == 0
            && result->tm_yday == 125
            && result->tm_isdst == 1))
        {
          fprintf (stderr, "thread1 disturbed by thread2!\n"); fflush (stderr);
          abort ();
        }
    }

  /*NOTREACHED*/
}

static void *
thread2_func (void *arg)
{
  for (;;)
    {
      time_t t = 1336320000; /* 2012-05-06 18:00:00 */
      struct tm tm;
      struct tm *result = localtime_r (&t, &tm);
      ASSERT (result == &tm);
      if (!(result->tm_sec == 0
            && result->tm_min == 0
            && result->tm_hour == 18
            && result->tm_mday == 6
            && result->tm_mon == 5 - 1
            && result->tm_year == 2012 - 1900
            && result->tm_wday == 0
            && result->tm_yday == 126
            && result->tm_isdst == 1))
        {
          fprintf (stderr, "thread2 disturbed by thread1!\n"); fflush (stderr);
          abort ();
        }
    }

  /*NOTREACHED*/
}

int
main (int argc, char *argv[])
{
  setenv ("TZ", FRENCH_TZ, 1);

  /* Check that this TZ works.  */
  {
    time_t t = 0; /* 1970-01-01 01:00:00 */
    struct tm *result = localtime (&t);
    if (! (result
           && result->tm_sec == 0
           && result->tm_min == 0
           && result->tm_hour == 1
           && result->tm_mday == 1
           && result->tm_mon == 1 - 1
           && result->tm_year == 1970 - 1900
           && result->tm_wday == 4
           && result->tm_yday == 0
           && result->tm_isdst == 0))
      {
        fputs ("Skipping test: TZ='" FRENCH_TZ "' is not Paris time\n",
               stderr);
        return 77;
      }
  }

  /* Create the threads.  */
  gl_thread_create (thread1_func, NULL);
  gl_thread_create (thread2_func, NULL);

  /* Let them run for 1 second.  */
  {
    struct timespec duration;
    duration.tv_sec = (argc > 1 ? atoi (argv[1]) : 1);
    duration.tv_nsec = 0;

    nanosleep (&duration, NULL);
  }

  return test_exit_status;
}

#else

/* No multithreading available.  */

#include <stdio.h>

int
main ()
{
  fputs ("Skipping test: multithreading not enabled\n", stderr);
  return 77;
}

#endif
