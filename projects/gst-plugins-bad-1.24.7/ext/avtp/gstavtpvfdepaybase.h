/*
 * GStreamer AVTP Plugin
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

#ifndef __GST_AVTP_VF_DEPAY_BASE_H__
#define __GST_AVTP_VF_DEPAY_BASE_H__

#include <gst/gst.h>

#include "gstavtpbasedepayload.h"

G_BEGIN_DECLS
#define GST_TYPE_AVTP_VF_DEPAY_BASE (gst_avtp_vf_depay_base_get_type())
#define GST_AVTP_VF_DEPAY_BASE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVTP_VF_DEPAY_BASE,GstAvtpVfDepayBase))
#define GST_AVTP_VF_DEPAY_BASE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVTP_VF_DEPAY_BASE,GstAvtpVfDepayBaseClass))
#define GST_IS_AVTP_VF_DEPAY_BASE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVTP_VF_DEPAY_BASE))
#define GST_IS_AVTP_VF_DEPAY_BASE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVTP_VF_DEPAY_BASE))
#define GST_AVTP_VF_DEPAY_BASE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_AVTP_VF_DEBAYPAY_BASE, GstAvtpVfDepayBaseClass))
typedef struct _GstAvtpVfDepayBase GstAvtpVfDepayBase;
typedef struct _GstAvtpVfDepayBaseClass GstAvtpVfDepayBaseClass;

typedef gboolean (*GstAvtpDepayVdDepayPushCapsFunction) (GstAvtpVfDepayBase *
    avtpvfdepay);

struct _GstAvtpVfDepayBase
{

  GstAvtpBaseDepayload depayload;

  GstBuffer *out_buffer;
};

struct _GstAvtpVfDepayBaseClass
{
  GstAvtpBaseDepayloadClass parent_class;

  /* Pure virtual function. */
  GstAvtpDepayVdDepayPushCapsFunction depay_push_caps;
};

GType gst_avtp_vf_depay_base_get_type (void);

GstFlowReturn gst_avtp_vf_depay_base_push (GstAvtpVfDepayBase *
    avtpvfdepaybase);

G_END_DECLS
#endif /* __GST_AVTP_VF_DEPAY_BASE_H__ */
