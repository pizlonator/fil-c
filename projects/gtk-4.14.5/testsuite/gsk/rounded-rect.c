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

#include "config.h"

#include <gtk/gtk.h>
#include <gsk/gskroundedrectprivate.h>

static void
test_contains_rect (void)
{
  static double points[] = { -5, 0, 5, 10, 15, 85, 90, 95, 100, 105 };
#define LAST (G_N_ELEMENTS(points) - 1)
  GskRoundedRect rounded;
  guint x1, x2, y1, y2;

  gsk_rounded_rect_init_from_rect (&rounded, &GRAPHENE_RECT_INIT (0, 0, 100, 100), 10);

  for (x1 = 0; x1 < G_N_ELEMENTS (points); x1++)
    for (x2 = x1 + 1; x2 < G_N_ELEMENTS (points); x2++)
      for (y1 = 0; y1 < G_N_ELEMENTS (points); y1++)
        for (y2 = y1 + 1; y2 < G_N_ELEMENTS (points); y2++)
          {
            graphene_rect_t rect;
            gboolean inside;

            /* check all points are in the bounding box */
            inside = x1 > 0 && y1 > 0 && x2 < LAST && y2 < LAST;
            /* now check all the corners */
            inside &= x1 > 2 || y1 > 2 || (x1 == 2 && y1 == 2);
            inside &= x2 < LAST - 2 || y1 > 2 || (x2 == LAST - 2 && y1 == 2);
            inside &= x2 < LAST - 2 || y2 < LAST - 2 || (x2 == LAST - 2 && y2 == LAST - 2);
            inside &= x1 > 2 || y2 < LAST - 2 || (x1 == 2 && y2 == LAST - 2);

            graphene_rect_init (&rect, points[x1], points[y1], points[x2] - points[x1], points[y2] - points[y1]);
            if (inside)
              g_assert_true (gsk_rounded_rect_contains_rect (&rounded, &rect));
            else
              g_assert_false (gsk_rounded_rect_contains_rect (&rounded, &rect));
          }
#undef LAST
}

static void
test_intersects_rect (void)
{
  static double points[] = { -1, 0, 1, 99, 100, 101 };
#define ALL_THE_POINTS (G_N_ELEMENTS(points))
#define HALF_THE_POINTS (ALL_THE_POINTS / 2)
  GskRoundedRect rounded;
  guint x1, x2, y1, y2;

  gsk_rounded_rect_init_from_rect (&rounded, &GRAPHENE_RECT_INIT (0, 0, 100, 100), 10);

  for (x1 = 0; x1 < ALL_THE_POINTS; x1++)
    for (x2 = x1 + 1; x2 < ALL_THE_POINTS; x2++)
      for (y1 = 0; y1 < ALL_THE_POINTS; y1++)
        for (y2 = y1 + 1; y2 < ALL_THE_POINTS; y2++)
          {
            graphene_rect_t rect;
            gboolean should_contain_x, should_contain_y;

            graphene_rect_init (&rect, points[x1], points[y1], points[x2] - points[x1], points[y2] - points[y1]);
            should_contain_x = x1 < HALF_THE_POINTS && x2 >= HALF_THE_POINTS && y2 > 1 && y1 < ALL_THE_POINTS - 2;
            should_contain_y = y1 < HALF_THE_POINTS && y2 >= HALF_THE_POINTS && x2 > 1 && x1 < ALL_THE_POINTS - 2;
            if (should_contain_x || should_contain_y)
              g_assert_true (gsk_rounded_rect_intersects_rect (&rounded, &rect));
            else
              g_assert_false (gsk_rounded_rect_intersects_rect (&rounded, &rect));
          }
#undef ALL_THE_POINTS
#undef HALF_THE_POINTS
}

static void
test_contains_point (void)
{
  GskRoundedRect rect;

  gsk_rounded_rect_init (&rect,
                         &GRAPHENE_RECT_INIT (0, 0, 100, 100),
                         &GRAPHENE_SIZE_INIT (0, 0),
                         &GRAPHENE_SIZE_INIT (10, 10),
                         &GRAPHENE_SIZE_INIT (10, 20),
                         &GRAPHENE_SIZE_INIT (20, 10));

  g_assert_true (gsk_rounded_rect_contains_point (&rect, &GRAPHENE_POINT_INIT (50, 50)));
  g_assert_true (gsk_rounded_rect_contains_point (&rect, &GRAPHENE_POINT_INIT (0, 0)));
  g_assert_false (gsk_rounded_rect_contains_point (&rect, &GRAPHENE_POINT_INIT (100, 0)));
  g_assert_false (gsk_rounded_rect_contains_point (&rect, &GRAPHENE_POINT_INIT (100, 100)));
  g_assert_false (gsk_rounded_rect_contains_point (&rect, &GRAPHENE_POINT_INIT (0, 100)));
  g_assert_true (gsk_rounded_rect_contains_point (&rect, &GRAPHENE_POINT_INIT (0, 50)));
  g_assert_true (gsk_rounded_rect_contains_point (&rect, &GRAPHENE_POINT_INIT (50, 0)));
  g_assert_true (gsk_rounded_rect_contains_point (&rect, &GRAPHENE_POINT_INIT (50, 100)));
  g_assert_true (gsk_rounded_rect_contains_point (&rect, &GRAPHENE_POINT_INIT (100, 50)));

  g_assert_true (gsk_rounded_rect_contains_point (&rect, &GRAPHENE_POINT_INIT (95, 5)));
  g_assert_true (gsk_rounded_rect_contains_point (&rect, &GRAPHENE_POINT_INIT (95, 90)));
  g_assert_true (gsk_rounded_rect_contains_point (&rect, &GRAPHENE_POINT_INIT (10, 95)));
}

static void
test_is_circular (void)
{
  GskRoundedRect rect;

  gsk_rounded_rect_init (&rect,
                         &GRAPHENE_RECT_INIT (0, 0, 100, 100),
                         &GRAPHENE_SIZE_INIT (0, 0),
                         &GRAPHENE_SIZE_INIT (10, 10),
                         &GRAPHENE_SIZE_INIT (10, 20),
                         &GRAPHENE_SIZE_INIT (20, 10));

  g_assert_false (gsk_rounded_rect_is_circular (&rect));

  gsk_rounded_rect_init (&rect,
                         &GRAPHENE_RECT_INIT (0, 0, 100, 100),
                         &GRAPHENE_SIZE_INIT (0, 0),
                         &GRAPHENE_SIZE_INIT (10, 10),
                         &GRAPHENE_SIZE_INIT (20, 20),
                         &GRAPHENE_SIZE_INIT (30, 30));

  g_assert_true (gsk_rounded_rect_is_circular (&rect));
}

static void
test_to_float (void)
{
  GskRoundedRect rect;
  float flt[12];

  gsk_rounded_rect_init (&rect,
                         &GRAPHENE_RECT_INIT (0, 11, 22, 33),
                         &GRAPHENE_SIZE_INIT (4, 5),
                         &GRAPHENE_SIZE_INIT (6, 7),
                         &GRAPHENE_SIZE_INIT (8, 9),
                         &GRAPHENE_SIZE_INIT (10, 11));

  gsk_rounded_rect_to_float (&rect, &GRAPHENE_POINT_INIT(0, 0), flt);
  g_assert_true (flt[0] == 0. && flt[1] == 11. && flt[2] == 22. && flt[3] == 33.);
  g_assert_true (flt[4] == 4. && flt[5] == 6.);
  g_assert_true (flt[6] == 8. && flt[7] == 10.);
  g_assert_true (flt[8] == 5. && flt[9] == 7.);
  g_assert_true (flt[10] == 9. && flt[11] == 11.);

  gsk_rounded_rect_to_float (&rect, &GRAPHENE_POINT_INIT(100, 200), flt);
  g_assert_true (flt[0] == 100. && flt[1] == 211. && flt[2] == 22. && flt[3] == 33.);
  g_assert_true (flt[4] == 4. && flt[5] == 6.);
  g_assert_true (flt[6] == 8. && flt[7] == 10.);
  g_assert_true (flt[8] == 5. && flt[9] == 7.);
  g_assert_true (flt[10] == 9. && flt[11] == 11.);
}

#define ROUNDED_RECT_INIT_FULL(x,y,w,h,w0,h0,w1,h1,w2,h2,w3,h3) \
  (GskRoundedRect) { .bounds = GRAPHENE_RECT_INIT (x, y, w, h), \
                     .corner = { \
                       GRAPHENE_SIZE_INIT (w0, h0), \
                       GRAPHENE_SIZE_INIT (w1, h1), \
                       GRAPHENE_SIZE_INIT (w2, h2), \
                       GRAPHENE_SIZE_INIT (w3, h3), \
                   }}

#define ROUNDED_RECT_INIT(x,y,w,h,r) \
  ROUNDED_RECT_INIT_FULL (x, y, w, h, r, r, r, r, r, r, r, r)

#define ROUNDED_RECT_INIT_UNIFORM(x,y,w,h,r1,r2,r3,r4) \
  ROUNDED_RECT_INIT_FULL (x, y, w, h, r1, r1, r2, r2, r3, r3, r4, r4)


static void
test_intersect_with_rect (void)
{
  struct {
    GskRoundedRect rounded;
    graphene_rect_t rect;
    GskRoundedRect expected;
    GskRoundedRectIntersection result;
  } test[] = {
    { ROUNDED_RECT_INIT (20, 50, 100, 100, 50), GRAPHENE_RECT_INIT (60, 80, 60, 70),
      ROUNDED_RECT_INIT (0, 0, 0, 0, 0), GSK_INTERSECTION_NOT_REPRESENTABLE },
    { ROUNDED_RECT_INIT (0, 0, 100, 100, 10), GRAPHENE_RECT_INIT (0, 0, 100, 100),
      ROUNDED_RECT_INIT (0, 0, 100, 100, 10), GSK_INTERSECTION_NONEMPTY },
    { ROUNDED_RECT_INIT (0, 0, 100, 100, 10), GRAPHENE_RECT_INIT (0, 0, 80, 80),
      ROUNDED_RECT_INIT_UNIFORM (0, 0, 80, 80, 10, 0, 0, 0), GSK_INTERSECTION_NONEMPTY },
    { ROUNDED_RECT_INIT (0, 0, 100, 100, 10), GRAPHENE_RECT_INIT (10, 10, 80, 80),
      ROUNDED_RECT_INIT (10, 10, 80, 80, 0), GSK_INTERSECTION_NONEMPTY },
    { ROUNDED_RECT_INIT (0, 0, 100, 100, 10), GRAPHENE_RECT_INIT (10, 15, 100, 70),
      ROUNDED_RECT_INIT (10, 15, 90, 70, 0), GSK_INTERSECTION_NONEMPTY },
    { ROUNDED_RECT_INIT (0, 0, 100, 100, 10), GRAPHENE_RECT_INIT (110, 0, 10, 10),
      ROUNDED_RECT_INIT (0, 0, 0, 0, 0), GSK_INTERSECTION_EMPTY },
    { ROUNDED_RECT_INIT (0, 0, 100, 100, 10), GRAPHENE_RECT_INIT (5, 5, 90, 90),
      ROUNDED_RECT_INIT (5, 5, 90, 90, 0), GSK_INTERSECTION_NONEMPTY },
    { ROUNDED_RECT_INIT (0, 0, 100, 100, 10), GRAPHENE_RECT_INIT (1, 1, 1, 1),
      ROUNDED_RECT_INIT (0, 0, 0, 0, 0), GSK_INTERSECTION_EMPTY },
    { ROUNDED_RECT_INIT (0, 0, 100, 100, 10), GRAPHENE_RECT_INIT (5, -5, 10, 20),
      ROUNDED_RECT_INIT (0, 0, 0, 0, 0), GSK_INTERSECTION_NOT_REPRESENTABLE },
    { ROUNDED_RECT_INIT_UNIFORM (-200, 0, 200, 100, 0, 0, 0, 40), GRAPHENE_RECT_INIT (-200, 0, 160, 100),
      ROUNDED_RECT_INIT_UNIFORM (-200, 0, 160, 100, 0, 0, 0, 40), GSK_INTERSECTION_NONEMPTY },

    { ROUNDED_RECT_INIT_UNIFORM (0, 0, 50, 50, 50, 0, 50, 0), GRAPHENE_RECT_INIT (0, 0, 49, 49),
      ROUNDED_RECT_INIT_UNIFORM (0, 0, 0, 0, 0, 0, 0, 0), GSK_INTERSECTION_NOT_REPRESENTABLE },
    { ROUNDED_RECT_INIT_UNIFORM (0, 0, 50, 50, 50, 0, 50, 0), GRAPHENE_RECT_INIT (1, 0, 49, 49),
      ROUNDED_RECT_INIT_UNIFORM (0, 0, 0, 0, 0, 0, 0, 0), GSK_INTERSECTION_NOT_REPRESENTABLE },
    { ROUNDED_RECT_INIT_UNIFORM (0, 0, 50, 50, 50, 0, 50, 0), GRAPHENE_RECT_INIT (0, 1, 49, 49),
      ROUNDED_RECT_INIT_UNIFORM (0, 0, 0, 0, 0, 0, 0, 0), GSK_INTERSECTION_NOT_REPRESENTABLE },
    { ROUNDED_RECT_INIT_UNIFORM (0, 0, 50, 50, 50, 0, 50, 0), GRAPHENE_RECT_INIT (1, 1, 49, 49),
      ROUNDED_RECT_INIT_UNIFORM (0, 0, 0, 0, 0, 0, 0, 0), GSK_INTERSECTION_NOT_REPRESENTABLE },
    { ROUNDED_RECT_INIT_UNIFORM (0, 0, 50, 50, 0, 50, 0, 50), GRAPHENE_RECT_INIT (0, 0, 49, 49),
      ROUNDED_RECT_INIT_UNIFORM (0, 0, 0, 0, 0, 0, 0, 0), GSK_INTERSECTION_NOT_REPRESENTABLE },
    { ROUNDED_RECT_INIT_UNIFORM (0, 0, 50, 50, 0, 50, 0, 50), GRAPHENE_RECT_INIT (1, 0, 49, 49),
      ROUNDED_RECT_INIT_UNIFORM (0, 0, 0, 0, 0, 0, 0, 0), GSK_INTERSECTION_NOT_REPRESENTABLE },
    { ROUNDED_RECT_INIT_UNIFORM (0, 0, 50, 50, 0, 50, 0, 50), GRAPHENE_RECT_INIT (0, 1, 49, 49),
      ROUNDED_RECT_INIT_UNIFORM (0, 0, 0, 0, 0, 0, 0, 0), GSK_INTERSECTION_NOT_REPRESENTABLE },
    { ROUNDED_RECT_INIT_UNIFORM (0, 0, 50, 50, 0, 50, 0, 50), GRAPHENE_RECT_INIT (1, 1, 49, 49),
      ROUNDED_RECT_INIT_UNIFORM (0, 0, 0, 0, 0, 0, 0, 0), GSK_INTERSECTION_NOT_REPRESENTABLE },
  };

  for (unsigned int i = 0; i < G_N_ELEMENTS (test); i++)
    {
      GskRoundedRect out;
      GskRoundedRectIntersection res;

      if (g_test_verbose ())
        g_test_message ("intersection test %u", i);

      memset (&out, 0, sizeof (GskRoundedRect));

      res = gsk_rounded_rect_intersect_with_rect (&test[i].rounded, &test[i].rect, &out);
      g_assert_true (res == test[i].result);
      if (res == GSK_INTERSECTION_NONEMPTY)
        {
          if (!gsk_rounded_rect_equal (&out, &test[i].expected))
            {
              char *s = gsk_rounded_rect_to_string (&test[i].expected);
              char *s2 = gsk_rounded_rect_to_string (&out);
              g_test_message ("expected %s, got %s\n", s, s2);
            }
          g_assert_true (gsk_rounded_rect_equal (&out, &test[i].expected));
        }

      g_assert_true ((res != GSK_INTERSECTION_EMPTY) == gsk_rounded_rect_intersects_rect (&test[i].rounded, &test[i].rect));
    }
}

static void
test_intersect (void)
{
  struct {
    GskRoundedRect a;
    GskRoundedRect b;
    GskRoundedRectIntersection result;
    GskRoundedRect expected;
  } test[] = {
    {
      ROUNDED_RECT_INIT(0, 0, 100, 100, 0),
      ROUNDED_RECT_INIT(0, 0, 100, 100, 20),
      GSK_INTERSECTION_NONEMPTY,
      ROUNDED_RECT_INIT(0, 0, 100, 100, 20),
    },
    {
      ROUNDED_RECT_INIT(0, 0, 100, 100, 20),
      ROUNDED_RECT_INIT(50, 50, 100, 100, 20),
      GSK_INTERSECTION_NONEMPTY,
      ROUNDED_RECT_INIT_UNIFORM(50, 50, 50, 50, 20, 0, 20, 0),
    },
    {
      ROUNDED_RECT_INIT(0, 0, 100, 100, 20),
      ROUNDED_RECT_INIT(50, 0, 100, 100, 20),
      GSK_INTERSECTION_NONEMPTY,
      ROUNDED_RECT_INIT(50, 0, 50, 100, 20),
    },
    {
      ROUNDED_RECT_INIT(0, 0, 100, 100, 20),
      ROUNDED_RECT_INIT(0, 50, 100, 100, 20),
      GSK_INTERSECTION_NONEMPTY,
      ROUNDED_RECT_INIT(0, 50, 100, 50, 20),
    },
    {
      ROUNDED_RECT_INIT(0, 0, 100, 100, 20),
      ROUNDED_RECT_INIT(-50, -50, 100, 100, 20),
      GSK_INTERSECTION_NONEMPTY,
      ROUNDED_RECT_INIT_UNIFORM(0, 0, 50, 50, 20, 0, 20, 0),
    },
    {
      ROUNDED_RECT_INIT(0, 0, 100, 100, 20),
      ROUNDED_RECT_INIT(0, -50, 100, 100, 20),
      GSK_INTERSECTION_NONEMPTY,
      ROUNDED_RECT_INIT(0, 0, 100, 50, 20),
    },
    {
      ROUNDED_RECT_INIT(0, 0, 100, 100, 20),
      ROUNDED_RECT_INIT(-50, 0, 100, 100, 20),
      GSK_INTERSECTION_NONEMPTY,
      ROUNDED_RECT_INIT(0, 0, 50, 100, 20),
    },
    {
      ROUNDED_RECT_INIT(0, 0, 100, 100, 20),
      ROUNDED_RECT_INIT(10, 10, 80, 80, 20),
      GSK_INTERSECTION_NONEMPTY,
      ROUNDED_RECT_INIT(10, 10, 80, 80, 20),
    },
    {
      ROUNDED_RECT_INIT(0, 0, 100, 100, 20),
      ROUNDED_RECT_INIT(10, 10, 80, 80, 10),
      GSK_INTERSECTION_NONEMPTY,
      ROUNDED_RECT_INIT(10, 10, 80, 80, 10),
    },
    {
      ROUNDED_RECT_INIT(0, 0, 100, 100, 40),
      ROUNDED_RECT_INIT(10, 10, 80, 80, 0),
      GSK_INTERSECTION_NOT_REPRESENTABLE,
    },
    {
      ROUNDED_RECT_INIT(10, 10, 100, 100, 40),
      ROUNDED_RECT_INIT(30, 0, 40, 40, 0),
      GSK_INTERSECTION_NOT_REPRESENTABLE,
    },
    {
      ROUNDED_RECT_INIT(10, 10, 100, 100, 40),
      ROUNDED_RECT_INIT(0, 0, 100, 20, 0),
      GSK_INTERSECTION_NOT_REPRESENTABLE,
    },
    {
      ROUNDED_RECT_INIT_UNIFORM(647, 18, 133, 35, 5, 0, 0, 5),
      ROUNDED_RECT_INIT_UNIFORM(14, 12, 1666, 889, 8, 8, 0, 0),
      GSK_INTERSECTION_NONEMPTY,
      ROUNDED_RECT_INIT_UNIFORM(647, 18, 133, 35, 5, 0, 0, 5),
    },
    {
      ROUNDED_RECT_INIT_UNIFORM(0, 0, 100, 100, 100, 0, 0, 0),
      ROUNDED_RECT_INIT_UNIFORM(0, 0, 100, 100, 0, 0, 100, 0),
      GSK_INTERSECTION_NONEMPTY,
      ROUNDED_RECT_INIT_UNIFORM(0, 0, 100, 100, 100, 0, 100, 0),
    },
    {
      ROUNDED_RECT_INIT_UNIFORM(0, 0, 100, 100, 100, 0, 0, 0),
      ROUNDED_RECT_INIT_UNIFORM(-20, -20, 100, 100, 0, 0, 100, 0),
      GSK_INTERSECTION_NOT_REPRESENTABLE,
    },
    {
      ROUNDED_RECT_INIT_UNIFORM(0, 0, 50, 50, 0, 0, 50, 0),
      ROUNDED_RECT_INIT_UNIFORM(0, 0, 20, 20, 20, 0, 0, 0),
      GSK_INTERSECTION_NOT_REPRESENTABLE, /* FIXME: should be empty */
    },
    {
      ROUNDED_RECT_INIT_UNIFORM(0, 0, 50, 50, 0, 0, 50, 0),
      ROUNDED_RECT_INIT_UNIFORM(0, 0, 21, 21, 21, 0, 0, 0),
      GSK_INTERSECTION_NOT_REPRESENTABLE,
    },
    {
      ROUNDED_RECT_INIT_UNIFORM(0, 0, 50, 50, 50, 0, 50, 0),
      ROUNDED_RECT_INIT_UNIFORM(0, 0, 50, 50, 0, 50, 0, 50),
      GSK_INTERSECTION_NOT_REPRESENTABLE,
    },
  };
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (test); i++)
    {
      GskRoundedRect out;
      GskRoundedRectIntersection res;

      if (g_test_verbose ())
        g_test_message ("intersection test %zu", i);

      memset (&out, 0, sizeof (GskRoundedRect));

      res = gsk_rounded_rect_intersection (&test[i].a, &test[i].b, &out);
      g_assert_cmpint (res, ==, test[i].result);
      if (res == GSK_INTERSECTION_NONEMPTY)
        {
          if (!gsk_rounded_rect_equal (&out, &test[i].expected))
            {
              char *a = gsk_rounded_rect_to_string (&test[i].a);
              char *b = gsk_rounded_rect_to_string (&test[i].b);
              char *expected = gsk_rounded_rect_to_string (&test[i].expected);
              char *result = gsk_rounded_rect_to_string (&out);
              g_test_message ("     A = %s\n"
                              "     B = %s\n"
                              "expected %s\n"
                              "     got %s\n",
                              a, b, expected, result);
            }
          g_assert_true (gsk_rounded_rect_equal (&out, &test[i].expected));
        }
    }
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/rounded-rect/contains-rect", test_contains_rect);
  g_test_add_func ("/rounded-rect/intersects-rect", test_intersects_rect);
  g_test_add_func ("/rounded-rect/contains-point", test_contains_point);
  g_test_add_func ("/rounded-rect/is-circular", test_is_circular);
  g_test_add_func ("/rounded-rect/to-float", test_to_float);
  g_test_add_func ("/rounded-rect/intersect-with-rect", test_intersect_with_rect);
  g_test_add_func ("/rounded-rect/intersect", test_intersect);

  return g_test_run ();
}
