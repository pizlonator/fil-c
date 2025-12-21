/*
 * GStreamer
 * Copyright (C) 2008 Filippo Argiolas <filippo.argiolas@gmail.com>
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
 * SECTION:element-gleffects.
 * @title: gleffects.
 *
 * GL Shading Language effects.
 *
 * ## Examples
 * |[
 * gst-launch-1.0 videotestsrc ! glupload ! gleffects effect=5 ! glimagesink
 * ]|
 * FBO (Frame Buffer Object) and GLSL (OpenGL Shading Language) are required.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstglelements.h"
#include "gstgleffects.h"

#define GST_CAT_DEFAULT gst_gl_effects_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  PROP_0 = 0x0,
  PROP_EFFECT = 0x1 << 1,
  PROP_HSWAP = 0x1 << 2,
  PROP_INVERT = 0x1 << 3
};

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_gl_effects_debug, "gleffects", 0, "gleffects element");

#define gst_gl_effects_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGLEffects, gst_gl_effects, GST_TYPE_GL_FILTER,
    DEBUG_INIT);

static gboolean
gst_element_init_gleffects (GstPlugin * plugin)
{
  gl_element_init (plugin);
  return gst_gl_effects_register_filters (plugin, GST_RANK_NONE);
}

GST_ELEMENT_REGISTER_DEFINE_CUSTOM (gleffects, gst_element_init_gleffects);

static void gst_gl_effects_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gl_effects_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_gl_effects_init_resources (GstBaseTransform * trans);
static gboolean gst_gl_effects_reset_resources (GstBaseTransform * trans);

static gboolean gst_gl_effects_on_init_gl_context (GstGLFilter * filter);

static void gst_gl_effects_ghash_func_clean (gpointer key, gpointer value,
    gpointer data);

static gboolean gst_gl_effects_filter_texture (GstGLFilter * filter,
    GstGLMemory * in_tex, GstGLMemory * out_tex);
static gboolean gst_gl_effects_filters_is_property_supported (const
    GstGLEffectsFilterDescriptor *, gint property);

/* dont' forget to edit the following when a new effect is added */
typedef enum
{
  GST_GL_EFFECT_IDENTITY,
  GST_GL_EFFECT_MIRROR,
  GST_GL_EFFECT_SQUEEZE,
  GST_GL_EFFECT_STRETCH,
  GST_GL_EFFECT_TUNNEL,
  GST_GL_EFFECT_FISHEYE,
  GST_GL_EFFECT_TWIRL,
  GST_GL_EFFECT_BULGE,
  GST_GL_EFFECT_SQUARE,
  GST_GL_EFFECT_HEAT,
  GST_GL_EFFECT_SEPIA,
  GST_GL_EFFECT_XPRO,
  GST_GL_EFFECT_LUMA_XPRO,
  GST_GL_EFFECT_XRAY,
  GST_GL_EFFECT_SIN,
  GST_GL_EFFECT_GLOW,
  GST_GL_EFFECT_SOBEL,
  GST_GL_EFFECT_BLUR,
  GST_GL_EFFECT_LAPLACIAN,
  GST_GL_N_EFFECTS
} GstGLEffectsEffect;

static const GEnumValue *
gst_gl_effects_get_effects (void)
{
  static const GEnumValue effect_types[] = {
    {GST_GL_EFFECT_IDENTITY, "Do nothing Effect", "identity"},
    {GST_GL_EFFECT_MIRROR, "Mirror Effect", "mirror"},
    {GST_GL_EFFECT_SQUEEZE, "Squeeze Effect", "squeeze"},
    {GST_GL_EFFECT_STRETCH, "Stretch Effect", "stretch"},
    {GST_GL_EFFECT_TUNNEL, "Light Tunnel Effect", "tunnel"},
    {GST_GL_EFFECT_FISHEYE, "FishEye Effect", "fisheye"},
    {GST_GL_EFFECT_TWIRL, "Twirl Effect", "twirl"},
    {GST_GL_EFFECT_BULGE, "Bulge Effect", "bulge"},
    {GST_GL_EFFECT_SQUARE, "Square Effect", "square"},
    {GST_GL_EFFECT_HEAT, "Heat Signature Effect", "heat"},
    {GST_GL_EFFECT_SEPIA, "Sepia Toning Effect", "sepia"},
    {GST_GL_EFFECT_XPRO, "Cross Processing Effect", "xpro"},
    {GST_GL_EFFECT_LUMA_XPRO, "Luma Cross Processing Effect", "lumaxpro"},
    {GST_GL_EFFECT_XRAY, "Glowing negative effect", "xray"},
    {GST_GL_EFFECT_SIN, "All Grey but Red Effect", "sin"},
    {GST_GL_EFFECT_GLOW, "Glow Lighting Effect", "glow"},
    {GST_GL_EFFECT_SOBEL, "Sobel edge detection Effect", "sobel"},
    {GST_GL_EFFECT_BLUR, "Blur with 9x9 separable convolution Effect", "blur"},
    {GST_GL_EFFECT_LAPLACIAN, "Laplacian Convolution Demo Effect", "laplacian"},
    {0, NULL, NULL}
  };
  return effect_types;
}

#define GST_TYPE_GL_EFFECTS_EFFECT (gst_gl_effects_effect_get_type ())
static GType
gst_gl_effects_effect_get_type (void)
{
  static GType gl_effects_effect_type = 0;
  if (!gl_effects_effect_type) {
    gl_effects_effect_type =
        g_enum_register_static ("GstGLEffectsEffect",
        gst_gl_effects_get_effects ());
  }
  return gl_effects_effect_type;
}

static void
gst_gl_effects_set_effect (GstGLEffects * effects, gint effect_type)
{
  GstGLBaseFilterClass *filter_class = GST_GL_BASE_FILTER_GET_CLASS (effects);

  switch (effect_type) {
    case GST_GL_EFFECT_IDENTITY:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_identity;
      filter_class->supported_gl_api =
          GST_GL_API_GLES2 | GST_GL_API_OPENGL | GST_GL_API_OPENGL3;
      effects->current_effect = effect_type;
      break;
    case GST_GL_EFFECT_MIRROR:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_mirror;
      filter_class->supported_gl_api =
          GST_GL_API_GLES2 | GST_GL_API_OPENGL | GST_GL_API_OPENGL3;
      effects->current_effect = effect_type;
      break;
    case GST_GL_EFFECT_SQUEEZE:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_squeeze;
      filter_class->supported_gl_api =
          GST_GL_API_GLES2 | GST_GL_API_OPENGL | GST_GL_API_OPENGL3;
      effects->current_effect = effect_type;
      break;
    case GST_GL_EFFECT_STRETCH:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_stretch;
      filter_class->supported_gl_api =
          GST_GL_API_GLES2 | GST_GL_API_OPENGL | GST_GL_API_OPENGL3;
      effects->current_effect = effect_type;
      break;
    case GST_GL_EFFECT_TUNNEL:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_tunnel;
      filter_class->supported_gl_api =
          GST_GL_API_GLES2 | GST_GL_API_OPENGL | GST_GL_API_OPENGL3;
      effects->current_effect = effect_type;
      break;
    case GST_GL_EFFECT_FISHEYE:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_fisheye;
      filter_class->supported_gl_api =
          GST_GL_API_GLES2 | GST_GL_API_OPENGL | GST_GL_API_OPENGL3;
      effects->current_effect = effect_type;
      break;
    case GST_GL_EFFECT_TWIRL:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_twirl;
      filter_class->supported_gl_api =
          GST_GL_API_GLES2 | GST_GL_API_OPENGL | GST_GL_API_OPENGL3;
      effects->current_effect = effect_type;
      break;
    case GST_GL_EFFECT_BULGE:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_bulge;
      filter_class->supported_gl_api =
          GST_GL_API_GLES2 | GST_GL_API_OPENGL | GST_GL_API_OPENGL3;
      effects->current_effect = effect_type;
      break;
    case GST_GL_EFFECT_SQUARE:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_square;
      filter_class->supported_gl_api =
          GST_GL_API_GLES2 | GST_GL_API_OPENGL | GST_GL_API_OPENGL3;
      effects->current_effect = effect_type;
      break;
    case GST_GL_EFFECT_HEAT:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_heat;
      filter_class->supported_gl_api =
          GST_GL_API_GLES2 | GST_GL_API_OPENGL | GST_GL_API_OPENGL3;
      effects->current_effect = effect_type;
      break;
    case GST_GL_EFFECT_SEPIA:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_sepia;
      filter_class->supported_gl_api =
          GST_GL_API_GLES2 | GST_GL_API_OPENGL | GST_GL_API_OPENGL3;
      effects->current_effect = effect_type;
      break;
    case GST_GL_EFFECT_XPRO:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_xpro;
      filter_class->supported_gl_api =
          GST_GL_API_GLES2 | GST_GL_API_OPENGL | GST_GL_API_OPENGL3;
      effects->current_effect = effect_type;
      break;
    case GST_GL_EFFECT_LUMA_XPRO:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_luma_xpro;
      filter_class->supported_gl_api =
          GST_GL_API_GLES2 | GST_GL_API_OPENGL | GST_GL_API_OPENGL3;
      effects->current_effect = effect_type;
      break;
    case GST_GL_EFFECT_SIN:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_sin;
      filter_class->supported_gl_api =
          GST_GL_API_GLES2 | GST_GL_API_OPENGL | GST_GL_API_OPENGL3;
      effects->current_effect = effect_type;
      break;
    case GST_GL_EFFECT_XRAY:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_xray;
      filter_class->supported_gl_api =
          GST_GL_API_GLES2 | GST_GL_API_OPENGL | GST_GL_API_OPENGL3;
      effects->current_effect = effect_type;
      break;
    case GST_GL_EFFECT_GLOW:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_glow;
      filter_class->supported_gl_api =
          GST_GL_API_GLES2 | GST_GL_API_OPENGL | GST_GL_API_OPENGL3;
      effects->current_effect = effect_type;
      break;
    case GST_GL_EFFECT_SOBEL:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_sobel;
      filter_class->supported_gl_api =
          GST_GL_API_GLES2 | GST_GL_API_OPENGL | GST_GL_API_OPENGL3;
      effects->current_effect = effect_type;
      break;
    case GST_GL_EFFECT_BLUR:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_blur;
      filter_class->supported_gl_api =
          GST_GL_API_GLES2 | GST_GL_API_OPENGL | GST_GL_API_OPENGL3;
      effects->current_effect = effect_type;
      break;
    case GST_GL_EFFECT_LAPLACIAN:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_laplacian;
      filter_class->supported_gl_api =
          GST_GL_API_GLES2 | GST_GL_API_OPENGL | GST_GL_API_OPENGL3;
      effects->current_effect = effect_type;
      break;
    default:
      g_assert_not_reached ();
  }

  effects->current_effect = effect_type;
}

/* init resources that need a gl context */
static gboolean
gst_gl_effects_gl_start (GstGLBaseFilter * base_filter)
{
  GstGLEffects *effects = GST_GL_EFFECTS (base_filter);
  GstGLFilter *filter = GST_GL_FILTER (base_filter);
  GstGLContext *context = base_filter->context;
  GstGLBaseMemoryAllocator *base_alloc;
  GstGLAllocationParams *params;
  gint i;

  if (!GST_GL_BASE_FILTER_CLASS (parent_class)->gl_start (base_filter))
    return FALSE;

  base_alloc = (GstGLBaseMemoryAllocator *)
      gst_allocator_find (GST_GL_MEMORY_ALLOCATOR_NAME);
  params =
      (GstGLAllocationParams *) gst_gl_video_allocation_params_new (context,
      NULL, &filter->out_info, 0, NULL, GST_GL_TEXTURE_TARGET_2D, GST_GL_RGBA);

  for (i = 0; i < NEEDED_TEXTURES; i++) {
    if (effects->midtexture[i])
      gst_memory_unref (GST_MEMORY_CAST (effects->midtexture[i]));

    effects->midtexture[i] =
        (GstGLMemory *) gst_gl_base_memory_alloc (base_alloc, params);
  }

  gst_object_unref (base_alloc);
  gst_gl_allocation_params_free (params);

  return TRUE;
}

/* free resources that need a gl context */
static void
gst_gl_effects_gl_stop (GstGLBaseFilter * base_filter)
{
  GstGLEffects *effects = GST_GL_EFFECTS (base_filter);
  const GstGLFuncs *gl = base_filter->context->gl_vtable;
  gint i = 0;

  for (i = 0; i < NEEDED_TEXTURES; i++) {
    gst_memory_unref (GST_MEMORY_CAST (effects->midtexture[i]));
  }

  for (i = 0; i < GST_GL_EFFECTS_N_CURVES; i++) {
    gl->DeleteTextures (1, &effects->curve[i]);
    effects->curve[i] = 0;
  }

  GST_GL_BASE_FILTER_CLASS (parent_class)->gl_stop (base_filter);
}

static void
gst_gl_effects_class_init (GstGLEffectsClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_gl_filter_add_rgba_pad_templates (GST_GL_FILTER_CLASS (klass));

  GST_BASE_TRANSFORM_CLASS (klass)->start = gst_gl_effects_init_resources;
  GST_BASE_TRANSFORM_CLASS (klass)->stop = gst_gl_effects_reset_resources;

  GST_GL_BASE_FILTER_CLASS (klass)->gl_start = gst_gl_effects_gl_start;
  GST_GL_BASE_FILTER_CLASS (klass)->gl_stop = gst_gl_effects_gl_stop;

  GST_GL_FILTER_CLASS (klass)->filter_texture = gst_gl_effects_filter_texture;
  GST_GL_FILTER_CLASS (klass)->init_fbo = gst_gl_effects_on_init_gl_context;

  klass->filter_descriptor = NULL;

  gst_element_class_set_metadata (element_class,
      "Gstreamer OpenGL Effects", "Filter/Effect/Video",
      "GL Shading Language effects",
      "Filippo Argiolas <filippo.argiolas@gmail.com>");

  GST_GL_BASE_FILTER_CLASS (klass)->supported_gl_api =
      GST_GL_API_OPENGL | GST_GL_API_GLES2 | GST_GL_API_OPENGL3;

  gst_type_mark_as_plugin_api (GST_TYPE_GL_EFFECTS_EFFECT, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_GL_EFFECTS, 0);
}

static void
gst_gl_effects_filter_class_init (GstGLEffectsClass * klass,
    const GstGLEffectsFilterDescriptor * filter_descriptor)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  klass->filter_descriptor = filter_descriptor;

  gobject_class->set_property = gst_gl_effects_set_property;
  gobject_class->get_property = gst_gl_effects_get_property;

  /* if filterDescriptor is null it's a generic gleffects */
  if (!filter_descriptor) {
    g_object_class_install_property (gobject_class,
        PROP_EFFECT,
        g_param_spec_enum ("effect",
            "Effect",
            "Select which effect apply to GL video texture",
            GST_TYPE_GL_EFFECTS_EFFECT,
            GST_GL_EFFECT_IDENTITY,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  } else {
    gchar *description = g_strdup_printf ("GL Shading Language effects - %s",
        filter_descriptor->filter_longname);

    gst_element_class_set_metadata (element_class,
        filter_descriptor->filter_longname, "Filter/Effect/Video",
        description, "Filippo Argiolas <filippo.argiolas@gmail.com>");

    g_free (description);
  }

  g_object_class_install_property (gobject_class,
      PROP_HSWAP,
      g_param_spec_boolean ("hswap",
          "Horizontal Swap",
          "Switch video texture left to right, useful with webcams",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* FIXME: make it work on every effect */
  if (gst_gl_effects_filters_is_property_supported (filter_descriptor,
          PROP_INVERT)) {
    g_object_class_install_property (gobject_class, PROP_INVERT,
        g_param_spec_boolean ("invert", "Invert the colors for sobel effect",
            "Invert colors to get dark edges on bright background when using sobel effect",
            FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  }
}

static void
set_horizontal_swap (GstGLEffects * effects)
{
#if GST_GL_HAVE_OPENGL
  GstGLContext *context = GST_GL_BASE_FILTER (effects)->context;
  const GstGLFuncs *gl = context->gl_vtable;

  if (gst_gl_context_get_gl_api (context) & GST_GL_API_OPENGL) {
    const gfloat mirrormatrix[16] = {
      -1.0, 0.0, 0.0, 0.0,
      0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0
    };

    gl->MatrixMode (GL_MODELVIEW);
    gl->LoadMatrixf (mirrormatrix);
  }
#endif
}

static void
gst_gl_effects_init (GstGLEffects * effects)
{
  effects->horizontal_swap = FALSE;
  effects->invert = FALSE;
  effects->effect = gst_gl_effects_identity;
}

static void
gst_gl_effects_filter_init (GstGLEffects * effects)
{
  gst_gl_effects_set_effect (effects,
      GST_GL_EFFECTS_GET_CLASS (effects)->filter_descriptor->effect);
}

static void
gst_gl_effects_ghash_func_clean (gpointer key, gpointer value, gpointer data)
{
  GstGLShader *shader = (GstGLShader *) value;

  gst_object_unref (shader);

  value = NULL;
}

static gboolean
gst_gl_effects_reset_resources (GstBaseTransform * trans)
{
  GstGLEffects *effects = GST_GL_EFFECTS (trans);

  /* release shaders in the gl thread */
  g_hash_table_foreach (effects->shaderstable, gst_gl_effects_ghash_func_clean,
      effects);

  /* clean the htable without calling values destructors
   * because shaders have been released in the glthread
   * through the foreach func */
  g_hash_table_unref (effects->shaderstable);
  effects->shaderstable = NULL;

  return GST_BASE_TRANSFORM_CLASS (parent_class)->stop (trans);
}

static void
gst_gl_effects_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGLEffects *effects = GST_GL_EFFECTS (object);

  switch (prop_id) {
    case PROP_EFFECT:
      gst_gl_effects_set_effect (effects, g_value_get_enum (value));
      break;
    case PROP_HSWAP:
      effects->horizontal_swap = g_value_get_boolean (value);
      break;
    case PROP_INVERT:
      effects->invert = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_effects_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGLEffects *effects = GST_GL_EFFECTS (object);

  switch (prop_id) {
    case PROP_EFFECT:
      g_value_set_enum (value, effects->current_effect);
      break;
    case PROP_HSWAP:
      g_value_set_boolean (value, effects->horizontal_swap);
      break;
    case PROP_INVERT:
      g_value_set_boolean (value, effects->invert);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_gl_effects_init_resources (GstBaseTransform * trans)
{
  GstGLEffects *effects = GST_GL_EFFECTS (trans);
  gint i;

  effects->shaderstable = g_hash_table_new (g_str_hash, g_str_equal);

  for (i = 0; i < NEEDED_TEXTURES; i++) {
    effects->midtexture[i] = 0;
  }
  for (i = 0; i < GST_GL_EFFECTS_N_CURVES; i++) {
    effects->curve[i] = 0;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->start (trans);
}

static gboolean
gst_gl_effects_on_init_gl_context (GstGLFilter * filter)
{
  return TRUE;
}

static gboolean
gst_gl_effects_filter_texture (GstGLFilter * filter, GstGLMemory * in_tex,
    GstGLMemory * out_tex)
{
  GstGLEffects *effects = GST_GL_EFFECTS (filter);

  effects->intexture = in_tex;
  effects->outtexture = out_tex;

  if (effects->horizontal_swap == TRUE)
    set_horizontal_swap (effects);

  effects->effect (effects);

  return TRUE;
}

GstGLShader *
gst_gl_effects_get_fragment_shader (GstGLEffects * effects,
    const gchar * shader_name, const gchar * shader_source_gles2)
{
  GstGLShader *shader = NULL;
  GstGLFilter *filter = GST_GL_FILTER (effects);
  GstGLContext *context = GST_GL_BASE_FILTER (filter)->context;

  shader = g_hash_table_lookup (effects->shaderstable, shader_name);

  if (!shader) {
    GError *error = NULL;
    const gchar *frag_strs[2];

    frag_strs[0] =
        gst_gl_shader_string_get_highest_precision (context,
        GST_GLSL_VERSION_NONE,
        GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY);
    frag_strs[1] = shader_source_gles2;

    if (!(shader = gst_gl_shader_new_link_with_stages (context, &error,
                gst_glsl_stage_new_default_vertex (context),
                gst_glsl_stage_new_with_strings (context, GL_FRAGMENT_SHADER,
                    GST_GLSL_VERSION_NONE,
                    GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY, 2,
                    frag_strs), NULL))) {
      GST_ELEMENT_ERROR (effects, RESOURCE, NOT_FOUND,
          ("Failed to initialize %s shader", shader_name), (NULL));
    }

    filter->draw_attr_position_loc =
        gst_gl_shader_get_attribute_location (shader, "a_position");
    filter->draw_attr_texture_loc =
        gst_gl_shader_get_attribute_location (shader, "a_texcoord");
  }

  g_hash_table_insert (effects->shaderstable, (gchar *) shader_name, shader);

  return shader;
}

static const GstGLEffectsFilterDescriptor *
gst_gl_effects_filters_supported_properties (void)
{
  /* Horizontal swap property is supported by all filters */
  static const GstGLEffectsFilterDescriptor effects[] = {
    {GST_GL_EFFECT_SOBEL, PROP_INVERT, NULL},
    {GST_GL_EFFECT_LAPLACIAN, PROP_INVERT, NULL},
    {0, 0, NULL}
  };
  return effects;
}

static inline gboolean
gst_gl_effects_filters_is_property_supported (const GstGLEffectsFilterDescriptor
    * descriptor, gint property)
{
  /* generic filter (NULL descriptor) supports all properties */
  return !descriptor || (descriptor->supported_properties & property);
}

static const GstGLEffectsFilterDescriptor *
gst_gl_effects_filters_descriptors (void)
{
  static GstGLEffectsFilterDescriptor *descriptors = NULL;
  if (!descriptors) {
    const GEnumValue *e;
    const GEnumValue *effect = gst_gl_effects_get_effects ();
    const GstGLEffectsFilterDescriptor *defined;
    guint n_filters = 0, i;

    for (e = effect; NULL != e->value_nick; ++e, ++n_filters) {
    }

    descriptors = g_new0 (GstGLEffectsFilterDescriptor, n_filters + 1);
    for (i = 0; i < n_filters; ++i, ++effect) {
      descriptors[i].effect = effect->value;
      descriptors[i].filter_name = effect->value_nick;
      descriptors[i].filter_longname = effect->value_name;
    }

    for (defined = gst_gl_effects_filters_supported_properties ();
        0 != defined->supported_properties; ++defined) {

      for (i = 0; i < n_filters; ++i) {
        if (descriptors[i].effect == defined->effect) {
          descriptors[i].supported_properties = defined->supported_properties;
          break;
        }
      }
      if (i >= n_filters) {
        GST_WARNING ("Could not match gstgleffects-%s descriptor",
            defined->filter_name);
      }
    }
  }
  return descriptors;
}

gboolean
gst_gl_effects_register_filters (GstPlugin * plugin, GstRank rank)
{
  static gsize registered = 0;

  if (g_once_init_enter (&registered)) {
    GTypeInfo info = {
      sizeof (GstGLEffectsClass),
      NULL,
      NULL,
      (GClassInitFunc) gst_gl_effects_filter_class_init,
      NULL,
      NULL,
      sizeof (GstGLEffects),
      0,
      NULL
    };
    GType generic_type =
        g_type_register_static (GST_TYPE_GL_EFFECTS, "GstGLEffectsGeneric",
        &info, 0);

    if (gst_element_register (plugin, "gleffects", rank, generic_type)) {
      const GstGLEffectsFilterDescriptor *filters;
      for (filters = gst_gl_effects_filters_descriptors ();
          NULL != filters->filter_name; ++filters) {
        gchar *name = g_strdup_printf ("gleffects_%s", filters->filter_name);
        GTypeInfo info = {
          sizeof (GstGLEffectsClass),
          NULL,
          NULL,
          (GClassInitFunc) gst_gl_effects_filter_class_init,
          NULL,
          filters,
          sizeof (GstGLEffects),
          0,
          (GInstanceInitFunc) gst_gl_effects_filter_init
        };
        GType type =
            g_type_register_static (GST_TYPE_GL_EFFECTS, name, &info, 0);
        if (!gst_element_register (plugin, name, rank, type)) {
          GST_WARNING ("Could not register %s", name);
        }
        g_free (name);
      }
    }
    g_once_init_leave (&registered, generic_type);
  }
  return registered;
}
