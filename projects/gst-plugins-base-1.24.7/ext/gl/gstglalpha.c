/* GStreamer
 * Copyright (C) 2018 Matthew Waters <matthew@centricular.com>
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
 * This file was modified from the alpha element and converted to OpenGL
 */

/**
 * SECTION:element-glalpha
 * @title: glalpha
 *
 * The glalpha element adds an alpha channel to a video stream. The values
 * of the alpha channel can be either be set to a constant or can be
 * dynamically calculated via chroma keying, e.g. blue can be set as
 * the transparent color.
 *
 * Sample pipeline:
 * |[
 * gst-launch-1.0 gltestsrc pattern=snow ! mixer.sink_0 \
 *   gltestsrc pattern=smpte ! glalpha method=green ! mixer.sink_1 \
 *   glvideomixer name=mixer sink_0::zorder=0 sink_1::zorder=1 ! \
 *   glimagesink
 * ]| This pipeline adds a alpha channel to the SMPTE color bars
 * with green as the transparent color and overlays the output on
 * top of a snow video stream.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstglelements.h"
#include "gstglalpha.h"
#include <string.h>
#include <math.h>
#include <gst/gl/gstglfuncs.h>

#ifndef M_PI
#define M_PI  3.14159265358979323846
#endif

GST_DEBUG_CATEGORY_STATIC (glalpha_debug);
#define GST_CAT_DEFAULT glalpha_debug

#define GST_TYPE_GL_ALPHA_METHOD (gst_gl_alpha_method_get_type())
static GType
gst_gl_alpha_method_get_type (void)
{
  static GType alpha_method_type = 0;
  static const GEnumValue alpha_method[] = {
    {ALPHA_METHOD_SET, "Set/adjust alpha channel", "set"},
    {ALPHA_METHOD_GREEN, "Chroma Key on pure green", "green"},
    {ALPHA_METHOD_BLUE, "Chroma Key on pure blue", "blue"},
    {ALPHA_METHOD_CUSTOM, "Chroma Key on custom RGB values", "custom"},
    {0, NULL, NULL},
  };

  if (!alpha_method_type) {
    alpha_method_type =
        g_enum_register_static ("GstGLAlphaMethod", alpha_method);
  }
  return alpha_method_type;
}

/* GstGLAlpha properties */
#define DEFAULT_METHOD ALPHA_METHOD_SET
#define DEFAULT_ALPHA 1.0
#define DEFAULT_TARGET_R 0
#define DEFAULT_TARGET_G 255
#define DEFAULT_TARGET_B 0
#define DEFAULT_ANGLE 20.0
#define DEFAULT_NOISE_LEVEL 2.0
#define DEFAULT_BLACK_SENSITIVITY 100
#define DEFAULT_WHITE_SENSITIVITY 100

enum
{
  PROP_0,
  PROP_METHOD,
  PROP_ALPHA,
  PROP_TARGET_R,
  PROP_TARGET_G,
  PROP_TARGET_B,
  PROP_ANGLE,
  PROP_NOISE_LEVEL,
  PROP_BLACK_SENSITIVITY,
  PROP_WHITE_SENSITIVITY,
};

/* *INDENT-OFF* */
static const gchar *alpha_frag =
  "varying vec2 v_texcoord;\n"
  "uniform sampler2D tex;\n"
  "uniform float alpha;\n"
  "void main () {\n"
  "  vec4 rgba = texture2D (tex, v_texcoord);\n"
  "  rgba.a = alpha;\n"
  "  gl_FragColor = rgba;\n"
  "}\n";

static const gchar *chroma_key_frag =
  "varying vec2 v_texcoord;\n"
  "uniform sampler2D tex;\n"
  "uniform float cb;\n"
  "uniform float cr;\n"
  "uniform float kg;\n"
  "uniform float accept_angle_tg;\n"
  "uniform float accept_angle_ctg;\n"
  "uniform float one_over_kc;\n"
  "uniform float kfgy_scale;\n"
  "uniform float noise_level2;\n"
  "uniform float smin;\n"
  "uniform float smax;\n"
  /* these values are taken from the alpha element and divided by 256 to
   * get the floating point numbers below.
   * XXX: They are different from the values produced by videoconvert and used
   * by glcolorconvert
  298, 0, 409, -57068,
  298, -100, -208, 34707,
  298, 516, 0, -70870,
  */
  "#define from_yuv_bt601_offset vec3(-0.0625, -0.5, -0.5)\n"
  "#define from_yuv_bt601_rcoeff vec3(1.1640625, 0.000, 1.787)\n"
  "#define from_yuv_bt601_gcoeff vec3(1.1640625,-0.213,-0.531)\n"
  "#define from_yuv_bt601_bcoeff vec3(1.1640625, 2.112, 0.000)\n"
  /* these values are taken from the alpha element and divided by 256 to
   * get the floating point numbers below
   * XXX: They are different from the values produced by videoconvert and used
   * by glcolorconvert
  66, 129, 25, 4096,
  -38, -74, 112, 32768,
  112, -94, -18, 32768,
  */
  "#define from_rgb_bt601_offset vec3(0.0625, 0.5, 0.5)\n"
  "#define from_rgb_bt601_ycoeff vec3( 0.2578125, 0.50390625, 0.09765625)\n"
  "#define from_rgb_bt601_ucoeff vec3(-0.1484375,-0.28906250, 0.43750000)\n"
  "#define from_rgb_bt601_vcoeff vec3( 0.4375000,-0.36718750,-0.07031250)\n"
  "#define PI 3.14159265\n"
  "\n"
  "vec3 yuv_to_rgb (vec3 val) {\n"
  "  vec3 rgb;\n"
  "  val += from_yuv_bt601_offset;\n"
  "  rgb.r = dot(val, from_yuv_bt601_rcoeff);\n"
  "  rgb.g = dot(val, from_yuv_bt601_gcoeff);\n"
  "  rgb.b = dot(val, from_yuv_bt601_bcoeff);\n"
  "  return rgb;\n"
  "}\n"
  "vec3 rgb_to_yuv (vec3 val) {\n"
  "  vec3 yuv;\n"
  "  yuv.r = dot(val.rgb, from_rgb_bt601_ycoeff);\n"
  "  yuv.g = dot(val.rgb, from_rgb_bt601_ucoeff);\n"
  "  yuv.b = dot(val.rgb, from_rgb_bt601_vcoeff);\n"
  "  yuv += from_rgb_bt601_offset;\n"
  "  return yuv;\n"
  "}\n"
  "vec4 chroma_keying_yuv (vec4 yuva) {\n"
  "  float y, u, v;\n"
  "  y = yuva.x;\n"
  "  u = yuva.y-0.5;\n"
  "  v = yuva.z-0.5;\n"
  "  if (y < smin || y > smax) {\n"
  "    return yuva;\n"
  "  }\n"
  "  vec4 new;\n"
  "  float tmp, tmp1;\n"
  "  float x = clamp ((u * cb + v * cr) * 2.0, -0.5, 0.5);\n"
  "  float z = clamp ((v * cb - u * cr) * 2.0, -0.5, 0.5);\n"
  "  if (abs(z) > min (0.5, x * accept_angle_tg * 0.0625)) {\n"
  "    return yuva;\n"
  "  }\n"
  "  float x1 = min (0.5, abs(z * accept_angle_ctg * 0.0625));\n"
  "  float y1 = z;\n"
  "  tmp1 = max (0.0, x-x1);\n"
  "  float b_alpha = yuva.a * (1.0 - clamp (tmp1 * one_over_kc, 0.0, 1.0));\n"
  "  tmp = min (1.0, tmp1 * kfgy_scale * 0.0625);\n"
  "  new.x = max (y-tmp, 0.0);//y < tmp ? 0.0 : y - tmp;\n"
  "  new.y = clamp ((x1 * cb - y1 * cr) * 2.0, -0.5, 0.5)+0.5;\n"
  "  new.z = clamp ((y1 * cb + x1 * cr) * 2.0, -0.5, 0.5)+0.5;\n"
  "  new.a = z * z + (x - kg) * (x - kg) < noise_level2 ? 0.0 : b_alpha;\n"
  "  return new;\n"
  "}\n"
  "void main () {\n"
  "  vec4 yuva;\n"
  /* operations translated from alpha and tested with glvideomixer
   * with one pad's parameters blend-equation-rgb={subtract,reverse-subtract},
   * blend-function-src-rgb=src-color and blend-function-dst-rgb=dst-color */
  "  vec4 rgba = texture2D (tex, v_texcoord);\n"
  "  yuva.xyz = rgb_to_yuv (rgba.rgb);\n"
  "  yuva.a = rgba.a;\n"
  "  yuva = chroma_keying_yuv (yuva);\n"
  "  rgba.rgb = yuv_to_rgb (yuva.xyz);\n"
  "  rgba.a = yuva.a;\n"
  "  gl_FragColor = rgba;\n"
  "}\n";
/* *INDENT-ON* */

static const gfloat cog_rgb_to_ycbcr_matrix_8bit_sdtv[] = {
  0.2578125, 0.50390625, 0.09765625, 0.0625f,
  -0.1484375, -0.28906250, 0.43750000, 0.5f,
  0.4375000, -0.36718750, -0.07031250, 0.5f
};

static void gst_gl_alpha_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gl_alpha_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

#define gst_gl_alpha_parent_class parent_class
G_DEFINE_TYPE (GstGLAlpha, gst_gl_alpha, GST_TYPE_GL_FILTER);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (glalpha, "glalpha",
    GST_RANK_NONE, GST_TYPE_GL_ALPHA, gl_element_init (plugin));

static gboolean
gst_gl_alpha_is_passthrough (GstGLAlpha * glalpha)
{
  return glalpha->method == ALPHA_METHOD_SET && glalpha->alpha == 1.0;
}

/* move v to be inside [from, to] by some multiple of (to-from) */
static float
wrap (float v, float from, float to)
{
  float diff;

  if (from > to) {
    float t = to;
    to = from;
    from = t;
  }

  if (!isfinite (to) || !isfinite (from) || !isfinite (v))
    return v;

  diff = to - from;

  while (v < from)
    v += diff;
  while (v > to)
    v -= diff;

  return v;
}

static void
gst_gl_alpha_update_properties (GstGLAlpha * glalpha)
{
  GstBaseTransform *base = GST_BASE_TRANSFORM (glalpha);
  gboolean current_passthrough, passthrough;
  gfloat kgl;
  gfloat tmp;
  gfloat target_r, target_g, target_b;
  gfloat target_y, target_u, target_v;
  const float *matrix = cog_rgb_to_ycbcr_matrix_8bit_sdtv;

  GST_OBJECT_LOCK (glalpha);
  switch (glalpha->method) {
    case ALPHA_METHOD_GREEN:
      target_r = 0.0;
      target_g = 1.0;
      target_b = 0.0;
      break;
    case ALPHA_METHOD_BLUE:
      target_r = 0.0;
      target_g = 0.0;
      target_b = 1.0;
      break;
    default:
      target_r = (gfloat) glalpha->target_r / 255.0;
      target_g = (gfloat) glalpha->target_g / 255.0;
      target_b = (gfloat) glalpha->target_b / 255.0;
      break;
  }

  target_y =
      matrix[0] * target_r + matrix[1] * target_g + matrix[2] * target_b +
      matrix[3];
  /* Cb,Cr without offset here because the chroma keying
   * works with them being in range [-128,127]
   */
  target_u = matrix[4] * target_r + matrix[5] * target_g + matrix[6] * target_b;
  target_v =
      matrix[8] * target_r + matrix[9] * target_g + matrix[10] * target_b;

  tmp = target_u * target_u + target_v * target_v;
  kgl = sqrt (tmp);
  glalpha->cb = target_u / kgl * 0.5;
  glalpha->cr = target_v / kgl * 0.5;

  tmp = 15 * tan (M_PI * glalpha->angle / 180);
  tmp = MIN (tmp, 255);
  glalpha->accept_angle_tg = tmp;
  tmp = 15 / tan (M_PI * glalpha->angle / 180);
  tmp = MIN (tmp, 255);
  glalpha->accept_angle_ctg = tmp;
  glalpha->one_over_kc = wrap (2 / kgl - 255, 0, 256);
  tmp = 15 * target_y / kgl;
  tmp = MIN (tmp, 255);
  glalpha->kfgy_scale = tmp;
  glalpha->kg = MIN (kgl, 0.5);

  glalpha->noise_level2 =
      glalpha->noise_level / 256.0 * glalpha->noise_level / 256.0;

  GST_INFO_OBJECT (glalpha, "target yuv: %f, %f, %f, "
      "kgl: %f, cb: %f, cr: %f, accept_angle_tg: %f, accept_angle_ctg: %f, "
      "one_over_kc: %f, kgfy_scale: %f, kg: %f, noise level: %f",
      (float) target_y, (float) target_u, (float) target_v, (float) kgl,
      (float) glalpha->cb, (float) glalpha->cr,
      (float) glalpha->accept_angle_tg, (float) glalpha->accept_angle_ctg,
      (float) glalpha->one_over_kc, (float) glalpha->kfgy_scale,
      (float) glalpha->kg, (float) glalpha->noise_level2);

  passthrough = gst_gl_alpha_is_passthrough (glalpha);
  GST_OBJECT_UNLOCK (glalpha);
  current_passthrough = gst_base_transform_is_passthrough (base);

  gst_base_transform_set_passthrough (base, passthrough);
  if (current_passthrough != passthrough)
    gst_base_transform_reconfigure_src (base);
}

static gboolean
_create_shader (GstGLAlpha * alpha)
{
  GstGLBaseFilter *base_filter = GST_GL_BASE_FILTER (alpha);
  GstGLFilter *filter = GST_GL_FILTER (alpha);
  GError *error = NULL;
  const gchar *frags[2];

  if (alpha->alpha_shader)
    gst_object_unref (alpha->alpha_shader);

  frags[0] =
      gst_gl_shader_string_get_highest_precision (base_filter->context,
      GST_GLSL_VERSION_NONE,
      GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY);
  frags[1] = alpha_frag;

  if (!(alpha->alpha_shader =
          gst_gl_shader_new_link_with_stages (base_filter->context, &error,
              gst_glsl_stage_new_default_vertex (base_filter->context),
              gst_glsl_stage_new_with_strings (base_filter->context,
                  GL_FRAGMENT_SHADER, GST_GLSL_VERSION_NONE,
                  GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY, 2,
                  frags), NULL))) {
    GST_ELEMENT_ERROR (alpha, RESOURCE, NOT_FOUND, ("%s",
            "Failed to initialize alpha shader"), ("%s",
            error ? error->message : "Unknown error"));
    return FALSE;
  }

  if (alpha->chroma_key_shader)
    gst_object_unref (alpha->chroma_key_shader);

  frags[1] = chroma_key_frag;

  if (!(alpha->chroma_key_shader =
          gst_gl_shader_new_link_with_stages (base_filter->context, &error,
              gst_glsl_stage_new_default_vertex (base_filter->context),
              gst_glsl_stage_new_with_strings (base_filter->context,
                  GL_FRAGMENT_SHADER, GST_GLSL_VERSION_NONE,
                  GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY, 2,
                  frags), NULL))) {
    GST_ELEMENT_ERROR (alpha, RESOURCE, NOT_FOUND, ("%s",
            "Failed to initialize chroma key shader"), ("%s",
            error ? error->message : "Unknown error"));
    return FALSE;
  }

  filter->draw_attr_position_loc =
      gst_gl_shader_get_attribute_location (alpha->alpha_shader, "a_position");
  filter->draw_attr_texture_loc =
      gst_gl_shader_get_attribute_location (alpha->alpha_shader, "a_texcoord");

  return TRUE;
}

static gboolean
gst_gl_alpha_gl_start (GstGLBaseFilter * base_filter)
{
  GstGLAlpha *alpha = GST_GL_ALPHA (base_filter);

  if (!_create_shader (alpha))
    return FALSE;

  return GST_GL_BASE_FILTER_CLASS (parent_class)->gl_start (base_filter);
}

static void
gst_gl_alpha_gl_stop (GstGLBaseFilter * base_filter)
{
  GstGLAlpha *alpha = GST_GL_ALPHA (base_filter);

  if (alpha->alpha_shader)
    gst_object_unref (alpha->alpha_shader);
  alpha->alpha_shader = NULL;

  if (alpha->chroma_key_shader)
    gst_object_unref (alpha->chroma_key_shader);
  alpha->chroma_key_shader = NULL;

  GST_GL_BASE_FILTER_CLASS (parent_class)->gl_stop (base_filter);
}

static void
gst_gl_alpha_before_transform (GstBaseTransform * base, GstBuffer * buf)
{
  GstGLAlpha *alpha = GST_GL_ALPHA (base);
  GstClockTime timestamp, stream_time;

  timestamp = GST_BUFFER_TIMESTAMP (buf);
  stream_time =
      gst_segment_to_stream_time (&base->segment, GST_FORMAT_TIME, timestamp);

  GST_DEBUG_OBJECT (alpha, "sync to %" GST_TIME_FORMAT,
      GST_TIME_ARGS (timestamp));

  if (GST_CLOCK_TIME_IS_VALID (stream_time))
    gst_object_sync_values (GST_OBJECT (alpha), stream_time);
}

static gboolean
gst_gl_alpha_filter_texture (GstGLFilter * filter, GstGLMemory * in_tex,
    GstGLMemory * out_tex)
{
  GstGLAlpha *alpha = GST_GL_ALPHA (filter);
  GstGLShader *shader;

  if (!alpha->alpha_shader)
    _create_shader (alpha);

  GST_OBJECT_LOCK (alpha);
  if (alpha->method == ALPHA_METHOD_SET) {
    shader = alpha->alpha_shader;
    gst_gl_shader_use (shader);
    gst_gl_shader_set_uniform_1f (shader, "alpha", alpha->alpha);
  } else {
    shader = alpha->chroma_key_shader;
    gst_gl_shader_use (shader);

    gst_gl_shader_set_uniform_1f (shader, "cb", alpha->cb);
    gst_gl_shader_set_uniform_1f (shader, "cr", alpha->cr);
    gst_gl_shader_set_uniform_1f (shader, "kg", alpha->kg);
    gst_gl_shader_set_uniform_1f (shader, "accept_angle_tg",
        alpha->accept_angle_tg);
    gst_gl_shader_set_uniform_1f (shader, "accept_angle_ctg",
        alpha->accept_angle_ctg);
    gst_gl_shader_set_uniform_1f (shader, "one_over_kc", alpha->one_over_kc);
    gst_gl_shader_set_uniform_1f (shader, "kfgy_scale", alpha->kfgy_scale);
    gst_gl_shader_set_uniform_1f (shader, "noise_level2", alpha->noise_level2);
    gst_gl_shader_set_uniform_1f (shader, "smin",
        0.5 - alpha->black_sensitivity / 255.0);
    gst_gl_shader_set_uniform_1f (shader, "smax",
        0.5 + alpha->white_sensitivity / 255.0);
  }
  GST_OBJECT_UNLOCK (alpha);

  gst_gl_filter_render_to_target_with_shader (filter, in_tex, out_tex, shader);

  return TRUE;
}

static void
gst_gl_alpha_class_init (GstGLAlphaClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseTransformClass *trans_class = (GstBaseTransformClass *) klass;
  GstGLBaseFilterClass *base_filter_class = (GstGLBaseFilterClass *) klass;
  GstGLFilterClass *filter_class = (GstGLFilterClass *) klass;

  GST_DEBUG_CATEGORY_INIT (glalpha_debug, "glalpha", 0, "glalpha");

  gst_gl_filter_add_rgba_pad_templates (GST_GL_FILTER_CLASS (klass));

  gobject_class->set_property = gst_gl_alpha_set_property;
  gobject_class->get_property = gst_gl_alpha_get_property;

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_METHOD,
      g_param_spec_enum ("method", "Method",
          "How the alpha channels should be created", GST_TYPE_GL_ALPHA_METHOD,
          DEFAULT_METHOD, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_ALPHA,
      g_param_spec_double ("alpha", "Alpha", "The value for the alpha channel",
          0.0, 1.0, DEFAULT_ALPHA,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_TARGET_R,
      g_param_spec_uint ("target-r", "Target Red",
          "The red color value for custom RGB chroma keying", 0, 255,
          DEFAULT_TARGET_R,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_TARGET_G,
      g_param_spec_uint ("target-g", "Target Green",
          "The green color value for custom RGB chroma keying", 0, 255,
          DEFAULT_TARGET_G,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_TARGET_B,
      g_param_spec_uint ("target-b", "Target Blue",
          "The blue color value for custom RGB chroma keying", 0, 255,
          DEFAULT_TARGET_B,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_ANGLE,
      g_param_spec_float ("angle", "Angle", "Size of the colorcube to change",
          0.0, 90.0, DEFAULT_ANGLE,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_NOISE_LEVEL,
      g_param_spec_float ("noise-level", "Noise Level", "Size of noise radius",
          0.0, 64.0, DEFAULT_NOISE_LEVEL,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_BLACK_SENSITIVITY, g_param_spec_uint ("black-sensitivity",
          "Black Sensitivity", "Sensitivity to dark colors", 0, 128,
          DEFAULT_BLACK_SENSITIVITY,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_WHITE_SENSITIVITY, g_param_spec_uint ("white-sensitivity",
          "White Sensitivity", "Sensitivity to bright colors", 0, 128,
          DEFAULT_WHITE_SENSITIVITY,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (gstelement_class,
      "OpenGL Alpha Filter", "Filter/Effect/Video",
      "Adds an alpha channel to video using OpenGL - uniform or chroma-keying",
      "Matthew Waters <matthew@centricular.com>");

  trans_class->before_transform =
      GST_DEBUG_FUNCPTR (gst_gl_alpha_before_transform);
  trans_class->transform_ip_on_passthrough = FALSE;

  base_filter_class->gl_start = GST_DEBUG_FUNCPTR (gst_gl_alpha_gl_start);
  base_filter_class->gl_stop = GST_DEBUG_FUNCPTR (gst_gl_alpha_gl_stop);

  filter_class->filter_texture =
      GST_DEBUG_FUNCPTR (gst_gl_alpha_filter_texture);

  gst_type_mark_as_plugin_api (GST_TYPE_GL_ALPHA_METHOD, 0);
}

static void
gst_gl_alpha_init (GstGLAlpha * alpha)
{
  alpha->alpha = DEFAULT_ALPHA;
  alpha->method = DEFAULT_METHOD;
  alpha->target_r = DEFAULT_TARGET_R;
  alpha->target_g = DEFAULT_TARGET_G;
  alpha->target_b = DEFAULT_TARGET_B;
  alpha->angle = DEFAULT_ANGLE;
  alpha->noise_level = DEFAULT_NOISE_LEVEL;
  alpha->black_sensitivity = DEFAULT_BLACK_SENSITIVITY;
  alpha->white_sensitivity = DEFAULT_WHITE_SENSITIVITY;

  gst_gl_alpha_update_properties (alpha);
}

static void
gst_gl_alpha_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGLAlpha *alpha = GST_GL_ALPHA (object);

  GST_OBJECT_LOCK (alpha);
  switch (prop_id) {
    case PROP_METHOD:{
      alpha->method = g_value_get_enum (value);
      break;
    }
    case PROP_ALPHA:{
      alpha->alpha = g_value_get_double (value);
      break;
    }
    case PROP_TARGET_R:
      alpha->target_r = g_value_get_uint (value);
      break;
    case PROP_TARGET_G:
      alpha->target_g = g_value_get_uint (value);
      break;
    case PROP_TARGET_B:
      alpha->target_b = g_value_get_uint (value);
      break;
    case PROP_ANGLE:
      alpha->angle = g_value_get_float (value);
      break;
    case PROP_NOISE_LEVEL:
      alpha->noise_level = g_value_get_float (value);
      break;
    case PROP_BLACK_SENSITIVITY:
      alpha->black_sensitivity = g_value_get_uint (value);
      break;
    case PROP_WHITE_SENSITIVITY:
      alpha->white_sensitivity = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (alpha);
  gst_gl_alpha_update_properties (alpha);
}

static void
gst_gl_alpha_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGLAlpha *alpha = GST_GL_ALPHA (object);

  switch (prop_id) {
    case PROP_METHOD:
      g_value_set_enum (value, alpha->method);
      break;
    case PROP_ALPHA:
      g_value_set_double (value, alpha->alpha);
      break;
    case PROP_TARGET_R:
      g_value_set_uint (value, alpha->target_r);
      break;
    case PROP_TARGET_G:
      g_value_set_uint (value, alpha->target_g);
      break;
    case PROP_TARGET_B:
      g_value_set_uint (value, alpha->target_b);
      break;
    case PROP_ANGLE:
      g_value_set_float (value, alpha->angle);
      break;
    case PROP_NOISE_LEVEL:
      g_value_set_float (value, alpha->noise_level);
      break;
    case PROP_BLACK_SENSITIVITY:
      g_value_set_uint (value, alpha->black_sensitivity);
      break;
    case PROP_WHITE_SENSITIVITY:
      g_value_set_uint (value, alpha->white_sensitivity);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
