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

#ifndef __GST_AVTP_RVF_DEPAY_H__
#define __GST_AVTP_RVF_DEPAY_H__

#include <gst/gst.h>

#include "gstavtpvfdepaybase.h"

G_BEGIN_DECLS
#define GST_TYPE_AVTP_RVF_DEPAY (gst_avtp_rvf_depay_get_type())
#define GST_AVTP_RVF_DEPAY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVTP_RVF_DEPAY,GstAvtpRvfDepay))
#define GST_AVTP_RVF_DEPAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVTP_RVF_DEPAY,GstAvtpRvfDepayClass))
#define GST_IS_AVTP_RVF_DEPAY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVTP_RVF_DEPAY))
#define GST_IS_AVTP_RVF_DEPAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVTP_RVF_DEPAY))
typedef struct _GstAvtpRvfDepay GstAvtpRvfDepay;
typedef struct _GstAvtpRvfDepayClass GstAvtpRvfDepayClass;

struct _GstAvtpRvfDepay
{
  GstAvtpVfDepayBaseClass vfdepayload;

  guint8 seqnum;

  gboolean format_fixed;

  guint16 active_pixels;
  guint16 total_lines;
  guint16 stream_data_length;
  gboolean pd;
  guint8 pixel_depth;
  guint8 pixel_format;
  guint8 frame_rate;
  guint8 colorspace;

  gsize line_size;
  gsize fragment_size;
  gsize fragment_eol_size;
  guint8 i_seq_max;
};

struct _GstAvtpRvfDepayClass
{
  GstAvtpVfDepayBaseClass parent_class;
};

GType gst_avtp_rvf_depay_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (avtprvfdepay);

G_END_DECLS
#endif /* __GST_AVTP_RVF_DEPAY_H__ */
