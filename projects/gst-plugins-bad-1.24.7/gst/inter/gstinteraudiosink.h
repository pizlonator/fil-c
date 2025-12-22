/* GStreamer
 * Copyright (C) 2011 David A. Schleef <ds@schleef.org>
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

#ifndef _GST_INTER_AUDIO_SINK_H_
#define _GST_INTER_AUDIO_SINK_H_

#include <gst/base/gstbasesink.h>
#include "gstintersurface.h"

G_BEGIN_DECLS

#define GST_TYPE_INTER_AUDIO_SINK   (gst_inter_audio_sink_get_type())
#define GST_INTER_AUDIO_SINK(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_INTER_AUDIO_SINK,GstInterAudioSink))
#define GST_INTER_AUDIO_SINK_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_INTER_AUDIO_SINK,GstInterAudioSinkClass))
#define GST_IS_INTER_AUDIO_SINK(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_INTER_AUDIO_SINK))
#define GST_IS_INTER_AUDIO_SINK_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_INTER_AUDIO_SINK))

typedef struct _GstInterAudioSink GstInterAudioSink;
typedef struct _GstInterAudioSinkClass GstInterAudioSinkClass;

struct _GstInterAudioSink
{
  GstBaseSink base_interaudiosink;

  GstInterSurface *surface;
  char *channel;

  GstAdapter *input_adapter;
  GstAudioInfo info;
};

struct _GstInterAudioSinkClass
{
  GstBaseSinkClass base_interaudiosink_class;
};

GType gst_inter_audio_sink_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (interaudiosink);

G_END_DECLS

#endif
