/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_CODEC_TIMESTAMPER             (gst_codec_timestamper_get_type())
#define GST_CODEC_TIMESTAMPER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CODEC_TIMESTAMPER,GstCodecTimestamper))
#define GST_CODEC_TIMESTAMPER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CODEC_TIMESTAMPER,GstCodecTimestamperClass))
#define GST_CODEC_TIMESTAMPER_GET_CLASS(obj)   (GST_CODEC_TIMESTAMPER_CLASS(G_OBJECT_GET_CLASS(obj)))
#define GST_CODEC_TIMESTAMPER_CAST(obj)        ((GstCodecTimestamper*)(obj))

typedef struct _GstCodecTimestamper GstCodecTimestamper;
typedef struct _GstCodecTimestamperClass GstCodecTimestamperClass;
typedef struct _GstCodecTimestamperPrivate GstCodecTimestamperPrivate;

struct _GstCodecTimestamper
{
  GstElement parent;

  GstPad *sinkpad;
  GstPad *srcpad;

  GstCodecTimestamperPrivate *priv;
};

struct _GstCodecTimestamperClass
{
  GstElementClass parent_class;

  gboolean      (*start)         (GstCodecTimestamper * timestamper);

  gboolean      (*stop)          (GstCodecTimestamper * timestamper);

  gboolean      (*set_caps)      (GstCodecTimestamper * timestamper,
                                  GstCaps * caps);

  GstCaps *     (*get_sink_caps) (GstCodecTimestamper * timestamper,
                                  GstCaps      * filter);

  GstFlowReturn (*handle_buffer) (GstCodecTimestamper * timestamper,
                                  GstBuffer * buffer);
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GstCodecTimestamper, gst_object_unref);

GType gst_codec_timestamper_get_type (void);

void  gst_codec_timestamper_set_window_size (GstCodecTimestamper * timestamper,
                                             guint window_size);

G_END_DECLS
