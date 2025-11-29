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
#include "gsk/gskpathprivate.h"
#include "gsk/gskcontourprivate.h"

static gboolean
add_segment (GskPathOperation        op,
             const graphene_point_t *pts,
             gsize                   n_pts,
             float                   weight,
             gpointer                user_data)
{
  GskPathBuilder *builder = user_data;

  switch (op)
    {
    case GSK_PATH_MOVE:
      gsk_path_builder_move_to (builder, pts[0].x, pts[0].y);
      break;
    case GSK_PATH_LINE:
      gsk_path_builder_line_to (builder, pts[1].x, pts[1].y);
      break;
    case GSK_PATH_QUAD:
      gsk_path_builder_quad_to (builder,
                                pts[1].x, pts[1].y,
                                pts[2].x, pts[2].y);
      break;
    case GSK_PATH_CUBIC:
      gsk_path_builder_cubic_to (builder,
                                 pts[1].x, pts[1].y,
                                 pts[2].x, pts[2].y,
                                 pts[3].x, pts[3].y);
      break;
    case GSK_PATH_CONIC:
      gsk_path_builder_conic_to (builder,
                                 pts[1].x, pts[1].y,
                                 pts[2].x, pts[2].y,
                                 weight);
      break;
    case GSK_PATH_CLOSE:
      gsk_path_builder_close (builder);
      break;
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static GskPath *
convert_to_standard_contour (GskPath *path)
{
  GskPathBuilder *builder;

  builder = gsk_path_builder_new ();
  gsk_path_foreach (path, -1, add_segment, builder);
  return gsk_path_builder_free_to_path (builder);
}

static void
test_circle_roundtrip (void)
{
  GskPathBuilder *builder;
  GskPath *path, *path1;
  const GskContour *contour, *contour1;
  char *s;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (100, 100), 33);
  path = gsk_path_builder_free_to_path (builder);
  contour = gsk_path_get_contour (path, 0);

  g_assert_cmpstr (gsk_contour_get_type_name (contour), ==, "GskCircleContour");

  s = gsk_path_to_string (path);
  path1 = gsk_path_parse (s);
  contour1 = gsk_path_get_contour (path1, 0);
  g_free (s);

  g_assert_cmpstr (gsk_contour_get_type_name (contour1), ==, "GskCircleContour");

  gsk_path_unref (path1);
  gsk_path_unref (path);
}

static void
test_circle_winding (void)
{
  GskPathBuilder *builder;
  GskPath *path, *path1, *path2;
  const GskContour *contour, *contour1, *contour2;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (100, 100), 33);
  path = gsk_path_builder_free_to_path (builder);
  contour = gsk_path_get_contour (path, 0);

  path1 = convert_to_standard_contour (path);
  contour1 = gsk_path_get_contour (path1, 0);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_reverse_path (builder, path);
  path2 = gsk_path_builder_free_to_path (builder);
  contour2 = gsk_path_get_contour (path2, 0);

  g_assert_true (gsk_contour_get_winding (contour, &GRAPHENE_POINT_INIT (100, 100)) == 1);
  g_assert_true (gsk_contour_get_winding (contour1, &GRAPHENE_POINT_INIT (100, 100)) == 1);
  g_assert_true (gsk_contour_get_winding (contour2, &GRAPHENE_POINT_INIT (100, 100)) == -1);

  gsk_path_unref (path2);
  gsk_path_unref (path1);
  gsk_path_unref (path);
}

static void
test_rounded_rect_roundtrip (void)
{
  GskRoundedRect rr;
  GskPathBuilder *builder;
  GskPath *path, *path2;
  const GskContour *contour;
  char *s;

  /* our parser only recognizes 'complete' rounded rects
   * (i.e. no empty curves omitted).
   */
  rr.bounds = GRAPHENE_RECT_INIT (100, 100, 200, 150);
  rr.corner[GSK_CORNER_TOP_LEFT] = GRAPHENE_SIZE_INIT (10, 10);
  rr.corner[GSK_CORNER_TOP_RIGHT] = GRAPHENE_SIZE_INIT (20, 10);
  rr.corner[GSK_CORNER_BOTTOM_RIGHT] = GRAPHENE_SIZE_INIT (20, 20);
  rr.corner[GSK_CORNER_BOTTOM_LEFT] = GRAPHENE_SIZE_INIT (5, 10);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_rounded_rect (builder, &rr);
  path = gsk_path_builder_free_to_path (builder);
  contour = gsk_path_get_contour (path, 0);

  g_assert_cmpstr (gsk_contour_get_type_name (contour), ==, "GskRoundedRectContour");

  s = gsk_path_to_string (path);
  path2 = gsk_path_parse (s);
  contour = gsk_path_get_contour (path2, 0);

  g_assert_cmpstr (gsk_contour_get_type_name (contour), ==, "GskRoundedRectContour");

  g_free (s);
  gsk_path_unref (path2);
  gsk_path_unref (path);
}

static void
test_rounded_rect_winding (void)
{
  GskRoundedRect rr;
  GskPathBuilder *builder;
  GskPath *path, *path1, *path2;
  const GskContour *contour, *contour1, *contour2;

  rr.bounds = GRAPHENE_RECT_INIT (100, 100, 200, 150);
  rr.corner[GSK_CORNER_TOP_LEFT] = GRAPHENE_SIZE_INIT (10, 10);
  rr.corner[GSK_CORNER_TOP_RIGHT] = GRAPHENE_SIZE_INIT (20, 10);
  rr.corner[GSK_CORNER_BOTTOM_RIGHT] = GRAPHENE_SIZE_INIT (20, 0);
  rr.corner[GSK_CORNER_BOTTOM_LEFT] = GRAPHENE_SIZE_INIT (0, 0);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_rounded_rect (builder, &rr);
  path = gsk_path_builder_free_to_path (builder);
  contour = gsk_path_get_contour (path, 0);

  path1 = convert_to_standard_contour (path);
  contour1 = gsk_path_get_contour (path1, 0);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_reverse_path (builder, path);
  path2 = gsk_path_builder_free_to_path (builder);
  contour2 = gsk_path_get_contour (path2, 0);

  g_assert_true (gsk_contour_get_winding (contour, &GRAPHENE_POINT_INIT (150, 150)) == 1);
  g_assert_true (gsk_contour_get_winding (contour1, &GRAPHENE_POINT_INIT (150, 150)) == 1);
  g_assert_true (gsk_contour_get_winding (contour2, &GRAPHENE_POINT_INIT (150, 150)) == -1);

  gsk_path_unref (path2);
  gsk_path_unref (path1);
}

static void
test_rect_roundtrip (void)
{
  graphene_rect_t rect;
  GskPathBuilder *builder;
  GskPath *path, *path2;
  const GskContour *contour;
  char *s;

  rect = GRAPHENE_RECT_INIT (100, 100, 200, 150);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_rect (builder, &rect);
  path = gsk_path_builder_free_to_path (builder);
  contour = gsk_path_get_contour (path, 0);

  g_assert_cmpstr (gsk_contour_get_type_name (contour), ==, "GskRectContour");

  s = gsk_path_to_string (path);
  path2 = gsk_path_parse (s);
  contour = gsk_path_get_contour (path2, 0);

  g_assert_cmpstr (gsk_contour_get_type_name (contour), ==, "GskRectContour");

  g_free (s);
  gsk_path_unref (path2);
  gsk_path_unref (path);
}

static void
test_rect_winding (void)
{
  GskPathBuilder *builder;
  GskPath *path, *path1, *path2, *path3;
  const GskContour *contour, *contour1, *contour2, *contour3;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_rect (builder, &GRAPHENE_RECT_INIT (100, 100, 200, 150));
  path = gsk_path_builder_free_to_path (builder);
  contour = gsk_path_get_contour (path, 0);

  path1 = convert_to_standard_contour (path);
  contour1 = gsk_path_get_contour (path1, 0);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_reverse_path (builder, path);
  path2 = gsk_path_builder_free_to_path (builder);
  contour2 = gsk_path_get_contour (path2, 0);

  path3 = convert_to_standard_contour (path2);
  contour3 = gsk_path_get_contour (path3, 0);

  g_assert_true (gsk_contour_get_winding (contour, &GRAPHENE_POINT_INIT (150, 150)) == 1);
  g_assert_true (gsk_contour_get_winding (contour1, &GRAPHENE_POINT_INIT (150, 150)) == 1);
  g_assert_true (gsk_contour_get_winding (contour2, &GRAPHENE_POINT_INIT (150, 150)) == -1);
  g_assert_true (gsk_contour_get_winding (contour3, &GRAPHENE_POINT_INIT (150, 150)) == -1);

  gsk_path_unref (path3);
  gsk_path_unref (path2);
  gsk_path_unref (path1);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/path/circle/roundtrip", test_circle_roundtrip);
  g_test_add_func ("/path/circle/winding", test_circle_winding);
  g_test_add_func ("/path/rounded-rect/roundtrip", test_rounded_rect_roundtrip);
  g_test_add_func ("/path/rounded-rect/winding", test_rounded_rect_winding);
  g_test_add_func ("/path/rect/roundtrip", test_rect_roundtrip);
  g_test_add_func ("/path/rect/winding", test_rect_winding);

  return g_test_run ();
}
