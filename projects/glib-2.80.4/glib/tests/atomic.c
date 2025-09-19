/*
 * Copyright 2011 Red Hat, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#include <glib.h>

/* We want the g_atomic_pointer_get() macros to work when compiling third party
 * projects with -Wbad-function-cast.
 * See https://gitlab.gnome.org/GNOME/glib/issues/1041. */
#pragma GCC diagnostic error "-Wbad-function-cast"

static void
test_types (void)
{
  const gint *csp;
  const gint * const *cspp;
  guint u, u2;
  gint s, s2;
  gpointer vp, vp2, cp;
  const char *vp_str, *vp_str2;
  const char *volatile vp_str_vol;
  const char *str = "Hello";
  const char *old_str;
  int *ip, *ip2;
  guintptr gu, gu2;
  gboolean res;

  csp = &s;
  cspp = &csp;

  g_atomic_int_set (&u, 5);
  u2 = (guint) g_atomic_int_get (&u);
  g_assert_cmpint (u2, ==, 5);
  res = g_atomic_int_compare_and_exchange (&u, 6, 7);
  g_assert_false (res);
  g_assert_cmpint (u, ==, 5);
  res = g_atomic_int_compare_and_exchange_full (&u, 6, 7, &u2);
  g_assert_false (res);
  g_assert_cmpint (u, ==, 5);
  g_assert_cmpint (u2, ==, 5);
  g_atomic_int_add (&u, 1);
  g_assert_cmpint (u, ==, 6);
  g_atomic_int_inc (&u);
  g_assert_cmpint (u, ==, 7);
  res = g_atomic_int_dec_and_test (&u);
  g_assert_false (res);
  g_assert_cmpint (u, ==, 6);
  u2 = g_atomic_int_and (&u, 5);
  g_assert_cmpint (u2, ==, 6);
  g_assert_cmpint (u, ==, 4);
  u2 = g_atomic_int_or (&u, 8);
  g_assert_cmpint (u2, ==, 4);
  g_assert_cmpint (u, ==, 12);
  u2 = g_atomic_int_xor (&u, 4);
  g_assert_cmpint (u2, ==, 12);
  g_assert_cmpint (u, ==, 8);
  u2 = g_atomic_int_exchange (&u, 55);
  g_assert_cmpint (u2, ==, 8);
  g_assert_cmpint (u, ==, 55);

  g_atomic_int_set (&s, 5);
  s2 = g_atomic_int_get (&s);
  g_assert_cmpint (s2, ==, 5);
  res = g_atomic_int_compare_and_exchange (&s, 6, 7);
  g_assert_false (res);
  g_assert_cmpint (s, ==, 5);
  s2 = 0;
  res = g_atomic_int_compare_and_exchange_full (&s, 6, 7, &s2);
  g_assert_false (res);
  g_assert_cmpint (s, ==, 5);
  g_assert_cmpint (s2, ==, 5);
  g_atomic_int_add (&s, 1);
  g_assert_cmpint (s, ==, 6);
  g_atomic_int_inc (&s);
  g_assert_cmpint (s, ==, 7);
  res = g_atomic_int_dec_and_test (&s);
  g_assert_false (res);
  g_assert_cmpint (s, ==, 6);
  s2 = (gint) g_atomic_int_and (&s, 5);
  g_assert_cmpint (s2, ==, 6);
  g_assert_cmpint (s, ==, 4);
  s2 = (gint) g_atomic_int_or (&s, 8);
  g_assert_cmpint (s2, ==, 4);
  g_assert_cmpint (s, ==, 12);
  s2 = (gint) g_atomic_int_xor (&s, 4);
  g_assert_cmpint (s2, ==, 12);
  g_assert_cmpint (s, ==, 8);
  s2 = g_atomic_int_exchange (&s, 55);
  g_assert_cmpint (s2, ==, 8);
  g_assert_cmpint (s, ==, 55);

  g_atomic_pointer_set (&vp, 0);
  vp2 = g_atomic_pointer_get (&vp);
  g_assert_true (vp2 == 0);
  res = g_atomic_pointer_compare_and_exchange (&vp, &s, &s);
  g_assert_false (res);
  cp = &s;
  res = g_atomic_pointer_compare_and_exchange_full (&vp, &s, &s, &cp);
  g_assert_false (res);
  g_assert_null (cp);
  g_assert_true (vp == 0);
  res = g_atomic_pointer_compare_and_exchange (&vp, NULL, NULL);
  g_assert_true (res);
  g_assert_true (vp == 0);
  g_assert_null (g_atomic_pointer_exchange (&vp, &s));
  g_assert_true (vp == &s);
  res = g_atomic_pointer_compare_and_exchange_full (&vp, &s, NULL, &cp);
  g_assert_true (res);
  g_assert_true (cp == &s);

  g_atomic_pointer_set (&vp_str, NULL);
  res = g_atomic_pointer_compare_and_exchange (&vp_str, NULL, str);
  g_assert_true (res);
  g_assert_cmpstr (g_atomic_pointer_exchange (&vp_str, NULL), ==, str);
  g_assert_null (vp_str);
  res = g_atomic_pointer_compare_and_exchange_full (&vp_str, NULL, str, &vp_str2);
  g_assert_true (res);
  g_assert_cmpstr (vp_str, ==, str);
  g_assert_null (vp_str2);
  res = g_atomic_pointer_compare_and_exchange_full (&vp_str, (char *) str, NULL, &vp_str2);
  g_assert_true (res);
  g_assert_null (vp_str);
  g_assert_true (vp_str2 == str);

  /* Note that atomic variables should almost certainly not be marked as
   * `volatile` — see http://isvolatileusefulwiththreads.in/c/. This test exists
   * to make sure that we don’t warn when built against older third party code. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
  g_atomic_pointer_set (&vp_str_vol, NULL);
  g_atomic_pointer_set (&vp_str, str);
  res = g_atomic_pointer_compare_and_exchange (&vp_str_vol, NULL, str);
  g_assert_true (res);
  g_assert_cmpstr (g_atomic_pointer_exchange (&vp_str, NULL), ==, str);
  g_assert_null (vp_str);

  res = g_atomic_pointer_compare_and_exchange_full (&vp_str_vol, str, NULL, &old_str);
  g_assert_true (res);
  g_assert_true (old_str == str);
#pragma GCC diagnostic pop

  g_atomic_pointer_set (&ip, 0);
  ip2 = g_atomic_pointer_get (&ip);
  g_assert_true (ip2 == 0);
  res = g_atomic_pointer_compare_and_exchange (&ip, NULL, NULL);
  g_assert_true (res);
  g_assert_true (ip == 0);

  res = g_atomic_pointer_compare_and_exchange_full (&ip, NULL, &s, &ip2);
  g_assert_true (res);
  g_assert_true (ip == &s);
  g_assert_cmpuint ((guintptr) ip2, ==, 0);

  res = g_atomic_pointer_compare_and_exchange_full (&ip, NULL, NULL, &ip2);
  g_assert_false (res);
  g_assert_true (ip == &s);
  g_assert_true (ip2 == &s);

  g_atomic_pointer_set (&gu, 0);
  vp2 = (gpointer) g_atomic_pointer_get (&gu);
  gu2 = (guintptr) vp2;
  g_assert_cmpuint (gu2, ==, 0);
  res = g_atomic_pointer_compare_and_exchange (&gu, NULL, (guintptr) NULL);
  g_assert_true (res);
  g_assert_cmpuint (gu, ==, 0);
  res = g_atomic_pointer_compare_and_exchange_full (&gu, (guintptr) NULL, (guintptr) NULL, &gu2);
  g_assert_true (res);
  g_assert_cmpuint (gu, ==, 0);
  g_assert_cmpuint (gu2, ==, 0);
  gu2 = (guintptr) g_atomic_pointer_add (&gu, 5);
  g_assert_cmpuint (gu2, ==, 0);
  g_assert_cmpuint (gu, ==, 5);
  gu2 = (guintptr) g_atomic_pointer_and (&gu, 6);
  g_assert_cmpuint (gu2, ==, 5);
  g_assert_cmpuint (gu, ==, 4);
  gu2 = (guintptr) g_atomic_pointer_or (&gu, 8);
  g_assert_cmpuint (gu2, ==, 4);
  g_assert_cmpuint (gu, ==, 12);
  gu2 = (guintptr) g_atomic_pointer_xor (&gu, 4);
  g_assert_cmpuint (gu2, ==, 12);
  g_assert_cmpuint (gu, ==, 8);
  vp_str2 = g_atomic_pointer_exchange (&vp_str, str);
  g_assert_cmpstr (vp_str, ==, str);
  g_assert_null (vp_str2);

  g_assert_cmpint (g_atomic_int_get (csp), ==, s);
  g_assert_true (g_atomic_pointer_get ((const gint **) cspp) == csp);

  /* repeat, without the macros */
#undef g_atomic_int_set
#undef g_atomic_int_get
#undef g_atomic_int_compare_and_exchange
#undef g_atomic_int_compare_and_exchange_full
#undef g_atomic_int_exchange
#undef g_atomic_int_add
#undef g_atomic_int_inc
#undef g_atomic_int_and
#undef g_atomic_int_or
#undef g_atomic_int_xor
#undef g_atomic_int_dec_and_test
#undef g_atomic_pointer_set
#undef g_atomic_pointer_get
#undef g_atomic_pointer_compare_and_exchange
#undef g_atomic_pointer_compare_and_exchange_full
#undef g_atomic_pointer_exchange
#undef g_atomic_pointer_add
#undef g_atomic_pointer_and
#undef g_atomic_pointer_or
#undef g_atomic_pointer_xor

  g_atomic_int_set ((gint*)&u, 5);
  u2 = (guint) g_atomic_int_get ((gint*)&u);
  g_assert_cmpint (u2, ==, 5);
  res = g_atomic_int_compare_and_exchange ((gint*)&u, 6, 7);
  g_assert_false (res);
  g_assert_cmpint (u, ==, 5);
  u2 = 0;
  res = g_atomic_int_compare_and_exchange_full ((gint*)&u, 6, 7, (gint*) &u2);
  g_assert_false (res);
  g_assert_cmpuint (u, ==, 5);
  g_assert_cmpuint (u2, ==, 5);
  g_atomic_int_add ((gint*)&u, 1);
  g_assert_cmpint (u, ==, 6);
  g_atomic_int_inc ((gint*)&u);
  g_assert_cmpint (u, ==, 7);
  res = g_atomic_int_dec_and_test ((gint*)&u);
  g_assert_false (res);
  g_assert_cmpint (u, ==, 6);
  u2 = g_atomic_int_and (&u, 5);
  g_assert_cmpint (u2, ==, 6);
  g_assert_cmpint (u, ==, 4);
  u2 = g_atomic_int_or (&u, 8);
  g_assert_cmpint (u2, ==, 4);
  g_assert_cmpint (u, ==, 12);
  u2 = g_atomic_int_xor (&u, 4);
  g_assert_cmpint (u2, ==, 12);
  u2 = g_atomic_int_exchange ((gint*) &u, 55);
  g_assert_cmpint (u2, ==, 8);
  g_assert_cmpint (u, ==, 55);

  g_atomic_int_set (&s, 5);
  s2 = g_atomic_int_get (&s);
  g_assert_cmpint (s2, ==, 5);
  res = g_atomic_int_compare_and_exchange (&s, 6, 7);
  g_assert_false (res);
  g_assert_cmpint (s, ==, 5);
  s2 = 0;
  res = g_atomic_int_compare_and_exchange_full (&s, 6, 7, &s2);
  g_assert_false (res);
  g_assert_cmpint (s, ==, 5);
  g_assert_cmpint (s2, ==, 5);
  g_atomic_int_add (&s, 1);
  g_assert_cmpint (s, ==, 6);
  g_atomic_int_inc (&s);
  g_assert_cmpint (s, ==, 7);
  res = g_atomic_int_dec_and_test (&s);
  g_assert_false (res);
  g_assert_cmpint (s, ==, 6);
  s2 = (gint) g_atomic_int_and ((guint*)&s, 5);
  g_assert_cmpint (s2, ==, 6);
  g_assert_cmpint (s, ==, 4);
  s2 = (gint) g_atomic_int_or ((guint*)&s, 8);
  g_assert_cmpint (s2, ==, 4);
  g_assert_cmpint (s, ==, 12);
  s2 = (gint) g_atomic_int_xor ((guint*)&s, 4);
  g_assert_cmpint (s2, ==, 12);
  g_assert_cmpint (s, ==, 8);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  s2 = g_atomic_int_exchange_and_add ((gint*)&s, 1);
G_GNUC_END_IGNORE_DEPRECATIONS
  g_assert_cmpint (s2, ==, 8);
  g_assert_cmpint (s, ==, 9);
  s2 = g_atomic_int_exchange (&s, 55);
  g_assert_cmpint (s2, ==, 9);
  g_assert_cmpint (s, ==, 55);

  g_atomic_pointer_set (&vp, 0);
  vp2 = g_atomic_pointer_get (&vp);
  g_assert_true (vp2 == 0);
  res = g_atomic_pointer_compare_and_exchange (&vp, &s, &s);
  g_assert_false (res);
  g_assert_true (vp == 0);
  res = g_atomic_pointer_compare_and_exchange_full (&vp, &s, &s, &cp);
  g_assert_false (res);
  g_assert_null (vp);
  g_assert_null (cp);
  res = g_atomic_pointer_compare_and_exchange (&vp, NULL, NULL);
  g_assert_true (res);
  g_assert_true (vp == 0);
  res = g_atomic_pointer_compare_and_exchange_full (&vp, NULL, NULL, &cp);
  g_assert_true (res);
  g_assert_null (vp);
  g_assert_null (cp);
  g_assert_null (g_atomic_pointer_exchange (&vp, &s));
  g_assert_true (vp == &s);

  g_atomic_pointer_set (&vp_str, NULL);
  res = g_atomic_pointer_compare_and_exchange (&vp_str, NULL, (char *) str);
  g_assert_true (res);
  g_assert_cmpstr (g_atomic_pointer_exchange (&vp_str, NULL), ==, str);
  g_assert_null (vp_str);
  res = g_atomic_pointer_compare_and_exchange_full (&vp_str, NULL, (char *) str, &cp);
  g_assert_true (res);
  g_assert_cmpstr (vp_str, ==, str);
  g_assert_null (cp);
  res = g_atomic_pointer_compare_and_exchange_full (&vp_str, (char *) str, NULL, &cp);
  g_assert_true (res);
  g_assert_null (vp_str);
  g_assert_true (cp == str);

  /* Note that atomic variables should almost certainly not be marked as
   * `volatile` — see http://isvolatileusefulwiththreads.in/c/. This test exists
   * to make sure that we don’t warn when built against older third party code. */
  g_atomic_pointer_set (&vp_str_vol, NULL);
  g_atomic_pointer_set (&vp_str, (char *) str);
  res = g_atomic_pointer_compare_and_exchange (&vp_str_vol, NULL, (char *) str);
  g_assert_true (res);
  g_assert_cmpstr (g_atomic_pointer_exchange (&vp_str, NULL), ==, str);
  g_assert_null (vp_str);

  res = g_atomic_pointer_compare_and_exchange_full ((char **) &vp_str_vol, (char *) str, NULL, &old_str);
  g_assert_true (res);
  g_assert_true (old_str == str);

  g_atomic_pointer_set (&ip, 0);
  ip2 = g_atomic_pointer_get (&ip);
  g_assert_true (ip2 == 0);
  res = g_atomic_pointer_compare_and_exchange (&ip, NULL, NULL);
  g_assert_true (res);
  g_assert_true (ip == 0);

  res = g_atomic_pointer_compare_and_exchange_full (&ip, NULL, (gpointer) 1, &cp);
  g_assert_true (res);
  g_assert_cmpint ((guintptr) ip, ==, 1);
  g_assert_cmpuint ((guintptr) cp, ==, 0);

  res = g_atomic_pointer_compare_and_exchange_full (&ip, NULL, NULL, &cp);
  g_assert_false (res);
  g_assert_cmpuint ((guintptr) ip, ==, 1);
  g_assert_cmpuint ((guintptr) cp, ==, 1);

  g_atomic_pointer_set (&gu, 0);
  vp = g_atomic_pointer_get (&gu);
  gu2 = (guintptr) vp;
  g_assert_cmpuint (gu2, ==, 0);
  res = g_atomic_pointer_compare_and_exchange (&gu, NULL, NULL);
  g_assert_true (res);
  g_assert_cmpuint (gu, ==, 0);
  res = g_atomic_pointer_compare_and_exchange_full (&gu, NULL, NULL, &cp);
  g_assert_true (res);
  g_assert_cmpuint (gu, ==, 0);
  g_assert_cmpuint ((guintptr) cp, ==, 0);
#pragma clang diagnostic ignored "-Wbad-function-cast"
  gu2 = (guintptr) g_atomic_pointer_add (&gu, 5);
  g_assert_cmpuint (gu2, ==, 0);
  g_assert_cmpuint (gu, ==, 5);
  gu2 = (guintptr) g_atomic_pointer_and (&gu, 6);
  g_assert_cmpuint (gu2, ==, 5);
  g_assert_cmpuint (gu, ==, 4);
  gu2 = (guintptr) g_atomic_pointer_or (&gu, 8);
  g_assert_cmpuint (gu2, ==, 4);
  g_assert_cmpuint (gu, ==, 12);
  gu2 = (guintptr) g_atomic_pointer_xor (&gu, 4);
  g_assert_cmpuint (gu2, ==, 12);
  g_assert_cmpuint (gu, ==, 8);
  vp2 = g_atomic_pointer_exchange (&gu, NULL);
  gu2 = (guintptr) vp2;
  g_assert_cmpuint (gu2, ==, 8);
  g_assert_null ((gpointer) gu);

  g_assert_cmpint (g_atomic_int_get (csp), ==, s);
  g_assert_true (g_atomic_pointer_get (cspp) == csp);
}

#define THREADS 10
#define ROUNDS 10000

gint bucket[THREADS];  /* never contested by threads, not accessed atomically */
gint atomic;  /* (atomic) */

static gpointer
thread_func (gpointer data)
{
  gint idx = GPOINTER_TO_INT (data);
  gint i;
  gint d;

  for (i = 0; i < ROUNDS; i++)
    {
      d = g_random_int_range (-10, 100);
      bucket[idx] += d;
      g_atomic_int_add (&atomic, d);
      g_thread_yield ();
    }

  return NULL;
}

static void
test_threaded (void)
{
  gint sum;
  gint i;
  GThread *threads[THREADS];

  atomic = 0;
  for (i = 0; i < THREADS; i++)
    bucket[i] = 0;

  for (i = 0; i < THREADS; i++)
    threads[i] = g_thread_new ("atomic", thread_func, GINT_TO_POINTER (i));

  for (i = 0; i < THREADS; i++)
    g_thread_join (threads[i]);

  sum = 0;
  for (i = 0; i < THREADS; i++)
    sum += bucket[i];

  g_assert_cmpint (sum, ==, atomic);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/atomic/types", test_types);
  g_test_add_func ("/atomic/threaded", test_threaded);

  return g_test_run ();
}
