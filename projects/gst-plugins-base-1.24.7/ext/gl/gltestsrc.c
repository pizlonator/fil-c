/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2016> Matthew Waters <matthew@centricular.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gl/gstglfuncs.h>

#include "gltestsrc.h"

#define MAX_ATTRIBUTES 4

struct vts_color_struct
{
  gfloat R, G, B;
};

struct XYZWRGB
{
  gfloat X, Y, Z, W, R, G, B;
};

enum
{
  COLOR_WHITE = 0,
  COLOR_YELLOW,
  COLOR_CYAN,
  COLOR_GREEN,
  COLOR_MAGENTA,
  COLOR_RED,
  COLOR_BLUE,
  COLOR_BLACK,
  COLOR_NEG_I,
  COLOR_POS_Q,
  COLOR_SUPER_BLACK,
  COLOR_DARK_GREY
};

static const struct vts_color_struct vts_colors[] = {
  /* 100% white */
  {1.0f, 1.0f, 1.0f},
  /* yellow */
  {1.0f, 1.0f, 0.0f},
  /* cyan */
  {0.0f, 1.0f, 1.0f},
  /* green */
  {0.0f, 1.0f, 0.0f},
  /* magenta */
  {1.0f, 0.0f, 1.0f},
  /* red */
  {1.0f, 0.0f, 0.0f},
  /* blue */
  {0.0f, 0.0f, 1.0f},
  /* black */
  {0.0f, 0.0f, 0.0f},
  /* -I */
  {0.0, 0.0f, 0.5f},
  /* +Q */
  {0.0f, 0.5, 1.0f},
  /* superblack */
  {0.0f, 0.0f, 0.0f},
  /* 7.421875% grey */
  {19. / 256.0f, 19. / 256.0f, 19. / 256.0},
};

/* *INDENT-OFF* */
static const GLfloat positions[] = {
     -1.0,  1.0,  0.0, 1.0,
      1.0,  1.0,  0.0, 1.0,
      1.0, -1.0,  0.0, 1.0,
     -1.0, -1.0,  0.0, 1.0,
};

static const GLushort indices_quad[] = { 0, 1, 2, 0, 2, 3 };
/* *INDENT-ON* */

struct attribute
{
  const gchar *name;
  gint location;
  guint n_elements;
  GLenum element_type;
  guint offset;                 /* in bytes */
  guint stride;                 /* in bytes */
};

struct SrcShader
{
  struct BaseSrcImpl base;

  GstGLShader *shader;

  guint vao;
  guint vbo;
  guint vbo_indices;

  guint n_attributes;
  struct attribute attributes[MAX_ATTRIBUTES];

  gconstpointer vertices;
  gsize vertices_size;
  const gushort *indices;
  guint index_offset;
  guint n_indices;
};

static void
_bind_buffer (struct SrcShader *src)
{
  GstGLContext *context = src->base.context;
  const GstGLFuncs *gl = context->gl_vtable;
  gint i;

  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, src->vbo_indices);
  gl->BindBuffer (GL_ARRAY_BUFFER, src->vbo);

  /* Load the vertex position */
  for (i = 0; i < src->n_attributes; i++) {
    struct attribute *attr = &src->attributes[i];

    if (attr->location == -1)
      attr->location =
          gst_gl_shader_get_attribute_location (src->shader, attr->name);

    gl->VertexAttribPointer (attr->location, attr->n_elements,
        attr->element_type, GL_FALSE, attr->stride,
        (void *) (gintptr) attr->offset);

    gl->EnableVertexAttribArray (attr->location);
  }
}

static void
_unbind_buffer (struct SrcShader *src)
{
  GstGLContext *context = src->base.context;
  const GstGLFuncs *gl = context->gl_vtable;
  gint i;

  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
  gl->BindBuffer (GL_ARRAY_BUFFER, 0);

  for (i = 0; i < src->n_attributes; i++) {
    struct attribute *attr = &src->attributes[i];

    gl->DisableVertexAttribArray (attr->location);
  }
}

static gboolean
_src_shader_init (gpointer impl, GstGLContext * context,
    const GstVideoInfo * v_info)
{
  struct SrcShader *src = impl;
  const GstGLFuncs *gl = context->gl_vtable;

  src->base.context = context;

  if (!src->vbo) {
    if (gl->GenVertexArrays) {
      gl->GenVertexArrays (1, &src->vao);
      gl->BindVertexArray (src->vao);
    }

    gl->GenBuffers (1, &src->vbo);
    gl->BindBuffer (GL_ARRAY_BUFFER, src->vbo);
    gl->BufferData (GL_ARRAY_BUFFER, src->vertices_size,
        src->vertices, GL_STATIC_DRAW);

    gl->GenBuffers (1, &src->vbo_indices);
    gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, src->vbo_indices);
    gl->BufferData (GL_ELEMENT_ARRAY_BUFFER, src->n_indices * sizeof (gushort),
        src->indices, GL_STATIC_DRAW);

    if (gl->GenVertexArrays) {
      _bind_buffer (src);
      gl->BindVertexArray (0);
    }

    gl->BindBuffer (GL_ARRAY_BUFFER, 0);
    gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
  }

  return TRUE;
}

static gboolean
_src_shader_fill_bound_fbo (gpointer impl)
{
  struct SrcShader *src = impl;
  const GstGLFuncs *gl;

  g_return_val_if_fail (src->base.context, FALSE);
  g_return_val_if_fail (src->shader, FALSE);
  gl = src->base.context->gl_vtable;

  gst_gl_shader_use (src->shader);

  if (gl->GenVertexArrays)
    gl->BindVertexArray (src->vao);
  _bind_buffer (src);

  gl->DrawElements (GL_TRIANGLES, src->n_indices, GL_UNSIGNED_SHORT,
      (gpointer) (gintptr) src->index_offset);

  if (gl->GenVertexArrays)
    gl->BindVertexArray (0);
  else
    _unbind_buffer (src);

  gst_gl_context_clear_shader (src->base.context);

  return TRUE;
}

static void
_src_shader_deinit (gpointer impl)
{
  struct SrcShader *src = impl;
  const GstGLFuncs *gl = src->base.context->gl_vtable;

  if (src->shader)
    gst_object_unref (src->shader);
  src->shader = NULL;

  if (src->vao)
    gl->DeleteVertexArrays (1, &src->vao);
  src->vao = 0;

  if (src->vbo)
    gl->DeleteBuffers (1, &src->vbo);
  src->vbo = 0;

  if (src->vbo_indices)
    gl->DeleteBuffers (1, &src->vbo_indices);
  src->vbo_indices = 0;
}

/* *INDENT-OFF* */
static const gchar *smpte_vertex_src =
    "attribute vec4 position;\n"
    "attribute vec4 a_color;\n"
    "varying vec4 color;\n"
    "void main()\n"
    "{\n"
    "  gl_Position = position;\n"
    "  color = a_color;\n"
    "}";

static const gchar *smpte_fragment_src =
    "varying vec4 color;\n"
    "void main()\n"
    "{\n"
    "  gl_FragColor = color;\n"
    "}";

static const gchar *snow_vertex_src =
    "attribute vec4 position;\n"
    "varying vec2 out_uv;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = position;\n"
    "   out_uv = position.xy;\n"
    "}";

static const gchar *snow_fragment_src =
    "uniform float time;\n"
    "varying vec2 out_uv;\n"
    "\n"
    "float rand(vec2 co){\n"
    "    return fract(sin(dot(co.xy, vec2(12.9898,78.233))) * 43758.5453);\n"
    "}\n"
    "void main()\n"
    "{\n"
    "  gl_FragColor = vec4(rand(time * out_uv));\n"
    "}";
/* *INDENT-ON* */

#define N_QUADS 21
struct SrcSMPTE
{
  struct SrcShader base;

  GstGLShader *snow_shader;
  GstGLShader *color_shader;
  gint attr_snow_position;
};

static gpointer
_src_smpte_new (GstGLTestSrc * test)
{
  struct SrcSMPTE *src = g_new0 (struct SrcSMPTE, 1);

  src->base.base.src = test;

  return src;
}

static gboolean
_src_smpte_init (gpointer impl, GstGLContext * context,
    const GstVideoInfo * v_info)
{
  struct SrcSMPTE *src = impl;
  struct XYZWRGB *coord;
  gushort *plane_indices;
  GError *error = NULL;
  int color_idx = 0;
  const gchar *frags[2];
  int i;

  src->base.base.context = context;

  coord = g_new0 (struct XYZWRGB, N_QUADS * 4);
  plane_indices = g_new0 (gushort, N_QUADS * 6);

  /* top row */
  for (i = 0; i < 7; i++) {
    coord[color_idx * 4 + 0].X = -1.0f + i * (2.0f / 7.0f);
    coord[color_idx * 4 + 0].Y = 1.0f / 3.0f;
    coord[color_idx * 4 + 1].X = -1.0f + (i + 1) * (2.0f / 7.0f);
    coord[color_idx * 4 + 1].Y = 1.0f / 3.0f;
    coord[color_idx * 4 + 2].X = -1.0f + (i + 1) * (2.0f / 7.0f);
    coord[color_idx * 4 + 2].Y = -1.0f;
    coord[color_idx * 4 + 3].X = -1.0f + i * (2.0f / 7.0f);
    coord[color_idx * 4 + 3].Y = -1.0f;
    color_idx++;
  }

  /* middle row */
  for (i = 0; i < 7; i++) {
    coord[color_idx * 4 + 0].X = -1.0f + i * (2.0f / 7.0f);
    coord[color_idx * 4 + 0].Y = 0.5f;
    coord[color_idx * 4 + 1].X = -1.0f + (i + 1) * (2.0f / 7.0f);
    coord[color_idx * 4 + 1].Y = 0.5f;
    coord[color_idx * 4 + 2].X = -1.0f + (i + 1) * (2.0f / 7.0f);
    coord[color_idx * 4 + 2].Y = 1.0f / 3.0f;
    coord[color_idx * 4 + 3].X = -1.0f + i * (2.0f / 7.0f);
    coord[color_idx * 4 + 3].Y = 1.0f / 3.0f;
    color_idx++;
  }

  /* bottom row, left three */
  for (i = 0; i < 3; i++) {
    coord[color_idx * 4 + 0].X = -1.0f + i / 3.0f;
    coord[color_idx * 4 + 0].Y = 1.0f;
    coord[color_idx * 4 + 1].X = -1.0f + (i + 1) / 3.0f;
    coord[color_idx * 4 + 1].Y = 1.0f;
    coord[color_idx * 4 + 2].X = -1.0f + (i + 1) / 3.0f;
    coord[color_idx * 4 + 2].Y = 0.5f;
    coord[color_idx * 4 + 3].X = -1.0f + i / 3.0f;
    coord[color_idx * 4 + 3].Y = 0.5f;
    color_idx++;
  }

  /* bottom row, middle three (the blacks) */
  for (i = 0; i < 3; i++) {
    coord[color_idx * 4 + 0].X = i / 6.0f;
    coord[color_idx * 4 + 0].Y = 1.0f;
    coord[color_idx * 4 + 1].X = (i + 1) / 6.0f;
    coord[color_idx * 4 + 1].Y = 1.0f;
    coord[color_idx * 4 + 2].X = (i + 1) / 6.0f;
    coord[color_idx * 4 + 2].Y = 0.5f;
    coord[color_idx * 4 + 3].X = i / 6.0f;
    coord[color_idx * 4 + 3].Y = 0.5f;
    color_idx++;
  }

  g_assert (color_idx < N_QUADS);

  for (i = 0; i < N_QUADS - 1; i++) {
    int j, k;
    if (i < 7) {
      k = i;
    } else if ((i - 7) & 1) {
      k = COLOR_BLACK;
    } else {
      k = 13 - i;
    }

    if (i == 14) {
      k = COLOR_NEG_I;
    } else if (i == 15) {
      k = COLOR_WHITE;
    } else if (i == 16) {
      k = COLOR_POS_Q;
    } else if (i == 17) {
      k = COLOR_SUPER_BLACK;
    } else if (i == 18) {
      k = COLOR_BLACK;
    } else if (i == 19) {
      k = COLOR_DARK_GREY;
    }

    for (j = 0; j < 4; j++) {
      coord[i * 4 + j].Z = 0.0f;
      coord[i * 4 + j].W = 1.0f;
      coord[i * 4 + j].R = vts_colors[k].R;
      coord[i * 4 + j].G = vts_colors[k].G;
      coord[i * 4 + j].B = vts_colors[k].B;
    }

    for (j = 0; j < 6; j++)
      plane_indices[i * 6 + j] = i * 4 + indices_quad[j];
  }

  /* snow */
  coord[color_idx * 4 + 0].X = 0.5f;
  coord[color_idx * 4 + 0].Y = 1.0f;
  coord[color_idx * 4 + 0].Z = 0.0f;
  coord[color_idx * 4 + 0].W = 1.0f;
  coord[color_idx * 4 + 1].X = 1.0f;
  coord[color_idx * 4 + 1].Y = 1.0f;
  coord[color_idx * 4 + 1].Z = 0.0f;
  coord[color_idx * 4 + 1].W = 1.0f;
  coord[color_idx * 4 + 2].X = 1.0f;
  coord[color_idx * 4 + 2].Y = 0.5f;
  coord[color_idx * 4 + 2].Z = 0.0f;
  coord[color_idx * 4 + 2].W = 1.0f;
  coord[color_idx * 4 + 3].X = 0.5f;
  coord[color_idx * 4 + 3].Y = 0.5f;
  coord[color_idx * 4 + 3].Z = 0.0f;
  coord[color_idx * 4 + 3].W = 1.0f;
  for (i = 0; i < 6; i++)
    plane_indices[color_idx * 6 + i] = color_idx * 4 + indices_quad[i];
  color_idx++;

  if (src->color_shader)
    gst_object_unref (src->color_shader);

  frags[0] =
      gst_gl_shader_string_get_highest_precision (context,
      GST_GLSL_VERSION_NONE,
      GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY);
  frags[1] = smpte_fragment_src;

  src->color_shader = gst_gl_shader_new_link_with_stages (context, &error,
      gst_glsl_stage_new_with_string (context, GL_VERTEX_SHADER,
          GST_GLSL_VERSION_NONE,
          GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY,
          smpte_vertex_src),
      gst_glsl_stage_new_with_strings (context, GL_FRAGMENT_SHADER,
          GST_GLSL_VERSION_NONE,
          GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY, 2,
          frags), NULL);
  if (!src->color_shader) {
    GST_ERROR_OBJECT (src->base.base.src, "%s", error->message);
    return FALSE;
  }

  frags[1] = snow_fragment_src;

  if (src->snow_shader)
    gst_object_unref (src->snow_shader);
  src->snow_shader = gst_gl_shader_new_link_with_stages (context, &error,
      gst_glsl_stage_new_with_string (context, GL_VERTEX_SHADER,
          GST_GLSL_VERSION_NONE,
          GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY,
          snow_vertex_src),
      gst_glsl_stage_new_with_strings (context, GL_FRAGMENT_SHADER,
          GST_GLSL_VERSION_NONE,
          GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY, 2,
          frags), NULL);
  if (!src->snow_shader) {
    GST_ERROR_OBJECT (src->base.base.src, "%s", error->message);
    return FALSE;
  }

  src->attr_snow_position = -1;

  src->base.n_attributes = 2;

  src->base.attributes[0].name = "position";
  src->base.attributes[0].location = -1;
  src->base.attributes[0].n_elements = 4;
  src->base.attributes[0].element_type = GL_FLOAT;
  src->base.attributes[0].offset = 0;
  src->base.attributes[0].stride = sizeof (struct XYZWRGB);

  src->base.attributes[1].name = "a_color";
  src->base.attributes[1].location = -1;
  src->base.attributes[1].n_elements = 3;
  src->base.attributes[1].element_type = GL_FLOAT;
  src->base.attributes[1].offset = 4 * sizeof (gfloat);
  src->base.attributes[1].stride = sizeof (struct XYZWRGB);

  if (src->base.shader)
    gst_object_unref (src->base.shader);
  src->base.shader = gst_object_ref (src->color_shader);
  src->base.vertices = (gfloat *) coord;
  src->base.vertices_size = sizeof (struct XYZWRGB) * N_QUADS * 4;
  src->base.indices = plane_indices;
  src->base.n_indices = N_QUADS * 6;

  return _src_shader_init (impl, context, v_info);
}

static gboolean
_src_smpte_fill_bound_fbo (gpointer impl)
{
  struct SrcSMPTE *src = impl;
  gint attr_color_position = -1;
  GstGLBaseSrc *bsrc = GST_GL_BASE_SRC (src->base.base.src);

  src->base.n_attributes = 2;
  if (src->base.shader)
    gst_object_unref (src->base.shader);
  src->base.shader = gst_object_ref (src->color_shader);
  src->base.n_indices = (N_QUADS - 1) * 6;
  src->base.index_offset = 0;
  if (!_src_shader_fill_bound_fbo (impl))
    return FALSE;
  attr_color_position = src->base.attributes[0].location;

  src->base.attributes[0].location = src->attr_snow_position;
  src->base.n_attributes = 1;
  if (src->base.shader)
    gst_object_unref (src->base.shader);
  src->base.shader = gst_object_ref (src->snow_shader);
  src->base.n_indices = 6;
  src->base.index_offset = (N_QUADS - 1) * 6 * sizeof (gushort);
  gst_gl_shader_use (src->snow_shader);
  gst_gl_shader_set_uniform_1f (src->snow_shader, "time",
      (gfloat) bsrc->running_time / GST_SECOND);
  if (!_src_shader_fill_bound_fbo (impl))
    return FALSE;
  src->attr_snow_position = src->base.attributes[0].location;
  src->base.attributes[0].location = attr_color_position;

  return TRUE;
}

static void
_src_smpte_free (gpointer impl)
{
  struct SrcSMPTE *src = impl;

  if (!impl)
    return;

  _src_shader_deinit (impl);

  g_free ((gpointer) src->base.vertices);
  g_free ((gpointer) src->base.indices);

  if (src->snow_shader)
    gst_object_unref (src->snow_shader);
  if (src->color_shader)
    gst_object_unref (src->color_shader);

  g_free (impl);
}

static const struct SrcFuncs src_smpte = {
  GST_GL_TEST_SRC_SMPTE,
  _src_smpte_new,
  _src_smpte_init,
  _src_smpte_fill_bound_fbo,
  _src_smpte_free,
};

#undef N_QUADS

struct SrcUniColor
{
  struct BaseSrcImpl base;

  struct vts_color_struct color;
};

static gpointer
_src_uni_color_new (GstGLTestSrc * test)
{
  struct SrcUniColor *src = g_new0 (struct SrcUniColor, 1);

  src->base.src = test;

  return src;
}

static gboolean
_src_uni_color_init (gpointer impl, GstGLContext * context,
    const GstVideoInfo * v_info)
{
  struct SrcUniColor *src = impl;

  src->base.context = context;
  src->base.v_info = *v_info;

  return TRUE;
}

static gboolean
_src_uni_color_fill_bound_fbo (gpointer impl)
{
  struct SrcUniColor *src = impl;
  const GstGLFuncs *gl = src->base.context->gl_vtable;

  gl->ClearColor (src->color.R, src->color.G, src->color.B, 1.0f);
  gl->Clear (GL_COLOR_BUFFER_BIT);

  return TRUE;
}

static void
_src_uni_color_free (gpointer impl)
{
  g_free (impl);
}

#define SRC_UNICOLOR(name, cap_name) \
static gpointer \
G_PASTE(G_PASTE(_src_unicolor_,name),_new) (GstGLTestSrc * test) \
{ \
  struct SrcUniColor *src = _src_uni_color_new (test); \
  src->color = vts_colors[G_PASTE(COLOR_,cap_name)]; \
  return src; \
} \
static const struct SrcFuncs G_PASTE (src_,name) = { \
  G_PASTE(GST_GL_TEST_SRC_,cap_name), \
  G_PASTE(G_PASTE(_src_unicolor_,name),_new), \
  _src_uni_color_init, \
  _src_uni_color_fill_bound_fbo, \
  _src_uni_color_free, \
}

SRC_UNICOLOR (white, WHITE);
SRC_UNICOLOR (black, BLACK);
SRC_UNICOLOR (red, RED);
SRC_UNICOLOR (green, GREEN);
SRC_UNICOLOR (blue, BLUE);

static gpointer
_src_blink_new (GstGLTestSrc * test)
{
  struct SrcUniColor *src = _src_uni_color_new (test);

  src->color = vts_colors[COLOR_WHITE];

  return src;
}

static gboolean
_src_blink_fill_bound_fbo (gpointer impl)
{
  struct SrcUniColor *src = impl;

  if (src->color.R > 0.5) {
    src->color = vts_colors[COLOR_BLACK];
  } else {
    src->color = vts_colors[COLOR_WHITE];
  }

  return _src_uni_color_fill_bound_fbo (impl);
}

static const struct SrcFuncs src_blink = {
  GST_GL_TEST_SRC_BLINK,
  _src_blink_new,
  _src_uni_color_init,
  _src_blink_fill_bound_fbo,
  _src_uni_color_free,
};

/* *INDENT-OFF* */
static const gchar *checkers_vertex_src = "attribute vec4 position;\n"
    "varying vec2 uv;\n"
    "void main()\n"
    "{\n"
    "  gl_Position = position;\n"
    /* RPi gives incorrect results for positive uv (plus it makes us start on
     * the right pixel color i.e. red) */
    "  uv = position.xy - 1.0;\n"
    "}";

static const gchar *checkers_fragment_src =
    "uniform float checker_width;\n"
    "uniform float width;\n"
    "uniform float height;\n"
    "varying vec2 uv;\n"
    "void main()\n"
    "{\n"
    "  vec2 xy_mod = floor (0.5 * uv * vec2(width, height) / (checker_width));\n"
    "  float result = mod (xy_mod.x + xy_mod.y, 2.0);\n"
    "  gl_FragColor.r = step (result, 0.5);\n"
    "  gl_FragColor.g = 1.0 - gl_FragColor.r;\n"
    "  gl_FragColor.ba = vec2(0.0, 1.0);\n"
    "}";
/* *INDENT-ON* */

struct SrcCheckers
{
  struct SrcShader base;

  guint checker_width;
};

static gboolean
_src_checkers_init (gpointer impl, GstGLContext * context,
    const GstVideoInfo * v_info)
{
  struct SrcCheckers *src = impl;
  GError *error = NULL;
  const gchar *frags[2];

  src->base.base.context = context;

  frags[0] =
      gst_gl_shader_string_get_highest_precision (context,
      GST_GLSL_VERSION_NONE,
      GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY);
  frags[1] = checkers_fragment_src;

  if (src->base.shader)
    gst_object_unref (src->base.shader);
  src->base.shader = gst_gl_shader_new_link_with_stages (context, &error,
      gst_glsl_stage_new_with_string (context, GL_VERTEX_SHADER,
          GST_GLSL_VERSION_NONE,
          GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY,
          checkers_vertex_src),
      gst_glsl_stage_new_with_strings (context, GL_FRAGMENT_SHADER,
          GST_GLSL_VERSION_NONE,
          GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY, 2,
          frags), NULL);
  if (!src->base.shader) {
    GST_ERROR_OBJECT (src->base.base.src, "%s", error->message);
    return FALSE;
  }

  src->base.n_attributes = 1;

  src->base.attributes[0].name = "position";
  src->base.attributes[0].location = -1;
  src->base.attributes[0].n_elements = 4;
  src->base.attributes[0].element_type = GL_FLOAT;
  src->base.attributes[0].offset = 0;
  src->base.attributes[0].stride = 4 * sizeof (gfloat);

  src->base.vertices = positions;
  src->base.vertices_size = sizeof (positions);
  src->base.indices = indices_quad;
  src->base.n_indices = 6;

  gst_gl_shader_use (src->base.shader);
  gst_gl_shader_set_uniform_1f (src->base.shader, "checker_width",
      src->checker_width);
  gst_gl_shader_set_uniform_1f (src->base.shader, "width",
      (gfloat) GST_VIDEO_INFO_WIDTH (v_info));
  gst_gl_shader_set_uniform_1f (src->base.shader, "height",
      (gfloat) GST_VIDEO_INFO_HEIGHT (v_info));
  gst_gl_context_clear_shader (src->base.base.context);

  return _src_shader_init (impl, context, v_info);
}

static void
_src_checkers_free (gpointer impl)
{
  struct SrcCheckers *src = impl;

  if (!src)
    return;

  _src_shader_deinit (impl);

  g_free (impl);
}

static gpointer
_src_checkers_new (GstGLTestSrc * test)
{
  struct SrcCheckers *src = g_new0 (struct SrcCheckers, 1);

  src->base.base.src = test;

  return src;
}

#define SRC_CHECKERS(spacing) \
static gpointer \
G_PASTE(G_PASTE(_src_checkers,spacing),_new) (GstGLTestSrc * test) \
{ \
  struct SrcCheckers *src = _src_checkers_new (test); \
  src->checker_width = spacing; \
  return src; \
} \
static const struct SrcFuncs G_PASTE(src_checkers,spacing) = { \
  G_PASTE(GST_GL_TEST_SRC_CHECKERS,spacing), \
  G_PASTE(G_PASTE(_src_checkers,spacing),_new), \
  _src_checkers_init, \
  _src_shader_fill_bound_fbo, \
  _src_checkers_free, \
}

SRC_CHECKERS (1);
SRC_CHECKERS (2);
SRC_CHECKERS (4);
SRC_CHECKERS (8);

static gboolean
_src_snow_init (gpointer impl, GstGLContext * context,
    const GstVideoInfo * v_info)
{
  struct SrcShader *src = impl;
  GError *error = NULL;
  const gchar *frags[2];

  src->base.context = context;

  frags[0] =
      gst_gl_shader_string_get_highest_precision (context,
      GST_GLSL_VERSION_NONE,
      GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY);
  frags[1] = snow_fragment_src;

  if (src->shader)
    gst_object_unref (src->shader);
  src->shader = gst_gl_shader_new_link_with_stages (context, &error,
      gst_glsl_stage_new_with_string (context, GL_VERTEX_SHADER,
          GST_GLSL_VERSION_NONE,
          GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY,
          snow_vertex_src),
      gst_glsl_stage_new_with_strings (context, GL_FRAGMENT_SHADER,
          GST_GLSL_VERSION_NONE,
          GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY, 2,
          frags), NULL);
  if (!src->shader) {
    GST_ERROR_OBJECT (src->base.src, "%s", error->message);
    return FALSE;
  }

  src->n_attributes = 1;

  src->attributes[0].name = "position";
  src->attributes[0].location = -1;
  src->attributes[0].n_elements = 4;
  src->attributes[0].element_type = GL_FLOAT;
  src->attributes[0].offset = 0;
  src->attributes[0].stride = 4 * sizeof (gfloat);

  src->vertices = positions;
  src->vertices_size = sizeof (positions);
  src->indices = indices_quad;
  src->n_indices = 6;

  return _src_shader_init (impl, context, v_info);
}

static gboolean
_src_snow_fill_bound_fbo (gpointer impl)
{
  struct SrcShader *src = impl;
  GstGLBaseSrc *bsrc = GST_GL_BASE_SRC (src->base.src);

  g_return_val_if_fail (src->base.context, FALSE);
  g_return_val_if_fail (src->shader, FALSE);

  gst_gl_shader_use (src->shader);
  gst_gl_shader_set_uniform_1f (src->shader, "time",
      (gfloat) bsrc->running_time / GST_SECOND);

  return _src_shader_fill_bound_fbo (impl);
}

static void
_src_snow_free (gpointer impl)
{
  struct SrcShader *src = impl;

  if (!src)
    return;

  _src_shader_deinit (impl);

  g_free (impl);
}

static gpointer
_src_snow_new (GstGLTestSrc * test)
{
  struct SrcShader *src = g_new0 (struct SrcShader, 1);

  src->base.src = test;

  return src;
}

static const struct SrcFuncs src_snow = {
  GST_GL_TEST_SRC_SNOW,
  _src_snow_new,
  _src_snow_init,
  _src_snow_fill_bound_fbo,
  _src_snow_free,
};

/* *INDENT-OFF* */
static const gchar *mandelbrot_vertex_src = "attribute vec4 position;\n"
    "uniform float aspect_ratio;\n"
    "varying vec2 fractal_position;\n"
    "void main()\n"
    "{\n"
    "  gl_Position = position;\n"
    "  fractal_position = vec2(position.y * 0.5 - 0.3, aspect_ratio * position.x * 0.5);\n"
    "  fractal_position *= 2.5;\n"
    "}";

static const gchar *mandelbrot_fragment_src =
    "uniform float time;\n"
    "varying vec2 fractal_position;\n"
    "const vec4 K = vec4(1.0, 0.66, 0.33, 3.0);\n"
    "vec4 hsv_to_rgb(float hue, float saturation, float value) {\n"
    "  vec4 p = abs(fract(vec4(hue) + K) * 6.0 - K.wwww);\n"
    "  return value * mix(K.xxxx, clamp(p - K.xxxx, 0.0, 1.0), saturation);\n"
    "}\n"
    "vec4 i_to_rgb(int i) {\n"
    "  float hue = float(i) / 100.0 + sin(time);\n"
    "  return hsv_to_rgb(hue, 0.5, 0.8);\n"
    "}\n"
    "vec2 pow_2_complex(vec2 c) {\n"
    "  return vec2(c.x*c.x - c.y*c.y, 2.0 * c.x * c.y);\n"
    "}\n"
    "vec2 mandelbrot(vec2 c, vec2 c0) {\n"
    "  return pow_2_complex(c) + c0;\n"
    "}\n"
    "vec4 iterate_pixel(vec2 position) {\n"
    "  vec2 c = vec2(0);\n"
    "  for (int i=0; i < 20; i++) {\n"
    "    if (c.x*c.x + c.y*c.y > 2.0*2.0)\n"
    "      return i_to_rgb(i);\n"
    "    c = mandelbrot(c, position);\n"
    "  }\n"
    "  return vec4(0, 0, 0, 1);\n"
    "}\n"
    "void main() {\n"
    "  gl_FragColor = iterate_pixel(fractal_position);\n"
    "}";
/* *INDENT-ON* */

static gboolean
_src_mandelbrot_init (gpointer impl, GstGLContext * context,
    const GstVideoInfo * v_info)
{
  struct SrcShader *src = impl;
  GError *error = NULL;
  const gchar *frags[2];

  src->base.context = context;

  frags[0] =
      gst_gl_shader_string_get_highest_precision (context,
      GST_GLSL_VERSION_NONE,
      GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY);
  frags[1] = mandelbrot_fragment_src;

  if (src->shader)
    gst_object_unref (src->shader);
  src->shader = gst_gl_shader_new_link_with_stages (context, &error,
      gst_glsl_stage_new_with_string (context, GL_VERTEX_SHADER,
          GST_GLSL_VERSION_NONE,
          GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY,
          mandelbrot_vertex_src),
      gst_glsl_stage_new_with_strings (context, GL_FRAGMENT_SHADER,
          GST_GLSL_VERSION_NONE,
          GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY, 2,
          frags), NULL);
  if (!src->shader) {
    GST_ERROR_OBJECT (src->base.src, "%s", error->message);
    return FALSE;
  }

  src->n_attributes = 1;

  src->attributes[0].name = "position";
  src->attributes[0].location = -1;
  src->attributes[0].n_elements = 4;
  src->attributes[0].element_type = GL_FLOAT;
  src->attributes[0].offset = 0;
  src->attributes[0].stride = 4 * sizeof (gfloat);

  src->vertices = positions;
  src->vertices_size = sizeof (positions);
  src->indices = indices_quad;
  src->n_indices = 6;

  gst_gl_shader_use (src->shader);
  gst_gl_shader_set_uniform_1f (src->shader, "aspect_ratio",
      (gfloat) GST_VIDEO_INFO_WIDTH (v_info) /
      (gfloat) GST_VIDEO_INFO_HEIGHT (v_info));
  gst_gl_context_clear_shader (src->base.context);

  return _src_shader_init (impl, context, v_info);
}

static gboolean
_src_mandelbrot_fill_bound_fbo (gpointer impl)
{
  struct SrcShader *src = impl;
  GstGLBaseSrc *bsrc = GST_GL_BASE_SRC (src->base.src);

  g_return_val_if_fail (src->base.context, FALSE);
  g_return_val_if_fail (src->shader, FALSE);

  gst_gl_shader_use (src->shader);
  gst_gl_shader_set_uniform_1f (src->shader, "time",
      (gfloat) bsrc->running_time / GST_SECOND);

  return _src_shader_fill_bound_fbo (impl);
}

static void
_src_mandelbrot_free (gpointer impl)
{
  struct SrcShader *src = impl;

  if (!src)
    return;

  _src_shader_deinit (impl);

  g_free (impl);
}

static gpointer
_src_mandelbrot_new (GstGLTestSrc * test)
{
  struct SrcShader *src = g_new0 (struct SrcShader, 1);

  src->base.src = test;

  return src;
}

static const struct SrcFuncs src_mandelbrot = {
  GST_GL_TEST_SRC_MANDELBROT,
  _src_mandelbrot_new,
  _src_mandelbrot_init,
  _src_mandelbrot_fill_bound_fbo,
  _src_mandelbrot_free,
};

/* *INDENT-OFF* */
static const gchar *circular_vertex_src =
    "attribute vec4 position;\n"
    "varying vec2 uv;\n"
    "void main()\n"
    "{\n"
    "  gl_Position = position;\n"
    "  uv = position.xy;\n"
    "}";

static const gchar *circular_fragment_src =
    "uniform float aspect_ratio;\n"
    "varying vec2 uv;\n"
    "#define PI 3.14159265\n"
    "void main() {\n"
    "  float dist = 0.5 * sqrt(uv.x * uv.x + uv.y / aspect_ratio * uv.y / aspect_ratio);\n"
    "  float seg = floor(dist * 16.0);\n"
    "  if (seg <= 0.0 || seg >= 8.0) {\n"
    "    gl_FragColor = vec4(vec3(0.0), 1.0);\n"
    "  } else {\n"
    "    float d = floor (256.0 * dist * 200.0 * pow (2.0, - (seg - 1.0) / 4.0) + 0.5) / 128.0;\n"
    "    gl_FragColor = vec4 (vec3(sin (d * PI) * 0.5 + 0.5), 1.0);\n"
    "  }\n"
    "}";
/* *INDENT-ON* */

static gboolean
_src_circular_init (gpointer impl, GstGLContext * context,
    const GstVideoInfo * v_info)
{
  struct SrcShader *src = impl;
  GError *error = NULL;
  const gchar *frags[2];

  src->base.context = context;

  frags[0] =
      gst_gl_shader_string_get_highest_precision (context,
      GST_GLSL_VERSION_NONE,
      GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY);
  frags[1] = circular_fragment_src;

  if (src->shader)
    gst_object_unref (src->shader);
  src->shader = gst_gl_shader_new_link_with_stages (context, &error,
      gst_glsl_stage_new_with_string (context, GL_VERTEX_SHADER,
          GST_GLSL_VERSION_NONE,
          GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY,
          circular_vertex_src),
      gst_glsl_stage_new_with_strings (context, GL_FRAGMENT_SHADER,
          GST_GLSL_VERSION_NONE,
          GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY, 2,
          frags), NULL);
  if (!src->shader) {
    GST_ERROR_OBJECT (src->base.src, "%s", error->message);
    return FALSE;
  }

  src->n_attributes = 1;

  src->attributes[0].name = "position";
  src->attributes[0].location = -1;
  src->attributes[0].n_elements = 4;
  src->attributes[0].element_type = GL_FLOAT;
  src->attributes[0].offset = 0;
  src->attributes[0].stride = 4 * sizeof (gfloat);

  src->vertices = positions;
  src->vertices_size = sizeof (positions);
  src->indices = indices_quad;
  src->n_indices = 6;

  gst_gl_shader_use (src->shader);
  gst_gl_shader_set_uniform_1f (src->shader, "aspect_ratio",
      (gfloat) GST_VIDEO_INFO_WIDTH (v_info) /
      (gfloat) GST_VIDEO_INFO_HEIGHT (v_info));
  gst_gl_context_clear_shader (src->base.context);

  return _src_shader_init (impl, context, v_info);
}

static void
_src_circular_free (gpointer impl)
{
  struct SrcShader *src = impl;

  if (!src)
    return;

  _src_shader_deinit (impl);

  g_free (impl);
}

static gpointer
_src_circular_new (GstGLTestSrc * test)
{
  struct SrcShader *src = g_new0 (struct SrcShader, 1);

  src->base.src = test;

  return src;
}

static const struct SrcFuncs src_circular = {
  GST_GL_TEST_SRC_CIRCULAR,
  _src_circular_new,
  _src_circular_init,
  _src_mandelbrot_fill_bound_fbo,
  _src_circular_free,
};

static const struct SrcFuncs *src_impls[] = {
  &src_smpte,
  &src_snow,
  &src_black,
  &src_white,
  &src_red,
  &src_green,
  &src_blue,
  &src_checkers1,
  &src_checkers2,
  &src_checkers4,
  &src_checkers8,
  &src_circular,
  &src_blink,
  &src_mandelbrot,
};

const struct SrcFuncs *
gst_gl_test_src_get_src_funcs_for_pattern (GstGLTestSrcPattern pattern)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (src_impls); i++) {
    if (src_impls[i]->pattern == pattern)
      return src_impls[i];
  }

  return NULL;
}
