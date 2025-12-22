/* GStreamer
 * Copyright (C) 2016 Sebastian Dröge <sebastian@centricular.com>
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

#ifndef __GST_SDP_SRC_H__
#define __GST_SDP_SRC_H__

#include <gst/gst.h>

G_BEGIN_DECLS
#define GST_TYPE_SDP_SRC \
  (gst_sdp_src_get_type())
#define GST_SDP_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_SDP_SRC, GstSdpSrc))
#define GST_SDP_SRC_CAST(obj) \
  ((GstSdpSrc *) obj)
#define GST_SDP_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_SDP_SRC, GstSdpSrcClass))
#define GST_IS_SDP_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_SDP_SRC))
#define GST_IS_SDP_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_SDP_SRC))

typedef struct _GstSdpSrc GstSdpSrc;
typedef struct _GstSdpSrcClass GstSdpSrcClass;

struct _GstSdpSrc
{
  GstBin parent;

  gchar *location;
  gchar *sdp;

  GstBuffer *sdp_buffer;
  GstElement *src;
  GstElement *demux;
};

struct _GstSdpSrcClass
{
  GstBinClass parent;
};

GType gst_sdp_src_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (sdpsrc);

G_END_DECLS
#endif /* __GST_SDP_SRC_H__ */
