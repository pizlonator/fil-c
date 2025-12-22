/*
 * GStreamer AVTP Plugin
 * Copyright (C) 2019 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later
 * version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 */

#ifndef __GST_AVTP_SINK_H__
#define __GST_AVTP_SINK_H__

#include <gst/base/gstbasesink.h>
#include <gst/gst.h>
#include <linux/if_packet.h>

G_BEGIN_DECLS

#define GST_TYPE_AVTP_SINK (gst_avtp_sink_get_type())
#define GST_AVTP_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVTP_SINK,GstAvtpSink))
#define GST_AVTP_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVTP_SINK,GstAvtpSinkClass))
#define GST_IS_AVTP_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVTP_SINK))
#define GST_IS_AVTP_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVTP_SINK))

typedef struct _GstAvtpSink GstAvtpSink;
typedef struct _GstAvtpSinkClass GstAvtpSinkClass;

struct _GstAvtpSink
{
  GstBaseSink parent;

  gchar * ifname;
  gchar * address;
  gint priority;

  int sk_fd;
  struct sockaddr_ll sk_addr;
  struct msghdr * msg;
};

struct _GstAvtpSinkClass
{
  GstBaseSinkClass parent_class;
};

GType gst_avtp_sink_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (avtpsink);

G_END_DECLS

#endif /* __GST_AVTP_SINK_H__ */
