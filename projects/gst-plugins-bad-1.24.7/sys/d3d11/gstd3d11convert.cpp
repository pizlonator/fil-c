/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) 2005-2012 David Schleef <ds@schleef.org>
 * Copyright (C) 2012-2014 Matthew Waters <ystree00@gmail.com>
 * Copyright (C) <2019> Seungha Yang <seungha.yang@navercorp.com>
 * Copyright (C) <2019> Jeongki Kim <jeongki.kim@jeongki.kim>
 * Copyright (C) 2020 Thibault Saunier <tsaunier@igalia.com>
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
#  include <config.h>
#endif

#include "gstd3d11convert.h"
#include "gstd3d11pluginutils.h"

GST_DEBUG_CATEGORY_STATIC (gst_d3d11_convert_debug);
#define GST_CAT_DEFAULT gst_d3d11_convert_debug

static GstStaticCaps sink_template_caps =
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
    (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY, GST_D3D11_SINK_FORMATS) "; "
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES
    (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY ","
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
        GST_D3D11_SINK_FORMATS));

static GstStaticCaps src_template_caps =
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
    (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY, GST_D3D11_SRC_FORMATS) "; "
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES
    (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY ","
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
        GST_D3D11_SRC_FORMATS));

#define DEFAULT_ADD_BORDERS TRUE
#define DEFAULT_BORDER_COLOR G_GUINT64_CONSTANT(0xffff000000000000)
#define DEFAULT_GAMMA_MODE GST_VIDEO_GAMMA_MODE_NONE
#define DEFAULT_PRIMARIES_MODE GST_VIDEO_PRIMARIES_MODE_NONE
#define DEFAULT_SAMPLING_METHOD GST_D3D11_SAMPLING_METHOD_BILINEAR
#define DEFAULT_ALPHA_MODE GST_D3D11_ALPHA_MODE_UNSPECIFIED
#define DEFAULT_ROTATION 0.0f
#define DEFAULT_SCALE 1.0f
#define DEFAULT_FOV 90.0f
#define DEFAULT_ORTHO FALSE
#define DEFAULT_MSAA GST_D3D11_MSAA_DISABLED

struct _GstD3D11BaseConvert
{
  GstD3D11BaseFilter parent;

  GstD3D11Converter *converter;
  gboolean same_caps;
  gboolean downstream_supports_crop_meta;

  /* used for border rendering */
  RECT in_rect;
  RECT prev_in_rect;
  RECT out_rect;

  gint borders_h;
  gint borders_w;

  GstBuffer *msaa_buf;
  GstBufferPool *msaa_pool;

  /* Updated by subclass */
  gboolean add_borders;
  gboolean active_add_borders;

  guint64 border_color;

  GstVideoGammaMode gamma_mode;
  GstVideoGammaMode active_gamma_mode;

  GstVideoPrimariesMode primaries_mode;
  GstVideoPrimariesMode active_primaries_mode;

  /* sampling method, configured via property */
  GstD3D11SamplingMethod sampling_method;
  GstD3D11SamplingMethod active_sampling_method;

  /* orientation */
  /* method configured via property */
  GstVideoOrientationMethod method;
  /* method parsed from tag */
  GstVideoOrientationMethod tag_method;
  /* method currently selected based on "method" and "tag_method" */
  GstVideoOrientationMethod selected_method;
  /* method previously selected and used for negotiation */
  GstVideoOrientationMethod active_method;

  GstD3D11AlphaMode src_alpha_mode;
  GstD3D11AlphaMode dst_alpha_mode;

  /* transform */
  gfloat fov;
  gboolean ortho;
  gfloat rotation_x;
  gfloat rotation_y;
  gfloat rotation_z;
  gfloat scale_x;
  gfloat scale_y;
  gboolean transform_updated;

  GstD3D11MSAAMode msaa;
  gboolean msaa_updated;

  SRWLOCK lock;
};

/**
 * GstD3D11BaseConvert:
 *
 * A baseclass implementation for d3d11 convert elements
 *
 * Since: 1.20
 */
#define gst_d3d11_base_convert_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstD3D11BaseConvert, gst_d3d11_base_convert,
    GST_TYPE_D3D11_BASE_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_d3d11_convert_debug, "d3d11convert", 0,
        "d3d11convert"));

enum
{
  PROP_BASE_CONVERT_0,
  PROP_BASE_CONVERT_SAMPLING_METHOD,
};

static void
gst_d3d11_base_convert_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void
gst_d3d11_base_convert_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_d3d11_base_convert_dispose (GObject * object);
static GstCaps *gst_d3d11_base_convert_transform_caps (GstBaseTransform *
    trans, GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static GstCaps *gst_d3d11_base_convert_fixate_caps (GstBaseTransform *
    base, GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);
static gboolean
gst_d3d11_base_convert_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query);
static gboolean
gst_d3d11_base_convert_decide_allocation (GstBaseTransform * trans,
    GstQuery * query);
static GstFlowReturn
gst_d3d11_base_convert_generate_output (GstBaseTransform * trans,
    GstBuffer ** buffer);
static gboolean gst_d3d11_base_convert_transform_meta (GstBaseTransform * trans,
    GstBuffer * outbuf, GstMeta * meta, GstBuffer * inbuf);
static void gst_d3d11_base_convert_before_transform (GstBaseTransform * trans,
    GstBuffer * buffer);
static GstFlowReturn gst_d3d11_base_convert_transform (GstBaseTransform *
    trans, GstBuffer * inbuf, GstBuffer * outbuf);
static gboolean gst_d3d11_base_convert_set_info (GstD3D11BaseFilter * filter,
    GstCaps * incaps, GstVideoInfo * in_info, GstCaps * outcaps,
    GstVideoInfo * out_info);
static void
gst_d3d11_base_convert_set_sampling_method (GstD3D11BaseConvert * self,
    GstD3D11SamplingMethod method);

/* copies the given caps */
static GstCaps *
gst_d3d11_base_convert_caps_remove_format_info (GstCaps * caps)
{
  GstStructure *st;
  GstCapsFeatures *f;
  gint i, n;
  GstCaps *res;
  GstCapsFeatures *feature =
      gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY);

  res = gst_caps_new_empty ();

  n = gst_caps_get_size (caps);
  for (i = 0; i < n; i++) {
    st = gst_caps_get_structure (caps, i);
    f = gst_caps_get_features (caps, i);

    /* If this is already expressed by the existing caps
     * skip this structure */
    if (i > 0 && gst_caps_is_subset_structure_full (res, st, f))
      continue;

    st = gst_structure_copy (st);
    /* Only remove format info for the cases when we can actually convert */
    if (!gst_caps_features_is_any (f)
        && gst_caps_features_is_equal (f, feature)) {
      gst_structure_remove_fields (st, "format", "colorimetry", "chroma-site",
          NULL);
    }

    gst_caps_append_structure_full (res, st, gst_caps_features_copy (f));
  }
  gst_caps_features_free (feature);

  return res;
}

static GstCaps *
gst_d3d11_base_convert_caps_rangify_size_info (GstCaps * caps)
{
  GstStructure *st;
  GstCapsFeatures *f;
  gint i, n;
  GstCaps *res;
  GstCapsFeatures *feature =
      gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY);

  res = gst_caps_new_empty ();

  n = gst_caps_get_size (caps);
  for (i = 0; i < n; i++) {
    st = gst_caps_get_structure (caps, i);
    f = gst_caps_get_features (caps, i);

    /* If this is already expressed by the existing caps
     * skip this structure */
    if (i > 0 && gst_caps_is_subset_structure_full (res, st, f))
      continue;

    st = gst_structure_copy (st);
    /* Only remove format info for the cases when we can actually convert */
    if (!gst_caps_features_is_any (f)
        && gst_caps_features_is_equal (f, feature)) {
      gst_structure_set (st, "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
          "height", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);

      /* if pixel aspect ratio, make a range of it */
      if (gst_structure_has_field (st, "pixel-aspect-ratio")) {
        gst_structure_set (st, "pixel-aspect-ratio",
            GST_TYPE_FRACTION_RANGE, 1, G_MAXINT, G_MAXINT, 1, NULL);
      }
    }

    gst_caps_append_structure_full (res, st, gst_caps_features_copy (f));
  }
  gst_caps_features_free (feature);

  return res;
}

static GstCaps *
gst_d3d11_base_convert_caps_remove_format_and_rangify_size_info (GstCaps * caps)
{
  GstStructure *st;
  GstCapsFeatures *f;
  gint i, n;
  GstCaps *res;
  GstCapsFeatures *feature =
      gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY);

  res = gst_caps_new_empty ();

  n = gst_caps_get_size (caps);
  for (i = 0; i < n; i++) {
    st = gst_caps_get_structure (caps, i);
    f = gst_caps_get_features (caps, i);

    /* If this is already expressed by the existing caps
     * skip this structure */
    if (i > 0 && gst_caps_is_subset_structure_full (res, st, f))
      continue;

    st = gst_structure_copy (st);
    /* Only remove format info for the cases when we can actually convert */
    if (!gst_caps_features_is_any (f)
        && gst_caps_features_is_equal (f, feature)) {
      gst_structure_set (st, "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
          "height", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);
      /* if pixel aspect ratio, make a range of it */
      if (gst_structure_has_field (st, "pixel-aspect-ratio")) {
        gst_structure_set (st, "pixel-aspect-ratio",
            GST_TYPE_FRACTION_RANGE, 1, G_MAXINT, G_MAXINT, 1, NULL);
      }
      gst_structure_remove_fields (st, "format", "colorimetry", "chroma-site",
          NULL);
    }

    gst_caps_append_structure_full (res, st, gst_caps_features_copy (f));
  }
  gst_caps_features_free (feature);

  return res;
}

static void
gst_d3d11_base_convert_class_init (GstD3D11BaseConvertClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstD3D11BaseFilterClass *bfilter_class = GST_D3D11_BASE_FILTER_CLASS (klass);
  GstCaps *caps;

  gobject_class->set_property = gst_d3d11_base_convert_set_property;
  gobject_class->get_property = gst_d3d11_base_convert_get_property;
  gobject_class->dispose = gst_d3d11_base_convert_dispose;

  /**
   * GstD3D11BaseConvert:method:
   *
   * Method used for sampling
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class,
      PROP_BASE_CONVERT_SAMPLING_METHOD,
      g_param_spec_enum ("method", "Method",
          "Method used for sampling",
          GST_TYPE_D3D11_SAMPLING_METHOD, DEFAULT_SAMPLING_METHOD,
          (GParamFlags) (GST_PARAM_MUTABLE_PLAYING | G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS)));

  caps = gst_d3d11_get_updated_template_caps (&sink_template_caps);
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps));
  gst_caps_unref (caps);

  caps = gst_d3d11_get_updated_template_caps (&src_template_caps);
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps));
  gst_caps_unref (caps);

  trans_class->passthrough_on_same_caps = FALSE;

  trans_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_d3d11_base_convert_transform_caps);
  trans_class->fixate_caps =
      GST_DEBUG_FUNCPTR (gst_d3d11_base_convert_fixate_caps);
  trans_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_d3d11_base_convert_propose_allocation);
  trans_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_d3d11_base_convert_decide_allocation);
  trans_class->generate_output =
      GST_DEBUG_FUNCPTR (gst_d3d11_base_convert_generate_output);
  trans_class->transform_meta =
      GST_DEBUG_FUNCPTR (gst_d3d11_base_convert_transform_meta);
  trans_class->before_transform =
      GST_DEBUG_FUNCPTR (gst_d3d11_base_convert_before_transform);
  trans_class->transform = GST_DEBUG_FUNCPTR (gst_d3d11_base_convert_transform);

  bfilter_class->set_info = GST_DEBUG_FUNCPTR (gst_d3d11_base_convert_set_info);

  gst_type_mark_as_plugin_api (GST_TYPE_D3D11_BASE_CONVERT,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_D3D11_SAMPLING_METHOD,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_D3D11_ALPHA_MODE,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_D3D11_MSAA_MODE, (GstPluginAPIFlags) 0);
}

static void
gst_d3d11_base_convert_init (GstD3D11BaseConvert * self)
{
  self->add_borders = self->active_add_borders = DEFAULT_ADD_BORDERS;
  self->border_color = DEFAULT_BORDER_COLOR;
  self->gamma_mode = self->active_gamma_mode = DEFAULT_GAMMA_MODE;
  self->primaries_mode = self->active_primaries_mode = DEFAULT_PRIMARIES_MODE;
  self->sampling_method = self->active_sampling_method =
      DEFAULT_SAMPLING_METHOD;
  self->src_alpha_mode = self->dst_alpha_mode = DEFAULT_ALPHA_MODE;
  self->fov = DEFAULT_FOV;
  self->ortho = DEFAULT_ORTHO;
  self->rotation_x = DEFAULT_ROTATION;
  self->rotation_y = DEFAULT_ROTATION;
  self->rotation_z = DEFAULT_ROTATION;
  self->scale_x = DEFAULT_SCALE;
  self->scale_y = DEFAULT_SCALE;
  self->msaa = DEFAULT_MSAA;
}

static void
gst_d3d11_base_convert_dispose (GObject * object)
{
  GstD3D11BaseConvert *self = GST_D3D11_BASE_CONVERT (object);

  gst_clear_object (&self->converter);
  gst_clear_buffer (&self->msaa_buf);
  if (self->msaa_pool) {
    gst_buffer_pool_set_active (self->msaa_pool, FALSE);
    gst_clear_object (&self->msaa_pool);
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_d3d11_base_convert_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstD3D11BaseConvert *base = GST_D3D11_BASE_CONVERT (object);

  switch (prop_id) {
    case PROP_BASE_CONVERT_SAMPLING_METHOD:
      gst_d3d11_base_convert_set_sampling_method (base,
          (GstD3D11SamplingMethod) g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_d3d11_base_convert_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstD3D11BaseConvert *base = GST_D3D11_BASE_CONVERT (object);

  switch (prop_id) {
    case PROP_BASE_CONVERT_SAMPLING_METHOD:
      g_value_set_enum (value, base->sampling_method);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstCaps *
gst_d3d11_base_convert_transform_caps (GstBaseTransform *
    trans, GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *tmp, *tmp2;
  GstCaps *result;

  /* Get all possible caps that we can transform to */
  tmp = gst_d3d11_base_convert_caps_remove_format_and_rangify_size_info (caps);

  if (filter) {
    tmp2 = gst_caps_intersect_full (filter, tmp, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp);
    tmp = tmp2;
  }

  result = tmp;

  GST_DEBUG_OBJECT (trans, "transformed %" GST_PTR_FORMAT " into %"
      GST_PTR_FORMAT, caps, result);

  return result;
}

/*
 * This is an incomplete matrix of in formats and a score for the prefered output
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
 * PAL or GRAY are never prefered, if we can we would convert to PAL instead
 * of GRAY, though
 * less subsampling is prefered and if any, preferably horizontal
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
  guint in_flags, t_flags;
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
      loss += SCORE_DEPTH_LOSS + (in_info->bits - t_info->bits);
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
gst_d3d11_base_convert_fixate_format (GstBaseTransform * trans,
    GstCaps * caps, GstCaps * result)
{
  GstStructure *ins, *outs;
  const gchar *in_format;
  const GstVideoFormatInfo *in_info, *out_info = NULL;
  gint min_loss = G_MAXINT;
  guint i, capslen;

  ins = gst_caps_get_structure (caps, 0);
  in_format = gst_structure_get_string (ins, "format");
  if (!in_format) {
    return;
  }

  GST_DEBUG_OBJECT (trans, "source format %s", in_format);

  in_info =
      gst_video_format_get_info (gst_video_format_from_string (in_format));
  if (!in_info)
    return;

  outs = gst_caps_get_structure (result, 0);

  capslen = gst_caps_get_size (result);
  GST_DEBUG ("iterate %d structures", capslen);
  for (i = 0; i < capslen; i++) {
    GstStructure *tests;
    const GValue *format;

    tests = gst_caps_get_structure (result, i);
    format = gst_structure_get_value (tests, "format");

    /* should not happen */
    if (format == NULL)
      continue;

    if (GST_VALUE_HOLDS_LIST (format)) {
      gint j, len;

      len = gst_value_list_get_size (format);
      GST_DEBUG_OBJECT (trans, "have %d formats", len);
      for (j = 0; j < len; j++) {
        const GValue *val;

        val = gst_value_list_get_value (format, j);
        if (G_VALUE_HOLDS_STRING (val)) {
          score_value (trans, in_info, val, &min_loss, &out_info);
          if (min_loss == 0)
            break;
        }
      }
    } else if (G_VALUE_HOLDS_STRING (format)) {
      score_value (trans, in_info, format, &min_loss, &out_info);
    }
  }
  if (out_info)
    gst_structure_set (outs, "format", G_TYPE_STRING,
        GST_VIDEO_FORMAT_INFO_NAME (out_info), NULL);
}

static gboolean
subsampling_unchanged (GstVideoInfo * in_info, GstVideoInfo * out_info)
{
  guint i;
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

    if (!gst_video_info_from_caps (&in_info, in_caps)) {
      GST_WARNING_OBJECT (trans,
          "Failed to convert sink pad caps to video info");
      return;
    }
    if (!gst_video_info_from_caps (&out_info, out_caps)) {
      GST_WARNING_OBJECT (trans,
          "Failed to convert src pad caps to video info");
      return;
    }

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
gst_d3d11_base_convert_get_fixed_format (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstCaps *result;

  result = gst_caps_intersect (othercaps, caps);
  if (gst_caps_is_empty (result)) {
    gst_caps_unref (result);
    result = gst_caps_copy (othercaps);
  }

  gst_d3d11_base_convert_fixate_format (trans, caps, result);

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
gst_d3d11_base_convert_fixate_size (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstD3D11BaseConvert *self = GST_D3D11_BASE_CONVERT (base);
  GstStructure *ins, *outs;
  const GValue *from_par, *to_par;
  GValue fpar = G_VALUE_INIT, tpar = G_VALUE_INIT;
  gboolean rotate = FALSE;

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
  GstD3D11SRWLockGuard lk (&self->lock);
  switch (self->selected_method) {
    case GST_VIDEO_ORIENTATION_90R:
    case GST_VIDEO_ORIENTATION_90L:
    case GST_VIDEO_ORIENTATION_UL_LR:
    case GST_VIDEO_ORIENTATION_UR_LL:
      rotate = TRUE;
      break;
    default:
      rotate = FALSE;
      break;
  }

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
    gint from_par_n, from_par_d;

    if (!from_par) {
      g_value_init (&fpar, GST_TYPE_FRACTION);
      gst_value_set_fraction (&fpar, 1, 1);
      from_par = &fpar;

      from_par_n = from_par_d = 1;
    } else {
      from_par_n = gst_value_get_fraction_numerator (from_par);
      from_par_d = gst_value_get_fraction_denominator (from_par);
    }

    if (!to_par) {
      gint to_par_n, to_par_d;

      if (rotate) {
        to_par_n = from_par_d;
        to_par_d = from_par_n;
      } else {
        to_par_n = from_par_n;
        to_par_d = from_par_d;
      }

      g_value_init (&tpar, GST_TYPE_FRACTION);
      gst_value_set_fraction (&tpar, to_par_n, to_par_d);
      to_par = &tpar;

      gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
          to_par_n, to_par_d, NULL);
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

    /* swap dimensions when it's rotated */
    if (rotate) {
      gint _tmp = from_w;
      from_w = from_h;
      from_h = _tmp;

      _tmp = from_par_n;
      from_par_n = from_par_d;
      from_par_d = _tmp;
    }

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
            ("Error calculating the output scale sized - integer overflow"));
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
  if (from_par == &fpar)
    g_value_unset (&fpar);
  if (to_par == &tpar)
    g_value_unset (&tpar);

  return othercaps;
}

static GstCaps *
gst_d3d11_base_convert_fixate_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstCaps *format = NULL;

  GST_DEBUG_OBJECT (base,
      "trying to fixate othercaps %" GST_PTR_FORMAT " based on caps %"
      GST_PTR_FORMAT, othercaps, caps);

  format = gst_d3d11_base_convert_get_fixed_format (base, direction, caps,
      othercaps);

  if (gst_caps_is_empty (format)) {
    GST_ERROR_OBJECT (base, "Could not convert formats");
    return format;
  }

  /* convert mode is "all" or "size" here */
  othercaps =
      gst_d3d11_base_convert_fixate_size (base, direction, caps, othercaps);

  if (gst_caps_get_size (othercaps) == 1) {
    guint i;
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

static gboolean
gst_d3d11_base_convert_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
  GstD3D11BaseConvert *self = GST_D3D11_BASE_CONVERT (trans);
  GstD3D11BaseFilter *filter = GST_D3D11_BASE_FILTER (trans);
  GstVideoInfo info;
  GstBufferPool *pool = NULL;
  GstCaps *caps;
  guint n_pools, i;
  GstStructure *config;
  guint size;
  GstD3D11AllocationParams *d3d11_params;
  GstD3D11Format d3d11_format;
  guint bind_flags = D3D11_BIND_SHADER_RESOURCE;
  DXGI_FORMAT dxgi_format = DXGI_FORMAT_UNKNOWN;
  UINT supported = 0;
  HRESULT hr;
  ID3D11Device *device_handle;

  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->propose_allocation (trans,
          decide_query, query)) {
    return FALSE;
  }

  if (self->same_caps) {
    if (!gst_pad_peer_query (trans->srcpad, query))
      return FALSE;

    gst_query_add_allocation_meta (query,
        GST_VIDEO_CROP_META_API_TYPE, nullptr);
    return TRUE;
  }

  gst_query_parse_allocation (query, &caps, NULL);

  if (caps == NULL)
    return FALSE;

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (filter, "Invalid caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }

  if (!gst_d3d11_device_get_format (filter->device,
          GST_VIDEO_INFO_FORMAT (&info), &d3d11_format)) {
    GST_ERROR_OBJECT (filter, "Unknown format caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }

  if (d3d11_format.dxgi_format == DXGI_FORMAT_UNKNOWN) {
    dxgi_format = d3d11_format.resource_format[0];
  } else {
    dxgi_format = d3d11_format.dxgi_format;
  }

  device_handle = gst_d3d11_device_get_device_handle (filter->device);
  hr = device_handle->CheckFormatSupport (dxgi_format, &supported);
  if (gst_d3d11_result (hr, filter->device) &&
      (supported & D3D11_FORMAT_SUPPORT_RENDER_TARGET) ==
      D3D11_FORMAT_SUPPORT_RENDER_TARGET) {
    bind_flags |= D3D11_BIND_RENDER_TARGET;
  }

  n_pools = gst_query_get_n_allocation_pools (query);
  for (i = 0; i < n_pools; i++) {
    gst_query_parse_nth_allocation_pool (query, i, &pool, NULL, NULL, NULL);
    if (pool) {
      if (!GST_IS_D3D11_BUFFER_POOL (pool)) {
        gst_clear_object (&pool);
      } else {
        GstD3D11BufferPool *dpool = GST_D3D11_BUFFER_POOL (pool);
        if (dpool->device != filter->device)
          gst_clear_object (&pool);
      }
    }
  }

  if (!pool)
    pool = gst_d3d11_buffer_pool_new (filter->device);

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);

  d3d11_params = gst_buffer_pool_config_get_d3d11_allocation_params (config);
  if (!d3d11_params) {
    d3d11_params = gst_d3d11_allocation_params_new (filter->device, &info,
        GST_D3D11_ALLOCATION_FLAG_DEFAULT, bind_flags, 0);
  } else {
    gst_d3d11_allocation_params_set_bind_flags (d3d11_params, bind_flags);
  }

  gst_buffer_pool_config_set_d3d11_allocation_params (config, d3d11_params);
  gst_d3d11_allocation_params_free (d3d11_params);

  /* size will be updated by d3d11 buffer pool */
  gst_buffer_pool_config_set_params (config, caps, 0, 0, 0);

  if (!gst_buffer_pool_set_config (pool, config))
    goto config_failed;

  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, nullptr);
  gst_query_add_allocation_meta (query,
      GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE, nullptr);
  gst_query_add_allocation_meta (query, GST_VIDEO_CROP_META_API_TYPE, nullptr);

  /* d3d11 buffer pool will update buffer size based on allocated texture,
   * get size from config again */
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_get_params (config, nullptr, &size, nullptr, nullptr);
  gst_structure_free (config);

  gst_query_add_allocation_pool (query, pool, size, 0, 0);

  gst_object_unref (pool);

  return TRUE;

  /* ERRORS */
config_failed:
  {
    GST_ERROR_OBJECT (filter, "failed to set config");
    gst_object_unref (pool);
    return FALSE;
  }
}

static gboolean
gst_d3d11_base_convert_decide_allocation (GstBaseTransform * trans,
    GstQuery * query)
{
  GstD3D11BaseFilter *filter = GST_D3D11_BASE_FILTER (trans);
  GstD3D11BaseConvert *self = GST_D3D11_BASE_CONVERT (trans);
  GstCaps *outcaps = NULL;
  GstBufferPool *pool = NULL;
  guint size, min = 0, max = 0;
  GstStructure *config;
  GstD3D11AllocationParams *d3d11_params;
  gboolean update_pool = FALSE;
  GstVideoInfo info;
  GstD3D11Format d3d11_format;
  guint bind_flags = 0;
  DXGI_FORMAT dxgi_format = DXGI_FORMAT_UNKNOWN;
  UINT supported = 0;
  HRESULT hr;
  ID3D11Device *device_handle;

  gst_query_parse_allocation (query, &outcaps, NULL);

  if (!outcaps)
    return FALSE;

  if (!gst_video_info_from_caps (&info, outcaps)) {
    GST_ERROR_OBJECT (filter, "Invalid caps %" GST_PTR_FORMAT, outcaps);
    return FALSE;
  }

  if (!gst_d3d11_device_get_format (filter->device,
          GST_VIDEO_INFO_FORMAT (&info), &d3d11_format)) {
    GST_ERROR_OBJECT (filter, "Unknown format caps %" GST_PTR_FORMAT, outcaps);
    return FALSE;
  }

  self->downstream_supports_crop_meta = gst_query_find_allocation_meta (query,
      GST_VIDEO_CROP_META_API_TYPE, nullptr);
  GST_DEBUG_OBJECT (self, "Downstream crop meta support: %d",
      self->downstream_supports_crop_meta);

  if (d3d11_format.dxgi_format == DXGI_FORMAT_UNKNOWN) {
    dxgi_format = d3d11_format.resource_format[0];
  } else {
    dxgi_format = d3d11_format.dxgi_format;
  }

  device_handle = gst_d3d11_device_get_device_handle (filter->device);
  hr = device_handle->CheckFormatSupport (dxgi_format, &supported);
  if (!gst_d3d11_result (hr, filter->device)) {
    GST_ERROR_OBJECT (self, "CheckFormatSupport failed");
    return FALSE;
  }

  if ((supported & D3D11_FORMAT_SUPPORT_RENDER_TARGET) != 0) {
    bind_flags |= D3D11_BIND_RENDER_TARGET;
    if (d3d11_format.dxgi_format == DXGI_FORMAT_UNKNOWN &&
        (supported & D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW) != 0) {
      bind_flags |= D3D11_BIND_UNORDERED_ACCESS;
    }
  } else {
    if (d3d11_format.dxgi_format != DXGI_FORMAT_UNKNOWN &&
        (supported & D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_OUTPUT) != 0) {
      bind_flags |= D3D11_BIND_RENDER_TARGET;
    }

    if ((supported & D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW) != 0)
      bind_flags |= D3D11_BIND_UNORDERED_ACCESS;
  }

  if ((supported & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) != 0)
    bind_flags |= D3D11_BIND_SHADER_RESOURCE;

  size = GST_VIDEO_INFO_SIZE (&info);

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);
    if (pool) {
      if (!GST_IS_D3D11_BUFFER_POOL (pool)) {
        gst_clear_object (&pool);
      } else {
        GstD3D11BufferPool *dpool = GST_D3D11_BUFFER_POOL (pool);
        if (dpool->device != filter->device)
          gst_clear_object (&pool);
      }
    }

    update_pool = TRUE;
  }

  if (!pool)
    pool = gst_d3d11_buffer_pool_new (filter->device);

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);

  d3d11_params = gst_buffer_pool_config_get_d3d11_allocation_params (config);
  if (!d3d11_params) {
    d3d11_params = gst_d3d11_allocation_params_new (filter->device, &info,
        GST_D3D11_ALLOCATION_FLAG_DEFAULT, bind_flags, 0);
  } else {
    gst_d3d11_allocation_params_set_bind_flags (d3d11_params, bind_flags);
  }

  gst_buffer_pool_config_set_d3d11_allocation_params (config, d3d11_params);
  gst_d3d11_allocation_params_free (d3d11_params);

  gst_buffer_pool_config_set_params (config, outcaps, size, min, max);
  gst_buffer_pool_set_config (pool, config);

  /* d3d11 buffer pool will update buffer size based on allocated texture,
   * get size from config again */
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_get_params (config, nullptr, &size, nullptr, nullptr);
  gst_structure_free (config);

  if (update_pool)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  gst_object_unref (pool);

  return GST_BASE_TRANSFORM_CLASS (parent_class)->decide_allocation (trans,
      query);
}

static gboolean
gst_d3d11_base_convert_needs_color_convert (GstD3D11BaseConvert * self,
    const GstVideoInfo * in_info, const GstVideoInfo * out_info)
{
  const GstVideoColorimetry *in_cinfo = &in_info->colorimetry;
  const GstVideoColorimetry *out_cinfo = &out_info->colorimetry;

  if (in_cinfo->range != out_cinfo->range ||
      in_cinfo->matrix != out_cinfo->matrix) {
    return TRUE;
  }

  if (self->primaries_mode != GST_VIDEO_PRIMARIES_MODE_NONE &&
      !gst_video_color_primaries_is_equivalent (in_cinfo->primaries,
          out_cinfo->primaries)) {
    return TRUE;
  }

  if (self->gamma_mode != GST_VIDEO_GAMMA_MODE_NONE &&
      !gst_video_transfer_function_is_equivalent (in_cinfo->transfer,
          GST_VIDEO_INFO_COMP_DEPTH (in_info, 0), out_cinfo->transfer,
          GST_VIDEO_INFO_COMP_DEPTH (out_info, 0))) {
    return TRUE;
  }

  return FALSE;
}

static void
gst_d3d11_base_convert_setup_msaa_texture (GstD3D11BaseConvert * self,
    GstD3D11Device * device, const GstVideoInfo * info, GstCaps * caps)
{
  ID3D11Device *device_handle;
  UINT quality_levels = 0;
  UINT sample_count = 1;
  GstD3D11Format device_format;
  DXGI_FORMAT dxgi_format;
  GstD3D11AllocationParams *params;
  GstStructure *config;
  HRESULT hr;

  gst_clear_buffer (&self->msaa_buf);
  if (self->msaa_pool) {
    gst_buffer_pool_set_active (self->msaa_pool, FALSE);
    gst_clear_object (&self->msaa_pool);
  }

  switch (self->msaa) {
    case GST_D3D11_MSAA_2X:
      sample_count = 2;
      break;
    case GST_D3D11_MSAA_4X:
      sample_count = 4;
      break;
    case GST_D3D11_MSAA_8X:
      sample_count = 8;
      break;
    default:
      return;
  }

  if (!gst_d3d11_device_get_format (device, GST_VIDEO_INFO_FORMAT (info),
          &device_format)) {
    GST_WARNING_OBJECT (self, "Couldn't get device format");
    return;
  }

  if (device_format.dxgi_format == DXGI_FORMAT_UNKNOWN ||
      (device_format.format_support[0] &
          D3D11_FORMAT_SUPPORT_RENDER_TARGET) == 0) {
    GST_DEBUG_OBJECT (self,
        "MSAA supported only for native and RTV available formats");;
    return;
  } else {
    dxgi_format = device_format.dxgi_format;
  }

  device_handle = gst_d3d11_device_get_device_handle (device);
  while (sample_count > 1) {
    hr = device_handle->CheckMultisampleQualityLevels (dxgi_format,
        sample_count, &quality_levels);
    if (gst_d3d11_result (hr, device) && quality_levels > 0)
      break;

    sample_count = sample_count / 2;
  };

  if (sample_count <= 1 || quality_levels == 0) {
    GST_DEBUG_OBJECT (self, "Device does not support MSAA for DXGI format %d",
        dxgi_format);
    return;
  }

  self->msaa_pool = gst_d3d11_buffer_pool_new (device);
  config = gst_buffer_pool_get_config (self->msaa_pool);

  params = gst_d3d11_allocation_params_new (device, info,
      GST_D3D11_ALLOCATION_FLAG_DEFAULT, D3D11_BIND_RENDER_TARGET, 0);
  gst_d3d11_allocation_params_set_sample_desc (params,
      sample_count, quality_levels - 1);

  gst_buffer_pool_config_set_d3d11_allocation_params (config, params);
  gst_d3d11_allocation_params_free (params);

  gst_buffer_pool_config_set_params (config, caps, info->size, 0, 0);
  if (!gst_buffer_pool_set_config (self->msaa_pool, config)) {
    GST_ERROR_OBJECT (self, "Couldn't set pool config");
    gst_clear_object (&self->msaa_pool);
    return;
  }

  if (!gst_buffer_pool_set_active (self->msaa_pool, TRUE)) {
    GST_ERROR_OBJECT (self, "Pool active failed");
    gst_clear_object (&self->msaa_pool);
    return;
  }

  gst_buffer_pool_acquire_buffer (self->msaa_pool, &self->msaa_buf, nullptr);
  if (!self->msaa_buf) {
    GST_ERROR_OBJECT (self, "Couldn't acquire MSAA buffer");
    gst_buffer_pool_set_active (self->msaa_pool, FALSE);
    gst_clear_object (&self->msaa_pool);
    return;
  }

  GST_DEBUG_OBJECT (self, "MSAA %dx buffer is configured", sample_count);
}

static gboolean
gst_d3d11_base_convert_set_info (GstD3D11BaseFilter * filter,
    GstCaps * incaps, GstVideoInfo * in_info, GstCaps * outcaps,
    GstVideoInfo * out_info)
{
  GstD3D11BaseConvert *self = GST_D3D11_BASE_CONVERT (filter);
  gint from_dar_n, from_dar_d, to_dar_n, to_dar_d;
  gint border_offset_x = 0;
  gint border_offset_y = 0;
  gboolean need_flip = FALSE;
  gboolean need_transform = FALSE;
  gint in_width, in_height, in_par_n, in_par_d;
  gint out_width, out_height;
  GstStructure *config;

  GstD3D11SRWLockGuard lk (&self->lock);
  self->active_method = self->selected_method;
  self->active_add_borders = self->add_borders;
  self->active_gamma_mode = self->gamma_mode;
  self->active_primaries_mode = self->primaries_mode;
  self->active_sampling_method = self->sampling_method;

  self->transform_updated = FALSE;
  self->msaa_updated = FALSE;

  GST_DEBUG_OBJECT (self, "method %d, add-borders %d, gamma-mode %d, "
      "primaries-mode %d, sampling %d", self->active_method,
      self->active_add_borders, self->active_gamma_mode,
      self->active_primaries_mode, self->active_sampling_method);

  if (self->active_method != GST_VIDEO_ORIENTATION_IDENTITY)
    need_flip = TRUE;

  need_transform = gst_d3d11_need_transform (self->rotation_x, self->rotation_y,
      self->rotation_z, self->scale_x, self->scale_y);

  if (!need_flip && !need_transform && gst_caps_is_equal (incaps, outcaps)) {
    self->same_caps = TRUE;
  } else {
    self->same_caps = FALSE;
  }

  switch (self->selected_method) {
    case GST_VIDEO_ORIENTATION_90R:
    case GST_VIDEO_ORIENTATION_90L:
    case GST_VIDEO_ORIENTATION_UL_LR:
    case GST_VIDEO_ORIENTATION_UR_LL:
      in_width = in_info->height;
      in_height = in_info->width;
      in_par_n = in_info->par_d;
      in_par_d = in_info->par_n;
      break;
    default:
      in_width = in_info->width;
      in_height = in_info->height;
      in_par_n = in_info->par_n;
      in_par_d = in_info->par_d;
      break;
  }

  if (!gst_util_fraction_multiply (in_width,
          in_height, in_par_n, in_par_d, &from_dar_n, &from_dar_d)) {
    from_dar_n = from_dar_d = -1;
  }

  if (!gst_util_fraction_multiply (out_info->width,
          out_info->height, out_info->par_n, out_info->par_d, &to_dar_n,
          &to_dar_d)) {
    to_dar_n = to_dar_d = -1;
  }

  self->borders_w = self->borders_h = 0;
  if (to_dar_n != from_dar_n || to_dar_d != from_dar_d) {
    if (self->active_add_borders) {
      gint n, d, to_h, to_w;

      if (from_dar_n != -1 && from_dar_d != -1
          && gst_util_fraction_multiply (from_dar_n, from_dar_d,
              out_info->par_d, out_info->par_n, &n, &d)) {
        to_h = gst_util_uint64_scale_int (out_info->width, d, n);
        if (to_h <= out_info->height) {
          self->borders_h = out_info->height - to_h;
          self->borders_w = 0;
        } else {
          to_w = gst_util_uint64_scale_int (out_info->height, n, d);
          g_assert (to_w <= out_info->width);
          self->borders_h = 0;
          self->borders_w = out_info->width - to_w;
        }
      } else {
        GST_WARNING_OBJECT (self, "Can't calculate borders");
      }
    } else {
      GST_INFO_OBJECT (self, "Display aspect ratio update %d/%d -> %d/%d",
          from_dar_n, from_dar_d, to_dar_n, to_dar_d);
    }
  }

  gst_clear_object (&self->converter);

  GST_DEBUG_OBJECT (self, "Setup convert with format %s -> %s",
      gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (in_info)),
      gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (out_info)));

  /* if present, these must match */
  if (in_info->interlace_mode != out_info->interlace_mode) {
    GST_ERROR_OBJECT (self, "input and output formats do not match");
    return FALSE;
  }

  if (in_width == out_info->width && in_height == out_info->height
      && in_info->finfo == out_info->finfo && self->borders_w == 0 &&
      self->borders_h == 0 && !need_flip && !need_transform &&
      !gst_d3d11_base_convert_needs_color_convert (self, in_info, out_info)) {
    self->same_caps = TRUE;
  }

  config = gst_structure_new ("convert-config",
      GST_D3D11_CONVERTER_OPT_GAMMA_MODE,
      GST_TYPE_VIDEO_GAMMA_MODE, self->active_gamma_mode,
      GST_D3D11_CONVERTER_OPT_PRIMARIES_MODE,
      GST_TYPE_VIDEO_PRIMARIES_MODE, self->active_primaries_mode,
      GST_D3D11_CONVERTER_OPT_SAMPLER_FILTER,
      GST_TYPE_D3D11_CONVERTER_SAMPLER_FILTER,
      gst_d3d11_sampling_method_to_native
      (self->active_sampling_method),
      GST_D3D11_CONVERTER_OPT_SRC_ALPHA_MODE,
      GST_TYPE_D3D11_CONVERTER_ALPHA_MODE,
      self->src_alpha_mode,
      GST_D3D11_CONVERTER_OPT_DEST_ALPHA_MODE,
      GST_TYPE_D3D11_CONVERTER_ALPHA_MODE, self->dst_alpha_mode, nullptr);

  self->converter = gst_d3d11_converter_new (filter->device, in_info, out_info,
      config);
  if (!self->converter) {
    GST_ERROR_OBJECT (self, "Couldn't create converter");
    return FALSE;
  }

  GST_DEBUG_OBJECT (self, "from=%dx%d (par=%d/%d dar=%d/%d), size %"
      G_GSIZE_FORMAT " -> to=%dx%d (par=%d/%d dar=%d/%d borders=%d:%d), "
      "size %" G_GSIZE_FORMAT ", orientation: %d",
      in_info->width, in_info->height, in_info->par_n, in_info->par_d,
      from_dar_n, from_dar_d, in_info->size, out_info->width,
      out_info->height, out_info->par_n, out_info->par_d, to_dar_n, to_dar_d,
      self->borders_w, self->borders_h, out_info->size, self->active_method);

  self->in_rect.left = 0;
  self->in_rect.top = 0;
  self->in_rect.right = GST_VIDEO_INFO_WIDTH (in_info);
  self->in_rect.bottom = GST_VIDEO_INFO_HEIGHT (in_info);
  self->prev_in_rect = self->in_rect;

  if (self->borders_w) {
    border_offset_x = self->borders_w / 2;
    self->out_rect.left = border_offset_x;
    self->out_rect.right = GST_VIDEO_INFO_WIDTH (out_info) - border_offset_x;
  } else {
    self->out_rect.left = 0;
    self->out_rect.right = GST_VIDEO_INFO_WIDTH (out_info);
  }

  if (self->borders_h) {
    border_offset_y = self->borders_h / 2;
    self->out_rect.top = border_offset_y;
    self->out_rect.bottom = GST_VIDEO_INFO_HEIGHT (out_info) - border_offset_y;
  } else {
    self->out_rect.top = 0;
    self->out_rect.bottom = GST_VIDEO_INFO_HEIGHT (out_info);
  }

  out_width = self->out_rect.right - self->out_rect.left;
  out_height = self->out_rect.bottom - self->out_rect.top;

  g_object_set (self->converter, "dest-x", (gint) self->out_rect.left,
      "dest-y", (gint) self->out_rect.top,
      "dest-width", out_width, "dest-height", out_height, nullptr);

  if (need_transform) {
    GST_DEBUG_OBJECT (self, "Applying custom transform");

    gst_d3d11_converter_apply_transform (self->converter, self->active_method,
        (gfloat) out_width, (gfloat) out_height, self->fov,
        self->ortho, self->rotation_x, self->rotation_y,
        self->rotation_z, self->scale_x, self->scale_y);
  } else {
    g_object_set (self->converter,
        "video-direction", self->active_method, nullptr);
  }

  if (self->borders_w > 0 || self->borders_h > 0 || need_transform) {
    g_object_set (self->converter, "fill-border", TRUE, "border-color",
        self->border_color, nullptr);
  }

  gst_d3d11_base_convert_setup_msaa_texture (self, filter->device,
      out_info, outcaps);

  return TRUE;
}

static gboolean
gst_d3d11_base_convert_crop_and_copy (GstD3D11BaseConvert * self,
    const GstVideoCropMeta * meta, GstBuffer * inbuf, GstBuffer * outbuf)
{
  GstD3D11BaseFilter *filter = GST_D3D11_BASE_FILTER_CAST (self);
  GstVideoInfo *info = &filter->in_info;
  GstVideoFormat format = GST_VIDEO_INFO_FORMAT (info);
  GstMemory *in_mem, *out_mem;
  GstD3D11Memory *in_dmem, *out_dmem;
  GstMapInfo in_map, out_map;
  GstD3D11Device *device;
  ID3D11DeviceContext *context;
  D3D11_BOX src_box = { 0, };
  guint in_subresource;
  guint out_subresource;
  ID3D11Texture2D *in_tex, *out_tex;

  /* Copy into output memory */
  in_mem = gst_buffer_peek_memory (inbuf, 0);
  out_mem = gst_buffer_peek_memory (outbuf, 0);

  if (!gst_is_d3d11_memory (in_mem)) {
    GST_ERROR_OBJECT (self, "Input is not a d3d11 memory");
    return FALSE;
  }

  if (!gst_is_d3d11_memory (out_mem)) {
    GST_ERROR_OBJECT (self, "Output is not a d3d11 memory");
    return FALSE;
  }

  in_dmem = GST_D3D11_MEMORY_CAST (in_mem);
  out_dmem = GST_D3D11_MEMORY_CAST (out_mem);

  if (in_dmem->device != out_dmem->device) {
    GST_ERROR_OBJECT (self, "Different device");
    return FALSE;
  }

  device = in_dmem->device;
  context = gst_d3d11_device_get_device_context_handle (device);

  src_box.left = meta->x;
  src_box.top = meta->y;
  src_box.right = meta->x + meta->width;
  src_box.bottom = meta->y + meta->height;
  src_box.front = 0;
  src_box.back = 1;

  GST_TRACE_OBJECT (self, "Source box left:top:right:bottom = %d, %d, %d, %d",
      src_box.left, src_box.top, src_box.right, src_box.bottom);

  in_subresource = gst_d3d11_memory_get_subresource_index (in_dmem);
  out_subresource = gst_d3d11_memory_get_subresource_index (out_dmem);

  GstD3D11DeviceLockGuard lk (device);
  if (!gst_memory_map (in_mem, &in_map,
          (GstMapFlags) (GST_MAP_READ | GST_MAP_D3D11))) {
    GST_ERROR_OBJECT (self, "Failed to map input memory");
    return FALSE;
  }

  if (!gst_memory_map (out_mem, &out_map,
          (GstMapFlags) (GST_MAP_WRITE | GST_MAP_D3D11))) {
    GST_ERROR_OBJECT (self, "Failed to map output memory");
    gst_memory_unmap (in_mem, &in_map);
    return FALSE;
  }

  in_tex = (ID3D11Texture2D *) in_map.data;
  out_tex = (ID3D11Texture2D *) out_map.data;

  context->CopySubresourceRegion (out_tex, out_subresource, 0, 0, 0,
      in_tex, in_subresource, &src_box);
  gst_memory_unmap (in_mem, &in_map);
  gst_memory_unmap (out_mem, &out_map);

  if (gst_buffer_n_memory (inbuf) == 1)
    return TRUE;

  /* Non-native DXGI format YUV cases, copy UV plane(s) */
  switch (format) {
      /* semi-planar */
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_NV21:
    case GST_VIDEO_FORMAT_P010_10LE:
    case GST_VIDEO_FORMAT_P012_LE:
    case GST_VIDEO_FORMAT_P016_LE:
      src_box.top = GST_ROUND_DOWN_2 (meta->y) / 2;
      src_box.bottom = GST_ROUND_DOWN_2 (meta->y + meta->height) / 2;
      break;
      /* planar */
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_YV12:
    case GST_VIDEO_FORMAT_I420_10LE:
    case GST_VIDEO_FORMAT_I420_12LE:
      src_box.left = GST_ROUND_DOWN_2 (meta->x) / 2;
      src_box.top = GST_ROUND_DOWN_2 (meta->y) / 2;
      src_box.right = GST_ROUND_DOWN_2 (meta->x + meta->width) / 2;
      src_box.bottom = GST_ROUND_DOWN_2 (meta->y + meta->height) / 2;
      break;
    case GST_VIDEO_FORMAT_Y42B:
    case GST_VIDEO_FORMAT_I422_10LE:
    case GST_VIDEO_FORMAT_I422_12LE:
      src_box.left = GST_ROUND_DOWN_2 (meta->x) / 2;
      src_box.right = GST_ROUND_DOWN_2 (meta->x + meta->width) / 2;
      break;
    case GST_VIDEO_FORMAT_Y444:
    case GST_VIDEO_FORMAT_Y444_10LE:
    case GST_VIDEO_FORMAT_Y444_12LE:
    case GST_VIDEO_FORMAT_Y444_16LE:
      break;
    default:
      GST_ERROR_OBJECT (self, "Unexpected format %s",
          gst_video_format_to_string (format));
      return FALSE;
  }

  GST_TRACE_OBJECT (self, "UV left:top:right:bottom = %d, %d, %d, %d",
      src_box.left, src_box.top, src_box.right, src_box.bottom);

  for (guint i = 1; i < GST_VIDEO_INFO_N_PLANES (info); i++) {
    in_mem = gst_buffer_peek_memory (inbuf, i);
    out_mem = gst_buffer_peek_memory (outbuf, i);

    if (!gst_is_d3d11_memory (in_mem)) {
      GST_ERROR_OBJECT (self, "Input is not a d3d11 memory");
      return FALSE;
    }

    if (!gst_is_d3d11_memory (out_mem)) {
      GST_ERROR_OBJECT (self, "Output is not a d3d11 memory");
      return FALSE;
    }

    in_dmem = GST_D3D11_MEMORY_CAST (in_mem);
    out_dmem = GST_D3D11_MEMORY_CAST (out_mem);

    if (in_dmem->device != out_dmem->device) {
      GST_ERROR_OBJECT (self, "Different device");
      return FALSE;
    }

    in_subresource = gst_d3d11_memory_get_subresource_index (in_dmem);
    out_subresource = gst_d3d11_memory_get_subresource_index (out_dmem);

    if (!gst_memory_map (in_mem, &in_map,
            (GstMapFlags) (GST_MAP_READ | GST_MAP_D3D11))) {
      GST_ERROR_OBJECT (self, "Failed to map input memory");
      return FALSE;
    }

    if (!gst_memory_map (out_mem, &out_map,
            (GstMapFlags) (GST_MAP_WRITE | GST_MAP_D3D11))) {
      GST_ERROR_OBJECT (self, "Failed to map output memory");
      gst_memory_unmap (in_mem, &in_map);
      return FALSE;
    }

    in_tex = (ID3D11Texture2D *) in_map.data;
    out_tex = (ID3D11Texture2D *) out_map.data;

    context->CopySubresourceRegion (out_tex, out_subresource, 0, 0, 0,
        in_tex, in_subresource, &src_box);
    gst_memory_unmap (in_mem, &in_map);
    gst_memory_unmap (out_mem, &out_map);
  }

  return TRUE;
}

static gboolean
gst_d3d11_base_convert_need_convert (GstD3D11BaseConvert * self)
{
  if (!self->same_caps)
    return TRUE;

  if (self->src_alpha_mode == GST_D3D11_ALPHA_MODE_PREMULTIPLIED ||
      self->dst_alpha_mode == GST_D3D11_ALPHA_MODE_PREMULTIPLIED) {
    return TRUE;
  }

  return FALSE;
}

static GstFlowReturn
gst_d3d11_base_convert_generate_output (GstBaseTransform * trans,
    GstBuffer ** buffer)
{
  GstD3D11BaseConvert *self = GST_D3D11_BASE_CONVERT (trans);
  GstBuffer *inbuf;
  GstVideoCropMeta *crop_meta;
  GstFlowReturn ret;

  if (gst_d3d11_base_convert_need_convert (self)) {
    return GST_BASE_TRANSFORM_CLASS (parent_class)->generate_output (trans,
        buffer);
  }

  /* Retrieve stashed input buffer, if the default submit_input_buffer
   * was run. Takes ownership back from there */
  inbuf = trans->queued_buf;
  trans->queued_buf = nullptr;

  /* This default processing method needs one input buffer to feed to
   * the transform functions, we can't do anything without it */
  if (!inbuf)
    return GST_FLOW_OK;

  crop_meta = gst_buffer_get_video_crop_meta (inbuf);

  /* downstream supports crop meta or no crop meta. Just passthrough it */
  if (self->downstream_supports_crop_meta || !crop_meta) {
    *buffer = inbuf;
    return GST_FLOW_OK;
  }

  ret = GST_BASE_TRANSFORM_CLASS (parent_class)->prepare_output_buffer (trans,
      inbuf, buffer);
  if (ret != GST_FLOW_OK || *buffer == nullptr) {
    GST_WARNING_OBJECT (trans, "Could not get buffer from pool, %s",
        gst_flow_get_name (ret));
    gst_buffer_unref (inbuf);
    return ret;
  }

  GST_TRACE_OBJECT (self, "Copying cropped buffer");
  if (!gst_d3d11_base_convert_crop_and_copy (self, crop_meta, inbuf, *buffer)) {
    gst_buffer_unref (inbuf);
    gst_clear_buffer (buffer);

    return GST_FLOW_ERROR;
  }

  gst_buffer_unref (inbuf);

  return GST_FLOW_OK;
}

static gboolean
gst_d3d11_base_convert_transform_meta (GstBaseTransform * trans,
    GstBuffer * outbuf, GstMeta * meta, GstBuffer * inbuf)
{
  /* Do not copy crop meta in any case.
   *
   * 1) When input and output caps are identical,
   * - If downstream supports crop meta or crop meta is not attached on input
   *   buffer, then we do passthrough input buffers.
   *   In that case, this method must not be called already
   * - Otherwise (downstream does not support crop meta), we do crop input
   *   and copy cropped area to output buffer
   * 2) in case of input-caps != output-caps, we specify source rectangle to
   *   shader or video processor object. Then the conversion object will
   *   consider source cropping area automatically
   */
  if (meta->info->api == GST_VIDEO_CROP_META_API_TYPE)
    return FALSE;

  return GST_BASE_TRANSFORM_CLASS (parent_class)->transform_meta (trans,
      outbuf, meta, inbuf);
}

static void
gst_d3d11_base_convert_before_transform (GstBaseTransform * trans,
    GstBuffer * buffer)
{
  GstD3D11BaseConvert *self = GST_D3D11_BASE_CONVERT (trans);
  GstCaps *in_caps;
  GstCaps *out_caps;
  GstBaseTransformClass *klass;
  gboolean update = FALSE;

  GST_BASE_TRANSFORM_CLASS (parent_class)->before_transform (trans, buffer);

  AcquireSRWLockExclusive (&self->lock);
  if (self->selected_method != self->active_method ||
      self->add_borders != self->active_add_borders ||
      self->gamma_mode != self->active_gamma_mode ||
      self->primaries_mode != self->active_primaries_mode ||
      self->sampling_method != self->active_sampling_method ||
      self->transform_updated || self->msaa_updated) {
    update = TRUE;
  }
  ReleaseSRWLockExclusive (&self->lock);

  if (!update)
    return;

  GST_DEBUG_OBJECT (self, "Updating caps for property change");

  in_caps = gst_pad_get_current_caps (GST_BASE_TRANSFORM_SINK_PAD (trans));
  if (!in_caps) {
    GST_WARNING_OBJECT (trans, "sinkpad has no current caps");
    return;
  }

  out_caps = gst_pad_get_current_caps (GST_BASE_TRANSFORM_SRC_PAD (trans));
  if (!out_caps) {
    GST_WARNING_OBJECT (trans, "srcpad has no current caps");
    gst_caps_unref (in_caps);
    return;
  }

  klass = GST_BASE_TRANSFORM_GET_CLASS (trans);
  klass->set_caps (trans, in_caps, out_caps);
  gst_caps_unref (in_caps);
  gst_caps_unref (out_caps);

  gst_base_transform_reconfigure_src (trans);
}

static GstFlowReturn
gst_d3d11_base_convert_transform (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf)
{
  GstD3D11BaseConvert *self = GST_D3D11_BASE_CONVERT (trans);
  GstD3D11BaseFilter *filter = GST_D3D11_BASE_FILTER (trans);
  RECT in_rect;
  GstVideoCropMeta *crop_meta;
  GstD3D11Device *device = filter->device;
  GstBuffer *target_buf;
  GstFlowReturn ret = GST_FLOW_OK;

  crop_meta = gst_buffer_get_video_crop_meta (inbuf);
  if (crop_meta) {
    GST_TRACE_OBJECT (self, "Have crop rect, x:y:w:h = %d:%d:%d:%d",
        crop_meta->x, crop_meta->y, crop_meta->width, crop_meta->height);

    in_rect.left = crop_meta->x;
    in_rect.top = crop_meta->y;
    in_rect.right = crop_meta->x + crop_meta->width;
    in_rect.bottom = crop_meta->y + crop_meta->height;
  } else {
    in_rect = self->in_rect;
  }

  if (in_rect.left != self->prev_in_rect.left ||
      in_rect.top != self->prev_in_rect.top ||
      in_rect.right != self->prev_in_rect.right ||
      in_rect.bottom != self->prev_in_rect.bottom) {
    self->prev_in_rect = in_rect;
    g_object_set (self->converter, "src-x", (gint) in_rect.left,
        "src-y", (gint) in_rect.top,
        "src-width", (gint) in_rect.right - in_rect.left,
        "src-height", (gint) in_rect.bottom - in_rect.top, nullptr);
  }

  target_buf = self->msaa_buf ? self->msaa_buf : outbuf;

  gst_d3d11_device_lock (device);
  if (!gst_d3d11_converter_convert_buffer_unlocked (self->converter,
          inbuf, target_buf)) {
    ret = GST_FLOW_ERROR;
  }

  if (ret == GST_FLOW_OK && target_buf == self->msaa_buf) {
    guint num_memory = gst_buffer_n_memory (self->msaa_buf);
    ID3D11DeviceContext *context =
        gst_d3d11_device_get_device_context_handle (device);

    for (guint i = 0; i < num_memory; i++) {
      GstMemory *src_mem;
      GstMemory *dst_mem;
      GstD3D11Memory *dst_dmem;
      GstMapInfo src_map, dst_map;
      guint subresource_idx;
      D3D11_TEXTURE2D_DESC desc;

      src_mem = gst_buffer_peek_memory (self->msaa_buf, i);
      dst_mem = gst_buffer_peek_memory (outbuf, i);
      dst_dmem = GST_D3D11_MEMORY_CAST (dst_mem);

      subresource_idx = gst_d3d11_memory_get_subresource_index (dst_dmem);
      gst_d3d11_memory_get_texture_desc (dst_dmem, &desc);

      if (!gst_memory_map (src_mem, &src_map,
              (GstMapFlags) (GST_MAP_D3D11 | GST_MAP_READ))) {
        GST_ERROR_OBJECT (self, "Couldn't map src mem");
        ret = GST_FLOW_ERROR;
        break;
      }

      if (!gst_memory_map (dst_mem, &dst_map,
              (GstMapFlags) (GST_MAP_D3D11 | GST_MAP_WRITE))) {
        GST_ERROR_OBJECT (self, "Couldn't map dst mem");
        gst_memory_unmap (src_mem, &src_map);
        ret = GST_FLOW_ERROR;
        break;
      }

      context->ResolveSubresource ((ID3D11Resource *) dst_map.data,
          subresource_idx, (ID3D11Resource *) src_map.data, 0, desc.Format);

      gst_memory_unmap (dst_mem, &dst_map);
      gst_memory_unmap (src_mem, &src_map);
    }
  }
  gst_d3d11_device_unlock (device);

  if (ret != GST_FLOW_OK) {
    GST_ELEMENT_ERROR (self, CORE, FAILED, (nullptr),
        ("Couldn't convert texture"));
  }

  return ret;
}

static void
gst_d3d11_base_convert_set_add_border (GstD3D11BaseConvert * self,
    gboolean add_border)
{
  GstD3D11SRWLockGuard lk (&self->lock);

  self->add_borders = add_border;
  if (self->add_borders != self->active_add_borders)
    gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM_CAST (self));
}

static void
gst_d3d11_base_convert_set_border_color (GstD3D11BaseConvert * self,
    guint64 border_color)
{
  GstD3D11SRWLockGuard lk (&self->lock);
  self->border_color = border_color;
  if (self->converter)
    g_object_set (self->converter, "border-color", self->border_color, nullptr);
}

static void
gst_d3d11_base_convert_set_orientation (GstD3D11BaseConvert * self,
    GstVideoOrientationMethod method, gboolean from_tag)
{
  if (method == GST_VIDEO_ORIENTATION_CUSTOM) {
    GST_WARNING_OBJECT (self, "Unsupported custom orientation");
    return;
  }

  GstD3D11SRWLockGuard lk (&self->lock);
  if (from_tag)
    self->tag_method = method;
  else
    self->method = method;

  if (self->method == GST_VIDEO_ORIENTATION_AUTO) {
    self->selected_method = self->tag_method;
  } else {
    self->selected_method = self->method;
  }

  if (self->selected_method != self->active_method) {
    GST_DEBUG_OBJECT (self, "Rotation orientation %d -> %d",
        self->active_method, self->selected_method);

    gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM (self));
  }
}

static void
gst_d3d11_base_convert_set_gamma_mode (GstD3D11BaseConvert * self,
    GstVideoGammaMode mode)
{
  GstD3D11SRWLockGuard lk (&self->lock);
  GstVideoGammaMode prev_mode = self->gamma_mode;
  self->gamma_mode = mode;

  if (self->gamma_mode != self->active_gamma_mode) {
    GST_DEBUG_OBJECT (self, "Gamma mode %d -> %d", prev_mode, self->gamma_mode);
    gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM (self));
  }
}

static void
gst_d3d11_base_convert_set_primaries_mode (GstD3D11BaseConvert * self,
    GstVideoPrimariesMode mode)
{
  GstD3D11SRWLockGuard lk (&self->lock);
  GstVideoPrimariesMode prev_mode = self->primaries_mode;
  self->primaries_mode = mode;

  if (self->primaries_mode != self->active_primaries_mode) {
    gboolean prev_enabled = TRUE;
    gboolean new_enabled = TRUE;

    GST_DEBUG_OBJECT (self, "Primaries mode %d -> %d",
        prev_mode, self->primaries_mode);

    if (prev_mode == GST_VIDEO_PRIMARIES_MODE_NONE)
      prev_enabled = FALSE;

    if (self->primaries_mode == GST_VIDEO_PRIMARIES_MODE_NONE)
      new_enabled = FALSE;

    if (prev_enabled != new_enabled)
      gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM (self));
    else
      self->active_primaries_mode = self->primaries_mode;
  }
}

static void
gst_d3d11_base_convert_set_sampling_method (GstD3D11BaseConvert * self,
    GstD3D11SamplingMethod method)
{
  GstD3D11SRWLockGuard lk (&self->lock);

  GST_DEBUG_OBJECT (self,
      "Sampling method %d -> %d", self->sampling_method, method);

  self->sampling_method = method;
  if (self->sampling_method != self->active_sampling_method)
    gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM_CAST (self));
}

static void
gst_d3d11_base_convert_set_src_alpha_mode (GstD3D11BaseConvert * self,
    GstD3D11AlphaMode mode)
{
  GstD3D11SRWLockGuard lk (&self->lock);

  if (self->src_alpha_mode != mode) {
    self->src_alpha_mode = mode;
    gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM_CAST (self));
  }
}

static void
gst_d3d11_base_convert_set_dst_alpha_mode (GstD3D11BaseConvert * self,
    GstD3D11AlphaMode mode)
{
  GstD3D11SRWLockGuard lk (&self->lock);

  if (self->dst_alpha_mode != mode) {
    self->dst_alpha_mode = mode;
    gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM_CAST (self));
  }
}

static void
gst_d3d11_base_convert_set_fov (GstD3D11BaseConvert * self, gfloat fov)
{
  GstD3D11SRWLockGuard lk (&self->lock);

  if (self->fov != fov) {
    self->fov = fov;
    self->transform_updated = TRUE;
    gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM_CAST (self));
  }
}

static void
gst_d3d11_base_convert_set_ortho (GstD3D11BaseConvert * self, gboolean ortho)
{
  GstD3D11SRWLockGuard lk (&self->lock);

  if (self->ortho != ortho) {
    self->ortho = ortho;
    self->transform_updated = TRUE;
    gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM_CAST (self));
  }
}

static void
gst_d3d11_base_convert_set_rotation_x (GstD3D11BaseConvert * self,
    gfloat rotation_x)
{
  GstD3D11SRWLockGuard lk (&self->lock);

  if (self->rotation_x != rotation_x) {
    self->rotation_x = rotation_x;
    self->transform_updated = TRUE;
    gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM_CAST (self));
  }
}

static void
gst_d3d11_base_convert_set_rotation_y (GstD3D11BaseConvert * self,
    gfloat rotation_y)
{
  GstD3D11SRWLockGuard lk (&self->lock);

  if (self->rotation_y != rotation_y) {
    self->rotation_y = rotation_y;
    self->transform_updated = TRUE;
    gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM_CAST (self));
  }
}

static void
gst_d3d11_base_convert_set_rotation_z (GstD3D11BaseConvert * self,
    gfloat rotation_z)
{
  GstD3D11SRWLockGuard lk (&self->lock);

  if (self->rotation_z != rotation_z) {
    self->rotation_z = rotation_z;
    self->transform_updated = TRUE;
    gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM_CAST (self));
  }
}

static void
gst_d3d11_base_convert_set_scale_x (GstD3D11BaseConvert * self, gfloat scale_x)
{
  GstD3D11SRWLockGuard lk (&self->lock);

  if (self->scale_x != scale_x) {
    self->scale_x = scale_x;
    self->transform_updated = TRUE;
    gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM_CAST (self));
  }
}

static void
gst_d3d11_base_convert_set_scale_y (GstD3D11BaseConvert * self, gfloat scale_y)
{
  GstD3D11SRWLockGuard lk (&self->lock);

  if (self->scale_y != scale_y) {
    self->scale_y = scale_y;
    self->transform_updated = TRUE;
    gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM_CAST (self));
  }
}

static void
gst_d3d11_base_convert_set_msaa (GstD3D11BaseConvert * self,
    GstD3D11MSAAMode msaa)
{
  GstD3D11SRWLockGuard lk (&self->lock);

  if (self->msaa != msaa) {
    self->msaa = msaa;
    self->msaa_updated = TRUE;
    gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM_CAST (self));
  }
}

/**
 * SECTION:element-d3d11convert
 * @title: d3d11convert
 * @short_description: A Direct3D11 based color conversion and video resizing element
 *
 * This element resizes video frames and change color space.
 * By default the element will try to negotiate to the same size on the source
 * and sinkpad so that no scaling is needed.
 * It is therefore safe to insert this element in a pipeline to
 * get more robust behaviour without any cost if no scaling is needed.
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 videotestsrc ! video/x-raw,format=NV12 ! d3d11upload ! d3d11convert ! d3d11videosink
 * ```
 *  This will output a test video (generated in NV12 format) in a video
 * window. If the video sink selected does not support NV12
 * d3d11convert will automatically convert the video to a format understood
 * by the video sink.
 *
 * Since: 1.18
 *
 */

enum
{
  PROP_CONVERT_0,
  PROP_CONVERT_ADD_BORDERS,
  PROP_CONVERT_BORDER_COLOR,
  PROP_CONVERT_VIDEO_DIRECTION,
  PROP_CONVERT_GAMMA_MODE,
  PROP_CONVERT_PRIMARIES_MODE,
  PROP_CONVERT_SRC_ALPHA_MODE,
  PROP_CONVERT_DEST_ALPHA_MODE,
  PROP_CONVERT_FOV,
  PROP_CONVERT_ORTHO,
  PROP_CONVERT_ROTATION_X,
  PROP_CONVERT_ROTATION_Y,
  PROP_CONVERT_ROTATION_Z,
  PROP_CONVERT_SCALE_X,
  PROP_CONVERT_SCALE_Y,
  PROP_CONVERT_MSAA,
};

struct _GstD3D11Convert
{
  GstD3D11BaseConvert parent;
};

static void
gst_d3d11_convert_video_direction_interface_init (GstVideoDirectionInterface *
    iface)
{
}

G_DEFINE_TYPE_WITH_CODE (GstD3D11Convert, gst_d3d11_convert,
    GST_TYPE_D3D11_BASE_CONVERT,
    G_IMPLEMENT_INTERFACE (GST_TYPE_VIDEO_DIRECTION,
        gst_d3d11_convert_video_direction_interface_init));

static void gst_d3d11_convert_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_d3d11_convert_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static gboolean gst_d3d11_convert_sink_event (GstBaseTransform * trans,
    GstEvent * event);

static void
gst_d3d11_convert_class_init (GstD3D11ConvertClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (klass);

  gobject_class->set_property = gst_d3d11_convert_set_property;
  gobject_class->get_property = gst_d3d11_convert_get_property;

  /**
   * GstD3D11Convert:add-borders:
   *
   * Add borders if necessary to keep the display aspect ratio
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class, PROP_CONVERT_ADD_BORDERS,
      g_param_spec_boolean ("add-borders", "Add Borders",
          "Add black borders if necessary to keep the display aspect ratio",
          DEFAULT_ADD_BORDERS, (GParamFlags) (GST_PARAM_MUTABLE_PLAYING |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11Convert:border-color:
   *
   * Border color to use in ARGB64 format
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_CONVERT_BORDER_COLOR,
      g_param_spec_uint64 ("border-color", "Border color",
          "Border color to use in ARGB64 format", 0, G_MAXUINT64,
          DEFAULT_BORDER_COLOR, (GParamFlags) (GST_PARAM_MUTABLE_PLAYING |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11Convert:video-direction:
   *
   * Video rotation/flip method to use
   *
   * Since: 1.22
   */
  g_object_class_override_property (gobject_class, PROP_CONVERT_VIDEO_DIRECTION,
      "video-direction");

  /**
   * GstD3D11Convert:gamma-mode:
   *
   * Gamma conversion mode
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_CONVERT_GAMMA_MODE,
      g_param_spec_enum ("gamma-mode", "Gamma mode",
          "Gamma conversion mode", GST_TYPE_VIDEO_GAMMA_MODE,
          DEFAULT_GAMMA_MODE, (GParamFlags) (GST_PARAM_MUTABLE_PLAYING |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11Convert:primaries-mode:
   *
   * Primaries conversion mode
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_CONVERT_PRIMARIES_MODE,
      g_param_spec_enum ("primaries-mode", "Primaries Mode",
          "Primaries conversion mode", GST_TYPE_VIDEO_PRIMARIES_MODE,
          DEFAULT_PRIMARIES_MODE, (GParamFlags) (GST_PARAM_MUTABLE_PLAYING |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11Convert:src-alpha-mode:
   *
   * Input stream's applied alpha mode. In case of "premultiplied",
   * premultiplied to straight alpha conversion will be performed
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_CONVERT_SRC_ALPHA_MODE,
      g_param_spec_enum ("src-alpha-mode", "Src Alpha Mode",
          "Applied input alpha mode",
          GST_TYPE_D3D11_ALPHA_MODE, DEFAULT_ALPHA_MODE,
          (GParamFlags) (GST_PARAM_MUTABLE_PLAYING |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11Convert:dest-alpha-mode:
   *
   * Alpha mode to be applied to output stream. In case of "premultiplied",
   * straight to premultiplied alpha conversion will be performed
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_CONVERT_DEST_ALPHA_MODE,
      g_param_spec_enum ("dest-alpha-mode", "Dest Alpha Mode",
          "Output alpha mode to be applied",
          GST_TYPE_D3D11_ALPHA_MODE, DEFAULT_ALPHA_MODE,
          (GParamFlags) (GST_PARAM_MUTABLE_PLAYING |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11Convert:fov:
   *
   * Field of view angle in degrees
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_CONVERT_FOV,
      g_param_spec_float ("fov", "Fov",
          "Field of view angle in degrees",
          0, G_MAXFLOAT, DEFAULT_FOV,
          (GParamFlags) (GST_PARAM_MUTABLE_PLAYING |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11Convert:ortho:
   *
   * Use orthographic projection
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_CONVERT_ORTHO,
      g_param_spec_boolean ("ortho", "Orthographic",
          "Use orthographic projection", DEFAULT_ORTHO,
          (GParamFlags) (GST_PARAM_MUTABLE_PLAYING |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11Convert:rotation-x:
   *
   * x-axis rotation angle to be applied prior to "video-direction"
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_CONVERT_ROTATION_X,
      g_param_spec_float ("rotation-x", "Rotation X",
          "x-axis rotation angle in degrees",
          -G_MAXFLOAT, G_MAXFLOAT, DEFAULT_ROTATION,
          (GParamFlags) (GST_PARAM_MUTABLE_PLAYING |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11Convert:rotation-y:
   *
   * y-axis rotation angle to be applied prior to "video-direction"
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_CONVERT_ROTATION_Y,
      g_param_spec_float ("rotation-y", "Rotation Y",
          "y-axis rotation angle in degrees",
          -G_MAXFLOAT, G_MAXFLOAT, DEFAULT_ROTATION,
          (GParamFlags) (GST_PARAM_MUTABLE_PLAYING |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11Convert:rotation-z:
   *
   * z-axis rotation angle to be applied prior to "video-direction"
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_CONVERT_ROTATION_Z,
      g_param_spec_float ("rotation-z", "Rotation Z",
          "z-axis rotation angle in degrees",
          -G_MAXFLOAT, G_MAXFLOAT, DEFAULT_ROTATION,
          (GParamFlags) (GST_PARAM_MUTABLE_PLAYING |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11Convert:scale-x:
   *
   * Scale multiplier for x-axis
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_CONVERT_SCALE_X,
      g_param_spec_float ("scale-x", "Scale X",
          "Scale multiplier for x-axis",
          -G_MAXFLOAT, G_MAXFLOAT, DEFAULT_SCALE,
          (GParamFlags) (GST_PARAM_MUTABLE_PLAYING |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11Convert:scale-y:
   *
   * Scale multiplier for y-axis
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_CONVERT_SCALE_Y,
      g_param_spec_float ("scale-y", "Scale Y",
          "Scale multiplier for y-axis",
          -G_MAXFLOAT, G_MAXFLOAT, DEFAULT_SCALE,
          (GParamFlags) (GST_PARAM_MUTABLE_PLAYING |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11Convert:msaa:
   *
   * MSAA (Multi-Sampling Anti-Aliasing) level
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_CONVERT_MSAA,
      g_param_spec_enum ("msaa", "MSAA",
          "MSAA (Multi-Sampling Anti-Aliasing) level. "
          "This value will be ignored if device does not support the specified "
          "level for negotiated output format",
          GST_TYPE_D3D11_MSAA_MODE, DEFAULT_MSAA,
          (GParamFlags) (GST_PARAM_MUTABLE_PLAYING |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gst_element_class_set_static_metadata (element_class,
      "Direct3D11 Converter",
      "Filter/Converter/Scaler/Effect/Video/Hardware",
      "Performs resizing, colorspace conversion, cropping, and flipping/rotating using Direct3D11",
      "Seungha Yang <seungha.yang@navercorp.com>, "
      "Jeongki Kim <jeongki.kim@jeongki.kim>");

  trans_class->sink_event = GST_DEBUG_FUNCPTR (gst_d3d11_convert_sink_event);
}

static void
gst_d3d11_convert_init (GstD3D11Convert * self)
{
}

static void
gst_d3d11_convert_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstD3D11BaseConvert *base = GST_D3D11_BASE_CONVERT (object);

  switch (prop_id) {
    case PROP_CONVERT_ADD_BORDERS:
      gst_d3d11_base_convert_set_add_border (base, g_value_get_boolean (value));
      break;
    case PROP_CONVERT_BORDER_COLOR:
      gst_d3d11_base_convert_set_border_color (base,
          g_value_get_uint64 (value));
      break;
    case PROP_CONVERT_VIDEO_DIRECTION:
      gst_d3d11_base_convert_set_orientation (base,
          (GstVideoOrientationMethod) g_value_get_enum (value), FALSE);
      break;
    case PROP_CONVERT_GAMMA_MODE:
      gst_d3d11_base_convert_set_gamma_mode (base,
          (GstVideoGammaMode) g_value_get_enum (value));
      break;
    case PROP_CONVERT_PRIMARIES_MODE:
      gst_d3d11_base_convert_set_primaries_mode (base,
          (GstVideoPrimariesMode) g_value_get_enum (value));
      break;
    case PROP_CONVERT_SRC_ALPHA_MODE:
      gst_d3d11_base_convert_set_src_alpha_mode (base,
          (GstD3D11AlphaMode) g_value_get_enum (value));
      break;
    case PROP_CONVERT_DEST_ALPHA_MODE:
      gst_d3d11_base_convert_set_dst_alpha_mode (base,
          (GstD3D11AlphaMode) g_value_get_enum (value));
      break;
    case PROP_CONVERT_FOV:
      gst_d3d11_base_convert_set_fov (base, g_value_get_float (value));
      break;
    case PROP_CONVERT_ORTHO:
      gst_d3d11_base_convert_set_ortho (base, g_value_get_boolean (value));
      break;
    case PROP_CONVERT_ROTATION_X:
      gst_d3d11_base_convert_set_rotation_x (base, g_value_get_float (value));
      break;
    case PROP_CONVERT_ROTATION_Y:
      gst_d3d11_base_convert_set_rotation_y (base, g_value_get_float (value));
      break;
    case PROP_CONVERT_ROTATION_Z:
      gst_d3d11_base_convert_set_rotation_z (base, g_value_get_float (value));
      break;
    case PROP_CONVERT_SCALE_X:
      gst_d3d11_base_convert_set_scale_x (base, g_value_get_float (value));
      break;
    case PROP_CONVERT_SCALE_Y:
      gst_d3d11_base_convert_set_scale_y (base, g_value_get_float (value));
      break;
    case PROP_CONVERT_MSAA:
      gst_d3d11_base_convert_set_msaa (base,
          (GstD3D11MSAAMode) g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_d3d11_convert_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstD3D11BaseConvert *base = GST_D3D11_BASE_CONVERT (object);

  switch (prop_id) {
    case PROP_CONVERT_ADD_BORDERS:
      g_value_set_boolean (value, base->add_borders);
      break;
    case PROP_CONVERT_BORDER_COLOR:
      g_value_set_uint64 (value, base->border_color);
      break;
    case PROP_CONVERT_VIDEO_DIRECTION:
      g_value_set_enum (value, base->method);
      break;
    case PROP_CONVERT_GAMMA_MODE:
      g_value_set_enum (value, base->gamma_mode);
      break;
    case PROP_CONVERT_PRIMARIES_MODE:
      g_value_set_enum (value, base->primaries_mode);
      break;
    case PROP_CONVERT_SRC_ALPHA_MODE:
      g_value_set_enum (value, base->src_alpha_mode);
      break;
    case PROP_CONVERT_DEST_ALPHA_MODE:
      g_value_set_enum (value, base->dst_alpha_mode);
      break;
    case PROP_CONVERT_FOV:
      g_value_set_float (value, base->fov);
      break;
    case PROP_CONVERT_ORTHO:
      g_value_set_boolean (value, base->ortho);
      break;
    case PROP_CONVERT_ROTATION_X:
      g_value_set_float (value, base->rotation_x);
      break;
    case PROP_CONVERT_ROTATION_Y:
      g_value_set_float (value, base->rotation_y);
      break;
    case PROP_CONVERT_ROTATION_Z:
      g_value_set_float (value, base->rotation_z);
      break;
    case PROP_CONVERT_SCALE_X:
      g_value_set_float (value, base->scale_x);
      break;
    case PROP_CONVERT_SCALE_Y:
      g_value_set_float (value, base->scale_y);
      break;
    case PROP_CONVERT_MSAA:
      g_value_set_enum (value, base->msaa);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_d3d11_convert_sink_event (GstBaseTransform * trans, GstEvent * event)
{
  GstD3D11BaseConvert *base = GST_D3D11_BASE_CONVERT (trans);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_TAG:{
      GstTagList *taglist;
      GstVideoOrientationMethod method = GST_VIDEO_ORIENTATION_IDENTITY;

      gst_event_parse_tag (event, &taglist);
      if (gst_video_orientation_from_tag (taglist, &method))
        gst_d3d11_base_convert_set_orientation (base, method, TRUE);
      break;
    }
    default:
      break;
  }

  return GST_BASE_TRANSFORM_CLASS (gst_d3d11_convert_parent_class)->sink_event
      (trans, event);
}

/**
 * SECTION:element-d3d11colorconvert
 * @title: d3d11colorconvert
 *
 * A Direct3D11 based color conversion element
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 videotestsrc ! video/x-raw,format=NV12 ! d3d11upload ! d3d11colorconvert ! d3d11download ! video/x-raw,format=RGBA ! fakesink
 * ```
 *  This will upload a test video (generated in NV12 format) to Direct3D11
 * memory space and convert it to RGBA format. Then a converted Direct3D11
 * frame will be downloaded to system memory space.
 *
 * Since: 1.20
 *
 */

enum
{
  PROP_COLOR_CONVERT_0,
  PROP_COLOR_CONVERT_GAMMA_MODE,
  PROP_COLOR_CONVERT_PRIMARIES_MODE,
  PROP_COLOR_CONVERT_SRC_ALPHA_MODE,
  PROP_COLOR_CONVERT_DEST_ALPHA_MODE,
};

struct _GstD3D11ColorConvert
{
  GstD3D11BaseConvert parent;
};

static void gst_d3d11_color_convert_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_d3d11_color_convert_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static GstCaps *gst_d3d11_color_convert_transform_caps (GstBaseTransform *
    trans, GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static GstCaps *gst_d3d11_color_convert_fixate_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);

G_DEFINE_TYPE (GstD3D11ColorConvert, gst_d3d11_color_convert,
    GST_TYPE_D3D11_BASE_CONVERT);

static void
gst_d3d11_color_convert_class_init (GstD3D11ColorConvertClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (klass);

  gobject_class->set_property = gst_d3d11_color_convert_set_property;
  gobject_class->get_property = gst_d3d11_color_convert_get_property;

  /**
   * GstD3D11ColorConvert:gamma-mode:
   *
   * Gamma conversion mode
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_COLOR_CONVERT_GAMMA_MODE,
      g_param_spec_enum ("gamma-mode", "Gamma mode",
          "Gamma conversion mode", GST_TYPE_VIDEO_GAMMA_MODE,
          DEFAULT_GAMMA_MODE, (GParamFlags) (GST_PARAM_MUTABLE_PLAYING |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11ColorConvert:primaries-mode:
   *
   * Primaries conversion mode
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class,
      PROP_COLOR_CONVERT_PRIMARIES_MODE,
      g_param_spec_enum ("primaries-mode", "Primaries Mode",
          "Primaries conversion mode", GST_TYPE_VIDEO_PRIMARIES_MODE,
          DEFAULT_PRIMARIES_MODE, (GParamFlags) (GST_PARAM_MUTABLE_PLAYING |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11ColorConvert:src-alpha-mode:
   *
   * Input stream's applied alpha mode. In case of "premultiplied",
   * premultiplied to straight alpha conversion will be performed
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class,
      PROP_COLOR_CONVERT_SRC_ALPHA_MODE,
      g_param_spec_enum ("src-alpha-mode", "Src Alpha Mode",
          "Applied input alpha mode",
          GST_TYPE_D3D11_ALPHA_MODE, DEFAULT_ALPHA_MODE,
          (GParamFlags) (GST_PARAM_MUTABLE_PLAYING |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11ColorConvert:dest-alpha-mode:
   *
   * Alpha mode to be applied to output stream. In case of "premultiplied",
   * straight to premultiplied alpha conversion will be performed
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class,
      PROP_COLOR_CONVERT_DEST_ALPHA_MODE,
      g_param_spec_enum ("dest-alpha-mode", "Dest Alpha Mode",
          "Output alpha mode to be applied",
          GST_TYPE_D3D11_ALPHA_MODE, DEFAULT_ALPHA_MODE,
          (GParamFlags) (GST_PARAM_MUTABLE_PLAYING |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gst_element_class_set_static_metadata (element_class,
      "Direct3D11 Colorspace Converter",
      "Filter/Converter/Video/Hardware",
      "Color conversion using Direct3D11",
      "Seungha Yang <seungha@centricular.com>");

  trans_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_d3d11_color_convert_transform_caps);
  trans_class->fixate_caps =
      GST_DEBUG_FUNCPTR (gst_d3d11_color_convert_fixate_caps);
}

static void
gst_d3d11_color_convert_init (GstD3D11ColorConvert * self)
{
}

static void
gst_d3d11_color_convert_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstD3D11BaseConvert *base = GST_D3D11_BASE_CONVERT (object);

  switch (prop_id) {
    case PROP_COLOR_CONVERT_GAMMA_MODE:
      gst_d3d11_base_convert_set_gamma_mode (base,
          (GstVideoGammaMode) g_value_get_enum (value));
      break;
    case PROP_COLOR_CONVERT_PRIMARIES_MODE:
      gst_d3d11_base_convert_set_primaries_mode (base,
          (GstVideoPrimariesMode) g_value_get_enum (value));
      break;
    case PROP_COLOR_CONVERT_SRC_ALPHA_MODE:
      gst_d3d11_base_convert_set_src_alpha_mode (base,
          (GstD3D11AlphaMode) g_value_get_enum (value));
      break;
    case PROP_COLOR_CONVERT_DEST_ALPHA_MODE:
      gst_d3d11_base_convert_set_dst_alpha_mode (base,
          (GstD3D11AlphaMode) g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_d3d11_color_convert_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstD3D11BaseConvert *base = GST_D3D11_BASE_CONVERT (object);

  switch (prop_id) {
    case PROP_COLOR_CONVERT_GAMMA_MODE:
      g_value_set_enum (value, base->gamma_mode);
      break;
    case PROP_COLOR_CONVERT_PRIMARIES_MODE:
      g_value_set_enum (value, base->primaries_mode);
      break;
    case PROP_COLOR_CONVERT_SRC_ALPHA_MODE:
      g_value_set_enum (value, base->src_alpha_mode);
      break;
    case PROP_COLOR_CONVERT_DEST_ALPHA_MODE:
      g_value_set_enum (value, base->dst_alpha_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstCaps *
gst_d3d11_color_convert_transform_caps (GstBaseTransform *
    trans, GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *tmp, *tmp2;
  GstCaps *result;

  /* Get all possible caps that we can transform to */
  tmp = gst_d3d11_base_convert_caps_remove_format_info (caps);

  if (filter) {
    tmp2 = gst_caps_intersect_full (filter, tmp, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp);
    tmp = tmp2;
  }

  result = tmp;

  GST_DEBUG_OBJECT (trans, "transformed %" GST_PTR_FORMAT " into %"
      GST_PTR_FORMAT, caps, result);

  return result;
}

static GstCaps *
gst_d3d11_color_convert_fixate_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstCaps *format = NULL;

  GST_DEBUG_OBJECT (base,
      "trying to fixate othercaps %" GST_PTR_FORMAT " based on caps %"
      GST_PTR_FORMAT, othercaps, caps);

  format = gst_d3d11_base_convert_get_fixed_format (base, direction, caps,
      othercaps);
  gst_caps_unref (othercaps);

  if (gst_caps_is_empty (format)) {
    GST_ERROR_OBJECT (base, "Could not convert formats");
  } else {
    GST_DEBUG_OBJECT (base, "fixated othercaps to %" GST_PTR_FORMAT, format);
  }

  return format;
}

/**
 * SECTION:element-d3d11scale
 * @title: d3d11scale
 *
 * A Direct3D11 based video resizing element
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 videotestsrc ! video/x-raw,width=640,height=480 ! d3d11upload ! d3d11scale ! d3d11download ! video/x-raw,width=1280,height=720 ! fakesink
 * ```
 *  This will upload a 640x480 resolution test video to Direct3D11
 * memory space and resize it to 1280x720 resolution. Then a resized Direct3D11
 * frame will be downloaded to system memory space.
 *
 * Since: 1.20
 *
 */

enum
{
  PROP_SCALE_0,
  PROP_SCALE_ADD_BORDERS,
  PROP_SCALE_BORDER_COLOR,
};

struct _GstD3D11Scale
{
  GstD3D11BaseConvert parent;
};

static void gst_d3d11_scale_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_d3d11_scale_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstCaps *gst_d3d11_scale_transform_caps (GstBaseTransform *
    trans, GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static GstCaps *gst_d3d11_scale_fixate_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);

G_DEFINE_TYPE (GstD3D11Scale, gst_d3d11_scale, GST_TYPE_D3D11_BASE_CONVERT);

static void
gst_d3d11_scale_class_init (GstD3D11ScaleClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (klass);

  gobject_class->set_property = gst_d3d11_scale_set_property;
  gobject_class->get_property = gst_d3d11_scale_get_property;

  /**
   * GstD3D11Scale:add-borders:
   *
   * Add borders if necessary to keep the display aspect ratio
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class, PROP_SCALE_ADD_BORDERS,
      g_param_spec_boolean ("add-borders", "Add Borders",
          "Add black borders if necessary to keep the display aspect ratio",
          DEFAULT_ADD_BORDERS, (GParamFlags) (GST_PARAM_MUTABLE_PLAYING |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11Scale:border-color:
   *
   * Border color to use in ARGB64 format
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_SCALE_BORDER_COLOR,
      g_param_spec_uint64 ("border-color", "Border color",
          "Border color to use in ARGB64 format", 0, G_MAXUINT64,
          DEFAULT_BORDER_COLOR, (GParamFlags) (GST_PARAM_MUTABLE_PLAYING |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gst_element_class_set_static_metadata (element_class,
      "Direct3D11 Scaler",
      "Filter/Converter/Video/Scaler/Hardware",
      "Resizes video using Direct3D11",
      "Seungha Yang <seungha@centricular.com>");

  trans_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_d3d11_scale_transform_caps);
  trans_class->fixate_caps = GST_DEBUG_FUNCPTR (gst_d3d11_scale_fixate_caps);
}

static void
gst_d3d11_scale_init (GstD3D11Scale * self)
{
}

static void
gst_d3d11_scale_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstD3D11BaseConvert *base = GST_D3D11_BASE_CONVERT (object);

  switch (prop_id) {
    case PROP_SCALE_ADD_BORDERS:
      gst_d3d11_base_convert_set_add_border (base, g_value_get_boolean (value));
      break;
    case PROP_SCALE_BORDER_COLOR:
      gst_d3d11_base_convert_set_border_color (base,
          g_value_get_uint64 (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_d3d11_scale_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstD3D11BaseConvert *base = GST_D3D11_BASE_CONVERT (object);

  switch (prop_id) {
    case PROP_SCALE_ADD_BORDERS:
      g_value_set_boolean (value, base->add_borders);
      break;
    case PROP_SCALE_BORDER_COLOR:
      g_value_set_uint64 (value, base->border_color);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstCaps *
gst_d3d11_scale_transform_caps (GstBaseTransform *
    trans, GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *tmp, *tmp2;
  GstCaps *result;

  /* Get all possible caps that we can transform to */
  tmp = gst_d3d11_base_convert_caps_rangify_size_info (caps);

  if (filter) {
    tmp2 = gst_caps_intersect_full (filter, tmp, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp);
    tmp = tmp2;
  }

  result = tmp;

  GST_DEBUG_OBJECT (trans, "transformed %" GST_PTR_FORMAT " into %"
      GST_PTR_FORMAT, caps, result);

  return result;
}

static GstCaps *
gst_d3d11_scale_fixate_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GST_DEBUG_OBJECT (base,
      "trying to fixate othercaps %" GST_PTR_FORMAT " based on caps %"
      GST_PTR_FORMAT, othercaps, caps);

  othercaps =
      gst_d3d11_base_convert_fixate_size (base, direction, caps, othercaps);

  GST_DEBUG_OBJECT (base, "fixated othercaps to %" GST_PTR_FORMAT, othercaps);

  return othercaps;
}
