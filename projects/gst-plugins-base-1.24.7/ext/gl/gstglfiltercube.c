/*
 * GStreamer
 * Copyright (C) 2008 Julien Isorce <julien.isorce@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:element-glfiltercube
 * @title: glfiltercube
 *
 * The resize and redraw callbacks can be set from a client code.
 *
 * ## Examples
 * |[
 * gst-launch-1.0 -v videotestsrc ! glfiltercube ! glimagesink
 * ]| A pipeline to mpa textures on the 6 cube faces..
 * FBO is required.
 * |[
 * gst-launch-1.0 -v videotestsrc ! glfiltercube ! video/x-raw, width=640, height=480 ! glimagesink
 * ]| Resize scene after drawing the cube.
 * The scene size is greater than the input video size.
  |[
 * gst-launch-1.0 -v videotestsrc ! video/x-raw, width=640, height=480  ! glfiltercube ! glimagesink
 * ]| Resize scene before drawing the cube.
 * The scene size is greater than the input video size.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gl/gstglapi.h>

#include "gstglelements.h"
#include "gstglfiltercube.h"
#include "gstglutils.h"

#define GST_CAT_DEFAULT gst_gl_filter_cube_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  PROP_0,
  PROP_RED,
  PROP_GREEN,
  PROP_BLUE,
  PROP_FOVY,
  PROP_ASPECT,
  PROP_ZNEAR,
  PROP_ZFAR
};

#define DEBUG_INIT \
    GST_DEBUG_CATEGORY_INIT (gst_gl_filter_cube_debug, "glfiltercube", 0, "glfiltercube element");
#define gst_gl_filter_cube_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGLFilterCube, gst_gl_filter_cube,
    GST_TYPE_GL_FILTER, DEBUG_INIT);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (glfiltercube, "glfiltercube",
    GST_RANK_NONE, GST_TYPE_GL_FILTER_CUBE, gl_element_init (plugin));

static void gst_gl_filter_cube_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gl_filter_cube_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_gl_filter_cube_set_caps (GstGLFilter * filter,
    GstCaps * incaps, GstCaps * outcaps);
static gboolean gst_gl_filter_cube_gl_start (GstGLBaseFilter * filter);
static void gst_gl_filter_cube_gl_stop (GstGLBaseFilter * filter);
static gboolean _callback (gpointer stuff);
static gboolean gst_gl_filter_cube_filter_texture (GstGLFilter * filter,
    GstGLMemory * in_tex, GstGLMemory * out_tex);

/* vertex source */
static const gchar *cube_v_src =
    "attribute vec4 a_position;                                   \n"
    "attribute vec2 a_texcoord;                                   \n"
    "uniform mat4 u_matrix;                                       \n"
    "uniform float xrot_degree, yrot_degree, zrot_degree;         \n"
    "varying vec2 v_texcoord;                                     \n"
    "void main()                                                  \n"
    "{                                                            \n"
    "   float PI = 3.14159265;                                    \n"
    "   float xrot = xrot_degree*2.0*PI/360.0;                    \n"
    "   float yrot = yrot_degree*2.0*PI/360.0;                    \n"
    "   float zrot = zrot_degree*2.0*PI/360.0;                    \n"
    "   mat4 matX = mat4 (                                        \n"
    "            1.0,        0.0,        0.0, 0.0,                \n"
    "            0.0,  cos(xrot),  sin(xrot), 0.0,                \n"
    "            0.0, -sin(xrot),  cos(xrot), 0.0,                \n"
    "            0.0,        0.0,        0.0, 1.0 );              \n"
    "   mat4 matY = mat4 (                                        \n"
    "      cos(yrot),        0.0, -sin(yrot), 0.0,                \n"
    "            0.0,        1.0,        0.0, 0.0,                \n"
    "      sin(yrot),        0.0,  cos(yrot), 0.0,                \n"
    "            0.0,        0.0,       0.0,  1.0 );              \n"
    "   mat4 matZ = mat4 (                                        \n"
    "      cos(zrot),  sin(zrot),        0.0, 0.0,                \n"
    "     -sin(zrot),  cos(zrot),        0.0, 0.0,                \n"
    "            0.0,        0.0,        1.0, 0.0,                \n"
    "            0.0,        0.0,        0.0, 1.0 );              \n"
    "   gl_Position = u_matrix * matZ * matY * matX * a_position; \n"
    "   v_texcoord = a_texcoord;                                  \n"
    "}                                                            \n";

/* fragment source */
static const gchar *cube_f_src =
    "varying vec2 v_texcoord;                            \n"
    "uniform sampler2D s_texture;                        \n"
    "void main()                                         \n"
    "{                                                   \n"
    "  gl_FragColor = texture2D( s_texture, v_texcoord );\n"
    "}                                                   \n";

static void
gst_gl_filter_cube_class_init (GstGLFilterCubeClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;

  gobject_class = (GObjectClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);

  gst_gl_filter_add_rgba_pad_templates (GST_GL_FILTER_CLASS (klass));

  gobject_class->set_property = gst_gl_filter_cube_set_property;
  gobject_class->get_property = gst_gl_filter_cube_get_property;

  GST_GL_BASE_FILTER_CLASS (klass)->gl_start = gst_gl_filter_cube_gl_start;
  GST_GL_BASE_FILTER_CLASS (klass)->gl_stop = gst_gl_filter_cube_gl_stop;

  GST_GL_FILTER_CLASS (klass)->set_caps = gst_gl_filter_cube_set_caps;
  GST_GL_FILTER_CLASS (klass)->filter_texture =
      gst_gl_filter_cube_filter_texture;

  g_object_class_install_property (gobject_class, PROP_RED,
      g_param_spec_float ("red", "Red", "Background red color",
          0.0f, 1.0f, 0.0f, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_GREEN,
      g_param_spec_float ("green", "Green", "Background green color",
          0.0f, 1.0f, 0.0f, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BLUE,
      g_param_spec_float ("blue", "Blue", "Background blue color",
          0.0f, 1.0f, 0.0f, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_FOVY,
      g_param_spec_double ("fovy", "Fovy", "Field of view angle in degrees",
          0.0, 180.0, 45.0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ASPECT,
      g_param_spec_double ("aspect", "Aspect",
          "Field of view in the x direction", 0.0, 100, 0.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ZNEAR,
      g_param_spec_double ("znear", "Znear",
          "Specifies the distance from the viewer to the near clipping plane",
          0.0, 100.0, 0.1, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ZFAR,
      g_param_spec_double ("zfar", "Zfar",
          "Specifies the distance from the viewer to the far clipping plane",
          0.0, 1000.0, 100.0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_metadata (element_class, "OpenGL cube filter",
      "Filter/Effect/Video", "Map input texture on the 6 cube faces",
      "Julien Isorce <julien.isorce@gmail.com>");

  GST_GL_BASE_FILTER_CLASS (klass)->supported_gl_api =
      GST_GL_API_OPENGL | GST_GL_API_GLES2 | GST_GL_API_OPENGL3;
}

static void
gst_gl_filter_cube_init (GstGLFilterCube * filter)
{
  filter->shader = NULL;
  filter->fovy = 45;
  filter->aspect = 0;
  filter->znear = 0.1;
  filter->zfar = 100;
}

static void
gst_gl_filter_cube_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGLFilterCube *filter = GST_GL_FILTER_CUBE (object);

  switch (prop_id) {
    case PROP_RED:
      filter->red = g_value_get_float (value);
      break;
    case PROP_GREEN:
      filter->green = g_value_get_float (value);
      break;
    case PROP_BLUE:
      filter->blue = g_value_get_float (value);
      break;
    case PROP_FOVY:
      filter->fovy = g_value_get_double (value);
      break;
    case PROP_ASPECT:
      filter->aspect = g_value_get_double (value);
      break;
    case PROP_ZNEAR:
      filter->znear = g_value_get_double (value);
      break;
    case PROP_ZFAR:
      filter->zfar = g_value_get_double (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_filter_cube_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGLFilterCube *filter = GST_GL_FILTER_CUBE (object);

  switch (prop_id) {
    case PROP_RED:
      g_value_set_float (value, filter->red);
      break;
    case PROP_GREEN:
      g_value_set_float (value, filter->green);
      break;
    case PROP_BLUE:
      g_value_set_float (value, filter->blue);
      break;
    case PROP_FOVY:
      g_value_set_double (value, filter->fovy);
      break;
    case PROP_ASPECT:
      g_value_set_double (value, filter->aspect);
      break;
    case PROP_ZNEAR:
      g_value_set_double (value, filter->znear);
      break;
    case PROP_ZFAR:
      g_value_set_double (value, filter->zfar);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_gl_filter_cube_set_caps (GstGLFilter * filter, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstGLFilterCube *cube_filter = GST_GL_FILTER_CUBE (filter);

  if (cube_filter->aspect == 0)
    cube_filter->aspect = (gdouble) GST_VIDEO_INFO_WIDTH (&filter->out_info) /
        (gdouble) GST_VIDEO_INFO_HEIGHT (&filter->out_info);

  return TRUE;
}

static void
gst_gl_filter_cube_gl_stop (GstGLBaseFilter * base_filter)
{
  GstGLFilterCube *cube_filter = GST_GL_FILTER_CUBE (base_filter);
  const GstGLFuncs *gl = base_filter->context->gl_vtable;

  if (cube_filter->vao) {
    gl->DeleteVertexArrays (1, &cube_filter->vao);
    cube_filter->vao = 0;
  }

  if (cube_filter->vertex_buffer) {
    gl->DeleteBuffers (1, &cube_filter->vertex_buffer);
    cube_filter->vertex_buffer = 0;
  }

  if (cube_filter->vbo_indices) {
    gl->DeleteBuffers (1, &cube_filter->vbo_indices);
    cube_filter->vbo_indices = 0;
  }

  if (cube_filter->shader) {
    gst_object_unref (cube_filter->shader);
    cube_filter->shader = NULL;
  }

  GST_GL_BASE_FILTER_CLASS (parent_class)->gl_stop (base_filter);
}

static gboolean
gst_gl_filter_cube_gl_start (GstGLBaseFilter * filter)
{
  GstGLFilterCube *cube_filter = GST_GL_FILTER_CUBE (filter);
  GstGLContext *context = GST_GL_BASE_FILTER (filter)->context;
  gchar *frag_str;
  gboolean ret;

  cube_filter->xrot = 0.0;
  cube_filter->yrot = 0.0;
  cube_filter->zrot = 0.0;

  frag_str =
      g_strdup_printf ("%s%s",
      gst_gl_shader_string_get_highest_precision (context,
          GST_GLSL_VERSION_NONE,
          GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY), cube_f_src);

  /* blocking call, wait the opengl thread has compiled the shader */
  ret = gst_gl_context_gen_shader (context, cube_v_src, frag_str,
      &cube_filter->shader);
  g_free (frag_str);

  return ret;
}

static gboolean
gst_gl_filter_cube_filter_texture (GstGLFilter * filter, GstGLMemory * in_tex,
    GstGLMemory * out_tex)
{
  GstGLFilterCube *cube_filter = GST_GL_FILTER_CUBE (filter);

  cube_filter->in_tex = in_tex;

  return gst_gl_framebuffer_draw_to_texture (filter->fbo, out_tex, _callback,
      cube_filter);
}

/* *INDENT-OFF* */
static const GLfloat vertices[] = {
 /*|     Vertex     | TexCoord |*/ 
    /* front face */
     1.0,  1.0, -1.0, 1.0, 0.0,
     1.0, -1.0, -1.0, 1.0, 1.0,
    -1.0, -1.0, -1.0, 0.0, 1.0,
    -1.0,  1.0, -1.0, 0.0, 0.0,
    /* back face */
     1.0,  1.0,  1.0, 1.0, 0.0,
    -1.0,  1.0,  1.0, 0.0, 0.0,
    -1.0, -1.0,  1.0, 0.0, 1.0,
     1.0, -1.0,  1.0, 1.0, 1.0,
    /* right face */
     1.0,  1.0,  1.0, 1.0, 0.0,
     1.0, -1.0,  1.0, 0.0, 0.0,
     1.0, -1.0, -1.0, 0.0, 1.0,
     1.0,  1.0, -1.0, 1.0, 1.0,
    /* left face */
    -1.0,  1.0,  1.0, 1.0, 0.0,
    -1.0,  1.0, -1.0, 1.0, 1.0,
    -1.0, -1.0, -1.0, 0.0, 1.0,
    -1.0, -1.0,  1.0, 0.0, 0.0,
    /* top face */
     1.0, -1.0,  1.0, 1.0, 0.0,
    -1.0, -1.0,  1.0, 0.0, 0.0,
    -1.0, -1.0, -1.0, 0.0, 1.0,
     1.0, -1.0, -1.0, 1.0, 1.0,
    /* bottom face */
     1.0,  1.0,  1.0, 1.0, 0.0,
     1.0,  1.0, -1.0, 1.0, 1.0,
    -1.0,  1.0, -1.0, 0.0, 1.0,
    -1.0,  1.0,  1.0, 0.0, 0.0
};

static const GLushort indices[] = {
    0, 1, 2,
    0, 2, 3,
    4, 5, 6,
    4, 6, 7,
    8, 9, 10,
    8, 10, 11,
    12, 13, 14,
    12, 14, 15,
    16, 17, 18,
    16, 18, 19,
    20, 21, 22,
    20, 22, 23
};
/* *INDENT-ON* */

static void
_bind_buffer (GstGLFilterCube * cube_filter)
{
  const GstGLFuncs *gl = GST_GL_BASE_FILTER (cube_filter)->context->gl_vtable;

  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, cube_filter->vbo_indices);
  gl->BindBuffer (GL_ARRAY_BUFFER, cube_filter->vertex_buffer);

  cube_filter->attr_position =
      gst_gl_shader_get_attribute_location (cube_filter->shader, "a_position");

  cube_filter->attr_texture =
      gst_gl_shader_get_attribute_location (cube_filter->shader, "a_texcoord");

  /* Load the vertex position */
  gl->VertexAttribPointer (cube_filter->attr_position, 3, GL_FLOAT, GL_FALSE,
      5 * sizeof (GLfloat), (void *) 0);

  /* Load the texture coordinate */
  gl->VertexAttribPointer (cube_filter->attr_texture, 2, GL_FLOAT, GL_FALSE,
      5 * sizeof (GLfloat), (void *) (3 * sizeof (GLfloat)));

  gl->EnableVertexAttribArray (cube_filter->attr_position);
  gl->EnableVertexAttribArray (cube_filter->attr_texture);
}

static void
_unbind_buffer (GstGLFilterCube * cube_filter)
{
  const GstGLFuncs *gl = GST_GL_BASE_FILTER (cube_filter)->context->gl_vtable;

  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
  gl->BindBuffer (GL_ARRAY_BUFFER, 0);

  gl->DisableVertexAttribArray (cube_filter->attr_position);
  gl->DisableVertexAttribArray (cube_filter->attr_texture);
}

static gboolean
_callback (gpointer stuff)
{
  GstGLFilter *filter = GST_GL_FILTER (stuff);
  GstGLFilterCube *cube_filter = GST_GL_FILTER_CUBE (filter);
  GstGLFuncs *gl = GST_GL_BASE_FILTER (filter)->context->gl_vtable;

  const GLfloat matrix[] = {
    0.5f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.5f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.5f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
  };

  gl->Enable (GL_DEPTH_TEST);

  gl->ClearColor (cube_filter->red, cube_filter->green, cube_filter->blue, 0.0);
  gl->Clear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  gst_gl_shader_use (cube_filter->shader);

  gl->ActiveTexture (GL_TEXTURE0);
  gl->BindTexture (GL_TEXTURE_2D, cube_filter->in_tex->tex_id);
  gst_gl_shader_set_uniform_1i (cube_filter->shader, "s_texture", 0);
  gst_gl_shader_set_uniform_1f (cube_filter->shader, "xrot_degree",
      cube_filter->xrot);
  gst_gl_shader_set_uniform_1f (cube_filter->shader, "yrot_degree",
      cube_filter->yrot);
  gst_gl_shader_set_uniform_1f (cube_filter->shader, "zrot_degree",
      cube_filter->zrot);
  gst_gl_shader_set_uniform_matrix_4fv (cube_filter->shader, "u_matrix", 1,
      GL_FALSE, matrix);

  if (!cube_filter->vertex_buffer) {
    if (gl->GenVertexArrays) {
      gl->GenVertexArrays (1, &cube_filter->vao);
      gl->BindVertexArray (cube_filter->vao);
    }

    gl->GenBuffers (1, &cube_filter->vertex_buffer);
    gl->BindBuffer (GL_ARRAY_BUFFER, cube_filter->vertex_buffer);
    gl->BufferData (GL_ARRAY_BUFFER, 6 * 4 * 5 * sizeof (GLfloat), vertices,
        GL_STATIC_DRAW);

    gl->GenBuffers (1, &cube_filter->vbo_indices);
    gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, cube_filter->vbo_indices);
    gl->BufferData (GL_ELEMENT_ARRAY_BUFFER, sizeof (indices), indices,
        GL_STATIC_DRAW);

    if (gl->GenVertexArrays) {
      _bind_buffer (cube_filter);
      gl->BindVertexArray (0);
    }

    gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
    gl->BindBuffer (GL_ARRAY_BUFFER, 0);
  }

  if (gl->GenVertexArrays)
    gl->BindVertexArray (cube_filter->vao);
  _bind_buffer (cube_filter);

  gl->DrawElements (GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 0);

  if (gl->GenVertexArrays)
    gl->BindVertexArray (0);
  else
    _unbind_buffer (cube_filter);

  gl->Disable (GL_DEPTH_TEST);

  cube_filter->xrot += 0.3f;
  cube_filter->yrot += 0.2f;
  cube_filter->zrot += 0.4f;

  return TRUE;
}
