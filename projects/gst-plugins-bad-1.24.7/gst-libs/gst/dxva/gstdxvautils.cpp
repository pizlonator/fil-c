/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

#include "gstdxvautils.h"

/**
 * gst_dxva_codec_to_string:
 * @codec: a #GstDxvaCodec
 *
 * Returns: the string representation of @codec
 *
 * Since: 1.24
 */
const gchar *
gst_dxva_codec_to_string (GstDxvaCodec codec)
{
  switch (codec) {
    case GST_DXVA_CODEC_NONE:
      return "none";
    case GST_DXVA_CODEC_H264:
      return "H.264";
    case GST_DXVA_CODEC_VP9:
      return "VP9";
    case GST_DXVA_CODEC_H265:
      return "H.265";
    case GST_DXVA_CODEC_VP8:
      return "VP8";
    case GST_DXVA_CODEC_MPEG2:
      return "MPEG2";
    case GST_DXVA_CODEC_AV1:
      return "AV1";
    default:
      g_assert_not_reached ();
      break;
  }

  return "Unknown";
}
