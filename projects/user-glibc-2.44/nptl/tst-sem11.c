/* Test of semaphores.
   Copyright (C) 2007-2026 Free Software Foundation, Inc.
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

#include <semaphore.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <internaltypes.h>

#ifndef SEM_WAIT
# define SEM_WAIT(s) sem_wait (s)
#endif

static void *
tf (void *arg)
{
#ifdef TF_PREPARE
  TF_PREPARE
#endif
  SEM_WAIT (arg);
  return NULL;
}

int
do_test (void)
{
  int tries = 5;
  pthread_t th;
  union
  {
    sem_t s;
    struct new_sem ns;
  } u;
 again:
  if (sem_init (&u.s, 0, 0) != 0)
    {
      puts ("sem_init failed");
      return 1;
    }
#if USE_64B_ATOMICS_ON_SEM_T
  if ((u.ns.data >> SEM_NWAITERS_SHIFT) != 0)
#else
  if (u.ns.nwaiters != 0)
#endif
    {
      puts ("nwaiters not initialized");
      return 1;
    }

  if (pthread_create (&th, NULL, tf, &u.s) != 0)
    {
      puts ("pthread_create failed");
      return 1;
    }

  sleep (1);

  if (pthread_cancel (th) != 0)
    {
      puts ("pthread_cancel failed");
      return 1;
    }

  void *r;
  if (pthread_join (th, &r) != 0)
    {
      puts ("pthread_join failed");
      return 1;
    }
  if (r != PTHREAD_CANCELED && --tries > 0)
    {
      /* Maybe we get the scheduling right the next time.  */
      sem_destroy (&u.s);
      goto again;
    }

#if USE_64B_ATOMICS_ON_SEM_T
  if ((u.ns.data >> SEM_NWAITERS_SHIFT) != 0)
#else
  if (u.ns.nwaiters != 0)
#endif
    {
      puts ("nwaiters not reset");
      return 1;
    }

  return 0;
}

#include <support/test-driver.c>
