/*
 * GStreamer
 * Copyright (C) 2017 Collabora Inc.
 * Copyright (C) 2021 Igalia S.L.
 *   Author: Philippe Normand <philn@igalia.com>
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

#ifndef _GST_FAKE_AUDIO_SINK_H_
#define _GST_FAKE_AUDIO_SINK_H_

#include <gst/gst.h>

#define GST_TYPE_FAKE_AUDIO_SINK \
  (gst_fake_audio_sink_get_type())
#define GST_FAKE_AUDIO_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_FAKE_AUDIO_SINK, GstFakeAudioSink))
#define GST_FAKE_AUDIO_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_FAKE_AUDIO_SINK, GstFakeAudioSinkClass))
#define GST_FAKE_AUDIO_SINK_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_FAKE_AUDIO_SINK, GstFakeAudioSinkClass))
#define GST_IS_FAKE_AUDIO_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_FAKE_AUDIO_SINK))
#define GST_IS_FAKE_AUDIO_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_FAKE_AUDIO_SINK))

G_BEGIN_DECLS

typedef struct _GstFakeAudioSink GstFakeAudioSink;
typedef struct _GstFakeAudioSinkClass GstFakeAudioSinkClass;

struct _GstFakeAudioSink
{
    GstBin parent;
    GstElement *child;
    GstPad *sinkpad;
    gdouble volume;
    gboolean mute;
};

struct _GstFakeAudioSinkClass
{
    GstBinClass parent;

    /* signals */
    void (*handoff) (GstElement *element, GstBuffer *buf, GstPad *pad);
    void (*preroll_handoff) (GstElement *element, GstBuffer *buf, GstPad *pad);
};

GType gst_fake_audio_sink_get_type (void);

G_END_DECLS

#endif
