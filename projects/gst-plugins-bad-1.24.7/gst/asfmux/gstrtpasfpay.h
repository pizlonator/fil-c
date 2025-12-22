/* ASF RTP Payloader plugin for GStreamer
 * Copyright (C) 2009 Thiago Santos <thiagoss@embedded.ufcg.edu.br>
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


#ifndef __GST_RTP_ASF_PAY_H__
#define __GST_RTP_ASF_PAY_H__

#include <gst/gst.h>
#include <gst/rtp/gstrtpbasepayload.h>
#include <gst/rtp/gstrtpbuffer.h>
#include <gst/base/gstadapter.h>

#include "gstasfobjects.h"

G_BEGIN_DECLS
#define GST_TYPE_RTP_ASF_PAY \
  (gst_rtp_asf_pay_get_type())
#define GST_RTP_ASF_PAY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTP_ASF_PAY,GstRtpAsfPay))
#define GST_RTP_ASF_PAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RTP_ASF_PAY,GstRtpAsfPayClass))
#define GST_IS_RTP_ASF_PAY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTP_ASF_PAY))
#define GST_IS_RTP_ASF_PAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RTP_ASF_PAY))
#define GST_RTP_ASF_PAY_CAST(obj) ((GstRtpAsfPay*)(obj))
    enum GstRtpAsfPayState
{
  ASF_NOT_STARTED,
  ASF_DATA_OBJECT,
  ASF_PACKETS,
  ASF_END
};

typedef struct _GstRtpAsfPay GstRtpAsfPay;
typedef struct _GstRtpAsfPayClass GstRtpAsfPayClass;

struct _GstRtpAsfPay
{
  GstRTPBasePayload rtppay;

  enum GstRtpAsfPayState state;

  guint32 first_ts;
  gchar *config;
  guint64 packets_count;
  GstAsfFileInfo asfinfo;

  /* current output buffer */
  GstBuffer *current;
  guint32 cur_off;
  guint32 ts;
  gboolean has_ts;
  gboolean marker;

  /* keeping it here to avoid allocs/frees */
  GstAsfPacketInfo packetinfo;

  GstBuffer *headers;
};

struct _GstRtpAsfPayClass
{
  GstRTPBasePayloadClass parent_class;
};

GType gst_rtp_asf_pay_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (rtpasfpay);

G_END_DECLS
#endif /* __GST_RTP_ASF_PAY_H__ */
