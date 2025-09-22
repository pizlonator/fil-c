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

#include <string.h>
#include <stdlib.h>

#include "check.h"
#include "check_list.h"
#include "check_check.h"

START_TEST(test_create)
{
  List *lp = NULL;

  ck_assert_msg (check_list_val(lp) == NULL,
	       "Current list value should be NULL for NULL list");

  lp = check_list_create();

  ck_assert_msg (check_list_val(lp) == NULL,
	       "Current list value should be NULL for newly created list");

  ck_assert_msg (check_list_at_end(lp),
	       "Newly created list should be at end");
  check_list_advance(lp);
  ck_assert_msg (check_list_at_end(lp),
	       "Advancing a list at end should produce a list at end");
  check_list_free (lp);
}
END_TEST

START_TEST(test_free)
{
  List *lp = check_list_create();
  char data_abc[] = "abc";
  char data_123[] = "123";

  check_list_add_end (lp, data_abc);
  check_list_add_end (lp, data_123);
  check_list_add_end (lp, NULL);
  check_list_free (lp);
}
END_TEST

START_TEST(test_add_end)
{
  List * lp = check_list_create();
  char tval[] = "abc";
  
  check_list_add_end (lp, tval);
  
  ck_assert_msg (check_list_val (lp) != NULL,
	       "List current val should not be null after new insertion");
  ck_assert_msg (!check_list_at_end (lp),
	       "List should be at end after new insertion");
  ck_assert_msg (strcmp(tval, (char *) check_list_val (lp)) == 0,
	       "List current val should equal newly inserted val");
  check_list_free (lp);
}
END_TEST

START_TEST(test_add_front)
{
  List * lp = check_list_create();
  char tval[] = "abc";
  
  check_list_add_front (lp, tval);
  
  ck_assert_msg (check_list_val (lp) != NULL,
	       "List current val should not be null after new insertion");
  ck_assert_msg (strcmp(tval, (char *) check_list_val (lp)) == 0,
	       "List current val should equal newly inserted val");
  check_list_free (lp);
}
END_TEST

START_TEST(test_add_end_and_next)
{
  List *lp = check_list_create();
  char tval1[] = "abc";
  char tval2[] = "123";
  
  check_list_add_end (lp, tval1);
  check_list_add_end (lp, tval2);
  check_list_front(lp);
  ck_assert_msg (strcmp (tval1, (char *)check_list_val (lp)) == 0,
	       "List head val should equal first inserted val");
  check_list_advance (lp);
  ck_assert_msg (!check_list_at_end (lp),
	       "List should not be at end after two adds and one next");
  ck_assert_msg (strcmp (tval2, (char *)check_list_val (lp)) == 0,
	       "List val should equal second inserted val");
  check_list_advance(lp);
  ck_assert_msg (check_list_at_end (lp),
	       "List should be at and after two adds and two nexts");
  check_list_free (lp);
}
END_TEST


START_TEST(test_add_front_and_next)
{
  List * lp = check_list_create();
  char tval1[] = "abc";
  char tval2[] = "123";
  
  check_list_add_front (lp, tval1);
  check_list_add_front (lp, tval2);
  check_list_front(lp);
  ck_assert_msg (strcmp (tval2, (char *)check_list_val (lp)) == 0,
	       "List head val should equal last inserted val");
  check_list_advance (lp);
  ck_assert_msg (!check_list_at_end (lp),
	       "List should not be at end after two adds and one next");
  ck_assert_msg (strcmp (tval1, (char *)check_list_val (lp)) == 0,
	       "List val should equal first inserted val");
  check_list_advance(lp);
  ck_assert_msg (check_list_at_end (lp),
	       "List should be at and after two adds and two nexts");
  check_list_free (lp);
}
END_TEST

START_TEST(test_add_a_bunch)
{
  int i, j;
  char tval1[] = "abc";
  char tval2[] = "123";
  for (i = 0; i < 3; i++) {
    List *lp = check_list_create();
    for (j = 0; j < 1000; j++) {
      check_list_add_end (lp, tval1);
      check_list_add_front (lp, tval2);
    }
    check_list_free(lp);
  }
}
END_TEST

START_TEST(test_list_abuse)
{
    check_list_advance(NULL);
    /* Should not crash */
}
END_TEST

START_TEST(test_contains)
{
    List *lp = check_list_create();

    char otherData[] = "other";
    char goalData[] = "goal";
    int index;

    ck_assert_msg (check_list_contains(lp, goalData) == 0,
                       "The goal data should not be in the list yet");

    for(index = 0; index < 10; index++)
    {
        check_list_add_end (lp, otherData);
        ck_assert_msg (check_list_contains(lp, goalData) == 0,
                   "The goal data should not be in the list yet");
    }

    check_list_add_end (lp, goalData);
    ck_assert_msg (check_list_contains(lp, goalData) ,
                       "The goal data should be in the list");

    check_list_free(lp);
}
END_TEST

Suite *make_list_suite (void)
{
  Suite *s = suite_create("Lists");
  TCase * tc = tcase_create("Core");

  suite_add_tcase (s, tc);
  tcase_add_test (tc, test_create);
  tcase_add_test (tc, test_free);
  tcase_add_test (tc, test_add_end);
  tcase_add_test (tc, test_add_front);
  tcase_add_test (tc, test_add_end_and_next);
  tcase_add_test (tc, test_add_front_and_next);
  tcase_add_test (tc, test_add_a_bunch);
  tcase_add_test (tc, test_list_abuse);
  tcase_add_test (tc, test_contains);

  return s;
}
