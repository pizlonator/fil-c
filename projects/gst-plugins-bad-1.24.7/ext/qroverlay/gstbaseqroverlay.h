/*
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (c) 2020 Anthony Violo <anthony.violo@ubicast.eu>
 * Copyright (c) 2020 Thibault Saunier <tsaunier@igalia.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GST_BASE_QR_OVERLAY_H__
#define __GST_BASE_QR_OVERLAY_H__

#include <gst/gst.h>
#include <gst/video/gstvideofilter.h>

G_BEGIN_DECLS
#define GST_TYPE_BASE_QR_OVERLAY (gst_base_qr_overlay_get_type())
G_DECLARE_DERIVABLE_TYPE (GstBaseQROverlay, gst_base_qr_overlay, GST, BASE_QR_OVERLAY, GstVideoFilter);

struct _GstBaseQROverlayClass
{
  GstBinClass parent;

  gchar* (*get_content) (GstBaseQROverlay *self, GstBuffer *buf, GstVideoInfo *info,
    gboolean *reuse_previous);
};

G_END_DECLS
#endif /* __GST_BASE_QR_OVERLAY_H__ */

