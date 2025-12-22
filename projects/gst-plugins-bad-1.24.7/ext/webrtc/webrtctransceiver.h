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

#ifndef __WEBRTC_TRANSCEIVER_H__
#define __WEBRTC_TRANSCEIVER_H__

#include "fwd.h"
#include <gst/webrtc/rtptransceiver.h>
#include "gst/webrtc/webrtc-priv.h"
#include "transportstream.h"

G_BEGIN_DECLS

GType webrtc_transceiver_get_type(void);
#define WEBRTC_TYPE_TRANSCEIVER            (webrtc_transceiver_get_type())
#define WEBRTC_TRANSCEIVER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),WEBRTC_TYPE_TRANSCEIVER,WebRTCTransceiver))
#define WEBRTC_IS_TRANSCEIVER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),WEBRTC_TYPE_TRANSCEIVER))
#define WEBRTC_TRANSCEIVER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,WEBRTC_TYPE_TRANSCEIVER,WebRTCTransceiverClass))
#define WEBRTC_TRANSCEIVER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,WEBRTC_TYPE_TRANSCEIVER,WebRTCTransceiverClass))

struct _WebRTCTransceiver
{
  GstWebRTCRTPTransceiver   parent;

  TransportStream          *stream;
  GstStructure             *local_rtx_ssrc_map;
  GstEvent                 *tos_event;

  /* Properties */
  GstWebRTCFECType         fec_type;
  guint                    fec_percentage;
  gboolean                 do_nack;

  /* The last caps that we put into to a SDP media section */
  GstCaps                  *last_retrieved_caps;
  /* The last caps that we successfully configured from a valid
   * set_local/remote description call.
   */
  GstCaps                  *last_send_configured_caps;

  gchar                    *pending_mid;

  gboolean                 mline_locked;

  GstElement               *ulpfecdec;
  GstElement               *ulpfecenc;
  GstElement               *redenc;
};

struct _WebRTCTransceiverClass
{
  GstWebRTCRTPTransceiverClass      parent_class;
};

WebRTCTransceiver *       webrtc_transceiver_new            (GstWebRTCBin * webrtc,
                                                             GstWebRTCRTPSender * sender,
                                                             GstWebRTCRTPReceiver * receiver);

void                      webrtc_transceiver_set_transport  (WebRTCTransceiver * trans,
                                                             TransportStream * stream);

GstWebRTCDTLSTransport *  webrtc_transceiver_get_dtls_transport (GstWebRTCRTPTransceiver * trans);

G_END_DECLS

#endif /* __WEBRTC_TRANSCEIVER_H__ */
