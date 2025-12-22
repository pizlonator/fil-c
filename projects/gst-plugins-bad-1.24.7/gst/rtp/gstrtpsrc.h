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

#ifndef __GST_RTP_SRC_H__
#define __GST_RTP_SRC_H__

#include <gio/gio.h>
#include <gst/gst.h>

G_BEGIN_DECLS
#define GST_TYPE_RTP_SRC \
  (gst_rtp_src_get_type())
#define GST_RTP_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_RTP_SRC, GstRtpSrc))
#define GST_RTP_SRC_CAST(obj) \
  ((GstRtpSrc *) obj)
#define GST_RTP_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_RTP_SRC, GstRtpSrcClass))
#define GST_IS_RTP_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_RTP_SRC))
#define GST_IS_RTP_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_RTP_SRC))

typedef struct _GstRtpSrc GstRtpSrc;
typedef struct _GstRtpSrcClass GstRtpSrcClass;

struct _GstRtpSrc
{
  GstBin parent;

  /* Properties */
  GstUri *uri;

  gint ttl;
  gint ttl_mc;
  gchar *encoding_name;
  gchar *multi_iface;
  GstCaps *caps;

  /* Internal elements */
  GstElement *rtpbin;
  GstElement *rtp_src;
  GstElement *rtcp_src;
  GstElement *rtcp_sink;

  gulong rtcp_recv_probe;
  gulong rtcp_send_probe;
  GSocketAddress *rtcp_send_addr;

  GMutex lock;
};

struct _GstRtpSrcClass
{
  GstBinClass parent;
};

GType gst_rtp_src_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (rtpsrc);

G_END_DECLS
#endif /* __GST_RTP_SRC_H__ */
