#include <gtk/gtk.h>
#include "gsk/gskrectprivate.h"
#include "../reftests/reftest-compare.h"

static void
test_normalize (GskRenderNode *node1,
                GskRenderNode *node2)
{
  GskRenderer *renderer = gsk_ngl_renderer_new ();
  graphene_rect_t bounds1, bounds2;
  GdkTexture *texture1, *texture2, *diff;
  GError *error = NULL;

  gsk_renderer_realize_for_display (renderer, gdk_display_get_default (), &error);
  g_assert_no_error (error);

  gsk_render_node_get_bounds (node1, &bounds1);
  gsk_render_node_get_bounds (node2, &bounds2);

  texture1 = gsk_renderer_render_texture (renderer, node1, &bounds1);
  texture2 = gsk_renderer_render_texture (renderer, node2, &bounds2);

  diff = reftest_compare_textures (texture1, texture2);
  g_assert_null (diff);

  g_object_unref (texture1);
  g_object_unref (texture2);
  gsk_renderer_unrealize (renderer);
  g_object_unref (renderer);
}

static void
test_normalize_color (void)
{
  GskRenderNode *node1, *node2;
  GdkRGBA red = { 1, 0, 0, 1 };

  node1 = gsk_color_node_new (&red, &GRAPHENE_RECT_INIT (0, 0, 100, 100));
  node2 = gsk_color_node_new (&red, &GRAPHENE_RECT_INIT (0, 100, 100, -100));

  test_normalize (node1, node2);

  gsk_render_node_unref (node1);
  gsk_render_node_unref (node2);
}

static void
test_normalize_linear_gradient (void)
{
  GskRenderNode *node1, *node2;
  GskColorStop stops[] = {
    { 0, { 1, 0, 0, 1 } },
    { 1, { 0, 0, 1, 1 } },
  };

  node1 = gsk_linear_gradient_node_new (&GRAPHENE_RECT_INIT (0, 0, 100, 100),
                                        &GRAPHENE_POINT_INIT (0, 0),
                                        &GRAPHENE_POINT_INIT (100, 100),
                                        stops,
                                        G_N_ELEMENTS (stops));
  node2 = gsk_linear_gradient_node_new (&GRAPHENE_RECT_INIT (0, 100, 100, -100),
                                        &GRAPHENE_POINT_INIT (0, 0),
                                        &GRAPHENE_POINT_INIT (100, 100),
                                        stops,
                                        G_N_ELEMENTS (stops));

  test_normalize (node1, node2);

  gsk_render_node_unref (node1);
  gsk_render_node_unref (node2);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/node/normalize/color", test_normalize_color);
  g_test_add_func ("/node/normalize/linear-gradient", test_normalize_linear_gradient);

  return g_test_run ();
}
