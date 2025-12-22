/* AV1
 * Copyright (C) 2018 Wonchul Lee <chul0812@gmail.com>
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

#include "gstav1utils.h"

typedef struct
{
  enum aom_img_fmt aom_format;
  GstVideoFormat gst_format;
} AomImageFormat;

static const AomImageFormat img_formats[] = {
  {AOM_IMG_FMT_YV12, GST_VIDEO_FORMAT_YV12},
  {AOM_IMG_FMT_I420, GST_VIDEO_FORMAT_I420},
  {AOM_IMG_FMT_I422, GST_VIDEO_FORMAT_Y42B},
  {AOM_IMG_FMT_I444, GST_VIDEO_FORMAT_Y444},
};

const char *
gst_av1_get_error_name (aom_codec_err_t status)
{
  switch (status) {
    case AOM_CODEC_OK:
      return "OK";
    case AOM_CODEC_ERROR:
      return "error";
    case AOM_CODEC_MEM_ERROR:
      return "mem error";
    case AOM_CODEC_ABI_MISMATCH:
      return "abi mismatch";
    case AOM_CODEC_INCAPABLE:
      return "incapable";
    case AOM_CODEC_UNSUP_BITSTREAM:
      return "unsupported bitstream";
    case AOM_CODEC_UNSUP_FEATURE:
      return "unsupported feature";
    case AOM_CODEC_CORRUPT_FRAME:
      return "corrupt frame";
    case AOM_CODEC_INVALID_PARAM:
      return "invalid parameter";
    default:
      return "unknown";
  }
}

gint
gst_video_format_to_av1_img_format (GstVideoFormat format)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (img_formats); i++)
    if (img_formats[i].gst_format == format)
      return img_formats[i].aom_format;

  GST_WARNING ("av1 img format not found");
  return -1;
}
