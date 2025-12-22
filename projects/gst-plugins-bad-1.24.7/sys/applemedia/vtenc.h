/*
 * Copyright (C) 2010 Ole André Vadla Ravnås <oleavr@soundrop.com>
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

#ifndef __GST_VTENC_H__
#define __GST_VTENC_H__

#include <gst/gst.h>
#include <gst/base/gstqueuearray.h>
#include <gst/codecparsers/gsth264parser.h>
#include <gst/video/video.h>
#include <VideoToolbox/VideoToolbox.h>

G_BEGIN_DECLS

#define GST_VTENC_CAST(obj) \
  ((GstVTEnc *) (obj))
#define GST_VTENC_CLASS_GET_CODEC_DETAILS(klass) \
  ((const GstVTEncoderDetails *) g_type_get_qdata (G_OBJECT_CLASS_TYPE (klass), \
      GST_VTENC_CODEC_DETAILS_QDATA))

typedef struct _GstVTEncoderDetails GstVTEncoderDetails;

typedef struct _GstVTEncClassParams GstVTEncClassParams;
typedef struct _GstVTEncClass GstVTEncClass;
typedef struct _GstVTEnc GstVTEnc;

struct _GstVTEncoderDetails
{
  const gchar * name;
  const gchar * element_name;
  const gchar * mimetype;
  CMVideoCodecType format_id;
  gboolean require_hardware;
};

struct _GstVTEncClass
{
  GstVideoEncoderClass parent_class;
};

struct _GstVTEnc
{
  GstVideoEncoder parent;

  const GstVTEncoderDetails * details;

  CMVideoCodecType specific_format_id;
  CFStringRef profile_level;
  GstH264Profile h264_profile;
  guint bitrate;
  gboolean allow_frame_reordering;
  gboolean realtime;
  gdouble quality;
  gint max_keyframe_interval;
  GstClockTime max_keyframe_interval_duration;
  gint latency_frames;
  gboolean preserve_alpha;

  gboolean dump_properties;
  gboolean dump_attributes;

  gboolean have_field_order;
  GstVideoCodecState *input_state;
  GstVideoInfo video_info;
  VTCompressionSessionRef session;
  CFDictionaryRef keyframe_props;
  GstClockTime dts_offset;

  GstQueueArray * output_queue;
  /* Protects output_queue, is_flushing and pause_task */
  GMutex queue_mutex;
  GCond queue_cond;
  
  /* downstream_ret is protected by the STREAM_LOCK */
  GstFlowReturn downstream_ret;
  gboolean negotiate_downstream;
  gboolean is_flushing;
  gboolean pause_task;
};

void gst_vtenc_register_elements (GstPlugin * plugin);

G_END_DECLS

#endif /* __GST_VTENC_H__ */
