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

#include "video.h"
#include "gstvideometa.h"

/**
 * SECTION:gstvideo
 * @title: GstVideo
 * @short_description: Support library for video operations
 *
 * This library contains some helper functions and includes the
 * videosink and videofilter base classes.
 */

/**
 * gst_video_calculate_display_ratio:
 * @dar_n: (out): Numerator of the calculated display_ratio
 * @dar_d: (out): Denominator of the calculated display_ratio
 * @video_width: Width of the video frame in pixels
 * @video_height: Height of the video frame in pixels
 * @video_par_n: Numerator of the pixel aspect ratio of the input video.
 * @video_par_d: Denominator of the pixel aspect ratio of the input video.
 * @display_par_n: Numerator of the pixel aspect ratio of the display device
 * @display_par_d: Denominator of the pixel aspect ratio of the display device
 *
 * Given the Pixel Aspect Ratio and size of an input video frame, and the
 * pixel aspect ratio of the intended display device, calculates the actual
 * display ratio the video will be rendered with.
 *
 * Returns: A boolean indicating success and a calculated Display Ratio in the
 * dar_n and dar_d parameters.
 * The return value is FALSE in the case of integer overflow or other error.
 */
gboolean
gst_video_calculate_display_ratio (guint * dar_n, guint * dar_d,
    guint video_width, guint video_height,
    guint video_par_n, guint video_par_d,
    guint display_par_n, guint display_par_d)
{
  gint num, den;
  gint tmp_n, tmp_d;

  g_return_val_if_fail (dar_n != NULL, FALSE);
  g_return_val_if_fail (dar_d != NULL, FALSE);

  /* Calculate (video_width * video_par_n * display_par_d) /
   * (video_height * video_par_d * display_par_n) */
  if (!gst_util_fraction_multiply (video_width, video_height, video_par_n,
          video_par_d, &tmp_n, &tmp_d))
    goto error_overflow;

  if (!gst_util_fraction_multiply (tmp_n, tmp_d, display_par_d, display_par_n,
          &num, &den))
    goto error_overflow;

  g_return_val_if_fail (num > 0, FALSE);
  g_return_val_if_fail (den > 0, FALSE);

  *dar_n = num;
  *dar_d = den;

  return TRUE;

  /* ERRORS */
error_overflow:
  {
    GST_WARNING ("overflow in multiply");
    return FALSE;
  }
}

/**
 * gst_video_guess_framerate:
 * @duration: Nominal duration of one frame
 * @dest_n: (out) (allow-none): Numerator of the calculated framerate
 * @dest_d: (out) (allow-none): Denominator of the calculated framerate
 *
 * Given the nominal duration of one video frame,
 * this function will check some standard framerates for
 * a close match (within 0.1%) and return one if possible,
 *
 * It will calculate an arbitrary framerate if no close
 * match was found, and return %FALSE.
 *
 * It returns %FALSE if a duration of 0 is passed.
 *
 * Returns: %TRUE if a close "standard" framerate was
 * recognised, and %FALSE otherwise.
 *
 * Since: 1.6
 */
gboolean
gst_video_guess_framerate (GstClockTime duration, gint * dest_n, gint * dest_d)
{
  const int common_den[] = { 1, 2, 3, 4, 1001 };
  int best_n, best_d, gcd;
  guint64 best_error = G_MAXUINT64;
  guint64 a;
  int i;

  if (G_UNLIKELY (duration == 0))
    return FALSE;

  /* Use a limited precision conversion by default for more sensible results,
   * unless the frame duration is absurdly small (high speed cameras?) */
  if (duration > 100000) {
    best_n = GST_SECOND / 10000;
    best_d = duration / 10000;
  } else {
    best_n = GST_SECOND;
    best_d = duration;
  }

  for (i = 0; i < G_N_ELEMENTS (common_den); i++) {
    gint d = common_den[i];
    gint n = gst_util_uint64_scale_round (d, GST_SECOND, duration);

    /* For NTSC framerates, round to the nearest 1000 fps */
    if (d == 1001) {
      n += 500;
      n -= (n % 1000);
    }

    if (n > 0) {
      /* See what duration the given framerate should be */
      a = gst_util_uint64_scale_int (GST_SECOND, d, n);
      /* Compute absolute error */
      a = (a < duration) ? (duration - a) : (a - duration);
      if (a < 2) {
        /* Really precise - take this option */
        if (dest_n)
          *dest_n = n;
        if (dest_d)
          *dest_d = d;
        return TRUE;
      }
      /* If within 0.1%, remember this denominator */
      if (a * 1000 < duration && a < best_error) {
        best_error = a;
        best_n = n;
        best_d = d;
      }
    }
  }

  /* set results */
  gcd = gst_util_greatest_common_divisor (best_n, best_d);
  if (gcd) {
    best_n /= gcd;
    best_d /= gcd;
  }
  if (dest_n)
    *dest_n = best_n;
  if (dest_d)
    *dest_d = best_d;

  return (best_error != G_MAXUINT64);
}

/**
 * gst_video_is_common_aspect_ratio:
 * @width: Width of the video frame
 * @height: Height of the video frame
 * @par_n: Pixel aspect ratio numerator
 * @par_d: Pixel aspect ratio denominator
 *
 * Given a frame's dimensions and pixel aspect ratio, this function will
 * calculate the frame's aspect ratio and compare it against a set of
 * common well-known "standard" aspect ratios.
 *
 * Returns: %TRUE if a known "standard" aspect ratio was
 * recognised, and %FALSE otherwise.
 *
 * Since: 1.22
 */
gboolean
gst_video_is_common_aspect_ratio (gint width, gint height, gint par_n,
    gint par_d)
{
  gint dar_n, dar_d;

  gst_util_fraction_multiply (width, height, par_n, par_d, &dar_n, &dar_d);

  if (dar_n == 16 && dar_d == 9)
    return TRUE;
  if (dar_n == 4 && dar_d == 3)
    return TRUE;
  if (dar_n == 14 && dar_d == 9)
    return TRUE;
  if (dar_n == 8 && dar_d == 5)
    return TRUE;
  if (dar_n == 21 && dar_d == 11)
    return TRUE;

  return FALSE;
}


/**
 * gst_video_alignment_reset:
 * @align: a #GstVideoAlignment
 *
 * Set @align to its default values with no padding and no alignment.
 */
void
gst_video_alignment_reset (GstVideoAlignment * align)
{
  gint i;

  g_return_if_fail (align != NULL);

  align->padding_top = 0;
  align->padding_bottom = 0;
  align->padding_left = 0;
  align->padding_right = 0;
  for (i = 0; i < GST_VIDEO_MAX_PLANES; i++)
    align->stride_align[i] = 0;
}

/**
 * gst_video_orientation_from_tag:
 * @taglist: A #GstTagList
 * @method: (out): The location where to return the orientation.
 *
 * Parses the "image-orientation" tag and transforms it into the
 * #GstVideoOrientationMethod enum.
 *
 * Returns: TRUE if there was a valid "image-orientation" tag in the taglist.
 *
 * Since: 1.20
 */
gboolean
gst_video_orientation_from_tag (GstTagList * taglist,
    GstVideoOrientationMethod * method)
{
  gchar *orientation;
  gboolean ret = TRUE;

  g_return_val_if_fail (GST_IS_TAG_LIST (taglist), FALSE);
  g_return_val_if_fail (method != NULL, FALSE);

  if (!gst_tag_list_get_string (taglist, "image-orientation", &orientation))
    return FALSE;

  if (!g_strcmp0 ("rotate-0", orientation))
    *method = GST_VIDEO_ORIENTATION_IDENTITY;
  else if (!g_strcmp0 ("rotate-90", orientation))
    *method = GST_VIDEO_ORIENTATION_90R;
  else if (!g_strcmp0 ("rotate-180", orientation))
    *method = GST_VIDEO_ORIENTATION_180;
  else if (!g_strcmp0 ("rotate-270", orientation))
    *method = GST_VIDEO_ORIENTATION_90L;
  else if (!g_strcmp0 ("flip-rotate-0", orientation))
    *method = GST_VIDEO_ORIENTATION_HORIZ;
  else if (!g_strcmp0 ("flip-rotate-90", orientation))
    *method = GST_VIDEO_ORIENTATION_UR_LL;
  else if (!g_strcmp0 ("flip-rotate-180", orientation))
    *method = GST_VIDEO_ORIENTATION_VERT;
  else if (!g_strcmp0 ("flip-rotate-270", orientation))
    *method = GST_VIDEO_ORIENTATION_UL_LR;
  else
    ret = FALSE;

  g_free (orientation);

  return ret;
}
