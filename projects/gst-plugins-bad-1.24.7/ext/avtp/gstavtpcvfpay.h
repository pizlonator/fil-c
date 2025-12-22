/*
 * GStreamer AVTP Plugin
 * Copyright (C) 2019 Intel Corporation
 * Copyright (c) 2021, Fastree3D
 * Adrian Fiergolski <Adrian.Fiergolski@fastree3d.com>
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

#ifndef __GST_AVTP_CVF_PAY_H__
#define __GST_AVTP_CVF_PAY_H__

#include <gst/gst.h>

#include "gstavtpvfpaybase.h"

G_BEGIN_DECLS
#define GST_TYPE_AVTP_CVF_PAY (gst_avtp_cvf_pay_get_type())
#define GST_AVTP_CVF_PAY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVTP_CVF_PAY,GstAvtpCvfPay))
#define GST_AVTP_CVF_PAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVTP_CVF_PAY,GstAvtpCvfPayClass))
#define GST_IS_AVTP_CVF_PAY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVTP_CVF_PAY))
#define GST_IS_AVTP_CVF_PAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVTP_CVF_PAY))
typedef struct _GstAvtpCvfPay GstAvtpCvfPay;
typedef struct _GstAvtpCvfPayClass GstAvtpCvfPayClass;

struct _GstAvtpCvfPay
{
  GstAvtpVfPayBase vfbase;

  GstBuffer *header;

  /* H.264 specific information */
  guint8 nal_length_size;
};

struct _GstAvtpCvfPayClass
{
  GstAvtpVfPayBaseClass parent_class;
};

GType gst_avtp_cvf_pay_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (avtpcvfpay);

G_END_DECLS
#endif /* __GST_AVTP_CVF_PAY_H__ */
