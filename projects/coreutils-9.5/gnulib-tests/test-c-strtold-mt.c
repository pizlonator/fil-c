/* Multithread-safety test for c_strtold().
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
#include "c-strtod.h"

#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "glthread/thread.h"

static void *
thread1_func (void *arg)
{
  const char input[] = "1,5";

  for (;;)
    {
      char *ptr;
      long double result;
      errno = 0;
      result = c_strtold (input, &ptr);
      if (!(result == 1.0L && ptr == input + 1 && errno == 0))
        {
          fprintf (stderr, "thread1 malfunction!\n"); fflush (stderr);
          abort ();
        }
    }

  /*NOTREACHED*/
}

static void *
thread2_func (void *arg)
{
  const char input[] = "1,5";

  for (;;)
    {
      char *ptr;
      long double result;
      errno = 0;
      result = strtold (input, &ptr);
      if (!(result == 1.5L && ptr == input + 3 && errno == 0))
        {
          fprintf (stderr, "thread2 disturbed by thread1!\n"); fflush (stderr);
          abort ();
        }
    }

  /*NOTREACHED*/
}

static void *
thread3_func (void *arg)
{
  for (;;)
    {
      char pointbuf[5];
      sprintf (pointbuf, "%#.0f", 1.0);
      if (!(pointbuf[1] == ','))
        {
          fprintf (stderr, "thread3 disturbed by thread1!\n"); fflush (stderr);
          abort ();
        }
    }

  /*NOTREACHED*/
}

int
main (int argc, char *argv[])
{
  /* Try to set the locale by implicitly looking at the LC_ALL environment
     variable.
     configure should already have checked that the locale is supported.  */
  if (setlocale (LC_ALL, "") == NULL)
    return 1;

  /* Create the threads.  */
  gl_thread_create (thread1_func, NULL);
  gl_thread_create (thread2_func, NULL);
  gl_thread_create (thread3_func, NULL);

  /* Let them run for 1 second.  */
  {
    struct timespec duration;
    duration.tv_sec = (argc > 1 ? atoi (argv[1]) : 1);
    duration.tv_nsec = 0;

    nanosleep (&duration, NULL);
  }

  return 0;
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
