/* GStreamer
 * Copyright (C) <2010> Thiago Santos <thiago.sousa.santos@collabora.co.uk>
 * Copyright (C) <2018> Nicola Murino <nicola.murino@gmail.com>
 *
 * gstopencvutils.c: miscellaneous utility functions
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
#include "config.h"
#endif

#include "gstopencvutils.h"
#include <opencv2/core.hpp>

/*
The various opencv image containers or headers store the following information:
- number of channels (usually 1, 3 or 4)
- depth (8, 16, 32, 64...); all channels have the same depth.
The channel layout (BGR vs RGB) is not stored...

This gives us the following list of supported image formats:
  CV_8UC1, CV_8UC2, CV_8UC3, CV_8UC4
  CV_8SC1, CV_8SC2, CV_8SC3, CV_8SC4
  CV_16UC1, CV_16UC2, CV_16UC3, CV_16UC4
  CV_16SC1, CV_16SC2, CV_16SC3, CV_16SC4
  CV_32SC1, CV_32SC2, CV_32SC3, CV_32SC4
  CV_32FC1, CV_32FC2, CV_32FC3, CV_32FC4
  CV_64FC1, CV_64FC2, CV_64FC3, CV_64FC4

Where the first part of the format name is the depth followed by a digit
representing the number of channels.
Note that opencv supports more that 4 channels.

The opencv algorithms don't all support all the image types.
For example findChessboardCorners() supports only 8 bits formats
(gray scale and color).

And, typically, this algorithm will convert the image to gray scale before
proceeding. It will do so with something like this:
   cvtColor(srcImg, destImg, CV_BGR2GRAY);

The conversion will work on any BGR format (BGR, BGRA, BGRx).
The extra channel(s) will be ignored.
It will also produce a result for any RGB format.
The result will be "wrong" to the human eye and might affect some algorithms
(not findChessboardCorners() afaik...).
This is due to how RGB gets converted to gray where each color has a
different weight.

Another example is the 2D rendering API.
It work with RGB but the colors will be wrong.

Likewise other layouts like xBGR and ABGR formats will probably misbehave
with most algorithms.

The bad thing is that it is not possible to change the "default" BGR format.
Safest is to not assume that RGB will work and always convert to BGR.

That said, the current opencv gstreamer elements all accept BGR and RGB caps !
Some have restrictions but if a format is supported then both BGR and RGB
layouts will be supported.
*/

gboolean gst_opencv_parse_cv_mat_params_from_caps
    (GstCaps * caps, gint * width, gint * height, int *cv_type, GError ** err)
{
  GstVideoInfo info;
  gchar *caps_str;

  if (!gst_video_info_from_caps (&info, caps)) {
    caps_str = gst_caps_to_string (caps);
    GST_ERROR ("Failed to get video info from caps %s", caps_str);
    g_set_error (err, GST_CORE_ERROR, GST_CORE_ERROR_NEGOTIATION,
        "Failed to get video info from caps %s", caps_str);
    g_free (caps_str);
    return FALSE;
  }

  return gst_opencv_cv_mat_params_from_video_info (&info, width, height,
      cv_type, err);
}

gboolean gst_opencv_cv_mat_params_from_video_info
    (GstVideoInfo * info, gint * width, gint * height, int *cv_type,
    GError ** err)
{
  GstVideoFormat format;

  format = GST_VIDEO_INFO_FORMAT (info);
  if (!gst_opencv_cv_image_type_from_video_format (format, cv_type, err)) {
    return FALSE;
  }

  *width = GST_VIDEO_INFO_WIDTH (info);
  *height = GST_VIDEO_INFO_HEIGHT (info);

  return TRUE;
}

gboolean
gst_opencv_cv_image_type_from_video_format (GstVideoFormat format,
    int *cv_type, GError ** err)
{
  const gchar *format_str;

  switch (format) {
    case GST_VIDEO_FORMAT_GRAY8:
      *cv_type = CV_8UC1;
      break;
    case GST_VIDEO_FORMAT_RGB:
    case GST_VIDEO_FORMAT_BGR:
      *cv_type = CV_8UC3;
      break;
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_xRGB:
    case GST_VIDEO_FORMAT_BGRx:
    case GST_VIDEO_FORMAT_xBGR:
    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_ARGB:
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_ABGR:
      *cv_type = CV_8UC4;
      break;
    case GST_VIDEO_FORMAT_GRAY16_LE:
    case GST_VIDEO_FORMAT_GRAY16_BE:
      *cv_type = CV_16UC1;
      break;
    default:
      format_str = gst_video_format_to_string (format);
      g_set_error (err, GST_CORE_ERROR, GST_CORE_ERROR_NEGOTIATION,
          "Unsupported video format %s", format_str);
      return FALSE;
  }

  return TRUE;
}

GstCaps *
gst_opencv_caps_from_cv_image_type (int cv_type)
{
  GstCaps *c = gst_caps_new_empty ();
  switch (cv_type) {
    case CV_8UC1:
      gst_caps_append (c, gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("GRAY8")));
      break;
    case CV_8UC3:
      gst_caps_append (c, gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("RGB")));
      gst_caps_append (c, gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("BGR")));
      break;
    case CV_8UC4:
      gst_caps_append (c, gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("RGBx")));
      gst_caps_append (c, gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("xRGB")));
      gst_caps_append (c, gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("BGRx")));
      gst_caps_append (c, gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("xBGR")));
      gst_caps_append (c, gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("RGBA")));
      gst_caps_append (c, gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("ARGB")));
      gst_caps_append (c, gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("BGRA")));
      gst_caps_append (c, gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("ABGR")));
      break;
    case CV_16UC1:
      gst_caps_append (c,
          gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("GRAY16_LE")));
      gst_caps_append (c,
          gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("GRAY16_BE")));
      break;
  }
  return c;
}
