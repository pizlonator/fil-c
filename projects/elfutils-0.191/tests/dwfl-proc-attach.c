/* Test dwfl_linux_proc_attach works without any modules.
   Copyright (C) 2015 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/user.h>
#include <fcntl.h>
#include <string.h>
#include ELFUTILS_HEADER(dwfl)
#include <pthread.h>
#endif
#include "system.h"

#ifndef __linux__
int
main (int argc __attribute__ ((unused)), char **argv __attribute__ ((unused)))
{
  printf ("dwfl_linux_proc_attach unsupported.\n");
  return 77;
}
#else /* __linux__ */

static pthread_t thread1;
static pthread_t thread2;

static void *
sleeper (void* d __attribute__ ((unused)))
{
  sleep (60);
  return NULL;
}

static char *debuginfo_path = NULL;

static const Dwfl_Callbacks proc_callbacks =
  {
    .find_elf = dwfl_linux_proc_find_elf,
    .find_debuginfo = dwfl_standard_find_debuginfo,
    .debuginfo_path = &debuginfo_path,
  };

static int
thread_callback (Dwfl_Thread *thread, void *thread_arg)
{
  int *threads = (int *) thread_arg;
  pid_t tid = dwfl_thread_tid (thread);
  printf ("thread tid: %d\n", tid);
  (*threads)++;

  return DWARF_CB_OK;
}

int
main (int argc __attribute__ ((unused)),
      char **argv __attribute__ ((unused)))
{
  /* Create two extra threads to iterate through.  */
  int err;
  if ((err = pthread_create (&thread1, NULL, sleeper, NULL)) != 0)
    error (-1, err, "Couldn't create thread1");
  if ((err = pthread_create (&thread2, NULL, sleeper, NULL)) != 0)
    error (-1, err, "Couldn't create thread2");

  Dwfl *dwfl = dwfl_begin (&proc_callbacks);
  if (dwfl == NULL)
    error (-1, 0, "dwfl_begin: %s", dwfl_errmsg (-1));

  pid_t pid = getpid ();
  /* This used to fail, since we don't have any modules yet.  */
  if (dwfl_linux_proc_attach (dwfl, pid, false) < 0)
    error (-1, 0, "dwfl_linux_proc_attach pid %d: %s", pid,
	   dwfl_errmsg (-1));

  /* Did we see all 3 threads?  */
  int threads = 0;
  if (dwfl_getthreads (dwfl, thread_callback, &threads) != DWARF_CB_OK)
    error (-1, 0, "dwfl_getthreads failed: %s", dwfl_errmsg (-1));

  dwfl_end (dwfl);

  pthread_cancel (thread1);
  pthread_cancel (thread2);
  pthread_join (thread1, NULL);
  pthread_join (thread2, NULL);

  return (threads == 3) ? 0 : -1;
}

/* HACK. This is a simple workaround for a combination of old glibc
   and valgrind. libdw will try to dlopen libdebuginfod this causes
   some unsuppressable memory leak warnings when the process is
   multi-threaded under valgrind because of some bad backtraces.
   So simply override dlopen and always return NULL so libdebuginfod
   (and libcurl) are never loaded.  This test doesn't rely on
   libdebuginfod anyway.  */
void *dlopen (void)
{
  return NULL;
}

#endif /* __linux__ */
