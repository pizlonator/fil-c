/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Library       <2002> Ronald Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2007 David A. Schleef <ds@schleef.org>
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
 * SECTION:video-info
 * @title: GstVideoInfo
 * @short_description: Structures and enumerations to describe raw images
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>
#include <stdio.h>

#include "video-info.h"
#include "video-tile.h"

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT ensure_debug_category()
static GstDebugCategory *
ensure_debug_category (void)
{
  static gsize cat_gonce = 0;

  if (g_once_init_enter (&cat_gonce)) {
    gsize cat_done;

    cat_done = (gsize) _gst_debug_category_new ("video-info", 0,
        "video-info structure");

    g_once_init_leave (&cat_gonce, cat_done);
  }

  return (GstDebugCategory *) cat_gonce;
}
#else
#define ensure_debug_category() /* NOOP */
#endif /* GST_DISABLE_GST_DEBUG */

/**
 * gst_video_info_copy:
 * @info: a #GstVideoInfo
 *
 * Copy a GstVideoInfo structure.
 *
 * Returns: (transfer full): a new #GstVideoInfo. free with gst_video_info_free.
 *
 * Since: 1.6
 */
GstVideoInfo *
gst_video_info_copy (const GstVideoInfo * info)
{
  return g_memdup2 (info, sizeof (GstVideoInfo));
}

/**
 * gst_video_info_free:
 * @info: a #GstVideoInfo
 *
 * Free a GstVideoInfo structure previously allocated with gst_video_info_new()
 * or gst_video_info_copy().
 *
 * Since: 1.6
 */
void
gst_video_info_free (GstVideoInfo * info)
{
  g_free (info);
}

G_DEFINE_BOXED_TYPE (GstVideoInfo, gst_video_info,
    (GBoxedCopyFunc) gst_video_info_copy, (GBoxedFreeFunc) gst_video_info_free);

/**
 * gst_video_info_new:
 *
 * Allocate a new #GstVideoInfo that is also initialized with
 * gst_video_info_init().
 *
 * Returns: (transfer full): a new #GstVideoInfo. free with gst_video_info_free().
 *
 * Since: 1.6
 */
GstVideoInfo *
gst_video_info_new (void)
{
  GstVideoInfo *info;

  info = g_new (GstVideoInfo, 1);
  gst_video_info_init (info);

  return info;
}

static gboolean fill_planes (GstVideoInfo * info,
    gsize plane_size[GST_VIDEO_MAX_PLANES]);

/**
 * gst_video_info_init:
 * @info: (out caller-allocates): a #GstVideoInfo
 *
 * Initialize @info with default values.
 */
void
gst_video_info_init (GstVideoInfo * info)
{
  g_return_if_fail (info != NULL);

  memset (info, 0, sizeof (GstVideoInfo));

  info->finfo = gst_video_format_get_info (GST_VIDEO_FORMAT_UNKNOWN);

  info->views = 1;
  /* arrange for sensible defaults, e.g. if turned into caps */
  info->fps_n = 0;
  info->fps_d = 1;
  info->par_n = 1;
  info->par_d = 1;
  GST_VIDEO_INFO_MULTIVIEW_MODE (info) = GST_VIDEO_MULTIVIEW_MODE_NONE;
  GST_VIDEO_INFO_MULTIVIEW_FLAGS (info) = GST_VIDEO_MULTIVIEW_FLAGS_NONE;
  GST_VIDEO_INFO_FIELD_ORDER (info) = GST_VIDEO_FIELD_ORDER_UNKNOWN;
}

#define MAKE_COLORIMETRY(r,m,t,p) {  \
  GST_VIDEO_COLOR_RANGE ##r, GST_VIDEO_COLOR_MATRIX_ ##m, \
  GST_VIDEO_TRANSFER_ ##t, GST_VIDEO_COLOR_PRIMARIES_ ##p }

#define DEFAULT_YUV_SD  0
#define DEFAULT_YUV_HD  1
#define DEFAULT_RGB     2
#define DEFAULT_GRAY    3
#define DEFAULT_UNKNOWN 4

static const GstVideoColorimetry default_color[] = {
  MAKE_COLORIMETRY (_16_235, BT601, BT601, SMPTE170M),
  MAKE_COLORIMETRY (_16_235, BT709, BT709, BT709),
  MAKE_COLORIMETRY (_0_255, RGB, SRGB, BT709),
  MAKE_COLORIMETRY (_0_255, BT601, UNKNOWN, UNKNOWN),
  MAKE_COLORIMETRY (_UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN),
};

static void
set_default_colorimetry (GstVideoInfo * info)
{
  const GstVideoFormatInfo *finfo = info->finfo;

  if (GST_VIDEO_FORMAT_INFO_IS_YUV (finfo)) {
    if (info->height > 576) {
      info->chroma_site = GST_VIDEO_CHROMA_SITE_H_COSITED;
      info->colorimetry = default_color[DEFAULT_YUV_HD];
    } else {
      info->chroma_site = GST_VIDEO_CHROMA_SITE_NONE;
      info->colorimetry = default_color[DEFAULT_YUV_SD];
    }
  } else if (GST_VIDEO_FORMAT_INFO_IS_GRAY (finfo)) {
    info->colorimetry = default_color[DEFAULT_GRAY];
  } else if (GST_VIDEO_FORMAT_INFO_IS_RGB (finfo)) {
    info->colorimetry = default_color[DEFAULT_RGB];
  } else {
    info->colorimetry = default_color[DEFAULT_UNKNOWN];
  }
}

static gboolean
validate_colorimetry (GstVideoInfo * info)
{
  const GstVideoFormatInfo *finfo = info->finfo;

  if ((GST_VIDEO_FORMAT_INFO_IS_YUV (finfo)
          || GST_VIDEO_FORMAT_INFO_IS_GRAY (finfo))
      && info->colorimetry.matrix == GST_VIDEO_COLOR_MATRIX_RGB) {
    GST_WARNING
        ("color matrix RGB is only supported with RGB format, %s is not",
        finfo->name);
    return FALSE;
  }

  if (GST_VIDEO_FORMAT_INFO_IS_YUV (finfo) &&
      info->colorimetry.matrix == GST_VIDEO_COLOR_MATRIX_UNKNOWN) {
    GST_WARNING ("Need to specify a color matrix when using YUV format (%s)",
        finfo->name);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_video_info_set_format_common (GstVideoInfo * info, GstVideoFormat format,
    guint width, guint height)
{
  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (format != GST_VIDEO_FORMAT_UNKNOWN, FALSE);

  if (width > G_MAXINT || height > G_MAXINT)
    return FALSE;

  gst_video_info_init (info);

  info->finfo = gst_video_format_get_info (format);
  info->width = width;
  info->height = height;
  info->views = 1;

  set_default_colorimetry (info);

  return TRUE;
}

/**
 * gst_video_info_set_format:
 * @info: a #GstVideoInfo
 * @format: the format
 * @width: a width
 * @height: a height
 *
 * Set the default info for a video frame of @format and @width and @height.
 *
 * Note: This initializes @info first, no values are preserved. This function
 * does not set the offsets correctly for interlaced vertically
 * subsampled formats.
 *
 * Returns: %FALSE if the returned video info is invalid, e.g. because the
 *   size of a frame can't be represented as a 32 bit integer (Since: 1.12)
 */
gboolean
gst_video_info_set_format (GstVideoInfo * info, GstVideoFormat format,
    guint width, guint height)
{
  if (!gst_video_info_set_format_common (info, format, width, height))
    return FALSE;

  return fill_planes (info, NULL);
}

/**
 * gst_video_info_set_interlaced_format:
 * @info: a #GstVideoInfo
 * @format: the format
 * @mode: a #GstVideoInterlaceMode
 * @width: a width
 * @height: a height
 *
 * Same as #gst_video_info_set_format but also allowing to set the interlaced
 * mode.
 *
 * Returns: %FALSE if the returned video info is invalid, e.g. because the
 *   size of a frame can't be represented as a 32 bit integer.
 *
 * Since: 1.16
 */
gboolean
gst_video_info_set_interlaced_format (GstVideoInfo * info,
    GstVideoFormat format, GstVideoInterlaceMode mode, guint width,
    guint height)
{
  if (!gst_video_info_set_format_common (info, format, width, height))
    return FALSE;

  GST_VIDEO_INFO_INTERLACE_MODE (info) = mode;
  return fill_planes (info, NULL);
}

static const gchar *interlace_mode[] = {
  "progressive",
  "interleaved",
  "mixed",
  "fields",
  "alternate"
};

/**
 * gst_video_interlace_mode_to_string:
 * @mode: a #GstVideoInterlaceMode
 *
 * Convert @mode to its string representation.
 *
 * Returns: @mode as a string.
 *
 * Since: 1.6
 */
const gchar *
gst_video_interlace_mode_to_string (GstVideoInterlaceMode mode)
{
  if (((guint) mode) >= G_N_ELEMENTS (interlace_mode))
    return NULL;

  return interlace_mode[mode];
}

/**
 * gst_video_interlace_mode_from_string:
 * @mode: a mode
 *
 * Convert @mode to a #GstVideoInterlaceMode
 *
 * Returns: the #GstVideoInterlaceMode of @mode or
 *    #GST_VIDEO_INTERLACE_MODE_PROGRESSIVE when @mode is not a valid
 *    string representation for a #GstVideoInterlaceMode.
 *
 * Since: 1.6
 */
GstVideoInterlaceMode
gst_video_interlace_mode_from_string (const gchar * mode)
{
  gint i;
  for (i = 0; i < G_N_ELEMENTS (interlace_mode); i++) {
    if (g_str_equal (interlace_mode[i], mode))
      return i;
  }
  return GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
}

static const gchar *field_order[] = {
  "unknown",
  "top-field-first",
  "bottom-field-first"
};

/**
 * gst_video_field_order_to_string:
 * @order: a #GstVideoFieldOrder
 *
 * Convert @order to its string representation.
 *
 * Returns: @order as a string.
 *
 * Since: 1.12
 */
const gchar *
gst_video_field_order_to_string (GstVideoFieldOrder order)
{
  if (((guint) order) >= G_N_ELEMENTS (field_order))
    return NULL;

  return field_order[order];
}

/**
 * gst_video_field_order_from_string:
 * @order: a field order
 *
 * Convert @order to a #GstVideoFieldOrder
 *
 * Returns: the #GstVideoFieldOrder of @order or
 *    #GST_VIDEO_FIELD_ORDER_UNKNOWN when @order is not a valid
 *    string representation for a #GstVideoFieldOrder.
 *
 * Since: 1.12
 */
GstVideoFieldOrder
gst_video_field_order_from_string (const gchar * order)
{
  gint i;
  for (i = 0; i < G_N_ELEMENTS (field_order); i++) {
    if (g_str_equal (field_order[i], order))
      return i;
  }
  return GST_VIDEO_FIELD_ORDER_UNKNOWN;
}

/**
 * gst_video_info_from_caps:
 * @info: (out caller-allocates): #GstVideoInfo
 * @caps: a #GstCaps
 *
 * Parse @caps and update @info.
 *
 * Returns: TRUE if @caps could be parsed
 */
gboolean
gst_video_info_from_caps (GstVideoInfo * info, const GstCaps * caps)
{
  GstStructure *structure;
  const gchar *s;
  GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;
  gint width = 0, height = 0;
  gint fps_n, fps_d;
  gint par_n, par_d;
  guint multiview_flags;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (caps != NULL, FALSE);
  g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

  GST_DEBUG ("parsing caps %" GST_PTR_FORMAT, caps);

  structure = gst_caps_get_structure (caps, 0);

  if (gst_structure_has_name (structure, "video/x-raw")) {
    if (!(s = gst_structure_get_string (structure, "format")))
      goto no_format;

    format = gst_video_format_from_string (s);
    if (format == GST_VIDEO_FORMAT_UNKNOWN)
      goto unknown_format;

  } else if (g_str_has_prefix (gst_structure_get_name (structure), "video/") ||
      g_str_has_prefix (gst_structure_get_name (structure), "image/")) {
    format = GST_VIDEO_FORMAT_ENCODED;
  } else {
    goto wrong_name;
  }

  /* width and height are mandatory, except for non-raw-formats */
  if (!gst_structure_get_int (structure, "width", &width) &&
      format != GST_VIDEO_FORMAT_ENCODED)
    goto no_width;
  if (!gst_structure_get_int (structure, "height", &height) &&
      format != GST_VIDEO_FORMAT_ENCODED)
    goto no_height;

  gst_video_info_init (info);

  info->finfo = gst_video_format_get_info (format);
  info->width = width;
  info->height = height;

  if (gst_structure_get_fraction (structure, "framerate", &fps_n, &fps_d)) {
    if (fps_n == 0) {
      /* variable framerate */
      info->flags |= GST_VIDEO_FLAG_VARIABLE_FPS;
      /* see if we have a max-framerate */
      gst_structure_get_fraction (structure, "max-framerate", &fps_n, &fps_d);
    }
    info->fps_n = fps_n;
    info->fps_d = fps_d;
  } else {
    /* unspecified is variable framerate */
    info->fps_n = 0;
    info->fps_d = 1;
  }

  if (gst_structure_get_fraction (structure, "pixel-aspect-ratio",
          &par_n, &par_d)) {
    info->par_n = par_n;
    info->par_d = par_d;
  } else {
    info->par_n = 1;
    info->par_d = 1;
  }

  if ((s = gst_structure_get_string (structure, "interlace-mode")))
    info->interlace_mode = gst_video_interlace_mode_from_string (s);
  else
    info->interlace_mode = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;

  /* Interlaced feature is mandatory for raw alternate streams */
  if (info->interlace_mode == GST_VIDEO_INTERLACE_MODE_ALTERNATE &&
      format != GST_VIDEO_FORMAT_ENCODED) {
    GstCapsFeatures *f;

    f = gst_caps_get_features (caps, 0);
    if (!f
        || !gst_caps_features_contains (f, GST_CAPS_FEATURE_FORMAT_INTERLACED))
      goto alternate_no_feature;
  }

  if (GST_VIDEO_INFO_IS_INTERLACED (info) &&
      (s = gst_structure_get_string (structure, "field-order"))) {
    GST_VIDEO_INFO_FIELD_ORDER (info) = gst_video_field_order_from_string (s);
  } else {
    GST_VIDEO_INFO_FIELD_ORDER (info) = GST_VIDEO_FIELD_ORDER_UNKNOWN;
  }

  {
    if ((s = gst_structure_get_string (structure, "multiview-mode")))
      GST_VIDEO_INFO_MULTIVIEW_MODE (info) =
          gst_video_multiview_mode_from_caps_string (s);
    else
      GST_VIDEO_INFO_MULTIVIEW_MODE (info) = GST_VIDEO_MULTIVIEW_MODE_NONE;

    if (gst_structure_get_flagset (structure, "multiview-flags",
            &multiview_flags, NULL))
      GST_VIDEO_INFO_MULTIVIEW_FLAGS (info) = multiview_flags;

    if (!gst_structure_get_int (structure, "views", &info->views))
      info->views = 1;

    /* At one point, I tried normalising the half-aspect flag here,
     * but it behaves weird for GstVideoInfo operations other than
     * directly converting to/from caps - sometimes causing the
     * PAR to be doubled/halved too many times */
  }

  if ((s = gst_structure_get_string (structure, "chroma-site")))
    info->chroma_site = gst_video_chroma_site_from_string (s);
  else
    info->chroma_site = GST_VIDEO_CHROMA_SITE_UNKNOWN;

  if ((s = gst_structure_get_string (structure, "colorimetry"))) {
    if (!gst_video_colorimetry_from_string (&info->colorimetry, s)) {
      GST_WARNING ("unparsable colorimetry, using default");
      set_default_colorimetry (info);
    } else if (!validate_colorimetry (info)) {
      GST_WARNING ("invalid colorimetry, using default");
      set_default_colorimetry (info);
    } else {
      /* force RGB matrix for RGB formats */
      if (GST_VIDEO_FORMAT_INFO_IS_RGB (info->finfo) &&
          info->colorimetry.matrix != GST_VIDEO_COLOR_MATRIX_RGB) {
        GST_WARNING ("invalid matrix %d for RGB format, using RGB",
            info->colorimetry.matrix);
        info->colorimetry.matrix = GST_VIDEO_COLOR_MATRIX_RGB;
      }
    }
  } else {
    GST_DEBUG ("no colorimetry, using default");
    set_default_colorimetry (info);
  }

  if (!fill_planes (info, NULL))
    return FALSE;

  return TRUE;

  /* ERROR */
wrong_name:
  {
    GST_ERROR ("wrong name '%s', expected video/ or image/",
        gst_structure_get_name (structure));
    return FALSE;
  }
no_format:
  {
    GST_ERROR ("no format given");
    return FALSE;
  }
unknown_format:
  {
    GST_ERROR ("unknown format '%s' given", s);
    return FALSE;
  }
no_width:
  {
    GST_ERROR ("no width property given");
    return FALSE;
  }
no_height:
  {
    GST_ERROR ("no height property given");
    return FALSE;
  }
alternate_no_feature:
  {
    GST_ERROR
        ("caps has 'interlace-mode=alternate' but doesn't have the Interlaced feature");
    return FALSE;
  }
}

/**
 * gst_video_info_new_from_caps:
 * @caps: a #GstCaps
 *
 * Parse @caps to generate a #GstVideoInfo.
 *
 * Returns: (transfer full) (nullable): A #GstVideoInfo, or %NULL if @caps couldn't be parsed
 * Since: 1.20
 */
GstVideoInfo *
gst_video_info_new_from_caps (const GstCaps * caps)
{
  GstVideoInfo *ret = gst_video_info_new ();

  if (gst_video_info_from_caps (ret, caps)) {
    return ret;
  } else {
    gst_video_info_free (ret);
    return NULL;
  }
}

/**
 * gst_video_info_is_equal:
 * @info: a #GstVideoInfo
 * @other: a #GstVideoInfo
 *
 * Compares two #GstVideoInfo and returns whether they are equal or not
 *
 * Returns: %TRUE if @info and @other are equal, else %FALSE.
 */
gboolean
gst_video_info_is_equal (const GstVideoInfo * info, const GstVideoInfo * other)
{
  gint i;

  if (info == other)
    return TRUE;
  if (info->finfo == NULL || other->finfo == NULL)
    return FALSE;
  if (GST_VIDEO_INFO_FORMAT (info) != GST_VIDEO_INFO_FORMAT (other))
    return FALSE;
  if (GST_VIDEO_INFO_INTERLACE_MODE (info) !=
      GST_VIDEO_INFO_INTERLACE_MODE (other))
    return FALSE;
  if (GST_VIDEO_INFO_FLAGS (info) != GST_VIDEO_INFO_FLAGS (other))
    return FALSE;
  if (GST_VIDEO_INFO_WIDTH (info) != GST_VIDEO_INFO_WIDTH (other))
    return FALSE;
  if (GST_VIDEO_INFO_HEIGHT (info) != GST_VIDEO_INFO_HEIGHT (other))
    return FALSE;
  if (GST_VIDEO_INFO_SIZE (info) != GST_VIDEO_INFO_SIZE (other))
    return FALSE;
  if (GST_VIDEO_INFO_PAR_N (info) != GST_VIDEO_INFO_PAR_N (other))
    return FALSE;
  if (GST_VIDEO_INFO_PAR_D (info) != GST_VIDEO_INFO_PAR_D (other))
    return FALSE;
  if (GST_VIDEO_INFO_FPS_N (info) != GST_VIDEO_INFO_FPS_N (other))
    return FALSE;
  if (GST_VIDEO_INFO_FPS_D (info) != GST_VIDEO_INFO_FPS_D (other))
    return FALSE;
  if (!gst_video_colorimetry_is_equal (&GST_VIDEO_INFO_COLORIMETRY (info),
          &GST_VIDEO_INFO_COLORIMETRY (other)))
    return FALSE;
  if (GST_VIDEO_INFO_CHROMA_SITE (info) != GST_VIDEO_INFO_CHROMA_SITE (other))
    return FALSE;
  if (GST_VIDEO_INFO_MULTIVIEW_MODE (info) !=
      GST_VIDEO_INFO_MULTIVIEW_MODE (other))
    return FALSE;
  if (GST_VIDEO_INFO_MULTIVIEW_FLAGS (info) !=
      GST_VIDEO_INFO_MULTIVIEW_FLAGS (other))
    return FALSE;
  if (GST_VIDEO_INFO_VIEWS (info) != GST_VIDEO_INFO_VIEWS (other))
    return FALSE;

  for (i = 0; i < info->finfo->n_planes; i++) {
    if (info->stride[i] != other->stride[i])
      return FALSE;
    if (info->offset[i] != other->offset[i])
      return FALSE;
  }

  return TRUE;
}

/**
 * gst_video_info_to_caps:
 * @info: a #GstVideoInfo
 *
 * Convert the values of @info into a #GstCaps.
 *
 * Returns: (transfer full): a new #GstCaps containing the info of @info.
 */
GstCaps *
gst_video_info_to_caps (const GstVideoInfo * info)
{
  GstCaps *caps;
  const gchar *format;
  gchar *color;
  gint par_n, par_d;
  GstVideoColorimetry colorimetry;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (info->finfo != NULL, NULL);
  g_return_val_if_fail (info->finfo->format != GST_VIDEO_FORMAT_UNKNOWN, NULL);

  format = gst_video_format_to_string (info->finfo->format);
  g_return_val_if_fail (format != NULL, NULL);

  caps = gst_caps_new_simple ("video/x-raw",
      "format", G_TYPE_STRING, format,
      "width", G_TYPE_INT, info->width,
      "height", G_TYPE_INT, info->height, NULL);

  par_n = info->par_n;
  par_d = info->par_d;

  gst_caps_set_simple (caps, "interlace-mode", G_TYPE_STRING,
      gst_video_interlace_mode_to_string (info->interlace_mode), NULL);

  if ((info->interlace_mode == GST_VIDEO_INTERLACE_MODE_INTERLEAVED ||
          info->interlace_mode == GST_VIDEO_INTERLACE_MODE_ALTERNATE) &&
      GST_VIDEO_INFO_FIELD_ORDER (info) != GST_VIDEO_FIELD_ORDER_UNKNOWN) {
    gst_caps_set_simple (caps, "field-order", G_TYPE_STRING,
        gst_video_field_order_to_string (GST_VIDEO_INFO_FIELD_ORDER (info)),
        NULL);
  }

  if (info->interlace_mode == GST_VIDEO_INTERLACE_MODE_ALTERNATE) {
    /* 'alternate' mode must always be accompanied by interlaced caps feature.
     */
    GstCapsFeatures *features;

    features = gst_caps_features_new (GST_CAPS_FEATURE_FORMAT_INTERLACED, NULL);
    gst_caps_set_features (caps, 0, features);
  }

  if (GST_VIDEO_INFO_MULTIVIEW_MODE (info) != GST_VIDEO_MULTIVIEW_MODE_NONE) {
    const gchar *caps_str = NULL;
    GstVideoMultiviewFlags multiview_flags =
        GST_VIDEO_INFO_MULTIVIEW_FLAGS (info);

    /* If the half-aspect flag is set, applying it into the PAR of the
     * resulting caps now seems safe, and helps with automatic behaviour
     * in elements that aren't explicitly multiview aware */
    if (multiview_flags & GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT) {
      multiview_flags &= ~GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT;
      switch (GST_VIDEO_INFO_MULTIVIEW_MODE (info)) {
        case GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE:
        case GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE_QUINCUNX:
        case GST_VIDEO_MULTIVIEW_MODE_COLUMN_INTERLEAVED:
        case GST_VIDEO_MULTIVIEW_MODE_CHECKERBOARD:
          par_n *= 2;           /* double the width / half the height */
          break;
        case GST_VIDEO_MULTIVIEW_MODE_ROW_INTERLEAVED:
        case GST_VIDEO_MULTIVIEW_MODE_TOP_BOTTOM:
          par_d *= 2;           /* half the width / double the height */
          break;
        default:
          break;
      }
    }

    caps_str =
        gst_video_multiview_mode_to_caps_string (GST_VIDEO_INFO_MULTIVIEW_MODE
        (info));
    if (caps_str != NULL) {
      gst_caps_set_simple (caps, "multiview-mode", G_TYPE_STRING,
          caps_str, "multiview-flags", GST_TYPE_VIDEO_MULTIVIEW_FLAGSET,
          multiview_flags, GST_FLAG_SET_MASK_EXACT, NULL);
    }
  }

  gst_caps_set_simple (caps, "pixel-aspect-ratio",
      GST_TYPE_FRACTION, par_n, par_d, NULL);

  if (info->chroma_site != GST_VIDEO_CHROMA_SITE_UNKNOWN) {
    gchar *chroma_site = gst_video_chroma_site_to_string (info->chroma_site);

    if (!chroma_site) {
      GST_WARNING ("Couldn't convert chroma-site 0x%x to string",
          info->chroma_site);
    } else {
      gst_caps_set_simple (caps,
          "chroma-site", G_TYPE_STRING, chroma_site, NULL);
      g_free (chroma_site);
    }
  }

  /* make sure we set the RGB matrix for RGB formats */
  colorimetry = info->colorimetry;
  if (GST_VIDEO_FORMAT_INFO_IS_RGB (info->finfo) &&
      colorimetry.matrix != GST_VIDEO_COLOR_MATRIX_RGB) {
    GST_WARNING ("invalid matrix %d for RGB format, using RGB",
        colorimetry.matrix);
    colorimetry.matrix = GST_VIDEO_COLOR_MATRIX_RGB;
  }
  if ((color = gst_video_colorimetry_to_string (&colorimetry))) {
    gst_caps_set_simple (caps, "colorimetry", G_TYPE_STRING, color, NULL);
    g_free (color);
  }

  if (info->views > 1)
    gst_caps_set_simple (caps, "views", G_TYPE_INT, info->views, NULL);

  if (info->flags & GST_VIDEO_FLAG_VARIABLE_FPS && info->fps_n != 0) {
    /* variable fps with a max-framerate */
    gst_caps_set_simple (caps, "framerate", GST_TYPE_FRACTION, 0, 1,
        "max-framerate", GST_TYPE_FRACTION, info->fps_n, info->fps_d, NULL);
  } else {
    /* no variable fps or no max-framerate */
    gst_caps_set_simple (caps, "framerate", GST_TYPE_FRACTION,
        info->fps_n, info->fps_d, NULL);
  }

  return caps;
}

static gboolean
fill_planes (GstVideoInfo * info, gsize plane_size[GST_VIDEO_MAX_PLANES])
{
  gsize width, height, cr_h;
  gint bpp = 0, i;

  width = (gsize) info->width;
  height = (gsize) GST_VIDEO_INFO_FIELD_HEIGHT (info);

  /* Sanity check the resulting frame size for overflows */
  for (i = 0; i < GST_VIDEO_INFO_N_COMPONENTS (info); i++)
    bpp += GST_VIDEO_INFO_COMP_DEPTH (info, i);
  bpp = GST_ROUND_UP_8 (bpp) / 8;
  if (bpp > 0 && GST_ROUND_UP_128 ((guint64) width) * ((guint64) height) >=
      G_MAXUINT / bpp) {
    GST_ERROR ("Frame size %ux%u would overflow", info->width, info->height);
    return FALSE;
  }

  switch (info->finfo->format) {
    case GST_VIDEO_FORMAT_YUY2:
    case GST_VIDEO_FORMAT_YVYU:
    case GST_VIDEO_FORMAT_UYVY:
    case GST_VIDEO_FORMAT_VYUY:
      info->stride[0] = GST_ROUND_UP_4 (width * 2);
      info->offset[0] = 0;
      info->size = info->stride[0] * height;
      break;
    case GST_VIDEO_FORMAT_AYUV:
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_BGRx:
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_xRGB:
    case GST_VIDEO_FORMAT_ARGB:
    case GST_VIDEO_FORMAT_xBGR:
    case GST_VIDEO_FORMAT_ABGR:
    case GST_VIDEO_FORMAT_r210:
    case GST_VIDEO_FORMAT_Y410:
    case GST_VIDEO_FORMAT_VUYA:
    case GST_VIDEO_FORMAT_BGR10A2_LE:
    case GST_VIDEO_FORMAT_RGB10A2_LE:
    case GST_VIDEO_FORMAT_RBGA:
      info->stride[0] = width * 4;
      info->offset[0] = 0;
      info->size = info->stride[0] * height;
      break;
    case GST_VIDEO_FORMAT_RGB16:
    case GST_VIDEO_FORMAT_BGR16:
    case GST_VIDEO_FORMAT_RGB15:
    case GST_VIDEO_FORMAT_BGR15:
      info->stride[0] = GST_ROUND_UP_4 (width * 2);
      info->offset[0] = 0;
      info->size = info->stride[0] * height;
      break;
    case GST_VIDEO_FORMAT_RGB:
    case GST_VIDEO_FORMAT_BGR:
    case GST_VIDEO_FORMAT_v308:
    case GST_VIDEO_FORMAT_IYU2:
      info->stride[0] = GST_ROUND_UP_4 (width * 3);
      info->offset[0] = 0;
      info->size = info->stride[0] * height;
      break;
    case GST_VIDEO_FORMAT_v210:
      info->stride[0] = ((width + 47) / 48) * 128;
      info->offset[0] = 0;
      info->size = info->stride[0] * height;
      break;
    case GST_VIDEO_FORMAT_v216:
    case GST_VIDEO_FORMAT_Y210:
    case GST_VIDEO_FORMAT_Y212_BE:
    case GST_VIDEO_FORMAT_Y212_LE:
      info->stride[0] = GST_ROUND_UP_8 (width * 4);
      info->offset[0] = 0;
      info->size = info->stride[0] * height;
      break;
    case GST_VIDEO_FORMAT_GRAY8:
      info->stride[0] = GST_ROUND_UP_4 (width);
      info->offset[0] = 0;
      info->size = info->stride[0] * height;
      break;
    case GST_VIDEO_FORMAT_GRAY16_BE:
    case GST_VIDEO_FORMAT_GRAY16_LE:
      info->stride[0] = GST_ROUND_UP_4 (width * 2);
      info->offset[0] = 0;
      info->size = info->stride[0] * height;
      break;
    case GST_VIDEO_FORMAT_UYVP:
      info->stride[0] = GST_ROUND_UP_4 ((width * 2 * 5 + 3) / 4);
      info->offset[0] = 0;
      info->size = info->stride[0] * height;
      break;
    case GST_VIDEO_FORMAT_RGB8P:
      info->stride[0] = GST_ROUND_UP_4 (width);
      info->stride[1] = 4;
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * height;
      info->size = info->offset[1] + (4 * 256);
      break;
    case GST_VIDEO_FORMAT_IYU1:
      info->stride[0] = GST_ROUND_UP_4 (GST_ROUND_UP_4 (width) +
          GST_ROUND_UP_4 (width) / 2);
      info->offset[0] = 0;
      info->size = info->stride[0] * height;
      break;
    case GST_VIDEO_FORMAT_ARGB64:
    case GST_VIDEO_FORMAT_ARGB64_BE:
    case GST_VIDEO_FORMAT_ARGB64_LE:
    case GST_VIDEO_FORMAT_RGBA64_BE:
    case GST_VIDEO_FORMAT_RGBA64_LE:
    case GST_VIDEO_FORMAT_BGRA64_BE:
    case GST_VIDEO_FORMAT_BGRA64_LE:
    case GST_VIDEO_FORMAT_ABGR64_BE:
    case GST_VIDEO_FORMAT_ABGR64_LE:
    case GST_VIDEO_FORMAT_AYUV64:
    case GST_VIDEO_FORMAT_Y412_BE:
    case GST_VIDEO_FORMAT_Y412_LE:
      info->stride[0] = width * 8;
      info->offset[0] = 0;
      info->size = info->stride[0] * height;
      break;
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_YV12:        /* same as I420, but plane 1+2 swapped */
      info->stride[0] = GST_ROUND_UP_4 (width);
      info->stride[1] = GST_ROUND_UP_4 (GST_ROUND_UP_2 (width) / 2);
      info->stride[2] = info->stride[1];
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * GST_ROUND_UP_2 (height);
      cr_h = GST_ROUND_UP_2 (height) / 2;
      if (GST_VIDEO_INFO_IS_INTERLACED (info))
        cr_h = GST_ROUND_UP_2 (cr_h);
      info->offset[2] = info->offset[1] + info->stride[1] * cr_h;
      info->size = info->offset[2] + info->stride[2] * cr_h;
      break;
    case GST_VIDEO_FORMAT_Y41B:
      info->stride[0] = GST_ROUND_UP_4 (width);
      info->stride[1] = GST_ROUND_UP_16 (width) / 4;
      info->stride[2] = info->stride[1];
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * height;
      info->offset[2] = info->offset[1] + info->stride[1] * height;
      /* simplification of ROUNDUP4(w)*h + 2*((ROUNDUP16(w)/4)*h */
      info->size = (info->stride[0] + (GST_ROUND_UP_16 (width) / 2)) * height;
      break;
    case GST_VIDEO_FORMAT_Y42B:
      info->stride[0] = GST_ROUND_UP_4 (width);
      info->stride[1] = GST_ROUND_UP_8 (width) / 2;
      info->stride[2] = info->stride[1];
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * height;
      info->offset[2] = info->offset[1] + info->stride[1] * height;
      /* simplification of ROUNDUP4(w)*h + 2*(ROUNDUP8(w)/2)*h */
      info->size = (info->stride[0] + GST_ROUND_UP_8 (width)) * height;
      break;
    case GST_VIDEO_FORMAT_Y444:
    case GST_VIDEO_FORMAT_GBR:
    case GST_VIDEO_FORMAT_RGBP:
    case GST_VIDEO_FORMAT_BGRP:
      info->stride[0] = GST_ROUND_UP_4 (width);
      info->stride[1] = info->stride[0];
      info->stride[2] = info->stride[0];
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * height;
      info->offset[2] = info->offset[1] * 2;
      info->size = info->stride[0] * height * 3;
      break;
    case GST_VIDEO_FORMAT_GBRA:
      info->stride[0] = GST_ROUND_UP_4 (width);
      info->stride[1] = info->stride[0];
      info->stride[2] = info->stride[0];
      info->stride[3] = info->stride[0];
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * height;
      info->offset[2] = info->offset[1] * 2;
      info->offset[3] = info->offset[1] * 3;
      info->size = info->stride[0] * height * 4;
      break;
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_NV21:
      info->stride[0] = GST_ROUND_UP_4 (width);
      info->stride[1] = info->stride[0];
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * GST_ROUND_UP_2 (height);
      cr_h = GST_ROUND_UP_2 (height) / 2;
      if (GST_VIDEO_INFO_IS_INTERLACED (info))
        cr_h = GST_ROUND_UP_2 (cr_h);
      info->size = info->offset[1] + info->stride[0] * cr_h;
      break;
    case GST_VIDEO_FORMAT_AV12:
      info->stride[0] = GST_ROUND_UP_4 (width);
      info->stride[1] = info->stride[0];
      info->stride[2] = info->stride[0];
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * GST_ROUND_UP_2 (height);
      info->offset[2] =
          info->offset[1] + (info->stride[1] * GST_ROUND_UP_2 (height) / 2);
      info->size = info->offset[2] + info->stride[2] * GST_ROUND_UP_2 (height);
      break;
    case GST_VIDEO_FORMAT_NV16:
    case GST_VIDEO_FORMAT_NV61:
      info->stride[0] = GST_ROUND_UP_4 (width);
      info->stride[1] = info->stride[0];
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * height;
      info->size = info->stride[0] * height * 2;
      break;
    case GST_VIDEO_FORMAT_NV24:
      info->stride[0] = GST_ROUND_UP_4 (width);
      info->stride[1] = GST_ROUND_UP_4 (width * 2);
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * height;
      info->size = info->stride[0] * height + info->stride[1] * height;
      break;
    case GST_VIDEO_FORMAT_A420:
      info->stride[0] = GST_ROUND_UP_4 (width);
      info->stride[1] = GST_ROUND_UP_4 (GST_ROUND_UP_2 (width) / 2);
      info->stride[2] = info->stride[1];
      info->stride[3] = info->stride[0];
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * GST_ROUND_UP_2 (height);
      cr_h = GST_ROUND_UP_2 (height) / 2;
      if (GST_VIDEO_INFO_IS_INTERLACED (info))
        cr_h = GST_ROUND_UP_2 (cr_h);
      info->offset[2] = info->offset[1] + info->stride[1] * cr_h;
      info->offset[3] = info->offset[2] + info->stride[2] * cr_h;
      info->size = info->offset[3] + info->stride[0] * GST_ROUND_UP_2 (height);
      break;
    case GST_VIDEO_FORMAT_A422:
      info->stride[0] = GST_ROUND_UP_4 (width);
      info->stride[1] = GST_ROUND_UP_8 (width) / 2;
      info->stride[2] = info->stride[1];
      info->stride[3] = info->stride[0];
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * GST_ROUND_UP_2 (height);
      info->offset[2] = info->offset[1] +
          info->stride[1] * GST_ROUND_UP_2 (height);
      info->offset[3] =
          info->offset[2] + info->stride[2] * GST_ROUND_UP_2 (height);
      info->size = info->offset[3] + info->stride[0] * GST_ROUND_UP_2 (height);
      break;
    case GST_VIDEO_FORMAT_A444:
      info->stride[0] = GST_ROUND_UP_4 (width);
      info->stride[1] = info->stride[0];
      info->stride[2] = info->stride[0];
      info->stride[3] = info->stride[0];
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * GST_ROUND_UP_2 (height);
      info->offset[2] = info->offset[1] +
          info->stride[1] * GST_ROUND_UP_2 (height);
      info->offset[3] =
          info->offset[2] + info->stride[2] * GST_ROUND_UP_2 (height);
      info->size = info->offset[3] + info->stride[0] * GST_ROUND_UP_2 (height);
      break;
    case GST_VIDEO_FORMAT_YUV9:
    case GST_VIDEO_FORMAT_YVU9:
      info->stride[0] = GST_ROUND_UP_4 (width);
      info->stride[1] = GST_ROUND_UP_4 (GST_ROUND_UP_4 (width) / 4);
      info->stride[2] = info->stride[1];
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * height;
      cr_h = GST_ROUND_UP_4 (height) / 4;
      if (GST_VIDEO_INFO_IS_INTERLACED (info))
        cr_h = GST_ROUND_UP_2 (cr_h);
      info->offset[2] = info->offset[1] + info->stride[1] * cr_h;
      info->size = info->offset[2] + info->stride[2] * cr_h;
      break;
    case GST_VIDEO_FORMAT_I420_10LE:
    case GST_VIDEO_FORMAT_I420_10BE:
    case GST_VIDEO_FORMAT_I420_12LE:
    case GST_VIDEO_FORMAT_I420_12BE:
      info->stride[0] = GST_ROUND_UP_4 (width * 2);
      info->stride[1] = GST_ROUND_UP_4 (width);
      info->stride[2] = info->stride[1];
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * GST_ROUND_UP_2 (height);
      cr_h = GST_ROUND_UP_2 (height) / 2;
      if (GST_VIDEO_INFO_IS_INTERLACED (info))
        cr_h = GST_ROUND_UP_2 (cr_h);
      info->offset[2] = info->offset[1] + info->stride[1] * cr_h;
      info->size = info->offset[2] + info->stride[2] * cr_h;
      break;
    case GST_VIDEO_FORMAT_I422_10LE:
    case GST_VIDEO_FORMAT_I422_10BE:
    case GST_VIDEO_FORMAT_I422_12LE:
    case GST_VIDEO_FORMAT_I422_12BE:
      info->stride[0] = GST_ROUND_UP_4 (width * 2);
      info->stride[1] = GST_ROUND_UP_4 (width);
      info->stride[2] = info->stride[1];
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * GST_ROUND_UP_2 (height);
      info->offset[2] = info->offset[1] +
          info->stride[1] * GST_ROUND_UP_2 (height);
      info->size = info->offset[2] + info->stride[2] * GST_ROUND_UP_2 (height);
      break;
    case GST_VIDEO_FORMAT_Y444_10LE:
    case GST_VIDEO_FORMAT_Y444_10BE:
    case GST_VIDEO_FORMAT_Y444_12LE:
    case GST_VIDEO_FORMAT_Y444_12BE:
    case GST_VIDEO_FORMAT_GBR_10LE:
    case GST_VIDEO_FORMAT_GBR_10BE:
    case GST_VIDEO_FORMAT_GBR_12LE:
    case GST_VIDEO_FORMAT_GBR_12BE:
    case GST_VIDEO_FORMAT_GBR_16LE:
    case GST_VIDEO_FORMAT_GBR_16BE:
    case GST_VIDEO_FORMAT_Y444_16LE:
    case GST_VIDEO_FORMAT_Y444_16BE:
      info->stride[0] = GST_ROUND_UP_4 (width * 2);
      info->stride[1] = info->stride[0];
      info->stride[2] = info->stride[0];
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * height;
      info->offset[2] = info->offset[1] * 2;
      info->size = info->stride[0] * height * 3;
      break;
    case GST_VIDEO_FORMAT_GBRA_10LE:
    case GST_VIDEO_FORMAT_GBRA_10BE:
    case GST_VIDEO_FORMAT_GBRA_12LE:
    case GST_VIDEO_FORMAT_GBRA_12BE:
      info->stride[0] = GST_ROUND_UP_4 (width * 2);
      info->stride[1] = info->stride[0];
      info->stride[2] = info->stride[0];
      info->stride[3] = info->stride[0];
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * height;
      info->offset[2] = info->offset[1] * 2;
      info->offset[3] = info->offset[1] * 3;
      info->size = info->stride[0] * height * 4;
      break;
    case GST_VIDEO_FORMAT_NV12_64Z32:
      info->stride[0] =
          GST_VIDEO_TILE_MAKE_STRIDE (GST_ROUND_UP_128 (width) / 64,
          GST_ROUND_UP_32 (height) / 32);
      info->stride[1] =
          GST_VIDEO_TILE_MAKE_STRIDE (GST_ROUND_UP_128 (width) / 64,
          GST_ROUND_UP_64 (height) / 64);
      info->offset[0] = 0;
      info->offset[1] = GST_ROUND_UP_128 (width) * GST_ROUND_UP_32 (height);
      info->size = info->offset[1] +
          GST_ROUND_UP_128 (width) * (GST_ROUND_UP_64 (height) / 2);
      break;
    case GST_VIDEO_FORMAT_NV12_4L4:
    case GST_VIDEO_FORMAT_NV12_32L32:
    case GST_VIDEO_FORMAT_NV12_10LE40_4L4:
    {
      const gsize tile_size = GST_VIDEO_FORMAT_INFO_TILE_SIZE (info->finfo, 0);
      gint tile_width = GST_VIDEO_FORMAT_INFO_TILE_WIDTH (info->finfo, 0);
      gint tile_height = GST_VIDEO_FORMAT_INFO_TILE_HEIGHT (info->finfo, 0);
      gint n_tile_x = GST_ROUND_UP_N (info->width, tile_width) / tile_width;
      gint n_tile_y = GST_ROUND_UP_N (info->height, tile_height) / tile_height;

      info->stride[0] = GST_VIDEO_TILE_MAKE_STRIDE (n_tile_x, n_tile_y);
      info->stride[1] = GST_VIDEO_TILE_MAKE_STRIDE (n_tile_x,
          GST_ROUND_UP_2 (n_tile_y) / 2);
      info->offset[1] = n_tile_x * n_tile_y * tile_size;
      info->size = info->offset[1] +
          n_tile_x * GST_ROUND_UP_2 (n_tile_y) / 2 * tile_size;
      break;
    }
    case GST_VIDEO_FORMAT_NV12_16L32S:
    {
      const gsize y_tile_size =
          GST_VIDEO_FORMAT_INFO_TILE_SIZE (info->finfo, 0);
      const gsize uv_tile_size =
          GST_VIDEO_FORMAT_INFO_TILE_SIZE (info->finfo, 1);
      gint tile_width = GST_VIDEO_FORMAT_INFO_TILE_WIDTH (info->finfo, 0);
      gint tile_height = GST_VIDEO_FORMAT_INFO_TILE_HEIGHT (info->finfo, 0);
      gint n_tile_x = GST_ROUND_UP_N (info->width, tile_width) / tile_width;
      gint n_tile_y = GST_ROUND_UP_N (info->height, tile_height) / tile_height;

      info->stride[0] = GST_VIDEO_TILE_MAKE_STRIDE (n_tile_x, n_tile_y);
      /*
       * size of UV plane tiles is subsample, hence have the same number of
       * tiles in both directions.
       */
      info->stride[1] = info->stride[0];
      info->offset[0] = 0;
      info->offset[1] = n_tile_x * n_tile_y * y_tile_size;
      info->size = info->offset[1] + n_tile_x * n_tile_y * uv_tile_size;
      break;
    }
    case GST_VIDEO_FORMAT_A420_10LE:
    case GST_VIDEO_FORMAT_A420_10BE:
    case GST_VIDEO_FORMAT_A420_12LE:
    case GST_VIDEO_FORMAT_A420_12BE:
    case GST_VIDEO_FORMAT_A420_16LE:
    case GST_VIDEO_FORMAT_A420_16BE:
      info->stride[0] = GST_ROUND_UP_4 (width * 2);
      info->stride[1] = GST_ROUND_UP_4 (width);
      info->stride[2] = info->stride[1];
      info->stride[3] = info->stride[0];
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * GST_ROUND_UP_2 (height);
      cr_h = GST_ROUND_UP_2 (height) / 2;
      if (GST_VIDEO_INFO_IS_INTERLACED (info))
        cr_h = GST_ROUND_UP_2 (cr_h);
      info->offset[2] = info->offset[1] + info->stride[1] * cr_h;
      info->offset[3] = info->offset[2] + info->stride[2] * cr_h;
      info->size = info->offset[3] + info->stride[0] * GST_ROUND_UP_2 (height);
      break;
    case GST_VIDEO_FORMAT_A422_10LE:
    case GST_VIDEO_FORMAT_A422_10BE:
    case GST_VIDEO_FORMAT_A422_12LE:
    case GST_VIDEO_FORMAT_A422_12BE:
    case GST_VIDEO_FORMAT_A422_16LE:
    case GST_VIDEO_FORMAT_A422_16BE:
      info->stride[0] = GST_ROUND_UP_4 (width * 2);
      info->stride[1] = GST_ROUND_UP_4 (width);
      info->stride[2] = info->stride[1];
      info->stride[3] = info->stride[0];
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * GST_ROUND_UP_2 (height);
      info->offset[2] = info->offset[1] +
          info->stride[1] * GST_ROUND_UP_2 (height);
      info->offset[3] =
          info->offset[2] + info->stride[2] * GST_ROUND_UP_2 (height);
      info->size = info->offset[3] + info->stride[0] * GST_ROUND_UP_2 (height);
      break;
    case GST_VIDEO_FORMAT_A444_10LE:
    case GST_VIDEO_FORMAT_A444_10BE:
    case GST_VIDEO_FORMAT_A444_12LE:
    case GST_VIDEO_FORMAT_A444_12BE:
    case GST_VIDEO_FORMAT_A444_16LE:
    case GST_VIDEO_FORMAT_A444_16BE:
      info->stride[0] = GST_ROUND_UP_4 (width * 2);
      info->stride[1] = info->stride[0];
      info->stride[2] = info->stride[0];
      info->stride[3] = info->stride[0];
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * height;
      info->offset[2] = info->offset[1] * 2;
      info->offset[3] = info->offset[1] * 3;
      info->size = info->stride[0] * height * 4;
      break;
    case GST_VIDEO_FORMAT_P010_10LE:
    case GST_VIDEO_FORMAT_P010_10BE:
    case GST_VIDEO_FORMAT_P016_LE:
    case GST_VIDEO_FORMAT_P016_BE:
    case GST_VIDEO_FORMAT_P012_LE:
    case GST_VIDEO_FORMAT_P012_BE:
      info->stride[0] = GST_ROUND_UP_4 (width * 2);
      info->stride[1] = info->stride[0];
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * GST_ROUND_UP_2 (height);
      cr_h = GST_ROUND_UP_2 (height) / 2;
      info->size = info->offset[1] + info->stride[0] * cr_h;
      break;
    case GST_VIDEO_FORMAT_GRAY10_LE32:
      info->stride[0] = (width + 2) / 3 * 4;
      info->offset[0] = 0;
      info->size = info->stride[0] * GST_ROUND_UP_2 (height);
      break;
    case GST_VIDEO_FORMAT_NV12_10LE32:
      info->stride[0] = (width + 2) / 3 * 4;
      info->stride[1] = info->stride[0];
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * GST_ROUND_UP_2 (height);
      cr_h = GST_ROUND_UP_2 (height) / 2;
      if (GST_VIDEO_INFO_IS_INTERLACED (info))
        cr_h = GST_ROUND_UP_2 (cr_h);
      info->size = info->offset[1] + info->stride[0] * cr_h;
      break;
    case GST_VIDEO_FORMAT_NV16_10LE32:
      info->stride[0] = (width + 2) / 3 * 4;
      info->stride[1] = info->stride[0];
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * height;
      info->size = info->stride[0] * height * 2;
      break;
    case GST_VIDEO_FORMAT_NV12_10LE40:
      info->stride[0] = ((width * 5 >> 2) + 4) / 5 * 5;
      info->stride[1] = info->stride[0];
      info->offset[0] = 0;
      info->offset[1] = info->stride[0] * GST_ROUND_UP_2 (height);
      cr_h = GST_ROUND_UP_2 (height) / 2;
      if (GST_VIDEO_INFO_IS_INTERLACED (info))
        cr_h = GST_ROUND_UP_2 (cr_h);
      info->size = info->offset[1] + info->stride[0] * cr_h;
      break;
    case GST_VIDEO_FORMAT_NV12_8L128:
    {
      gint tile_size = GST_VIDEO_FORMAT_INFO_TILE_SIZE (info->finfo, 0);
      gint tile_width = GST_VIDEO_FORMAT_INFO_TILE_WIDTH (info->finfo, 0);
      gint tile_height = GST_VIDEO_FORMAT_INFO_TILE_HEIGHT (info->finfo, 0);
      gint ALIGN_W = tile_width;
      gint ALIGN_H = tile_height;
      guint n_tile_x, y_n_tile_y, uv_n_tile_y;

      if (GST_VIDEO_INFO_IS_INTERLACED (info))
        ALIGN_H *= 2;

      n_tile_x = GST_ROUND_UP_N (width, ALIGN_W) / tile_width;
      y_n_tile_y = GST_ROUND_UP_N (height, ALIGN_H) / tile_height;
      uv_n_tile_y = GST_ROUND_UP_2 (y_n_tile_y) / 2;

      info->stride[0] = GST_VIDEO_TILE_MAKE_STRIDE (n_tile_x, y_n_tile_y);
      info->stride[1] = GST_VIDEO_TILE_MAKE_STRIDE (n_tile_x, uv_n_tile_y);
      info->offset[0] = 0;
      info->offset[1] = n_tile_x * y_n_tile_y * tile_size;
      info->size = info->offset[1] + n_tile_x * uv_n_tile_y * tile_size;
      break;
    }
    case GST_VIDEO_FORMAT_NV12_10BE_8L128:
    {
      gint tile_size = GST_VIDEO_FORMAT_INFO_TILE_SIZE (info->finfo, 0);
      gint tile_stride = GST_VIDEO_FORMAT_INFO_TILE_STRIDE (info->finfo, 0);
      gint tile_height = GST_VIDEO_FORMAT_INFO_TILE_HEIGHT (info->finfo, 0);
      gint stride = GST_ROUND_UP_8 (width * 10) >> 3;
      gint ALIGN_W = tile_stride;
      gint ALIGN_H = tile_height;
      guint n_tile_x, y_n_tile_y, uv_n_tile_y;

      if (GST_VIDEO_INFO_IS_INTERLACED (info))
        ALIGN_H *= 2;

      n_tile_x = GST_ROUND_UP_N (stride, ALIGN_W) / tile_stride;
      y_n_tile_y = GST_ROUND_UP_N (height, ALIGN_H) / tile_height;
      uv_n_tile_y = GST_ROUND_UP_2 (y_n_tile_y) / 2;

      info->stride[0] = GST_VIDEO_TILE_MAKE_STRIDE (n_tile_x, y_n_tile_y);
      info->stride[1] = GST_VIDEO_TILE_MAKE_STRIDE (n_tile_x, uv_n_tile_y);
      info->offset[0] = 0;
      info->offset[1] = n_tile_x * y_n_tile_y * tile_size;
      info->size = info->offset[1] + n_tile_x * uv_n_tile_y * tile_size;
      break;
    }
    case GST_VIDEO_FORMAT_MT2110T:
    case GST_VIDEO_FORMAT_MT2110R:
    {
      const gsize tile_size = GST_VIDEO_FORMAT_INFO_TILE_SIZE (info->finfo, 0);
      gint n_tile_x = GST_ROUND_UP_16 (info->width) / 16;
      gint n_tile_y = GST_ROUND_UP_32 (info->height) / 32;

      info->stride[0] = GST_VIDEO_TILE_MAKE_STRIDE (n_tile_x, n_tile_y);
      info->stride[1] = info->stride[0];
      info->offset[0] = 0;
      info->offset[1] = tile_size * n_tile_x * n_tile_y;
      info->size = info->offset[1] + info->offset[1] / 2;
      break;
    }
    case GST_VIDEO_FORMAT_ENCODED:
    case GST_VIDEO_FORMAT_DMA_DRM:
      break;
    case GST_VIDEO_FORMAT_UNKNOWN:
      GST_ERROR ("invalid format");
      g_warning ("invalid format");
      return FALSE;
      break;
  }

  if (plane_size) {
    for (i = 0; i < GST_VIDEO_MAX_PLANES; i++) {
      if (i < GST_VIDEO_INFO_N_PLANES (info)) {
        if (GST_VIDEO_FORMAT_INFO_IS_TILED (info->finfo)) {
          guint x_tiles = GST_VIDEO_TILE_X_TILES (info->stride[i]);
          guint y_tiles = GST_VIDEO_TILE_Y_TILES (info->stride[i]);
          plane_size[i] = x_tiles * y_tiles *
              GST_VIDEO_FORMAT_INFO_TILE_SIZE (info->finfo, i);
        } else {
          gint comp[GST_VIDEO_MAX_COMPONENTS];
          guint plane_height;

          /* Convert plane index to component index */
          gst_video_format_info_component (info->finfo, i, comp);
          plane_height =
              GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (info->finfo, comp[0],
              GST_VIDEO_INFO_FIELD_HEIGHT (info));
          plane_size[i] = plane_height * GST_VIDEO_INFO_PLANE_STRIDE (info, i);
        }
      } else {
        plane_size[i] = 0;
      }
    }
  }

  return TRUE;
}

/**
 * gst_video_info_convert:
 * @info: a #GstVideoInfo
 * @src_format: #GstFormat of the @src_value
 * @src_value: value to convert
 * @dest_format: #GstFormat of the @dest_value
 * @dest_value: (out): pointer to destination value
 *
 * Converts among various #GstFormat types.  This function handles
 * GST_FORMAT_BYTES, GST_FORMAT_TIME, and GST_FORMAT_DEFAULT.  For
 * raw video, GST_FORMAT_DEFAULT corresponds to video frames.  This
 * function can be used to handle pad queries of the type GST_QUERY_CONVERT.
 *
 * Returns: TRUE if the conversion was successful.
 */
gboolean
gst_video_info_convert (const GstVideoInfo * info,
    GstFormat src_format, gint64 src_value,
    GstFormat dest_format, gint64 * dest_value)
{
  gboolean ret = FALSE;
  int fps_n, fps_d;
  gsize size;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (info->finfo != NULL, 0);
  g_return_val_if_fail (info->finfo->format != GST_VIDEO_FORMAT_UNKNOWN, 0);
  g_return_val_if_fail (info->size > 0, 0);

  size = info->size;
  fps_n = info->fps_n;
  fps_d = info->fps_d;

  GST_DEBUG ("converting value %" G_GINT64_FORMAT " from %s to %s",
      src_value, gst_format_get_name (src_format),
      gst_format_get_name (dest_format));

  if (src_format == dest_format) {
    *dest_value = src_value;
    ret = TRUE;
    goto done;
  }

  if (src_value == -1) {
    *dest_value = -1;
    ret = TRUE;
    goto done;
  }

  /* bytes to frames */
  if (src_format == GST_FORMAT_BYTES && dest_format == GST_FORMAT_DEFAULT) {
    if (size != 0) {
      *dest_value = gst_util_uint64_scale (src_value, 1, size);
    } else {
      GST_ERROR ("blocksize is 0");
      *dest_value = 0;
    }
    ret = TRUE;
    goto done;
  }

  /* frames to bytes */
  if (src_format == GST_FORMAT_DEFAULT && dest_format == GST_FORMAT_BYTES) {
    *dest_value = gst_util_uint64_scale (src_value, size, 1);
    ret = TRUE;
    goto done;
  }

  /* time to frames */
  if (src_format == GST_FORMAT_TIME && dest_format == GST_FORMAT_DEFAULT) {
    if (fps_d != 0) {
      *dest_value = gst_util_uint64_scale (src_value,
          fps_n, GST_SECOND * fps_d);
    } else {
      GST_ERROR ("framerate denominator is 0");
      *dest_value = 0;
    }
    ret = TRUE;
    goto done;
  }

  /* frames to time */
  if (src_format == GST_FORMAT_DEFAULT && dest_format == GST_FORMAT_TIME) {
    if (fps_n != 0) {
      *dest_value = gst_util_uint64_scale (src_value,
          GST_SECOND * fps_d, fps_n);
    } else {
      GST_ERROR ("framerate numerator is 0");
      *dest_value = 0;
    }
    ret = TRUE;
    goto done;
  }

  /* time to bytes */
  if (src_format == GST_FORMAT_TIME && dest_format == GST_FORMAT_BYTES) {
    if (fps_d != 0) {
      *dest_value = gst_util_uint64_scale (src_value,
          fps_n * size, GST_SECOND * fps_d);
    } else {
      GST_ERROR ("framerate denominator is 0");
      *dest_value = 0;
    }
    ret = TRUE;
    goto done;
  }

  /* bytes to time */
  if (src_format == GST_FORMAT_BYTES && dest_format == GST_FORMAT_TIME) {
    if (fps_n != 0 && size != 0) {
      *dest_value = gst_util_uint64_scale (src_value,
          GST_SECOND * fps_d, fps_n * size);
    } else {
      GST_ERROR ("framerate denominator and/or blocksize is 0");
      *dest_value = 0;
    }
    ret = TRUE;
  }

done:

  GST_DEBUG ("ret=%d result %" G_GINT64_FORMAT, ret, *dest_value);

  return ret;
}

/**
 * gst_video_info_align_full:
 * @info: a #GstVideoInfo
 * @align: alignment parameters
 * @plane_size: (out) (optional): array used to store the plane sizes
 *
 * Extra padding will be added to the right side when stride alignment padding
 * is required and @align will be updated with the new padding values.
 *
 * This variant of gst_video_info_align() provides the updated size, in bytes,
 * of each video plane after the alignment, including all horizontal and vertical
 * paddings.
 *
 * In case of GST_VIDEO_INTERLACE_MODE_ALTERNATE info, the returned sizes are the
 * ones used to hold a single field, not the full frame.
 *
 * Returns: %FALSE if alignment could not be applied, e.g. because the
 *   size of a frame can't be represented as a 32 bit integer
 *
 * Since: 1.18
 */
gboolean
gst_video_info_align_full (GstVideoInfo * info, GstVideoAlignment * align,
    gsize plane_size[GST_VIDEO_MAX_PLANES])
{
  const GstVideoFormatInfo *vinfo = info->finfo;
  gint width, height;
  gint padded_width, padded_height;
  gint i, n_planes;
  gboolean aligned;

  width = GST_VIDEO_INFO_WIDTH (info);
  height = GST_VIDEO_INFO_HEIGHT (info);

  GST_LOG ("padding %u-%ux%u-%u", align->padding_top,
      align->padding_left, align->padding_right, align->padding_bottom);

  n_planes = GST_VIDEO_INFO_N_PLANES (info);

  if (GST_VIDEO_FORMAT_INFO_HAS_PALETTE (vinfo))
    n_planes--;

  /* first make sure the left padding does not cause alignment problems later */
  do {
    GST_LOG ("left padding %u", align->padding_left);
    aligned = TRUE;
    for (i = 0; i < n_planes; i++) {
      gint comp[GST_VIDEO_MAX_COMPONENTS];
      gint hedge;

      /* this is the amount of pixels to add as left padding */
      gst_video_format_info_component (vinfo, i, comp);
      hedge = GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (vinfo, comp[0],
          align->padding_left);
      hedge *= GST_VIDEO_FORMAT_INFO_PSTRIDE (vinfo, comp[0]);

      GST_LOG ("plane %d, padding %d, alignment %u", i, hedge,
          align->stride_align[i]);
      aligned &= (hedge & align->stride_align[i]) == 0;
    }
    if (aligned)
      break;

    GST_LOG ("unaligned padding, increasing padding");
    /* increase padded_width */
    align->padding_left += align->padding_left & ~(align->padding_left - 1);
  } while (!aligned);

  /* add the padding */
  padded_width = width + align->padding_left + align->padding_right;
  padded_height = height + align->padding_top + align->padding_bottom;

  do {
    GST_LOG ("padded dimension %u-%u", padded_width, padded_height);

    info->width = padded_width;
    info->height = padded_height;

    if (!fill_planes (info, plane_size))
      return FALSE;

    /* check alignment */
    aligned = TRUE;
    for (i = 0; i < n_planes; i++) {
      GST_LOG ("plane %d, stride %d, alignment %u", i, info->stride[i],
          align->stride_align[i]);
      aligned &= (info->stride[i] & align->stride_align[i]) == 0;
    }
    if (aligned)
      break;

    GST_LOG ("unaligned strides, increasing dimension");
    /* increase padded_width */
    padded_width += padded_width & ~(padded_width - 1);
  } while (!aligned);

  align->padding_right = padded_width - width - align->padding_left;

  info->width = width;
  info->height = height;

  for (i = 0; i < n_planes; i++) {
    gint comp[GST_VIDEO_MAX_COMPONENTS];
    gint vedge, hedge;

    gst_video_format_info_component (info->finfo, i, comp);
    hedge =
        GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (vinfo, comp[0], align->padding_left);
    vedge =
        GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (vinfo, comp[0], align->padding_top);

    GST_DEBUG ("plane %d: comp: %d, hedge %d vedge %d align %d stride %d", i,
        comp[0], hedge, vedge, align->stride_align[i], info->stride[i]);

    info->offset[i] += (vedge * info->stride[i]) +
        (hedge * GST_VIDEO_FORMAT_INFO_PSTRIDE (vinfo, comp[0]));
  }

  return TRUE;
}

/**
 * gst_video_info_align:
 * @info: a #GstVideoInfo
 * @align: alignment parameters
 *
 * Adjust the offset and stride fields in @info so that the padding and
 * stride alignment in @align is respected.
 *
 * Extra padding will be added to the right side when stride alignment padding
 * is required and @align will be updated with the new padding values.
 *
 * Returns: %FALSE if alignment could not be applied, e.g. because the
 *   size of a frame can't be represented as a 32 bit integer (Since: 1.12)
 */
gboolean
gst_video_info_align (GstVideoInfo * info, GstVideoAlignment * align)
{
  return gst_video_info_align_full (info, align, NULL);
}
