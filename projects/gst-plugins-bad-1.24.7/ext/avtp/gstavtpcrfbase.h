/*
 * GStreamer AVTP Plugin
 * Copyright (C) 2019 Intel Corporation
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

#ifndef __GST_AVTP_CRF_BASE_H__
#define __GST_AVTP_CRF_BASE_H__

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <linux/if_packet.h>

G_BEGIN_DECLS
#define GST_TYPE_AVTP_CRF_BASE (gst_avtp_crf_base_get_type())
#define GST_AVTP_CRF_BASE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVTP_CRF_BASE,GstAvtpCrfBase))
#define GST_AVTP_CRF_BASE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVTP_CRF_BASE,GstAvtpCrfBaseClass))
#define GST_IS_AVTP_CRF_BASE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVTP_CRF_BASE))
#define GST_IS_AVTP_CRF_BASE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVTP_CRF_BASE))
typedef struct _GstAvtpCrfBase GstAvtpCrfBase;
typedef struct _GstAvtpCrfBaseClass GstAvtpCrfBaseClass;
typedef struct _GstAvtpCrfThreadData GstAvtpCrfThreadData;

struct _GstAvtpCrfThreadData
{
  GThread *thread;
  gboolean is_running;

  guint64 num_pkt_tstamps;
  GstClockTime timestamp_interval;
  guint64 base_freq;
  guint64 pull;
  guint64 type;
  guint64 mr;

  gdouble *past_periods;
  int past_periods_iter;
  int periods_stored;
  /** The time in ns between two events. The type of the event is depending on
   *  the CRF type: Audio sample, video frame sync, video line sync, ...
   */
  gdouble average_period;
  GstClockTime current_ts;
  GstClockTime last_received_tstamp;
  guint64 last_seqnum;
};

struct _GstAvtpCrfBase
{
  GstBaseTransform element;

  guint64 streamid;
  gchar *ifname;
  gchar *address;

  GstAvtpCrfThreadData thread_data;
};

struct _GstAvtpCrfBaseClass
{
  GstBaseTransformClass parent_class;

  gpointer _gst_reserved[GST_PADDING];
};

GType gst_avtp_crf_base_get_type (void);

G_END_DECLS
#endif /* __GST_AVTP_CRF_BASE_H__ */
