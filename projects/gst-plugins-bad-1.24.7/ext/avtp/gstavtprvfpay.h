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

#ifndef __GST_AVTP_RVF_PAY_H__
#define __GST_AVTP_RVF_PAY_H__

#include <gst/gst.h>

#include "gstavtpvfpaybase.h"

G_BEGIN_DECLS
#define GST_TYPE_AVTP_RVF_PAY (gst_avtp_rvf_pay_get_type())
#define GST_AVTP_RVF_PAY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVTP_RVF_PAY,GstAvtpRvfPay))
#define GST_AVTP_RVF_PAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVTP_RVF_PAY,GstAvtpRvfPayClass))
#define GST_IS_AVTP_RVF_PAY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVTP_RVF_PAY))
#define GST_IS_AVTP_RVF_PAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVTP_RVF_PAY))
typedef struct _GstAvtpRvfPay GstAvtpRvfPay;
typedef struct _GstAvtpRvfPayClass GstAvtpRvfPayClass;

struct _GstAvtpRvfPay
{
  GstAvtpVfPayBase vfbase;

  GstBuffer *header;
  //size of the buffer fragment
  gsize fragment_size;
  //large raster: size of the end of line fragment
  gsize fragment_eol_size;
  //padding bytes appended to the last fragment of the frame
  GstBuffer *fragment_padding;
  //number of lines per fragment
  guint num_lines;
  //size of the single line
  gsize line_size;
  //maximum value of i_seq_num
  guint8 i_seq_max;
};

struct _GstAvtpRvfPayClass
{
  GstAvtpVfPayBaseClass parent_class;
};

GType gst_avtp_rvf_pay_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (avtprvfpay);

G_END_DECLS
#endif /* __GST_AVTP_RVF_PAY_H__ */
