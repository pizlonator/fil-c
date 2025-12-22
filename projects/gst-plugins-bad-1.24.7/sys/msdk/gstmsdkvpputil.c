/*
 *  gstmsdkvpputil.c - MediaSDK video post processing utilities
 *
 *  Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 *  Copyright (C) 2005-2012 David Schleef <ds@schleef.org>
 *  Copyright (C) 2016 Intel Corporation
 *  Copyright (C) 2018 Intel Corporation
 *    Author: Gwenole Beauchesne <gwenole.beauchesne@intel.com>
 *    Author: Victor Jaquez <victorx.jaquez@intel.com>
 *    Author: Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#include "gstmsdkvpputil.h"
#include "msdk-enums.h"
#include "gstmsdkcaps.h"

#ifndef _WIN32
#include <libdrm/drm_fourcc.h>
#include <gst/va/gstvavideoformat.h>
#else
#include <gst/d3d11/gstd3d11.h>
#endif

#define SWAP_GINT(a, b) do {      \
        const gint t = a; a = b; b = t; \
    } while (0)

gboolean
gst_msdkvpp_is_deinterlace_enabled (GstMsdkVPP * msdkvpp, GstVideoInfo * vip)
{
  gboolean deinterlace;

  switch (msdkvpp->deinterlace_mode) {
    case GST_MSDKVPP_DEINTERLACE_MODE_AUTO:
      deinterlace = GST_VIDEO_INFO_IS_INTERLACED (vip);
      break;
    case GST_MSDKVPP_DEINTERLACE_MODE_INTERLACED:
      deinterlace = TRUE;
      break;
    default:
      deinterlace = FALSE;
      break;
  }
  return deinterlace;
}

/* This help function is to fixate output format for vpp src caps,
 * when downstream element does not set a specific format, i.e. queried
 * caps contains a list of formats, vpp src caps will loop the format
 * list and find one format that matches the vpp sink caps format (in vinfo);
 * if downstream has set a specific format, we use this format for vpp src caps.
 */
static GstCaps *
fixate_output_format (GstMsdkVPP * thiz, GstVideoInfo * vinfo, GstCaps * caps)
{
  GstVideoFormat fmt = GST_VIDEO_FORMAT_UNKNOWN;
  guint i, size, fixated_idx = 0;
  GstStructure *s, *out = NULL;
  GstCapsFeatures *features;
  GstCaps *ret;
  const GValue *format;
  gboolean is_va = FALSE, is_dma = FALSE, is_d3d = FALSE;
  gboolean fixate = FALSE;
#ifndef _WIN32
  guint64 modifier = DRM_FORMAT_MOD_INVALID;
  guint32 fourcc;
#endif

  if (!caps)
    return NULL;
  size = gst_caps_get_size (caps);

  for (i = 0; i < size; i++) {
    s = gst_caps_get_structure (caps, i);
    features = gst_caps_get_features (caps, i);
    is_va = is_dma = is_d3d = FALSE;

    if (gst_caps_features_contains (features, GST_CAPS_FEATURE_MEMORY_DMABUF)) {
      format = gst_structure_get_value (s, "drm-format");
      is_dma = TRUE;
    } else {
      format = gst_structure_get_value (s, "format");
#ifndef _WIN32
      if (gst_caps_features_contains (features, GST_CAPS_FEATURE_MEMORY_VA))
        is_va = TRUE;
#else
      if (gst_caps_features_contains (features,
              GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY))
        is_d3d = TRUE;
#endif
    }

    if (format == NULL)
      continue;

    if (GST_VALUE_HOLDS_LIST (format)) {
      gint j, len;

      len = gst_value_list_get_size (format);
      GST_DEBUG_OBJECT (thiz, "have %d formats", len);
      for (j = 0; j < len; j++) {
        const GValue *val;

        val = gst_value_list_get_value (format, j);
        if (G_VALUE_HOLDS_STRING (val)) {
#ifndef _WIN32
          if (is_dma) {
            fourcc = gst_video_dma_drm_fourcc_from_string
                (g_value_get_string (val), &modifier);
            fmt = gst_va_video_format_from_drm_fourcc (fourcc);
          } else {
            fmt = gst_video_format_from_string (g_value_get_string (val));
          }
#else
          fmt = gst_video_format_from_string (g_value_get_string (val));
#endif
          if (fmt == GST_VIDEO_FORMAT_UNKNOWN)
            continue;
          if (fmt == GST_VIDEO_INFO_FORMAT (vinfo)) {
            fixate = TRUE;
            fixated_idx = i;
            break;
          }
        }
      }
    } else if (G_VALUE_HOLDS_STRING (format)) {
#ifndef _WIN32
      if (is_dma) {
        fourcc = gst_video_dma_drm_fourcc_from_string
            (g_value_get_string (format), &modifier);
        fmt = gst_va_video_format_from_drm_fourcc (fourcc);
      } else {
        fmt = gst_video_format_from_string (g_value_get_string (format));
      }
#else
      fmt = gst_video_format_from_string (g_value_get_string (format));
#endif
      if (fmt == GST_VIDEO_FORMAT_UNKNOWN)
        continue;

      /* size > 1 means downstream doesn't specify the caps directly, so we
       * still need to compare fmt with the one in vpp sinkcaps */
      if (size > 1 && fmt != GST_VIDEO_INFO_FORMAT (vinfo))
        continue;

      fixate = TRUE;
      break;
    }
    if (fixate)
      break;
  }

  if (!fixate)
    fmt = GST_VIDEO_FORMAT_NV12;

  out = gst_structure_copy (gst_caps_get_structure (caps, fixated_idx));
  features = gst_caps_features_copy (gst_caps_get_features (caps, fixated_idx));

#ifndef _WIN32
  if (is_dma) {
    gchar *drm_fmt_name;

    g_assert (modifier != DRM_FORMAT_MOD_INVALID);

    drm_fmt_name = gst_video_dma_drm_fourcc_to_string
        (gst_va_drm_fourcc_from_video_format (fmt), modifier);

    gst_structure_set (out, "drm-format", G_TYPE_STRING, drm_fmt_name, NULL);
    g_free (drm_fmt_name);
  } else {
    gst_structure_set (out, "format", G_TYPE_STRING,
        gst_video_format_to_string (fmt), NULL);
  }
#else
  gst_structure_set (out, "format", G_TYPE_STRING,
      gst_video_format_to_string (fmt), NULL);
#endif

  ret = gst_caps_new_full (out, NULL);
  gst_caps_set_features_simple (ret, features);
#ifndef _WIN32
  if (is_va)
    gst_caps_set_features (ret, 0,
        gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_VA));
#else
  if (is_d3d)
    gst_caps_set_features (ret, 0,
        gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY));
#endif
  else if (is_dma)
    gst_caps_set_features (ret, 0,
        gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_DMABUF));

  return ret;
}

static gboolean
fixate_output_frame_size (GstMsdkVPP * thiz, GstVideoInfo * vinfo,
    GstStructure * outs)
{
  const GValue *to_par;
  GValue tpar = G_VALUE_INIT;
  gboolean ret;

  ret = TRUE;
  to_par = gst_structure_get_value (outs, "pixel-aspect-ratio");
  if (!to_par) {
    g_value_init (&tpar, GST_TYPE_FRACTION_RANGE);
    gst_value_set_fraction_range_full (&tpar, 1, G_MAXINT, G_MAXINT, 1);
    to_par = &tpar;
  }

  /* we have both PAR but they might not be fixated */
  {
    gint from_w, from_h, from_par_n, from_par_d, to_par_n, to_par_d;
    gint w = 0, h = 0;
    gint from_dar_n, from_dar_d;
    gint num, den;

    from_par_n = GST_VIDEO_INFO_PAR_N (vinfo);
    from_par_d = GST_VIDEO_INFO_PAR_D (vinfo);
    from_w = GST_VIDEO_INFO_WIDTH (vinfo);
    from_h = GST_VIDEO_INFO_HEIGHT (vinfo);

    /* adjust for crop settings (NOTE: msdk min frame size is 2x2) */
    if ((thiz->crop_left + thiz->crop_right >= from_w - 1)
        || (thiz->crop_top + thiz->crop_bottom >= from_h - 1)) {
      GST_WARNING_OBJECT (thiz, "ignoring crop... cropping too much!");
    } else {
      from_w -= thiz->crop_left + thiz->crop_right;
      from_h -= thiz->crop_top + thiz->crop_bottom;
    }

    /* compensate for rotation if needed */
    if (thiz->rotation == 90 || thiz->rotation == 270) {
      SWAP_GINT (from_w, from_h);
      SWAP_GINT (from_par_n, from_par_d);
    }

    gst_structure_get_int (outs, "width", &w);
    gst_structure_get_int (outs, "height", &h);

    /* if both width and height are already fixed, we can't do anything
     * about it anymore */
    if (w && h) {
      guint n, d;

      GST_DEBUG_OBJECT (thiz,
          "dimensions already set to %dx%d, not fixating", w, h);

      if (!gst_value_is_fixed (to_par)) {
        if (gst_video_calculate_display_ratio (&n, &d, from_w, from_h,
                from_par_n, from_par_d, w, h)) {
          GST_DEBUG_OBJECT (thiz, "fixating to_par to %dx%d", n, d);
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
            &from_dar_n, &from_dar_d))
      goto overflow_error;

    GST_DEBUG_OBJECT (thiz, "Input DAR is %d/%d", from_dar_n, from_dar_d);

    /* If either width or height are fixed there's not much we
     * can do either except choosing a height or width and PAR
     * that matches the DAR as good as possible
     */
    if (h) {
      GstStructure *tmp;
      gint set_w, set_par_n, set_par_d;

      GST_DEBUG_OBJECT (thiz, "height is fixed (%d)", h);

      /* If the PAR is fixed too, there's not much to do
       * except choosing the width that is nearest to the
       * width with the same DAR */
      if (gst_value_is_fixed (to_par)) {
        to_par_n = gst_value_get_fraction_numerator (to_par);
        to_par_d = gst_value_get_fraction_denominator (to_par);

        GST_DEBUG_OBJECT (thiz, "PAR is fixed %d/%d", to_par_n, to_par_d);

        if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, to_par_d,
                to_par_n, &num, &den))
          goto overflow_error;

        w = (guint) gst_util_uint64_scale_int (h, num, den);
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
        gst_structure_free (tmp);
        goto overflow_error;
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
              set_par_n, &num, &den))
        goto overflow_error;

      w = (guint) gst_util_uint64_scale_int (h, num, den);
      gst_structure_fixate_field_nearest_int (outs, "width", w);
      if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
          set_par_n != set_par_d)
        gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
            set_par_n, set_par_d, NULL);

      goto done;
    } else if (w) {
      GstStructure *tmp;
      gint set_h, set_par_n, set_par_d;

      GST_DEBUG_OBJECT (thiz, "width is fixed (%d)", w);

      /* If the PAR is fixed too, there's not much to do
       * except choosing the height that is nearest to the
       * height with the same DAR */
      if (gst_value_is_fixed (to_par)) {
        to_par_n = gst_value_get_fraction_numerator (to_par);
        to_par_d = gst_value_get_fraction_denominator (to_par);

        GST_DEBUG_OBJECT (thiz, "PAR is fixed %d/%d", to_par_n, to_par_d);

        if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, to_par_d,
                to_par_n, &num, &den))
          goto overflow_error;

        h = (guint) gst_util_uint64_scale_int (w, den, num);
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
        gst_structure_free (tmp);
        goto overflow_error;
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
              set_par_n, &num, &den))
        goto overflow_error;

      h = (guint) gst_util_uint64_scale_int (w, den, num);
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
              to_par_d, &num, &den))
        goto overflow_error;

      /* Try to keep the input height (because of interlacing) */
      tmp = gst_structure_copy (outs);
      gst_structure_fixate_field_nearest_int (tmp, "height", from_h);
      gst_structure_get_int (tmp, "height", &set_h);

      /* This might have failed but try to scale the width
       * to keep the DAR nonetheless */
      w = (guint) gst_util_uint64_scale_int (set_h, num, den);
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
      h = (guint) gst_util_uint64_scale_int (set_w, den, num);
      gst_structure_fixate_field_nearest_int (tmp, "height", h);
      gst_structure_get_int (tmp, "height", &set_h);
      gst_structure_free (tmp);

      /* We kept the DAR and the width is nearest to the original width */
      if (set_h == h) {
        gst_structure_set (outs, "width", G_TYPE_INT, set_w, "height",
            G_TYPE_INT, set_h, NULL);
        goto done;
      }

      /* If all this failed, keep the height that was nearest to the original
       * height and the nearest possible width. This changes the DAR but
       * there's not much else to do here.
       */
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
        gst_structure_free (tmp);
        goto overflow_error;
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
              set_par_n, &num, &den))
        goto overflow_error;

      w = (guint) gst_util_uint64_scale_int (set_h, num, den);
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
      h = (guint) gst_util_uint64_scale_int (set_w, den, num);
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
  if (to_par == &tpar)
    g_value_unset (&tpar);

  return ret;

  /* ERRORS */
overflow_error:
  {
    ret = FALSE;
    GST_ELEMENT_ERROR (thiz, CORE, NEGOTIATION, (NULL),
        ("Error calculating the output scaled size - integer overflow"));
    goto done;
  }
}

static gboolean
fixate_frame_rate (GstMsdkVPP * thiz, GstVideoInfo * vinfo, GstStructure * outs)
{
  gint fps_n = 0, fps_d;

  /* fixate the srcpad fps */
  if (gst_structure_fixate_field (outs, "framerate"))
    gst_structure_get (outs, "framerate", GST_TYPE_FRACTION, &fps_n, &fps_d,
        NULL);

  /* if we don't have a fixed non-zero fps_n, use the sinkpad fps */
  if (!fps_n) {
    fps_n = GST_VIDEO_INFO_FPS_N (vinfo);
    fps_d = GST_VIDEO_INFO_FPS_D (vinfo);
  }

  if (gst_msdkvpp_is_deinterlace_enabled (thiz, vinfo)) {
    /* Fixme: set double framerate?:
     * msdk is not outputting double framerate for bob or adv deinterlace */
    if (!gst_util_fraction_multiply (fps_n, fps_d, 1, 1, &fps_n, &fps_d))
      goto overflow_error;
  }
  gst_structure_set (outs, "framerate", GST_TYPE_FRACTION, fps_n, fps_d, NULL);
  return TRUE;

  /* ERRORS */
overflow_error:
  {
    GST_ELEMENT_ERROR (thiz, CORE, NEGOTIATION, (NULL),
        ("Error calculating the output framerate - integer overflow"));
    return FALSE;
  }
}

static gboolean
set_multiview_mode (GstMsdkVPP * thiz, GstVideoInfo * vinfo,
    GstStructure * outs)
{
  const gchar *caps_str;

  caps_str =
      gst_video_multiview_mode_to_caps_string (GST_VIDEO_INFO_MULTIVIEW_MODE
      (vinfo));
  if (!caps_str)
    return TRUE;

  gst_structure_set (outs, "multiview-mode", G_TYPE_STRING, caps_str,
      "multiview-flags", GST_TYPE_VIDEO_MULTIVIEW_FLAGSET,
      GST_VIDEO_INFO_MULTIVIEW_FLAGS (vinfo), GST_FLAG_SET_MASK_EXACT, NULL);

  if (GST_VIDEO_INFO_VIEWS (vinfo) > 1) {
    gst_structure_set (outs, "views", G_TYPE_INT, GST_VIDEO_INFO_VIEWS (vinfo),
        NULL);
  }
  return TRUE;
}

static gboolean
set_interlace_mode (GstMsdkVPP * thiz, GstVideoInfo * vinfo,
    GstStructure * outs)
{
  const gchar *interlace_mode = NULL;

  if (gst_msdkvpp_is_deinterlace_enabled (thiz, vinfo)) {
    interlace_mode = "progressive";
  } else {
    interlace_mode =
        gst_video_interlace_mode_to_string (GST_VIDEO_INFO_INTERLACE_MODE
        (vinfo));
  }

  if (!interlace_mode)
    return FALSE;

  gst_structure_set (outs, "interlace-mode", G_TYPE_STRING, interlace_mode,
      NULL);
  return TRUE;
}

static GstCaps *
_get_preferred_src_caps (GstMsdkVPP * thiz, GstVideoInfo * vinfo,
    GstCaps * srccaps)
{
  GstStructure *structure = NULL;
  GstCaps *outcaps, *fixate_caps;
  GstCapsFeatures *features;

  /* Fixate the format */
  fixate_caps = fixate_output_format (thiz, vinfo, srccaps);
  if (!fixate_caps)
    goto fixate_failed;

  structure = gst_caps_get_structure (fixate_caps, 0);
  features = gst_caps_get_features (fixate_caps, 0);
  /* make a copy */
  structure = gst_structure_copy (structure);
  features = gst_caps_features_copy (features);

  gst_caps_unref (fixate_caps);

  if (thiz->keep_aspect)
    gst_structure_set (structure, "pixel-aspect-ratio", GST_TYPE_FRACTION, 1,
        1, NULL);

  /* Fixate the frame size */
  if (!fixate_output_frame_size (thiz, vinfo, structure))
    goto fixate_failed;

  /* Fixate the framerate */
  if (!fixate_frame_rate (thiz, vinfo, structure))
    goto fixate_failed;

  /* set multiview mode based on input caps */
  if (!set_multiview_mode (thiz, vinfo, structure))
    goto fixate_failed;

  /*Fixme: Set colorimetry */

  /* set interlace mode */
  if (!set_interlace_mode (thiz, vinfo, structure))
    goto interlace_mode_failed;

  outcaps = gst_caps_new_empty ();
  gst_caps_append_structure (outcaps, structure);
  gst_caps_set_features (outcaps, 0, features);

  return outcaps;

  /* ERRORS */
fixate_failed:
  {
    GST_WARNING_OBJECT (thiz, "Could not fixate src caps");
    if (structure)
      gst_structure_free (structure);
    return NULL;
  }
interlace_mode_failed:
  {
    GST_WARNING_OBJECT (thiz, "Invalid sink caps interlace mode");
    return NULL;
  }
}

/**
 * gst_msdkvpp_fixate_srccaps:
 * @vpp: a #GstMsdkVPP instance
 * @sinkcaps: fixed #GstCaps from sink pad
 * @srccaps: #GstCaps from src pad to fixate
 *
 * Given @srccaps and @sinkcaps returns a new allocated #GstCaps with
 * the fixated caps for the src pad.
 *
 * Returns: A new allocated #GstCaps
 **/
GstCaps *
gst_msdkvpp_fixate_srccaps (GstMsdkVPP * msdkvpp,
    GstCaps * sinkcaps, GstCaps * srccaps)
{
  GstVideoInfo vi;

  if (!gst_msdkcaps_video_info_from_caps (sinkcaps, &vi, NULL))
    return FALSE;

  return _get_preferred_src_caps (msdkvpp, &vi, srccaps);
}
