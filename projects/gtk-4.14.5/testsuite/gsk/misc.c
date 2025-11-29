#include <gtk/gtk.h>
#include "gsk/gskrendernodeprivate.h"

#include <gobject/gvaluecollector.h>

static void
test_rendernode_gvalue (void)
{
  GValue value = G_VALUE_INIT;
  GValue value2 = G_VALUE_INIT;
  GskRenderNode *node, *node2;

  g_assert_false (GSK_VALUE_HOLDS_RENDER_NODE (&value));
  g_value_init (&value, GSK_TYPE_RENDER_NODE);
  g_assert_true (GSK_VALUE_HOLDS_RENDER_NODE (&value));

  node = gsk_value_get_render_node (&value);
  g_assert_null (node);

  node = gsk_color_node_new (&(GdkRGBA){0,1,1,1}, &GRAPHENE_RECT_INIT (0, 0, 50, 50));
  gsk_value_set_render_node (&value, node);

  node2 = gsk_value_dup_render_node (&value);
  g_assert_true (node == node2);

  g_value_reset (&value);
  gsk_value_take_render_node (&value, node);

  g_value_init (&value2, GSK_TYPE_RENDER_NODE);
  g_value_copy (&value, &value2);
  g_assert_true (gsk_value_get_render_node (&value2) == node);

  gsk_value_set_render_node (&value, NULL);
  gsk_value_take_render_node (&value2, NULL);

  g_value_unset (&value);
  g_value_unset (&value2);
}

static void
test_collect_varargs (GskRenderNode *node, ...)
{
  va_list ap;
  char *err = NULL;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, GSK_TYPE_RENDER_NODE);

  va_start (ap, node);
  G_VALUE_COLLECT (&value, ap, 0, &err);
  va_end (ap);

  g_assert_true (gsk_value_get_render_node (&value) == node);

  g_value_unset (&value);
}

static void
test_rendernode_varargs (void)
{
  GskRenderNode *node;

  node = gsk_color_node_new (&(GdkRGBA){0,1,1,1}, &GRAPHENE_RECT_INIT (0, 0, 50, 50));

  test_collect_varargs (node, node, NULL);

  gsk_render_node_unref (node);
}

static void
test_bordernode_uniform (void)
{
  GskRenderNode *node;
  GskRoundedRect rect;
  GdkRGBA colors[4] = {
    { 0, 0, 0, 1 },
    { 0, 0, 0, 1 },
    { 0, 0, 0, 1 },
    { 0, 0, 0, 1 },
  };

  gsk_rounded_rect_init (&rect,
                         &GRAPHENE_RECT_INIT (0, 0, 50, 50),
                         &GRAPHENE_SIZE_INIT (10, 10),
                         &GRAPHENE_SIZE_INIT (10, 10),
                         &GRAPHENE_SIZE_INIT (10, 10),
                         &GRAPHENE_SIZE_INIT (10, 10));

  node = gsk_border_node_new (&rect, (const float[]){ 1, 1, 1, 1}, colors);

  g_assert_true (gsk_border_node_get_uniform (node));
  g_assert_true (gsk_border_node_get_uniform_color (node));

  gsk_render_node_unref (node);

  node = gsk_border_node_new (&rect, (const float[]){ 1, 2, 3, 4}, colors);

  g_assert_false (gsk_border_node_get_uniform (node));
  g_assert_true (gsk_border_node_get_uniform_color (node));

  gsk_render_node_unref (node);
}

#define DEG_TO_RAD(x)          ((x) * (G_PI / 180.f))

static void
test_conic_gradient_angle (void)
{
  GskRenderNode *node;
  GskColorStop stops[] = {
    { 0.f, (GdkRGBA) { 0, 0, 0, 1} },
    { 1.f, (GdkRGBA) { 1, 0, 1, 1} },
  };

  node = gsk_conic_gradient_node_new (&GRAPHENE_RECT_INIT (0, 0, 50, 50),
                                      &GRAPHENE_POINT_INIT (10, 20),
                                      33.f,
                                      stops,
                                      G_N_ELEMENTS (stops));

  g_assert_cmpfloat_with_epsilon (gsk_conic_gradient_node_get_angle (node), DEG_TO_RAD (90.f - 33.f), 0.001);

  gsk_render_node_unref (node);
}

static void
test_container_disjoint (void)
{
  GskRenderNode *node, *nodes[2];

  nodes[0] = gsk_color_node_new (&(GdkRGBA){0,1,1,1}, &GRAPHENE_RECT_INIT (0, 0, 50, 50));
  nodes[1] = gsk_color_node_new (&(GdkRGBA){0,1,1,1}, &GRAPHENE_RECT_INIT (50, 0, 50, 50));
  node = gsk_container_node_new (nodes, 2);

  g_assert_true (gsk_container_node_is_disjoint (node));

  gsk_render_node_unref (node);
  gsk_render_node_unref (nodes[0]);
  gsk_render_node_unref (nodes[1]);

  nodes[0] = gsk_color_node_new (&(GdkRGBA){0,1,1,1}, &GRAPHENE_RECT_INIT (0, 0, 50, 50));
  nodes[1] = gsk_color_node_new (&(GdkRGBA){0,1,1,1}, &GRAPHENE_RECT_INIT (25, 0, 50, 50));
  node = gsk_container_node_new (nodes, 2);

  g_assert_false (gsk_container_node_is_disjoint (node));

  gsk_render_node_unref (node);
  gsk_render_node_unref (nodes[0]);
  gsk_render_node_unref (nodes[1]);
}

const char shader1[] =
"uniform float progress;\n"
"uniform sampler2D u_texture1;\n"
"uniform sampler2D u_texture2;\n"
""
"vec4 getFromColor (vec2 uv) {\n"
"  return GskTexture(u_texture1, uv);\n"
"}\n"
"\n"
"vec4 getToColor (vec2 uv) {\n"
"  return GskTexture(u_texture2, uv);\n"
"}\n"
"\n"
"// author: bobylito\n"
"// license: MIT\n"
"const float SQRT_2 = 1.414213562373;\n"
"uniform float dots;// = 20.0;\n"
"uniform vec2 center; //= vec2(0, 0);\n"
"\n"
"uniform int test1;\n"
"uniform bool test2;\n"
"uniform vec3 test3;\n"
"uniform vec4 test4;\n"
"\n"
"vec4 transition(vec2 uv) {\n"
"  bool nextImage = distance(fract(uv * dots), vec2(0.5, 0.5)) < ( progress / distance(uv, center));\n"
"  return nextImage ? getToColor(uv) : getFromColor(uv);\n"
"}\n"
"\n"
"void mainImage(out vec4 fragColor, in vec2 fragCoord, in vec2 resolution, in vec2 uv)\n"
"{\n"
"  fragColor = transition(uv);\n"
"}\n";

static void
test_renderer (GskRenderer *renderer)
{
  GdkDisplay *display;
  gboolean realized;
  GdkSurface *surface;
  gboolean res;
  GError *error = NULL;
  GskGLShader *shader;
  GBytes *bytes;

  g_assert (GSK_IS_RENDERER (renderer));

  display = gdk_display_open (NULL);

  g_object_get (renderer,
                "realized", &realized,
                "surface", &surface,
                NULL);

  g_assert_false (realized);
  g_assert_null (surface);

  g_assert_null (gsk_renderer_get_surface (renderer));

  surface = gdk_surface_new_toplevel (display);

  res = gsk_renderer_realize (renderer, surface, &error);

  g_assert_true (res);

  g_assert_true (gsk_renderer_is_realized (renderer));
  g_assert_true (gsk_renderer_get_surface (renderer) == surface);

  bytes = g_bytes_new_static (shader1, sizeof (shader1));
  shader = gsk_gl_shader_new_from_bytes (bytes);
  g_bytes_unref (bytes);
  res = gsk_gl_shader_compile (shader, renderer, &error);
  if (GSK_IS_GL_RENDERER (renderer))
    {
      g_assert_no_error (error);
      g_assert_true (res);
    }
  else
    {
      g_assert_false (res);
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
      g_clear_error (&error);
    }

  gsk_renderer_unrealize (renderer);

  g_assert_false (gsk_renderer_is_realized (renderer));
  g_assert_null (gsk_renderer_get_surface (renderer));

  gdk_surface_destroy (surface);

  gdk_display_close (display);
}

static void
test_cairo_renderer (void)
{
  GskRenderer *renderer;

  renderer = gsk_cairo_renderer_new ();
  test_renderer (renderer);
  g_clear_object (&renderer);
}

static void
test_gl_renderer (void)
{
#ifdef GDK_RENDERING_GL
  GskRenderer *renderer;

  renderer = gsk_gl_renderer_new ();
  test_renderer (renderer);
  g_clear_object (&renderer);
#else
  g_test_skip ("no GL support");
#endif
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  gtk_init ();

  g_test_add_func ("/rendernode/gvalue", test_rendernode_gvalue);
  g_test_add_func ("/rendernode/varargs", test_rendernode_varargs);
  g_test_add_func ("/rendernode/border/uniform", test_bordernode_uniform);
  g_test_add_func ("/rendernode/conic-gradient/angle", test_conic_gradient_angle);
  g_test_add_func ("/rendernode/container/disjoint", test_container_disjoint);
  g_test_add_func ("/renderer/cairo", test_cairo_renderer);
  g_test_add_func ("/renderer/gl", test_gl_renderer);

  return g_test_run ();
}
