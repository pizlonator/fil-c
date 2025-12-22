/* GStreamer JPEG 2000 Sampling
 * Copyright (C) <2016> Grok Image Compression Inc.
 *  @author Aaron Boxer <boxerab@gmail.com>
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
 * SECTION:gstjpeg2000sampling
 * @title: GstJpeg2000Sampling
 * @short_description: Manage JPEG 2000 sampling and colorspace fields
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstjpeg2000sampling.h"

/* string values corresponding to GstJPEG2000Sampling enums */
static const gchar *gst_jpeg2000_sampling_strings[] = {
  "RGB",
  "BGR",
  "RGBA",
  "BGRA",
  "YCbCr-4:4:4",
  "YCbCr-4:2:2",
  "YCbCr-4:2:0",
  "YCbCr-4:1:0",
  "GRAYSCALE",
  "YCbCrA-4:4:4:4",
  "YCbCr-4:1:1",
};

/* convert string to GstJPEG2000Sampling enum */
GstJPEG2000Sampling
gst_jpeg2000_sampling_from_string (const gchar * sampling_string)
{
  GstJPEG2000Sampling i;
  if (sampling_string == NULL)
    return GST_JPEG2000_SAMPLING_NONE;
  for (i = 0; i < G_N_ELEMENTS (gst_jpeg2000_sampling_strings); ++i) {
    if (!g_strcmp0 (sampling_string, gst_jpeg2000_sampling_strings[i]))
      return (i + 1);
  }
  return GST_JPEG2000_SAMPLING_NONE;


}

/* convert GstJPEG2000Sampling enum to string */
const gchar *
gst_jpeg2000_sampling_to_string (GstJPEG2000Sampling sampling)
{
  g_return_val_if_fail (sampling > 0
      && sampling <= G_N_ELEMENTS (gst_jpeg2000_sampling_strings), NULL);
  return gst_jpeg2000_sampling_strings[sampling - 1];
}

/* check if @sampling is in RGB color space */
gboolean
gst_jpeg2000_sampling_is_rgb (GstJPEG2000Sampling sampling)
{
  return sampling == GST_JPEG2000_SAMPLING_RGB ||
      sampling == GST_JPEG2000_SAMPLING_RGBA ||
      sampling == GST_JPEG2000_SAMPLING_BGR
      || sampling == GST_JPEG2000_SAMPLING_BGRA;
}

/* check if @sampling is in YUV color space */
gboolean
gst_jpeg2000_sampling_is_yuv (GstJPEG2000Sampling sampling)
{
  return sampling == GST_JPEG2000_SAMPLING_YBRA4444_EXT ||
      sampling == GST_JPEG2000_SAMPLING_YBR444 ||
      sampling == GST_JPEG2000_SAMPLING_YBR422 ||
      sampling == GST_JPEG2000_SAMPLING_YBR420 ||
      sampling == GST_JPEG2000_SAMPLING_YBR411 ||
      sampling == GST_JPEG2000_SAMPLING_YBR410;
}

/* check if @sampling is in GRAYSCALE color space */
gboolean
gst_jpeg2000_sampling_is_mono (GstJPEG2000Sampling sampling)
{
  return sampling == GST_JPEG2000_SAMPLING_GRAYSCALE;
}

/* string values corresponding to GstJPEG2000Colorspace enums */
static const gchar *gst_jpeg2000_colorspace_strings[] = {
  "sRGB",
  "sYUV",
  "GRAY",
};

/* convert GstJPEG2000Colorspace enum to string */
GstJPEG2000Colorspace
gst_jpeg2000_colorspace_from_string (const gchar * colorspace_string)
{
  GstJPEG2000Colorspace i;

  g_return_val_if_fail (colorspace_string != NULL,
      GST_JPEG2000_COLORSPACE_NONE);

  for (i = 0; i < G_N_ELEMENTS (gst_jpeg2000_colorspace_strings); ++i) {
    if (!g_strcmp0 (colorspace_string, gst_jpeg2000_colorspace_strings[i]))
      return (i + 1);
  }
  return GST_JPEG2000_COLORSPACE_NONE;
}

/* convert string to GstJPEG2000Colorspace enum */
const gchar *
gst_jpeg2000_colorspace_to_string (GstJPEG2000Colorspace colorspace)
{
  g_return_val_if_fail (colorspace > GST_JPEG2000_COLORSPACE_NONE
      && colorspace <= G_N_ELEMENTS (gst_jpeg2000_colorspace_strings), NULL);

  return gst_jpeg2000_colorspace_strings[colorspace - 1];
}
