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
#include "check_check.h"

int main (void)
{
  int number_failed;
  SRunner *sr;

  fork_setup();
  setup_fixture();
  setup();

  sr = srunner_create (make_master_suite());
  srunner_add_suite(sr, make_list_suite());
  srunner_add_suite(sr, make_msg_suite());
  srunner_add_suite(sr, make_log_suite());
  srunner_add_suite(sr, make_log_internal_suite());
  srunner_add_suite(sr, make_limit_suite());
  srunner_add_suite(sr, make_fork_suite());
  srunner_add_suite(sr, make_fixture_suite());
  srunner_add_suite(sr, make_pack_suite());
  srunner_add_suite(sr, make_tag_suite());

#if defined(HAVE_FORK) && HAVE_FORK==1
  srunner_add_suite(sr, make_exit_suite());
#endif

  srunner_add_suite(sr, make_selective_suite());
  
  printf ("Ran %d tests in subordinate suite\n", sub_ntests);
  srunner_run_all (sr, CK_VERBOSE);
  cleanup();
  fork_teardown();
  teardown_fixture();
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
