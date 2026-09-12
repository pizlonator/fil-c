/* Test lock ordering of fflush (NULL) vs freopen, fclose (bug 24963).
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

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

#include <array_length.h>
#include <stdint.h>
#include <stdio_ext.h>
#include <support/check.h>
#include <support/xstdio.h>
#include <support/xthread.h>
#include <unistd.h>

static _Atomic bool running = true;

static void *
fflush_thread (void *ignored)
{
  while (running)
    TEST_COMPARE (fflush (NULL), 0);

  return NULL;
}

static void *
flushlbf_thread (void *ignored)
{
  while (running)
    _flushlbf ();

  return NULL;
}

static void *
fopen_thread (void *ignored)
{
  while (running)
    {
      FILE *fp = xfopen ("/etc/passwd", "r");
      (void) fgetc (fp);
      xfclose (fp);
    }
  return NULL;
}

static void *
freopen_thread (void *fp)
{
  while (running)
    {
      uintptr_t old_address = (uintptr_t) fp;
      FILE *fpnew = xfreopen ("/etc/passwd", "r", fp);
      TEST_COMPARE (old_address, (uintptr_t) fpnew);
    }
  return NULL;
}

static int
do_test (void)
{
  pthread_t fflush_thr = xpthread_create (NULL, fflush_thread, NULL);
  pthread_t flushlbf_thr = xpthread_create (NULL, flushlbf_thread, NULL);

  pthread_t fopens[2];
  for (int i = 0; i < array_length(fopens); ++i)
    fopens[i] = xpthread_create (NULL, fopen_thread, NULL);

  FILE *fps[2];
  pthread_t freopens[array_length (fps)];
  for (int i = 0; i < array_length(fps); ++i)
    {
      fps[i] = xfopen ("/etc/passwd", "r");
      freopens[i] = xpthread_create (NULL, freopen_thread, fps[i]);
    }

  usleep (2 * 1000 * 1000);
  running = false;

  for (int i = 0; i < array_length(fopens); ++i)
    xpthread_join (fopens[i]);
  for (int i = 0; i < array_length(fps); ++i)
    {
      xpthread_join (freopens[i]);
      xfclose (fps[i]);
    }

  xpthread_join (flushlbf_thr);
  xpthread_join (fflush_thr);

  return 0;
}

#include <support/test-driver.c>
