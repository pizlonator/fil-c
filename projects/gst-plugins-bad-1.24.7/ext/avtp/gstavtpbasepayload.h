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

#ifndef __GST_AVTP_BASE_PAYLOAD_H__
#define __GST_AVTP_BASE_PAYLOAD_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_AVTP_BASE_PAYLOAD (gst_avtp_base_payload_get_type())
#define GST_AVTP_BASE_PAYLOAD(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVTP_BASE_PAYLOAD,GstAvtpBasePayload))
#define GST_AVTP_BASE_PAYLOAD_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVTP_BASE_PAYLOAD,GstAvtpBasePayloadClass))
#define GST_IS_AVTP_BASE_PAYLOAD(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVTP_BASE_PAYLOAD))
#define GST_IS_AVTP_BASE_PAYLOAD_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVTP_BASE_PAYLOAD))

typedef struct _GstAvtpBasePayload GstAvtpBasePayload;
typedef struct _GstAvtpBasePayloadClass GstAvtpBasePayloadClass;

struct _GstAvtpBasePayload
{
  GstElement element;

  GstPad *sinkpad;
  GstPad *srcpad;

  guint64 streamid;
  guint mtt;
  guint tu;
  guint64 processing_deadline;

  GstClockTime latency;
  GstSegment segment;
  guint8 seqnum;

  gpointer _gst_reserved[GST_PADDING];
};

struct _GstAvtpBasePayloadClass
{
  GstElementClass parent_class;

  /* Pure virtual function. */
  GstPadChainFunction chain;

  GstPadEventFunction sink_event;

  gpointer _gst_reserved[GST_PADDING];
};

GType gst_avtp_base_payload_get_type (void);

GstClockTime gst_avtp_base_payload_calc_ptime (GstAvtpBasePayload *
    avtpbasepayload, GstBuffer * buffer);

G_END_DECLS

#endif /* __GST_AVTP_BASE_PAYLOAD_H__ */
