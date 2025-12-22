/* GStreamer
 * Copyright (C) 2017 Matthew Waters <matthew@centricular.com>
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

#ifndef __TRANSPORT_SEND_BIN_H__
#define __TRANSPORT_SEND_BIN_H__

#include <gst/gst.h>
#include "transportstream.h"
#include "utils.h"

G_BEGIN_DECLS

GType transport_send_bin_get_type(void);
#define GST_TYPE_WEBRTC_TRANSPORT_SEND_BIN (transport_send_bin_get_type())
#define TRANSPORT_SEND_BIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_WEBRTC_TRANSPORT_SEND_BIN,TransportSendBin))
#define TRANSPORT_SEND_BIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_WEBRTC_TRANSPORT_SEND_BIN,TransportSendBinClass))
#define TRANSPORT_SEND_BIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_WEBRTC_TRANSPORT_SEND_BIN,TransportSendBinClass))

typedef struct _TransportSendBinDTLSContext TransportSendBinDTLSContext;

struct _TransportSendBin
{
  GstBin                     parent;

  GMutex                     lock; /* Lock for managing children and pad blocks */
  gboolean                   active; /* Flag that's cleared on shutdown */

  TransportStream           *stream;        /* parent transport stream */

  GstElement *dtlssrtpenc;
  GstElement *nicesink;

  gboolean has_clientness;

  /* Block on the dtlssrtpenc RTP sink pad, if any */
  struct pad_block          *rtp_block;
  /* Block on the dtlssrtpenc RTCP sink pad, if any */
  struct pad_block          *rtcp_block;
};

struct _TransportSendBinClass
{
  GstBinClass           parent_class;
};

G_END_DECLS

#endif /* __TRANSPORT_SEND_BIN_H__ */
