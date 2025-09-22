/*
 * Check: a unit test framework for C
 * Copyright (C) 2001, 2002 Arien Malec
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "../lib/libcompat.h"

#include <stdlib.h>
#include <stdio.h>
#include <check.h>

Suite *s;
TCase *tc;
SRunner *sr;

#if defined (HAVE_PTHREAD) || defined (HAVE_FORK)
static void *
sendinfo (void *userdata CK_ATTRIBUTE_UNUSED)
{
  unsigned int i;
  for (i = 0; i < 999; i++)
    {
      ck_assert_msg (1, "Shouldn't see this message");
    }
  return NULL;
}
#endif /* HAVE_PTHREAD || HAVE_FORK */

#ifdef HAVE_PTHREAD
START_TEST (test_stress_threads)
{
  pthread_t a, b;
  pthread_create (&a, NULL, sendinfo, (void *) 0xa);
  pthread_create (&b, NULL, sendinfo, (void *) 0xb);

  pthread_join (a, NULL);
  pthread_join (b, NULL);
}
END_TEST
#endif /* HAVE_PTHREAD */

#if defined(HAVE_FORK) && HAVE_FORK==1
START_TEST (test_stress_forks)
{
  pid_t cpid = fork ();
  if (cpid == 0)
    {
      /* child */
      sendinfo ((void *) 0x1);
      exit (EXIT_SUCCESS);
    }
  else
    {
      /* parent */
      sendinfo ((void *) 0x2);
    }
}
END_TEST
#endif /* HAVE_FORK */

int
main (void)
{
  int number_failed;
  s = suite_create ("ForkThreadStress");
  tc = tcase_create ("ForkThreadStress");
  sr = srunner_create (s);
  suite_add_tcase (s, tc);

#ifdef HAVE_PTHREAD
  tcase_add_loop_test (tc, test_stress_threads, 0, 100);
#endif /* HAVE_PTHREAD */

#if defined(HAVE_FORK) && HAVE_FORK==1
  tcase_add_loop_test (tc, test_stress_forks, 0, 100);
#endif /* HAVE_FORK */

  srunner_run_all (sr, CK_VERBOSE);
  number_failed = srunner_ntests_failed (sr);
  srunner_free (sr);

  /* hack to give us XFAIL on non-posix platforms */
#ifndef HAVE_FORK
  number_failed++;
#endif /* !HAVE_FORK */

  return number_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
