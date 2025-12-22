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

#ifndef __GST_AVTP_BASE_DEPAYLOAD_H__
#define __GST_AVTP_BASE_DEPAYLOAD_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_AVTP_BASE_DEPAYLOAD (gst_avtp_base_depayload_get_type())
#define GST_AVTP_BASE_DEPAYLOAD(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVTP_BASE_DEPAYLOAD,GstAvtpBaseDepayload))
#define GST_AVTP_BASE_DEPAYLOAD_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVTP_BASE_DEPAYLOAD,GstAvtpBaseDepayloadClass))
#define GST_IS_AVTP_BASE_DEPAYLOAD(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVTP_BASE_DEPAYLOAD))
#define GST_IS_AVTP_BASE_DEPAYLOAD_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVTP_BASE_DEPAYLOAD))

typedef struct _GstAvtpBaseDepayload GstAvtpBaseDepayload;
typedef struct _GstAvtpBaseDepayloadClass GstAvtpBaseDepayloadClass;

struct _GstAvtpBaseDepayload
{
  GstElement element;

  GstPad *sinkpad;
  GstPad *srcpad;

  guint64 streamid;

  GstClockTime prev_ptime;
  guint8 seqnum;

  gpointer _gst_reserved[GST_PADDING];
};

struct _GstAvtpBaseDepayloadClass
{
  GstElementClass parent_class;

  /* Pure virtual function. */
  GstPadChainFunction chain;

  GstPadEventFunction sink_event;

  gpointer _gst_reserved[GST_PADDING];
};

GType gst_avtp_base_depayload_get_type (void);

GstClockTime gst_avtp_base_depayload_tstamp_to_ptime (GstAvtpBaseDepayload *
    avtpbasedepayload, guint32 tstamp, GstClockTime ref);

gboolean gst_avtp_base_depayload_push_segment_event (GstAvtpBaseDepayload *
    avtpbasedepayload, guint32 avtp_tstamp);

G_END_DECLS

#endif /* __GST_AVTP_BASE_DEPAYLOAD_H__ */
