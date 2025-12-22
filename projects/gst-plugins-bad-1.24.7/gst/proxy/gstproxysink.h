/*
 * Copyright (C) 2015 Centricular Ltd.
 *   Author: Sebastian Dröge <sebastian@centricular.com>
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

#ifndef __GST_PROXY_SINK_H__
#define __GST_PROXY_SINK_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_PROXY_SINK             (gst_proxy_sink_get_type())
#define GST_PROXY_SINK(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_PROXY_SINK, GstProxySink))
#define GST_IS_PROXY_SINK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_PROXY_SINK))
#define GST_PROXY_SINK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass) , GST_TYPE_PROXY_SINK, GstProxySinkClass))
#define GST_IS_PROXY_SINK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass) , GST_TYPE_PROXY_SINK))
#define GST_PROXY_SINK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj) , GST_TYPE_PROXY_SINK, GstProxySinkClass))

typedef struct _GstProxySink GstProxySink;
typedef struct _GstProxySinkClass GstProxySinkClass;
typedef struct _GstProxySinkPrivate GstProxySinkPrivate;

struct _GstProxySink {
  GstElement parent;

  /* < private > */
  GstPad *sinkpad;

  /* The proxysrc that we push events, buffers, queries to */
  GWeakRef proxysrc;

  /* Whether there are sticky events pending */
  gboolean pending_sticky_events;
  gboolean sent_stream_start;
  gboolean sent_caps;
};

struct _GstProxySinkClass {
  GstElementClass parent_class;
};

GType gst_proxy_sink_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (proxysink);

G_END_DECLS

#endif /* __GST_PROXY_SINK_H__ */
