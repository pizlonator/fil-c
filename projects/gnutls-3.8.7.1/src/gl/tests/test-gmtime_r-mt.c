/* Multithread-safety test for gmtime_r().
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

static void *
thread1_func (void *arg)
{
  for (;;)
    {
      time_t t = 1509000003; /* 2017-10-26 06:40:03 */
      struct tm tm;
      struct tm *result = gmtime_r (&t, &tm);
      ASSERT (result == &tm);
      if (!(result->tm_sec == 3
            && result->tm_min == 40
            && result->tm_hour == 6
            && result->tm_mday == 26
            && result->tm_mon == 10 - 1
            && result->tm_year == 2017 - 1900
            && result->tm_wday == 4
            && result->tm_yday == 298
            && result->tm_isdst == 0))
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
      time_t t = 2000050005; /* 2033-05-18 17:26:45 */
      struct tm tm;
      struct tm *result = gmtime_r (&t, &tm);
      ASSERT (result == &tm);
      if (!(result->tm_sec == 45
            && result->tm_min == 26
            && result->tm_hour == 17
            && result->tm_mday == 18
            && result->tm_mon == 5 - 1
            && result->tm_year == 2033 - 1900
            && result->tm_wday == 3
            && result->tm_yday == 137
            && result->tm_isdst == 0))
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
