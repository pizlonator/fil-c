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

#ifndef __GST_AVTP_CVF_DEPAY_H__
#define __GST_AVTP_CVF_DEPAY_H__

#include <gst/gst.h>

#include "gstavtpvfdepaybase.h"

G_BEGIN_DECLS
#define GST_TYPE_AVTP_CVF_DEPAY (gst_avtp_cvf_depay_get_type())
#define GST_AVTP_CVF_DEPAY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVTP_CVF_DEPAY,GstAvtpCvfDepay))
#define GST_AVTP_CVF_DEPAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVTP_CVF_DEPAY,GstAvtpCvfDepayClass))
#define GST_IS_AVTP_CVF_DEPAY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVTP_CVF_DEPAY))
#define GST_IS_AVTP_CVF_DEPAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVTP_CVF_DEPAY))
typedef struct _GstAvtpCvfDepay GstAvtpCvfDepay;
typedef struct _GstAvtpCvfDepayClass GstAvtpCvfDepayClass;

struct _GstAvtpCvfDepay
{
  GstAvtpVfDepayBaseClass vfdepayload;

  guint8 seqnum;
  GstBuffer *fragments;
};

struct _GstAvtpCvfDepayClass
{
  GstAvtpVfDepayBaseClass parent_class;
};

GType gst_avtp_cvf_depay_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (avtpcvfdepay);

G_END_DECLS
#endif /* __GST_AVTP_CVF_DEPAY_H__ */
