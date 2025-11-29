/*
 * Copyright © 2020 Red Hat, Inc.
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

/* shader fragment as found in nature */
const char shader0[] =
"// author: bobylito\n"
"// license: MIT\n"
"const float SQRT_2 = 1.414213562373;"
"uniform float dots;// = 20.0;"
"uniform vec2 center; //= vec2(0, 0);"
""
"vec4 transition(vec2 uv) {"
"  bool nextImage = distance(fract(uv * dots), vec2(0.5, 0.5)) < ( progress / distance(uv, center));"
"  return nextImage ? getToColor(uv) : getFromColor(uv);"
"}";

/* Same shader, with our preamble added, and with newlines
 * to make the regex happy. Added a variety of uniforms to
 * exercise the parser.
 */
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
"uniform int test1 = -2;\n"
"uniform uint test2 = 2;   \n"
"uniform bool test3;\n"
"uniform vec3 test4;\n"
"uniform vec4 test5;\n"
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
test_create_simple (void)
{
  GBytes *bytes;
  GskGLShader *shader;
  GBytes *source;

  bytes = g_bytes_new_static (shader1, sizeof (shader1));
  shader = gsk_gl_shader_new_from_bytes (bytes);
  g_assert_nonnull (shader);

  g_assert_cmpint (gsk_gl_shader_get_n_textures (shader), ==, 2);
  g_assert_cmpint (gsk_gl_shader_get_n_uniforms (shader), ==, 8);
  g_assert_cmpstr (gsk_gl_shader_get_uniform_name (shader, 0), ==, "progress");
  g_assert_cmpint (gsk_gl_shader_get_uniform_type (shader, 0), ==, GSK_GL_UNIFORM_TYPE_FLOAT);
  g_assert_cmpstr (gsk_gl_shader_get_uniform_name (shader, 1), ==, "dots");
  g_assert_cmpint (gsk_gl_shader_get_uniform_type (shader, 1), ==, GSK_GL_UNIFORM_TYPE_FLOAT);
  g_assert_cmpstr (gsk_gl_shader_get_uniform_name (shader, 2), ==, "center");
  g_assert_cmpint (gsk_gl_shader_get_uniform_type (shader, 2), ==, GSK_GL_UNIFORM_TYPE_VEC2);
  g_assert_cmpstr (gsk_gl_shader_get_uniform_name (shader, 3), ==, "test1");
  g_assert_cmpint (gsk_gl_shader_get_uniform_type (shader, 3), ==, GSK_GL_UNIFORM_TYPE_INT);
  g_assert_cmpstr (gsk_gl_shader_get_uniform_name (shader, 4), ==, "test2");
  g_assert_cmpint (gsk_gl_shader_get_uniform_type (shader, 4), ==, GSK_GL_UNIFORM_TYPE_UINT);
  g_assert_cmpstr (gsk_gl_shader_get_uniform_name (shader, 5), ==, "test3");
  g_assert_cmpint (gsk_gl_shader_get_uniform_type (shader, 5), ==, GSK_GL_UNIFORM_TYPE_BOOL);
  g_assert_cmpstr (gsk_gl_shader_get_uniform_name (shader, 6), ==, "test4");
  g_assert_cmpint (gsk_gl_shader_get_uniform_type (shader, 6), ==, GSK_GL_UNIFORM_TYPE_VEC3);
  g_assert_cmpstr (gsk_gl_shader_get_uniform_name (shader, 7), ==, "test5");
  g_assert_cmpint (gsk_gl_shader_get_uniform_type (shader, 7), ==, GSK_GL_UNIFORM_TYPE_VEC4);

  g_assert_cmpint (gsk_gl_shader_find_uniform_by_name (shader, "progress"), ==, 0);
  g_assert_cmpint (gsk_gl_shader_find_uniform_by_name (shader, "dots"), ==, 1);
  g_assert_cmpint (gsk_gl_shader_find_uniform_by_name (shader, "center"), ==, 2);
  g_assert_cmpint (gsk_gl_shader_find_uniform_by_name (shader, "test1"), ==, 3);
  g_assert_cmpint (gsk_gl_shader_find_uniform_by_name (shader, "test2"), ==, 4);
  g_assert_cmpint (gsk_gl_shader_find_uniform_by_name (shader, "test3"), ==, 5);
  g_assert_cmpint (gsk_gl_shader_find_uniform_by_name (shader, "test4"), ==, 6);
  g_assert_cmpint (gsk_gl_shader_find_uniform_by_name (shader, "test5"), ==, 7);
  g_assert_cmpint (gsk_gl_shader_find_uniform_by_name (shader, "nosucharg"), ==, -1);

  g_assert_cmpint (gsk_gl_shader_get_uniform_offset (shader, 0), ==, 0);
  g_assert_cmpint (gsk_gl_shader_get_uniform_offset (shader, 1), >, 0);
  g_assert_cmpint (gsk_gl_shader_get_uniform_offset (shader, 2), >, 0);
  g_assert_cmpint (gsk_gl_shader_get_uniform_offset (shader, 3), >, 0);
  g_assert_cmpint (gsk_gl_shader_get_uniform_offset (shader, 4), >, 0);
  g_assert_cmpint (gsk_gl_shader_get_uniform_offset (shader, 5), >, 0);
  g_assert_cmpint (gsk_gl_shader_get_uniform_offset (shader, 6), >, 0);
  g_assert_cmpint (gsk_gl_shader_get_uniform_offset (shader, 7), >, 0);

  g_assert_null (gsk_gl_shader_get_resource (shader));

  g_object_get (shader, "source", &source, NULL);
  g_assert_true (g_bytes_equal (source, bytes));

  g_object_unref (shader);
  g_bytes_unref (bytes);
  g_bytes_unref (source);
}

static void
test_create_data (void)
{
  GBytes *bytes;
  GBytes *bytes2;
  GskGLShader *shader;
  GskShaderArgsBuilder *builder;
  GskShaderArgsBuilder *builder2;
  graphene_vec2_t v2, vv2;
  graphene_vec3_t v3, vv3;
  graphene_vec4_t v4, vv4;
  GskRenderNode *node;
  GskRenderNode *children[2];

  bytes = g_bytes_new_static (shader1, sizeof (shader1));
  shader = gsk_gl_shader_new_from_bytes (bytes);
  g_assert_nonnull (shader);
  g_clear_pointer (&bytes, g_bytes_unref);

  builder = gsk_shader_args_builder_new (shader, NULL);
  g_assert_nonnull (builder);

  graphene_vec2_init (&v2, 20, 30);
  graphene_vec3_init (&v3, -1, -2, -3);
  graphene_vec4_init (&v4, 100, 0, -100, 10);
  gsk_shader_args_builder_set_float (builder, 0, 0.5);
  gsk_shader_args_builder_set_float (builder, 1, 20.0);
  gsk_shader_args_builder_set_vec2 (builder, 2, &v2);
  gsk_shader_args_builder_set_int (builder, 3, -99);
  gsk_shader_args_builder_set_uint (builder, 4, 99);
  gsk_shader_args_builder_set_bool (builder, 5, 1);
  gsk_shader_args_builder_set_vec3 (builder, 6, &v3);
  gsk_shader_args_builder_set_vec4 (builder, 7, &v4);

  bytes = gsk_shader_args_builder_to_args (builder);

  g_assert_cmpfloat (gsk_gl_shader_get_arg_float (shader, bytes, 0), ==, 0.5);
  g_assert_cmpfloat (gsk_gl_shader_get_arg_float (shader, bytes, 1), ==, 20.0);
  gsk_gl_shader_get_arg_vec2 (shader, bytes, 2, &vv2);
  g_assert_true (graphene_vec2_equal (&v2, &vv2));

  g_assert_cmpint (gsk_gl_shader_get_arg_int (shader, bytes, 3), ==, -99);
  g_assert_cmpuint (gsk_gl_shader_get_arg_uint (shader, bytes, 4), ==, 99);
  g_assert_cmpint (gsk_gl_shader_get_arg_bool (shader, bytes, 5), ==, 1);

  gsk_gl_shader_get_arg_vec3 (shader, bytes, 6, &vv3);
  g_assert_true (graphene_vec3_equal (&v3, &vv3));
  gsk_gl_shader_get_arg_vec4 (shader, bytes, 7, &vv4);
  g_assert_true (graphene_vec4_equal (&v4, &vv4));

  children[0] = gsk_color_node_new (&(GdkRGBA){0,0,0,1}, &GRAPHENE_RECT_INIT (0, 0, 50, 50));
  children[1] = gsk_color_node_new (&(GdkRGBA){1,0,0,1}, &GRAPHENE_RECT_INIT (0, 0, 50, 50));
  node = gsk_gl_shader_node_new (shader,
                                 &GRAPHENE_RECT_INIT (0, 0, 50, 50),
                                 bytes,
                                 children,
                                 2);

  g_assert_true (gsk_gl_shader_node_get_shader (node) == shader);
  g_assert_cmpuint (gsk_gl_shader_node_get_n_children (node), ==, 2);
  g_assert_true (gsk_gl_shader_node_get_child (node, 0) == children[0]);
  g_assert_true (gsk_gl_shader_node_get_child (node, 1) == children[1]);

  gsk_render_node_unref (children[0]);
  gsk_render_node_unref (children[1]);
  gsk_render_node_unref (node);

  builder2 = gsk_shader_args_builder_new (shader, bytes);
  gsk_shader_args_builder_ref (builder2);
  bytes2 = gsk_shader_args_builder_free_to_args (builder2);
  gsk_shader_args_builder_unref (builder2);

  g_assert_true (g_bytes_equal (bytes, bytes2));

  g_bytes_unref (bytes2);
  g_bytes_unref (bytes);

  gsk_shader_args_builder_unref (builder);

  g_object_unref (shader);
}

static void
test_format_args (void)
{
  GBytes *bytes;
  GskGLShader *shader;
  graphene_vec2_t v2, vv2;
  graphene_vec3_t v3, vv3;
  graphene_vec4_t v4, vv4;
  float f1, f2;
  GBytes *args;

  bytes = g_bytes_new_static (shader1, sizeof (shader1));
  shader = gsk_gl_shader_new_from_bytes (bytes);
  g_assert_nonnull (shader);
  g_clear_pointer (&bytes, g_bytes_unref);

  graphene_vec2_init (&v2, 20, 30);
  graphene_vec3_init (&v3, -1, -2, -3);
  graphene_vec4_init (&v4, 100, 0, -100, 10);

  f1 = 0.5;
  f2 = 20.0;
  args = gsk_gl_shader_format_args (shader,
                                    "progress", f1,
                                    "dots", f2,
                                    "center", &v2,
                                    "test4", &v3,
                                    "test5", &v4,
                                    NULL);

  g_assert_cmpfloat (gsk_gl_shader_get_arg_float (shader, args, 0), ==, 0.5);
  g_assert_cmpfloat (gsk_gl_shader_get_arg_float (shader, args, 1), ==, 20.0);
  gsk_gl_shader_get_arg_vec2 (shader, args, 2, &vv2);
  g_assert_true (graphene_vec2_equal (&v2, &vv2));
  gsk_gl_shader_get_arg_vec3 (shader, args, 6, &vv3);
  g_assert_true (graphene_vec3_equal (&v3, &vv3));
  gsk_gl_shader_get_arg_vec4 (shader, args, 7, &vv4);
  g_assert_true (graphene_vec4_equal (&v4, &vv4));

  g_bytes_unref (args);

  g_object_unref (shader);
}

static void
test_compile (void)
{
  GBytes *bytes;
  GskGLShader *shader;
  GdkSurface *surface;
  GskRenderer *renderer;
  GError *error = NULL;
  gboolean ret;

  bytes = g_bytes_new_static ("blaat", 6);
  shader = gsk_gl_shader_new_from_bytes (bytes);
  g_assert_nonnull (shader);

  surface = gdk_surface_new_toplevel (gdk_display_get_default ());
  renderer = gsk_renderer_new_for_surface (surface);
  g_assert_nonnull (renderer);

  ret = gsk_gl_shader_compile (shader, renderer, &error);

  g_assert_false (ret);
  g_assert_nonnull (error);
  if (g_str_equal (G_OBJECT_TYPE_NAME (renderer), "GskGLRenderer"))
    g_assert_nonnull (strstr (error->message, "blaat"));
  else
    g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  g_clear_error (&error);

  gsk_renderer_unrealize (renderer);
  g_object_unref (renderer);
  gdk_surface_destroy (surface);
  g_bytes_unref (bytes);
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/shader/create/simple", test_create_simple);
  g_test_add_func ("/shader/create/data", test_create_data);
  g_test_add_func ("/shader/format-args", test_format_args);
  g_test_add_func ("/shader/compile", test_compile);

  return g_test_run ();
}
