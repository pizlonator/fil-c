/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) 2003 Ronald Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2005-2012 David Schleef <ds@schleef.org>
 * Copyright (C) 2022 Thibault Saunier <tsaunier@igalia.com>
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
 * SECTION:element-videoconvertscale
 * @title: videoconvertscale
 *
 * This element resizes video frames and allows changing colorspace. By default
 * the element will try to negotiate to the same size on the source and sinkpad
 * so that no scaling is needed. It is therefore safe to insert this element in
 * a pipeline to get more robust behaviour without any cost if no scaling is
 * needed.
 *
 * This element supports a wide range of color spaces including various YUV and
 * RGB formats and is therefore generally able to operate anywhere in a
 * pipeline.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v filesrc location=videotestsrc.ogg ! oggdemux ! theoradec ! videoconvertscale ! autovideosink
 * ]|
 *  Decode an Ogg/Theora and display the video. If the video sink chosen
 * cannot perform scaling, the video scaling will be performed by videoconvertscale
 * when you resize the video window.
 * To create the test Ogg/Theora file refer to the documentation of theoraenc.
 * |[
 * gst-launch-1.0 -v filesrc location=videotestsrc.ogg ! oggdemux ! theoradec ! videoconvertscale ! video/x-raw,width=100 ! autovideosink
 * ]|
 *  Decode an Ogg/Theora and display the video with a width of 100.
 *
 * Since: 1.22
 */

/*
 * Formulas for PAR, DAR, width and height relations:
 *
 * dar_n   w   par_n
 * ----- = - * -----
 * dar_d   h   par_d
 *
 * par_n    h   dar_n
 * ----- =  - * -----
 * par_d    w   dar_d
 *
 *         dar_n   par_d
 * w = h * ----- * -----
 *         dar_d   par_n
 *
 *         dar_d   par_n
 * h = w * ----- * -----
 *         dar_n   par_d
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <math.h>

#include <gst/video/gstvideometa.h>
#include <gst/video/gstvideopool.h>

#include "gstvideoconvertscale.h"

typedef struct
{
  /* properties */
  GstVideoScaleMethod method;
  gboolean add_borders;
  double sharpness;
  double sharpen;
  int submethod;
  double envelope;
  gint n_threads;
  GstVideoDitherMethod dither;
  guint dither_quantization;
  GstVideoResamplerMethod chroma_resampler;
  GstVideoAlphaMode alpha_mode;
  GstVideoChromaMode chroma_mode;
  GstVideoMatrixMode matrix_mode;
  GstVideoGammaMode gamma_mode;
  GstVideoPrimariesMode primaries_mode;
  gdouble alpha_value;

  GstVideoConverter *convert;

  GstStructure *converter_config;
  gboolean converter_config_changed;

  gint borders_h;
  gint borders_w;
} GstVideoConvertScalePrivate;

#define gst_video_convert_scale_parent_class parent_class
G_DEFINE_TYPE_WITH_PRIVATE (GstVideoConvertScale, gst_video_convert_scale,
    GST_TYPE_VIDEO_FILTER);
GST_ELEMENT_REGISTER_DEFINE (videoconvertscale, "videoconvertscale",
    GST_RANK_SECONDARY, GST_TYPE_VIDEO_CONVERT_SCALE);

#define PRIV(self) gst_video_convert_scale_get_instance_private(((GstVideoConvertScale*) self))

#define GST_CAT_DEFAULT video_convertscale_debug
GST_DEBUG_CATEGORY_STATIC (video_convertscale_debug);
GST_DEBUG_CATEGORY_STATIC (CAT_PERFORMANCE);

#define DEFAULT_PROP_METHOD       GST_VIDEO_SCALE_BILINEAR
#define DEFAULT_PROP_ADD_BORDERS  TRUE
#define DEFAULT_PROP_SHARPNESS    1.0
#define DEFAULT_PROP_SHARPEN      0.0
#define DEFAULT_PROP_DITHER      GST_VIDEO_DITHER_BAYER
#define DEFAULT_PROP_ENVELOPE     2.0
#define DEFAULT_PROP_DITHER_QUANTIZATION 1
#define DEFAULT_PROP_CHROMA_RESAMPLER	GST_VIDEO_RESAMPLER_METHOD_LINEAR
#define DEFAULT_PROP_ALPHA_MODE GST_VIDEO_ALPHA_MODE_COPY
#define DEFAULT_PROP_ALPHA_VALUE 1.0
#define DEFAULT_PROP_CHROMA_MODE GST_VIDEO_CHROMA_MODE_FULL
#define DEFAULT_PROP_MATRIX_MODE GST_VIDEO_MATRIX_MODE_FULL
#define DEFAULT_PROP_GAMMA_MODE GST_VIDEO_GAMMA_MODE_NONE
#define DEFAULT_PROP_PRIMARIES_MODE GST_VIDEO_PRIMARIES_MODE_NONE
#define DEFAULT_PROP_N_THREADS 1

static GQuark _colorspace_quark;

enum
{
  PROP_0,
  PROP_METHOD,
  PROP_ADD_BORDERS,
  PROP_SHARPNESS,
  PROP_SHARPEN,
  PROP_DITHER,
  PROP_SUBMETHOD,
  PROP_ENVELOPE,
  PROP_N_THREADS,
  PROP_DITHER_QUANTIZATION,
  PROP_CHROMA_RESAMPLER,
  PROP_ALPHA_MODE,
  PROP_ALPHA_VALUE,
  PROP_CHROMA_MODE,
  PROP_MATRIX_MODE,
  PROP_GAMMA_MODE,
  PROP_PRIMARIES_MODE,
  PROP_CONVERTER_CONFIG,
};

#undef GST_VIDEO_SIZE_RANGE
#define GST_VIDEO_SIZE_RANGE "(int) [ 1, 32767]"

/* FIXME: add v210 support
 * FIXME: add v216 support
 * FIXME: add UYVP support
 * FIXME: add A420 support
 * FIXME: add YUV9 support
 * FIXME: add YVU9 support
 * FIXME: add IYU1 support
 * FIXME: add r210 support
 */

#define GST_VIDEO_FORMATS GST_VIDEO_FORMATS_ALL

static GstStaticCaps gst_video_convert_scale_format_caps =
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (GST_VIDEO_FORMATS) ";"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("ANY", GST_VIDEO_FORMATS_ANY));

static GQuark _size_quark;
static GQuark _scale_quark;

#define GST_TYPE_VIDEO_SCALE_METHOD (gst_video_scale_method_get_type())
static GType
gst_video_scale_method_get_type (void)
{
  static GType video_scale_method_type = 0;

  static const GEnumValue video_scale_methods[] = {
    {GST_VIDEO_SCALE_NEAREST, "Nearest Neighbour", "nearest-neighbour"},
    {GST_VIDEO_SCALE_BILINEAR, "Bilinear (2-tap)", "bilinear"},
    {GST_VIDEO_SCALE_4TAP, "4-tap Sinc", "4-tap"},
    {GST_VIDEO_SCALE_LANCZOS, "Lanczos", "lanczos"},
    {GST_VIDEO_SCALE_BILINEAR2, "Bilinear (multi-tap)", "bilinear2"},
    {GST_VIDEO_SCALE_SINC, "Sinc (multi-tap)", "sinc"},
    {GST_VIDEO_SCALE_HERMITE, "Hermite (multi-tap)", "hermite"},
    {GST_VIDEO_SCALE_SPLINE, "Spline (multi-tap)", "spline"},
    {GST_VIDEO_SCALE_CATROM, "Catmull-Rom (multi-tap)", "catrom"},
    {GST_VIDEO_SCALE_MITCHELL, "Mitchell (multi-tap)", "mitchell"},
    {0, NULL, NULL},
  };

  if (!video_scale_method_type) {
    video_scale_method_type =
        g_enum_register_static ("GstVideoScaleMethod", video_scale_methods);
  }
  return video_scale_method_type;
}

static GstCaps *
gst_video_convert_scale_get_capslist (void)
{
  static GstCaps *caps = NULL;
  static gsize inited = 0;

  if (g_once_init_enter (&inited)) {
    caps = gst_static_caps_get (&gst_video_convert_scale_format_caps);
    g_once_init_leave (&inited, 1);
  }
  return caps;
}

static GstPadTemplate *
gst_video_convert_scale_src_template_factory (void)
{
  return gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
      gst_video_convert_scale_get_capslist ());
}

static GstPadTemplate *
gst_video_convert_scale_sink_template_factory (void)
{
  return gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
      gst_video_convert_scale_get_capslist ());
}


static void gst_video_convert_scale_finalize (GstVideoConvertScale * self);
static gboolean gst_video_convert_scale_src_event (GstBaseTransform * trans,
    GstEvent * event);

/* base transform vmethods */
static GstCaps *gst_video_convert_scale_transform_caps (GstBaseTransform *
    trans, GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static GstCaps *gst_video_convert_scale_fixate_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);
static gboolean gst_video_convert_scale_transform_meta (GstBaseTransform *
    trans, GstBuffer * outbuf, GstMeta * meta, GstBuffer * inbuf);

static gboolean gst_video_convert_scale_set_info (GstVideoFilter * filter,
    GstCaps * in, GstVideoInfo * in_info, GstCaps * out,
    GstVideoInfo * out_info);
static GstFlowReturn gst_video_convert_scale_transform_frame (GstVideoFilter *
    filter, GstVideoFrame * in, GstVideoFrame * out);

static void gst_video_convert_scale_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_video_convert_scale_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static GstCapsFeatures *features_format_interlaced,
    *features_format_interlaced_sysmem;

static gboolean
gst_video_convert_scale_filter_meta (GstBaseTransform * trans, GstQuery * query,
    GType api, const GstStructure * params)
{
  /* This element cannot passthrough the crop meta, because it would convert the
   * wrong sub-region of the image, and worst, our output image may not be large
   * enough for the crop to be applied later */
  if (api == GST_VIDEO_CROP_META_API_TYPE)
    return FALSE;

  /* propose all other metadata upstream */
  return TRUE;
}

static void
gst_video_convert_scale_class_init (GstVideoConvertScaleClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *element_class = (GstElementClass *) klass;
  GstBaseTransformClass *trans_class = (GstBaseTransformClass *) klass;
  GstVideoFilterClass *filter_class = (GstVideoFilterClass *) klass;

  GST_DEBUG_CATEGORY_INIT (video_convertscale_debug, "videoconvertscale", 0,
      "videoconvertscale element");
  GST_DEBUG_CATEGORY_GET (CAT_PERFORMANCE, "GST_PERFORMANCE");

  features_format_interlaced =
      gst_caps_features_new (GST_CAPS_FEATURE_FORMAT_INTERLACED, NULL);
  features_format_interlaced_sysmem =
      gst_caps_features_copy (features_format_interlaced);
  gst_caps_features_add (features_format_interlaced_sysmem,
      GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY);

  _colorspace_quark = g_quark_from_static_string ("colorspace");

  gobject_class->finalize =
      (GObjectFinalizeFunc) gst_video_convert_scale_finalize;
  gobject_class->set_property = gst_video_convert_scale_set_property;
  gobject_class->get_property = gst_video_convert_scale_get_property;

  g_object_class_install_property (gobject_class, PROP_METHOD,
      g_param_spec_enum ("method", "method", "method",
          GST_TYPE_VIDEO_SCALE_METHOD, DEFAULT_PROP_METHOD,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ADD_BORDERS,
      g_param_spec_boolean ("add-borders", "Add Borders",
          "Add black borders if necessary to keep the display aspect ratio",
          DEFAULT_PROP_ADD_BORDERS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SHARPNESS,
      g_param_spec_double ("sharpness", "Sharpness",
          "Sharpness of filter", 0.5, 1.5, DEFAULT_PROP_SHARPNESS,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SHARPEN,
      g_param_spec_double ("sharpen", "Sharpen",
          "Sharpening", 0.0, 1.0, DEFAULT_PROP_SHARPEN,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DITHER,
      g_param_spec_enum ("dither", "Dither", "Apply dithering while converting",
          gst_video_dither_method_get_type (), DEFAULT_PROP_DITHER,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_ENVELOPE,
      g_param_spec_double ("envelope", "Envelope",
          "Size of filter envelope", 1.0, 5.0, DEFAULT_PROP_ENVELOPE,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_N_THREADS,
      g_param_spec_uint ("n-threads", "Threads",
          "Maximum number of threads to use", 0, G_MAXUINT,
          DEFAULT_PROP_N_THREADS,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DITHER_QUANTIZATION,
      g_param_spec_uint ("dither-quantization", "Dither Quantize",
          "Quantizer to use", 0, G_MAXUINT, DEFAULT_PROP_DITHER_QUANTIZATION,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_CHROMA_RESAMPLER,
      g_param_spec_enum ("chroma-resampler", "Chroma resampler",
          "Chroma resampler method", gst_video_resampler_method_get_type (),
          DEFAULT_PROP_CHROMA_RESAMPLER,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_ALPHA_MODE,
      g_param_spec_enum ("alpha-mode", "Alpha Mode",
          "Alpha Mode to use", gst_video_alpha_mode_get_type (),
          DEFAULT_PROP_ALPHA_MODE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_ALPHA_VALUE,
      g_param_spec_double ("alpha-value", "Alpha Value",
          "Alpha Value to use", 0.0, 1.0,
          DEFAULT_PROP_ALPHA_VALUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_CHROMA_MODE,
      g_param_spec_enum ("chroma-mode", "Chroma Mode", "Chroma Resampling Mode",
          gst_video_chroma_mode_get_type (), DEFAULT_PROP_CHROMA_MODE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MATRIX_MODE,
      g_param_spec_enum ("matrix-mode", "Matrix Mode", "Matrix Conversion Mode",
          gst_video_matrix_mode_get_type (), DEFAULT_PROP_MATRIX_MODE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_GAMMA_MODE,
      g_param_spec_enum ("gamma-mode", "Gamma Mode", "Gamma Conversion Mode",
          gst_video_gamma_mode_get_type (), DEFAULT_PROP_GAMMA_MODE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PRIMARIES_MODE,
      g_param_spec_enum ("primaries-mode", "Primaries Mode",
          "Primaries Conversion Mode", gst_video_primaries_mode_get_type (),
          DEFAULT_PROP_PRIMARIES_MODE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstVideoConvertScale:converter-config:
   *
   * A #GstStructure describing the configuration that should be used. This
   * configuration, if set, takes precedence over the other similar conversion
   * properties.
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class,
      PROP_CONVERTER_CONFIG, g_param_spec_boxed ("converter-config",
          "Converter configuration",
          "A GstStructure describing the configuration that should be used."
          " This configuration, if set, takes precedence over the other similar conversion properties.",
          GST_TYPE_STRUCTURE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));


  gst_element_class_set_static_metadata (element_class,
      "Video colorspace converter and scaler",
      "Filter/Converter/Video/Scaler/Colorspace",
      "Resizes video and converts from one colorspace to another",
      "Wim Taymans <wim.taymans@gmail.com>");

  gst_element_class_add_pad_template (element_class,
      gst_video_convert_scale_sink_template_factory ());
  gst_element_class_add_pad_template (element_class,
      gst_video_convert_scale_src_template_factory ());

  _size_quark = g_quark_from_static_string (GST_META_TAG_VIDEO_SIZE_STR);
  _scale_quark = gst_video_meta_transform_scale_get_quark ();

  gst_type_mark_as_plugin_api (GST_TYPE_VIDEO_SCALE_METHOD, 0);
  trans_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_video_convert_scale_transform_caps);
  trans_class->fixate_caps =
      GST_DEBUG_FUNCPTR (gst_video_convert_scale_fixate_caps);
  trans_class->filter_meta =
      GST_DEBUG_FUNCPTR (gst_video_convert_scale_filter_meta);
  trans_class->src_event =
      GST_DEBUG_FUNCPTR (gst_video_convert_scale_src_event);
  trans_class->transform_meta =
      GST_DEBUG_FUNCPTR (gst_video_convert_scale_transform_meta);

  filter_class->set_info = GST_DEBUG_FUNCPTR (gst_video_convert_scale_set_info);
  filter_class->transform_frame =
      GST_DEBUG_FUNCPTR (gst_video_convert_scale_transform_frame);

  klass->converts = TRUE;
  klass->scales = TRUE;

  gst_type_mark_as_plugin_api (GST_TYPE_VIDEO_CONVERT_SCALE, 0);
}

static void
gst_video_convert_scale_init (GstVideoConvertScale * self)
{
  GstVideoConvertScalePrivate *priv = PRIV (self);

  priv->method = DEFAULT_PROP_METHOD;
  priv->add_borders = DEFAULT_PROP_ADD_BORDERS;
  priv->sharpness = DEFAULT_PROP_SHARPNESS;
  priv->sharpen = DEFAULT_PROP_SHARPEN;
  priv->envelope = DEFAULT_PROP_ENVELOPE;
  priv->n_threads = DEFAULT_PROP_N_THREADS;
  priv->dither = DEFAULT_PROP_DITHER;
  priv->dither_quantization = DEFAULT_PROP_DITHER_QUANTIZATION;
  priv->chroma_resampler = DEFAULT_PROP_CHROMA_RESAMPLER;
  priv->alpha_mode = DEFAULT_PROP_ALPHA_MODE;
  priv->alpha_value = DEFAULT_PROP_ALPHA_VALUE;
  priv->chroma_mode = DEFAULT_PROP_CHROMA_MODE;
  priv->matrix_mode = DEFAULT_PROP_MATRIX_MODE;
  priv->gamma_mode = DEFAULT_PROP_GAMMA_MODE;
  priv->primaries_mode = DEFAULT_PROP_PRIMARIES_MODE;

  priv->converter_config = NULL;
  priv->converter_config_changed = FALSE;
}

static void
gst_video_convert_scale_finalize (GstVideoConvertScale * self)
{
  GstVideoConvertScalePrivate *priv = PRIV (self);

  if (priv->convert)
    gst_video_converter_free (priv->convert);

  if (priv->converter_config)
    gst_structure_free (priv->converter_config);
  priv->converter_config = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (G_OBJECT (self));
}

static void
gst_video_convert_scale_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVideoConvertScalePrivate *priv = PRIV (object);

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
    case PROP_METHOD:
      priv->method = g_value_get_enum (value);
      break;
    case PROP_ADD_BORDERS:
      priv->add_borders = g_value_get_boolean (value);
      GST_OBJECT_UNLOCK (object);

      gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM_CAST (object));
      return;
    case PROP_SHARPNESS:
      priv->sharpness = g_value_get_double (value);
      break;
    case PROP_SHARPEN:
      priv->sharpen = g_value_get_double (value);
      break;
    case PROP_SUBMETHOD:
      priv->submethod = g_value_get_int (value);
      break;
    case PROP_ENVELOPE:
      priv->envelope = g_value_get_double (value);
      break;
    case PROP_N_THREADS:
      priv->n_threads = g_value_get_uint (value);
      break;
    case PROP_DITHER:
      priv->dither = g_value_get_enum (value);
      break;
    case PROP_CHROMA_RESAMPLER:
      priv->chroma_resampler = g_value_get_enum (value);
      break;
    case PROP_ALPHA_MODE:
      priv->alpha_mode = g_value_get_enum (value);
      break;
    case PROP_ALPHA_VALUE:
      priv->alpha_value = g_value_get_double (value);
      break;
    case PROP_CHROMA_MODE:
      priv->chroma_mode = g_value_get_enum (value);
      break;
    case PROP_MATRIX_MODE:
      priv->matrix_mode = g_value_get_enum (value);
      break;
    case PROP_GAMMA_MODE:
      priv->gamma_mode = g_value_get_enum (value);
      break;
    case PROP_PRIMARIES_MODE:
      priv->primaries_mode = g_value_get_enum (value);
      break;
    case PROP_DITHER_QUANTIZATION:
      priv->dither_quantization = g_value_get_uint (value);
      break;
    case PROP_CONVERTER_CONFIG:
      if (priv->converter_config)
        gst_structure_free (priv->converter_config);
      priv->converter_config = g_value_dup_boxed (value);
      priv->converter_config_changed = TRUE;
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (object);
}

static void
gst_video_convert_scale_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstVideoConvertScalePrivate *priv = PRIV (object);

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
    case PROP_METHOD:
      g_value_set_enum (value, priv->method);
      break;
    case PROP_ADD_BORDERS:
      g_value_set_boolean (value, priv->add_borders);
      break;
    case PROP_SHARPNESS:
      g_value_set_double (value, priv->sharpness);
      break;
    case PROP_SHARPEN:
      g_value_set_double (value, priv->sharpen);
      break;
    case PROP_SUBMETHOD:
      g_value_set_int (value, priv->submethod);
      break;
    case PROP_ENVELOPE:
      g_value_set_double (value, priv->envelope);
      break;
    case PROP_N_THREADS:
      g_value_set_uint (value, priv->n_threads);
      break;
    case PROP_DITHER:
      g_value_set_enum (value, priv->dither);
      break;
    case PROP_CHROMA_RESAMPLER:
      g_value_set_enum (value, priv->chroma_resampler);
      break;
    case PROP_ALPHA_MODE:
      g_value_set_enum (value, priv->alpha_mode);
      break;
    case PROP_ALPHA_VALUE:
      g_value_set_double (value, priv->alpha_value);
      break;
    case PROP_CHROMA_MODE:
      g_value_set_enum (value, priv->chroma_mode);
      break;
    case PROP_MATRIX_MODE:
      g_value_set_enum (value, priv->matrix_mode);
      break;
    case PROP_GAMMA_MODE:
      g_value_set_enum (value, priv->gamma_mode);
      break;
    case PROP_PRIMARIES_MODE:
      g_value_set_enum (value, priv->primaries_mode);
      break;
    case PROP_DITHER_QUANTIZATION:
      g_value_set_uint (value, priv->dither_quantization);
      break;
    case PROP_CONVERTER_CONFIG:
      g_value_set_boxed (value, priv->converter_config);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (object);
}

static GstCaps *
gst_video_convert_caps_remove_format_and_rangify_size_info (GstVideoConvertScale
    * self, GstCaps * caps)
{
  GstVideoConvertScaleClass *klass = GST_VIDEO_CONVERT_SCALE_GET_CLASS (self);
  GstCaps *ret;
  GstStructure *structure;
  GstCapsFeatures *features;
  gint i, n;

  ret = gst_caps_new_empty ();

  n = gst_caps_get_size (caps);
  for (i = 0; i < n; i++) {
    structure = gst_caps_get_structure (caps, i);
    features = gst_caps_get_features (caps, i);

    /* If this is already expressed by the existing caps
     * skip this structure */
    if (i > 0 && gst_caps_is_subset_structure_full (ret, structure, features))
      continue;

    structure = gst_structure_copy (structure);
    /* Only remove format info for the cases when we can actually convert */
    if (!gst_caps_features_is_any (features)
        && (gst_caps_features_is_equal (features,
                GST_CAPS_FEATURES_MEMORY_SYSTEM_MEMORY)
            || gst_caps_features_is_equal (features, features_format_interlaced)
            || gst_caps_features_is_equal (features,
                features_format_interlaced_sysmem))) {
      if (klass->scales) {
        gst_structure_set (structure, "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
            "height", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);
        /* if pixel aspect ratio, make a range of it */
        if (gst_structure_has_field (structure, "pixel-aspect-ratio")) {
          gst_structure_set (structure, "pixel-aspect-ratio",
              GST_TYPE_FRACTION_RANGE, 1, G_MAXINT, G_MAXINT, 1, NULL);
        }
      }

      if (klass->converts) {
        gst_structure_remove_fields (structure, "format", "colorimetry",
            "chroma-site", NULL);
      }
    }
    gst_caps_append_structure_full (ret, structure,
        gst_caps_features_copy (features));
  }

  return ret;
}

static GstCaps *
gst_video_convert_scale_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstVideoConvertScale *self = GST_VIDEO_CONVERT_SCALE (trans);
  GstCaps *ret;

  GST_DEBUG_OBJECT (trans,
      "Transforming caps %" GST_PTR_FORMAT " in direction %s", caps,
      (direction == GST_PAD_SINK) ? "sink" : "src");

  ret = gst_video_convert_caps_remove_format_and_rangify_size_info (self, caps);
  if (filter) {
    GstCaps *intersection;

    intersection =
        gst_caps_intersect_full (filter, ret, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (ret);
    ret = intersection;
  }

  return ret;
}

static gboolean
gst_video_convert_scale_transform_meta (GstBaseTransform * trans,
    GstBuffer * outbuf, GstMeta * meta, GstBuffer * inbuf)
{
  GstVideoFilter *videofilter = GST_VIDEO_FILTER (trans);
  const GstMetaInfo *info = meta->info;
  const gchar *const *tags;
  const gchar *const *curr = NULL;
  gboolean should_copy = TRUE;
  const gchar *const valid_tags[] = {
    GST_META_TAG_VIDEO_STR,
    GST_META_TAG_VIDEO_ORIENTATION_STR,
    GST_META_TAG_VIDEO_SIZE_STR,
    NULL
  };

  tags = gst_meta_api_type_get_tags (info->api);

  /* No specific tags, we are good to copy */
  if (!tags) {
    return TRUE;
  }

  if (gst_meta_api_type_has_tag (info->api, _colorspace_quark)) {
    /* don't copy colorspace specific metadata, FIXME, we need a MetaTransform
     * for the colorspace metadata. */
    return FALSE;
  }

  /* We are only changing size, we can preserve other metas tagged as
     orientation and colorspace */
  for (curr = tags; *curr; ++curr) {

    /* We dont handle any other tag */
    if (!g_strv_contains (valid_tags, *curr)) {
      should_copy = FALSE;
      break;
    }
  }

  /* Cant handle the tags in this meta, let the parent class handle it */
  if (!should_copy) {
    return GST_BASE_TRANSFORM_CLASS (parent_class)->transform_meta (trans,
        outbuf, meta, inbuf);
  }

  /* This meta is size sensitive, try to transform it accordingly */
  if (gst_meta_api_type_has_tag (info->api, _size_quark)) {
    GstVideoMetaTransform trans =
        { &videofilter->in_info, &videofilter->out_info };

    if (info->transform_func)
      info->transform_func (outbuf, meta, inbuf, _scale_quark, &trans);
    return FALSE;
  }

  /* No need to transform, we can safely copy this meta */
  return TRUE;
}

static GstStructure *
gst_video_convert_scale_get_converter_config (GstVideoConvertScale * self,
    GstVideoInfo * out_info)
{
  GstVideoConvertScalePrivate *priv = PRIV (self);
  return gst_structure_copy (priv->converter_config);
}

static gboolean
gst_video_convert_scale_set_info (GstVideoFilter * filter, GstCaps * in,
    GstVideoInfo * in_info, GstCaps * out, GstVideoInfo * out_info)
{
  GstVideoConvertScale *self = GST_VIDEO_CONVERT_SCALE (filter);
  GstVideoConvertScalePrivate *priv = PRIV (self);
  gint from_dar_n, from_dar_d, to_dar_n, to_dar_d;
  GstVideoInfo tmp_info;
  GstStructure *options;

  if (priv->convert) {
    gst_video_converter_free (priv->convert);
    priv->convert = NULL;
  }

  if (!gst_util_fraction_multiply (in_info->width,
          in_info->height, in_info->par_n, in_info->par_d, &from_dar_n,
          &from_dar_d)) {
    from_dar_n = from_dar_d = -1;
  }

  if (!gst_util_fraction_multiply (out_info->width,
          out_info->height, out_info->par_n, out_info->par_d, &to_dar_n,
          &to_dar_d)) {
    to_dar_n = to_dar_d = -1;
  }

  priv->borders_w = priv->borders_h = 0;
  if (to_dar_n != from_dar_n || to_dar_d != from_dar_d) {
    if (priv->add_borders) {
      gint n, d, to_h, to_w;

      if (from_dar_n != -1 && from_dar_d != -1
          && gst_util_fraction_multiply (from_dar_n, from_dar_d,
              out_info->par_d, out_info->par_n, &n, &d)) {
        to_h = gst_util_uint64_scale_int (out_info->width, d, n);
        if (to_h <= out_info->height) {
          priv->borders_h = out_info->height - to_h;
          priv->borders_w = 0;
        } else {
          to_w = gst_util_uint64_scale_int (out_info->height, n, d);
          g_assert (to_w <= out_info->width);
          priv->borders_h = 0;
          priv->borders_w = out_info->width - to_w;
        }
      } else {
        GST_WARNING_OBJECT (self, "Can't calculate borders");
      }
    } else {
      GST_WARNING_OBJECT (self, "Can't keep DAR!");
    }
  }

  /* if present, these must match */
  if (in_info->interlace_mode != out_info->interlace_mode)
    goto format_mismatch;

  if (priv->converter_config) {
    options = gst_video_convert_scale_get_converter_config (self, out_info);
    GST_DEBUG_OBJECT (self,
        "Using user-provided converter-config: %" GST_PTR_FORMAT, options);
    goto build_converter;
  }

  /* if the only thing different in the caps is the transfer function, and
   * we're converting between equivalent transfer functions and not
   * quantizing/dithering or adjusting alpha, then do passthrough */
  tmp_info = *in_info;
  tmp_info.colorimetry.transfer = out_info->colorimetry.transfer;

  gboolean need_dither = (priv->dither_quantization > 1)
      && (priv->dither != GST_VIDEO_DITHER_NONE);
  gboolean need_alpha = GST_VIDEO_INFO_HAS_ALPHA (out_info)
      && ((priv->alpha_mode == GST_VIDEO_ALPHA_MODE_SET) ||
      (priv->alpha_mode == GST_VIDEO_ALPHA_MODE_MULT
          && priv->alpha_value != 1.0));

  if (gst_video_info_is_equal (&tmp_info, out_info) &&
      gst_video_transfer_function_is_equivalent (in_info->colorimetry.transfer,
          in_info->finfo->bits, out_info->colorimetry.transfer,
          out_info->finfo->bits) && !need_dither && !need_alpha) {
    gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (filter), TRUE);
  } else {
    GST_CAT_DEBUG_OBJECT (CAT_PERFORMANCE, filter, "setup videoscaling");
    gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (filter), FALSE);

    options = gst_structure_new_empty ("videoconvertscale");

    switch (priv->method) {
      case GST_VIDEO_SCALE_NEAREST:
        gst_structure_set (options,
            GST_VIDEO_CONVERTER_OPT_RESAMPLER_METHOD,
            GST_TYPE_VIDEO_RESAMPLER_METHOD, GST_VIDEO_RESAMPLER_METHOD_NEAREST,
            NULL);
        break;
      case GST_VIDEO_SCALE_BILINEAR:
        gst_structure_set (options,
            GST_VIDEO_CONVERTER_OPT_RESAMPLER_METHOD,
            GST_TYPE_VIDEO_RESAMPLER_METHOD, GST_VIDEO_RESAMPLER_METHOD_LINEAR,
            GST_VIDEO_RESAMPLER_OPT_MAX_TAPS, G_TYPE_INT, 2, NULL);
        break;
      case GST_VIDEO_SCALE_4TAP:
        gst_structure_set (options,
            GST_VIDEO_CONVERTER_OPT_RESAMPLER_METHOD,
            GST_TYPE_VIDEO_RESAMPLER_METHOD, GST_VIDEO_RESAMPLER_METHOD_SINC,
            GST_VIDEO_RESAMPLER_OPT_MAX_TAPS, G_TYPE_INT, 4, NULL);
        break;
      case GST_VIDEO_SCALE_LANCZOS:
        gst_structure_set (options,
            GST_VIDEO_CONVERTER_OPT_RESAMPLER_METHOD,
            GST_TYPE_VIDEO_RESAMPLER_METHOD, GST_VIDEO_RESAMPLER_METHOD_LANCZOS,
            NULL);
        break;
      case GST_VIDEO_SCALE_BILINEAR2:
        gst_structure_set (options,
            GST_VIDEO_CONVERTER_OPT_RESAMPLER_METHOD,
            GST_TYPE_VIDEO_RESAMPLER_METHOD, GST_VIDEO_RESAMPLER_METHOD_LINEAR,
            NULL);
        break;
      case GST_VIDEO_SCALE_SINC:
        gst_structure_set (options,
            GST_VIDEO_CONVERTER_OPT_RESAMPLER_METHOD,
            GST_TYPE_VIDEO_RESAMPLER_METHOD, GST_VIDEO_RESAMPLER_METHOD_SINC,
            NULL);
        break;
      case GST_VIDEO_SCALE_HERMITE:
        gst_structure_set (options,
            GST_VIDEO_CONVERTER_OPT_RESAMPLER_METHOD,
            GST_TYPE_VIDEO_RESAMPLER_METHOD, GST_VIDEO_RESAMPLER_METHOD_CUBIC,
            GST_VIDEO_RESAMPLER_OPT_CUBIC_B, G_TYPE_DOUBLE, (gdouble) 0.0,
            GST_VIDEO_RESAMPLER_OPT_CUBIC_C, G_TYPE_DOUBLE, (gdouble) 0.0,
            NULL);
        break;
      case GST_VIDEO_SCALE_SPLINE:
        gst_structure_set (options,
            GST_VIDEO_CONVERTER_OPT_RESAMPLER_METHOD,
            GST_TYPE_VIDEO_RESAMPLER_METHOD, GST_VIDEO_RESAMPLER_METHOD_CUBIC,
            GST_VIDEO_RESAMPLER_OPT_CUBIC_B, G_TYPE_DOUBLE, (gdouble) 1.0,
            GST_VIDEO_RESAMPLER_OPT_CUBIC_C, G_TYPE_DOUBLE, (gdouble) 0.0,
            NULL);
        break;
      case GST_VIDEO_SCALE_CATROM:
        gst_structure_set (options,
            GST_VIDEO_CONVERTER_OPT_RESAMPLER_METHOD,
            GST_TYPE_VIDEO_RESAMPLER_METHOD, GST_VIDEO_RESAMPLER_METHOD_CUBIC,
            GST_VIDEO_RESAMPLER_OPT_CUBIC_B, G_TYPE_DOUBLE, (gdouble) 0.0,
            GST_VIDEO_RESAMPLER_OPT_CUBIC_C, G_TYPE_DOUBLE, (gdouble) 0.5,
            NULL);
        break;
      case GST_VIDEO_SCALE_MITCHELL:
        gst_structure_set (options,
            GST_VIDEO_CONVERTER_OPT_RESAMPLER_METHOD,
            GST_TYPE_VIDEO_RESAMPLER_METHOD, GST_VIDEO_RESAMPLER_METHOD_CUBIC,
            GST_VIDEO_RESAMPLER_OPT_CUBIC_B, G_TYPE_DOUBLE, (gdouble) 1.0 / 3.0,
            GST_VIDEO_RESAMPLER_OPT_CUBIC_C, G_TYPE_DOUBLE, (gdouble) 1.0 / 3.0,
            NULL);
        break;
    }
    gst_structure_set (options,
        GST_VIDEO_RESAMPLER_OPT_ENVELOPE, G_TYPE_DOUBLE, priv->envelope,
        GST_VIDEO_RESAMPLER_OPT_SHARPNESS, G_TYPE_DOUBLE, priv->sharpness,
        GST_VIDEO_RESAMPLER_OPT_SHARPEN, G_TYPE_DOUBLE, priv->sharpen,
        GST_VIDEO_CONVERTER_OPT_DEST_X, G_TYPE_INT, priv->borders_w / 2,
        GST_VIDEO_CONVERTER_OPT_DEST_Y, G_TYPE_INT, priv->borders_h / 2,
        GST_VIDEO_CONVERTER_OPT_DEST_WIDTH, G_TYPE_INT,
        out_info->width - priv->borders_w, GST_VIDEO_CONVERTER_OPT_DEST_HEIGHT,
        G_TYPE_INT, out_info->height - priv->borders_h,
        GST_VIDEO_CONVERTER_OPT_DITHER_METHOD, GST_TYPE_VIDEO_DITHER_METHOD,
        priv->dither, GST_VIDEO_CONVERTER_OPT_DITHER_QUANTIZATION, G_TYPE_UINT,
        priv->dither_quantization,
        GST_VIDEO_CONVERTER_OPT_CHROMA_RESAMPLER_METHOD,
        GST_TYPE_VIDEO_RESAMPLER_METHOD, priv->chroma_resampler,
        GST_VIDEO_CONVERTER_OPT_ALPHA_MODE, GST_TYPE_VIDEO_ALPHA_MODE,
        priv->alpha_mode, GST_VIDEO_CONVERTER_OPT_ALPHA_VALUE, G_TYPE_DOUBLE,
        priv->alpha_value, GST_VIDEO_CONVERTER_OPT_CHROMA_MODE,
        GST_TYPE_VIDEO_CHROMA_MODE, priv->chroma_mode,
        GST_VIDEO_CONVERTER_OPT_MATRIX_MODE, GST_TYPE_VIDEO_MATRIX_MODE,
        priv->matrix_mode, GST_VIDEO_CONVERTER_OPT_GAMMA_MODE,
        GST_TYPE_VIDEO_GAMMA_MODE, priv->gamma_mode,
        GST_VIDEO_CONVERTER_OPT_PRIMARIES_MODE, GST_TYPE_VIDEO_PRIMARIES_MODE,
        priv->primaries_mode, GST_VIDEO_CONVERTER_OPT_THREADS, G_TYPE_UINT,
        priv->n_threads, NULL);

  build_converter:
    priv->convert = gst_video_converter_new (in_info, out_info, options);
    if (priv->convert == NULL)
      goto no_convert;
  }

  GST_DEBUG_OBJECT (filter, "converting format %s -> %s",
      gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (in_info)),
      gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (out_info)));
  GST_DEBUG_OBJECT (self, "from=%dx%d (par=%d/%d dar=%d/%d), size %"
      G_GSIZE_FORMAT " -> to=%dx%d (par=%d/%d dar=%d/%d borders=%d:%d), "
      "size %" G_GSIZE_FORMAT,
      in_info->width, in_info->height, in_info->par_n, in_info->par_d,
      from_dar_n, from_dar_d, in_info->size, out_info->width,
      out_info->height, out_info->par_n, out_info->par_d, to_dar_n, to_dar_d,
      priv->borders_w, priv->borders_h, out_info->size);

  return TRUE;

  /* ERRORS */
format_mismatch:
  {
    GST_ERROR_OBJECT (self, "input and output formats do not match");
    return FALSE;
  }
no_convert:
  {
    GST_ERROR_OBJECT (self, "could not create converter");
    return FALSE;
  }
}

/*
 * This is an incomplete matrix of in formats and a score for the preferred output
 * format.
 *
 *         out: RGB24   RGB16  ARGB  AYUV  YUV444  YUV422 YUV420 YUV411 YUV410  PAL  GRAY
 *  in
 * RGB24          0      2       1     2     2       3      4      5      6      7    8
 * RGB16          1      0       1     2     2       3      4      5      6      7    8
 * ARGB           2      3       0     1     4       5      6      7      8      9    10
 * AYUV           3      4       1     0     2       5      6      7      8      9    10
 * YUV444         2      4       3     1     0       5      6      7      8      9    10
 * YUV422         3      5       4     2     1       0      6      7      8      9    10
 * YUV420         4      6       5     3     2       1      0      7      8      9    10
 * YUV411         4      6       5     3     2       1      7      0      8      9    10
 * YUV410         6      8       7     5     4       3      2      1      0      9    10
 * PAL            1      3       2     6     4       6      7      8      9      0    10
 * GRAY           1      4       3     2     1       5      6      7      8      9    0
 *
 * PAL or GRAY are never preferred, if we can we would convert to PAL instead
 * of GRAY, though
 * less subsampling is preferred and if any, preferably horizontal
 * We would like to keep the alpha, even if we would need to to colorspace conversion
 * or lose depth.
 */
#define SCORE_FORMAT_CHANGE       1
#define SCORE_DEPTH_CHANGE        1
#define SCORE_ALPHA_CHANGE        1
#define SCORE_CHROMA_W_CHANGE     1
#define SCORE_CHROMA_H_CHANGE     1
#define SCORE_PALETTE_CHANGE      1

#define SCORE_COLORSPACE_LOSS     2     /* RGB <-> YUV */
#define SCORE_DEPTH_LOSS          4     /* change bit depth */
#define SCORE_ALPHA_LOSS          8     /* lose the alpha channel */
#define SCORE_CHROMA_W_LOSS      16     /* vertical subsample */
#define SCORE_CHROMA_H_LOSS      32     /* horizontal subsample */
#define SCORE_PALETTE_LOSS       64     /* convert to palette format */
#define SCORE_COLOR_LOSS        128     /* convert to GRAY */

#define COLORSPACE_MASK (GST_VIDEO_FORMAT_FLAG_YUV | \
                         GST_VIDEO_FORMAT_FLAG_RGB | GST_VIDEO_FORMAT_FLAG_GRAY)
#define ALPHA_MASK      (GST_VIDEO_FORMAT_FLAG_ALPHA)
#define PALETTE_MASK    (GST_VIDEO_FORMAT_FLAG_PALETTE)

/* calculate how much loss a conversion would be */
static void
score_value (GstBaseTransform * base, const GstVideoFormatInfo * in_info,
    const GValue * val, gint * min_loss, const GstVideoFormatInfo ** out_info)
{
  const gchar *fname;
  const GstVideoFormatInfo *t_info;
  GstVideoFormatFlags in_flags, t_flags;
  gint loss;

  fname = g_value_get_string (val);
  t_info = gst_video_format_get_info (gst_video_format_from_string (fname));
  if (!t_info || t_info->format == GST_VIDEO_FORMAT_UNKNOWN)
    return;

  /* accept input format immediately without loss */
  if (in_info == t_info) {
    *min_loss = 0;
    *out_info = t_info;
    return;
  }

  loss = SCORE_FORMAT_CHANGE;

  in_flags = GST_VIDEO_FORMAT_INFO_FLAGS (in_info);
  in_flags &= ~GST_VIDEO_FORMAT_FLAG_LE;
  in_flags &= ~GST_VIDEO_FORMAT_FLAG_COMPLEX;
  in_flags &= ~GST_VIDEO_FORMAT_FLAG_UNPACK;

  t_flags = GST_VIDEO_FORMAT_INFO_FLAGS (t_info);
  t_flags &= ~GST_VIDEO_FORMAT_FLAG_LE;
  t_flags &= ~GST_VIDEO_FORMAT_FLAG_COMPLEX;
  t_flags &= ~GST_VIDEO_FORMAT_FLAG_UNPACK;

  if ((t_flags & PALETTE_MASK) != (in_flags & PALETTE_MASK)) {
    loss += SCORE_PALETTE_CHANGE;
    if (t_flags & PALETTE_MASK)
      loss += SCORE_PALETTE_LOSS;
  }

  if ((t_flags & COLORSPACE_MASK) != (in_flags & COLORSPACE_MASK)) {
    loss += SCORE_COLORSPACE_LOSS;
    if (t_flags & GST_VIDEO_FORMAT_FLAG_GRAY)
      loss += SCORE_COLOR_LOSS;
  }

  if ((t_flags & ALPHA_MASK) != (in_flags & ALPHA_MASK)) {
    loss += SCORE_ALPHA_CHANGE;
    if (in_flags & ALPHA_MASK)
      loss += SCORE_ALPHA_LOSS;
  }

  if ((in_info->h_sub[1]) != (t_info->h_sub[1])) {
    loss += SCORE_CHROMA_H_CHANGE;
    if ((in_info->h_sub[1]) < (t_info->h_sub[1]))
      loss += SCORE_CHROMA_H_LOSS;
  }
  if ((in_info->w_sub[1]) != (t_info->w_sub[1])) {
    loss += SCORE_CHROMA_W_CHANGE;
    if ((in_info->w_sub[1]) < (t_info->w_sub[1]))
      loss += SCORE_CHROMA_W_LOSS;
  }

  if ((in_info->bits) != (t_info->bits)) {
    loss += SCORE_DEPTH_CHANGE;
    if ((in_info->bits) > (t_info->bits))
      loss += SCORE_DEPTH_LOSS;
  }

  GST_DEBUG_OBJECT (base, "score %s -> %s = %d",
      GST_VIDEO_FORMAT_INFO_NAME (in_info),
      GST_VIDEO_FORMAT_INFO_NAME (t_info), loss);

  if (loss < *min_loss) {
    GST_DEBUG_OBJECT (base, "found new best %d", loss);
    *out_info = t_info;
    *min_loss = loss;
  }
}

static void
gst_video_convert_scale_fixate_format (GstBaseTransform * base, GstCaps * caps,
    GstCaps * result)
{
  GstStructure *ins, *outs;
  const gchar *in_format;
  const GstVideoFormatInfo *in_info, *out_info = NULL;
  gint min_loss = G_MAXINT;
  guint i, capslen;

  ins = gst_caps_get_structure (caps, 0);
  in_format = gst_structure_get_string (ins, "format");
  if (!in_format)
    return;

  GST_DEBUG_OBJECT (base, "source format %s", in_format);

  in_info =
      gst_video_format_get_info (gst_video_format_from_string (in_format));
  if (!in_info)
    return;

  outs = gst_caps_get_structure (result, 0);

  capslen = gst_caps_get_size (result);
  GST_DEBUG_OBJECT (base, "iterate %d structures", capslen);
  for (i = 0; i < capslen; i++) {
    GstStructure *tests;
    const GValue *format;

    tests = gst_caps_get_structure (result, i);
    format = gst_structure_get_value (tests, "format");
    gst_structure_remove_fields (tests, "height", "width", "pixel-aspect-ratio",
        "display-aspect-ratio", NULL);
    /* should not happen */
    if (format == NULL)
      continue;

    if (GST_VALUE_HOLDS_LIST (format)) {
      gint j, len;

      len = gst_value_list_get_size (format);
      GST_DEBUG_OBJECT (base, "have %d formats", len);
      for (j = 0; j < len; j++) {
        const GValue *val;

        val = gst_value_list_get_value (format, j);
        if (G_VALUE_HOLDS_STRING (val)) {
          score_value (base, in_info, val, &min_loss, &out_info);
          if (min_loss == 0)
            break;
        }
      }
    } else if (G_VALUE_HOLDS_STRING (format)) {
      score_value (base, in_info, format, &min_loss, &out_info);
    }
  }
  if (out_info)
    gst_structure_set (outs, "format", G_TYPE_STRING,
        GST_VIDEO_FORMAT_INFO_NAME (out_info), NULL);
}

static gboolean
subsampling_unchanged (GstVideoInfo * in_info, GstVideoInfo * out_info)
{
  gint i;
  const GstVideoFormatInfo *in_format, *out_format;

  if (GST_VIDEO_INFO_N_COMPONENTS (in_info) !=
      GST_VIDEO_INFO_N_COMPONENTS (out_info))
    return FALSE;

  in_format = in_info->finfo;
  out_format = out_info->finfo;

  for (i = 0; i < GST_VIDEO_INFO_N_COMPONENTS (in_info); i++) {
    if (GST_VIDEO_FORMAT_INFO_W_SUB (in_format,
            i) != GST_VIDEO_FORMAT_INFO_W_SUB (out_format, i))
      return FALSE;
    if (GST_VIDEO_FORMAT_INFO_H_SUB (in_format,
            i) != GST_VIDEO_FORMAT_INFO_H_SUB (out_format, i))
      return FALSE;
  }

  return TRUE;
}

static void
transfer_colorimetry_from_input (GstBaseTransform * trans, GstCaps * in_caps,
    GstCaps * out_caps)
{
  GstStructure *out_caps_s = gst_caps_get_structure (out_caps, 0);
  GstStructure *in_caps_s = gst_caps_get_structure (in_caps, 0);
  gboolean have_colorimetry =
      gst_structure_has_field (out_caps_s, "colorimetry");
  gboolean have_chroma_site =
      gst_structure_has_field (out_caps_s, "chroma-site");

  /* If the output already has colorimetry and chroma-site, stop,
   * otherwise try and transfer what we can from the input caps */
  if (have_colorimetry && have_chroma_site)
    return;

  {
    GstVideoInfo in_info, out_info;
    const GValue *in_colorimetry =
        gst_structure_get_value (in_caps_s, "colorimetry");
    GstCaps *tmp_caps = NULL;
    GstStructure *tmp_caps_s;

    if (!gst_video_info_from_caps (&in_info, in_caps)) {
      GST_WARNING_OBJECT (trans,
          "Failed to convert sink pad caps to video info");
      return;
    }

    /* We are before fixate_size(), the width and height of
       the output caps may be absent or not fixed. */
    tmp_caps = gst_caps_copy (out_caps);
    tmp_caps = gst_caps_fixate (tmp_caps);
    tmp_caps_s = gst_caps_get_structure (tmp_caps, 0);
    if (!gst_structure_has_field (tmp_caps_s, "width"))
      gst_structure_set_value (tmp_caps_s, "width",
          gst_structure_get_value (in_caps_s, "width"));
    if (!gst_structure_has_field (tmp_caps_s, "height"))
      gst_structure_set_value (tmp_caps_s, "height",
          gst_structure_get_value (in_caps_s, "height"));

    if (!gst_video_info_from_caps (&out_info, tmp_caps)) {
      gst_clear_caps (&tmp_caps);
      GST_WARNING_OBJECT (trans,
          "Failed to convert src pad caps to video info");
      return;
    }
    gst_clear_caps (&tmp_caps);

    if (!have_colorimetry && in_colorimetry != NULL) {
      if ((GST_VIDEO_INFO_IS_YUV (&out_info)
              && GST_VIDEO_INFO_IS_YUV (&in_info))
          || (GST_VIDEO_INFO_IS_RGB (&out_info)
              && GST_VIDEO_INFO_IS_RGB (&in_info))
          || (GST_VIDEO_INFO_IS_GRAY (&out_info)
              && GST_VIDEO_INFO_IS_GRAY (&in_info))) {
        /* Can transfer the colorimetry intact from the input if it has it */
        gst_structure_set_value (out_caps_s, "colorimetry", in_colorimetry);
      } else {
        gchar *colorimetry_str;

        /* Changing between YUV/RGB - forward primaries and transfer function, but use
         * default range and matrix.
         * the primaries is used for conversion between RGB and XYZ (CIE 1931 coordinate).
         * the transfer function could be another reference (e.g., HDR)
         */
        out_info.colorimetry.primaries = in_info.colorimetry.primaries;
        out_info.colorimetry.transfer = in_info.colorimetry.transfer;

        colorimetry_str =
            gst_video_colorimetry_to_string (&out_info.colorimetry);
        gst_caps_set_simple (out_caps, "colorimetry", G_TYPE_STRING,
            colorimetry_str, NULL);
        g_free (colorimetry_str);
      }
    }

    /* Only YUV output needs chroma-site. If the input was also YUV and had the same chroma
     * subsampling, transfer the siting. If the sub-sampling is changing, then the planes get
     * scaled anyway so there's no real reason to prefer the input siting. */
    if (!have_chroma_site && GST_VIDEO_INFO_IS_YUV (&out_info)) {
      if (GST_VIDEO_INFO_IS_YUV (&in_info)) {
        const GValue *in_chroma_site =
            gst_structure_get_value (in_caps_s, "chroma-site");
        if (in_chroma_site != NULL
            && subsampling_unchanged (&in_info, &out_info))
          gst_structure_set_value (out_caps_s, "chroma-site", in_chroma_site);
      }
    }
  }
}


static GstCaps *
gst_video_convert_scale_get_fixed_format (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstCaps *result;

  result = gst_caps_intersect (othercaps, caps);
  if (gst_caps_is_empty (result)) {
    gst_caps_unref (result);
    result = gst_caps_copy (othercaps);
  }

  result = gst_caps_make_writable (result);
  gst_video_convert_scale_fixate_format (trans, caps, result);

  /* fixate remaining fields */
  result = gst_caps_fixate (result);

  if (direction == GST_PAD_SINK) {
    if (gst_caps_is_subset (caps, result)) {
      gst_caps_replace (&result, caps);
    } else {
      /* Try and preserve input colorimetry / chroma information */
      transfer_colorimetry_from_input (trans, caps, result);
    }
  }

  return result;
}

static GstCaps *
gst_video_convert_scale_fixate_size (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstStructure *ins, *outs;
  const GValue *from_par, *to_par;
  GValue fpar = { 0, };
  GValue tpar = { 0, };

  othercaps = gst_caps_truncate (othercaps);
  othercaps = gst_caps_make_writable (othercaps);
  ins = gst_caps_get_structure (caps, 0);
  outs = gst_caps_get_structure (othercaps, 0);

  from_par = gst_structure_get_value (ins, "pixel-aspect-ratio");
  to_par = gst_structure_get_value (outs, "pixel-aspect-ratio");

  /* If we're fixating from the sinkpad we always set the PAR and
   * assume that missing PAR on the sinkpad means 1/1 and
   * missing PAR on the srcpad means undefined
   */
  if (direction == GST_PAD_SINK) {
    if (!from_par) {
      g_value_init (&fpar, GST_TYPE_FRACTION);
      gst_value_set_fraction (&fpar, 1, 1);
      from_par = &fpar;
    }
    if (!to_par) {
      g_value_init (&tpar, GST_TYPE_FRACTION_RANGE);
      gst_value_set_fraction_range_full (&tpar, 1, G_MAXINT, G_MAXINT, 1);
      to_par = &tpar;
    }
  } else {
    if (!to_par) {
      g_value_init (&tpar, GST_TYPE_FRACTION);
      gst_value_set_fraction (&tpar, 1, 1);
      to_par = &tpar;

      gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
          NULL);
    }
    if (!from_par) {
      g_value_init (&fpar, GST_TYPE_FRACTION);
      gst_value_set_fraction (&fpar, 1, 1);
      from_par = &fpar;
    }
  }

  /* we have both PAR but they might not be fixated */
  {
    gint from_w, from_h, from_par_n, from_par_d, to_par_n, to_par_d;
    gint w = 0, h = 0;
    gint from_dar_n, from_dar_d;
    gint num, den;

    /* from_par should be fixed */
    g_return_val_if_fail (gst_value_is_fixed (from_par), othercaps);

    from_par_n = gst_value_get_fraction_numerator (from_par);
    from_par_d = gst_value_get_fraction_denominator (from_par);

    gst_structure_get_int (ins, "width", &from_w);
    gst_structure_get_int (ins, "height", &from_h);

    gst_structure_get_int (outs, "width", &w);
    gst_structure_get_int (outs, "height", &h);

    /* if both width and height are already fixed, we can't do anything
     * about it anymore */
    if (w && h) {
      guint n, d;

      GST_DEBUG_OBJECT (base, "dimensions already set to %dx%d, not fixating",
          w, h);
      if (!gst_value_is_fixed (to_par)) {
        if (gst_video_calculate_display_ratio (&n, &d, from_w, from_h,
                from_par_n, from_par_d, w, h)) {
          GST_DEBUG_OBJECT (base, "fixating to_par to %dx%d", n, d);
          if (gst_structure_has_field (outs, "pixel-aspect-ratio"))
            gst_structure_fixate_field_nearest_fraction (outs,
                "pixel-aspect-ratio", n, d);
          else if (n != d)
            gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
                n, d, NULL);
        }
      }
      goto done;
    }

    /* Calculate input DAR */
    if (!gst_util_fraction_multiply (from_w, from_h, from_par_n, from_par_d,
            &from_dar_n, &from_dar_d)) {
      GST_ELEMENT_ERROR (base, CORE, NEGOTIATION, (NULL),
          ("Error calculating the output scaled size - integer overflow"));
      goto done;
    }

    GST_DEBUG_OBJECT (base, "Input DAR is %d/%d", from_dar_n, from_dar_d);

    /* If either width or height are fixed there's not much we
     * can do either except choosing a height or width and PAR
     * that matches the DAR as good as possible
     */
    if (h) {
      GstStructure *tmp;
      gint set_w, set_par_n, set_par_d;

      GST_DEBUG_OBJECT (base, "height is fixed (%d)", h);

      /* If the PAR is fixed too, there's not much to do
       * except choosing the width that is nearest to the
       * width with the same DAR */
      if (gst_value_is_fixed (to_par)) {
        to_par_n = gst_value_get_fraction_numerator (to_par);
        to_par_d = gst_value_get_fraction_denominator (to_par);

        GST_DEBUG_OBJECT (base, "PAR is fixed %d/%d", to_par_n, to_par_d);

        if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, to_par_d,
                to_par_n, &num, &den)) {
          GST_ELEMENT_ERROR (base, CORE, NEGOTIATION, (NULL),
              ("Error calculating the output scaled size - integer overflow"));
          goto done;
        }

        w = (guint) gst_util_uint64_scale_int_round (h, num, den);
        gst_structure_fixate_field_nearest_int (outs, "width", w);

        goto done;
      }

      /* The PAR is not fixed and it's quite likely that we can set
       * an arbitrary PAR. */

      /* Check if we can keep the input width */
      tmp = gst_structure_copy (outs);
      gst_structure_fixate_field_nearest_int (tmp, "width", from_w);
      gst_structure_get_int (tmp, "width", &set_w);

      /* Might have failed but try to keep the DAR nonetheless by
       * adjusting the PAR */
      if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, h, set_w,
              &to_par_n, &to_par_d)) {
        GST_ELEMENT_ERROR (base, CORE, NEGOTIATION, (NULL),
            ("Error calculating the output scaled size - integer overflow"));
        gst_structure_free (tmp);
        goto done;
      }

      if (!gst_structure_has_field (tmp, "pixel-aspect-ratio"))
        gst_structure_set_value (tmp, "pixel-aspect-ratio", to_par);
      gst_structure_fixate_field_nearest_fraction (tmp, "pixel-aspect-ratio",
          to_par_n, to_par_d);
      gst_structure_get_fraction (tmp, "pixel-aspect-ratio", &set_par_n,
          &set_par_d);
      gst_structure_free (tmp);

      /* Check if the adjusted PAR is accepted */
      if (set_par_n == to_par_n && set_par_d == to_par_d) {
        if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
            set_par_n != set_par_d)
          gst_structure_set (outs, "width", G_TYPE_INT, set_w,
              "pixel-aspect-ratio", GST_TYPE_FRACTION, set_par_n, set_par_d,
              NULL);
        goto done;
      }

      /* Otherwise scale the width to the new PAR and check if the
       * adjusted with is accepted. If all that fails we can't keep
       * the DAR */
      if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, set_par_d,
              set_par_n, &num, &den)) {
        GST_ELEMENT_ERROR (base, CORE, NEGOTIATION, (NULL),
            ("Error calculating the output scaled size - integer overflow"));
        goto done;
      }

      w = (guint) gst_util_uint64_scale_int_round (h, num, den);
      gst_structure_fixate_field_nearest_int (outs, "width", w);
      if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
          set_par_n != set_par_d)
        gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
            set_par_n, set_par_d, NULL);

      goto done;
    } else if (w) {
      GstStructure *tmp;
      gint set_h, set_par_n, set_par_d;

      GST_DEBUG_OBJECT (base, "width is fixed (%d)", w);

      /* If the PAR is fixed too, there's not much to do
       * except choosing the height that is nearest to the
       * height with the same DAR */
      if (gst_value_is_fixed (to_par)) {
        to_par_n = gst_value_get_fraction_numerator (to_par);
        to_par_d = gst_value_get_fraction_denominator (to_par);

        GST_DEBUG_OBJECT (base, "PAR is fixed %d/%d", to_par_n, to_par_d);

        if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, to_par_d,
                to_par_n, &num, &den)) {
          GST_ELEMENT_ERROR (base, CORE, NEGOTIATION, (NULL),
              ("Error calculating the output scaled size - integer overflow"));
          goto done;
        }

        h = (guint) gst_util_uint64_scale_int_round (w, den, num);
        gst_structure_fixate_field_nearest_int (outs, "height", h);

        goto done;
      }

      /* The PAR is not fixed and it's quite likely that we can set
       * an arbitrary PAR. */

      /* Check if we can keep the input height */
      tmp = gst_structure_copy (outs);
      gst_structure_fixate_field_nearest_int (tmp, "height", from_h);
      gst_structure_get_int (tmp, "height", &set_h);

      /* Might have failed but try to keep the DAR nonetheless by
       * adjusting the PAR */
      if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, set_h, w,
              &to_par_n, &to_par_d)) {
        GST_ELEMENT_ERROR (base, CORE, NEGOTIATION, (NULL),
            ("Error calculating the output scaled size - integer overflow"));
        gst_structure_free (tmp);
        goto done;
      }
      if (!gst_structure_has_field (tmp, "pixel-aspect-ratio"))
        gst_structure_set_value (tmp, "pixel-aspect-ratio", to_par);
      gst_structure_fixate_field_nearest_fraction (tmp, "pixel-aspect-ratio",
          to_par_n, to_par_d);
      gst_structure_get_fraction (tmp, "pixel-aspect-ratio", &set_par_n,
          &set_par_d);
      gst_structure_free (tmp);

      /* Check if the adjusted PAR is accepted */
      if (set_par_n == to_par_n && set_par_d == to_par_d) {
        if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
            set_par_n != set_par_d)
          gst_structure_set (outs, "height", G_TYPE_INT, set_h,
              "pixel-aspect-ratio", GST_TYPE_FRACTION, set_par_n, set_par_d,
              NULL);
        goto done;
      }

      /* Otherwise scale the height to the new PAR and check if the
       * adjusted with is accepted. If all that fails we can't keep
       * the DAR */
      if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, set_par_d,
              set_par_n, &num, &den)) {
        GST_ELEMENT_ERROR (base, CORE, NEGOTIATION, (NULL),
            ("Error calculating the output scaled size - integer overflow"));
        goto done;
      }

      h = (guint) gst_util_uint64_scale_int_round (w, den, num);
      gst_structure_fixate_field_nearest_int (outs, "height", h);
      if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
          set_par_n != set_par_d)
        gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
            set_par_n, set_par_d, NULL);

      goto done;
    } else if (gst_value_is_fixed (to_par)) {
      GstStructure *tmp;
      gint set_h, set_w, f_h, f_w;

      to_par_n = gst_value_get_fraction_numerator (to_par);
      to_par_d = gst_value_get_fraction_denominator (to_par);

      /* Calculate scale factor for the PAR change */
      if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, to_par_n,
              to_par_d, &num, &den)) {
        GST_ELEMENT_ERROR (base, CORE, NEGOTIATION, (NULL),
            ("Error calculating the output scaled size - integer overflow"));
        goto done;
      }

      /* Try to keep the input height (because of interlacing) */
      tmp = gst_structure_copy (outs);
      gst_structure_fixate_field_nearest_int (tmp, "height", from_h);
      gst_structure_get_int (tmp, "height", &set_h);

      /* This might have failed but try to scale the width
       * to keep the DAR nonetheless */
      w = (guint) gst_util_uint64_scale_int_round (set_h, num, den);
      gst_structure_fixate_field_nearest_int (tmp, "width", w);
      gst_structure_get_int (tmp, "width", &set_w);
      gst_structure_free (tmp);

      /* We kept the DAR and the height is nearest to the original height */
      if (set_w == w) {
        gst_structure_set (outs, "width", G_TYPE_INT, set_w, "height",
            G_TYPE_INT, set_h, NULL);
        goto done;
      }

      f_h = set_h;
      f_w = set_w;

      /* If the former failed, try to keep the input width at least */
      tmp = gst_structure_copy (outs);
      gst_structure_fixate_field_nearest_int (tmp, "width", from_w);
      gst_structure_get_int (tmp, "width", &set_w);

      /* This might have failed but try to scale the width
       * to keep the DAR nonetheless */
      h = (guint) gst_util_uint64_scale_int_round (set_w, den, num);
      gst_structure_fixate_field_nearest_int (tmp, "height", h);
      gst_structure_get_int (tmp, "height", &set_h);
      gst_structure_free (tmp);

      /* We kept the DAR and the width is nearest to the original width */
      if (set_h == h) {
        gst_structure_set (outs, "width", G_TYPE_INT, set_w, "height",
            G_TYPE_INT, set_h, NULL);
        goto done;
      }

      /* If all this failed, keep the dimensions with the DAR that was closest
       * to the correct DAR. This changes the DAR but there's not much else to
       * do here.
       */
      if (set_w * ABS (set_h - h) < ABS (f_w - w) * f_h) {
        f_h = set_h;
        f_w = set_w;
      }
      gst_structure_set (outs, "width", G_TYPE_INT, f_w, "height", G_TYPE_INT,
          f_h, NULL);
      goto done;
    } else {
      GstStructure *tmp;
      gint set_h, set_w, set_par_n, set_par_d, tmp2;

      /* width, height and PAR are not fixed but passthrough is not possible */

      /* First try to keep the height and width as good as possible
       * and scale PAR */
      tmp = gst_structure_copy (outs);
      gst_structure_fixate_field_nearest_int (tmp, "height", from_h);
      gst_structure_get_int (tmp, "height", &set_h);
      gst_structure_fixate_field_nearest_int (tmp, "width", from_w);
      gst_structure_get_int (tmp, "width", &set_w);

      if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, set_h, set_w,
              &to_par_n, &to_par_d)) {
        GST_ELEMENT_ERROR (base, CORE, NEGOTIATION, (NULL),
            ("Error calculating the output scaled size - integer overflow"));
        gst_structure_free (tmp);
        goto done;
      }

      if (!gst_structure_has_field (tmp, "pixel-aspect-ratio"))
        gst_structure_set_value (tmp, "pixel-aspect-ratio", to_par);
      gst_structure_fixate_field_nearest_fraction (tmp, "pixel-aspect-ratio",
          to_par_n, to_par_d);
      gst_structure_get_fraction (tmp, "pixel-aspect-ratio", &set_par_n,
          &set_par_d);
      gst_structure_free (tmp);

      if (set_par_n == to_par_n && set_par_d == to_par_d) {
        gst_structure_set (outs, "width", G_TYPE_INT, set_w, "height",
            G_TYPE_INT, set_h, NULL);

        if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
            set_par_n != set_par_d)
          gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
              set_par_n, set_par_d, NULL);
        goto done;
      }

      /* Otherwise try to scale width to keep the DAR with the set
       * PAR and height */
      if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, set_par_d,
              set_par_n, &num, &den)) {
        GST_ELEMENT_ERROR (base, CORE, NEGOTIATION, (NULL),
            ("Error calculating the output scaled size - integer overflow"));
        goto done;
      }

      w = (guint) gst_util_uint64_scale_int_round (set_h, num, den);
      tmp = gst_structure_copy (outs);
      gst_structure_fixate_field_nearest_int (tmp, "width", w);
      gst_structure_get_int (tmp, "width", &tmp2);
      gst_structure_free (tmp);

      if (tmp2 == w) {
        gst_structure_set (outs, "width", G_TYPE_INT, tmp2, "height",
            G_TYPE_INT, set_h, NULL);
        if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
            set_par_n != set_par_d)
          gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
              set_par_n, set_par_d, NULL);
        goto done;
      }

      /* ... or try the same with the height */
      h = (guint) gst_util_uint64_scale_int_round (set_w, den, num);
      tmp = gst_structure_copy (outs);
      gst_structure_fixate_field_nearest_int (tmp, "height", h);
      gst_structure_get_int (tmp, "height", &tmp2);
      gst_structure_free (tmp);

      if (tmp2 == h) {
        gst_structure_set (outs, "width", G_TYPE_INT, set_w, "height",
            G_TYPE_INT, tmp2, NULL);
        if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
            set_par_n != set_par_d)
          gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
              set_par_n, set_par_d, NULL);
        goto done;
      }

      /* If all fails we can't keep the DAR and take the nearest values
       * for everything from the first try */
      gst_structure_set (outs, "width", G_TYPE_INT, set_w, "height",
          G_TYPE_INT, set_h, NULL);
      if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
          set_par_n != set_par_d)
        gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
            set_par_n, set_par_d, NULL);
    }
  }

done:
  othercaps = gst_caps_fixate (othercaps);

  GST_DEBUG_OBJECT (base, "fixated othercaps to %" GST_PTR_FORMAT, othercaps);

  if (from_par == &fpar)
    g_value_unset (&fpar);
  if (to_par == &tpar)
    g_value_unset (&tpar);

  return othercaps;
}

static GstCaps *
gst_video_convert_scale_fixate_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstCaps *format;

  GST_DEBUG_OBJECT (base,
      "trying to fixate othercaps %" GST_PTR_FORMAT " based on caps %"
      GST_PTR_FORMAT, othercaps, caps);

  format = gst_video_convert_scale_get_fixed_format (base, direction, caps,
      othercaps);

  if (gst_caps_is_empty (format)) {
    GST_ERROR_OBJECT (base, "Could not convert formats");
    return format;
  }

  othercaps =
      gst_video_convert_scale_fixate_size (base, direction, caps, othercaps);
  if (gst_caps_get_size (othercaps) == 1) {
    gint i;
    const gchar *format_fields[] = { "format", "colorimetry", "chroma-site" };
    GstStructure *format_struct = gst_caps_get_structure (format, 0);
    GstStructure *fixated_struct;

    othercaps = gst_caps_make_writable (othercaps);
    fixated_struct = gst_caps_get_structure (othercaps, 0);

    for (i = 0; i < G_N_ELEMENTS (format_fields); i++) {
      if (gst_structure_has_field (format_struct, format_fields[i])) {
        gst_structure_set (fixated_struct, format_fields[i], G_TYPE_STRING,
            gst_structure_get_string (format_struct, format_fields[i]), NULL);
      } else {
        gst_structure_remove_field (fixated_struct, format_fields[i]);
      }
    }
  }
  gst_caps_unref (format);

  GST_DEBUG_OBJECT (base, "fixated othercaps to %" GST_PTR_FORMAT, othercaps);

  return othercaps;
}

#define GET_LINE(frame, line) \
    (gpointer)(((guint8*)(GST_VIDEO_FRAME_PLANE_DATA (frame, 0))) + \
     GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0) * (line))

static GstFlowReturn
gst_video_convert_scale_transform_frame (GstVideoFilter * filter,
    GstVideoFrame * in_frame, GstVideoFrame * out_frame)
{
  GstVideoConvertScalePrivate *priv = PRIV (filter);
  GstFlowReturn ret = GST_FLOW_OK;

  GST_CAT_DEBUG_OBJECT (CAT_PERFORMANCE, filter, "doing video scaling");

  if (priv->converter_config_changed) {
    GstStructure *options =
        gst_video_convert_scale_get_converter_config (GST_VIDEO_CONVERT_SCALE
        (filter), &filter->out_info);

    gst_video_converter_free (priv->convert);
    priv->convert =
        gst_video_converter_new (&filter->in_info, &filter->out_info, options);

    priv->converter_config_changed = FALSE;
  }

  gst_video_converter_frame (priv->convert, in_frame, out_frame);

  return ret;
}

static gboolean
gst_video_convert_scale_src_event (GstBaseTransform * trans, GstEvent * event)
{
  GstVideoConvertScale *self = GST_VIDEO_CONVERT_SCALE_CAST (trans);
  GstVideoFilter *filter = GST_VIDEO_FILTER_CAST (trans);
  gboolean ret;
  gdouble x, y;

  GST_DEBUG_OBJECT (self, "handling %s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_NAVIGATION:
      if (filter->in_info.width != filter->out_info.width ||
          filter->in_info.height != filter->out_info.height) {
        event = gst_event_make_writable (event);

        if (gst_navigation_event_get_coordinates (event, &x, &y)) {
          gst_navigation_event_set_coordinates (event,
              x * filter->in_info.width / filter->out_info.width,
              y * filter->in_info.height / filter->out_info.height);
        }
      }
      break;
    default:
      break;
  }

  ret = GST_BASE_TRANSFORM_CLASS (parent_class)->src_event (trans, event);

  return ret;
}
