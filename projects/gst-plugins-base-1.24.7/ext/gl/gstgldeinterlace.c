/*
 * GStreamer
 * Copyright (C) 2009 Julien Isorce <julien.isorce@mail.com>
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
 * SECTION:element-gldeinterlace
 * @title: gldeinterlace
 *
 * Deinterlace video using OpenGL fragment shaders.
 *
 * ## Examples
 * |[
 * gst-launch-1.0 videotestsrc ! glupload ! gldeinterlace ! glimagesink
 * ]|
 * FBO (Frame Buffer Object) and GLSL (OpenGL Shading Language) are required.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gl/gstglfuncs.h>

#include "gstglelements.h"
#include "gstgldeinterlace.h"

#define GST_CAT_DEFAULT gst_gl_deinterlace_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  PROP_0,
  PROP_METHOD
};

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_gl_deinterlace_debug, "gldeinterlace", 0, "gldeinterlace element");
#define gst_gl_deinterlace_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGLDeinterlace, gst_gl_deinterlace,
    GST_TYPE_GL_FILTER, DEBUG_INIT);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (gldeinterlace, "gldeinterlace",
    GST_RANK_NONE, GST_TYPE_GL_DEINTERLACE, gl_element_init (plugin));

static void gst_gl_deinterlace_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_gl_deinterlace_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static gboolean gst_gl_deinterlace_start (GstBaseTransform * trans);
static gboolean gst_gl_deinterlace_reset (GstBaseTransform * trans);
static GstCaps *gst_gl_deinterlace_transform_internal_caps (GstGLFilter *
    filter, GstPadDirection direction, GstCaps * caps, GstCaps * caps_filter);
static gboolean gst_gl_deinterlace_init_fbo (GstGLFilter * filter);
static gboolean gst_gl_deinterlace_filter (GstGLFilter * filter,
    GstBuffer * inbuf, GstBuffer * outbuf);
static gboolean gst_gl_deinterlace_filter_texture (GstGLFilter * filter,
    GstGLMemory * in_tex, GstGLMemory * out_tex);
static gboolean gst_gl_deinterlace_vfir_callback (GstGLFilter * filter,
    GstGLMemory * in_tex, gpointer stuff);
static gboolean gst_gl_deinterlace_greedyh_callback (GstGLFilter * filter,
    GstGLMemory * in_tex, gpointer stuff);

/* *INDENT-OFF* */
static const gchar *greedyh_fragment_source =
  "uniform sampler2D tex;\n"
  "uniform sampler2D tex_prev;\n"
  "uniform float max_comb;\n"
  "uniform float motion_threshold;\n"
  "uniform float motion_sense;\n"
  "uniform float width;\n"
  "uniform float height;\n"
  "varying vec2 v_texcoord;\n"

  "void main () {\n"
  "  if (int(mod(v_texcoord.y * height, 2.0)) == 0) {\n"
  "    gl_FragColor = vec4(texture2D(tex_prev, v_texcoord).rgb, 1.0);\n"
  "  } else {\n"
  "    vec2 texcoord_L1_a1, texcoord_L3_a1, texcoord_L1, texcoord_L3, texcoord_L1_1, texcoord_L3_1;\n"
  "    vec3 L1_a1, L3_a1, L1, L3, L1_1, L3_1;\n"

  "    texcoord_L1 = vec2(v_texcoord.x, v_texcoord.y - 1.0 / height);\n"
  "    texcoord_L3 = vec2(v_texcoord.x, v_texcoord.y + 1.0 / height);\n"
  "    L1 = texture2D(tex_prev, texcoord_L1).rgb;\n"
  "    L3 = texture2D(tex_prev, texcoord_L3).rgb;\n"
  "    if (v_texcoord.x == 1.0 && v_texcoord.y == 1.0) {\n"
  "      L1_1 = L1;\n"
  "      L3_1 = L3;\n"
  "    } else {\n"
  "      texcoord_L1_1 = vec2(v_texcoord.x + 1.0 / width, v_texcoord.y - 1.0 / height);\n"
  "      texcoord_L3_1 = vec2(v_texcoord.x + 1.0 / width, v_texcoord.y + 1.0 / height);\n"
  "      L1_1 = texture2D(tex_prev, texcoord_L1_1).rgb;\n"
  "      L3_1 = texture2D(tex_prev, texcoord_L3_1).rgb;\n"
  "    }\n"

  "    if (int(ceil(v_texcoord.x + v_texcoord.y)) == 0) {\n"
  "      L1_a1 = L1;\n"
  "      L3_a1 = L3;\n"
  "    } else {\n"
  "      texcoord_L1_a1 = vec2(v_texcoord.x - 1.0 / width, v_texcoord.y - 1.0 / height);\n"
  "      texcoord_L3_a1 = vec2(v_texcoord.x - 1.0 / width, v_texcoord.y + 1.0 / height);\n"
  "      L1_a1 = texture2D(tex_prev, texcoord_L1_a1).rgb;\n"
  "      L3_a1 = texture2D(tex_prev, texcoord_L3_a1).rgb;\n"
  "    }\n"
          //STEP 1
  "    vec3 avg_a1 = (L1_a1 + L3_a1) / 2.0;\n"
  "    vec3 avg = (L1 + L3) / 2.0;\n"
  "    vec3 avg_1 = (L1_1 + L3_1) / 2.0;\n"
  "    vec3 avg_s = (avg_a1 + avg_1) / 2.0;\n"
  "    vec3 avg_sc = (avg_s + avg) / 2.0;\n"
  "    vec3 L2 = texture2D(tex, v_texcoord).rgb;\n"
  "    vec3 LP2 = texture2D(tex_prev, v_texcoord).rgb;\n"
  "    vec3 best;\n"
  "    if (abs(L2.r - avg_sc.r) < abs(LP2.r - avg_sc.r)) {\n"
  "      best.r = L2.r;\n" "    } else {\n"
  "      best.r = LP2.r;\n"
  "    }\n"

  "    if (abs(L2.g - avg_sc.g) < abs(LP2.g - avg_sc.g)) {\n"
  "      best.g = L2.g;\n"
  "    } else {\n"
  "      best.g = LP2.g;\n"
  "    }\n"

  "    if (abs(L2.b - avg_sc.b) < abs(LP2.b - avg_sc.b)) {\n"
  "      best.b = L2.b;\n"
  "    } else {\n"
  "      best.b = LP2.b;\n"
  "    }\n"
          //STEP 2
  "    vec3 last;\n"
  "    last.r = clamp(best.r, max(min(L1.r, L3.r) - max_comb, 0.0), min(max(L1.r, L3.r) + max_comb, 1.0));\n"
  "    last.g = clamp(best.g, max(min(L1.g, L3.g) - max_comb, 0.0), min(max(L1.g, L3.g) + max_comb, 1.0));\n"
  "    last.b = clamp(best.b, max(min(L1.b, L3.b) - max_comb, 0.0), min(max(L1.b, L3.b) + max_comb, 1.0));\n"
          //STEP 3
  "    const vec3 luma = vec3 (0.299011, 0.586987, 0.114001);"
  "    float mov = min(max(abs(dot(L2 - LP2, luma)) - motion_threshold, 0.0) * motion_sense, 1.0);\n"
  "    last = last * (1.0 - mov) + avg_sc * mov;\n"
  "    gl_FragColor = vec4(last, 1.0);\n"
  "  }\n"
  "}\n";

const gchar *vfir_fragment_source =
  "uniform sampler2D tex;\n"
  "uniform float width;\n"
  "uniform float height;\n"
  "varying vec2 v_texcoord;\n"
  "void main()\n"
  "{\n"
  "  vec2 topcoord, botcoord;\n"
  "  vec4 cur_color, top_color, bot_color;\n"
  "  topcoord.x = v_texcoord.x;\n"
  "  botcoord.x = v_texcoord.x;\n"
  "  if (v_texcoord.y == 0.0 || v_texcoord.y == 1.0) {\n"
  "    topcoord.y = v_texcoord.y ;\n"
  "    botcoord.y = v_texcoord.y ;\n"
  "  }\n"
  "  else {\n"
  "    topcoord.y = v_texcoord.y - 1.0/height;\n"
  "    botcoord.y = v_texcoord.y + 1.0/height;\n"
  "  }\n"
  "  cur_color = texture2D(tex, v_texcoord);\n"
  "  top_color = texture2D(tex, topcoord);\n"
  "  bot_color = texture2D(tex, botcoord);\n"
  "  gl_FragColor = 0.5*cur_color + 0.25*top_color + 0.25*bot_color;\n"
  "}";
/* *INDENT-ON* */

/* dont' forget to edit the following when a new method is added */
typedef enum
{
  GST_GL_DEINTERLACE_VFIR,
  GST_GL_DEINTERLACE_GREEDYH
} GstGLDeinterlaceMethod;

static const GEnumValue *
gst_gl_deinterlace_get_methods (void)
{
  static const GEnumValue method_types[] = {
    {GST_GL_DEINTERLACE_VFIR, "Blur Vertical", "vfir"},
    {GST_GL_DEINTERLACE_GREEDYH, "Motion Adaptive: Advanced Detection",
        "greedyh"},
    {0, NULL, NULL}
  };
  return method_types;
}

#define GST_TYPE_GL_DEINTERLACE_METHODS (gst_gl_deinterlace_method_get_type ())
static GType
gst_gl_deinterlace_method_get_type (void)
{
  static GType gl_deinterlace_method_type = 0;
  if (!gl_deinterlace_method_type) {
    gl_deinterlace_method_type =
        g_enum_register_static ("GstGLDeinterlaceMethod",
        gst_gl_deinterlace_get_methods ());
  }
  return gl_deinterlace_method_type;
}

static void
gst_gl_deinterlace_set_method (GstGLDeinterlace * deinterlace,
    guint method_types)
{
  switch (method_types) {
    case GST_GL_DEINTERLACE_VFIR:
      deinterlace->deinterlacefunc = gst_gl_deinterlace_vfir_callback;
      deinterlace->current_method = method_types;
      break;
    case GST_GL_DEINTERLACE_GREEDYH:
      deinterlace->deinterlacefunc = gst_gl_deinterlace_greedyh_callback;
      deinterlace->current_method = method_types;
      break;
    default:
      g_assert_not_reached ();
      break;
  }
}

static void
gst_gl_deinterlace_class_init (GstGLDeinterlaceClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;

  gobject_class = (GObjectClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);

  gst_gl_filter_add_rgba_pad_templates (GST_GL_FILTER_CLASS (klass));

  gobject_class->set_property = gst_gl_deinterlace_set_property;
  gobject_class->get_property = gst_gl_deinterlace_get_property;

  gst_element_class_set_metadata (element_class,
      "OpenGL deinterlacing filter", "Deinterlace",
      "Deinterlacing based on fragment shaders",
      "Julien Isorce <julien.isorce@mail.com>");

  g_object_class_install_property (gobject_class,
      PROP_METHOD,
      g_param_spec_enum ("method",
          "Deinterlace Method",
          "Select which deinterlace method apply to GL video texture",
          GST_TYPE_GL_DEINTERLACE_METHODS,
          GST_GL_DEINTERLACE_VFIR, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  GST_BASE_TRANSFORM_CLASS (klass)->start = gst_gl_deinterlace_start;
  GST_BASE_TRANSFORM_CLASS (klass)->stop = gst_gl_deinterlace_reset;

  GST_GL_FILTER_CLASS (klass)->transform_internal_caps =
      gst_gl_deinterlace_transform_internal_caps;
  GST_GL_FILTER_CLASS (klass)->filter = gst_gl_deinterlace_filter;
  GST_GL_FILTER_CLASS (klass)->filter_texture =
      gst_gl_deinterlace_filter_texture;
  GST_GL_FILTER_CLASS (klass)->init_fbo = gst_gl_deinterlace_init_fbo;

  GST_GL_BASE_FILTER_CLASS (klass)->supported_gl_api =
      GST_GL_API_OPENGL | GST_GL_API_GLES2 | GST_GL_API_OPENGL3;

  gst_type_mark_as_plugin_api (GST_TYPE_GL_DEINTERLACE_METHODS, 0);
}

static void
gst_gl_deinterlace_init (GstGLDeinterlace * filter)
{
  filter->shaderstable = NULL;
  filter->deinterlacefunc = gst_gl_deinterlace_vfir_callback;
  filter->current_method = GST_GL_DEINTERLACE_VFIR;
  filter->prev_buffer = NULL;
  filter->prev_tex = NULL;
}

static gboolean
gst_gl_deinterlace_start (GstBaseTransform * trans)
{
  GstGLDeinterlace *deinterlace_filter = GST_GL_DEINTERLACE (trans);

  deinterlace_filter->shaderstable = g_hash_table_new (g_str_hash, g_str_equal);

  return GST_BASE_TRANSFORM_CLASS (parent_class)->start (trans);
}

static void
gst_gl_deinterlace_ghash_func_clean (gpointer key, gpointer value,
    gpointer data)
{
  GstGLShader *shader = (GstGLShader *) value;

  gst_object_unref (shader);

  value = NULL;
}

static gboolean
gst_gl_deinterlace_reset (GstBaseTransform * trans)
{
  GstGLDeinterlace *deinterlace_filter = GST_GL_DEINTERLACE (trans);

  gst_buffer_replace (&deinterlace_filter->prev_buffer, NULL);

  //blocking call, wait the opengl thread has destroyed the shader
  if (deinterlace_filter->shaderstable) {
    /* release shaders in the gl thread */
    g_hash_table_foreach (deinterlace_filter->shaderstable,
        gst_gl_deinterlace_ghash_func_clean, deinterlace_filter);

    /* clean the htable without calling values destructors
     * because shaders have been released in the glthread
     * through the foreach func */
    g_hash_table_unref (deinterlace_filter->shaderstable);
    deinterlace_filter->shaderstable = NULL;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->stop (trans);
}

static GstCaps *
gst_gl_deinterlace_transform_internal_caps (GstGLFilter * filter,
    GstPadDirection direction, GstCaps * caps, GstCaps * caps_filter)
{
  gint len;
  GstCaps *res;
  GstStructure *s;

  res = gst_caps_copy (caps);

  for (len = gst_caps_get_size (res); len > 0; len--) {
    s = gst_caps_get_structure (res, len - 1);
    if (direction == GST_PAD_SINK) {
      gst_structure_remove_field (s, "interlace-mode");
    }
  }

  return res;
}

static void
gst_gl_deinterlace_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGLDeinterlace *filter = GST_GL_DEINTERLACE (object);

  switch (prop_id) {
    case PROP_METHOD:
      gst_gl_deinterlace_set_method (filter, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_deinterlace_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGLDeinterlace *filter = GST_GL_DEINTERLACE (object);

  switch (prop_id) {
    case PROP_METHOD:
      g_value_set_enum (value, filter->current_method);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_gl_deinterlace_init_fbo (GstGLFilter * filter)
{
  return TRUE;
}

static gboolean
gst_gl_deinterlace_filter_texture (GstGLFilter * filter, GstGLMemory * in_tex,
    GstGLMemory * out_tex)
{
  GstGLDeinterlace *deinterlace_filter = GST_GL_DEINTERLACE (filter);

  //blocking call, use a FBO
  gst_gl_filter_render_to_target (filter, in_tex, out_tex,
      deinterlace_filter->deinterlacefunc, deinterlace_filter);

  return TRUE;
}

static gboolean
gst_gl_deinterlace_filter (GstGLFilter * filter, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstGLDeinterlace *deinterlace_filter = GST_GL_DEINTERLACE (filter);

  gst_gl_filter_filter_texture (filter, inbuf, outbuf);

  gst_buffer_replace (&deinterlace_filter->prev_buffer, inbuf);

  return TRUE;
}

static GstGLShader *
gst_gl_deinterlace_get_fragment_shader (GstGLFilter * filter,
    const gchar * shader_name, const gchar * shader_source)
{
  GstGLShader *shader = NULL;
  GstGLDeinterlace *deinterlace_filter = GST_GL_DEINTERLACE (filter);
  GstGLContext *context = GST_GL_BASE_FILTER (filter)->context;
  const gchar *frags[2];

  shader = g_hash_table_lookup (deinterlace_filter->shaderstable, shader_name);

  frags[0] =
      gst_gl_shader_string_get_highest_precision (context,
      GST_GLSL_VERSION_NONE,
      GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY);
  frags[1] = shader_source;

  if (!shader) {
    GError *error = NULL;

    if (!(shader = gst_gl_shader_new_link_with_stages (context, &error,
                gst_glsl_stage_new_default_vertex (context),
                gst_glsl_stage_new_with_strings (context, GL_FRAGMENT_SHADER,
                    GST_GLSL_VERSION_NONE,
                    GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY, 2,
                    frags), NULL))) {
      GST_ELEMENT_ERROR (deinterlace_filter, RESOURCE, NOT_FOUND,
          ("Failed to initialize %s shader", shader_name), (NULL));
    }

    filter->draw_attr_position_loc =
        gst_gl_shader_get_attribute_location (shader, "a_position");
    filter->draw_attr_texture_loc =
        gst_gl_shader_get_attribute_location (shader, "a_texcoord");
  }

  g_hash_table_insert (deinterlace_filter->shaderstable, (gchar *) shader_name,
      shader);

  return shader;
}

static gboolean
gst_gl_deinterlace_vfir_callback (GstGLFilter * filter, GstGLMemory * in_tex,
    gpointer user_data)
{
  GstGLContext *context = GST_GL_BASE_FILTER (filter)->context;
  const GstGLFuncs *gl = context->gl_vtable;
  GstGLShader *shader;

  shader = gst_gl_deinterlace_get_fragment_shader (filter, "vfir",
      vfir_fragment_source);

  if (!shader)
    return FALSE;

#if GST_GL_HAVE_OPENGL
  if (USING_OPENGL (context)) {
    gl->MatrixMode (GL_PROJECTION);
    gl->LoadIdentity ();
  }
#endif

  gst_gl_shader_use (shader);

  gl->ActiveTexture (GL_TEXTURE0);
  gl->BindTexture (GL_TEXTURE_2D, gst_gl_memory_get_texture_id (in_tex));

  gst_gl_shader_set_uniform_1i (shader, "tex", 0);
  gst_gl_shader_set_uniform_1f (shader, "width",
      GST_VIDEO_INFO_WIDTH (&filter->out_info));
  gst_gl_shader_set_uniform_1f (shader, "height",
      GST_VIDEO_INFO_HEIGHT (&filter->out_info));

  gst_gl_filter_draw_fullscreen_quad (filter);

  return TRUE;
}

static gboolean
gst_gl_deinterlace_greedyh_callback (GstGLFilter * filter, GstGLMemory * in_tex,
    gpointer user_data)
{
  GstGLShader *shader;
  GstGLDeinterlace *deinterlace_filter = GST_GL_DEINTERLACE (filter);
  GstGLContext *context = GST_GL_BASE_FILTER (filter)->context;
  GstGLFuncs *gl = context->gl_vtable;

  shader =
      gst_gl_deinterlace_get_fragment_shader (filter, "greedhy",
      greedyh_fragment_source);

  if (!shader)
    return FALSE;

#if GST_GL_HAVE_OPENGL
  if (USING_OPENGL (context)) {
    gl->MatrixMode (GL_PROJECTION);
    gl->LoadIdentity ();
  }
#endif

  gst_gl_shader_use (shader);

  if (G_LIKELY (deinterlace_filter->prev_tex != NULL)) {
    gl->ActiveTexture (GL_TEXTURE1);
    gst_gl_shader_set_uniform_1i (shader, "tex_prev", 1);
    gl->BindTexture (GL_TEXTURE_2D,
        gst_gl_memory_get_texture_id (deinterlace_filter->prev_tex));
  }

  gl->ActiveTexture (GL_TEXTURE0);
  gl->BindTexture (GL_TEXTURE_2D, gst_gl_memory_get_texture_id (in_tex));

  gst_gl_shader_set_uniform_1i (shader, "tex", 0);
  gst_gl_shader_set_uniform_1f (shader, "max_comb", 5.0f / 255.0f);
  gst_gl_shader_set_uniform_1f (shader, "motion_threshold", 25.0f / 255.0f);
  gst_gl_shader_set_uniform_1f (shader, "motion_sense", 30.0f / 255.0f);

  gst_gl_shader_set_uniform_1f (shader, "width",
      GST_VIDEO_INFO_WIDTH (&filter->out_info));
  gst_gl_shader_set_uniform_1f (shader, "height",
      GST_VIDEO_INFO_HEIGHT (&filter->out_info));

  gst_gl_filter_draw_fullscreen_quad (filter);

  /* we keep the previous buffer around so this is safe */
  deinterlace_filter->prev_tex = in_tex;

  return TRUE;
}
