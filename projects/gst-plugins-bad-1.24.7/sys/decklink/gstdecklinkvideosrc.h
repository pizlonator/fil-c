/* GStreamer
 *
 * Copyright (C) 2011 David Schleef <ds@schleef.org>
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015 Florian Langlois <florian.langlois@fr.thalesgroup.com>
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

#ifndef __GST_DECKLINK_VIDEO_SRC_H__
#define __GST_DECKLINK_VIDEO_SRC_H__

#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/video/video.h>
#include "gstdecklink.h"

G_BEGIN_DECLS

#define GST_TYPE_DECKLINK_VIDEO_SRC \
  (gst_decklink_video_src_get_type())
#define GST_DECKLINK_VIDEO_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_DECKLINK_VIDEO_SRC, GstDecklinkVideoSrc))
#define GST_DECKLINK_VIDEO_SRC_CAST(obj) \
  ((GstDecklinkVideoSrc*)obj)
#define GST_DECKLINK_VIDEO_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_DECKLINK_VIDEO_SRC, GstDecklinkVideoSrcClass))
#define GST_IS_DECKLINK_VIDEO_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_DECKLINK_VIDEO_SRC))
#define GST_IS_DECKLINK_VIDEO_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_DECKLINK_VIDEO_SRC))

typedef struct _GstDecklinkVideoSrc GstDecklinkVideoSrc;
typedef struct _GstDecklinkVideoSrcClass GstDecklinkVideoSrcClass;

typedef enum {
  SIGNAL_STATE_UNKNOWN,
  SIGNAL_STATE_LOST,
  SIGNAL_STATE_AVAILABLE,
} GstDecklinkSignalState;

struct _GstDecklinkVideoSrc
{
  GstPushSrc parent;

  GstDecklinkModeEnum mode;
  GstDecklinkModeEnum caps_mode;
  gint aspect_ratio_flag; /* -1 when unknown, 0 not set, 1 set */
  BMDPixelFormat caps_format;
  GstDecklinkConnectionEnum connection;
  gint device_number;
  gint64 persistent_id;
  gboolean output_stream_time;
  GstClockTime skip_first_time;
  gboolean drop_no_signal_frames;
  GstClockTime expected_stream_time;
  guint64 processed;
  guint64 dropped;
  guint64 first_stream_time;
  guint64 no_signal_count;

  GstVideoInfo info;
  GstDecklinkVideoFormat video_format;
  GstDecklinkProfileId profile_id;
  BMDTimecodeFormat timecode_format;

  GstDecklinkInput *input;

  GCond cond;
  GMutex lock;
  gboolean flushing;
  GstQueueArray *current_frames;
  GstDecklinkSignalState signal_state;

  guint buffer_size;

  /* Protected by lock */
  GstClockTime first_time;

  GstClockTime *times;
  GstClockTime *times_temp;
  guint window_size, window_fill;
  gboolean window_filled;
  guint window_skip, window_skip_count;
  struct {
    GstClockTime xbase, b;
    GstClockTime num, den;
  } current_time_mapping;
  struct {
    GstClockTime xbase, b;
    GstClockTime num, den;
  } next_time_mapping;
  gboolean next_time_mapping_pending;

  GstVideoVBIParser *vbiparser;
  GstVideoFormat anc_vformat;
  gint anc_width;
  gboolean output_cc;
  gint last_cc_vbi_line;
  gint last_cc_vbi_line_field2;
  gboolean output_afd_bar;
  gint last_afd_bar_vbi_line;
  gint last_afd_bar_vbi_line_field2;

  guint skipped_last;
  GstClockTime skip_from_timestamp;
  GstClockTime skip_to_timestamp;
};

struct _GstDecklinkVideoSrcClass
{
  GstPushSrcClass parent_class;
};

GType gst_decklink_video_src_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (decklinkvideosrc);

G_END_DECLS

#endif /* __GST_DECKLINK_VIDEO_SRC_H__ */
