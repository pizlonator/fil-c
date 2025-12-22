/* GStreamer
 *
 * Copyright (C) 2011 David Schleef <ds@schleef.org>
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
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

#ifndef __GST_DECKLINK_VIDEO_SINK_H__
#define __GST_DECKLINK_VIDEO_SINK_H__

#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/video/video.h>
#include "gstdecklink.h"

G_BEGIN_DECLS

#define GST_TYPE_DECKLINK_VIDEO_SINK \
  (gst_decklink_video_sink_get_type())
#define GST_DECKLINK_VIDEO_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_DECKLINK_VIDEO_SINK, GstDecklinkVideoSink))
#define GST_DECKLINK_VIDEO_SINK_CAST(obj) \
  ((GstDecklinkVideoSink*)obj)
#define GST_DECKLINK_VIDEO_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_DECKLINK_VIDEO_SINK, GstDecklinkVideoSinkClass))
#define GST_IS_DECKLINK_VIDEO_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_DECKLINK_VIDEO_SINK))
#define GST_IS_DECKLINK_VIDEO_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_DECKLINK_VIDEO_SINK))

typedef struct _GstDecklinkVideoSink GstDecklinkVideoSink;
typedef struct _GstDecklinkVideoSinkClass GstDecklinkVideoSinkClass;

struct _GstDecklinkVideoSink
{
  GstBaseSink parent;

  GstDecklinkModeEnum mode;
  gint device_number;
  gint64 persistent_id;
  GstDecklinkVideoFormat video_format;
  GstDecklinkProfileId profile_id;
  BMDTimecodeFormat timecode_format;
  BMDKeyerMode keyer_mode;
  gint keyer_level;

  GstVideoInfo info;

  GstClockTime internal_base_time;
  GstClockTime external_base_time;

  /* really an internal start time */
  GstClockTime internal_time_offset;
  GstClockTime internal_pause_time;

  GstDecklinkOutput *output;

  GstVideoVBIEncoder *vbiencoder;
  GstVideoFormat anc_vformat;

  gint caption_line;
  guint16 cdp_hdr_sequence_cntr;

  gint afd_bar_line;
  GstDecklinkMappingFormat mapping_format;
};

struct _GstDecklinkVideoSinkClass
{
  GstBaseSinkClass parent_class;
};

GType gst_decklink_video_sink_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (decklinkvideosink);

void gst_decklink_video_sink_convert_to_internal_clock (GstDecklinkVideoSink * self,
    GstClockTime * timestamp, GstClockTime * duration);

G_END_DECLS

#endif /* __GST_DECKLINK_VIDEO_SINK_H__ */
