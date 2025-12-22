/*
 * GStreamer
 *
 * Copyright (C) 2012 Cisco Systems, Inc.
 *   Author: Youness Alaoui <youness.alaoui@collabora.co.uk>
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


#ifndef __GST_UVC_H264_SRC_H__
#define __GST_UVC_H264_SRC_H__

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <gst/basecamerabinsrc/gstbasecamerasrc.h>

#include "uvc_h264.h"

G_BEGIN_DECLS

#define GST_TYPE_UVC_H264_SRC                   \
  (gst_uvc_h264_src_get_type())
#define GST_UVC_H264_SRC(obj)                                           \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_UVC_H264_SRC, GstUvcH264Src))
#define GST_UVC_H264_SRC_CLASS(klass)                                   \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_UVC_H264_SRC, GstUvcH264SrcClass))
#define GST_IS_UVC_H264_SRC(obj)                                \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_UVC_H264_SRC))
#define GST_IS_UVC_H264_SRC_CLASS(klass)                        \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_UVC_H264_SRC))
    GType gst_uvc_h264_src_get_type (void);

typedef struct _GstUvcH264Src GstUvcH264Src;
typedef struct _GstUvcH264SrcClass GstUvcH264SrcClass;

enum GstVideoRecordingStatus {
  GST_VIDEO_RECORDING_STATUS_DONE,
  GST_VIDEO_RECORDING_STATUS_STARTING,
  GST_VIDEO_RECORDING_STATUS_RUNNING,
  GST_VIDEO_RECORDING_STATUS_FINISHING
};

enum  {
  QP_I_FRAME = 0,
  QP_P_FRAME,
  QP_B_FRAME,
  QP_FRAMES
};

typedef enum {
  UVC_H264_SRC_FORMAT_NONE,
  UVC_H264_SRC_FORMAT_JPG,
  UVC_H264_SRC_FORMAT_H264,
  UVC_H264_SRC_FORMAT_RAW
} GstUvcH264SrcFormat;

/**
 * GstUcH264Src:
 *
 */
struct _GstUvcH264Src
{
  GstBaseCameraSrc parent;

  GstPad *vfsrc;
  GstPad *imgsrc;
  GstPad *vidsrc;

  /* source elements */
  GstElement *v4l2_src;
  GstElement *mjpg_demux;
  GstElement *jpeg_dec;
  GstElement *vid_colorspace;
  GstElement *vf_colorspace;

  GstUvcH264SrcFormat main_format;
  guint16 main_width;
  guint16 main_height;
  guint32 main_frame_interval;
  UvcH264StreamFormat main_stream_format;
  guint16 main_profile;
  GstUvcH264SrcFormat secondary_format;
  guint16 secondary_width;
  guint16 secondary_height;
  guint32 secondary_frame_interval;

  int v4l2_fd;
  guint8 h264_unit_id;
  libusb_context *usb_ctx;

  GstPadEventFunction srcpad_event_func;
  GstEvent *key_unit_event;
  GstSegment segment;

  gboolean started;

  /* When restarting the source */
  gboolean reconfiguring;
  gboolean vid_newseg;
  gboolean vf_newseg;

  gchar *colorspace_name;
  gchar *jpeg_decoder_name;
  int num_clock_samples;

  /* v4l2src proxied properties */
  guint32 num_buffers;
  gchar *device;

  /* Static controls */
  guint32 initial_bitrate;
  guint16 slice_units;
  UvcH264SliceMode slice_mode;
  guint16 iframe_period;
  UvcH264UsageType usage_type;
  UvcH264Entropy entropy;
  gboolean enable_sei;
  guint8 num_reorder_frames;
  gboolean preview_flipped;
  guint16 leaky_bucket_size;

  /* Dynamic controls */
  UvcH264RateControl rate_control;
  gboolean fixed_framerate;
  guint8 level_idc;
  guint32 peak_bitrate;
  guint32 average_bitrate;
  gint8 min_qp[QP_FRAMES];
  gint8 max_qp[QP_FRAMES];
  guint8 ltr_buffer_size;
  guint8 ltr_encoder_control;
};


/**
 * GstUvcH264SrcClass:
 *
 */
struct _GstUvcH264SrcClass
{
  GstBaseCameraSrcClass parent;
};

GST_ELEMENT_REGISTER_DECLARE (uvch264src);

G_END_DECLS

#endif /* __GST_UVC_H264_SRC_H__ */
