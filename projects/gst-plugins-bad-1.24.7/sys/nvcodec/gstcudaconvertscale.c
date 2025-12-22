/* GStreamer
 * Copyright (C) 2020 Thibault Saunier <tsaunier@igalia.com>
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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
#include <config.h>
#endif

#include <gst/cuda/gstcudautils.h>
#include "gstcudaconvertscale.h"
#include "gstcudaconverter.h"

GST_DEBUG_CATEGORY_STATIC (gst_cuda_base_convert_debug);
#define GST_CAT_DEFAULT gst_cuda_base_convert_debug

#define GST_CUDA_CONVET_FORMATS \
    "{ I420, YV12, NV12, NV21, P010_10LE, P012_LE, P016_LE, I420_10LE, I420_12LE, Y444, " \
    "Y444_10LE, Y444_12LE, Y444_16LE, BGRA, RGBA, RGBx, BGRx, ARGB, ABGR, RGB, " \
    "BGR, BGR10A2_LE, RGB10A2_LE, Y42B, I422_10LE, I422_12LE, RGBP, BGRP, GBR, " \
    "GBRA, GBR_10LE, GBR_12LE, GBR_16LE }"

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY, GST_CUDA_CONVET_FORMATS))
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY, GST_CUDA_CONVET_FORMATS))
    );

#define DEFAULT_ADD_BORDERS TRUE

struct _GstCudaBaseConvert
{
  GstCudaBaseTransform parent;

  GstCudaConverter *converter;
  GstCudaStream *other_stream;

  gint borders_h;
  gint borders_w;
  gboolean add_borders;

  /* orientation */
  /* method configured via property */
  GstVideoOrientationMethod method;
  /* method parsed from tag */
  GstVideoOrientationMethod tag_method;
  /* method currently selected based on "method" and "tag_method" */
  GstVideoOrientationMethod selected_method;
  /* method previously selected and used for negotiation */
  GstVideoOrientationMethod active_method;

  GMutex lock;
};

static void gst_cuda_base_convert_dispose (GObject * object);
static void gst_cuda_base_convert_finalize (GObject * object);
static GstCaps *gst_cuda_base_convert_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static GstCaps *gst_cuda_base_convert_fixate_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);
static gboolean
gst_cuda_base_convert_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query);
static gboolean gst_cuda_base_convert_decide_allocation (GstBaseTransform *
    trans, GstQuery * query);
static gboolean gst_cuda_base_convert_filter_meta (GstBaseTransform * trans,
    GstQuery * query, GType api, const GstStructure * params);
static GstFlowReturn gst_cuda_base_convert_transform (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf);
static gboolean gst_cuda_base_convert_set_info (GstCudaBaseTransform * btrans,
    GstCaps * incaps, GstVideoInfo * in_info, GstCaps * outcaps,
    GstVideoInfo * out_info);

/**
 * GstCudaBaseConvert:
 *
 * A baseclass implementation for cuda convert elements
 *
 * Since: 1.22
 */
#define gst_cuda_base_convert_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstCudaBaseConvert,
    gst_cuda_base_convert, GST_TYPE_CUDA_BASE_TRANSFORM,
    GST_DEBUG_CATEGORY_INIT (gst_cuda_base_convert_debug,
        "cudaconvertscale", 0, "CUDA Base Filter"));

static void
gst_cuda_base_convert_class_init (GstCudaBaseConvertClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstCudaBaseTransformClass *btrans_class =
      GST_CUDA_BASE_TRANSFORM_CLASS (klass);

  gobject_class->dispose = gst_cuda_base_convert_dispose;
  gobject_class->finalize = gst_cuda_base_convert_finalize;

  gst_element_class_add_static_pad_template (element_class, &sink_template);
  gst_element_class_add_static_pad_template (element_class, &src_template);

  trans_class->passthrough_on_same_caps = TRUE;

  trans_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_cuda_base_convert_transform_caps);
  trans_class->fixate_caps =
      GST_DEBUG_FUNCPTR (gst_cuda_base_convert_fixate_caps);
  trans_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_cuda_base_convert_propose_allocation);
  trans_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_cuda_base_convert_decide_allocation);
  trans_class->filter_meta =
      GST_DEBUG_FUNCPTR (gst_cuda_base_convert_filter_meta);
  trans_class->transform = GST_DEBUG_FUNCPTR (gst_cuda_base_convert_transform);

  btrans_class->set_info = GST_DEBUG_FUNCPTR (gst_cuda_base_convert_set_info);

  gst_type_mark_as_plugin_api (GST_TYPE_CUDA_BASE_CONVERT, 0);
}

static void
gst_cuda_base_convert_init (GstCudaBaseConvert * self)
{
  self->add_borders = DEFAULT_ADD_BORDERS;
  g_mutex_init (&self->lock);
}

static void
gst_cuda_base_convert_dispose (GObject * object)
{
  GstCudaBaseConvert *self = GST_CUDA_BASE_CONVERT (object);

  gst_clear_cuda_stream (&self->other_stream);
  gst_clear_object (&self->converter);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_cuda_base_convert_finalize (GObject * object)
{
  GstCudaBaseConvert *self = GST_CUDA_BASE_CONVERT (object);

  g_mutex_clear (&self->lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstCaps *
gst_cuda_base_convert_caps_remove_format_info (GstCaps * caps)
{
  GstStructure *st;
  GstCapsFeatures *f;
  gint i, n;
  GstCaps *res;
  GstCapsFeatures *feature =
      gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY);

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
gst_cuda_base_convert_caps_rangify_size_info (GstCaps * caps)
{
  GstStructure *st;
  GstCapsFeatures *f;
  gint i, n;
  GstCaps *res;
  GstCapsFeatures *feature =
      gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY);

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
gst_cuda_base_convert_caps_remove_format_and_rangify_size_info (GstCaps * caps)
{
  GstStructure *st;
  GstCapsFeatures *f;
  gint i, n;
  GstCaps *res;
  GstCapsFeatures *feature =
      gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY);

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

static GstCaps *
gst_cuda_base_convert_transform_caps (GstBaseTransform *
    trans, GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *tmp, *tmp2;
  GstCaps *result;

  /* Get all possible caps that we can transform to */
  tmp = gst_cuda_base_convert_caps_remove_format_and_rangify_size_info (caps);

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
gst_cuda_base_convert_fixate_format (GstBaseTransform * trans,
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
gst_cuda_base_convert_get_fixed_format (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstCaps *result;

  result = gst_caps_intersect (othercaps, caps);
  if (gst_caps_is_empty (result)) {
    gst_caps_unref (result);
    result = gst_caps_copy (othercaps);
  }

  gst_cuda_base_convert_fixate_format (trans, caps, result);

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
gst_cuda_base_convert_fixate_size (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstCudaBaseConvert *self = GST_CUDA_BASE_CONVERT (base);
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

  g_mutex_lock (&self->lock);
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
        to_par_n = from_par_n;
        to_par_d = from_par_d;
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
  g_mutex_unlock (&self->lock);

  return othercaps;
}

static GstCaps *
gst_cuda_base_convert_fixate_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstCaps *format = NULL;

  GST_DEBUG_OBJECT (trans,
      "trying to fixate othercaps %" GST_PTR_FORMAT " based on caps %"
      GST_PTR_FORMAT, othercaps, caps);

  format = gst_cuda_base_convert_get_fixed_format (trans, direction, caps,
      othercaps);

  if (gst_caps_is_empty (format)) {
    GST_ERROR_OBJECT (trans, "Could not convert formats");
    return format;
  }

  /* convert mode is "all" or "size" here */
  othercaps =
      gst_cuda_base_convert_fixate_size (trans, direction, caps, othercaps);

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

  GST_DEBUG_OBJECT (trans, "fixated othercaps to %" GST_PTR_FORMAT, othercaps);

  return othercaps;
}

static gboolean
gst_cuda_base_convert_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
  GstCudaBaseConvert *self = GST_CUDA_BASE_CONVERT (trans);
  GstCudaBaseTransform *ctrans = GST_CUDA_BASE_TRANSFORM (trans);
  GstVideoInfo info;
  GstBufferPool *pool;
  GstCaps *caps;
  guint size;

  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->propose_allocation (trans,
          decide_query, query))
    return FALSE;

  /* passthrough, we're done */
  if (decide_query == NULL)
    return TRUE;

  gst_query_parse_allocation (query, &caps, NULL);

  if (caps == NULL)
    return FALSE;

  if (!gst_video_info_from_caps (&info, caps))
    return FALSE;

  if (gst_query_get_n_allocation_pools (query) == 0) {
    GstStructure *config;

    pool = gst_cuda_buffer_pool_new (ctrans->context);

    config = gst_buffer_pool_get_config (pool);
    /* Forward downstream CUDA stream to upstream */
    if (self->other_stream) {
      GST_DEBUG_OBJECT (self, "Have downstream CUDA stream, forwarding");
      gst_buffer_pool_config_set_cuda_stream (config, self->other_stream);
    } else if (ctrans->stream) {
      GST_DEBUG_OBJECT (self, "Set our stream to proposing buffer pool");
      gst_buffer_pool_config_set_cuda_stream (config, ctrans->stream);
    }

    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);

    size = GST_VIDEO_INFO_SIZE (&info);
    gst_buffer_pool_config_set_params (config, caps, size, 0, 0);

    if (!gst_buffer_pool_set_config (pool, config)) {
      GST_ERROR_OBJECT (ctrans, "failed to set config");
      gst_object_unref (pool);
      return FALSE;
    }

    /* Get updated size by cuda buffer pool */
    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_get_params (config, NULL, &size, NULL, NULL);
    gst_structure_free (config);

    gst_query_add_allocation_pool (query, pool, size, 0, 0);

    gst_object_unref (pool);
  }

  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  return TRUE;
}

static gboolean
gst_cuda_base_convert_decide_allocation (GstBaseTransform * trans,
    GstQuery * query)
{
  GstCudaBaseConvert *self = GST_CUDA_BASE_CONVERT (trans);
  GstCudaBaseTransform *ctrans = GST_CUDA_BASE_TRANSFORM (trans);
  GstCaps *outcaps = NULL;
  GstBufferPool *pool = NULL;
  guint size, min, max;
  GstStructure *config;
  gboolean update_pool = FALSE;

  gst_query_parse_allocation (query, &outcaps, NULL);

  if (!outcaps)
    return FALSE;

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);
    if (pool) {
      if (!GST_IS_CUDA_BUFFER_POOL (pool)) {
        gst_clear_object (&pool);
      } else {
        GstCudaBufferPool *cpool = GST_CUDA_BUFFER_POOL (pool);

        if (cpool->context != ctrans->context) {
          gst_clear_object (&pool);
        }
      }
    }

    update_pool = TRUE;
  } else {
    GstVideoInfo vinfo;
    gst_video_info_from_caps (&vinfo, outcaps);
    size = GST_VIDEO_INFO_SIZE (&vinfo);
    min = max = 0;
  }

  if (!pool) {
    GST_DEBUG_OBJECT (ctrans, "create our pool");

    pool = gst_cuda_buffer_pool_new (ctrans->context);
  }

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  gst_buffer_pool_config_set_params (config, outcaps, size, min, max);
  gst_clear_cuda_stream (&self->other_stream);
  self->other_stream = gst_buffer_pool_config_get_cuda_stream (config);
  if (self->other_stream) {
    GST_DEBUG_OBJECT (self, "Downstream provided CUDA stream");
  } else if (ctrans->stream) {
    GST_DEBUG_OBJECT (self, "Set our stream to decided buffer pool");
    gst_buffer_pool_config_set_cuda_stream (config, ctrans->stream);
  }

  gst_buffer_pool_set_config (pool, config);

  /* Get updated size by cuda buffer pool */
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_get_params (config, NULL, &size, NULL, NULL);
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
needs_color_convert (const GstVideoInfo * in_info,
    const GstVideoInfo * out_info)
{
  const GstVideoColorimetry *in_cinfo = &in_info->colorimetry;
  const GstVideoColorimetry *out_cinfo = &out_info->colorimetry;

  if (in_cinfo->range != out_cinfo->range ||
      in_cinfo->matrix != out_cinfo->matrix) {
    return TRUE;
  }

  if (!gst_video_color_primaries_is_equivalent (in_cinfo->primaries,
          out_cinfo->primaries)) {
    return TRUE;
  }

  if (!gst_video_transfer_function_is_equivalent (in_cinfo->transfer,
          GST_VIDEO_INFO_COMP_DEPTH (in_info, 0), out_cinfo->transfer,
          GST_VIDEO_INFO_COMP_DEPTH (out_info, 0))) {
    return TRUE;
  }

  return FALSE;
}

static gboolean
gst_cuda_base_convert_set_info (GstCudaBaseTransform * btrans,
    GstCaps * incaps, GstVideoInfo * in_info, GstCaps * outcaps,
    GstVideoInfo * out_info)
{
  GstCudaBaseConvert *self = GST_CUDA_BASE_CONVERT (btrans);
  gint from_dar_n, from_dar_d, to_dar_n, to_dar_d;
  gboolean need_flip = FALSE;
  gint in_width, in_height, in_par_n, in_par_d;
  GstVideoOrientationMethod active_method;

  gst_clear_object (&self->converter);

  g_mutex_lock (&self->lock);
  active_method = self->active_method = self->selected_method;
  g_mutex_unlock (&self->lock);

  if (active_method != GST_VIDEO_ORIENTATION_IDENTITY)
    need_flip = TRUE;

  switch (active_method) {
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
    if (self->add_borders) {
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
      GST_WARNING_OBJECT (self, "Can't keep DAR!");
    }
  }

  /* if present, these must match */
  if (in_info->interlace_mode != out_info->interlace_mode) {
    GST_ERROR_OBJECT (self, "input and output formats do not match");
    return FALSE;
  }

  if (in_width == out_info->width && in_height == out_info->height
      && in_info->finfo == out_info->finfo && self->borders_w == 0 &&
      self->borders_h == 0 && !need_flip &&
      !needs_color_convert (in_info, out_info)) {
    gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (self), TRUE);
  } else {
    GstStructure *config;

    gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (self), FALSE);

    config = gst_structure_new_empty ("GstCudaConverter");
    gst_structure_set (config,
        GST_CUDA_CONVERTER_OPT_DEST_X, G_TYPE_INT, self->borders_w / 2,
        GST_CUDA_CONVERTER_OPT_DEST_Y, G_TYPE_INT, self->borders_h / 2,
        GST_CUDA_CONVERTER_OPT_DEST_WIDTH,
        G_TYPE_INT, out_info->width - self->borders_w,
        GST_CUDA_CONVERTER_OPT_DEST_HEIGHT,
        G_TYPE_INT, out_info->height - self->borders_h,
        GST_CUDA_CONVERTER_OPT_ORIENTATION_METHOD,
        GST_TYPE_VIDEO_ORIENTATION_METHOD, active_method, NULL);

    self->converter = gst_cuda_converter_new (in_info,
        out_info, btrans->context, config);
    if (!self->converter) {
      GST_ERROR_OBJECT (self, "Couldn't create converter");
      return FALSE;
    }
  }

  GST_DEBUG_OBJECT (self, "%s from=%dx%d (par=%d/%d dar=%d/%d), size %"
      G_GSIZE_FORMAT " -> %s to=%dx%d (par=%d/%d dar=%d/%d borders=%d:%d), "
      "size %" G_GSIZE_FORMAT,
      gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (in_info)),
      in_info->width, in_info->height, in_info->par_n, in_info->par_d,
      from_dar_n, from_dar_d, in_info->size,
      gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (out_info)),
      out_info->width,
      out_info->height, out_info->par_n, out_info->par_d, to_dar_n, to_dar_d,
      self->borders_w, self->borders_h, out_info->size);

  return TRUE;
}

static gboolean
gst_cuda_base_convert_filter_meta (GstBaseTransform * trans, GstQuery * query,
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

static GstFlowReturn
gst_cuda_base_convert_transform (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf)
{
  GstCudaBaseConvert *self = GST_CUDA_BASE_CONVERT (trans);
  GstCudaBaseTransform *btrans = GST_CUDA_BASE_TRANSFORM (trans);
  GstVideoFrame in_frame, out_frame;
  GstFlowReturn ret = GST_FLOW_OK;
  GstMemory *mem;
  GstCudaMemory *in_cmem, *out_cmem;
  GstCudaStream *in_stream, *out_stream;
  GstCudaStream *selected_stream = NULL;
  gboolean sync_done = FALSE;

  if (gst_buffer_n_memory (inbuf) != 1) {
    GST_ERROR_OBJECT (self, "Invalid input buffer");
    return GST_FLOW_ERROR;
  }

  mem = gst_buffer_peek_memory (inbuf, 0);
  if (!gst_is_cuda_memory (mem)) {
    GST_ERROR_OBJECT (self, "Input buffer is not CUDA");
    return GST_FLOW_ERROR;
  }
  in_cmem = GST_CUDA_MEMORY_CAST (mem);
  in_stream = gst_cuda_memory_get_stream (in_cmem);

  if (gst_buffer_n_memory (outbuf) != 1) {
    GST_ERROR_OBJECT (self, "Invalid output buffer");
    return GST_FLOW_ERROR;
  }

  mem = gst_buffer_peek_memory (outbuf, 0);
  if (!gst_is_cuda_memory (mem)) {
    GST_ERROR_OBJECT (self, "Input buffer is not CUDA");
    return GST_FLOW_ERROR;
  }
  out_cmem = GST_CUDA_MEMORY_CAST (mem);
  out_stream = gst_cuda_memory_get_stream (out_cmem);

  if (!gst_video_frame_map (&in_frame, &btrans->in_info, inbuf,
          GST_MAP_READ | GST_MAP_CUDA)) {
    GST_ERROR_OBJECT (self, "Failed to map input buffer");
    return GST_FLOW_ERROR;
  }

  if (!gst_video_frame_map (&out_frame, &btrans->out_info, outbuf,
          GST_MAP_WRITE | GST_MAP_CUDA)) {
    gst_video_frame_unmap (&in_frame);
    GST_ERROR_OBJECT (self, "Failed to map output buffer");
    return GST_FLOW_ERROR;
  }

  /* If downstream does not aware of CUDA stream (i.e., using default stream) */
  if (!out_stream) {
    if (in_stream) {
      GST_TRACE_OBJECT (self, "Use upstram CUDA stream");
      selected_stream = in_stream;
    } else if (btrans->stream) {
      GST_TRACE_OBJECT (self, "Use our CUDA stream");
      selected_stream = btrans->stream;
    }
  } else {
    selected_stream = out_stream;
    if (in_stream) {
      if (in_stream == out_stream) {
        GST_TRACE_OBJECT (self, "Same stream");
      } else {
        GST_TRACE_OBJECT (self, "Different CUDA stream");
        gst_cuda_memory_sync (in_cmem);
      }
    }
  }

  if (!gst_cuda_converter_convert_frame (self->converter, &in_frame, &out_frame,
          gst_cuda_stream_get_handle (selected_stream), &sync_done)) {
    GST_ERROR_OBJECT (self, "Failed to convert frame");
    ret = GST_FLOW_ERROR;
  }

  if (sync_done) {
    GST_TRACE_OBJECT (self, "Sync done by converter");
    GST_MEMORY_FLAG_UNSET (out_cmem, GST_CUDA_MEMORY_TRANSFER_NEED_SYNC);
  } else if (selected_stream != out_stream) {
    GST_MEMORY_FLAG_UNSET (out_cmem, GST_CUDA_MEMORY_TRANSFER_NEED_SYNC);
    GST_TRACE_OBJECT (self, "Waiting for convert sync");
    gst_cuda_context_push (btrans->context);
    CuStreamSynchronize (gst_cuda_stream_get_handle (selected_stream));
    gst_cuda_context_pop (NULL);
  }

  gst_video_frame_unmap (&out_frame);
  gst_video_frame_unmap (&in_frame);

  return ret;
}

static void
gst_cuda_base_convert_set_add_border (GstCudaBaseConvert * self,
    gboolean add_border)
{
  gboolean prev = self->add_borders;

  self->add_borders = add_border;
  if (prev != self->add_borders)
    gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM_CAST (self));
}

static void
gst_cuda_base_convert_set_orientation (GstCudaBaseConvert * self,
    GstVideoOrientationMethod method, gboolean from_tag)
{
  if (method == GST_VIDEO_ORIENTATION_CUSTOM) {
    GST_WARNING_OBJECT (self, "Unsupported custom orientation");
    return;
  }

  g_mutex_lock (&self->lock);
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

  g_mutex_unlock (&self->lock);
}

/**
 * SECTION:element-cudaconvertscale
 * @title: cudaconvertscale
 * @short_description: A CUDA based color conversion and video resizing element
 *
 * This element resizes video frames and change color space.
 * By default the element will try to negotiate to the same size on the source
 * and sinkpad so that no scaling is needed.
 * It is therefore safe to insert this element in a pipeline to
 * get more robust behaviour without any cost if no scaling is needed.
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 videotestsrc ! cudaupload ! cudaconvertscale ! cudadownload ! autovideosink
 * ```
 *
 * Since: 1.22
 */

enum
{
  PROP_CONVERT_SCALE_0,
  PROP_CONVERT_SCALE_ADD_BORDERS,
  PROP_CONVERT_SCALE_VIDEO_DIRECTION,
};

struct _GstCudaConvertScale
{
  GstCudaBaseConvert parent;
};

static void
    gst_cuda_convert_scale_video_direction_interface_init
    (GstVideoDirectionInterface * iface)
{
}

static void gst_cuda_convert_scale_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_cuda_convert_scale_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static gboolean gst_cuda_convert_scale_sink_event (GstBaseTransform * trans,
    GstEvent * event);

#define gst_cuda_convert_scale_parent_class convert_scale_parent_class
G_DEFINE_TYPE_WITH_CODE (GstCudaConvertScale, gst_cuda_convert_scale,
    GST_TYPE_CUDA_BASE_CONVERT,
    G_IMPLEMENT_INTERFACE (GST_TYPE_VIDEO_DIRECTION,
        gst_cuda_convert_scale_video_direction_interface_init));

static void gst_cuda_convert_scale_before_transform (GstBaseTransform * trans,
    GstBuffer * buffer);

static void
gst_cuda_convert_scale_class_init (GstCudaConvertScaleClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *transform_class = GST_BASE_TRANSFORM_CLASS (klass);

  gobject_class->set_property = gst_cuda_convert_scale_set_property;
  gobject_class->get_property = gst_cuda_convert_scale_get_property;

  g_object_class_install_property (gobject_class,
      PROP_CONVERT_SCALE_ADD_BORDERS,
      g_param_spec_boolean ("add-borders", "Add Borders",
          "Add borders if necessary to keep the display aspect ratio",
          DEFAULT_ADD_BORDERS, (GParamFlags) (GST_PARAM_MUTABLE_PLAYING |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstCudaConvertScale:video-direction:
   *
   * Video rotation/flip method to use
   *
   * Since: 1.24
   */
  g_object_class_override_property (gobject_class,
      PROP_CONVERT_SCALE_VIDEO_DIRECTION, "video-direction");

  gst_element_class_set_static_metadata (element_class,
      "CUDA colorspace converter and scaler",
      "Filter/Converter/Video/Scaler/Colorspace/Effect/Hardware",
      "Resizes video and allow color conversion using CUDA",
      "Seungha Yang <seungha@centricular.com>");

  transform_class->passthrough_on_same_caps = FALSE;
  transform_class->before_transform =
      GST_DEBUG_FUNCPTR (gst_cuda_convert_scale_before_transform);
  transform_class->sink_event =
      GST_DEBUG_FUNCPTR (gst_cuda_convert_scale_sink_event);
}

static void
gst_cuda_convert_scale_init (GstCudaConvertScale * self)
{
}

static void
gst_cuda_convert_scale_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCudaBaseConvert *base = GST_CUDA_BASE_CONVERT (object);

  switch (prop_id) {
    case PROP_CONVERT_SCALE_ADD_BORDERS:
      gst_cuda_base_convert_set_add_border (base, g_value_get_boolean (value));
      break;
    case PROP_CONVERT_SCALE_VIDEO_DIRECTION:
      gst_cuda_base_convert_set_orientation (base, g_value_get_enum (value),
          FALSE);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_cuda_convert_scale_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstCudaBaseConvert *base = GST_CUDA_BASE_CONVERT (object);

  switch (prop_id) {
    case PROP_CONVERT_SCALE_ADD_BORDERS:
      g_value_set_boolean (value, base->add_borders);
      break;
    case PROP_CONVERT_SCALE_VIDEO_DIRECTION:
      g_value_set_enum (value, base->method);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_cuda_convert_scale_before_transform (GstBaseTransform * trans,
    GstBuffer * buffer)
{
  GstCudaBaseConvert *base = GST_CUDA_BASE_CONVERT (trans);
  gboolean update = FALSE;
  GstCaps *in_caps;
  GstCaps *out_caps;
  GstBaseTransformClass *klass;

  GST_BASE_TRANSFORM_CLASS (convert_scale_parent_class)->before_transform
      (trans, buffer);

  g_mutex_lock (&base->lock);
  if (base->selected_method != base->active_method)
    update = TRUE;
  g_mutex_unlock (&base->lock);

  if (!update)
    return;

  /* basetransform wouldn't call set_caps if in/out caps were not changed.
   * Update it manually here */
  GST_DEBUG_OBJECT (base, "Updating caps for direction change");

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

static gboolean
gst_cuda_convert_scale_sink_event (GstBaseTransform * trans, GstEvent * event)
{
  GstCudaBaseConvert *base = GST_CUDA_BASE_CONVERT (trans);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_TAG:{
      GstTagList *taglist;
      GstVideoOrientationMethod method = GST_VIDEO_ORIENTATION_IDENTITY;

      gst_event_parse_tag (event, &taglist);
      if (gst_video_orientation_from_tag (taglist, &method))
        gst_cuda_base_convert_set_orientation (base, method, TRUE);
      break;
    }
    default:
      break;
  }

  return
      GST_BASE_TRANSFORM_CLASS (convert_scale_parent_class)->sink_event
      (trans, event);
}

/**
 * SECTION:element-cudaconvert
 * @title: cudaconvert
 *
 * Convert video frames between supported video formats.
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 videotestsrc ! cudaupload ! cudaconvert ! cudadownload ! autovideosink
 * ```
 *
 * Since: 1.20
 */

struct _GstCudaConvert
{
  GstCudaBaseConvert parent;
};

static GstCaps *gst_cuda_convert_transform_caps (GstBaseTransform *
    trans, GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static GstCaps *gst_cuda_convert_fixate_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);

G_DEFINE_TYPE (GstCudaConvert, gst_cuda_convert, GST_TYPE_CUDA_BASE_CONVERT);

static void
gst_cuda_convert_class_init (GstCudaConvertClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (klass);

  gst_element_class_set_static_metadata (element_class,
      "CUDA colorspace converter",
      "Filter/Converter/Video/Hardware",
      "Converts video from one colorspace to another using CUDA",
      "Seungha Yang <seungha.yang@navercorp.com>");

  trans_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_cuda_convert_transform_caps);
  trans_class->fixate_caps = GST_DEBUG_FUNCPTR (gst_cuda_convert_fixate_caps);
}

static void
gst_cuda_convert_init (GstCudaConvert * self)
{
}

static GstCaps *
gst_cuda_convert_transform_caps (GstBaseTransform *
    trans, GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *tmp, *tmp2;
  GstCaps *result;

  /* Get all possible caps that we can transform to */
  tmp = gst_cuda_base_convert_caps_remove_format_info (caps);

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
gst_cuda_convert_fixate_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstCaps *format = NULL;

  GST_DEBUG_OBJECT (base,
      "trying to fixate othercaps %" GST_PTR_FORMAT " based on caps %"
      GST_PTR_FORMAT, othercaps, caps);

  format = gst_cuda_base_convert_get_fixed_format (base, direction, caps,
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
 * SECTION:element-cudascale
 * @title: cudascale
 *
 * A CUDA based video resizing element
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 videotestsrc ! video/x-raw,width=640,height=480 ! cudaupload ! cudascale ! cudadownload ! video/x-raw,width=1280,height=720 ! fakesink
 * ```
 *  This will upload a 640x480 resolution test video to CUDA
 * memory space and resize it to 1280x720 resolution. Then a resized CUDA
 * frame will be downloaded to system memory space.
 *
 * Since: 1.20
 */

enum
{
  PROP_SCALE_0,
  PROP_SCALE_ADD_BORDERS,
};

struct _GstCudaScale
{
  GstCudaBaseConvert parent;
};

static void gst_cuda_scale_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_cuda_scale_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstCaps *gst_cuda_scale_transform_caps (GstBaseTransform *
    trans, GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static GstCaps *gst_cuda_scale_fixate_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);

G_DEFINE_TYPE (GstCudaScale, gst_cuda_scale, GST_TYPE_CUDA_BASE_CONVERT);

static void
gst_cuda_scale_class_init (GstCudaScaleClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (klass);

  gobject_class->set_property = gst_cuda_scale_set_property;
  gobject_class->get_property = gst_cuda_scale_get_property;

  /**
   * GstCudaScale:add-borders:
   *
   * Add borders if necessary to keep the display aspect ratio
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_SCALE_ADD_BORDERS,
      g_param_spec_boolean ("add-borders", "Add Borders",
          "Add borders if necessary to keep the display aspect ratio",
          DEFAULT_ADD_BORDERS, (GParamFlags) (GST_PARAM_MUTABLE_PLAYING |
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gst_element_class_set_static_metadata (element_class,
      "CUDA video scaler",
      "Filter/Converter/Video/Scaler/Hardware",
      "Resize video using CUDA", "Seungha Yang <seungha.yang@navercorp.com>");

  trans_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_cuda_scale_transform_caps);
  trans_class->fixate_caps = GST_DEBUG_FUNCPTR (gst_cuda_scale_fixate_caps);
}

static void
gst_cuda_scale_init (GstCudaScale * self)
{
}

static void
gst_cuda_scale_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCudaBaseConvert *base = GST_CUDA_BASE_CONVERT (object);

  switch (prop_id) {
    case PROP_SCALE_ADD_BORDERS:
      gst_cuda_base_convert_set_add_border (base, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_cuda_scale_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstCudaBaseConvert *base = GST_CUDA_BASE_CONVERT (object);

  switch (prop_id) {
    case PROP_SCALE_ADD_BORDERS:
      g_value_set_boolean (value, base->add_borders);
      break;
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstCaps *
gst_cuda_scale_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *tmp, *tmp2;
  GstCaps *result;

  /* Get all possible caps that we can transform to */
  tmp = gst_cuda_base_convert_caps_rangify_size_info (caps);

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
gst_cuda_scale_fixate_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GST_DEBUG_OBJECT (base,
      "trying to fixate othercaps %" GST_PTR_FORMAT " based on caps %"
      GST_PTR_FORMAT, othercaps, caps);

  othercaps =
      gst_cuda_base_convert_fixate_size (base, direction, caps, othercaps);

  GST_DEBUG_OBJECT (base, "fixated othercaps to %" GST_PTR_FORMAT, othercaps);

  return othercaps;
}
