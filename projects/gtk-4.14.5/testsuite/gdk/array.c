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

#include <locale.h>

#include <gtk/gtk.h>

static void
int_free_func (int data)
{
}

#define GDK_ARRAY_ELEMENT_TYPE int
#define GDK_ARRAY_NAME int_array
#define GDK_ARRAY_TYPE_NAME IntVector
#include "arrayimpl.c"

#define GDK_ARRAY_ELEMENT_TYPE int
#define GDK_ARRAY_NAME pre_int_array
#define GDK_ARRAY_TYPE_NAME PreIntVector
#define GDK_ARRAY_PREALLOC 100
#include "arrayimpl.c"

#define GDK_ARRAY_ELEMENT_TYPE int
#define GDK_ARRAY_NAME free_int_array
#define GDK_ARRAY_TYPE_NAME FreeIntVector
#define GDK_ARRAY_FREE_FUNC int_free_func
#include "arrayimpl.c"

#define GDK_ARRAY_ELEMENT_TYPE int
#define GDK_ARRAY_NAME pre_free_int_array
#define GDK_ARRAY_TYPE_NAME PreFreeIntVector
#define GDK_ARRAY_PREALLOC 100
#define GDK_ARRAY_FREE_FUNC int_free_func
#include "arrayimpl.c"

#define GDK_ARRAY_ELEMENT_TYPE int
#define GDK_ARRAY_NAME null_int_array
#define GDK_ARRAY_TYPE_NAME NullIntVector
#define GDK_ARRAY_NULL_TERMINATED 1
#include "arrayimpl.c"

#define GDK_ARRAY_ELEMENT_TYPE int
#define GDK_ARRAY_NAME null_pre_int_array
#define GDK_ARRAY_TYPE_NAME NullPreIntVector
#define GDK_ARRAY_PREALLOC 100
#define GDK_ARRAY_NULL_TERMINATED 1
#include "arrayimpl.c"

#define GDK_ARRAY_ELEMENT_TYPE int
#define GDK_ARRAY_NAME null_free_int_array
#define GDK_ARRAY_TYPE_NAME NullFreeIntVector
#define GDK_ARRAY_FREE_FUNC int_free_func
#define GDK_ARRAY_NULL_TERMINATED 1
#include "arrayimpl.c"

#define GDK_ARRAY_ELEMENT_TYPE int
#define GDK_ARRAY_NAME null_pre_free_int_array
#define GDK_ARRAY_TYPE_NAME NullPreFreeIntVector
#define GDK_ARRAY_PREALLOC 100
#define GDK_ARRAY_FREE_FUNC int_free_func
#define GDK_ARRAY_NULL_TERMINATED 1
#include "arrayimpl.c"

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  g_test_add_func ("/intarray/simple", int_array_test_simple);
  g_test_add_func ("/intarray/prealloc/simple", pre_int_array_test_simple);
  g_test_add_func ("/intarray/freefunc/simple", free_int_array_test_simple);
  g_test_add_func ("/intarray/prealloc/freefunc/simple", pre_free_int_array_test_simple);
  g_test_add_func ("/intarray/null/simple", null_int_array_test_simple);
  g_test_add_func ("/intarray/null/prealloc/simple", null_pre_int_array_test_simple);
  g_test_add_func ("/intarray/null/freefunc/simple", null_free_int_array_test_simple);
  g_test_add_func ("/intarray/null/prealloc/freefunc/simple", null_pre_free_int_array_test_simple);
  g_test_add_func ("/intarray/splice", int_array_test_splice);
  g_test_add_func ("/intarray/prealloc/splice", pre_int_array_test_splice);
  g_test_add_func ("/intarray/freefunc/splice", free_int_array_test_splice);
  g_test_add_func ("/intarray/prealloc/freefunc/splice", pre_free_int_array_test_splice);
  g_test_add_func ("/intarray/null/splice", null_int_array_test_splice);
  g_test_add_func ("/intarray/null/prealloc/splice", null_pre_int_array_test_splice);
  g_test_add_func ("/intarray/null/freefunc/splice", null_free_int_array_test_splice);
  g_test_add_func ("/intarray/null/prealloc/freefunc/splice", null_pre_free_int_array_test_splice);

  return g_test_run ();
}
