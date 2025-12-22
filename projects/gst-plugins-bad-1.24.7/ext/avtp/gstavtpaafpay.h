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

#ifndef __GST_AVTP_AAF_PAY_H__
#define __GST_AVTP_AAF_PAY_H__

#include <gst/gst.h>

#include "gstavtpbasepayload.h"

G_BEGIN_DECLS

#define GST_TYPE_AVTP_AAF_PAY (gst_avtp_aaf_pay_get_type())
#define GST_AVTP_AAF_PAY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVTP_AAF_PAY,GstAvtpAafPay))
#define GST_AVTP_AAF_PAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVTP_AAF_PAY,GstAvtpAafPayClass))
#define GST_IS_AVTP_AAF_PAY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVTP_AAF_PAY))
#define GST_IS_AVTP_AAF_PAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVTP_AAF_PAY))

typedef struct _GstAvtpAafPay GstAvtpAafPay;
typedef struct _GstAvtpAafPayClass GstAvtpAafPayClass;
typedef enum _GstAvtpAafTimestampMode GstAvtpAafTimestampMode;

enum _GstAvtpAafTimestampMode {
  GST_AVTP_AAF_TIMESTAMP_MODE_NORMAL,
  GST_AVTP_AAF_TIMESTAMP_MODE_SPARSE,
};

struct _GstAvtpAafPay
{
  GstAvtpBasePayload payload;

  GstAvtpAafTimestampMode timestamp_mode;

  GstMemory *header;
  gint channels;
  gint depth;
  gint rate;
  gint format;
};

struct _GstAvtpAafPayClass
{
  GstAvtpBasePayloadClass parent_class;
};

GType gst_avtp_aaf_pay_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (avtpaafpay);

G_END_DECLS

#endif /* __GST_AVTP_AAF_PAY_H__ */
