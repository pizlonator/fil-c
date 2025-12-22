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

#ifndef __GST_WEBRTC_FWD_H__
#define __GST_WEBRTC_FWD_H__

#ifndef GST_USE_UNSTABLE_API
#warning "The WebRTC library from gst-plugins-bad is unstable API and may change in future."
#warning "You can define GST_USE_UNSTABLE_API to avoid this warning."
#endif

#include <gst/gst.h>

/**
 * SECTION:webrtc_fwd.h
 * @title: GstWebRTC Enumerations
 */

G_BEGIN_DECLS

#ifndef GST_WEBRTC_API
# ifdef BUILDING_GST_WEBRTC
#  define GST_WEBRTC_API GST_API_EXPORT         /* from config.h */
# else
#  define GST_WEBRTC_API GST_API_IMPORT
# endif
#endif

/**
 * GST_WEBRTC_DEPRECATED: (attributes doc.skip=true)
 */
/**
 * GST_WEBRTC_DEPRECATED_FOR: (attributes doc.skip=true)
 */
#ifndef GST_DISABLE_DEPRECATED
#define GST_WEBRTC_DEPRECATED GST_WEBRTC_API
#define GST_WEBRTC_DEPRECATED_FOR(f) GST_WEBRTC_API
#else
#define GST_WEBRTC_DEPRECATED G_DEPRECATED GST_WEBRTC_API
#define GST_WEBRTC_DEPRECATED_FOR(f) G_DEPRECATED_FOR(f) GST_WEBRTC_API
#endif

#include <gst/webrtc/webrtc-enumtypes.h>

/**
 * GstWebRTCDTLSTransport:
 */
typedef struct _GstWebRTCDTLSTransport GstWebRTCDTLSTransport;
typedef struct _GstWebRTCDTLSTransportClass GstWebRTCDTLSTransportClass;

/**
 * GstWebRTCICE:
 *
 * Since: 1.22
 */
typedef struct _GstWebRTCICE GstWebRTCICE;
typedef struct _GstWebRTCICEClass GstWebRTCICEClass;

/**
 * GstWebRTCICECandidateStats:
 *
 * Since: 1.22
 */
typedef struct _GstWebRTCICECandidateStats GstWebRTCICECandidateStats;

/**
 * GstWebRTCICEStream:
 *
 * Since: 1.22
 */
typedef struct _GstWebRTCICEStream GstWebRTCICEStream;
typedef struct _GstWebRTCICEStreamClass GstWebRTCICEStreamClass;

/**
 * GstWebRTCICETransport:
 */
typedef struct _GstWebRTCICETransport GstWebRTCICETransport;
typedef struct _GstWebRTCICETransportClass GstWebRTCICETransportClass;

/**
 * GstWebRTCRTPReceiver:
 *
 * An object to track the receiving aspect of the stream
 *
 * Mostly matches the WebRTC RTCRtpReceiver interface.
 */
typedef struct _GstWebRTCRTPReceiver GstWebRTCRTPReceiver;
typedef struct _GstWebRTCRTPReceiverClass GstWebRTCRTPReceiverClass;

/**
 * GstWebRTCRTPSender:
 *
 * An object to track the sending aspect of the stream
 *
 * Mostly matches the WebRTC RTCRtpSender interface.
 */
typedef struct _GstWebRTCRTPSender GstWebRTCRTPSender;
typedef struct _GstWebRTCRTPSenderClass GstWebRTCRTPSenderClass;

typedef struct _GstWebRTCSessionDescription GstWebRTCSessionDescription;

/**
 * GstWebRTCRTPTransceiver:
 *
 * Mostly matches the WebRTC RTCRtpTransceiver interface.
 */
typedef struct _GstWebRTCRTPTransceiver GstWebRTCRTPTransceiver;
typedef struct _GstWebRTCRTPTransceiverClass GstWebRTCRTPTransceiverClass;

/**
 * GstWebRTCDataChannel:
 *
 * Since: 1.18
 */
typedef struct _GstWebRTCDataChannel GstWebRTCDataChannel;
typedef struct _GstWebRTCDataChannelClass GstWebRTCDataChannelClass;

typedef struct _GstWebRTCSCTPTransport GstWebRTCSCTPTransport;
typedef struct _GstWebRTCSCTPTransportClass GstWebRTCSCTPTransportClass;

/**
 * GstWebRTCDTLSTransportState:
 * @GST_WEBRTC_DTLS_TRANSPORT_STATE_NEW: new
 * @GST_WEBRTC_DTLS_TRANSPORT_STATE_CLOSED: closed
 * @GST_WEBRTC_DTLS_TRANSPORT_STATE_FAILED: failed
 * @GST_WEBRTC_DTLS_TRANSPORT_STATE_CONNECTING: connecting
 * @GST_WEBRTC_DTLS_TRANSPORT_STATE_CONNECTED: connected
 */
typedef enum /*< underscore_name=gst_webrtc_dtls_transport_state >*/
{
  GST_WEBRTC_DTLS_TRANSPORT_STATE_NEW,
  GST_WEBRTC_DTLS_TRANSPORT_STATE_CLOSED,
  GST_WEBRTC_DTLS_TRANSPORT_STATE_FAILED,
  GST_WEBRTC_DTLS_TRANSPORT_STATE_CONNECTING,
  GST_WEBRTC_DTLS_TRANSPORT_STATE_CONNECTED,
} GstWebRTCDTLSTransportState;

/**
 * GstWebRTCICEGatheringState:
 * @GST_WEBRTC_ICE_GATHERING_STATE_NEW: new
 * @GST_WEBRTC_ICE_GATHERING_STATE_GATHERING: gathering
 * @GST_WEBRTC_ICE_GATHERING_STATE_COMPLETE: complete
 *
 * See <http://w3c.github.io/webrtc-pc/#dom-rtcicegatheringstate>
 */
typedef enum /*< underscore_name=gst_webrtc_ice_gathering_state >*/
{
  GST_WEBRTC_ICE_GATHERING_STATE_NEW,
  GST_WEBRTC_ICE_GATHERING_STATE_GATHERING,
  GST_WEBRTC_ICE_GATHERING_STATE_COMPLETE,
} GstWebRTCICEGatheringState; /*< underscore_name=gst_webrtc_ice_gathering_state >*/

/**
 * GstWebRTCICEConnectionState:
 * @GST_WEBRTC_ICE_CONNECTION_STATE_NEW: new
 * @GST_WEBRTC_ICE_CONNECTION_STATE_CHECKING: checking
 * @GST_WEBRTC_ICE_CONNECTION_STATE_CONNECTED: connected
 * @GST_WEBRTC_ICE_CONNECTION_STATE_COMPLETED: completed
 * @GST_WEBRTC_ICE_CONNECTION_STATE_FAILED: failed
 * @GST_WEBRTC_ICE_CONNECTION_STATE_DISCONNECTED: disconnected
 * @GST_WEBRTC_ICE_CONNECTION_STATE_CLOSED: closed
 *
 * See <http://w3c.github.io/webrtc-pc/#dom-rtciceconnectionstate>
 */
typedef enum /*< underscore_name=gst_webrtc_ice_connection_state >*/
{
  GST_WEBRTC_ICE_CONNECTION_STATE_NEW,
  GST_WEBRTC_ICE_CONNECTION_STATE_CHECKING,
  GST_WEBRTC_ICE_CONNECTION_STATE_CONNECTED,
  GST_WEBRTC_ICE_CONNECTION_STATE_COMPLETED,
  GST_WEBRTC_ICE_CONNECTION_STATE_FAILED,
  GST_WEBRTC_ICE_CONNECTION_STATE_DISCONNECTED,
  GST_WEBRTC_ICE_CONNECTION_STATE_CLOSED,
} GstWebRTCICEConnectionState;

/**
 * GstWebRTCSignalingState:
 * @GST_WEBRTC_SIGNALING_STATE_STABLE: stable
 * @GST_WEBRTC_SIGNALING_STATE_CLOSED: closed
 * @GST_WEBRTC_SIGNALING_STATE_HAVE_LOCAL_OFFER: have-local-offer
 * @GST_WEBRTC_SIGNALING_STATE_HAVE_REMOTE_OFFER: have-remote-offer
 * @GST_WEBRTC_SIGNALING_STATE_HAVE_LOCAL_PRANSWER: have-local-pranswer
 * @GST_WEBRTC_SIGNALING_STATE_HAVE_REMOTE_PRANSWER: have-remote-pranswer
 *
 * See <http://w3c.github.io/webrtc-pc/#dom-rtcsignalingstate>
 */
typedef enum /*< underscore_name=gst_webrtc_signaling_state >*/
{
  GST_WEBRTC_SIGNALING_STATE_STABLE,
  GST_WEBRTC_SIGNALING_STATE_CLOSED,
  GST_WEBRTC_SIGNALING_STATE_HAVE_LOCAL_OFFER,
  GST_WEBRTC_SIGNALING_STATE_HAVE_REMOTE_OFFER,
  GST_WEBRTC_SIGNALING_STATE_HAVE_LOCAL_PRANSWER,
  GST_WEBRTC_SIGNALING_STATE_HAVE_REMOTE_PRANSWER,
} GstWebRTCSignalingState;

/**
 * GstWebRTCPeerConnectionState:
 * @GST_WEBRTC_PEER_CONNECTION_STATE_NEW: new
 * @GST_WEBRTC_PEER_CONNECTION_STATE_CONNECTING: connecting
 * @GST_WEBRTC_PEER_CONNECTION_STATE_CONNECTED: connected
 * @GST_WEBRTC_PEER_CONNECTION_STATE_DISCONNECTED: disconnected
 * @GST_WEBRTC_PEER_CONNECTION_STATE_FAILED: failed
 * @GST_WEBRTC_PEER_CONNECTION_STATE_CLOSED: closed
 *
 * See <http://w3c.github.io/webrtc-pc/#dom-rtcpeerconnectionstate>
 */
typedef enum /*< underscore_name=gst_webrtc_peer_connection_state >*/
{
  GST_WEBRTC_PEER_CONNECTION_STATE_NEW,
  GST_WEBRTC_PEER_CONNECTION_STATE_CONNECTING,
  GST_WEBRTC_PEER_CONNECTION_STATE_CONNECTED,
  GST_WEBRTC_PEER_CONNECTION_STATE_DISCONNECTED,
  GST_WEBRTC_PEER_CONNECTION_STATE_FAILED,
  GST_WEBRTC_PEER_CONNECTION_STATE_CLOSED,
} GstWebRTCPeerConnectionState;

/**
 * GstWebRTCICERole:
 * @GST_WEBRTC_ICE_ROLE_CONTROLLED: controlled
 * @GST_WEBRTC_ICE_ROLE_CONTROLLING: controlling
 */
typedef enum /*< underscore_name=gst_webrtc_ice_role >*/
{
  GST_WEBRTC_ICE_ROLE_CONTROLLED,
  GST_WEBRTC_ICE_ROLE_CONTROLLING,
} GstWebRTCICERole;

/**
 * GstWebRTCICEComponent:
 * @GST_WEBRTC_ICE_COMPONENT_RTP: RTP component
 * @GST_WEBRTC_ICE_COMPONENT_RTCP: RTCP component
 */
typedef enum /*< underscore_name=gst_webrtc_ice_component >*/
{
  GST_WEBRTC_ICE_COMPONENT_RTP,
  GST_WEBRTC_ICE_COMPONENT_RTCP,
} GstWebRTCICEComponent;

/**
 * GstWebRTCSDPType:
 * @GST_WEBRTC_SDP_TYPE_OFFER: offer
 * @GST_WEBRTC_SDP_TYPE_PRANSWER: pranswer
 * @GST_WEBRTC_SDP_TYPE_ANSWER: answer
 * @GST_WEBRTC_SDP_TYPE_ROLLBACK: rollback
 *
 * See <http://w3c.github.io/webrtc-pc/#rtcsdptype>
 */
typedef enum /*< underscore_name=gst_webrtc_sdp_type >*/
{
  GST_WEBRTC_SDP_TYPE_OFFER = 1,
  GST_WEBRTC_SDP_TYPE_PRANSWER,
  GST_WEBRTC_SDP_TYPE_ANSWER,
  GST_WEBRTC_SDP_TYPE_ROLLBACK,
} GstWebRTCSDPType;

/**
 * GstWebRTCRTPTransceiverDirection:
 * @GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_NONE: none
 * @GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE: inactive
 * @GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY: sendonly
 * @GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY: recvonly
 * @GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV: sendrecv
 */
typedef enum /*< underscore_name=gst_webrtc_rtp_transceiver_direction >*/
{
  GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_NONE,
  GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE,
  GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY,
  GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY,
  GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV,
} GstWebRTCRTPTransceiverDirection;

/**
 * GstWebRTCDTLSSetup:
 * @GST_WEBRTC_DTLS_SETUP_NONE: none
 * @GST_WEBRTC_DTLS_SETUP_ACTPASS: actpass
 * @GST_WEBRTC_DTLS_SETUP_ACTIVE: sendonly
 * @GST_WEBRTC_DTLS_SETUP_PASSIVE: recvonly
 */
typedef enum /*< underscore_name=gst_webrtc_dtls_setup >*/
{
  GST_WEBRTC_DTLS_SETUP_NONE,
  GST_WEBRTC_DTLS_SETUP_ACTPASS,
  GST_WEBRTC_DTLS_SETUP_ACTIVE,
  GST_WEBRTC_DTLS_SETUP_PASSIVE,
} GstWebRTCDTLSSetup;

/**
 * GstWebRTCStatsType:
 * @GST_WEBRTC_STATS_CODEC: codec
 * @GST_WEBRTC_STATS_INBOUND_RTP: inbound-rtp
 * @GST_WEBRTC_STATS_OUTBOUND_RTP: outbound-rtp
 * @GST_WEBRTC_STATS_REMOTE_INBOUND_RTP: remote-inbound-rtp
 * @GST_WEBRTC_STATS_REMOTE_OUTBOUND_RTP: remote-outbound-rtp
 * @GST_WEBRTC_STATS_CSRC: csrc
 * @GST_WEBRTC_STATS_PEER_CONNECTION: peer-connection
 * @GST_WEBRTC_STATS_DATA_CHANNEL: data-channel
 * @GST_WEBRTC_STATS_STREAM: stream
 * @GST_WEBRTC_STATS_TRANSPORT: transport
 * @GST_WEBRTC_STATS_CANDIDATE_PAIR: candidate-pair
 * @GST_WEBRTC_STATS_LOCAL_CANDIDATE: local-candidate
 * @GST_WEBRTC_STATS_REMOTE_CANDIDATE: remote-candidate
 * @GST_WEBRTC_STATS_CERTIFICATE: certificate
 *
 * See <https://w3c.github.io/webrtc-stats/#dom-rtcstatstype>
 */
typedef enum /*< underscore_name=gst_webrtc_stats_type >*/
{
  GST_WEBRTC_STATS_CODEC = 1,
  GST_WEBRTC_STATS_INBOUND_RTP,
  GST_WEBRTC_STATS_OUTBOUND_RTP,
  GST_WEBRTC_STATS_REMOTE_INBOUND_RTP,
  GST_WEBRTC_STATS_REMOTE_OUTBOUND_RTP,
  GST_WEBRTC_STATS_CSRC,
  GST_WEBRTC_STATS_PEER_CONNECTION,
  GST_WEBRTC_STATS_DATA_CHANNEL,
  GST_WEBRTC_STATS_STREAM,
  GST_WEBRTC_STATS_TRANSPORT,
  GST_WEBRTC_STATS_CANDIDATE_PAIR,
  GST_WEBRTC_STATS_LOCAL_CANDIDATE,
  GST_WEBRTC_STATS_REMOTE_CANDIDATE,
  GST_WEBRTC_STATS_CERTIFICATE,
} GstWebRTCStatsType;

/**
 * GstWebRTCFECType:
 * @GST_WEBRTC_FEC_TYPE_NONE: none
 * @GST_WEBRTC_FEC_TYPE_ULP_RED: ulpfec + red
 *
 * Since: 1.14.1
 */
typedef enum /*< underscore_name=gst_webrtc_fec_type >*/
{
  GST_WEBRTC_FEC_TYPE_NONE,
  GST_WEBRTC_FEC_TYPE_ULP_RED,
} GstWebRTCFECType;

/**
 * GstWebRTCSCTPTransportState:
 * @GST_WEBRTC_SCTP_TRANSPORT_STATE_NEW: new
 * @GST_WEBRTC_SCTP_TRANSPORT_STATE_CONNECTING: connecting
 * @GST_WEBRTC_SCTP_TRANSPORT_STATE_CONNECTED: connected
 * @GST_WEBRTC_SCTP_TRANSPORT_STATE_CLOSED: closed
 *
 * See <http://w3c.github.io/webrtc-pc/#dom-rtcsctptransportstate>
 *
 * Since: 1.16
 */
typedef enum /*< underscore_name=gst_webrtc_sctp_transport_state >*/
{
  GST_WEBRTC_SCTP_TRANSPORT_STATE_NEW,
  GST_WEBRTC_SCTP_TRANSPORT_STATE_CONNECTING,
  GST_WEBRTC_SCTP_TRANSPORT_STATE_CONNECTED,
  GST_WEBRTC_SCTP_TRANSPORT_STATE_CLOSED,
} GstWebRTCSCTPTransportState;

/**
 * GstWebRTCPriorityType:
 * @GST_WEBRTC_PRIORITY_TYPE_VERY_LOW: very-low
 * @GST_WEBRTC_PRIORITY_TYPE_LOW: low
 * @GST_WEBRTC_PRIORITY_TYPE_MEDIUM: medium
 * @GST_WEBRTC_PRIORITY_TYPE_HIGH: high
 *
 * See <http://w3c.github.io/webrtc-pc/#dom-rtcprioritytype>
 *
 * Since: 1.16
 */
typedef enum /*< underscore_name=gst_webrtc_priority_type >*/
{
  GST_WEBRTC_PRIORITY_TYPE_VERY_LOW = 1,
  GST_WEBRTC_PRIORITY_TYPE_LOW,
  GST_WEBRTC_PRIORITY_TYPE_MEDIUM,
  GST_WEBRTC_PRIORITY_TYPE_HIGH,
} GstWebRTCPriorityType;

/**
 * GstWebRTCDataChannelState:
 * @GST_WEBRTC_DATA_CHANNEL_STATE_CONNECTING: connecting
 * @GST_WEBRTC_DATA_CHANNEL_STATE_OPEN: open
 * @GST_WEBRTC_DATA_CHANNEL_STATE_CLOSING: closing
 * @GST_WEBRTC_DATA_CHANNEL_STATE_CLOSED: closed
 *
 * See <http://w3c.github.io/webrtc-pc/#dom-rtcdatachannelstate>
 *
 * Since: 1.16
 */
typedef enum /*< underscore_name=gst_webrtc_data_channel_state >*/
{
  GST_WEBRTC_DATA_CHANNEL_STATE_CONNECTING = 1,
  GST_WEBRTC_DATA_CHANNEL_STATE_OPEN,
  GST_WEBRTC_DATA_CHANNEL_STATE_CLOSING,
  GST_WEBRTC_DATA_CHANNEL_STATE_CLOSED,
} GstWebRTCDataChannelState;

/**
 * GstWebRTCBundlePolicy:
 * @GST_WEBRTC_BUNDLE_POLICY_NONE: none
 * @GST_WEBRTC_BUNDLE_POLICY_BALANCED: balanced
 * @GST_WEBRTC_BUNDLE_POLICY_MAX_COMPAT: max-compat
 * @GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE: max-bundle
 *
 * See https://tools.ietf.org/html/draft-ietf-rtcweb-jsep-24#section-4.1.1
 * for more information.
 *
 * Since: 1.16
 */
typedef enum /*<underscore_name=gst_webrtc_bundle_policy>*/
{
  GST_WEBRTC_BUNDLE_POLICY_NONE,
  GST_WEBRTC_BUNDLE_POLICY_BALANCED,
  GST_WEBRTC_BUNDLE_POLICY_MAX_COMPAT,
  GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE,
} GstWebRTCBundlePolicy;

/**
 * GstWebRTCICETransportPolicy:
 * @GST_WEBRTC_ICE_TRANSPORT_POLICY_ALL: all
 * @GST_WEBRTC_ICE_TRANSPORT_POLICY_RELAY: relay
 *
 * See https://tools.ietf.org/html/draft-ietf-rtcweb-jsep-24#section-4.1.1
 * for more information.
 *
 * Since: 1.16
 */
typedef enum /*<underscore_name=gst_webrtc_ice_transport_policy>*/
{
  GST_WEBRTC_ICE_TRANSPORT_POLICY_ALL,
  GST_WEBRTC_ICE_TRANSPORT_POLICY_RELAY,
} GstWebRTCICETransportPolicy;

/**
 * GstWebRTCKind:
 * @GST_WEBRTC_KIND_UNKNOWN: Kind has not yet been set
 * @GST_WEBRTC_KIND_AUDIO: Kind is audio
 * @GST_WEBRTC_KIND_VIDEO: Kind is audio
 *
 * https://w3c.github.io/mediacapture-main/#dom-mediastreamtrack-kind
 *
 * Since: 1.20
 */
typedef enum /*<underscore_name=gst_webrtc_kind>*/
{
  GST_WEBRTC_KIND_UNKNOWN,
  GST_WEBRTC_KIND_AUDIO,
  GST_WEBRTC_KIND_VIDEO,
} GstWebRTCKind;


GST_WEBRTC_API
GQuark gst_webrtc_error_quark (void);

/**
 * GST_WEBRTC_ERROR:
 *
 * Since: 1.20
 */
#define GST_WEBRTC_ERROR gst_webrtc_error_quark ()

/**
 * GstWebRTCError:
 * @GST_WEBRTC_ERROR_DATA_CHANNEL_FAILURE: data-channel-failure
 * @GST_WEBRTC_ERROR_DTLS_FAILURE: dtls-failure
 * @GST_WEBRTC_ERROR_FINGERPRINT_FAILURE: fingerprint-failure
 * @GST_WEBRTC_ERROR_SCTP_FAILURE: sctp-failure
 * @GST_WEBRTC_ERROR_SDP_SYNTAX_ERROR: sdp-syntax-error
 * @GST_WEBRTC_ERROR_HARDWARE_ENCODER_NOT_AVAILABLE: hardware-encoder-not-available
 * @GST_WEBRTC_ERROR_ENCODER_ERROR: encoder-error
 * @GST_WEBRTC_ERROR_INVALID_STATE: invalid-state (part of WebIDL specification)
 * @GST_WEBRTC_ERROR_INTERNAL_FAILURE: GStreamer-specific failure, not matching any other value from the specification
 *
 * See <https://www.w3.org/TR/webrtc/#dom-rtcerrordetailtype> for more information.
 *
 * Since: 1.20
 */
/**
 * GST_WEBRTC_ERROR_INVALID_MODIFICATION:
 *
 * invalid-modification (part of WebIDL specification)
 *
 * Since: 1.22
 */
/**
 * GST_WEBRTC_ERROR_TYPE_ERROR:
 *
 * type-error (maps to JavaScript TypeError)
 *
 * Since: 1.22
 */
typedef enum /*<underscore_name=gst_webrtc_error>*/
{
  GST_WEBRTC_ERROR_DATA_CHANNEL_FAILURE,
  GST_WEBRTC_ERROR_DTLS_FAILURE,
  GST_WEBRTC_ERROR_FINGERPRINT_FAILURE,
  GST_WEBRTC_ERROR_SCTP_FAILURE,
  GST_WEBRTC_ERROR_SDP_SYNTAX_ERROR,
  GST_WEBRTC_ERROR_HARDWARE_ENCODER_NOT_AVAILABLE,
  GST_WEBRTC_ERROR_ENCODER_ERROR,
  GST_WEBRTC_ERROR_INVALID_STATE,
  GST_WEBRTC_ERROR_INTERNAL_FAILURE,
  GST_WEBRTC_ERROR_INVALID_MODIFICATION,
  GST_WEBRTC_ERROR_TYPE_ERROR,
} GstWebRTCError;

G_END_DECLS

#endif /* __GST_WEBRTC_FWD_H__ */
