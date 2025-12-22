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

#ifndef __GST_JPEG2000_SAMPLING_H__
#define __GST_JPEG2000_SAMPLING_H__

#include <gst/gst.h>
#include <gst/codecparsers/codecparsers-prelude.h>

/**
 * GstJPEG2000Sampling:
 * Sampling values from RF 5371 for JPEG 2000 over RTP : https://datatracker.ietf.org/doc/rfc5371/C
 * Note: sampling extensions that are not listed in the RFC are signified by an _EXT at the end of the enum
 *
 * @GST_JPEG2000_SAMPLING_NONE: no sampling
 * @GST_JPEG2000_SAMPLING_RGB: standard Red, Green, Blue color space.
 * @GST_JPEG2000_SAMPLING_BGR: standard Blue, Green, Red color space.
 * @GST_JPEG2000_SAMPLING_RGBA: standard Red, Green, Blue, Alpha color space.
 * @GST_JPEG2000_SAMPLING_BGRA: standard Blue, Green, Red, Alpha color space.
 * @GST_JPEG2000_SAMPLING_YBR444: standard YCbCr color space; no subsampling.
 * @GST_JPEG2000_SAMPLING_YBR422: standard YCbCr color space; Cb and Cr are subsampled horizontally by 1/2.
 * @GST_JPEG2000_SAMPLING_YBR420: standard YCbCr color space; Cb and Cr are subsampled horizontally and vertically by 1/2.
 * @GST_JPEG2000_SAMPLING_YBR411: standard YCbCr color space; Cb and Cr are subsampled vertically by 1/4 (Since: 1.20).
 * @GST_JPEG2000_SAMPLING_YBR410: standard YCbCr color space; Cb and Cr are subsampled vertically by 1/4 alternating the Cb and Cr component.
 * @GST_JPEG2000_SAMPLING_GRAYSCALE:  basically, a single component image of just multilevels of grey.
 * @GST_JPEG2000_SAMPLING_YBRA4444_EXT: standard YCbCr color space, alpha channel, no subsampling,
 */

/**
 * GST_JPEG2000_SAMPLING_YBR411:
 *
 * standard YCbCr color space; Cb and Cr are subsampled vertically by 1/4
 *
 * Since: 1.20
 */
typedef enum
{
  GST_JPEG2000_SAMPLING_NONE,
  GST_JPEG2000_SAMPLING_RGB,
  GST_JPEG2000_SAMPLING_BGR,
  GST_JPEG2000_SAMPLING_RGBA,
  GST_JPEG2000_SAMPLING_BGRA,
  GST_JPEG2000_SAMPLING_YBR444,
  GST_JPEG2000_SAMPLING_YBR422,
  GST_JPEG2000_SAMPLING_YBR420,
  GST_JPEG2000_SAMPLING_YBR410,
  GST_JPEG2000_SAMPLING_GRAYSCALE,
  GST_JPEG2000_SAMPLING_YBRA4444_EXT,
  GST_JPEG2000_SAMPLING_YBR411
} GstJPEG2000Sampling;

/* GST_JPEG2000_SAMPLING_LIST: sampling strings in list form, for use in caps */
#define GST_JPEG2000_SAMPLING_LIST "sampling = (string) {\"RGB\", \"BGR\", \"RGBA\", \"BGRA\", \"YCbCr-4:4:4\", \"YCbCr-4:2:2\", \"YCbCr-4:2:0\", \"YCbCr-4:1:1\", \"YCbCr-4:1:0\", \"GRAYSCALE\" , \"YCbCrA-4:4:4:4\"}"

GST_CODEC_PARSERS_API
const gchar *gst_jpeg2000_sampling_to_string (GstJPEG2000Sampling sampling);

GST_CODEC_PARSERS_API
GstJPEG2000Sampling gst_jpeg2000_sampling_from_string (const gchar *
    sampling_string);

GST_CODEC_PARSERS_API
gboolean gst_jpeg2000_sampling_is_rgb (GstJPEG2000Sampling sampling);

GST_CODEC_PARSERS_API
gboolean gst_jpeg2000_sampling_is_yuv (GstJPEG2000Sampling sampling);

GST_CODEC_PARSERS_API
gboolean gst_jpeg2000_sampling_is_mono (GstJPEG2000Sampling sampling);


/**
 * GstJPEG2000Colorspace:
 * @GST_JPEG2000_COLORSPACE_NONE: no color space
 * @GST_JPEG2000_COLORSPACE_RGB: standard RGB color space
 * @GST_JPEG2000_COLORSPACE_YUV: standard YUV color space
 * @GST_JPEG2000_COLORSPACE_GRAY: monochrome color space
 */
typedef enum
{
  GST_JPEG2000_COLORSPACE_NONE,
  GST_JPEG2000_COLORSPACE_RGB,
  GST_JPEG2000_COLORSPACE_YUV,
  GST_JPEG2000_COLORSPACE_GRAY
} GstJPEG2000Colorspace;

GST_CODEC_PARSERS_API
const gchar *gst_jpeg2000_colorspace_to_string (GstJPEG2000Colorspace
    colorspace);

GST_CODEC_PARSERS_API
GstJPEG2000Colorspace gst_jpeg2000_colorspace_from_string (const gchar *
    colorspace_string);

/* GST_JPEG2000_COLORSPACE_LIST: color space strings in list form, for use in caps */
#define GST_JPEG2000_COLORSPACE_LIST "colorspace = (string) { \"sRGB\", \"sYUV\", \"GRAY\" }"

#endif
