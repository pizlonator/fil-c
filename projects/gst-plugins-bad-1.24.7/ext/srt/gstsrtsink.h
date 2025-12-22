/* GStreamer
 * Copyright (C) 2018, Collabora Ltd.
 * Copyright (C) 2018, SK Telecom, Co., Ltd.
 *   Author: Jeongseok Kim <jeongseok.kim@sk.com>
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

#ifndef __GST_SRT_SINK_H__
#define __GST_SRT_SINK_H__

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include <gio/gio.h>

#include "gstsrtobject.h"

G_BEGIN_DECLS

#define GST_TYPE_SRT_SINK              (gst_srt_sink_get_type ())
#define GST_IS_SRT_SINK(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_SRT_SINK))
#define GST_IS_SRT_SINK_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_SRT_SINK))
#define GST_SRT_SINK_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_SRT_SINK, GstSRTSinkClass))
#define GST_SRT_SINK(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_SRT_SINK, GstSRTSink))
#define GST_SRT_SINK_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_SRT_SINK, GstSRTSinkClass))
#define GST_SRT_SINK_CAST(obj)         ((GstSRTSink*)(obj))
#define GST_SRT_SINK_CLASS_CAST(klass) ((GstSRTSinkClass*)(klass))

typedef struct _GstSRTSink GstSRTSink;
typedef struct _GstSRTSinkClass GstSRTSinkClass;

struct _GstSRTSink {
  GstBaseSink parent;

  GstBufferList *headers;

  GstSRTObject *srtobject;
};

struct _GstSRTSinkClass {
  GstBaseSinkClass parent_class;

  void (*caller_added)      (GstSRTSink *self, int sock, GSocketAddress * addr);
  void (*caller_removed)    (GstSRTSink *self, int sock, GSocketAddress * addr);
  void (*caller_rejected)   (GstSRTSink *self, GSocketAddress * peer_address,
    const gchar * stream_id, gpointer data);
  gboolean (*caller_connecting) (GstSRTSink *self, GSocketAddress * peer_address,
    const gchar * stream_id, gpointer data);
};

GType gst_srt_sink_get_type (void);

G_END_DECLS

#endif // __GST_SRT_SINK_H__
