/* GStreamer JPEG 2000 Parser
 *
 * Copyright (C) <2016> Milos Seleceni
 *  @author Milos Seleceni <milos.seleceni@comprimato.com>
 *
 * Copyright (C) <2016-2017> Grok Image Compression Inc.
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
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef __BASETSMUX_JPEG2000_H__
#define __BASETSMUX_JPEG2000_H__

#include "gstbasetsmux.h"

/* color specifications for JPEG 2000 stream over MPEG TS */
typedef enum
{
  GST_MPEGTS_JPEG2000_COLORSPEC_UNKNOWN,
  GST_MPEGTS_JPEG2000_COLORSPEC_SRGB,
  GST_MPEGTS_JPEG2000_COLORSPEC_REC601,
  GST_MPEGTS_JPEG2000_COLORSPEC_REC709,
  GST_MPEGTS_JPEG2000_COLORSPEC_CIELUV,
  GST_MPEGTS_JPEG2000_COLORSPEC_CIEXYZ,
  GST_MPEGTS_JPEG2000_COLORSPEC_REC2020,
  GST_MPEGTS_JPEG2000_COLORSPEC_SMPTE2084
} GstGstBaseTsJpeg2000ColorSpec;


typedef struct j2k_private_data
{
  gboolean interlace;
  guint16 den;
  guint16 num;
  /* Maximum bitrate box */
  guint32 max_bitrate;
  /* Field Coding Box */
  guint8 Fic;
  guint8 Fio;
  /* Broadcast color box */
  guint8 color_spec;
} j2k_private_data;

GstBuffer *gst_base_ts_mux_prepare_jpeg2000 (GstBuffer * buf, GstBaseTsMuxPad * pad,
    GstBaseTsMux * mux);

void gst_base_ts_mux_free_jpeg2000 (gpointer prepare_data);

#endif /* __BASETSMUX_JPEG2000_H__ */
