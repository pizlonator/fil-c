/* 
 * GStreamer
 * Copyright (C) 2010 Jan Schmidt <thaytan@noraisin.net>
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
 
#ifndef __GST_RTMP_SINK_H__
#define __GST_RTMP_SINK_H__

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>

#include <librtmp/rtmp.h>
#include <librtmp/log.h>
#include <librtmp/amf.h>

G_BEGIN_DECLS

#define GST_TYPE_RTMP_SINK \
  (gst_rtmp_sink_get_type())
#define GST_RTMP_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTMP_SINK,GstRTMPSink))
#define GST_RTMP_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RTMP_SINK,GstRTMPSinkClass))
#define GST_IS_RTMP_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTMP_SINK))
#define GST_IS_RTMP_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RTMP_SINK))

typedef struct _GstRTMPSink      GstRTMPSink;
typedef struct _GstRTMPSinkClass GstRTMPSinkClass;

struct _GstRTMPSink {
  GstBaseSink parent;

  /* < private > */
  gchar *uri;

  RTMP *rtmp;
  gchar *rtmp_uri; /* copy of url for librtmp */

  GstBuffer *header;
  gboolean first;
  gboolean have_write_error;
};

struct _GstRTMPSinkClass {
  GstBaseSinkClass parent_class;
};

GType gst_rtmp_sink_get_type (void);


G_END_DECLS

#endif /* __GST_RTMP_SINK_H__ */
