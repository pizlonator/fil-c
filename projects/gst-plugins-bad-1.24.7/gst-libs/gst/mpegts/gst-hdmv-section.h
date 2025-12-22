/*
 * gst-hdmv-section.h -
 * Copyright (C) 2020, Centricular ltd
 *
 * Authors:
 *   Edward Hervey <edward@centricular.com>
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

#ifndef GST_HDMV_SECTION_H
#define GST_HDMV_SECTION_H

#include <gst/gst.h>
#include <gst/mpegts/gstmpegtssection.h>
#include <gst/mpegts/gstmpegtsdescriptor.h>

G_BEGIN_DECLS

/**
 * SECTION:gst-hdmv-section
 * @title: HDMV variants of MPEG-TS (Bluray, AVCHD, ...)
 * @short_description: Stream Types for the various Bluray specifications
 * @include: gst/mpegts/mpegts.h
 */

/**
 * GstMpegtsHdmvStreamType:
 *
 * Type of mpeg-ts streams for Blu-ray formats. To be matched with the
 * stream-type of a #GstMpegtsSection.
 *
 * Since: 1.20
 */
typedef enum {
  GST_MPEGTS_STREAM_TYPE_HDMV_AUDIO_LPCM = 0x80,
  GST_MPEGTS_STREAM_TYPE_HDMV_AUDIO_AC3 = 0x81,
  GST_MPEGTS_STREAM_TYPE_HDMV_AUDIO_DTS = 0x82,
  GST_MPEGTS_STREAM_TYPE_HDMV_AUDIO_AC3_TRUE_HD = 0x83,
  GST_MPEGTS_STREAM_TYPE_HDMV_AUDIO_AC3_PLUS = 0x84,
  GST_MPEGTS_STREAM_TYPE_HDMV_AUDIO_DTS_HD = 0x85,
  GST_MPEGTS_STREAM_TYPE_HDMV_AUDIO_DTS_HD_MASTER_AUDIO = 0x86,
  GST_MPEGTS_STREAM_TYPE_HDMV_AUDIO_EAC3 = 0x87,
  GST_MPEGTS_STREAM_TYPE_HDMV_SUBPICTURE_PGS = 0x90,
  GST_MPEGTS_STREAM_TYPE_HDMV_IGS = 0x91,
  GST_MPEGTS_STREAM_TYPE_HDMV_SUBTITLE = 0x92,
  GST_MPEGTS_STREAM_TYPE_HDMV_AUDIO_AC3_PLUS_SECONDARY = 0xa1,
  GST_MPEGTS_STREAM_TYPE_HDMV_AUDIO_DTS_HD_SECONDARY = 0xa2,
} GstMpegtsHdmvStreamType;

G_END_DECLS

#endif  /* GST_HDMV_SECTION_H */
