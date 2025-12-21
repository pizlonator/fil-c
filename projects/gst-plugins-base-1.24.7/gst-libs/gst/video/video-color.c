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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>
#include <stdio.h>
#include <math.h>

#include "video-color.h"

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT ensure_debug_category()
static GstDebugCategory *
ensure_debug_category (void)
{
  static gsize cat_gonce = 0;

  if (g_once_init_enter (&cat_gonce)) {
    gsize cat_done;

    cat_done = (gsize) _gst_debug_category_new ("video-color", 0,
        "video-color object");

    g_once_init_leave (&cat_gonce, cat_done);
  }

  return (GstDebugCategory *) cat_gonce;
}
#else
#define ensure_debug_category() /* NOOP */
#endif /* GST_DISABLE_GST_DEBUG */

typedef struct
{
  const gchar *name;
  GstVideoColorimetry color;
} ColorimetryInfo;

#define MAKE_COLORIMETRY(n,r,m,t,p) { GST_VIDEO_COLORIMETRY_ ##n, \
  { GST_VIDEO_COLOR_RANGE ##r, GST_VIDEO_COLOR_MATRIX_ ##m, \
  GST_VIDEO_TRANSFER_ ##t, GST_VIDEO_COLOR_PRIMARIES_ ##p } }

#define GST_VIDEO_COLORIMETRY_NONAME  NULL

static const ColorimetryInfo colorimetry[] = {
  MAKE_COLORIMETRY (BT601, _16_235, BT601, BT601, SMPTE170M),
  MAKE_COLORIMETRY (BT709, _16_235, BT709, BT709, BT709),
  MAKE_COLORIMETRY (SMPTE240M, _16_235, SMPTE240M, SMPTE240M, SMPTE240M),
  MAKE_COLORIMETRY (SRGB, _0_255, RGB, SRGB, BT709),
  MAKE_COLORIMETRY (BT2020, _16_235, BT2020, BT2020_12, BT2020),
  MAKE_COLORIMETRY (BT2020_10, _16_235, BT2020, BT2020_10, BT2020),
  MAKE_COLORIMETRY (BT2100_PQ, _16_235, BT2020, SMPTE2084, BT2020),
  MAKE_COLORIMETRY (BT2100_HLG, _16_235, BT2020, ARIB_STD_B67, BT2020),
  MAKE_COLORIMETRY (NONAME, _0_255, BT601, UNKNOWN, UNKNOWN),
  MAKE_COLORIMETRY (NONAME, _UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN),       /* Keep last! */
};

#define DEFAULT_UNKNOWN (G_N_ELEMENTS(colorimetry)-1)

static const ColorimetryInfo *
gst_video_get_colorimetry (const gchar * s)
{
  gint i;

  for (i = 0; colorimetry[i].name; i++) {
    if (g_str_equal (colorimetry[i].name, s))
      return &colorimetry[i];
  }
  return NULL;
}

#define CI_IS_EQUAL(ci,i) (((ci)->range == (i)->range) && \
                        ((ci)->matrix == (i)->matrix) && \
                        ((ci)->transfer == (i)->transfer) && \
                        ((ci)->primaries == (i)->primaries))

#define IS_EQUAL(ci,i) CI_IS_EQUAL(&(ci)->color, (i))

#define IS_UNKNOWN(ci) (IS_EQUAL (&colorimetry[DEFAULT_UNKNOWN], ci))

/**
 * gst_video_colorimetry_from_string:
 * @cinfo: a #GstVideoColorimetry
 * @color: a colorimetry string
 *
 * Parse the colorimetry string and update @cinfo with the parsed
 * values.
 *
 * Returns: %TRUE if @color points to valid colorimetry info.
 */
gboolean
gst_video_colorimetry_from_string (GstVideoColorimetry * cinfo,
    const gchar * color)
{
  const ColorimetryInfo *ci;
  gboolean res = FALSE;

  if (!color) {
    *cinfo = colorimetry[DEFAULT_UNKNOWN].color;
    res = TRUE;
  } else if ((ci = gst_video_get_colorimetry (color))) {
    *cinfo = ci->color;
    res = TRUE;
  } else {
    gint r, m, t, p;

    if (sscanf (color, "%d:%d:%d:%d", &r, &m, &t, &p) == 4) {
      cinfo->range = r;
      cinfo->matrix = m;
      cinfo->transfer = t;
      cinfo->primaries = p;
      res = TRUE;
    }
  }
  return res;
}

/**
 * gst_video_colorimetry_to_string:
 * @cinfo: a #GstVideoColorimetry
 *
 * Make a string representation of @cinfo.
 *
 * Returns: (transfer full) (nullable): a string representation of @cinfo
 * or %NULL if all the entries of @cinfo are unknown values.
 */
gchar *
gst_video_colorimetry_to_string (const GstVideoColorimetry * cinfo)
{
  gint i;

  for (i = 0; colorimetry[i].name; i++) {
    if (IS_EQUAL (&colorimetry[i], cinfo)) {
      return g_strdup (colorimetry[i].name);
    }
  }
  if (!IS_UNKNOWN (cinfo)) {
    return g_strdup_printf ("%d:%d:%d:%d", cinfo->range, cinfo->matrix,
        cinfo->transfer, cinfo->primaries);
  }
  return NULL;
}

/**
 * gst_video_colorimetry_matches:
 * @cinfo: a #GstVideoInfo
 * @color: a colorimetry string
 *
 * Check if the colorimetry information in @info matches that of the
 * string @color.
 *
 * Returns: %TRUE if @color conveys the same colorimetry info as the color
 * information in @info.
 */
gboolean
gst_video_colorimetry_matches (const GstVideoColorimetry * cinfo,
    const gchar * color)
{
  const ColorimetryInfo *ci;

  if ((ci = gst_video_get_colorimetry (color)))
    return IS_EQUAL (ci, cinfo);

  return FALSE;
}

/**
 * gst_video_color_range_offsets:
 * @range: a #GstVideoColorRange
 * @info: a #GstVideoFormatInfo
 * @offset: (out caller-allocates) (array fixed-size=4): output offsets
 * @scale: (out caller-allocates) (array fixed-size=4): output scale
 *
 * Compute the offset and scale values for each component of @info. For each
 * component, (c[i] - offset[i]) / scale[i] will scale the component c[i] to the
 * range [0.0 .. 1.0].
 *
 * The reverse operation (c[i] * scale[i]) + offset[i] can be used to convert
 * the component values in range [0.0 .. 1.0] back to their representation in
 * @info and @range.
 */
void
gst_video_color_range_offsets (GstVideoColorRange range,
    const GstVideoFormatInfo * info, gint offset[GST_VIDEO_MAX_COMPONENTS],
    gint scale[GST_VIDEO_MAX_COMPONENTS])
{
  gboolean yuv;

  yuv = GST_VIDEO_FORMAT_INFO_IS_YUV (info);

  switch (range) {
    default:
    case GST_VIDEO_COLOR_RANGE_0_255:
      offset[0] = 0;
      if (yuv) {
        offset[1] = 1 << (info->depth[1] - 1);
        offset[2] = 1 << (info->depth[2] - 1);
      } else {
        offset[1] = 0;
        offset[2] = 0;
      }
      scale[0] = (1 << info->depth[0]) - 1;
      scale[1] = (1 << info->depth[1]) - 1;
      scale[2] = (1 << info->depth[2]) - 1;
      break;
    case GST_VIDEO_COLOR_RANGE_16_235:
      offset[0] = 1 << (info->depth[0] - 4);
      scale[0] = 219 << (info->depth[0] - 8);
      if (yuv) {
        offset[1] = 1 << (info->depth[1] - 1);
        offset[2] = 1 << (info->depth[2] - 1);
        scale[1] = 224 << (info->depth[1] - 8);
        scale[2] = 224 << (info->depth[2] - 8);
      } else {
        offset[1] = 1 << (info->depth[1] - 4);
        offset[2] = 1 << (info->depth[2] - 4);
        scale[1] = 219 << (info->depth[1] - 8);
        scale[2] = 219 << (info->depth[2] - 8);
      }
      break;
  }
  /* alpha channel is always full range */
  offset[3] = 0;
  scale[3] = (1 << info->depth[3]) - 1;

  GST_DEBUG ("scale: %d %d %d %d", scale[0], scale[1], scale[2], scale[3]);
  GST_DEBUG ("offset: %d %d %d %d", offset[0], offset[1], offset[2], offset[3]);
}

/**
 * gst_video_colorimetry_is_equal:
 * @cinfo: a #GstVideoColorimetry
 * @other: another #GstVideoColorimetry
 *
 * Compare the 2 colorimetry sets for equality
 *
 * Returns: %TRUE if @cinfo and @other are equal.
 *
 * Since: 1.6
 */
gboolean
gst_video_colorimetry_is_equal (const GstVideoColorimetry * cinfo,
    const GstVideoColorimetry * other)
{
  g_return_val_if_fail (cinfo != NULL, FALSE);
  g_return_val_if_fail (other != NULL, FALSE);

  return CI_IS_EQUAL (cinfo, other);
}

/**
 * gst_video_colorimetry_is_equivalent:
 * @cinfo: a #GstVideoColorimetry
 * @bitdepth: bitdepth of a format associated with @cinfo
 * @other: another #GstVideoColorimetry
 * @other_bitdepth: bitdepth of a format associated with @other
 *
 * Compare the 2 colorimetry sets for functionally equality
 *
 * Returns: %TRUE if @cinfo and @other are equivalent.
 *
 * Since: 1.22
 */
gboolean
gst_video_colorimetry_is_equivalent (const GstVideoColorimetry * cinfo,
    guint bitdepth, const GstVideoColorimetry * other, guint other_bitdepth)
{
  g_return_val_if_fail (cinfo != NULL, FALSE);
  g_return_val_if_fail (other != NULL, FALSE);

  if (cinfo->range != other->range || cinfo->matrix != other->matrix)
    return FALSE;

  if (!gst_video_color_primaries_is_equivalent (cinfo->primaries,
          other->primaries)) {
    return FALSE;
  }

  return gst_video_transfer_function_is_equivalent (cinfo->transfer, bitdepth,
      other->transfer, other_bitdepth);
}

#define WP_C    0.31006, 0.31616
#define WP_D65  0.31271, 0.32902
#define WP_CENTRE (1/3), (1/3)
#define WP_WHITE 0.314, 0.351

static const GstVideoColorPrimariesInfo color_primaries[] = {
  {GST_VIDEO_COLOR_PRIMARIES_UNKNOWN, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
  {GST_VIDEO_COLOR_PRIMARIES_BT709, WP_D65, 0.64, 0.33, 0.30, 0.60, 0.15, 0.06},
  {GST_VIDEO_COLOR_PRIMARIES_BT470M, WP_C, 0.67, 0.33, 0.21, 0.71, 0.14, 0.08},
  {GST_VIDEO_COLOR_PRIMARIES_BT470BG, WP_D65, 0.64, 0.33, 0.29, 0.60, 0.15,
      0.06},
  {GST_VIDEO_COLOR_PRIMARIES_SMPTE170M, WP_D65, 0.63, 0.34, 0.31, 0.595, 0.155,
      0.07},
  {GST_VIDEO_COLOR_PRIMARIES_SMPTE240M, WP_D65, 0.63, 0.34, 0.31, 0.595, 0.155,
      0.07},
  {GST_VIDEO_COLOR_PRIMARIES_FILM, WP_C, 0.681, 0.319, 0.243, 0.692, 0.145,
      0.049},
  {GST_VIDEO_COLOR_PRIMARIES_BT2020, WP_D65, 0.708, 0.292, 0.170, 0.797, 0.131,
      0.046},
  {GST_VIDEO_COLOR_PRIMARIES_ADOBERGB, WP_D65, 0.64, 0.33, 0.21, 0.71, 0.15,
      0.06},
  {GST_VIDEO_COLOR_PRIMARIES_SMPTEST428, WP_CENTRE, 1.0, 0.0, 0.0, 1.0, 0.0,
      0.0},
  {GST_VIDEO_COLOR_PRIMARIES_SMPTERP431, WP_WHITE, 0.68, 0.32, 0.265, 0.69,
      0.15, 0.06},
  {GST_VIDEO_COLOR_PRIMARIES_SMPTEEG432, WP_D65, 0.68, 0.32, 0.265, 0.69, 0.15,
      0.06},
  {GST_VIDEO_COLOR_PRIMARIES_EBU3213, WP_D65, 0.63, 0.34, 0.295, 0.605, 0.155,
      0.077},
};

/**
 * gst_video_color_primaries_get_info:
 * @primaries: a #GstVideoColorPrimaries
 *
 * Get information about the chromaticity coordinates of @primaries.
 *
 * Returns: a #GstVideoColorPrimariesInfo for @primaries.
 *
 * Since: 1.6
 */
const GstVideoColorPrimariesInfo *
gst_video_color_primaries_get_info (GstVideoColorPrimaries primaries)
{
  g_return_val_if_fail ((gint) primaries <
      G_N_ELEMENTS (color_primaries), NULL);

  return &color_primaries[primaries];
}

/**
 * gst_video_color_primaries_is_equivalent:
 * @primaries: a #GstVideoColorPrimaries
 * @other: another #GstVideoColorPrimaries
 *
 * Checks whether @primaries and @other are functionally equivalent
 *
 * Returns: TRUE if @primaries and @other can be considered equivalent.
 *
 * Since: 1.22
 */
gboolean
gst_video_color_primaries_is_equivalent (GstVideoColorPrimaries primaries,
    GstVideoColorPrimaries other)
{
  if (primaries == other)
    return TRUE;

  /* smpte-170m and 240m use the same reference RGB primaries and white point */
  if ((primaries == GST_VIDEO_COLOR_PRIMARIES_SMPTE170M ||
          primaries == GST_VIDEO_COLOR_PRIMARIES_SMPTE240M) &&
      (other == GST_VIDEO_COLOR_PRIMARIES_SMPTE170M ||
          other == GST_VIDEO_COLOR_PRIMARIES_SMPTE240M)) {
    return TRUE;
  }

  return FALSE;
}

/**
 * gst_video_color_matrix_get_Kr_Kb:
 * @matrix: a #GstVideoColorMatrix
 * @Kr: (out): result red channel coefficient
 * @Kb: (out): result blue channel coefficient
 *
 * Get the coefficients used to convert between Y'PbPr and R'G'B' using @matrix.
 *
 * When:
 *
 * |[
 *   0.0 <= [Y',R',G',B'] <= 1.0)
 *   (-0.5 <= [Pb,Pr] <= 0.5)
 * ]|
 *
 * the general conversion is given by:
 *
 * |[
 *   Y' = Kr*R' + (1-Kr-Kb)*G' + Kb*B'
 *   Pb = (B'-Y')/(2*(1-Kb))
 *   Pr = (R'-Y')/(2*(1-Kr))
 * ]|
 *
 * and the other way around:
 *
 * |[
 *   R' = Y' + Cr*2*(1-Kr)
 *   G' = Y' - Cb*2*(1-Kb)*Kb/(1-Kr-Kb) - Cr*2*(1-Kr)*Kr/(1-Kr-Kb)
 *   B' = Y' + Cb*2*(1-Kb)
 * ]|
 *
 * Returns: TRUE if @matrix was a YUV color format and @Kr and @Kb contain valid
 *    values.
 *
 * Since: 1.6
 */
gboolean
gst_video_color_matrix_get_Kr_Kb (GstVideoColorMatrix matrix, gdouble * Kr,
    gdouble * Kb)
{
  gboolean res = TRUE;

  switch (matrix) {
      /* RGB */
    default:
    case GST_VIDEO_COLOR_MATRIX_RGB:
      res = FALSE;
      break;
      /* YUV */
    case GST_VIDEO_COLOR_MATRIX_FCC:
      *Kr = 0.30;
      *Kb = 0.11;
      break;
    case GST_VIDEO_COLOR_MATRIX_BT709:
      *Kr = 0.2126;
      *Kb = 0.0722;
      break;
    case GST_VIDEO_COLOR_MATRIX_BT601:
      *Kr = 0.2990;
      *Kb = 0.1140;
      break;
    case GST_VIDEO_COLOR_MATRIX_SMPTE240M:
      *Kr = 0.212;
      *Kb = 0.087;
      break;
    case GST_VIDEO_COLOR_MATRIX_BT2020:
      *Kr = 0.2627;
      *Kb = 0.0593;
      break;
  }
  GST_DEBUG ("matrix: %d, Kr %f, Kb %f", matrix, *Kr, *Kb);

  return res;
}

/**
 * gst_video_color_transfer_encode:
 * @func: a #GstVideoTransferFunction
 * @val: a value
 *
 * Deprecated: 1.20: Use gst_video_transfer_function_encode() instead.
 *
 * Since: 1.6
 */
gdouble
gst_video_color_transfer_encode (GstVideoTransferFunction func, gdouble val)
{
  return gst_video_transfer_function_encode (func, val);
}

/**
 * gst_video_transfer_function_encode:
 * @func: a #GstVideoTransferFunction
 * @val: a value
 *
 * Convert @val to its gamma encoded value.
 *
 * For a linear value L in the range [0..1], conversion to the non-linear
 * (gamma encoded) L' is in general performed with a power function like:
 *
 * |[
 *    L' = L ^ (1 / gamma)
 * ]|
 *
 * Depending on @func, different formulas might be applied. Some formulas
 * encode a linear segment in the lower range.
 *
 * Returns: the gamma encoded value of @val
 *
 * Since: 1.20
 */
gdouble
gst_video_transfer_function_encode (GstVideoTransferFunction func, gdouble val)
{
  gdouble res;

  switch (func) {
    case GST_VIDEO_TRANSFER_UNKNOWN:
    case GST_VIDEO_TRANSFER_GAMMA10:
    default:
      res = val;
      break;
    case GST_VIDEO_TRANSFER_GAMMA18:
      res = pow (val, 1.0 / 1.8);
      break;
    case GST_VIDEO_TRANSFER_GAMMA20:
      res = pow (val, 1.0 / 2.0);
      break;
    case GST_VIDEO_TRANSFER_GAMMA22:
      res = pow (val, 1.0 / 2.2);
      break;
    case GST_VIDEO_TRANSFER_BT601:
    case GST_VIDEO_TRANSFER_BT709:
    case GST_VIDEO_TRANSFER_BT2020_10:
      if (val < 0.018)
        res = 4.5 * val;
      else
        res = 1.099 * pow (val, 0.45) - 0.099;
      break;
    case GST_VIDEO_TRANSFER_SMPTE240M:
      if (val < 0.0228)
        res = val * 4.0;
      else
        res = 1.1115 * pow (val, 0.45) - 0.1115;
      break;
    case GST_VIDEO_TRANSFER_SRGB:
      if (val <= 0.0031308)
        res = 12.92 * val;
      else
        res = 1.055 * pow (val, 1.0 / 2.4) - 0.055;
      break;
    case GST_VIDEO_TRANSFER_GAMMA28:
      res = pow (val, 1 / 2.8);
      break;
    case GST_VIDEO_TRANSFER_LOG100:
      if (val < 0.01)
        res = 0.0;
      else
        res = 1.0 + log10 (val) / 2.0;
      break;
    case GST_VIDEO_TRANSFER_LOG316:
      if (val < 0.0031622777)
        res = 0.0;
      else
        res = 1.0 + log10 (val) / 2.5;
      break;
    case GST_VIDEO_TRANSFER_BT2020_12:
      if (val < 0.0181)
        res = 4.5 * val;
      else
        res = 1.0993 * pow (val, 0.45) - 0.0993;
      break;
    case GST_VIDEO_TRANSFER_ADOBERGB:
      res = pow (val, 1.0 / 2.19921875);
      break;
    case GST_VIDEO_TRANSFER_SMPTE2084:
    {
      gdouble c1 = 0.8359375;
      gdouble c2 = 18.8515625;
      gdouble c3 = 18.6875;
      gdouble m1 = 0.1593017578125;
      gdouble m2 = 78.84375;
      gdouble Ln = pow (val, m1);

      /* val equal to 1 for peak white is ordinarily intended to
       * correspond to a reference output luminance level of 10000 cd/m^2  */
      res = pow ((c1 + c2 * Ln) / (1.0 + c3 * Ln), m2);
      break;
    }
    case GST_VIDEO_TRANSFER_ARIB_STD_B67:
    {
      gdouble a = 0.17883277;
      gdouble b = 0.28466892;
      gdouble c = 0.55991073;

      /* For [0, 1] normalized source as defined by HEVC specification */
      if (val > (1.0 / 12.0))
        res = a * log (12.0 * val - b) + c;
      else
        res = sqrt (3.0 * val);

      break;
    }
  }
  return res;
}

/**
 * gst_video_color_transfer_decode:
 * @func: a #GstVideoTransferFunction
 * @val: a value
 *
 * Deprecated: 1.20: Use gst_video_transfer_function_decode() instead.
 *
 * Since: 1.6
 */
gdouble
gst_video_color_transfer_decode (GstVideoTransferFunction func, gdouble val)
{
  return gst_video_transfer_function_decode (func, val);
}

/**
 * gst_video_transfer_function_decode:
 * @func: a #GstVideoTransferFunction
 * @val: a value
 *
 * Convert @val to its gamma decoded value. This is the inverse operation of
 * gst_video_color_transfer_encode().
 *
 * For a non-linear value L' in the range [0..1], conversion to the linear
 * L is in general performed with a power function like:
 *
 * |[
 *    L = L' ^ gamma
 * ]|
 *
 * Depending on @func, different formulas might be applied. Some formulas
 * encode a linear segment in the lower range.
 *
 * Returns: the gamma decoded value of @val
 *
 * Since: 1.20
 */
gdouble
gst_video_transfer_function_decode (GstVideoTransferFunction func, gdouble val)
{
  gdouble res;

  switch (func) {
    case GST_VIDEO_TRANSFER_UNKNOWN:
    case GST_VIDEO_TRANSFER_GAMMA10:
    default:
      res = val;
      break;
    case GST_VIDEO_TRANSFER_GAMMA18:
      res = pow (val, 1.8);
      break;
    case GST_VIDEO_TRANSFER_GAMMA20:
      res = pow (val, 2.0);
      break;
    case GST_VIDEO_TRANSFER_GAMMA22:
      res = pow (val, 2.2);
      break;
    case GST_VIDEO_TRANSFER_BT601:
    case GST_VIDEO_TRANSFER_BT709:
    case GST_VIDEO_TRANSFER_BT2020_10:
      if (val < 0.081)
        res = val / 4.5;
      else
        res = pow ((val + 0.099) / 1.099, 1.0 / 0.45);
      break;
    case GST_VIDEO_TRANSFER_SMPTE240M:
      if (val < 0.0913)
        res = val / 4.0;
      else
        res = pow ((val + 0.1115) / 1.1115, 1.0 / 0.45);
      break;
    case GST_VIDEO_TRANSFER_SRGB:
      if (val <= 0.04045)
        res = val / 12.92;
      else
        res = pow ((val + 0.055) / 1.055, 2.4);
      break;
    case GST_VIDEO_TRANSFER_GAMMA28:
      res = pow (val, 2.8);
      break;
    case GST_VIDEO_TRANSFER_LOG100:
      if (val == 0.0)
        res = 0.0;
      else
        res = pow (10.0, 2.0 * (val - 1.0));
      break;
    case GST_VIDEO_TRANSFER_LOG316:
      if (val == 0.0)
        res = 0.0;
      else
        res = pow (10.0, 2.5 * (val - 1.0));
      break;
    case GST_VIDEO_TRANSFER_BT2020_12:
      if (val < 0.08145)
        res = val / 4.5;
      else
        res = pow ((val + 0.0993) / 1.0993, 1.0 / 0.45);
      break;
    case GST_VIDEO_TRANSFER_ADOBERGB:
      res = pow (val, 2.19921875);
      break;
    case GST_VIDEO_TRANSFER_SMPTE2084:
    {
      gdouble c1 = 0.8359375;
      gdouble c2 = 18.8515625;
      gdouble c3 = 18.6875;
      gdouble m1 = 0.1593017578125;
      gdouble m2 = 78.84375;
      gdouble tmp = pow (val, 1 / m2);
      gdouble tmp2 = MAX (tmp - c1, 0.0f);

      res = pow (tmp2 / (c2 - c3 * tmp), 1 / m1);
      break;
    }
    case GST_VIDEO_TRANSFER_ARIB_STD_B67:
    {
      gdouble a = 0.17883277;
      gdouble b = 0.28466892;
      gdouble c = 0.55991073;

      if (val > 0.5)
        res = (exp ((val - c) / a) + b) / 12.0;
      else
        res = val * val / 3.0;

      break;
    }
  }
  return res;
}

/* conversion between GStreamer color{matrix,transfer,primaries} enum
 * and indices defined by ITU-T H.273 and ISO/IEC 230001-8 specification */

/* FIXME 2.0: Define color{matrix,transfer,primaries} with explicit numbering
 * to be matched with specification
 */

/**
 * gst_video_color_matrix_to_iso:
 * @matrix: a #GstVideoColorMatrix
 *
 * Converts #GstVideoColorMatrix to the "matrix coefficients"
 * (MatrixCoefficients) value defined by "ISO/IEC 23001-8 Section 7.3 Table 4"
 * and "ITU-T H.273 Table 4".
 * "H.264 Table E-5" and "H.265 Table E.5" share the identical values.
 *
 * Returns: The value of ISO/IEC 23001-8 matrix coefficients.
 *
 * Since: 1.18
 */
guint
gst_video_color_matrix_to_iso (GstVideoColorMatrix matrix)
{
  switch (matrix) {
    case GST_VIDEO_COLOR_MATRIX_RGB:
      return 0;
    case GST_VIDEO_COLOR_MATRIX_BT709:
      return 1;
    case GST_VIDEO_COLOR_MATRIX_FCC:
      return 4;
    case GST_VIDEO_COLOR_MATRIX_BT601:
      return 6;
    case GST_VIDEO_COLOR_MATRIX_SMPTE240M:
      return 7;
    case GST_VIDEO_COLOR_MATRIX_BT2020:
      return 9;
    case GST_VIDEO_COLOR_MATRIX_UNKNOWN:
    default:
      return 2;
  }
}

/**
 * gst_video_transfer_function_to_iso:
 * @func: a #GstVideoTransferFunction
 *
 * Converts #GstVideoTransferFunction to the "transfer characteristics"
 * (TransferCharacteristics) value defined by "ISO/IEC 23001-8 Section 7.2 Table 3"
 * and "ITU-T H.273 Table 3".
 * "H.264 Table E-4" and "H.265 Table E.4" share the identical values.
 *
 * Returns: The value of ISO/IEC 23001-8 transfer characteristics.
 *
 * Since: 1.18
 */
guint
gst_video_transfer_function_to_iso (GstVideoTransferFunction func)
{
  switch (func) {
    case GST_VIDEO_TRANSFER_BT709:
      return 1;
    case GST_VIDEO_TRANSFER_GAMMA22:
      return 4;
    case GST_VIDEO_TRANSFER_GAMMA28:
      return 5;
    case GST_VIDEO_TRANSFER_BT601:
      return 6;
    case GST_VIDEO_TRANSFER_SMPTE240M:
      return 7;
    case GST_VIDEO_TRANSFER_GAMMA10:
      return 8;
    case GST_VIDEO_TRANSFER_LOG100:
      return 9;
    case GST_VIDEO_TRANSFER_LOG316:
      return 10;
    case GST_VIDEO_TRANSFER_SRGB:
      return 13;
    case GST_VIDEO_TRANSFER_BT2020_10:
      return 14;
    case GST_VIDEO_TRANSFER_BT2020_12:
      return 15;
    case GST_VIDEO_TRANSFER_SMPTE2084:
      return 16;
    case GST_VIDEO_TRANSFER_ARIB_STD_B67:
      return 18;
    case GST_VIDEO_TRANSFER_GAMMA18:
    case GST_VIDEO_TRANSFER_GAMMA20:
    case GST_VIDEO_TRANSFER_ADOBERGB:
    case GST_VIDEO_TRANSFER_UNKNOWN:
    default:
      return 2;
  }
}

/**
 * gst_video_color_primaries_to_iso:
 * @primaries: a #GstVideoColorPrimaries
 *
 * Converts #GstVideoColorPrimaries to the "colour primaries" (ColourPrimaries)
 * value defined by "ISO/IEC 23001-8 Section 7.1 Table 2"
 * and "ITU-T H.273 Table 2".
 * "H.264 Table E-3" and "H.265 Table E.3" share the identical values.
 *
 * Returns: The value of ISO/IEC 23001-8 colour primaries.
 *
 * Since: 1.18
 */
guint
gst_video_color_primaries_to_iso (GstVideoColorPrimaries primaries)
{
  switch (primaries) {
    case GST_VIDEO_COLOR_PRIMARIES_BT709:
      return 1;
    case GST_VIDEO_COLOR_PRIMARIES_BT470M:
      return 4;
    case GST_VIDEO_COLOR_PRIMARIES_BT470BG:
      return 5;
    case GST_VIDEO_COLOR_PRIMARIES_SMPTE170M:
      return 6;
    case GST_VIDEO_COLOR_PRIMARIES_SMPTE240M:
      return 7;
    case GST_VIDEO_COLOR_PRIMARIES_FILM:
      return 8;
    case GST_VIDEO_COLOR_PRIMARIES_BT2020:
      return 9;
    case GST_VIDEO_COLOR_PRIMARIES_SMPTEST428:
      return 10;
    case GST_VIDEO_COLOR_PRIMARIES_SMPTERP431:
      return 11;
    case GST_VIDEO_COLOR_PRIMARIES_SMPTEEG432:
      return 12;
    case GST_VIDEO_COLOR_PRIMARIES_EBU3213:
      return 22;
    case GST_VIDEO_COLOR_PRIMARIES_ADOBERGB:
    case GST_VIDEO_COLOR_PRIMARIES_UNKNOWN:
    default:
      return 2;
  }
}

/**
 * gst_video_color_matrix_from_iso:
 * @value: a ITU-T H.273 matrix coefficients value
 *
 * Converts the @value to the #GstVideoColorMatrix
 * The matrix coefficients (MatrixCoefficients) value is
 * defined by "ISO/IEC 23001-8 Section 7.3 Table 4"
 * and "ITU-T H.273 Table 4".
 * "H.264 Table E-5" and "H.265 Table E.5" share the identical values.
 *
 * Returns: the matched #GstVideoColorMatrix
 *
 * Since: 1.18
 */
GstVideoColorMatrix
gst_video_color_matrix_from_iso (guint value)
{
  switch (value) {
    case 0:
      return GST_VIDEO_COLOR_MATRIX_RGB;
    case 1:
      return GST_VIDEO_COLOR_MATRIX_BT709;
    case 4:
      return GST_VIDEO_COLOR_MATRIX_FCC;
    case 5:
    case 6:
      return GST_VIDEO_COLOR_MATRIX_BT601;
    case 7:
      return GST_VIDEO_COLOR_MATRIX_SMPTE240M;
    case 9:
      return GST_VIDEO_COLOR_MATRIX_BT2020;
    case 2:
    default:
      return GST_VIDEO_COLOR_MATRIX_UNKNOWN;
  }
}

/**
 * gst_video_transfer_function_from_iso:
 * @value: a ITU-T H.273 transfer characteristics value
 *
 * Converts the @value to the #GstVideoTransferFunction
 * The transfer characteristics (TransferCharacteristics) value is
 * defined by "ISO/IEC 23001-8 Section 7.2 Table 3"
 * and "ITU-T H.273 Table 3".
 * "H.264 Table E-4" and "H.265 Table E.4" share the identical values.
 *
 * Returns: the matched #GstVideoTransferFunction
 *
 * Since: 1.18
 */
GstVideoTransferFunction
gst_video_transfer_function_from_iso (guint value)
{
  switch (value) {
    case 1:
      return GST_VIDEO_TRANSFER_BT709;
    case 4:
      return GST_VIDEO_TRANSFER_GAMMA22;
    case 5:
      return GST_VIDEO_TRANSFER_GAMMA28;
    case 6:
      return GST_VIDEO_TRANSFER_BT601;
    case 7:
      return GST_VIDEO_TRANSFER_SMPTE240M;
    case 8:
      return GST_VIDEO_TRANSFER_GAMMA10;
    case 9:
      return GST_VIDEO_TRANSFER_LOG100;
    case 10:
      return GST_VIDEO_TRANSFER_LOG316;
    case 13:
      return GST_VIDEO_TRANSFER_SRGB;
    case 14:
      return GST_VIDEO_TRANSFER_BT2020_10;
    case 15:
      return GST_VIDEO_TRANSFER_BT2020_12;
    case 16:
      return GST_VIDEO_TRANSFER_SMPTE2084;
    case 18:
      return GST_VIDEO_TRANSFER_ARIB_STD_B67;
    case 2:
    default:
      return GST_VIDEO_TRANSFER_UNKNOWN;
  }
}

/**
 * gst_video_color_primaries_from_iso:
 * @value: a ITU-T H.273 colour primaries value
 *
 * Converts the @value to the #GstVideoColorPrimaries
 * The colour primaries (ColourPrimaries) value is
 * defined by "ISO/IEC 23001-8 Section 7.1 Table 2" and "ITU-T H.273 Table 2".
 * "H.264 Table E-3" and "H.265 Table E.3" share the identical values.
 *
 * Returns: the matched #GstVideoColorPrimaries
 *
 * Since: 1.18
 */
GstVideoColorPrimaries
gst_video_color_primaries_from_iso (guint value)
{
  switch (value) {
    case 1:
      return GST_VIDEO_COLOR_PRIMARIES_BT709;
    case 4:
      return GST_VIDEO_COLOR_PRIMARIES_BT470M;
    case 5:
      return GST_VIDEO_COLOR_PRIMARIES_BT470BG;
    case 6:
      return GST_VIDEO_COLOR_PRIMARIES_SMPTE170M;
    case 7:
      return GST_VIDEO_COLOR_PRIMARIES_SMPTE240M;
    case 8:
      return GST_VIDEO_COLOR_PRIMARIES_FILM;
    case 9:
      return GST_VIDEO_COLOR_PRIMARIES_BT2020;
    case 10:
      return GST_VIDEO_COLOR_PRIMARIES_SMPTEST428;
    case 11:
      return GST_VIDEO_COLOR_PRIMARIES_SMPTERP431;
    case 12:
      return GST_VIDEO_COLOR_PRIMARIES_SMPTEEG432;
    case 22:
      return GST_VIDEO_COLOR_PRIMARIES_EBU3213;
    case 2:
    default:
      return GST_VIDEO_COLOR_PRIMARIES_UNKNOWN;
  }
}

static GstVideoTransferFunction
map_equivalent_transfer (GstVideoTransferFunction func, guint bpp)
{
  switch (func) {
    case GST_VIDEO_TRANSFER_BT2020_12:
      if (bpp >= 12)
        break;
      /* fallthrough */
    case GST_VIDEO_TRANSFER_BT709:
    case GST_VIDEO_TRANSFER_BT601:
    case GST_VIDEO_TRANSFER_BT2020_10:
      return GST_VIDEO_TRANSFER_BT709;
    default:
      break;
  }

  return func;
}

/**
 * gst_video_transfer_function_is_equivalent:
 * @from_func: #GstVideoTransferFunction to convert from
 * @from_bpp: bits per pixel to convert from
 * @to_func: #GstVideoTransferFunction to convert into
 * @to_bpp: bits per pixel to convert into
 *
 * Returns whether @from_func and @to_func are equivalent. There are cases
 * (e.g. BT601, BT709, and BT2020_10) where several functions are functionally
 * identical. In these cases, when doing conversion, we should consider them
 * as equivalent. Also, BT2020_12 is the same as the aforementioned three for
 * less than 12 bits per pixel.
 *
 * Returns: TRUE if @from_func and @to_func can be considered equivalent.
 *
 * Since: 1.18
 */
gboolean
gst_video_transfer_function_is_equivalent (GstVideoTransferFunction from_func,
    guint from_bpp, GstVideoTransferFunction to_func, guint to_bpp)
{
  from_func = map_equivalent_transfer (from_func, from_bpp);
  to_func = map_equivalent_transfer (to_func, to_bpp);
  if (from_func == GST_VIDEO_TRANSFER_BT2020_12 && to_bpp < 12 &&
      to_func == GST_VIDEO_TRANSFER_BT709)
    return TRUE;
  return from_func == to_func;
}
