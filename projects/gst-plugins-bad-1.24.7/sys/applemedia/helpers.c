/*
 * Copyright (C) 2023 Nirbheek Chauhan <nirbheek@centricular.com>
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

#include <CoreVideo/CVPixelBuffer.h>
#include "helpers.h"

GstVideoFormat
gst_video_format_from_cvpixelformat (int fmt)
{
  /* Note that video and full-range color values map to the same format */
  switch (fmt) {
      /* YUV */
    case kCVPixelFormatType_420YpCbCr8Planar:
    case kCVPixelFormatType_420YpCbCr8PlanarFullRange:
      return GST_VIDEO_FORMAT_I420;
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
    case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
      return GST_VIDEO_FORMAT_NV12;
    case kCVPixelFormatType_422YpCbCr8:
      return GST_VIDEO_FORMAT_UYVY;
    case kCVPixelFormatType_422YpCbCr8_yuvs:
      return GST_VIDEO_FORMAT_YUY2;
      /* Alpha YUV */
    case kCVPixelFormatType_4444AYpCbCr16:
      return GST_VIDEO_FORMAT_AYUV64;
      /* RGB formats */
    case kCVPixelFormatType_32ARGB:
      return GST_VIDEO_FORMAT_ARGB;
    case kCVPixelFormatType_32BGRA:
      return GST_VIDEO_FORMAT_BGRA;
    case kCVPixelFormatType_64ARGB:
      return GST_VIDEO_FORMAT_ARGB64_BE;
    case kCVPixelFormatType_64RGBALE:
      return GST_VIDEO_FORMAT_RGBA64_LE;
    default:
      return GST_VIDEO_FORMAT_UNKNOWN;
  }
}

int
gst_video_format_to_cvpixelformat (GstVideoFormat fmt)
{
  switch (fmt) {
      /* YUV */
    case GST_VIDEO_FORMAT_I420:
      return kCVPixelFormatType_420YpCbCr8Planar;
    case GST_VIDEO_FORMAT_NV12:
      return kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
    case GST_VIDEO_FORMAT_UYVY:
      return kCVPixelFormatType_422YpCbCr8;
    case GST_VIDEO_FORMAT_YUY2:
      return kCVPixelFormatType_422YpCbCr8_yuvs;
      /* Alpha YUV */
    case GST_VIDEO_FORMAT_AYUV64:
/* This is fine for now because Apple only ships LE devices */
#if G_BYTE_ORDER != G_LITTLE_ENDIAN
#error "AYUV64 is NE but kCVPixelFormatType_4444AYpCbCr16 is LE"
#endif
      return kCVPixelFormatType_4444AYpCbCr16;
      /* RGB formats */
    case GST_VIDEO_FORMAT_ARGB:
      return kCVPixelFormatType_32ARGB;
    case GST_VIDEO_FORMAT_BGRA:
      return kCVPixelFormatType_32BGRA;
    case GST_VIDEO_FORMAT_ARGB64_BE:
      return kCVPixelFormatType_64ARGB;
    case GST_VIDEO_FORMAT_RGBA64_LE:
      return kCVPixelFormatType_64RGBALE;
    default:
      g_assert_not_reached ();
      return -1;
  }
}
