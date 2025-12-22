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

#ifndef __GST_AVTP_PAY_BASE_H__
#define __GST_AVTP_PAY_BASE_H__

#include <gst/gst.h>

#include "gstavtpbasepayload.h"

G_BEGIN_DECLS
#define GST_TYPE_AVTP_VF_PAY_BASE (gst_avtp_vf_pay_base_get_type())
#define GST_AVTP_VF_PAY_BASE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVTP_VF_PAY_BASE,GstAvtpVfPayBase))
#define GST_AVTP_VF_PAY_BASE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVTP_VF_PAY_BASE,GstAvtpVfPayBaseClass))
#define GST_IS_AVTP_VF_PAY_BASE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVTP_VF_PAY_BASE))
#define GST_IS_AVTP_VF_PAY_BASE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVTP_VF_PAY_BASE))
#define GST_AVTP_VF_PAY_BASE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_AVTP_VF_PAY_BASE, GstAvtpVfPayBaseClass))
typedef struct _GstAvtpVfPayBase GstAvtpVfPayBase;
typedef struct _GstAvtpVfPayBaseClass GstAvtpVfPayBaseClass;

typedef gboolean (*GstAvtpVfPayNewCapsFunction) (GstAvtpVfPayBase *
    avtpvfpaybase, GstCaps * caps);
typedef gboolean (*GstAvtpVfPayPrepareAvtpPacketsFunction) (GstAvtpVfPayBase *
    avtprvfpaybase, GstBuffer * buffer, GPtrArray * avtp_packets);

struct _GstAvtpVfPayBase
{
  GstAvtpBasePayload payload;

  guint mtu;
  guint64 measurement_interval;
  guint max_interval_frames;
  guint64 last_interval_ct;
};

struct _GstAvtpVfPayBaseClass
{
  GstAvtpBasePayloadClass parent_class;

  /* Pure virtual function. */
  GstAvtpVfPayNewCapsFunction new_caps;
  GstAvtpVfPayPrepareAvtpPacketsFunction prepare_avtp_packets;
};

GType gst_avtp_vf_pay_base_get_type (void);

G_END_DECLS
#endif /* __GST_AVTP_PAY_BASE_H__ */
