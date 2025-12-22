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

#ifndef __GST_WEBRTC_PRIV_H__
#define __GST_WEBRTC_PRIV_H__

#include <gst/gst.h>
#include <gst/webrtc/webrtc_fwd.h>
#include <gst/webrtc/rtpsender.h>
#include <gst/webrtc/rtpreceiver.h>

G_BEGIN_DECLS

/**
 * GstWebRTCRTPTransceiver:
 * @mline: the mline number this transceiver corresponds to
 * @mid: The media ID of the m-line associated with this
 * transceiver. This association is established, when possible,
 * whenever either a local or remote description is applied. This
 * field is NULL if neither a local or remote description has been
 * applied, or if its associated m-line is rejected by either a remote
 * offer or any answer.
 * @stopped: Indicates whether or not sending and receiving using the paired
 * #GstWebRTCRTPSender and #GstWebRTCRTPReceiver has been permanently disabled,
 * either due to SDP offer/answer
 * @sender: The #GstWebRTCRTPSender object responsible sending  data to the
 * remote peer
 * @receiver: The #GstWebRTCRTPReceiver object responsible for receiver data from
 * the remote peer.
 * @direction: The transceiver's desired direction.
 * @current_direction: The transceiver's current direction (read-only)
 * @codec_preferences: A caps representing the codec preferences (read-only)
 * @kind: Type of media (Since: 1.20)
 *
 * Mostly matches the WebRTC RTCRtpTransceiver interface.
 */
/**
 * GstWebRTCRTPTransceiver.kind:
 *
 * Type of media
 *
 * Since: 1.20
 */
struct _GstWebRTCRTPTransceiver
{
  GstObject                         parent;
  guint                             mline;
  gchar                            *mid;
  gboolean                          stopped;

  GstWebRTCRTPSender               *sender;
  GstWebRTCRTPReceiver             *receiver;

  GstWebRTCRTPTransceiverDirection  direction;
  GstWebRTCRTPTransceiverDirection  current_direction;

  GstCaps                          *codec_preferences;
  GstWebRTCKind                     kind;

  gpointer                          _padding[GST_PADDING];
};

struct _GstWebRTCRTPTransceiverClass
{
  GstObjectClass        parent_class;

  /* FIXME; reset */
  gpointer              _padding[GST_PADDING];
};

/**
 * GstWebRTCRTPSender:
 * @transport: The transport for RTP packets
 * @send_encodings: Unused
 * @priority: The priority of the stream (Since: 1.20)
 *
 * An object to track the sending aspect of the stream
 *
 * Mostly matches the WebRTC RTCRtpSender interface.
 */
/**
 * GstWebRTCRTPSender.priority:
 *
 * The priority of the stream
 *
 * Since: 1.20
 */
struct _GstWebRTCRTPSender
{
  GstObject                          parent;

  /* The MediStreamTrack is represented by the stream and is output into @transport as necessary */
  GstWebRTCDTLSTransport            *transport;

  GArray                            *send_encodings;
  GstWebRTCPriorityType              priority;

  gpointer                          _padding[GST_PADDING];
};

struct _GstWebRTCRTPSenderClass
{
  GstObjectClass        parent_class;

  gpointer              _padding[GST_PADDING];
};

GST_WEBRTC_API
GstWebRTCRTPSender *        gst_webrtc_rtp_sender_new                   (void);

/**
 * GstWebRTCRTPReceiver:
 * @transport: The transport for RTP packets
 *
 * An object to track the receiving aspect of the stream
 *
 * Mostly matches the WebRTC RTCRtpReceiver interface.
 */
struct _GstWebRTCRTPReceiver
{
  GstObject                          parent;

  /* The MediStreamTrack is represented by the stream and is output into @transport as necessary */
  GstWebRTCDTLSTransport            *transport;

  gpointer                          _padding[GST_PADDING];
};

struct _GstWebRTCRTPReceiverClass
{
  GstObjectClass            parent_class;

  gpointer                  _padding[GST_PADDING];
};

GST_WEBRTC_API
GstWebRTCRTPReceiver *      gst_webrtc_rtp_receiver_new                 (void);

/**
 * GstWebRTCDTLSTransport:
 */
struct _GstWebRTCDTLSTransport
{
  GstObject                          parent;

  GstWebRTCICETransport             *transport;
  GstWebRTCDTLSTransportState        state;

  gboolean                           client;
  guint                              session_id;
  GstElement                        *dtlssrtpenc;
  GstElement                        *dtlssrtpdec;

  gpointer                          _padding[GST_PADDING];
};

struct _GstWebRTCDTLSTransportClass
{
  GstObjectClass               parent_class;

  gpointer                  _padding[GST_PADDING];
};

GST_WEBRTC_API
GstWebRTCDTLSTransport *    gst_webrtc_dtls_transport_new               (guint session_id);

GST_WEBRTC_API
void                        gst_webrtc_dtls_transport_set_transport     (GstWebRTCDTLSTransport * transport,
                                                                         GstWebRTCICETransport * ice);

#define GST_WEBRTC_DATA_CHANNEL_LOCK(channel) g_mutex_lock(&((GstWebRTCDataChannel *)(channel))->lock)
#define GST_WEBRTC_DATA_CHANNEL_UNLOCK(channel) g_mutex_unlock(&((GstWebRTCDataChannel *)(channel))->lock)

/**
 * GstWebRTCDataChannel:
 *
 * Since: 1.18
 */
struct _GstWebRTCDataChannel
{
  GObject                           parent;

  GMutex                            lock;

  gchar                            *label;
  gboolean                          ordered;
  guint                             max_packet_lifetime;
  guint                             max_retransmits;
  gchar                            *protocol;
  gboolean                          negotiated;
  gint                              id;
  GstWebRTCPriorityType             priority;
  GstWebRTCDataChannelState         ready_state;
  guint64                           buffered_amount;
  guint64                           buffered_amount_low_threshold;

  gpointer                         _padding[GST_PADDING];
};

/**
 * GstWebRTCDataChannelClass:
 *
 * Since: 1.18
 */
struct _GstWebRTCDataChannelClass
{
  GObjectClass        parent_class;

  gboolean          (*send_data)   (GstWebRTCDataChannel * channel, GBytes *data, GError ** error);
  gboolean          (*send_string) (GstWebRTCDataChannel * channel, const gchar *str, GError ** error);
  void              (*close)       (GstWebRTCDataChannel * channel);

  gpointer           _padding[GST_PADDING];
};

GST_WEBRTC_API
void gst_webrtc_data_channel_on_open (GstWebRTCDataChannel * channel);

GST_WEBRTC_API
void gst_webrtc_data_channel_on_close (GstWebRTCDataChannel * channel);

GST_WEBRTC_API
void gst_webrtc_data_channel_on_error (GstWebRTCDataChannel * channel, GError * error);

GST_WEBRTC_API
void gst_webrtc_data_channel_on_message_data (GstWebRTCDataChannel * channel, GBytes * data);

GST_WEBRTC_API
void gst_webrtc_data_channel_on_message_string (GstWebRTCDataChannel * channel, const gchar * str);

GST_WEBRTC_API
void gst_webrtc_data_channel_on_buffered_amount_low (GstWebRTCDataChannel * channel);


/**
 * GstWebRTCSCTPTransport:
 *
 * Since: 1.20
 */
struct _GstWebRTCSCTPTransport
{
  GstObject                     parent;
};

/**
 * GstWebRTCSCTPTransportClass:
 *
 * Since: 1.20
 */
struct _GstWebRTCSCTPTransportClass
{
  GstObjectClass                parent_class;
};


G_END_DECLS

#endif /* __GST_WEBRTC_PRIV_H__ */
