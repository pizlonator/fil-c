/*
 * Copyright © 2020 Benjamin Otte
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include <gtk/gtk.h>

#define GDK_ARRAY_NO_UNDEF

#include "../../gdk/gdkarrayimpl.c"

static void
gdk_array(test_simple) (void)
{
  GdkArray v;
  gsize i;

  gdk_array(init) (&v);

  for (i = 0; i < 1000; i++)
    {
      g_assert_cmpint (gdk_array(get_size) (&v), ==, i);
      g_assert_cmpint (gdk_array(get_size) (&v), <=, gdk_array(get_capacity) (&v));
      gdk_array(append) (&v, i);
#ifdef GDK_ARRAY_NULL_TERMINATED
      g_assert_cmpint (*gdk_array(index) (&v, gdk_array(get_size) (&v)), ==, 0);
#endif
    }
  g_assert_cmpint (gdk_array(get_size) (&v), ==, i);
  g_assert_cmpint (gdk_array(get_size) (&v), <=, gdk_array(get_capacity) (&v));

  for (i = 0; i < 1000; i++)
    {
      g_assert_cmpint (gdk_array(get) (&v, i), ==, i);
    }

  gdk_array(clear) (&v);
}

static void
gdk_array(test_splice) (void)
{
  GdkArray v;
  gsize i, j, sum;
  gsize pos, add, remove;
  int additions[4] = { 0, 1, 2, 3 };

  gdk_array(init) (&v);
  sum = 0;

  for (i = 0; i < 1000; i++)
    {
      gsize old_size = gdk_array(get_size) (&v);

      pos = g_test_rand_int_range (0, old_size + 1);
      g_assert_true (pos <= old_size);
      remove = g_test_rand_int_range (0, 4);
      remove = MIN (remove, old_size - pos);
      add = g_test_rand_int_range (0, 4);

      for (j = 0; j < remove; j++)
        sum -= gdk_array(get) (&v, pos + j);
      for (j = 0; j < add; j++)
        sum += ++additions[j];

      gdk_array(splice) (&v, pos, remove, FALSE, additions, add);
      {
        gsize total = 0;
        for (j = 0; j < gdk_array(get_size) (&v); j++)
          total += gdk_array(get) (&v, j);
        g_assert_cmpint (total, ==, sum);
      }

      g_assert_cmpint (gdk_array(get_size) (&v), ==, old_size + add - remove);
      g_assert_cmpint (gdk_array(get_size) (&v), <=, gdk_array(get_capacity) (&v));
#ifdef GDK_ARRAY_NULL_TERMINATED
      if (gdk_array(get_size) (&v))
        g_assert_cmpint (*gdk_array(index) (&v, gdk_array(get_size) (&v)), ==, 0);
#endif
      for (j = 0; j < add; j++)
        g_assert_cmpint (gdk_array(get) (&v, pos + j), ==, additions[j]);
    }

  for (i = 0; i < gdk_array(get_size) (&v); i++)
    {
      sum -= gdk_array(get) (&v, i);
    }
  g_assert_cmpint (sum, ==, 0);

  gdk_array(clear) (&v);
}

#undef _T_
#undef GdkArray
#undef gdk_array_paste_more
#undef gdk_array_paste
#undef gdk_array
#undef GDK_ARRAY_REAL_SIZE

#undef GDK_ARRAY_ELEMENT_TYPE
#undef GDK_ARRAY_NAME
#undef GDK_ARRAY_TYPE_NAME
#undef GDK_ARRAY_PREALLOC
#undef GDK_ARRAY_NULL_TERMINATED

