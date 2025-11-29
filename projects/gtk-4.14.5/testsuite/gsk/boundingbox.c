/*
 * Copyright © 2023 Red Hat, Inc.
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include <gtk/gtk.h>
#include <gsk/gskboundingboxprivate.h>

static void
init_random_rect (graphene_rect_t *r)
{
  r->origin.x = g_test_rand_double_range (0, 1000);
  r->origin.y = g_test_rand_double_range (0, 1000);
  r->size.width = g_test_rand_double_range (0, 1000);
  r->size.height = g_test_rand_double_range (0, 1000);
}

static void
test_to_rect (void)
{
  graphene_rect_t rect, rect2;
  GskBoundingBox bb;
  graphene_point_t p, p2;

  for (unsigned int i = 0; i < 100; i++)
    {
      init_random_rect (&rect);

      gsk_bounding_box_init_from_rect (&bb, &rect);
      gsk_bounding_box_to_rect (&bb, &rect2);

      graphene_rect_get_top_left (&rect, &p);
      graphene_rect_get_top_left (&rect2, &p2);

      /* Note: that we can't assert equality here is the reason
       * GskBoundingBox exists.
       */
      g_assert_true (graphene_point_near (&p, &p2, 0.001));

      graphene_rect_get_bottom_right (&rect, &p);
      graphene_rect_get_bottom_right (&rect2, &p2);

      g_assert_true (graphene_point_near (&p, &p2, 0.001));
    }
}

static void
test_contains (void)
{
  graphene_rect_t rect;
  GskBoundingBox bb;
  graphene_point_t p;

  for (unsigned int i = 0; i < 100; i++)
    {
      init_random_rect (&rect);

      gsk_bounding_box_init_from_rect (&bb, &rect);

      g_assert_true (gsk_bounding_box_contains_point (&bb, &bb.min));
      g_assert_true (gsk_bounding_box_contains_point (&bb, &bb.max));

      graphene_point_interpolate (&bb.min, &bb.max, 0.5, &p);
      g_assert_true (gsk_bounding_box_contains_point (&bb, &p));
    }
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/bounding-box/to-rect", test_to_rect);
  g_test_add_func ("/bounding-box/contains", test_contains);

  return g_test_run ();
}
