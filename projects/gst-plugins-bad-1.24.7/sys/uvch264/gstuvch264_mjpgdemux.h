/* GStreamer
 *
 * uvch264_mjpg_demux: a demuxer for muxed stream in UVC H264 compliant MJPG
 *
 * Copyright (C) 2012 Cisco Systems, Inc.
 *   Author: Youness Alaoui <youness.alaoui@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_UVC_H264_MJPG_DEMUX_H__
#define __GST_UVC_H264_MJPG_DEMUX_H__

#include <gst/gst.h>


G_BEGIN_DECLS

#define GST_TYPE_UVC_H264_MJPG_DEMUX             \
  (gst_uvc_h264_mjpg_demux_get_type())
#define GST_UVC_H264_MJPG_DEMUX(obj)             \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),            \
      GST_TYPE_UVC_H264_MJPG_DEMUX,              \
      GstUvcH264MjpgDemux))
#define GST_UVC_H264_MJPG_DEMUX_CLASS(klass)     \
  (G_TYPE_CHECK_CLASS_CAST((klass),             \
      GST_TYPE_UVC_H264_MJPG_DEMUX,              \
      GstUvcH264MjpgDemuxClass))
#define GST_IS_UVC_H264_MJPG_DEMUX(obj)          \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),            \
      GST_TYPE_UVC_H264_MJPG_DEMUX))
#define GST_IS_UVC_H264_MJPG_DEMUX_CLASS(klass)  \
  (G_TYPE_CHECK_CLASS_TYPE((klass),             \
      GST_TYPE_UVC_H264_MJPG_DEMUX))

typedef struct _GstUvcH264MjpgDemux           GstUvcH264MjpgDemux;
typedef struct _GstUvcH264MjpgDemuxClass      GstUvcH264MjpgDemuxClass;

typedef struct
{
  guint32 dev_stc;
  guint32 dev_sof;
  GstClockTime host_ts;
  guint32 host_sof;
} GstUvcH264ClockSample;

typedef struct
{
  guint16 version;
  guint16 header_len;
  guint32 type;
  guint16 width;
  guint16 height;
  guint32 frame_interval;
  guint16 delay;
  guint32 pts;
} __attribute__ ((packed)) AuxiliaryStreamHeader;

struct _GstUvcH264MjpgDemux {
  GstElement element;

  /* private */
  int device_fd;
  int num_clock_samples;
  GstUvcH264ClockSample *clock_samples;
  int last_sample;
  int num_samples;
  GstPad *sink_pad;
  GstPad *jpeg_pad;
  GstPad *h264_pad;
  GstPad *yuy2_pad;
  GstPad *nv12_pad;
  GstCaps *h264_caps;
  GstCaps *yuy2_caps;
  GstCaps *nv12_caps;
  guint16 h264_width;
  guint16 h264_height;
  guint16 yuy2_width;
  guint16 yuy2_height;
  guint16 nv12_width;
  guint16 nv12_height;

  /* input segment */
  GstSegment segment;
  GstClockTime last_pts;
  gboolean pts_reordered_warning;};

struct _GstUvcH264MjpgDemuxClass {
  GstElementClass  parent_class;
};

GType gst_uvc_h264_mjpg_demux_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (uvch264mjpgdemux);

G_END_DECLS

#endif /* __GST_UVC_H264_MJPG_DEMUX_H__ */
