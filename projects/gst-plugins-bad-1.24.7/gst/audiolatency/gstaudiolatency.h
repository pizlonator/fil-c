/*
 * Copyright (C) 2018 Centricular Ltd.
 *   Author: Nirbheek Chauhan <nirbheek@centricular.com>
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

#ifndef __GST_AUDIOLATENCY_H__
#define __GST_AUDIOLATENCY_H__

#include <gst/gst.h>

G_BEGIN_DECLS
#define GST_TYPE_AUDIOLATENCY \
  (gst_audiolatency_get_type ())
#define GST_AUDIOLATENCY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_AUDIOLATENCY, GstAudioLatency))
#define GST_AUDIOLATENCY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_AUDIOLATENCY, GstAudioLatencyClass))
#define GST_IS_AUDIOLATENCY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_AUDIOLATENCY))
#define GST_IS_AUDIOLATENCY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_AUDIOLATENCY))
typedef struct _GstAudioLatency GstAudioLatency;
typedef struct _GstAudioLatencyClass GstAudioLatencyClass;

#define GST_AUDIOLATENCY_NUM_LATENCIES 5

struct _GstAudioLatency
{
  GstBin parent;

  GstPad *sinkpad;
  GstPad *srcpad;
  /* audiotestsrc */
  GstElement *audiosrc;

  /* measurements */
  gint64 send_pts;
  gint64 recv_pts;
  gint next_latency_idx;
  gint latencies[GST_AUDIOLATENCY_NUM_LATENCIES];

  /* properties */
  gboolean print_latency;
  gint samples_per_buffer;
};

struct _GstAudioLatencyClass
{
  GstBinClass parent_class;
};

GType gst_audiolatency_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (audiolatency);

G_END_DECLS
#endif /* __GST_AUDIOLATENCY_H__ */
