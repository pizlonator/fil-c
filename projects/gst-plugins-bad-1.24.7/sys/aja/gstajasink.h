/* GStreamer
 * Copyright (C) 2021 Sebastian Dröge <sebastian@centricular.com>
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

#pragma once

#include <gst/base/base.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include "gstajacommon.h"

G_BEGIN_DECLS

#define GST_TYPE_AJA_SINK (gst_aja_sink_get_type())
#define GST_AJA_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AJA_SINK, GstAjaSink))
#define GST_AJA_SINK_CAST(obj) ((GstAjaSink *)obj)
#define GST_AJA_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_AJA_SINK, GstAjaSinkClass))
#define GST_IS_AJA_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_AJA_SINK))
#define GST_IS_AJA_SINK_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_AJA_SINK))

typedef struct _GstAjaSink GstAjaSink;
typedef struct _GstAjaSinkClass GstAjaSinkClass;

struct _GstAjaSink {
  GstBaseSink parent;

  // Everything below protected by queue lock
  GMutex queue_lock;
  GCond queue_cond;
  GstQueueArray *queue;
  gboolean eos;
  gboolean playing;
  gboolean shutdown;
  gboolean draining;
  // Hold by set_caps() to wait until drained
  GCond drain_cond;
  gboolean flushing;

  GstAjaNtv2Device *device;
  NTV2DeviceID device_id;
  GstAllocator *allocator;

  // Only allocated on demand
  GstBufferPool *buffer_pool;
  GstBufferPool *audio_buffer_pool;
  GstBufferPool *anc_buffer_pool;

  // Properties
  gchar *device_identifier;
  NTV2Channel channel;
  guint queue_size;
  guint start_frame, end_frame;
  guint output_cpu_core;

  GstAjaAudioSystem audio_system_setting;
  GstAjaOutputDestination output_destination;
  GstAjaSdiMode sdi_mode;
  GstAjaTimecodeIndex timecode_index;
  gboolean rp188;
  GstAjaReferenceSource reference_source;
  gboolean handle_ancillary_meta;

  gint cea608_line_number;
  gint cea708_line_number;

  NTV2AudioSystem audio_system;
  NTV2VideoFormat video_format;
  bool quad_mode;
  NTV2VANCMode vanc_mode;
  guint32 f2_start_line;
  NTV2TCIndexes *tc_indexes;

  GstCaps *configured_caps;
  GstVideoInfo configured_info;
  gint configured_audio_channels;

  AJAThread *output_thread;
};

struct _GstAjaSinkClass {
  GstBaseSinkClass parent_class;
};

G_GNUC_INTERNAL
GType gst_aja_sink_get_type(void);

G_END_DECLS
