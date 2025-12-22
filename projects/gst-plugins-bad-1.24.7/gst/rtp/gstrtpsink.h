/* GStreamer
 * Copyright (C) 2019 Marc Leeman <marc.leeman@gmail.com>
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

#ifndef __GST_RTP_SINK_H__
#define __GST_RTP_SINK_H__

#include <gst/gst.h>

G_BEGIN_DECLS
#define GST_TYPE_RTP_SINK \
  (gst_rtp_sink_get_type())
#define GST_RTP_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_RTP_SINK, GstRtpSink))
#define GST_RTP_SINK_CAST(obj) \
  ((GstRtpSink *) obj)
#define GST_RTP_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_RTP_SINK, GstRtpSinkClass))
#define GST_IS_RTP_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_RTP_SINK))
#define GST_IS_RTP_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_RTP_SINK))

typedef struct _GstRtpSink GstRtpSink;
typedef struct _GstRtpSinkClass GstRtpSinkClass;

struct _GstRtpSink
{
  GstBin parent;

  /* Properties */
  GstUri *uri;
  gint ttl;
  gint ttl_mc;
  gchar *multi_iface;

  /* Internal elements */
  GstElement *rtpbin;
  GstElement *funnel_rtp;
  GstElement *funnel_rtcp;
  GstElement *rtp_sink;
  GstElement *rtcp_src;
  GstElement *rtcp_sink;

  GMutex lock;
};

struct _GstRtpSinkClass
{
  GstBinClass parent;
};

GType gst_rtp_sink_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (rtpsink);

G_END_DECLS
#endif /* __GST_RTP_SINK_H__ */
