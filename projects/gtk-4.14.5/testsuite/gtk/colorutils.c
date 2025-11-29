/* Copyright (C) 2023 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */
#include <gtk/gtk.h>
#include <gtk/gtkcolorutils.h>

struct {
  float r, g, b;
  float h, s, v;
} tests[] = {
  { 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 0, 0, 1 },
  { 1, 0, 0, 0, 1, 1 },
  { 1, 1, 0, 1.0 / 6.0, 1, 1 },
  { 0, 1, 0, 2.0 / 6.0, 1, 1 },
  { 0, 1, 1, 3.0 / 6.0, 1, 1 },
  { 0, 0, 1, 4.0 / 6.0, 1, 1 },
  { 1, 0, 1, 5.0 / 6.0, 1, 1 },
};

/* Close enough for float precision to match, even with some
 * rounding errors */
#define EPSILON 1e-6

static void
test_roundtrips (void)
{
  for (unsigned int i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      float r, g, b;
      float h, s, v;

      g_print ("color %u\n", i);
      gtk_hsv_to_rgb (tests[i].h, tests[i].s, tests[i].v, &r, &g, &b);
      g_assert_cmpfloat_with_epsilon (r, tests[i].r, EPSILON);
      g_assert_cmpfloat_with_epsilon (g, tests[i].g, EPSILON);
      g_assert_cmpfloat_with_epsilon (b, tests[i].b, EPSILON);
      gtk_rgb_to_hsv (tests[i].r, tests[i].g, tests[i].b, &h, &s, &v);
      g_assert_cmpfloat_with_epsilon (h, tests[i].h, EPSILON);
      g_assert_cmpfloat_with_epsilon (s, tests[i].s, EPSILON);
      g_assert_cmpfloat_with_epsilon (v, tests[i].v, EPSILON);
    }
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/color/roundtrips", test_roundtrips);

  return g_test_run();
}
