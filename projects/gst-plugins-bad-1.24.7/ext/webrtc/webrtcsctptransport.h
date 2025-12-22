/* GStreamer
 * Copyright (C) 2018 Matthew Waters <matthew@centricular.com>
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

#ifndef __WEBRTC_SCTP_TRANSPORT_H__
#define __WEBRTC_SCTP_TRANSPORT_H__

#include <gst/gst.h>
#include <gst/webrtc/webrtc.h>
#include <gst/webrtc/sctptransport.h>
#include "fwd.h"

#include "gst/webrtc/webrtc-priv.h"

G_BEGIN_DECLS

GType webrtc_sctp_transport_get_type(void);
#define TYPE_WEBRTC_SCTP_TRANSPORT            (webrtc_sctp_transport_get_type())
#define WEBRTC_SCTP_TRANSPORT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),TYPE_WEBRTC_SCTP_TRANSPORT,WebRTCSCTPTransport))
#define WEBRTC_IS_SCTP_TRANSPORT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),TYPE_WEBRTC_SCTP_TRANSPORT))
#define WEBRTC_SCTP_TRANSPORT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,TYPE_WEBRTC_SCTP_TRANSPORT,WebRTCSCTPTransportClass))
#define WEBRTC_SCTP_IS_TRANSPORT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,TYPE_WEBRTC_SCTP_TRANSPORT))
#define WEBRTC_SCTP_TRANSPORT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,TYPE_WEBRTC_SCTP_TRANSPORT,WebRTCSCTPTransportClass))

typedef struct _WebRTCSCTPTransport WebRTCSCTPTransport;
typedef struct _WebRTCSCTPTransportClass WebRTCSCTPTransportClass;

struct _WebRTCSCTPTransport
{
  GstWebRTCSCTPTransport        parent;

  GstWebRTCDTLSTransport       *transport;
  GstWebRTCSCTPTransportState   state;
  guint64                       max_message_size;
  guint                         max_channels;

  gboolean                      association_established;

  gulong                        sctpdec_block_id;
  GstElement                   *sctpdec;
  GstElement                   *sctpenc;

  GstWebRTCBin                 *webrtcbin;
};

struct _WebRTCSCTPTransportClass
{
  GstWebRTCSCTPTransportClass   parent_class;
};

WebRTCSCTPTransport *           webrtc_sctp_transport_new               (void);

void
webrtc_sctp_transport_set_priority (WebRTCSCTPTransport *sctp,
                                    GstWebRTCPriorityType priority);

G_END_DECLS

#endif /* __WEBRTC_SCTP_TRANSPORT_H__ */
