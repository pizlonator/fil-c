/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2004-2010  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __GST_AVDTP_SINK_H
#define __GST_AVDTP_SINK_H

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include "gstavdtputil.h"

G_BEGIN_DECLS
#define GST_TYPE_AVDTP_SINK \
	(gst_avdtp_sink_get_type())
#define GST_AVDTP_SINK(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVDTP_SINK,\
		GstAvdtpSink))
#define GST_AVDTP_SINK_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVDTP_SINK,\
		GstAvdtpSinkClass))
#define GST_IS_AVDTP_SINK(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVDTP_SINK))
#define GST_IS_AVDTP_SINK_CLASS(obj) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVDTP_SINK))
typedef struct _GstAvdtpSink GstAvdtpSink;
typedef struct _GstAvdtpSinkClass GstAvdtpSinkClass;

struct _GstAvdtpSink
{
  GstBaseSink sink;

  GstAvdtpConnection conn;

  gboolean autoconnect;

  /* mp3 stream data (outside caps data) */
  gint mp3_using_crc;
  gint channel_mode;

  /* stream connection data */
  GstCaps *stream_caps;

  GstCaps *dev_caps;

  GMutex sink_lock;

  guint watch_id;
};

struct _GstAvdtpSinkClass
{
  GstBaseSinkClass parent_class;
};

GType gst_avdtp_sink_get_type (void);

GstCaps *gst_avdtp_sink_get_device_caps (GstAvdtpSink * sink);

guint gst_avdtp_sink_get_link_mtu (GstAvdtpSink * sink);

void gst_avdtp_sink_set_device (GstAvdtpSink * sink, const gchar * device);

void gst_avdtp_sink_set_transport (GstAvdtpSink * sink,
    const gchar * transport);

gchar *gst_avdtp_sink_get_device (GstAvdtpSink * sink);

gchar *gst_avdtp_sink_get_transport (GstAvdtpSink * sink);

void gst_avdtp_sink_set_crc (GstAvdtpSink * self, gboolean crc);

void gst_avdtp_sink_set_channel_mode (GstAvdtpSink * self, const gchar * mode);


G_END_DECLS
#endif /* __GST_AVDTP_SINK_H */
