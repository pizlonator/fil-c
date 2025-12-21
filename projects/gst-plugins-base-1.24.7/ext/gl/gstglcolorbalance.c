/* GStreamer
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
/*
 * This file was modified from videobalance and converted to OpenGL
 */

/**
 * SECTION:element-glcolorbalance
 * @title: glcolorbalance
 *
 * Adjusts brightness, contrast, hue, saturation on a video stream.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 videotestsrc ! glupload ! glcolorbalance saturation=0.0 ! glcolorconvert ! gldownload ! ximagesink
 * ]| This pipeline converts the image to black and white by setting the
 * saturation to 0.0.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <gst/gl/gstglfuncs.h>
#include <gst/math-compat.h>
#include <gst/video/colorbalance.h>

#include "gstglelements.h"
#include "gstglcolorbalance.h"

GST_DEBUG_CATEGORY_STATIC (glcolorbalance_debug);
#define GST_CAT_DEFAULT glcolorbalance_debug

/* GstGLColorBalance properties */
#define DEFAULT_PROP_CONTRAST       1.0
#define DEFAULT_PROP_BRIGHTNESS	    0.0
#define DEFAULT_PROP_HUE            0.0
#define DEFAULT_PROP_SATURATION	    1.0

#define GST_GL_COLOR_BALANCE_VIDEO_CAPS \
    "video/x-raw(" GST_CAPS_FEATURE_MEMORY_GL_MEMORY "), "              \
    "format = (string) RGBA, "              \
    "width = " GST_VIDEO_SIZE_RANGE ", "                                \
    "height = " GST_VIDEO_SIZE_RANGE ", "                               \
    "framerate = " GST_VIDEO_FPS_RANGE ", "                             \
    "texture-target = (string) { 2D, external-oes } "        \
    " ; "                                                               \
    "video/x-raw(" GST_CAPS_FEATURE_MEMORY_GL_MEMORY ","                \
    GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION "), "           \
    "format = (string) RGBA, "              \
    "width = " GST_VIDEO_SIZE_RANGE ", "                                \
    "height = " GST_VIDEO_SIZE_RANGE ", "                               \
    "framerate = " GST_VIDEO_FPS_RANGE ", "                             \
    "texture-target = (string) { 2D, external-oes }"

#define GST_GL_COLOR_BALANCE_VIDEO_CAPS_FULL \
    GST_GL_COLOR_BALANCE_VIDEO_CAPS " ; "                               \
    GST_VIDEO_DMA_DRM_CAPS_MAKE

static GstStaticPadTemplate gst_gl_color_balance_element_src_pad_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_GL_COLOR_BALANCE_VIDEO_CAPS_FULL));

static GstStaticPadTemplate gst_gl_color_balance_element_sink_pad_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_GL_COLOR_BALANCE_VIDEO_CAPS_FULL));

/* *INDENT-OFF* */
static const gchar glsl_external_image_extension[] =
    "#extension GL_OES_EGL_image_external : require\n";

static const gchar glsl_external_image_sampler[] =
    "uniform samplerExternalOES tex;\n";

static const gchar glsl_2D_image_sampler[] =
  "uniform sampler2D tex;\n";

static const gchar color_balance_frag_templ[] =
  "uniform mat4 yuva_balance_matrix;\n"
  "uniform vec4 yuva_balance_constant;\n"
  "varying vec2 v_texcoord;\n"
  "#define from_yuv_bt601_offset vec4(-0.0625, -0.5, -0.5, 0.0)\n"
  "#define from_yuv_coeff_mat mat4(1.164, 0.000, 1.596, 0.0,  1.164,-0.391,-0.813, 0.0,  1.164, 2.018, 0.000, 0.0, 0.0,   0.0,   0.0,   1.0)\n"
  "void main () {\n"
  /* operations translated from videobalanceand tested with glvideomixer
   * with one pad's parameters blend-equation-rgb={subtract,reverse-subtract},
   * blend-function-src-rgb=src-color and blend-function-dst-rgb=dst-color */
  "  vec4 rgba = %s (tex, v_texcoord);\n" /* texture2D / texture2DOES */
  "  vec4 yuva = rgba * yuva_balance_matrix + yuva_balance_constant;\n"
  "  yuva = clamp(yuva, 0.0, 1.0);\n"
  "  gl_FragColor = yuva * from_yuv_coeff_mat + from_yuv_bt601_offset * from_yuv_coeff_mat;\n"
  "}\n";
/* *INDENT-ON* */

enum
{
  PROP_0,
  PROP_CONTRAST,
  PROP_BRIGHTNESS,
  PROP_HUE,
  PROP_SATURATION
};

static void gst_gl_color_balance_colorbalance_init (GstColorBalanceInterface *
    iface);

static void gst_gl_color_balance_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gl_color_balance_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

#define gst_gl_color_balance_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGLColorBalance, gst_gl_color_balance,
    GST_TYPE_GL_FILTER,
    G_IMPLEMENT_INTERFACE (GST_TYPE_COLOR_BALANCE,
        gst_gl_color_balance_colorbalance_init));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (glcolorbalance, "glcolorbalance",
    GST_RANK_NONE, GST_TYPE_GL_COLOR_BALANCE, gl_element_init (plugin));

static GstCaps *
gcb_transform_internal_caps (GstGLFilter * filter,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter_caps)
{
  static GstStaticCaps static_caps =
      GST_STATIC_CAPS (GST_GL_COLOR_BALANCE_VIDEO_CAPS);
  GstCaps *non_passthrough_caps = gst_static_caps_get (&static_caps);
  GstCaps *tmp;
  gint i;

  /* If we're not in passthrough mode, we can only output 2D textures,
   * but can always receive any compatible texture.
   * This function is not called in passthrough mode, so we can do the
   * transform unconditionally */

  non_passthrough_caps = gst_caps_make_writable (non_passthrough_caps);
  for (i = 0; i < gst_caps_get_size (non_passthrough_caps); i++) {
    GstStructure *outs = gst_caps_get_structure (non_passthrough_caps, i);
    gst_structure_remove_field (outs, "texture-target");
  }
  tmp = gst_caps_intersect_full (caps, non_passthrough_caps,
      GST_CAPS_INTERSECT_FIRST);
  gst_caps_unref (non_passthrough_caps);

  for (i = 0; i < gst_caps_get_size (tmp); i++) {
    GstStructure *outs = gst_caps_get_structure (tmp, i);
    if (direction == GST_PAD_SINK) {
      gst_structure_set (outs, "texture-target", G_TYPE_STRING,
          gst_gl_texture_target_to_string (GST_GL_TEXTURE_TARGET_2D), NULL);
    } else {
      gst_structure_remove_field (outs, "texture-target");
    }
  }
  return tmp;
}

static gboolean
gst_gl_color_balance_is_passthrough (GstGLColorBalance * glcolorbalance)
{
  return glcolorbalance->contrast == 1.0 &&
      glcolorbalance->brightness == 0.0 &&
      glcolorbalance->hue == 0.0 && glcolorbalance->saturation == 1.0;
}

static void
_update_yua_uniforms (GstGLColorBalance * glcolorbalance)
{
  gdouble brightness = glcolorbalance->brightness;
  gdouble contrast = glcolorbalance->contrast;
  gdouble hue = glcolorbalance->hue;
  gdouble saturation = glcolorbalance->saturation;
  gdouble s_hue_cos = saturation * cos (hue * G_PI);
  gdouble s_hue_sin = saturation * sin (hue * G_PI);

  // We reduce the color balance adjustment of each pixel to:
  // yuva_to_rgb(clamp(rgba * yuva_balance_matrix + yuva_balance_constant))
  // Where yuva_balance_matrix and yuva_balance_constant are obtained by
  // combining following steps:
  //
  // yuva = rgba * from_rgb_coeff_mat + from_rgb_bt601_offset
  // yuva = yuva * contrast_matrix + contrast_brightness_constant
  // yuva = (yuva - vec4(0, 0.5, 0.5, 0.0)) * hue_saturation_matrix + vec4(0, 0.5, 0.5, 0)
  //
  // Where,
  // from_rgb_coeff_mat = mat4(0.256816, 0.504154, 0.0979137, 0,
  //                          -0.148246,-0.29102,  0.439266,  0,
  //                           0.439271,-0.367833,-0.071438,  0
  //                           0,        0,        0,         1)
  // from_rgb_bt601_offset = vec4(0.0625, 0.5, 0.5, 0)
  //
  // contrast_matrix and contrast_brightness_constant represent the operation:
  // yuva.x = luma_to_narrow(luma_to_full(yuva.x)*contrast) + brightness
  //
  // If luma_to_full(x) = x * 256.0 / 219.0 - 16.0 / 256.0
  // and luma_to_narrow(x) = luma * 219.0/256.0 + 16.0 * 219.0 / 256.0 / 256.0
  // then luma_to_narrow(luma_to_full(x)*contrast) + brightness
  // = x * contrast + contrast * ((16.0 * 219.0 / 256.0 / 256.0) / (219.0 / 256.0)) + brightness - (16.0 / 256.0)
  //
  // Then contrast_matrix = mat4(contrast, 0, 0, 0,
  //                             0,        1, 0, 0
  //                             0,        0, 1, 0
  //                             0,        0, 0, 1)
  //
  // contrast_constant = vec4(contrast * ((16.0 * 219.0 / 256.0 / 256.0) / (219.0 / 256.0))
  //    + brightness - (16.0 / 256.0), 0, 0, 0)
  //
  // hue_saturation_matrix is obtained by reducing the following steps:
  // yuv.y = 0.5 + (((uv.x - 0.5) * hue_cos + (uv.y - 0.5) * hue_sin) * saturation);
  // yuv.z = 0.5 + (((0.5 - uv.x) * hue_sin + (uv.y - 0.5) * hue_cos) * saturation);
  //
  // as yuv.yz = vec2(0.5) +
  //        (yuv.yz - vec2(0.5)) * mat2(hue_cos * saturation, hue_sin * saturation,
  //                                   -hue_sin * saturation, hue_cos * saturation)
  // =>
  // (1, 0,                                  0,                                  0,
  //  0, saturation * Math.cos(Math.PI*hue),-saturation * Math.sin(Math.PI*hue), 0,
  //  0, saturation * Math.sin(Math.PI*hue), saturation * Math.cos(Math.PI*hue), 0,
  //  0, 0,                                  0,                                  1)
  gfloat *m = glcolorbalance->yuva_balance_matrix;

  // Column 0
  *m++ = 0.256816 * contrast;
  *m++ = 0.504154 * contrast;
  *m++ = 0.0979137 * contrast;
  *m++ = 0;

  // Column 1
  *m++ = -0.148246 * s_hue_cos + 0.439271 * s_hue_sin;
  *m++ = -0.29102 * s_hue_cos - 0.367833 * s_hue_sin;
  *m++ = 0.439266 * s_hue_cos - 0.071438 * s_hue_sin;
  *m++ = 0;

  // Column 2
  *m++ = 0.148246 * s_hue_sin + 0.439271 * s_hue_cos;
  *m++ = 0.29102 * s_hue_sin - 0.367833 * s_hue_cos;
  *m++ = -0.439266 * s_hue_sin - 0.071438 * s_hue_cos;
  *m++ = 0;

  // Column 3
  *m++ = 0;
  *m++ = 0;
  *m++ = 0;
  *m++ = 1;

  glcolorbalance->yuva_balance_constant[0] =
      0.0625 * contrast +
      contrast * ((16.0 * 219.0 / 256.0 / 256.0) / (219.0 / 256.0))
      + brightness - (16.0 / 256.0);
  glcolorbalance->yuva_balance_constant[1] = 0.5;
  glcolorbalance->yuva_balance_constant[2] = 0.5;
  glcolorbalance->yuva_balance_constant[3] = 0;
}

static void
gst_gl_color_balance_update_properties (GstGLColorBalance * glcolorbalance)
{
  gboolean current_passthrough, passthrough;
  GstBaseTransform *base = GST_BASE_TRANSFORM (glcolorbalance);

  GST_OBJECT_LOCK (glcolorbalance);
  passthrough = gst_gl_color_balance_is_passthrough (glcolorbalance);
  _update_yua_uniforms (glcolorbalance);
  GST_OBJECT_UNLOCK (glcolorbalance);
  current_passthrough = gst_base_transform_is_passthrough (base);

  gst_base_transform_set_passthrough (base, passthrough);
  if (current_passthrough != passthrough)
    gst_base_transform_reconfigure_src (base);
}

static gboolean
_create_shader (GstGLColorBalance * balance)
{
  GstGLBaseFilter *base_filter = GST_GL_BASE_FILTER (balance);
  GstGLFilter *filter = GST_GL_FILTER (balance);
  GError *error = NULL;
  gchar *frag_body;
  const gchar *frags[4];
  guint frag_i = 0;

  if (balance->shader)
    gst_clear_object (&balance->shader);

  if (filter->in_texture_target == GST_GL_TEXTURE_TARGET_EXTERNAL_OES)
    frags[frag_i++] = glsl_external_image_extension;

  frags[frag_i++] =
      gst_gl_shader_string_get_highest_precision (base_filter->context,
      GST_GLSL_VERSION_NONE,
      GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY);

  switch (filter->in_texture_target) {
    case GST_GL_TEXTURE_TARGET_2D:
      frags[frag_i++] = glsl_2D_image_sampler;
      break;
    case GST_GL_TEXTURE_TARGET_EXTERNAL_OES:
      frags[frag_i++] = glsl_external_image_sampler;
      break;
    default:
      GST_ERROR_OBJECT (balance, "Unsupported GstGLTextureTarget value: %d",
          filter->in_texture_target);
      return FALSE;
  }
  frags[frag_i++] = frag_body =
      g_strdup_printf (color_balance_frag_templ, "texture2D");

  g_assert (frag_i <= G_N_ELEMENTS (frags));

  if (!(balance->shader =
          gst_gl_shader_new_link_with_stages (base_filter->context, &error,
              gst_glsl_stage_new_default_vertex (base_filter->context),
              gst_glsl_stage_new_with_strings (base_filter->context,
                  GL_FRAGMENT_SHADER, GST_GLSL_VERSION_NONE,
                  GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY, frag_i,
                  frags), NULL))) {
    g_free (frag_body);
    GST_ELEMENT_ERROR (balance, RESOURCE, NOT_FOUND, ("%s",
            "Failed to initialize colorbalance shader"), ("%s",
            error ? error->message : "Unknown error"));
    return FALSE;
  }
  g_free (frag_body);

  filter->draw_attr_position_loc =
      gst_gl_shader_get_attribute_location (balance->shader, "a_position");
  filter->draw_attr_texture_loc =
      gst_gl_shader_get_attribute_location (balance->shader, "a_texcoord");

  return TRUE;
}

static gboolean
gst_gl_color_balance_gl_start (GstGLBaseFilter * base_filter)
{
  GstGLColorBalance *balance = GST_GL_COLOR_BALANCE (base_filter);

  if (!_create_shader (balance))
    return FALSE;

  return GST_GL_BASE_FILTER_CLASS (parent_class)->gl_start (base_filter);
}

static void
gst_gl_color_balance_gl_stop (GstGLBaseFilter * base_filter)
{
  GstGLColorBalance *balance = GST_GL_COLOR_BALANCE (base_filter);

  if (balance->shader)
    gst_object_unref (balance->shader);
  balance->shader = NULL;

  GST_GL_BASE_FILTER_CLASS (parent_class)->gl_stop (base_filter);
}

static void
gst_gl_color_balance_before_transform (GstBaseTransform * base, GstBuffer * buf)
{
  GstGLColorBalance *balance = GST_GL_COLOR_BALANCE (base);
  GstClockTime timestamp, stream_time;

  timestamp = GST_BUFFER_TIMESTAMP (buf);
  stream_time =
      gst_segment_to_stream_time (&base->segment, GST_FORMAT_TIME, timestamp);

  GST_DEBUG_OBJECT (balance, "sync to %" GST_TIME_FORMAT,
      GST_TIME_ARGS (timestamp));

  if (GST_CLOCK_TIME_IS_VALID (stream_time))
    gst_object_sync_values (GST_OBJECT (balance), stream_time);
}

static gboolean
gst_gl_color_balance_filter_texture (GstGLFilter * filter, GstGLMemory * in_tex,
    GstGLMemory * out_tex)
{
  GstGLColorBalance *balance = GST_GL_COLOR_BALANCE (filter);

  if (!balance->shader)
    _create_shader (balance);

  gst_gl_shader_use (balance->shader);
  GST_OBJECT_LOCK (balance);

  gst_gl_shader_set_uniform_matrix_4fv (balance->shader, "yuva_balance_matrix",
      1, GL_FALSE, balance->yuva_balance_matrix);
  gst_gl_shader_set_uniform_4fv (balance->shader, "yuva_balance_constant", 1,
      balance->yuva_balance_constant);

  GST_OBJECT_UNLOCK (balance);

  gst_gl_filter_render_to_target_with_shader (filter, in_tex, out_tex,
      balance->shader);

  return TRUE;
}

static void
gst_gl_color_balance_finalize (GObject * object)
{
  GstGLColorBalance *balance = GST_GL_COLOR_BALANCE (object);
  GList *channels = NULL;

  channels = balance->channels;
  while (channels) {
    GstColorBalanceChannel *channel = channels->data;

    g_object_unref (channel);
    channels->data = NULL;
    channels = g_list_next (channels);
  }

  if (balance->channels)
    g_list_free (balance->channels);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_gl_color_balance_class_init (GstGLColorBalanceClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseTransformClass *trans_class = (GstBaseTransformClass *) klass;
  GstGLBaseFilterClass *base_filter_class = (GstGLBaseFilterClass *) klass;
  GstGLFilterClass *filter_class = (GstGLFilterClass *) klass;

  GST_DEBUG_CATEGORY_INIT (glcolorbalance_debug, "glcolorbalance", 0,
      "glcolorbalance");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_gl_color_balance_element_src_pad_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_gl_color_balance_element_sink_pad_template);

  gobject_class->finalize = gst_gl_color_balance_finalize;
  gobject_class->set_property = gst_gl_color_balance_set_property;
  gobject_class->get_property = gst_gl_color_balance_get_property;

  g_object_class_install_property (gobject_class, PROP_CONTRAST,
      g_param_spec_double ("contrast", "Contrast", "contrast",
          0.0, 2.0, DEFAULT_PROP_CONTRAST,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_BRIGHTNESS,
      g_param_spec_double ("brightness", "Brightness", "brightness", -1.0, 1.0,
          DEFAULT_PROP_BRIGHTNESS,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_HUE,
      g_param_spec_double ("hue", "Hue", "hue", -1.0, 1.0, DEFAULT_PROP_HUE,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SATURATION,
      g_param_spec_double ("saturation", "Saturation", "saturation", 0.0, 2.0,
          DEFAULT_PROP_SATURATION,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (gstelement_class, "Video balance",
      "Filter/Effect/Video",
      "Adjusts brightness, contrast, hue, saturation on a video stream",
      "Matthew Waters <matthew@centricular.com>");

  trans_class->before_transform =
      GST_DEBUG_FUNCPTR (gst_gl_color_balance_before_transform);
  trans_class->transform_ip_on_passthrough = FALSE;

  base_filter_class->gl_start =
      GST_DEBUG_FUNCPTR (gst_gl_color_balance_gl_start);
  base_filter_class->gl_stop = GST_DEBUG_FUNCPTR (gst_gl_color_balance_gl_stop);

  filter_class->filter_texture =
      GST_DEBUG_FUNCPTR (gst_gl_color_balance_filter_texture);
  filter_class->transform_internal_caps = gcb_transform_internal_caps;
}

static void
gst_gl_color_balance_init (GstGLColorBalance * glcolorbalance)
{
  const gchar *channels[4] = { "HUE", "SATURATION",
    "BRIGHTNESS", "CONTRAST"
  };
  gint i;

  /* Initialize propertiews */
  glcolorbalance->contrast = DEFAULT_PROP_CONTRAST;
  glcolorbalance->brightness = DEFAULT_PROP_BRIGHTNESS;
  glcolorbalance->hue = DEFAULT_PROP_HUE;
  glcolorbalance->saturation = DEFAULT_PROP_SATURATION;

  gst_gl_color_balance_update_properties (glcolorbalance);

  /* Generate the channels list */
  for (i = 0; i < G_N_ELEMENTS (channels); i++) {
    GstColorBalanceChannel *channel;

    channel = g_object_new (GST_TYPE_COLOR_BALANCE_CHANNEL, NULL);
    channel->label = g_strdup (channels[i]);
    channel->min_value = -1000;
    channel->max_value = 1000;

    glcolorbalance->channels =
        g_list_append (glcolorbalance->channels, channel);
  }
}

static const GList *
gst_gl_color_balance_colorbalance_list_channels (GstColorBalance * balance)
{
  GstGLColorBalance *glcolorbalance = GST_GL_COLOR_BALANCE (balance);

  g_return_val_if_fail (glcolorbalance != NULL, NULL);
  g_return_val_if_fail (GST_IS_GL_COLOR_BALANCE (glcolorbalance), NULL);

  return glcolorbalance->channels;
}

static void
gst_gl_color_balance_colorbalance_set_value (GstColorBalance * balance,
    GstColorBalanceChannel * channel, gint value)
{
  GstGLColorBalance *vb = GST_GL_COLOR_BALANCE (balance);
  gdouble new_val;
  gboolean changed = FALSE;

  g_return_if_fail (vb != NULL);
  g_return_if_fail (GST_IS_GL_COLOR_BALANCE (vb));
  g_return_if_fail (channel->label != NULL);

  GST_OBJECT_LOCK (vb);
  if (!g_ascii_strcasecmp (channel->label, "HUE")) {
    new_val = (value + 1000.0) * 2.0 / 2000.0 - 1.0;
    changed = new_val != vb->hue;
    vb->hue = new_val;
  } else if (!g_ascii_strcasecmp (channel->label, "SATURATION")) {
    new_val = (value + 1000.0) * 2.0 / 2000.0;
    changed = new_val != vb->saturation;
    vb->saturation = new_val;
  } else if (!g_ascii_strcasecmp (channel->label, "BRIGHTNESS")) {
    new_val = (value + 1000.0) * 2.0 / 2000.0 - 1.0;
    changed = new_val != vb->brightness;
    vb->brightness = new_val;
  } else if (!g_ascii_strcasecmp (channel->label, "CONTRAST")) {
    new_val = (value + 1000.0) * 2.0 / 2000.0;
    changed = new_val != vb->contrast;
    vb->contrast = new_val;
  }
  GST_OBJECT_UNLOCK (vb);

  if (changed)
    gst_gl_color_balance_update_properties (vb);

  if (changed) {
    gst_color_balance_value_changed (balance, channel,
        gst_color_balance_get_value (balance, channel));
  }
}

static gint
gst_gl_color_balance_colorbalance_get_value (GstColorBalance * balance,
    GstColorBalanceChannel * channel)
{
  GstGLColorBalance *vb = GST_GL_COLOR_BALANCE (balance);
  gint value = 0;

  g_return_val_if_fail (vb != NULL, 0);
  g_return_val_if_fail (GST_IS_GL_COLOR_BALANCE (vb), 0);
  g_return_val_if_fail (channel->label != NULL, 0);

  if (!g_ascii_strcasecmp (channel->label, "HUE")) {
    value = (vb->hue + 1) * 2000.0 / 2.0 - 1000.0;
  } else if (!g_ascii_strcasecmp (channel->label, "SATURATION")) {
    value = vb->saturation * 2000.0 / 2.0 - 1000.0;
  } else if (!g_ascii_strcasecmp (channel->label, "BRIGHTNESS")) {
    value = (vb->brightness + 1) * 2000.0 / 2.0 - 1000.0;
  } else if (!g_ascii_strcasecmp (channel->label, "CONTRAST")) {
    value = vb->contrast * 2000.0 / 2.0 - 1000.0;
  }

  return value;
}

static GstColorBalanceType
gst_gl_color_balance_colorbalance_get_balance_type (GstColorBalance * balance)
{
  return GST_COLOR_BALANCE_HARDWARE;
}

static void
gst_gl_color_balance_colorbalance_init (GstColorBalanceInterface * iface)
{
  iface->list_channels = gst_gl_color_balance_colorbalance_list_channels;
  iface->set_value = gst_gl_color_balance_colorbalance_set_value;
  iface->get_value = gst_gl_color_balance_colorbalance_get_value;
  iface->get_balance_type = gst_gl_color_balance_colorbalance_get_balance_type;
}

static GstColorBalanceChannel *
gst_gl_color_balance_find_channel (GstGLColorBalance * balance,
    const gchar * label)
{
  GList *l;

  for (l = balance->channels; l; l = l->next) {
    GstColorBalanceChannel *channel = l->data;

    if (g_ascii_strcasecmp (channel->label, label) == 0)
      return channel;
  }
  return NULL;
}

static void
gst_gl_color_balance_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGLColorBalance *balance = GST_GL_COLOR_BALANCE (object);
  gdouble d;
  const gchar *label = NULL;

  GST_OBJECT_LOCK (balance);
  switch (prop_id) {
    case PROP_CONTRAST:
      d = g_value_get_double (value);
      GST_DEBUG_OBJECT (balance, "Changing contrast from %lf to %lf",
          balance->contrast, d);
      if (d != balance->contrast)
        label = "CONTRAST";
      balance->contrast = d;
      break;
    case PROP_BRIGHTNESS:
      d = g_value_get_double (value);
      GST_DEBUG_OBJECT (balance, "Changing brightness from %lf to %lf",
          balance->brightness, d);
      if (d != balance->brightness)
        label = "BRIGHTNESS";
      balance->brightness = d;
      break;
    case PROP_HUE:
      d = g_value_get_double (value);
      GST_DEBUG_OBJECT (balance, "Changing hue from %lf to %lf", balance->hue,
          d);
      if (d != balance->hue)
        label = "HUE";
      balance->hue = d;
      break;
    case PROP_SATURATION:
      d = g_value_get_double (value);
      GST_DEBUG_OBJECT (balance, "Changing saturation from %lf to %lf",
          balance->saturation, d);
      if (d != balance->saturation)
        label = "SATURATION";
      balance->saturation = d;
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK (balance);
  gst_gl_color_balance_update_properties (balance);

  if (label) {
    GstColorBalanceChannel *channel =
        gst_gl_color_balance_find_channel (balance, label);
    gst_color_balance_value_changed (GST_COLOR_BALANCE (balance), channel,
        gst_color_balance_get_value (GST_COLOR_BALANCE (balance), channel));
  }
}

static void
gst_gl_color_balance_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGLColorBalance *balance = GST_GL_COLOR_BALANCE (object);

  switch (prop_id) {
    case PROP_CONTRAST:
      g_value_set_double (value, balance->contrast);
      break;
    case PROP_BRIGHTNESS:
      g_value_set_double (value, balance->brightness);
      break;
    case PROP_HUE:
      g_value_set_double (value, balance->hue);
      break;
    case PROP_SATURATION:
      g_value_set_double (value, balance->saturation);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
