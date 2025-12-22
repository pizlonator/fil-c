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

#ifndef __GST_DECKLINK_AUDIO_SRC_H__
#define __GST_DECKLINK_AUDIO_SRC_H__

#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/audio/audio.h>
#include "gstdecklink.h"

G_BEGIN_DECLS

#define GST_TYPE_DECKLINK_AUDIO_SRC \
  (gst_decklink_audio_src_get_type())
#define GST_DECKLINK_AUDIO_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_DECKLINK_AUDIO_SRC, GstDecklinkAudioSrc))
#define GST_DECKLINK_AUDIO_SRC_CAST(obj) \
  ((GstDecklinkAudioSrc*)obj)
#define GST_DECKLINK_AUDIO_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_DECKLINK_AUDIO_SRC, GstDecklinkAudioSrcClass))
#define GST_IS_DECKLINK_AUDIO_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_DECKLINK_AUDIO_SRC))
#define GST_IS_DECKLINK_AUDIO_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_DECKLINK_AUDIO_SRC))

typedef struct _GstDecklinkAudioSrc GstDecklinkAudioSrc;
typedef struct _GstDecklinkAudioSrcClass GstDecklinkAudioSrcClass;

struct _GstDecklinkAudioSrc
{
  GstPushSrc parent;

  GstDecklinkModeEnum mode;
  GstDecklinkAudioConnectionEnum connection;
  gint device_number;
  gint64 persistent_id;
  GstDecklinkAudioChannelsEnum channels;
  gint64 channels_found;

  GstAudioInfo info;

  GstDecklinkInput *input;

  GCond cond;
  GMutex lock;
  gboolean flushing;
  GstQueueArray *current_packets;

  /* properties for handling jittery timestamps */
  GstClockTime alignment_threshold;
  GstClockTime discont_wait;

  /* counter to keep track of timestamps */
  guint64 next_offset;

  /* detect gaps in stream time */
  GstClockTime expected_stream_time;
  guint64 processed;
  guint64 dropped;
  GstClockTime last_hardware_time;

  /* Last time we noticed a discont */
  GstClockTime discont_time;

  guint buffer_size;

  guint skipped_last;
  GstClockTime skip_from_timestamp;
  GstClockTime skip_to_timestamp;
};

struct _GstDecklinkAudioSrcClass
{
  GstPushSrcClass parent_class;
};

GType gst_decklink_audio_src_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (decklinkaudiosrc);

G_END_DECLS

#endif /* __GST_DECKLINK_AUDIO_SRC_H__ */
