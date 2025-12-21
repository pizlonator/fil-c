/*
 * GStreamer
 * Copyright (C) 2009 Julien Isorce <julien.isorce@mail.com>
 * Copyright (C) 2014 Jan Schmidt <jan@centricular.com>
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
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
 * SECTION:gstglviewconvert
 * @title: GstGLViewConvert
 * @short_description: convert between steroscopic/multiview video formats
 * @see_also: #GstGLColorConvert, #GstGLContext
 *
 * Convert stereoscopic/multiview video using fragment shaders.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstglviewconvert.h"

#include <gst/video/gstvideoaffinetransformationmeta.h>

#include "gl.h"
#include "gstglsl_private.h"
#include "gstglutils_private.h"

#define USING_OPENGL(context) (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL, 1, 0))
#define USING_OPENGL3(context) (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL3, 3, 1))
#define USING_GLES(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES, 1, 0))
#define USING_GLES2(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 2, 0))
#define USING_GLES3(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 3, 0))

static GstStaticCaps caps_template =
GST_STATIC_CAPS ("video/x-raw(" GST_CAPS_FEATURE_MEMORY_GL_MEMORY "), "
    "format = (string) RGBA, "
    "width = " GST_VIDEO_SIZE_RANGE ", "
    "height = " GST_VIDEO_SIZE_RANGE ", "
    "framerate = " GST_VIDEO_FPS_RANGE ", "
    "texture-target = (string) { 2D, rectangle, external-oes } ");

#define GST_CAT_DEFAULT gst_gl_view_convert_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  PROP_0,
  PROP_INPUT_LAYOUT,
  PROP_INPUT_FLAGS,
  PROP_OUTPUT_LAYOUT,
  PROP_OUTPUT_FLAGS,
  PROP_OUTPUT_DOWNMIX_MODE
};

#define DEFAULT_DOWNMIX GST_GL_STEREO_DOWNMIX_ANAGLYPH_GREEN_MAGENTA_DUBOIS

struct _GstGLViewConvertPrivate
{
  gboolean result;

  GstVideoMultiviewMode input_mode;
  GstVideoMultiviewFlags input_flags;
  GstVideoMultiviewMode output_mode;
  GstVideoMultiviewFlags output_flags;

  GstBuffer *primary_in;
  GstBuffer *auxilliary_in;

  GstBuffer *primary_out;
  GstBuffer *auxilliary_out;

  GstGLMemory *in_tex[GST_VIDEO_MAX_PLANES];
  GstGLMemory *out_tex[GST_VIDEO_MAX_PLANES];
  guint n_out_tex;

  GLuint vao;
  GLuint vertex_buffer;
  GLuint vbo_indices;
  GLuint attr_position;
  GLuint attr_texture;
};

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_gl_view_convert_debug, "glviewconvert", 0, "glviewconvert object");

G_DEFINE_TYPE_WITH_CODE (GstGLViewConvert, gst_gl_view_convert,
    GST_TYPE_OBJECT, G_ADD_PRIVATE (GstGLViewConvert) DEBUG_INIT);

static void gst_gl_view_convert_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_gl_view_convert_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_gl_view_convert_finalize (GObject * object);

static void _do_view_convert (GstGLContext * context,
    GstGLViewConvert * viewconvert);

/* *INDENT-OFF* */
/* These match the order and number of DOWNMIX_ANAGLYPH_* modes */
static GLfloat downmix_matrices[][2][9] = {
  {                             /* Green-Magenta Dubois */
        {-0.062f, 0.284f, -0.015f, -0.158f, 0.668f, -0.027f, -0.039f, 0.143f, 0.021f},
        {0.529f, -0.016f, 0.009f, 0.705f, -0.015f, 0.075f, 0.024f, -0.065f, 0.937f}
      },
  {                             /* Red-Cyan Dubois */
        /* Source of this matrix: http://www.site.uottawa.ca/~edubois/anaglyph/LeastSquaresHowToPhotoshop.pdf */
        {0.437f, -0.062f, -0.048f, 0.449f, -0.062f, -0.050f, 0.164f, -0.024f, -0.017f},
        {-0.011f, 0.377f, -0.026f, -0.032f, 0.761f, -0.093f, -0.007f, 0.009f, 1.234f}
      },
  {                             /* Amber-blue Dubois */
        {1.062f, -0.026f, -0.038f, -0.205f, 0.908f, -0.173f, 0.299f, 0.068f, 0.022f},
        {-0.016f, 0.006f, 0.094f, -0.123f, 0.062f, 0.185f, -0.017f, -0.017f, 0.911f}
      }
};

static gfloat identity_matrix[] = {
  1.0f, 0.0f, 0.0f, 0.0f,
  0.0f, 1.0f, 0.0f, 0.0f,
  0.0f, 0.0f, 1.0f, 0.0f,
  0.0f, 0.0f, 0.0f, 1.0f,
};
/* *INDENT-ON* */

#define glsl_OES_extension_string "#extension GL_OES_EGL_image_external : require \n"

/* *INDENT-OFF* */
static const gchar *fragment_header =
  "uniform sampler2D tex_l;\n"
  "uniform sampler2D tex_r;\n"
  "uniform float width;\n"
  "uniform float height;\n"
  "uniform mat3 downmix[2];\n"
  "uniform vec2 tex_scale[2];\n"
  "uniform vec2 offsets[2];\n";

static const gchar *frag_input =
  "  vec2 l_tex = v_texcoord * tex_scale[0] + offsets[0];\n"
  "  vec2 r_tex = v_texcoord * tex_scale[1] + offsets[1];\n"
  "  l = texture2D(tex_l, l_tex).rgba;\n"
  "  r = texture2D(tex_r, r_tex).rgba;\n";

static const gchar *frag_output_downmix =
  "  vec3 lcol = l.rgb * l.a + vec3(1.0-l.a);\n"
  "  vec3 rcol = r.rgb * r.a + vec3(1.0-r.a);\n"
  "  if (l.a + r.a > 0.0) {\n"
  "    lcol = clamp (downmix[0] * lcol, 0.0, 1.0);\n"
  "    rcol = clamp (downmix[1] * rcol, 0.0, 1.0);\n"
  "    gl_FragColor = vec4 (lcol + rcol, 1.0);\n"
  "  } else {\n"
  "    gl_FragColor = vec4 (0.0);\n"
  "  }\n";

static const gchar *frag_output_left =
  "  gl_FragColor = l;\n";

static const gchar *frag_output_right =
  "  gl_FragColor = r;\n";

static const gchar *frag_output_side_by_side =
  "  if (v_texcoord.x < 0.5) {\n"
  "    gl_FragColor = l;\n"
  "  } else {\n"
  "    gl_FragColor = r;\n"
  "  };\n";

static const gchar *frag_output_top_bottom =
  "if (v_texcoord.y < 0.5) {\n"
  "  gl_FragColor = l;\n"
  "} else {\n"
  "  gl_FragColor = r;\n"
  "};\n";

static const gchar *frag_output_column_interleaved =
  "if (int(mod(l_tex.x * width, 2.0)) == 0) {\n"
  "  gl_FragColor = l;\n"
  "} else {\n"
  "  gl_FragColor = r;\n"
  "};\n";

static const gchar *frag_output_row_interleaved =
  "if (int(mod(l_tex.y * height, 2.0)) == 0) {\n"
  "  gl_FragColor = l;\n"
  "} else {\n"
  "  gl_FragColor = r;\n"
  "};\n";

static const gchar *frag_output_checkerboard =
  "if (int(mod(l_tex.x * width, 2.0)) == \n"
  "    int(mod(l_tex.y * height, 2.0))) {\n"
  "  gl_FragColor = l;\n"
  "} else {\n"
  "  gl_FragColor = r;\n"
  "};\n";

static const gchar *frag_output_separated =
  "gl_FragData[0] = l;\n"
  "gl_FragData[1] = r;\n";
/* *INDENT-ON* */

static const GLfloat vertices[] = {
  1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
  -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
  -1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
  1.0f, 1.0f, 0.0f, 1.0f, 1.0f
};

static const GLushort indices[] = { 0, 1, 2, 0, 2, 3 };

static void
gst_gl_view_convert_class_init (GstGLViewConvertClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->set_property = gst_gl_view_convert_set_property;
  gobject_class->get_property = gst_gl_view_convert_get_property;
  gobject_class->finalize = gst_gl_view_convert_finalize;

  g_object_class_install_property (gobject_class, PROP_INPUT_LAYOUT,
      g_param_spec_enum ("input-mode-override",
          "Input Multiview Mode Override",
          "Override any input information about multiview layout",
          GST_TYPE_VIDEO_MULTIVIEW_MODE,
          GST_VIDEO_MULTIVIEW_MODE_NONE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_INPUT_FLAGS,
      g_param_spec_flags ("input-flags-override",
          "Input Multiview Flags Override",
          "Override any input information about multiview layout flags",
          GST_TYPE_VIDEO_MULTIVIEW_FLAGS, GST_VIDEO_MULTIVIEW_FLAGS_NONE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_OUTPUT_LAYOUT,
      g_param_spec_enum ("output-mode-override",
          "Output Multiview Mode Override",
          "Override automatic output mode selection for multiview layout",
          GST_TYPE_VIDEO_MULTIVIEW_MODE, GST_VIDEO_MULTIVIEW_MODE_NONE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_OUTPUT_FLAGS,
      g_param_spec_flags ("output-flags-override",
          "Output Multiview Flags Override",
          "Override automatic negotiation for output multiview layout flags",
          GST_TYPE_VIDEO_MULTIVIEW_FLAGS, GST_VIDEO_MULTIVIEW_FLAGS_NONE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_OUTPUT_DOWNMIX_MODE,
      g_param_spec_enum ("downmix-mode", "Mode for mono downmixed output",
          "Output anaglyph type to generate when downmixing to mono",
          GST_TYPE_GL_STEREO_DOWNMIX, DEFAULT_DOWNMIX,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_gl_view_convert_init (GstGLViewConvert * convert)
{
  convert->priv = gst_gl_view_convert_get_instance_private (convert);

  convert->shader = NULL;
  convert->downmix_mode = DEFAULT_DOWNMIX;
  convert->priv->input_mode = GST_VIDEO_MULTIVIEW_MODE_NONE;
  convert->priv->input_flags = GST_VIDEO_MULTIVIEW_FLAGS_NONE;
  convert->priv->output_mode = GST_VIDEO_MULTIVIEW_MODE_NONE;
  convert->priv->output_flags = GST_VIDEO_MULTIVIEW_FLAGS_NONE;

  convert->input_mode_override = GST_VIDEO_MULTIVIEW_MODE_NONE;
  convert->input_flags_override = GST_VIDEO_MULTIVIEW_FLAGS_NONE;
  convert->output_mode_override = GST_VIDEO_MULTIVIEW_MODE_NONE;
  convert->output_flags_override = GST_VIDEO_MULTIVIEW_FLAGS_NONE;

  gst_video_info_init (&convert->in_info);
  gst_video_info_init (&convert->out_info);
}

static void
gst_gl_view_convert_finalize (GObject * object)
{
  GstGLViewConvert *viewconvert;

  viewconvert = GST_GL_VIEW_CONVERT (object);

  gst_gl_view_convert_reset (viewconvert);

  gst_buffer_replace (&viewconvert->priv->primary_in, NULL);
  gst_buffer_replace (&viewconvert->priv->auxilliary_in, NULL);
  gst_buffer_replace (&viewconvert->priv->primary_out, NULL);
  gst_buffer_replace (&viewconvert->priv->auxilliary_out, NULL);

  if (viewconvert->context) {
    gst_object_unref (viewconvert->context);
    viewconvert->context = NULL;
  }

  G_OBJECT_CLASS (gst_gl_view_convert_parent_class)->finalize (object);
}

/**
 * gst_gl_view_convert_new:
 *
 * Returns: (transfer full): a new #GstGLViewConvert
 *
 * Since: 1.6
 */
GstGLViewConvert *
gst_gl_view_convert_new (void)
{
  GstGLViewConvert *convert;

  convert = g_object_new (GST_TYPE_GL_VIEW_CONVERT, NULL);
  gst_object_ref_sink (convert);

  return convert;
}

static void
_reset_gl (GstGLContext * context, GstGLViewConvert * viewconvert)
{
  const GstGLFuncs *gl = context->gl_vtable;

  if (viewconvert->priv->vao) {
    gl->DeleteVertexArrays (1, &viewconvert->priv->vao);
    viewconvert->priv->vao = 0;
  }

  if (viewconvert->priv->vertex_buffer) {
    gl->DeleteBuffers (1, &viewconvert->priv->vertex_buffer);
    viewconvert->priv->vertex_buffer = 0;
  }

  if (viewconvert->priv->vbo_indices) {
    gl->DeleteBuffers (1, &viewconvert->priv->vbo_indices);
    viewconvert->priv->vbo_indices = 0;
  }
}

/**
 * gst_gl_view_convert_set_context:
 * @viewconvert: a #GstGLViewConvert
 * @context: the #GstGLContext to set
 *
 * Set @context on @viewconvert
 *
 * Since: 1.6
 */
void
gst_gl_view_convert_set_context (GstGLViewConvert * viewconvert,
    GstGLContext * context)
{
  GstGLContext *old_context = NULL;

  g_return_if_fail (GST_IS_GL_VIEW_CONVERT (viewconvert));

  GST_OBJECT_LOCK (viewconvert);
  if (context != viewconvert->context) {
    gst_gl_view_convert_reset (viewconvert);
    if (viewconvert->context)
      old_context = viewconvert->context;
    viewconvert->context = context ? gst_object_ref (context) : NULL;
  }
  GST_OBJECT_UNLOCK (viewconvert);

  gst_clear_object (&old_context);
}

static gboolean
_view_convert_set_format (GstGLViewConvert * viewconvert,
    const GstVideoInfo * in_info, GstGLTextureTarget from_target,
    const GstVideoInfo * out_info, GstGLTextureTarget to_target)
{
  gboolean passthrough;
  g_return_val_if_fail (GST_IS_GL_VIEW_CONVERT (viewconvert), FALSE);

  if (gst_video_info_is_equal (in_info, &viewconvert->in_info) &&
      gst_video_info_is_equal (out_info, &viewconvert->out_info) &&
      viewconvert->from_texture_target == from_target &&
      viewconvert->to_texture_target == to_target)
    return TRUE;

  if (GST_VIDEO_INFO_FORMAT (in_info) != GST_VIDEO_FORMAT_RGBA ||
      GST_VIDEO_INFO_FORMAT (out_info) != GST_VIDEO_FORMAT_RGBA) {
    GST_ERROR_OBJECT (viewconvert,
        "Multiview conversion can currently only be performed on RGBA textures");
    return FALSE;
  }

  passthrough = gst_video_info_is_equal (in_info, out_info) &&
      from_target == to_target;

  if (!passthrough && to_target != GST_GL_TEXTURE_TARGET_2D
      && to_target != GST_GL_TEXTURE_TARGET_RECTANGLE)
    return FALSE;

  /* FIXME: Compare what changed and decide if we need a full reset or not */
  GST_OBJECT_LOCK (viewconvert);
  gst_gl_view_convert_reset (viewconvert);

  viewconvert->in_info = *in_info;
  viewconvert->out_info = *out_info;
  viewconvert->from_texture_target = from_target;
  viewconvert->to_texture_target = to_target;
  viewconvert->caps_passthrough = passthrough;

  gst_buffer_replace (&viewconvert->priv->primary_in, NULL);
  gst_buffer_replace (&viewconvert->priv->auxilliary_in, NULL);
  gst_buffer_replace (&viewconvert->priv->primary_out, NULL);
  gst_buffer_replace (&viewconvert->priv->auxilliary_out, NULL);
  GST_OBJECT_UNLOCK (viewconvert);

  return TRUE;
}

/**
 * gst_gl_view_convert_set_caps:
 * @viewconvert: a #GstGLViewConvert
 * @in_caps: input #GstCaps
 * @out_caps: output #GstCaps
 *
 * Initializes @viewconvert with the information required for conversion.
 *
 * Since: 1.6
 */
gboolean
gst_gl_view_convert_set_caps (GstGLViewConvert * viewconvert,
    GstCaps * in_caps, GstCaps * out_caps)
{
  GstVideoInfo in_info, out_info;
  GstCapsFeatures *in_features, *out_features;
  GstGLTextureTarget from_target = GST_GL_TEXTURE_TARGET_2D;
  GstGLTextureTarget to_target = GST_GL_TEXTURE_TARGET_2D;

  g_return_val_if_fail (GST_IS_GL_VIEW_CONVERT (viewconvert), FALSE);
  g_return_val_if_fail (GST_IS_CAPS (in_caps), FALSE);
  g_return_val_if_fail (GST_IS_CAPS (out_caps), FALSE);

  GST_INFO_OBJECT (viewconvert,
      "Configuring multiview conversion from caps %" GST_PTR_FORMAT
      " to %" GST_PTR_FORMAT, in_caps, out_caps);

  in_features = gst_caps_get_features (in_caps, 0);
  out_features = gst_caps_get_features (out_caps, 0);

  if (!gst_caps_features_contains (in_features,
          GST_CAPS_FEATURE_MEMORY_GL_MEMORY))
    return FALSE;
  if (!gst_caps_features_contains (out_features,
          GST_CAPS_FEATURE_MEMORY_GL_MEMORY))
    return FALSE;

  if (!gst_video_info_from_caps (&in_info, in_caps))
    return FALSE;
  if (!gst_video_info_from_caps (&out_info, out_caps))
    return FALSE;

  {
    GstStructure *in_s = gst_caps_get_structure (in_caps, 0);
    GstStructure *out_s = gst_caps_get_structure (out_caps, 0);

    if (gst_structure_has_field_typed (in_s, "texture-target", G_TYPE_STRING)) {
      from_target =
          gst_gl_texture_target_from_string (gst_structure_get_string (in_s,
              "texture-target"));
    }

    if (gst_structure_has_field_typed (out_s, "texture-target", G_TYPE_STRING)) {
      to_target =
          gst_gl_texture_target_from_string (gst_structure_get_string (out_s,
              "texture-target"));
    }

    if (to_target == GST_GL_TEXTURE_TARGET_NONE
        || from_target == GST_GL_TEXTURE_TARGET_NONE)
      /* invalid caps */
      return FALSE;
  }

  return _view_convert_set_format (viewconvert, &in_info, from_target,
      &out_info, to_target);
}

/* Function that can halve the value
 * of ints, fractions, int/fraction ranges and lists of ints/fractions */
static gboolean
_halve_value (GValue * out, const GValue * in_value)
{
  /* Fundamental fixed types first */
  if (G_VALUE_HOLDS_INT (in_value)) {
    g_value_init (out, G_TYPE_INT);
    g_value_set_int (out, MAX (g_value_get_int (in_value) / 2, 1));
  } else if (GST_VALUE_HOLDS_FRACTION (in_value)) {
    gint num, den;
    num = gst_value_get_fraction_numerator (in_value);
    den = gst_value_get_fraction_denominator (in_value);
    g_value_init (out, GST_TYPE_FRACTION);
    /* Don't adjust 'infinite' fractions */
    if ((num != 1 || den != 2147483647) && (num != 2147483647 || den != 1)) {
      /* FIXME - could do better approximation when den > G_MAXINT/2? */
      den = den > G_MAXINT / 2 ? G_MAXINT : den * 2;
    }
    gst_value_set_fraction (out, num, den);
  } else if (GST_VALUE_HOLDS_INT_RANGE (in_value)) {
    gint range_min = gst_value_get_int_range_min (in_value);
    gint range_max = gst_value_get_int_range_max (in_value);
    gint range_step = gst_value_get_int_range_step (in_value);
    g_value_init (out, GST_TYPE_INT_RANGE);
    if (range_min != 1)
      range_min = MAX (1, range_min / 2);
    if (range_max != G_MAXINT)
      range_max = MAX (1, range_max / 2);
    gst_value_set_int_range_step (out, range_min,
        range_max, MAX (1, range_step / 2));
  } else if (GST_VALUE_HOLDS_FRACTION_RANGE (in_value)) {
    GValue min_out = G_VALUE_INIT;
    GValue max_out = G_VALUE_INIT;
    const GValue *range_min = gst_value_get_fraction_range_min (in_value);
    const GValue *range_max = gst_value_get_fraction_range_max (in_value);
    _halve_value (&min_out, range_min);
    _halve_value (&max_out, range_max);
    g_value_init (out, GST_TYPE_FRACTION_RANGE);
    gst_value_set_fraction_range (out, &min_out, &max_out);
    g_value_unset (&min_out);
    g_value_unset (&max_out);
  } else if (GST_VALUE_HOLDS_LIST (in_value)) {
    gint i;
    g_value_init (out, GST_TYPE_LIST);
    for (i = 0; i < gst_value_list_get_size (in_value); i++) {
      const GValue *entry;
      GValue tmp = G_VALUE_INIT;

      entry = gst_value_list_get_value (in_value, i);
      /* Random list values might not be the right type */
      if (!_halve_value (&tmp, entry))
        goto fail;
      gst_value_list_append_and_take_value (out, &tmp);
    }
  } else {
    return FALSE;
  }

  return TRUE;
fail:
  g_value_unset (out);
  return FALSE;
}

static GstStructure *
_halve_structure_field (const GstStructure * in, const gchar * field_name)
{
  GstStructure *out;
  const GValue *in_value = gst_structure_get_value (in, field_name);
  GValue tmp = G_VALUE_INIT;

  if (G_UNLIKELY (in_value == NULL))
    return gst_structure_copy (in);     /* Field doesn't exist, leave it as is */

  if (!_halve_value (&tmp, in_value))
    return NULL;

  out = gst_structure_copy (in);
  gst_structure_set_value (out, field_name, &tmp);
  g_value_unset (&tmp);

  return out;
}

/* Function that can double the value
 * of ints, fractions, int/fraction ranges and lists of ints/fractions */
static gboolean
_double_value (GValue * out, const GValue * in_value)
{
  /* Fundamental fixed types first */
  if (G_VALUE_HOLDS_INT (in_value)) {
    gint n = g_value_get_int (in_value);
    g_value_init (out, G_TYPE_INT);
    if (n <= G_MAXINT / 2)
      g_value_set_int (out, n * 2);
    else
      g_value_set_int (out, G_MAXINT);
  } else if (GST_VALUE_HOLDS_FRACTION (in_value)) {
    gint num, den;
    num = gst_value_get_fraction_numerator (in_value);
    den = gst_value_get_fraction_denominator (in_value);
    g_value_init (out, GST_TYPE_FRACTION);
    /* Don't adjust 'infinite' fractions */
    if ((num != 1 || den != 2147483647) && (num != 2147483647 || den != 1)) {
      /* FIXME - could do better approximation when num > G_MAXINT/2? */
      num = num > G_MAXINT / 2 ? G_MAXINT : num * 2;
    }
    gst_value_set_fraction (out, num, den);
  } else if (GST_VALUE_HOLDS_INT_RANGE (in_value)) {
    gint range_min = gst_value_get_int_range_min (in_value);
    gint range_max = gst_value_get_int_range_max (in_value);
    gint range_step = gst_value_get_int_range_step (in_value);
    if (range_min != 1) {
      range_min = MIN (G_MAXINT / 2, range_min);
      range_min *= 2;
    }
    if (range_max != G_MAXINT) {
      range_max = MIN (G_MAXINT / 2, range_max);
      range_max *= 2;
    }
    range_step = MIN (G_MAXINT / 2, range_step);
    g_value_init (out, GST_TYPE_INT_RANGE);
    gst_value_set_int_range_step (out, range_min, range_max, range_step);
  } else if (GST_VALUE_HOLDS_FRACTION_RANGE (in_value)) {
    GValue min_out = G_VALUE_INIT;
    GValue max_out = G_VALUE_INIT;
    const GValue *range_min = gst_value_get_fraction_range_min (in_value);
    const GValue *range_max = gst_value_get_fraction_range_max (in_value);
    _double_value (&min_out, range_min);
    _double_value (&max_out, range_max);
    g_value_init (out, GST_TYPE_FRACTION_RANGE);
    gst_value_set_fraction_range (out, &min_out, &max_out);
    g_value_unset (&min_out);
    g_value_unset (&max_out);
  } else if (GST_VALUE_HOLDS_LIST (in_value)) {
    gint i;
    g_value_init (out, GST_TYPE_LIST);
    for (i = 0; i < gst_value_list_get_size (in_value); i++) {
      const GValue *entry;
      GValue tmp = G_VALUE_INIT;

      entry = gst_value_list_get_value (in_value, i);
      /* Random list values might not be the right type */
      if (!_double_value (&tmp, entry))
        goto fail;
      gst_value_list_append_and_take_value (out, &tmp);
    }
  } else {
    return FALSE;
  }

  return TRUE;
fail:
  g_value_unset (out);
  return FALSE;
}

static GstStructure *
_double_structure_field (const GstStructure * in, const gchar * field_name)
{
  GstStructure *out;
  const GValue *in_value = gst_structure_get_value (in, field_name);
  GValue tmp = G_VALUE_INIT;

  if (G_UNLIKELY (in_value == NULL))
    return gst_structure_copy (in);     /* Field doesn't exist, leave it as is */

  if (!_double_value (&tmp, in_value))
    return NULL;

  out = gst_structure_copy (in);
  gst_structure_set_value (out, field_name, &tmp);
  g_value_unset (&tmp);

  return out;
}

/* Return a copy of the caps with the requested field halved in value/range */
#if 0
static GstCaps *
_halve_caps_field (const GstCaps * in, const gchar * field_name)
{
  gint i;
  GstCaps *out = gst_caps_new_empty ();

  for (i = 0; i < gst_caps_get_size (in); i++) {
    const GstStructure *cur = gst_caps_get_structure (in, i);
    GstCapsFeatures *f = gst_caps_get_features (in, i);

    GstStructure *res = _halve_structure_field (cur, field_name);
    out =
        gst_caps_merge_structure_full (out, res,
        f ? gst_caps_features_copy (f) : NULL);
  }

  return out;
}
#endif

/* Return a copy of the caps with the requested field doubled in value/range */
static GstCaps *
_double_caps_field (const GstCaps * in, const gchar * field_name)
{
  gint i;
  GstCaps *out = gst_caps_new_empty ();

  for (i = 0; i < gst_caps_get_size (in); i++) {
    const GstStructure *cur = gst_caps_get_structure (in, i);
    GstCapsFeatures *f = gst_caps_get_features (in, i);

    GstStructure *res = _double_structure_field (cur, field_name);
    out =
        gst_caps_merge_structure_full (out, res,
        f ? gst_caps_features_copy (f) : NULL);
  }

  return out;
}

/* Takes ownership of the input caps  */
static GstCaps *
_expand_par_for_half_aspect (GstCaps * in, gboolean vertical_half_aspect)
{

  guint mview_flags, mview_flags_mask;
  GstCaps *out;
  GstStructure *tmp;

  out = gst_caps_new_empty ();

  while (gst_caps_get_size (in) > 0) {
    GstStructure *s;
    GstCapsFeatures *features;

    features = gst_caps_get_features (in, 0);
    if (features)
      features = gst_caps_features_copy (features);

    s = gst_caps_steal_structure (in, 0);

    if (!gst_structure_get_flagset (s, "multiview-flags", &mview_flags,
            &mview_flags_mask)) {
      gst_caps_append_structure_full (out, s, features);
      continue;
    }
    /* If the input doesn't care about the half-aspect flag, allow current PAR in either variant */
    if ((mview_flags_mask & GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT) == 0) {
      gst_caps_append_structure_full (out, s, features);
      continue;
    }
    if (!gst_structure_has_field (s, "pixel-aspect-ratio")) {
      /* No par field, dont-care the half-aspect flag */
      gst_structure_set (s, "multiview-flags",
          GST_TYPE_VIDEO_MULTIVIEW_FLAGSET,
          mview_flags & ~GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT,
          mview_flags_mask & ~GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT, NULL);
      gst_caps_append_structure_full (out, s, features);
      continue;
    }

    /* Halve or double PAR base on inputs input specified. */

    /* Append a copy with the half-aspect flag as-is */
    tmp = gst_structure_copy (s);
    out = gst_caps_merge_structure_full (out, tmp,
        features ? gst_caps_features_copy (features) : NULL);

    /* and then a copy inverted */
    if (mview_flags & GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT) {
      /* Input is half-aspect. Double/halve the PAR, clear the flag */
      if (vertical_half_aspect)
        tmp = _halve_structure_field (s, "pixel-aspect-ratio");
      else
        tmp = _double_structure_field (s, "pixel-aspect-ratio");
      /* Clear the flag */
      gst_structure_set (tmp, "multiview-flags",
          GST_TYPE_VIDEO_MULTIVIEW_FLAGSET,
          mview_flags & ~GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT,
          mview_flags_mask | GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT, NULL);
    } else {
      if (vertical_half_aspect)
        tmp = _double_structure_field (s, "pixel-aspect-ratio");
      else
        tmp = _halve_structure_field (s, "pixel-aspect-ratio");
      /* Set the flag */
      gst_structure_set (tmp, "multiview-flags",
          GST_TYPE_VIDEO_MULTIVIEW_FLAGSET,
          mview_flags | GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT,
          mview_flags_mask | GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT, NULL);
    }

    out = gst_caps_merge_structure_full (out, tmp,
        features ? gst_caps_features_copy (features) : NULL);

    gst_structure_free (s);
    if (features)
      gst_caps_features_free (features);
  }

  gst_caps_unref (in);

  return out;
}

/* If input supports top-bottom or row-interleaved, we may halve height to mono frames.
 * If input supports left-right, checkerboard, quincunx or column-interleaved,
 * we may halve width to mono frames.
 * For output of top-bottom or row-interleaved, we may double the mono height
 * For output of left-right, checkerboard, quincunx or column-interleaved,
 * we may double the mono width.
 * In all cases, if input has half-aspect and output does not, we may double the PAR
 * And if input does *not* have half-aspect flag and output does not, we may halve the PAR
 */
static GstCaps *
_expand_structure (GstGLViewConvert * viewconvert,
    GstCaps * out_caps, GstStructure * structure, GstCapsFeatures * features)
{
  GstCaps *expanded_caps, *tmp;
  GstCaps *mono_caps;
  const gchar *default_mview_mode_str = NULL;
  guint mview_flags, mview_flags_mask;
  const GValue *in_modes;
  gint i;

  /* Empty caps to accumulate into */
  expanded_caps = gst_caps_new_empty ();

  /* First, set defaults if multiview flags are missing */
  default_mview_mode_str =
      gst_video_multiview_mode_to_caps_string (GST_VIDEO_MULTIVIEW_MODE_MONO);

  mview_flags = GST_VIDEO_MULTIVIEW_FLAGS_NONE;
  mview_flags_mask = GST_FLAG_SET_MASK_EXACT;

  if (!gst_structure_has_field (structure, "multiview-mode")) {
    gst_structure_set (structure,
        "multiview-mode", G_TYPE_STRING, default_mview_mode_str, NULL);
  }
  if (!gst_structure_has_field (structure, "multiview-flags")) {
    gst_structure_set (structure,
        "multiview-flags", GST_TYPE_VIDEO_MULTIVIEW_FLAGSET, mview_flags,
        mview_flags_mask, NULL);
  } else {
    gst_structure_get_flagset (structure, "multiview-flags",
        &mview_flags, &mview_flags_mask);
  }

  in_modes = gst_structure_get_value (structure, "multiview-mode");
  mono_caps = gst_caps_new_empty ();
  if (gst_value_intersect (NULL, in_modes,
          gst_video_multiview_get_mono_modes ())) {
    GstStructure *new_struct = gst_structure_copy (structure);
    gst_structure_set_value (new_struct, "multiview-mode",
        gst_video_multiview_get_mono_modes ());
    /* Half-aspect makes no sense for mono or unpacked, get rid of it */
    if (mview_flags & GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT) {
      gst_structure_set (new_struct, "multiview-flags",
          GST_TYPE_VIDEO_MULTIVIEW_FLAGSET,
          mview_flags & ~GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT,
          mview_flags_mask & ~GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT, NULL);
    }
    gst_caps_append_structure_full (mono_caps, new_struct,
        features ? gst_caps_features_copy (features) : NULL);
  }
  if (gst_value_intersect (NULL, in_modes,
          gst_video_multiview_get_unpacked_modes ())) {
    GstStructure *new_struct = gst_structure_copy (structure);

    gst_structure_set_value (new_struct, "multiview-mode",
        gst_video_multiview_get_mono_modes ());

    /* Half-aspect makes no sense for mono or unpacked, get rid of it */
    if (mview_flags & GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT) {
      gst_structure_set (new_struct, "multiview-flags",
          GST_TYPE_VIDEO_MULTIVIEW_FLAGSET,
          mview_flags & ~GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT,
          mview_flags_mask & ~GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT, NULL);
    }
    gst_caps_append_structure_full (mono_caps, new_struct,
        features ? gst_caps_features_copy (features) : NULL);
  }

  if (gst_value_intersect (NULL, in_modes,
          gst_video_multiview_get_doubled_height_modes ())) {
    /* Append mono formats with height halved */
    GstStructure *new_struct = _halve_structure_field (structure, "height");
    gst_structure_set_value (new_struct, "multiview-mode",
        gst_video_multiview_get_mono_modes ());
    /* Normalise the half-aspect flag away */
    if (mview_flags & GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT) {
      GstStructure *s =
          _halve_structure_field (new_struct, "pixel-aspect-ratio");
      gst_structure_set (structure, "multiview-flags",
          GST_TYPE_VIDEO_MULTIVIEW_FLAGSET,
          mview_flags & ~GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT,
          mview_flags_mask | GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT, NULL);
      gst_structure_free (new_struct);
      new_struct = s;
    }
    mono_caps = gst_caps_merge_structure_full (mono_caps, new_struct,
        features ? gst_caps_features_copy (features) : NULL);
  }
  if (gst_value_intersect (NULL, in_modes,
          gst_video_multiview_get_doubled_width_modes ())) {
    /* Append mono formats with width halved */
    GstStructure *new_struct = _halve_structure_field (structure, "width");
    gst_structure_set_value (new_struct, "multiview-mode",
        gst_video_multiview_get_mono_modes ());
    /* Normalise the half-aspect flag away */
    if (mview_flags & GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT) {
      GstStructure *s =
          _double_structure_field (new_struct, "pixel-aspect-ratio");
      gst_structure_set (structure, "multiview-flags",
          GST_TYPE_VIDEO_MULTIVIEW_FLAGSET,
          mview_flags & ~GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT,
          mview_flags_mask | GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT, NULL);
      gst_structure_free (new_struct);
      new_struct = s;
    }
    mono_caps = gst_caps_merge_structure_full (mono_caps, new_struct,
        features ? gst_caps_features_copy (features) : NULL);
  }
  if (gst_value_intersect (NULL, in_modes,
          gst_video_multiview_get_doubled_size_modes ())) {
    /* Append checkerboard/doubled size formats with width & height halved */
    GstStructure *new_struct_w = _halve_structure_field (structure, "width");
    GstStructure *new_struct_wh =
        _halve_structure_field (new_struct_w, "height");
    gst_structure_free (new_struct_w);
    gst_structure_set_value (new_struct_wh, "multiview-mode",
        gst_video_multiview_get_mono_modes ());
    mono_caps = gst_caps_merge_structure_full (mono_caps, new_struct_wh,
        features ? gst_caps_features_copy (features) : NULL);
  }

  /* Everything is normalised now, unset the flags we can change */
  /* Remove the views field, as these are all 'mono' modes
   * Need to do this before we expand caps back out to frame packed modes */
  for (i = 0; i < gst_caps_get_size (mono_caps); i++) {
    GstStructure *s = gst_caps_get_structure (mono_caps, i);
    gst_structure_remove_fields (s, "views", NULL);
    if (gst_structure_get_flagset (s, "multiview-flags", &mview_flags,
            &mview_flags_mask)) {
      /* Preserve only the half-aspect and mixed-mono flags, for now.
       * The rest we can change */
      mview_flags_mask &=
          (GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT |
          GST_VIDEO_MULTIVIEW_FLAGS_MIXED_MONO);
      gst_structure_set (s, "multiview-flags", GST_TYPE_VIDEO_MULTIVIEW_FLAGSET,
          mview_flags, mview_flags_mask, NULL);
    }
  }

  GST_TRACE_OBJECT (viewconvert,
      "Collected single-view caps %" GST_PTR_FORMAT, mono_caps);
  /* Put unpacked and mono modes first. We don't care about flags. Clear them */
  tmp = gst_caps_copy (mono_caps);
  for (i = 0; i < gst_caps_get_size (tmp); i++) {
    GstStructure *s = gst_caps_get_structure (tmp, i);
    gst_structure_remove_fields (s, "views", NULL);
    if (gst_structure_get_flagset (s, "multiview-flags", &mview_flags,
            &mview_flags_mask)) {
      /* We can change any flags for mono modes - half-aspect and mixed-mono have no meaning */
      mview_flags_mask = 0;
      gst_structure_set (s, "multiview-flags", GST_TYPE_VIDEO_MULTIVIEW_FLAGSET,
          mview_flags, mview_flags_mask, NULL);
    }
  }
  expanded_caps = gst_caps_merge (expanded_caps, tmp);

  /* Unpacked output modes have 2 views, for now */
  tmp = gst_caps_copy (mono_caps);
  gst_caps_set_value (tmp, "multiview-mode",
      gst_video_multiview_get_unpacked_modes ());
  for (i = 0; i < gst_caps_get_size (tmp); i++) {
    GstStructure *s = gst_caps_get_structure (tmp, i);
    gst_structure_set (s, "views", G_TYPE_INT, 2, NULL);
    if (gst_structure_get_flagset (s, "multiview-flags", &mview_flags,
            &mview_flags_mask)) {
      /* We can change any flags for unpacked modes - half-aspect and mixed-mono have no meaning */
      mview_flags_mask = 0;
      gst_structure_set (s, "multiview-flags", GST_TYPE_VIDEO_MULTIVIEW_FLAGSET,
          mview_flags, mview_flags_mask, NULL);
    }
  }
  expanded_caps = gst_caps_merge (expanded_caps, tmp);

  /* Double height output modes */
  tmp = _double_caps_field (mono_caps, "height");
  gst_caps_set_value (tmp, "multiview-mode",
      gst_video_multiview_get_doubled_height_modes ());
  tmp = _expand_par_for_half_aspect (tmp, TRUE);

  expanded_caps = gst_caps_merge (expanded_caps, tmp);

  /* Double width output modes */
  tmp = _double_caps_field (mono_caps, "width");
  gst_caps_set_value (tmp, "multiview-mode",
      gst_video_multiview_get_doubled_width_modes ());
  tmp = _expand_par_for_half_aspect (tmp, FALSE);

  expanded_caps = gst_caps_merge (expanded_caps, tmp);

  /* Double size output modes */
  {
    GstCaps *tmp_w = _double_caps_field (mono_caps, "width");
    tmp = _double_caps_field (tmp_w, "height");
    gst_caps_unref (tmp_w);
    gst_caps_set_value (tmp, "multiview-mode",
        gst_video_multiview_get_doubled_size_modes ());
    expanded_caps = gst_caps_merge (expanded_caps, tmp);
  }

  /* We're done with the mono caps now */
  gst_caps_unref (mono_caps);

  GST_TRACE_OBJECT (viewconvert,
      "expanded transform caps now %" GST_PTR_FORMAT, expanded_caps);

  if (gst_caps_is_empty (expanded_caps)) {
    gst_caps_unref (expanded_caps);
    return out_caps;
  }
  /* Really, we can rescale - so at this point we can append full-range
   * height/width/PAR as an unpreferred final option. */
  tmp = gst_caps_copy (expanded_caps);
  gst_caps_set_simple (tmp, "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
      "height", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);

  out_caps = gst_caps_merge (out_caps, expanded_caps);
  out_caps = gst_caps_merge (out_caps, tmp);
  return out_caps;
}

static GstCaps *
_intersect_with_mview_mode (GstCaps * caps,
    GstVideoMultiviewMode mode, GstVideoMultiviewFlags flags)
{
  GstCaps *filter, *result;
  const gchar *caps_str;

  caps_str = gst_video_multiview_mode_to_caps_string (mode);

  filter = gst_caps_new_simple ("video/x-raw",
      "multiview-mode", G_TYPE_STRING,
      caps_str, "multiview-flags", GST_TYPE_VIDEO_MULTIVIEW_FLAGSET, flags,
      GST_FLAG_SET_MASK_EXACT, NULL);

  if (mode == GST_VIDEO_MULTIVIEW_MODE_SEPARATED ||
      mode == GST_VIDEO_MULTIVIEW_MODE_FRAME_BY_FRAME)
    gst_caps_set_simple (filter, "views", G_TYPE_INT, 2, NULL);

  gst_caps_set_features (filter, 0, gst_caps_features_new_any ());

  GST_DEBUG ("Intersecting target caps %" GST_PTR_FORMAT
      " with caps %" GST_PTR_FORMAT, caps, filter);

  result = gst_caps_intersect_full (caps, filter, GST_CAPS_INTERSECT_FIRST);
  gst_caps_unref (filter);
  return result;
}

static GstCaps *
_intersect_with_mview_modes (GstCaps * caps, const GValue * modes)
{
  GstCaps *filter, *result;

  filter = gst_caps_new_empty_simple ("video/x-raw");

  gst_caps_set_value (filter, "multiview-mode", modes);
  gst_caps_set_features (filter, 0, gst_caps_features_new_any ());

  GST_DEBUG ("Intersecting target caps %" GST_PTR_FORMAT
      " with caps %" GST_PTR_FORMAT, caps, filter);

  result = gst_caps_intersect_full (caps, filter, GST_CAPS_INTERSECT_FIRST);
  gst_caps_unref (filter);
  return result;
}

/**
 * gst_gl_view_convert_transform_caps:
 * @viewconvert: a #GstGLViewConvert
 * @direction: a #GstPadDirection
 * @caps: (transfer none): the #GstCaps to transform
 * @filter: (transfer none): a set of filter #GstCaps
 *
 * Provides an implementation of #GstBaseTransformClass.transform_caps()
 *
 * Returns: (transfer full): the converted #GstCaps
 *
 * Since: 1.6
 */
GstCaps *
gst_gl_view_convert_transform_caps (GstGLViewConvert * viewconvert,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  gint i, n;
  GstCaps *base_caps = gst_static_caps_get (&caps_template);
  GstCaps *out_caps, *tmp_caps;

  g_return_val_if_fail (GST_IS_GL_VIEW_CONVERT (viewconvert), NULL);

  GST_DEBUG_OBJECT (viewconvert, "Direction %s "
      "input caps %" GST_PTR_FORMAT " filter %" GST_PTR_FORMAT,
      direction == GST_PAD_SINK ? "sink" : "src", caps, filter);

  /* We can only process GLmemory RGBA caps, start from that */
  caps = gst_caps_intersect (caps, base_caps);
  gst_caps_unref (base_caps);

  /* Change input/output to the formats we can convert to/from,
   * but keep the original caps at the start - we will always prefer
   * passthrough */
  if (direction == GST_PAD_SINK) {
    out_caps = gst_caps_copy (caps);
    if (viewconvert->input_mode_override != GST_VIDEO_MULTIVIEW_MODE_NONE) {
      GstVideoMultiviewMode mode = viewconvert->input_mode_override;
      GstVideoMultiviewFlags flags = viewconvert->input_flags_override;

      const gchar *caps_str = gst_video_multiview_mode_to_caps_string (mode);
      /* Coerce the input caps before transforming, so the sizes come out right */
      gst_caps_set_simple (out_caps, "multiview-mode", G_TYPE_STRING,
          caps_str, "multiview-flags", GST_TYPE_VIDEO_MULTIVIEW_FLAGSET, flags,
          GST_FLAG_SET_MASK_EXACT, NULL);
    }
  } else {
    out_caps = gst_caps_new_empty ();
  }

  for (i = 0; i < gst_caps_get_size (caps); i++) {
    GstStructure *structure = gst_caps_get_structure (caps, i);
    GstCapsFeatures *features = gst_caps_get_features (caps, i);
    out_caps = _expand_structure (viewconvert, out_caps, structure, features);
  }

  if (gst_caps_is_empty (out_caps))
    goto out;

  /* If we have an output mode override, limit things to that */
  if (direction == GST_PAD_SINK &&
      viewconvert->output_mode_override != GST_VIDEO_MULTIVIEW_MODE_NONE) {

    tmp_caps = _intersect_with_mview_mode (out_caps,
        viewconvert->output_mode_override, viewconvert->output_flags_override);

    gst_caps_unref (out_caps);
    out_caps = tmp_caps;
  } else if (viewconvert->input_mode_override != GST_VIDEO_MULTIVIEW_MODE_NONE) {
    /* Prepend a copy of our preferred input caps in case the peer
     * can handle them */
    tmp_caps = _intersect_with_mview_mode (out_caps,
        viewconvert->input_mode_override, viewconvert->input_flags_override);
    out_caps = gst_caps_merge (out_caps, tmp_caps);
  }
  if (direction == GST_PAD_SRC) {
    GstStructure *s;
    /* When generating input caps, we also need a copy of the mono caps
     * without multiview-mode or flags for backwards compat, at the end */
    tmp_caps = _intersect_with_mview_mode (caps,
        GST_VIDEO_MULTIVIEW_MODE_MONO, GST_VIDEO_MULTIVIEW_FLAGS_NONE);
    if (!gst_caps_is_empty (tmp_caps)) {
      s = gst_caps_get_structure (tmp_caps, 0);
      gst_structure_remove_fields (s, "multiview-mode", "multiview-flags",
          NULL);
      out_caps = gst_caps_merge (out_caps, tmp_caps);
    } else
      gst_caps_unref (tmp_caps);
  }
out:
  gst_caps_unref (caps);

  n = gst_caps_get_size (out_caps);
  for (i = 0; i < n; i++) {
    GstStructure *s = gst_caps_get_structure (out_caps, i);

    gst_structure_remove_fields (s, "texture-target", NULL);
  }

  GST_DEBUG_OBJECT (viewconvert, "Returning caps %" GST_PTR_FORMAT, out_caps);
  return out_caps;
}

static guint
_get_target_bitmask_from_g_value (const GValue * targets)
{
  guint new_targets = 0;

  if (targets == NULL) {
    new_targets = 1 << GST_GL_TEXTURE_TARGET_2D;
  } else if (G_TYPE_CHECK_VALUE_TYPE (targets, G_TYPE_STRING)) {
    GstGLTextureTarget target;
    const gchar *str;

    str = g_value_get_string (targets);
    target = gst_gl_texture_target_from_string (str);

    if (target)
      new_targets |= 1 << target;
  } else if (G_TYPE_CHECK_VALUE_TYPE (targets, GST_TYPE_LIST)) {
    gint j, m;

    m = gst_value_list_get_size (targets);
    for (j = 0; j < m; j++) {
      const GValue *val = gst_value_list_get_value (targets, j);
      GstGLTextureTarget target;
      const gchar *str;

      str = g_value_get_string (val);
      target = gst_gl_texture_target_from_string (str);
      if (target)
        new_targets |= 1 << target;
    }
  }

  return new_targets;
}

static GstCaps *
_fixate_texture_target (GstGLViewConvert * viewconvert,
    GstPadDirection direction, GstCaps * caps, GstCaps * other)
{
  GValue item = G_VALUE_INIT;
  const GValue *targets, *other_targets;
  guint targets_mask = 0, other_targets_mask = 0, result_mask;
  GstStructure *s, *s_other;

  other = gst_caps_make_writable (other);
  s = gst_caps_get_structure (caps, 0);
  s_other = gst_caps_get_structure (other, 0);

  other_targets = gst_structure_get_value (s_other, "texture-target");
  targets = gst_structure_get_value (s, "texture-target");

  targets_mask = _get_target_bitmask_from_g_value (targets);
  other_targets_mask = _get_target_bitmask_from_g_value (other_targets);

  result_mask = targets_mask & other_targets_mask;
  if (result_mask == 0) {
    /* nothing we can do here */
    return gst_caps_fixate (other);
  }

  if (direction == GST_PAD_SINK) {
    result_mask &=
        (1 << GST_GL_TEXTURE_TARGET_2D | 1 << GST_GL_TEXTURE_TARGET_RECTANGLE);
  } else {
    /* if the src caps has 2D support we can 'convert' to anything */
    if (targets_mask & (1 << GST_GL_TEXTURE_TARGET_2D))
      result_mask = -1;
    else
      result_mask = other_targets_mask;
  }

  g_value_init (&item, G_TYPE_STRING);
  if (result_mask & (1 << GST_GL_TEXTURE_TARGET_2D)) {
    g_value_set_static_string (&item, GST_GL_TEXTURE_TARGET_2D_STR);
  } else if (result_mask & (1 << GST_GL_TEXTURE_TARGET_RECTANGLE)) {
    g_value_set_static_string (&item, GST_GL_TEXTURE_TARGET_RECTANGLE_STR);
  } else if (result_mask & (1 << GST_GL_TEXTURE_TARGET_EXTERNAL_OES)) {
    g_value_set_static_string (&item, GST_GL_TEXTURE_TARGET_EXTERNAL_OES_STR);
  }

  gst_structure_set_value (s_other, "texture-target", &item);

  g_value_unset (&item);

  return gst_caps_fixate (other);
}

/**
 * gst_gl_view_convert_fixate_caps:
 * @viewconvert: a #GstGLViewConvert
 * @direction: a #GstPadDirection
 * @caps: (transfer none): the #GstCaps of @direction
 * @othercaps: (transfer full): the #GstCaps to fixate
 *
 * Provides an implementation of #GstBaseTransformClass.fixate_caps()
 *
 * Returns: (transfer full): the fixated #GstCaps
 *
 * Since: 1.6
 */
GstCaps *
gst_gl_view_convert_fixate_caps (GstGLViewConvert * viewconvert,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstVideoMultiviewMode mode = viewconvert->output_mode_override;
  GstVideoMultiviewFlags flags = viewconvert->output_flags_override;
  GstCaps *tmp;

  g_return_val_if_fail (GST_IS_GL_VIEW_CONVERT (viewconvert), NULL);

  othercaps = gst_caps_make_writable (othercaps);
  GST_LOG_OBJECT (viewconvert, "dir %s fixating %" GST_PTR_FORMAT
      " against caps %" GST_PTR_FORMAT,
      direction == GST_PAD_SINK ? "sink" : "src", othercaps, caps);

  if (direction == GST_PAD_SINK) {
    if (mode != GST_VIDEO_MULTIVIEW_MODE_NONE) {
      /* We have a requested output mode and are fixating source caps, try and enforce it */
      tmp = _intersect_with_mview_mode (othercaps, mode, flags);
      gst_caps_unref (othercaps);
      othercaps = tmp;
    } else {
      /* See if we can do passthrough */
      GstVideoInfo info;

      if (gst_video_info_from_caps (&info, caps)) {
        GstVideoMultiviewMode mode = GST_VIDEO_INFO_MULTIVIEW_MODE (&info);
        GstVideoMultiviewFlags flags = GST_VIDEO_INFO_MULTIVIEW_FLAGS (&info);

        if (viewconvert->input_mode_override != GST_VIDEO_MULTIVIEW_MODE_NONE) {
          mode = viewconvert->input_mode_override;
          flags = viewconvert->input_flags_override;
        }

        tmp = _intersect_with_mview_mode (othercaps, mode, flags);
        if (gst_caps_is_empty (tmp)) {
          /* Nope, we can't pass our input caps downstream */
          gst_caps_unref (tmp);
        } else {
          gst_caps_unref (othercaps);
          othercaps = tmp;
          goto done;
        }
      }

      /* Prefer an unpacked mode for output */
      tmp =
          _intersect_with_mview_modes (othercaps,
          gst_video_multiview_get_unpacked_modes ());
      if (!gst_caps_is_empty (tmp)) {
        gst_caps_unref (othercaps);
        othercaps = tmp;
      } else {
        gst_caps_unref (tmp);
      }
    }
  } else if (viewconvert->input_mode_override != GST_VIDEO_MULTIVIEW_MODE_NONE) {
    /* See if we can coerce the caps into matching input mode/flags,
     * in case it doesn't care at all, but allow it not to too */
    mode = viewconvert->input_mode_override;
    flags = viewconvert->input_flags_override;
    tmp = _intersect_with_mview_mode (othercaps, mode, flags);
    if (gst_caps_is_empty (tmp)) {
      /* Nope, we can pass our input caps downstream */
      gst_caps_unref (tmp);
    } else {
      gst_caps_unref (othercaps);
      othercaps = tmp;
    }
  }

  othercaps = _fixate_texture_target (viewconvert, direction, caps, othercaps);

done:
  GST_DEBUG_OBJECT (viewconvert, "dir %s fixated to %" GST_PTR_FORMAT
      " against caps %" GST_PTR_FORMAT,
      direction == GST_PAD_SINK ? "sink" : "src", othercaps, caps);
  return othercaps;
}

/**
 * gst_gl_view_convert_reset:
 * @viewconvert: a #GstGLViewConvert
 *
 * Reset @viewconvert to the default state.  Further operation will require
 * setting the caps with gst_gl_view_convert_set_caps().
 *
 * Since: 1.6
 */
void
gst_gl_view_convert_reset (GstGLViewConvert * viewconvert)
{
  g_return_if_fail (GST_IS_GL_VIEW_CONVERT (viewconvert));

  gst_clear_object (&viewconvert->shader);
  gst_clear_object (&viewconvert->fbo);

  if (viewconvert->context) {
    gst_gl_context_thread_add (viewconvert->context,
        (GstGLContextThreadFunc) _reset_gl, viewconvert);
  }

  viewconvert->initted = FALSE;
  viewconvert->reconfigure = FALSE;
}

static void
gst_gl_view_convert_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGLViewConvert *convert = GST_GL_VIEW_CONVERT (object);
  switch (prop_id) {
    case PROP_INPUT_LAYOUT:
      convert->input_mode_override = g_value_get_enum (value);
      break;
    case PROP_INPUT_FLAGS:
      convert->input_flags_override = g_value_get_flags (value);
      break;
    case PROP_OUTPUT_LAYOUT:
      convert->output_mode_override = g_value_get_enum (value);
      break;
    case PROP_OUTPUT_FLAGS:
      convert->output_flags_override = g_value_get_flags (value);
      break;
    case PROP_OUTPUT_DOWNMIX_MODE:
      convert->downmix_mode = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_LOCK (convert);
  convert->reconfigure = TRUE;
  GST_OBJECT_UNLOCK (convert);
}

static void
gst_gl_view_convert_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGLViewConvert *convert = GST_GL_VIEW_CONVERT (object);
  switch (prop_id) {
    case PROP_INPUT_LAYOUT:
      g_value_set_enum (value, convert->input_mode_override);
      break;
    case PROP_INPUT_FLAGS:
      g_value_set_flags (value, convert->input_flags_override);
      break;
    case PROP_OUTPUT_LAYOUT:
      g_value_set_enum (value, convert->output_mode_override);
      break;
    case PROP_OUTPUT_FLAGS:
      g_value_set_flags (value, convert->output_flags_override);
      break;
    case PROP_OUTPUT_DOWNMIX_MODE:
      g_value_set_enum (value, convert->downmix_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/**
 * gst_gl_view_convert_perform:
 * @viewconvert: a #GstGLViewConvert
 * @inbuf: (transfer none): the #GstGLMemory filled #GstBuffer to convert
 *
 * Converts the data contained by @inbuf using the formats specified by the
 * #GstCaps passed to gst_gl_view_convert_set_caps()
 *
 * Returns: (transfer full) (nullable): a converted #GstBuffer or %NULL
 *
 * Since: 1.6
 */
GstBuffer *
gst_gl_view_convert_perform (GstGLViewConvert * viewconvert, GstBuffer * inbuf)
{
  GstBuffer *out;

  if (gst_gl_view_convert_submit_input_buffer (viewconvert,
          GST_BUFFER_IS_DISCONT (inbuf), gst_buffer_ref (inbuf)) != GST_FLOW_OK)
    return NULL;
  if (gst_gl_view_convert_get_output (viewconvert, &out) != GST_FLOW_OK)
    return NULL;

  return out;
}

/* called by _init_convert (in the gl thread) */
static gboolean
_init_view_convert_fbo (GstGLViewConvert * viewconvert)
{
  guint out_width, out_height;

  out_width = GST_VIDEO_INFO_WIDTH (&viewconvert->out_info);
  out_height = GST_VIDEO_INFO_HEIGHT (&viewconvert->out_info);

  viewconvert->fbo =
      gst_gl_framebuffer_new_with_default_depth (viewconvert->context,
      out_width, out_height);

  return viewconvert->fbo != NULL;
}

/* free after use */
static gchar *
_get_shader_string (GstGLViewConvert * viewconvert, GstGLShader * shader,
    GstVideoMultiviewMode in_mode, GstVideoMultiviewMode out_mode,
    GstGLSLVersion version, GstGLSLProfile profile)
{
  const gchar *input_str, *output_str;
  gboolean mono_input = FALSE;
  gchar *tmp, *tmp2;
  GString *str = g_string_new (NULL);
  guint n_outputs = 1;

  switch (in_mode) {
    case GST_VIDEO_MULTIVIEW_MODE_NONE:
    case GST_VIDEO_MULTIVIEW_MODE_MONO:
    case GST_VIDEO_MULTIVIEW_MODE_LEFT:
    case GST_VIDEO_MULTIVIEW_MODE_RIGHT:
      mono_input = TRUE;
      /* Fall through */
    default:
      input_str = frag_input;
      break;
  }

  switch (out_mode) {
    case GST_VIDEO_MULTIVIEW_MODE_LEFT:
      output_str = frag_output_left;
      break;
    case GST_VIDEO_MULTIVIEW_MODE_RIGHT:
      output_str = frag_output_right;
      break;
    case GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE_QUINCUNX:
      /* FIXME: implement properly with sub-sampling */
    case GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE:
      output_str = frag_output_side_by_side;
      break;
    case GST_VIDEO_MULTIVIEW_MODE_TOP_BOTTOM:
      output_str = frag_output_top_bottom;
      break;
    case GST_VIDEO_MULTIVIEW_MODE_COLUMN_INTERLEAVED:
      output_str = frag_output_column_interleaved;
      break;
    case GST_VIDEO_MULTIVIEW_MODE_ROW_INTERLEAVED:
      output_str = frag_output_row_interleaved;
      break;
    case GST_VIDEO_MULTIVIEW_MODE_SEPARATED:
    case GST_VIDEO_MULTIVIEW_MODE_FRAME_BY_FRAME:
      output_str = frag_output_separated;
      n_outputs = 2;
      break;
    case GST_VIDEO_MULTIVIEW_MODE_CHECKERBOARD:
      output_str = frag_output_checkerboard;
      break;
    case GST_VIDEO_MULTIVIEW_MODE_NONE:
    case GST_VIDEO_MULTIVIEW_MODE_MONO:
    default:
      if (mono_input)
        output_str = frag_output_left;
      else
        output_str = frag_output_downmix;
      break;
  }

  if (viewconvert->from_texture_target == GST_GL_TEXTURE_TARGET_EXTERNAL_OES)
    g_string_append (str, glsl_OES_extension_string);

  g_string_append (str,
      gst_gl_shader_string_get_highest_precision (viewconvert->context, version,
          profile));
  g_string_append (str, fragment_header);

  /* GL 3.3+ and GL ES 3.x */
  if ((profile == GST_GLSL_PROFILE_CORE && version >= GST_GLSL_VERSION_330)
      || (profile == GST_GLSL_PROFILE_ES && version >= GST_GLSL_VERSION_300)) {
    if (n_outputs > 1) {
      gint i;

      for (i = 0; i < n_outputs; i++) {
        g_string_append_printf (str,
            "layout(location = %d) out vec4 fragColor_%d;\n", i, i);
      }
    } else {
      g_string_append (str, "layout (location = 0) out vec4 fragColor;\n");
    }
  } else if (profile == GST_GLSL_PROFILE_CORE
      && version >= GST_GLSL_VERSION_150) {
    /* no layout specifiers, use glBindFragDataLocation instead */
    if (n_outputs > 1) {
      gint i;

      for (i = 0; i < n_outputs; i++) {
        gchar *var_name = g_strdup_printf ("fragColor_%d", i);
        g_string_append_printf (str, "out vec4 %s;\n", var_name);
        gst_gl_shader_bind_frag_data_location (shader, i, var_name);
        g_free (var_name);
      }
    } else {
      g_string_append (str, "out vec4 fragColor;\n");
      gst_gl_shader_bind_frag_data_location (shader, 0, "fragColor");
    }
  }

  {
    const gchar *varying = NULL;

    if ((profile == GST_GLSL_PROFILE_ES && version >= GST_GLSL_VERSION_300)
        || (profile == GST_GLSL_PROFILE_CORE
            && version >= GST_GLSL_VERSION_150)) {
      varying = "in";
    } else {
      varying = "varying";
    }
    g_string_append_printf (str,
        "\n%s vec2 v_texcoord;\nvoid main() {\nvec4 l, r;\n", varying);
  }

  g_string_append (str, input_str);
  g_string_append (str, output_str);
  g_string_append (str, "\n}");
  tmp = g_string_free (str, FALSE);

  tmp2 =
      _gst_glsl_mangle_shader (tmp, GL_FRAGMENT_SHADER,
      GST_GL_TEXTURE_TARGET_2D, viewconvert->from_texture_target,
      viewconvert->context, &version, &profile);

  return tmp2;
}

static void
_bind_buffer (GstGLViewConvert * viewconvert)
{
  const GstGLFuncs *gl = viewconvert->context->gl_vtable;
  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, viewconvert->priv->vbo_indices);
  gl->BindBuffer (GL_ARRAY_BUFFER, viewconvert->priv->vertex_buffer);
  /* Load the vertex position */
  gl->VertexAttribPointer (viewconvert->priv->attr_position, 3, GL_FLOAT,
      GL_FALSE, 5 * sizeof (GLfloat), (void *) 0);
  /* Load the texture coordinate */
  gl->VertexAttribPointer (viewconvert->priv->attr_texture, 2, GL_FLOAT,
      GL_FALSE, 5 * sizeof (GLfloat), (void *) (3 * sizeof (GLfloat)));
  gl->EnableVertexAttribArray (viewconvert->priv->attr_position);
  gl->EnableVertexAttribArray (viewconvert->priv->attr_texture);
}

static void
_unbind_buffer (GstGLViewConvert * viewconvert)
{
  const GstGLFuncs *gl = viewconvert->context->gl_vtable;
  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
  gl->BindBuffer (GL_ARRAY_BUFFER, 0);
  gl->DisableVertexAttribArray (viewconvert->priv->attr_position);
  gl->DisableVertexAttribArray (viewconvert->priv->attr_texture);
}

/* Called in the gl thread */
static gboolean
_init_view_convert (GstGLViewConvert * viewconvert)
{
  GstGLViewConvertPrivate *priv = viewconvert->priv;
  GstVideoMultiviewMode in_mode = priv->input_mode;
  GstVideoMultiviewMode out_mode = priv->output_mode;
  GstVideoMultiviewFlags in_flags = priv->input_flags;
  GstVideoMultiviewFlags out_flags = priv->output_flags;
  gfloat tex_scale[2][2] = {
    {1., 1.},
    {1., 1.}
  };
  gfloat offsets[2][2] = {
    {0., 0.},
    {0., 0.}
  };
  gchar *fragment_source_str;
  GstGLFuncs *gl;
  gint l_index, r_index;

  gl = viewconvert->context->gl_vtable;
  if (viewconvert->reconfigure)
    gst_gl_view_convert_reset (viewconvert);
  if (viewconvert->initted)
    return TRUE;

  GST_LOG_OBJECT (viewconvert,
      "Initializing multiview conversion from %s mode %d flags 0x%x w %u h %u to "
      "%s mode %d flags 0x%x w %u h %u",
      gst_video_format_to_string (GST_VIDEO_INFO_FORMAT
          (&viewconvert->in_info)), in_mode, in_flags,
      viewconvert->in_info.width, viewconvert->in_info.height,
      gst_video_format_to_string (GST_VIDEO_INFO_FORMAT
          (&viewconvert->out_info)), out_mode, out_flags,
      viewconvert->out_info.width, viewconvert->out_info.height);

  if (!gl->CreateProgramObject && !gl->CreateProgram) {
    GST_ERROR_OBJECT (viewconvert, "Cannot perform multiview conversion "
        "without OpenGL shaders");
    goto error;
  }

  if (out_mode == GST_VIDEO_MULTIVIEW_MODE_SEPARATED
      || out_mode == GST_VIDEO_MULTIVIEW_MODE_FRAME_BY_FRAME) {
    if (!gl->DrawBuffers) {
      GST_ERROR_OBJECT (viewconvert,
          "Separate texture output mode requested however the current "
          "OpenGL API does not support drawing to multiple buffers");
      goto error;
    }
  }

  if ((in_flags & GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_VIEW_FIRST) ==
      (out_flags & GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_VIEW_FIRST)) {
    l_index = 0;
    r_index = 1;
  } else {
    GST_LOG_OBJECT (viewconvert, "Switching left/right views");
    /* Swap the views */
    l_index = 1;
    r_index = 0;
  }

  if (in_mode < GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE) {        /* unknown/mono/left/right single image */
  } else if (in_mode == GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE ||
      in_mode == GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE_QUINCUNX) {
    /* Side-by-side input */
    offsets[r_index][0] += 0.5 * tex_scale[r_index][0];
    tex_scale[0][0] *= 0.5f;    /* Half horizontal scale */
    tex_scale[1][0] *= 0.5f;
  } else if (in_mode == GST_VIDEO_MULTIVIEW_MODE_TOP_BOTTOM) {  /* top-bottom */
    offsets[r_index][1] += 0.5 * tex_scale[r_index][1];
    tex_scale[0][1] *= 0.5f;    /* Half vertical scale */
    tex_scale[1][1] *= 0.5f;
  }

  /* Flipped is vertical, flopped is horizontal.
   * Adjust and offset per-view scaling. This needs to be done
   * after the input scaling already splits the views, before
   * adding any output scaling. */
  if ((in_flags & GST_VIDEO_MULTIVIEW_FLAGS_LEFT_FLIPPED) !=
      (out_flags & GST_VIDEO_MULTIVIEW_FLAGS_LEFT_FLIPPED)) {
    offsets[l_index][1] += tex_scale[l_index][1];
    tex_scale[l_index][1] *= -1.0;
  }
  if ((in_flags & GST_VIDEO_MULTIVIEW_FLAGS_LEFT_FLOPPED) !=
      (out_flags & GST_VIDEO_MULTIVIEW_FLAGS_LEFT_FLOPPED)) {
    offsets[l_index][0] += tex_scale[l_index][0];
    tex_scale[l_index][0] *= -1.0;
  }
  if ((in_flags & GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_FLIPPED) !=
      (out_flags & GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_FLIPPED)) {
    offsets[r_index][1] += tex_scale[r_index][1];
    tex_scale[r_index][1] *= -1.0;
  }
  if ((in_flags & GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_FLOPPED) !=
      (out_flags & GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_FLOPPED)) {
    offsets[r_index][0] += tex_scale[r_index][0];
    tex_scale[r_index][0] *= -1.0;
  }

  if (out_mode == GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE ||
      out_mode == GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE_QUINCUNX) {
    /* Side-by-Side */
    offsets[1][0] -= tex_scale[1][0];
    tex_scale[0][0] *= 2.0f;
    tex_scale[1][0] *= 2.0f;
  } else if (out_mode == GST_VIDEO_MULTIVIEW_MODE_TOP_BOTTOM) {
    offsets[1][1] -= tex_scale[1][1];
    tex_scale[0][1] *= 2.0f;
    tex_scale[1][1] *= 2.0f;
  }

  GST_DEBUG_OBJECT (viewconvert,
      "Scaling matrix [ %f, %f ] [ %f %f]. Offsets [ %f, %f ] [ %f, %f ]",
      tex_scale[0][0], tex_scale[0][1],
      tex_scale[1][0], tex_scale[1][1],
      offsets[0][0], offsets[0][1], offsets[1][0], offsets[1][1]);

  viewconvert->shader = gst_gl_shader_new (viewconvert->context);
  {
    GstGLSLVersion version;
    GstGLSLProfile profile;
    GstGLSLStage *vert, *frag;
    gchar *tmp, *tmp1, *version_str;
    const gchar *strings[2];
    GError *error = NULL;

    tmp =
        _gst_glsl_mangle_shader
        (gst_gl_shader_string_vertex_mat4_vertex_transform, GL_VERTEX_SHADER,
        GST_GL_TEXTURE_TARGET_2D, viewconvert->from_texture_target,
        viewconvert->context, &version, &profile);

    tmp1 = gst_glsl_version_profile_to_string (version, profile);
    version_str = g_strdup_printf ("#version %s\n", tmp1);
    g_free (tmp1);
    strings[0] = version_str;

    strings[1] = tmp;
    vert =
        gst_glsl_stage_new_with_strings (viewconvert->context,
        GL_VERTEX_SHADER, version, profile, 2, strings);
    g_free (tmp);

    if (!gst_gl_shader_compile_attach_stage (viewconvert->shader, vert, &error)) {
      GST_ERROR_OBJECT (viewconvert, "Failed to compile vertex stage %s",
          error->message);
      gst_object_unref (viewconvert->shader);
      viewconvert->shader = NULL;
      g_free (version_str);
      goto error;
    }

    fragment_source_str = _get_shader_string (viewconvert, viewconvert->shader,
        in_mode, out_mode, version, profile);
    strings[1] = fragment_source_str;

    frag =
        gst_glsl_stage_new_with_strings (viewconvert->context,
        GL_FRAGMENT_SHADER, version, profile, 2, strings);
    g_free (version_str);

    if (!gst_gl_shader_compile_attach_stage (viewconvert->shader, frag, &error)) {
      GST_ERROR_OBJECT (viewconvert, "Failed to compile fragment stage %s",
          error->message);
      g_free (fragment_source_str);
      gst_object_unref (viewconvert->shader);
      viewconvert->shader = NULL;
      goto error;
    }
    g_free (fragment_source_str);

    if (!gst_gl_shader_link (viewconvert->shader, &error)) {
      GST_ERROR_OBJECT (viewconvert, "Failed to link conversion shader %s",
          error->message);
      gst_object_unref (viewconvert->shader);
      viewconvert->shader = NULL;
      goto error;
    }
  }

  viewconvert->priv->attr_position =
      gst_gl_shader_get_attribute_location (viewconvert->shader, "a_position");
  viewconvert->priv->attr_texture =
      gst_gl_shader_get_attribute_location (viewconvert->shader, "a_texcoord");
  gst_gl_shader_use (viewconvert->shader);
  gst_gl_shader_set_uniform_2fv (viewconvert->shader, "tex_scale",
      2, tex_scale[0]);
  gst_gl_shader_set_uniform_2fv (viewconvert->shader, "offsets", 2, offsets[0]);
  gst_gl_shader_set_uniform_1f (viewconvert->shader, "width",
      GST_VIDEO_INFO_WIDTH (&viewconvert->out_info));
  gst_gl_shader_set_uniform_1f (viewconvert->shader, "height",
      GST_VIDEO_INFO_HEIGHT (&viewconvert->out_info));
  gst_gl_shader_set_uniform_matrix_3fv (viewconvert->shader, "downmix",
      2, FALSE, &downmix_matrices[viewconvert->downmix_mode][0][0]);
  gst_gl_shader_set_uniform_matrix_4fv (viewconvert->shader, "u_transformation",
      1, FALSE, identity_matrix);
  if (in_mode == GST_VIDEO_MULTIVIEW_MODE_SEPARATED ||
      in_mode == GST_VIDEO_MULTIVIEW_MODE_FRAME_BY_FRAME) {
    gst_gl_shader_set_uniform_1i (viewconvert->shader, "tex_l", l_index);
    gst_gl_shader_set_uniform_1i (viewconvert->shader, "tex_r", r_index);
  } else {
    gst_gl_shader_set_uniform_1i (viewconvert->shader, "tex_l", 0);
    gst_gl_shader_set_uniform_1i (viewconvert->shader, "tex_r", 0);
  }
  gst_gl_context_clear_shader (viewconvert->context);
  if (!_init_view_convert_fbo (viewconvert)) {
    goto error;
  }

  if (!viewconvert->priv->vertex_buffer) {
    if (gl->GenVertexArrays) {
      gl->GenVertexArrays (1, &viewconvert->priv->vao);
      gl->BindVertexArray (viewconvert->priv->vao);
    }

    gl->GenBuffers (1, &viewconvert->priv->vertex_buffer);
    gl->BindBuffer (GL_ARRAY_BUFFER, viewconvert->priv->vertex_buffer);
    gl->BufferData (GL_ARRAY_BUFFER, 4 * 5 * sizeof (GLfloat), vertices,
        GL_STATIC_DRAW);
    gl->GenBuffers (1, &viewconvert->priv->vbo_indices);
    gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, viewconvert->priv->vbo_indices);
    gl->BufferData (GL_ELEMENT_ARRAY_BUFFER, sizeof (indices), indices,
        GL_STATIC_DRAW);
    if (gl->GenVertexArrays) {
      _bind_buffer (viewconvert);
      gl->BindVertexArray (0);
    }

    gl->BindBuffer (GL_ARRAY_BUFFER, 0);
    gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
  }

  viewconvert->initted = TRUE;
  return TRUE;
error:
  return FALSE;
}

static gboolean
_do_view_convert_draw (GstGLContext * context, GstGLViewConvert * viewconvert)
{
  GstGLViewConvertPrivate *priv = viewconvert->priv;
  GstGLFuncs *gl;
  guint out_width, out_height;
  gint out_views, i;
  GLenum multipleRT[] = {
    GL_COLOR_ATTACHMENT0,
    GL_COLOR_ATTACHMENT1,
    GL_COLOR_ATTACHMENT2
  };
  GstVideoMultiviewMode in_mode = priv->input_mode;
  GstVideoMultiviewMode out_mode = priv->output_mode;
  guint from_gl_target =
      gst_gl_texture_target_to_gl (viewconvert->from_texture_target);

  gl = context->gl_vtable;

  gst_gl_framebuffer_bind (viewconvert->fbo);

  if (out_mode == GST_VIDEO_MULTIVIEW_MODE_SEPARATED ||
      out_mode == GST_VIDEO_MULTIVIEW_MODE_FRAME_BY_FRAME) {
    out_views = viewconvert->out_info.views;
  } else {
    out_views = 1;
  }

  /* attach the texture to the FBO to renderer to */
  for (i = 0; i < out_views; i++) {
    GstGLBaseMemory *tex = (GstGLBaseMemory *) priv->out_tex[i];

    gst_gl_framebuffer_attach (viewconvert->fbo, GL_COLOR_ATTACHMENT0 + i, tex);
  }

  if (gl->DrawBuffers)
    gl->DrawBuffers (out_views, multipleRT);
  else if (gl->DrawBuffer)
    gl->DrawBuffer (GL_COLOR_ATTACHMENT0);

  gst_gl_framebuffer_get_effective_dimensions (viewconvert->fbo, &out_width,
      &out_height);
  gl->Viewport (0, 0, out_width, out_height);

  gst_gl_shader_use (viewconvert->shader);

  /* FIXME: the auxiliary buffer could have a different transform matrix */
  {
    GstVideoAffineTransformationMeta *af_meta;
    gfloat matrix[16];

    af_meta =
        gst_buffer_get_video_affine_transformation_meta (priv->primary_in);
    gst_gl_get_affine_transformation_meta_as_ndc (af_meta, matrix);
    gst_gl_shader_set_uniform_matrix_4fv (viewconvert->shader,
        "u_transformation", 1, FALSE, matrix);
  }

  if (gl->BindVertexArray)
    gl->BindVertexArray (priv->vao);
  _bind_buffer (viewconvert);

  if (in_mode == GST_VIDEO_MULTIVIEW_MODE_SEPARATED ||
      in_mode == GST_VIDEO_MULTIVIEW_MODE_FRAME_BY_FRAME) {
    if (priv->in_tex[1] == NULL) {
      GST_ERROR_OBJECT (viewconvert,
          "No 2nd view available during conversion!");
      return FALSE;
    }
    gl->ActiveTexture (GL_TEXTURE1);
    gl->BindTexture (from_gl_target, priv->in_tex[1]->tex_id);
  }

  gl->ActiveTexture (GL_TEXTURE0);
  gl->BindTexture (from_gl_target, priv->in_tex[0]->tex_id);

  gl->ClearColor (0.0, 0.0, 0.0, 1.0);
  gl->Clear (GL_COLOR_BUFFER_BIT);

  gl->DrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);

  if (gl->BindVertexArray)
    gl->BindVertexArray (0);
  else
    _unbind_buffer (viewconvert);
  if (gl->DrawBuffer)
    gl->DrawBuffer (GL_COLOR_ATTACHMENT0);
  /* we are done with the shader */
  gst_gl_context_clear_shader (context);
  gst_gl_context_clear_framebuffer (context);

  return TRUE;
}

static gboolean
_gen_buffer (GstGLViewConvert * viewconvert, GstBuffer ** target)
{
  GstGLVideoAllocationParams *params;
  GstGLMemoryAllocator *mem_allocator;
  GstAllocator *allocator;
  GstVideoMeta *meta;

  *target = gst_buffer_new ();

  allocator =
      GST_ALLOCATOR (gst_gl_memory_allocator_get_default
      (viewconvert->context));
  mem_allocator = GST_GL_MEMORY_ALLOCATOR (allocator);
  params = gst_gl_video_allocation_params_new (viewconvert->context, NULL,
      &viewconvert->out_info, 0, NULL, viewconvert->to_texture_target, 0);

  if (!gst_gl_memory_setup_buffer (mem_allocator, *target, params, NULL, NULL,
          0)) {
    gst_gl_allocation_params_free ((GstGLAllocationParams *) params);
    gst_object_unref (allocator);
    return FALSE;
  }
  gst_object_unref (allocator);

  meta = gst_buffer_add_video_meta_full (*target, 0,
      GST_VIDEO_INFO_FORMAT (&viewconvert->out_info),
      GST_VIDEO_INFO_WIDTH (&viewconvert->out_info),
      GST_VIDEO_INFO_HEIGHT (&viewconvert->out_info),
      GST_VIDEO_INFO_N_PLANES (&viewconvert->out_info),
      viewconvert->out_info.offset, viewconvert->out_info.stride);

  if (params->valign)
    gst_video_meta_set_alignment (meta, *params->valign);

  gst_gl_allocation_params_free ((GstGLAllocationParams *) params);

  return TRUE;
}

static void
_do_view_convert (GstGLContext * context, GstGLViewConvert * viewconvert)
{
  GstGLViewConvertPrivate *priv = viewconvert->priv;
  guint in_width, in_height, out_width, out_height;
  GstMapInfo out_info[GST_VIDEO_MAX_PLANES], in_info[GST_VIDEO_MAX_PLANES];
  GstGLMemory *dest_tex[GST_VIDEO_MAX_PLANES];
  gboolean res = TRUE;
  gint i = 0, j = 0;
  gint in_views, out_views;
  GstVideoMultiviewMode in_mode;
  GstVideoMultiviewMode out_mode;
  GstGLSyncMeta *sync_meta;

  out_width = GST_VIDEO_INFO_WIDTH (&viewconvert->out_info);
  out_height = GST_VIDEO_INFO_HEIGHT (&viewconvert->out_info);
  in_width = GST_VIDEO_INFO_WIDTH (&viewconvert->in_info);
  in_height = GST_VIDEO_INFO_HEIGHT (&viewconvert->in_info);

  g_return_if_fail (priv->primary_out == NULL);
  g_return_if_fail (priv->auxilliary_out == NULL);

  in_mode = priv->input_mode;
  out_mode = priv->output_mode;

  if (in_mode == GST_VIDEO_MULTIVIEW_MODE_SEPARATED ||
      in_mode == GST_VIDEO_MULTIVIEW_MODE_FRAME_BY_FRAME)
    in_views = viewconvert->in_info.views;
  else
    in_views = 1;

  if (out_mode == GST_VIDEO_MULTIVIEW_MODE_SEPARATED ||
      out_mode == GST_VIDEO_MULTIVIEW_MODE_FRAME_BY_FRAME)
    out_views = viewconvert->out_info.views;
  else
    out_views = 1;

  if (!_init_view_convert (viewconvert)) {
    priv->result = FALSE;
    return;
  }

  if (!_gen_buffer (viewconvert, &priv->primary_out)) {
    GST_ERROR_OBJECT (viewconvert,
        "Failed to setup memory for primary output buffer");
    priv->result = FALSE;
    return;
  }

  if (out_mode == GST_VIDEO_MULTIVIEW_MODE_FRAME_BY_FRAME) {
    if (!_gen_buffer (viewconvert, &priv->auxilliary_out)) {
      GST_ERROR_OBJECT (viewconvert,
          "Failed to setup memory for second view output buffer");
      priv->result = FALSE;
      return;
    }
  }

  for (i = 0; i < in_views; i++) {
    if (in_mode == GST_VIDEO_MULTIVIEW_MODE_FRAME_BY_FRAME && i > 0) {
      priv->in_tex[i] =
          (GstGLMemory *) gst_buffer_peek_memory (priv->auxilliary_in, 0);
    } else {
      priv->in_tex[i] =
          (GstGLMemory *) gst_buffer_peek_memory (priv->primary_in, i);
    }
    if (!gst_is_gl_memory ((GstMemory *) priv->in_tex[i])) {
      GST_ERROR_OBJECT (viewconvert, "input must be GstGLMemory");
      res = FALSE;
      goto out;
    }
    if (!gst_memory_map ((GstMemory *) priv->in_tex[i],
            &in_info[i], GST_MAP_READ | GST_MAP_GL)) {
      GST_ERROR_OBJECT (viewconvert, "failed to map input memory %p",
          priv->in_tex[i]);
      res = FALSE;
      goto out;
    }
  }

  for (j = 0; j < out_views; j++) {
    GstGLMemory *out_tex;
    guint width, height;
    GstVideoInfo temp_info;

    if (j > 0 && out_mode == GST_VIDEO_MULTIVIEW_MODE_FRAME_BY_FRAME) {
      dest_tex[j] = out_tex =
          (GstGLMemory *) gst_buffer_peek_memory (priv->auxilliary_out, 0);
    } else {
      dest_tex[j] = out_tex =
          (GstGLMemory *) gst_buffer_peek_memory (priv->primary_out, j);
    }

    if (!gst_is_gl_memory ((GstMemory *) out_tex)) {
      GST_ERROR_OBJECT (viewconvert, "output must be GstGLMemory");
      res = FALSE;
      goto out;
    }

    width = gst_gl_memory_get_texture_width (out_tex);
    height = gst_gl_memory_get_texture_height (out_tex);
    gst_video_info_set_format (&temp_info, GST_VIDEO_FORMAT_RGBA, width,
        height);
    if (out_tex->tex_format == GST_GL_LUMINANCE
        || out_tex->tex_format == GST_GL_LUMINANCE_ALPHA
        || out_width != width || out_height != height) {
      /* Luminance formats are not color renderable */
      /* rendering to a framebuffer only renders the intersection of all
       * the attachments i.e. the smallest attachment size */
      if (!priv->out_tex[j]) {
        GstGLVideoAllocationParams *params;
        GstGLBaseMemoryAllocator *base_mem_allocator;
        GstAllocator *allocator;
        GstVideoInfo temp_info;

        gst_video_info_set_format (&temp_info, GST_VIDEO_FORMAT_RGBA, out_width,
            out_height);

        allocator =
            GST_ALLOCATOR (gst_gl_memory_allocator_get_default (context));
        base_mem_allocator = GST_GL_BASE_MEMORY_ALLOCATOR (allocator);
        params = gst_gl_video_allocation_params_new (context, NULL, &temp_info,
            0, NULL, viewconvert->to_texture_target, GST_GL_RGBA);

        priv->out_tex[j] =
            (GstGLMemory *) gst_gl_base_memory_alloc (base_mem_allocator,
            (GstGLAllocationParams *) params);

        gst_gl_allocation_params_free ((GstGLAllocationParams *) params);
        gst_object_unref (allocator);
      }
    } else {
      priv->out_tex[j] = out_tex;
    }

    if (!gst_memory_map ((GstMemory *) priv->out_tex[j],
            &out_info[j], GST_MAP_WRITE | GST_MAP_GL)) {
      GST_ERROR_OBJECT (viewconvert, "failed to map output memory %p",
          priv->out_tex[i]);
      res = FALSE;
      goto out;
    }
  }
  priv->n_out_tex = out_views;

  if (priv->primary_in) {
    if ((sync_meta = gst_buffer_get_gl_sync_meta (priv->primary_in))) {
      gst_gl_sync_meta_wait (sync_meta, context);
    }
  }

  if (priv->auxilliary_in) {
    if ((sync_meta = gst_buffer_get_gl_sync_meta (priv->auxilliary_in))) {
      gst_gl_sync_meta_wait (sync_meta, context);
    }
  }

  GST_LOG_OBJECT (viewconvert, "multiview splitting to textures:%p,%p,%p,%p "
      "dimensions:%ux%u, from textures:%p,%p,%p,%p dimensions:%ux%u",
      priv->out_tex[0], priv->out_tex[1],
      priv->out_tex[2], priv->out_tex[3],
      out_width, out_height, priv->in_tex[0],
      priv->in_tex[1], priv->in_tex[2], priv->in_tex[3], in_width, in_height);

  if (!_do_view_convert_draw (context, viewconvert))
    res = FALSE;
out:
  for (j--; j >= 0; j--) {
    GstGLMemory *out_tex;
    guint width, height;

    out_tex = dest_tex[j];

    width = gst_gl_memory_get_texture_width (out_tex);
    height = gst_gl_memory_get_texture_height (out_tex);

    gst_memory_unmap ((GstMemory *) priv->out_tex[j], &out_info[j]);
    if (out_tex != priv->out_tex[j]) {
      GstMapInfo to_info, from_info;
      if (!gst_memory_map ((GstMemory *) priv->out_tex[j],
              &from_info, GST_MAP_READ | GST_MAP_GL)) {
        GST_ERROR_OBJECT (viewconvert, "Failed to map intermediate memory");
        res = FALSE;
        continue;
      }
      if (!gst_memory_map ((GstMemory *) out_tex, &to_info,
              GST_MAP_WRITE | GST_MAP_GL)) {
        GST_ERROR_OBJECT (viewconvert, "Failed to map intermediate memory");
        res = FALSE;
        continue;
      }
      gst_gl_memory_copy_into (priv->out_tex[j], out_tex->tex_id,
          viewconvert->to_texture_target, out_tex->tex_format, width, height);
      gst_memory_unmap ((GstMemory *) out_tex, &to_info);
    }

    priv->out_tex[j] = NULL;
  }

  for (i--; i >= 0; i--) {
    gst_memory_unmap ((GstMemory *) priv->in_tex[i], &in_info[i]);
  }

  if (!res) {
    gst_buffer_replace (&priv->primary_out, NULL);
    gst_buffer_replace (&priv->auxilliary_out, NULL);
  }

  if (priv->primary_out) {
    if ((sync_meta = gst_buffer_add_gl_sync_meta (context, priv->primary_out)))
      gst_gl_sync_meta_set_sync_point (sync_meta, context);
  }

  if (priv->auxilliary_out) {
    if ((sync_meta =
            gst_buffer_add_gl_sync_meta (context, priv->auxilliary_out)))
      gst_gl_sync_meta_set_sync_point (sync_meta, context);
  }

  priv->result = res;
  return;
}

/**
 * gst_gl_view_convert_submit_input_buffer:
 * @viewconvert: a #GstGLViewConvert
 * @is_discont: true if we have a discontinuity
 * @input: (transfer full): a #GstBuffer
 *
 * Submit @input to be processed by @viewconvert
 *
 * Returns: a #GstFlowReturn
 *
 * Since: 1.6
 */
GstFlowReturn
gst_gl_view_convert_submit_input_buffer (GstGLViewConvert * viewconvert,
    gboolean is_discont, GstBuffer * input)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstVideoMultiviewMode mode;
  GstBuffer **target;

  if (is_discont) {
    gst_buffer_replace (&viewconvert->priv->primary_in, NULL);
    gst_buffer_replace (&viewconvert->priv->auxilliary_in, NULL);
  }

  mode = viewconvert->input_mode_override;
  if (mode == GST_VIDEO_MULTIVIEW_MODE_NONE)
    mode = GST_VIDEO_INFO_MULTIVIEW_MODE (&viewconvert->in_info);

  target = &viewconvert->priv->primary_in;

  /* For frame-by-frame mode, we need to collect the 2nd eye into
   * our auxiliary buffer */
  if (mode == GST_VIDEO_MULTIVIEW_MODE_FRAME_BY_FRAME) {
    if (!GST_BUFFER_FLAG_IS_SET (input, GST_VIDEO_BUFFER_FLAG_FIRST_IN_BUNDLE))
      target = &viewconvert->priv->auxilliary_in;
  }

  if (*target)
    gst_buffer_unref (*target);
  *target = input;

  return ret;
}

/**
 * gst_gl_view_convert_get_output:
 * @viewconvert: a #GstGLViewConvert
 * @outbuf_ptr: (out): a #GstBuffer
 *
 * Retrieve the processed output buffer placing the output in @outbuf_ptr.
 *
 * Returns: a #GstFlowReturn
 *
 * Since: 1.6
 */
GstFlowReturn
gst_gl_view_convert_get_output (GstGLViewConvert * viewconvert,
    GstBuffer ** outbuf_ptr)
{
  GstGLViewConvertPrivate *priv = viewconvert->priv;
  GstBuffer *outbuf = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  GstVideoMultiviewMode in_mode, out_mode;
  GstVideoMultiviewFlags in_flags, out_flags;

  g_return_val_if_fail (GST_IS_GL_VIEW_CONVERT (viewconvert), GST_FLOW_ERROR);
  g_return_val_if_fail (GST_IS_GL_CONTEXT (viewconvert->context),
      GST_FLOW_ERROR);

  GST_OBJECT_LOCK (viewconvert);

  /* See if a buffer is available already */
  if (priv->primary_out) {
    outbuf = viewconvert->priv->primary_out;
    priv->primary_out = NULL;
    goto done;
  }
  if (viewconvert->priv->auxilliary_out) {
    outbuf = priv->auxilliary_out;
    priv->auxilliary_out = NULL;
    goto done;
  }

  /* Check prereqs before processing a new input buffer */
  if (priv->primary_in == NULL)
    goto done;

  in_mode = viewconvert->input_mode_override;
  in_flags = viewconvert->input_flags_override;
  if (in_mode == GST_VIDEO_MULTIVIEW_MODE_NONE) {
    in_mode = GST_VIDEO_INFO_MULTIVIEW_MODE (&viewconvert->in_info);
    in_flags = GST_VIDEO_INFO_MULTIVIEW_FLAGS (&viewconvert->in_info);
  }

  /* Configured output mode already takes any override
   * into account */
  out_mode = GST_VIDEO_INFO_MULTIVIEW_MODE (&viewconvert->out_info);
  out_flags = GST_VIDEO_INFO_MULTIVIEW_FLAGS (&viewconvert->out_info);

  if (in_mode == GST_VIDEO_MULTIVIEW_MODE_FRAME_BY_FRAME) {
    /* For frame-by-frame, we need 2 input buffers */
    if (priv->auxilliary_in == NULL) {
      GST_LOG_OBJECT (viewconvert,
          "Can't generate output yet - frame-by-frame mode");
      goto done;
    }
  }

  /* Store the current conversion in the priv vars */
  priv->input_mode = in_mode;
  priv->input_flags = in_flags;
  priv->output_mode = out_mode;
  priv->output_flags = out_flags;

  if (priv->input_mode == priv->output_mode &&
      priv->input_flags == priv->output_flags &&
      viewconvert->in_info.width == viewconvert->out_info.width &&
      viewconvert->in_info.height == viewconvert->out_info.height &&
      viewconvert->from_texture_target == viewconvert->to_texture_target) {
    /* passthrough - just pass input buffers */
    outbuf = gst_buffer_ref (priv->primary_in);
    if (in_mode == GST_VIDEO_MULTIVIEW_MODE_FRAME_BY_FRAME)
      priv->auxilliary_out = gst_buffer_ref (priv->auxilliary_in);
    goto done_clear_input;
  }

  /* We can't output to OES textures, they're only supported for passthrough */
  if (viewconvert->to_texture_target == GST_GL_TEXTURE_TARGET_EXTERNAL_OES) {
    ret = GST_FLOW_ERROR;
    goto done_clear_input;
  }

  /* Generate new output buffer(s) */
  gst_gl_context_thread_add (viewconvert->context,
      (GstGLContextThreadFunc) _do_view_convert, viewconvert);

  if (!priv->result) {
    if (priv->primary_out)
      gst_object_unref (priv->primary_out);
    if (priv->auxilliary_out)
      gst_object_unref (priv->auxilliary_out);
    priv->primary_out = NULL;
    priv->auxilliary_out = NULL;
    ret = GST_FLOW_ERROR;
    goto done_clear_input;
  }

  outbuf = priv->primary_out;
  if (outbuf) {
    GstVideoOverlayCompositionMeta *composition_meta;

    gst_buffer_copy_into (outbuf, priv->primary_in,
        GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS, 0, -1);
    GST_BUFFER_FLAG_SET (outbuf,
        GST_VIDEO_BUFFER_FLAG_FIRST_IN_BUNDLE |
        GST_VIDEO_BUFFER_FLAG_MULTIPLE_VIEW);

    composition_meta =
        gst_buffer_get_video_overlay_composition_meta (priv->primary_in);
    if (composition_meta) {
      GST_DEBUG ("found video overlay composition meta, applying on output.");
      gst_buffer_add_video_overlay_composition_meta
          (outbuf, composition_meta->overlay);
    }
  }

  if (priv->auxilliary_out) {
    GstVideoOverlayCompositionMeta *composition_meta;

    gst_buffer_copy_into (priv->auxilliary_out,
        priv->primary_out, GST_BUFFER_COPY_FLAGS, 0, -1);
    GST_BUFFER_FLAG_UNSET (priv->auxilliary_out,
        GST_VIDEO_BUFFER_FLAG_FIRST_IN_BUNDLE);

    composition_meta =
        gst_buffer_get_video_overlay_composition_meta (priv->primary_out);
    if (composition_meta) {
      GST_DEBUG ("found video overlay composition meta, applying on output.");
      gst_buffer_add_video_overlay_composition_meta
          (priv->auxilliary_out, composition_meta->overlay);
    }
  }
  priv->primary_out = NULL;

done_clear_input:
  /* Invalidate input buffers now they've been used */
  gst_buffer_replace (&priv->primary_in, NULL);
  gst_buffer_replace (&priv->auxilliary_in, NULL);

done:
  GST_OBJECT_UNLOCK (viewconvert);
  *outbuf_ptr = outbuf;
  return ret;
}

#ifndef GST_REMOVE_DEPRECATED
#ifdef GST_DISABLE_DEPRECATED
GST_GL_API GType gst_gl_stereo_downmix_mode_get_type (void);
#endif

GType
gst_gl_stereo_downmix_mode_get_type (void)
{
  return gst_gl_stereo_downmix_get_type ();
}
#endif
