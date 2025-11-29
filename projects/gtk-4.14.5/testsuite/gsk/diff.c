/*
 * Copyright © 2021 Red Hat, Inc.
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
 * Authors: Matthias Clasen
 */

#include <gtk/gtk.h>
#include "gsk/gskrendernodeprivate.h"

static void
test_can_diff_basic (void)
{
  GskRenderNode *container1, *container2;
  GskRenderNode *color1, *color2;
  GskRenderNode *debug1, *debug2;

  color1 = gsk_color_node_new (&(GdkRGBA){0, 1, 0, 1 }, &GRAPHENE_RECT_INIT (0, 0, 10, 10));
  color2 = gsk_color_node_new (&(GdkRGBA){1, 1, 0, 1 }, &GRAPHENE_RECT_INIT (0, 0, 10, 10));

  container1 = gsk_container_node_new (&color1, 1);
  container2 = gsk_container_node_new (&color2, 1);

  debug1 = gsk_debug_node_new (color1, g_strdup ("Debug node!"));
  debug2 = gsk_debug_node_new (color2, g_strdup ("Debug node!"));

  /* We can diff two color nodes */
  g_assert_true (gsk_render_node_can_diff (color1, color2));
  /* We can diff two container nodes */
  g_assert_true (gsk_render_node_can_diff (container1, container2));
  /* We can diff two debug nodes */
  g_assert_true (gsk_render_node_can_diff (debug1, debug2));
  /* We can diff container nodes against anything else */
  g_assert_true (gsk_render_node_can_diff (container1, color2));
  g_assert_true (gsk_render_node_can_diff (color1, container2));

  gsk_render_node_unref (color1);
  gsk_render_node_unref (color2);

  gsk_render_node_unref (container1);
  gsk_render_node_unref (container2);

  gsk_render_node_unref (debug1);
  gsk_render_node_unref (debug2);
}

static void
test_can_diff_transform (void)
{
  GskRenderNode *color1, *color2;
  GskRenderNode *opacity;
  GskRenderNode *transform1, *transform2, *transform3, *transform4;
  GskTransform *t1, *t2;

  color1 = gsk_color_node_new (&(GdkRGBA){0, 1, 0, 1 }, &GRAPHENE_RECT_INIT (0, 0, 10, 10));
  color2 = gsk_color_node_new (&(GdkRGBA){1, 1, 0, 1 }, &GRAPHENE_RECT_INIT (0, 0, 10, 10));
  opacity = gsk_opacity_node_new (color2, 0.5);

  t1 = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (10, 10));
  t2 = gsk_transform_scale (NULL, 2, 1);

  transform1 = gsk_transform_node_new (color1, t1);
  transform2 = gsk_transform_node_new (color2, t1);
  transform3 = gsk_transform_node_new (color2, t2);
  transform4 = gsk_transform_node_new (opacity, t1);

  /* The case we can handle */
  g_assert_true (gsk_render_node_can_diff (transform1, transform2));

  /* These, we can't */
  g_assert_false (gsk_render_node_can_diff (transform1, transform3));
  g_assert_false (gsk_render_node_can_diff (transform1, transform4));

  gsk_render_node_unref (color1);
  gsk_render_node_unref (color2);
  gsk_render_node_unref (opacity);
  gsk_render_node_unref (transform1);
  gsk_render_node_unref (transform2);
  gsk_render_node_unref (transform3);
  gsk_render_node_unref (transform4);

  gsk_transform_unref (t1);
  gsk_transform_unref (t2);
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/node/can-diff/basic", test_can_diff_basic);
  g_test_add_func ("/node/can-diff/transform", test_can_diff_transform);

  return g_test_run ();
}
