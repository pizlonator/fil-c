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

#ifndef __GST_WEBRTC_BIN_H__
#define __GST_WEBRTC_BIN_H__

#include <gst/sdp/sdp.h>
#include "fwd.h"
#include "transportstream.h"
#include "webrtcsctptransport.h"

G_BEGIN_DECLS

GType gst_webrtc_bin_pad_get_type(void);
#define GST_TYPE_WEBRTC_BIN_PAD            (gst_webrtc_bin_pad_get_type())
#define GST_WEBRTC_BIN_PAD(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_WEBRTC_BIN_PAD,GstWebRTCBinPad))
#define GST_IS_WEBRTC_BIN_PAD(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_WEBRTC_BIN_PAD))
#define GST_WEBRTC_BIN_PAD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_WEBRTC_BIN_PAD,GstWebRTCBinPadClass))
#define GST_IS_WEBRTC_BIN_PAD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_WEBRTC_BIN_PAD))
#define GST_WEBRTC_BIN_PAD_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_WEBRTC_BIN_PAD,GstWebRTCBinPadClass))

typedef struct _GstWebRTCBinPad GstWebRTCBinPad;
typedef struct _GstWebRTCBinPadClass GstWebRTCBinPadClass;

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GstWebRTCBinPad, gst_object_unref);

struct _GstWebRTCBinPad
{
  GstGhostPad           parent;

  GstWebRTCRTPTransceiver *trans;
  gulong                block_id;

  GstCaps              *received_caps;
  char                 *msid;
};

struct _GstWebRTCBinPadClass
{
  GstGhostPadClass      parent_class;
};

G_DECLARE_FINAL_TYPE (GstWebRTCBinSinkPad, gst_webrtc_bin_sink_pad, GST,
    WEBRTC_BIN_SINK_PAD, GstWebRTCBinPad);
#define GST_TYPE_WEBRTC_BIN_SINK_PAD (gst_webrtc_bin_sink_pad_get_type())

G_DECLARE_FINAL_TYPE (GstWebRTCBinSrcPad, gst_webrtc_bin_src_pad, GST,
    WEBRTC_BIN_SRC_PAD, GstWebRTCBinPad);
#define GST_TYPE_WEBRTC_BIN_SRC_PAD (gst_webrtc_bin_src_pad_get_type())

GType gst_webrtc_bin_get_type(void);
#define GST_TYPE_WEBRTC_BIN            (gst_webrtc_bin_get_type())
#define GST_WEBRTC_BIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_WEBRTC_BIN,GstWebRTCBin))
#define GST_IS_WEBRTC_BIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_WEBRTC_BIN))
#define GST_WEBRTC_BIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_WEBRTC_BIN,GstWebRTCBinClass))
#define GST_IS_WEBRTC_BIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_WEBRTC_BIN))
#define GST_WEBRTC_BIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_WEBRTC_BIN,GstWebRTCBinClass))

struct _GstWebRTCBin
{
  GstBin                            parent;

  GstElement                       *rtpbin;
  GstElement                       *rtpfunnel;

  GstWebRTCSignalingState           signaling_state;
  GstWebRTCICEGatheringState        ice_gathering_state;
  GstWebRTCICEConnectionState       ice_connection_state;
  GstWebRTCPeerConnectionState      peer_connection_state;

  GstWebRTCSessionDescription      *current_local_description;
  GstWebRTCSessionDescription      *pending_local_description;
  GstWebRTCSessionDescription      *current_remote_description;
  GstWebRTCSessionDescription      *pending_remote_description;

  GstWebRTCBundlePolicy             bundle_policy;
  GstWebRTCICETransportPolicy       ice_transport_policy;

  GstWebRTCBinPrivate              *priv;
};

struct _GstWebRTCBinClass
{
  GstBinClass           parent_class;
};

struct _GstWebRTCBinPrivate
{
  guint max_sink_pad_serial;
  guint src_pad_counter;

  gboolean bundle;
  GPtrArray *transceivers;
  GPtrArray *transports;
  /* stats according to https://www.w3.org/TR/webrtc-stats/#dictionary-rtcpeerconnectionstats-members */
  guint data_channels_opened;
  guint data_channels_closed;
  GPtrArray *data_channels;
  /* list of data channels we've received a sctp stream for but no data
   * channel protocol for */
  GPtrArray *pending_data_channels;
  /* dc_lock protects data_channels and pending_data_channels
   * and data_channels_opened and data_channels_closed */
  /* lock ordering is pc_lock first, then dc_lock */
  GMutex dc_lock;

  guint jb_latency;

  WebRTCSCTPTransport *sctp_transport;
  TransportStream *data_channel_transport;

  GstWebRTCICE *ice;
  GArray *ice_stream_map;
  GMutex ice_lock;
  GArray *pending_remote_ice_candidates;
  GArray *pending_local_ice_candidates;

  /* peerconnection variables */
  gboolean is_closed;
  gboolean need_negotiation;

  /* peerconnection helper thread for promises */
  GMainContext *main_context;
  GMainLoop *loop;
  GThread *thread;
  GMutex pc_lock;
  GCond pc_cond;

  gboolean running;
  gboolean async_pending;

  GList *pending_pads;
  GList *pending_sink_transceivers;

  /* count of the number of media streams we've offered for uniqueness */
  /* FIXME: overflow? */
  guint media_counter;
  /* the number of times create_offer has been called for the version field */
  guint offer_count;
  GstWebRTCSessionDescription *last_generated_offer;
  GstWebRTCSessionDescription *last_generated_answer;

  gboolean tos_attached;
};

typedef GstStructure *(*GstWebRTCBinFunc) (GstWebRTCBin * webrtc, gpointer data);

typedef struct
{
  GstWebRTCBin *webrtc;
  GstWebRTCBinFunc op;
  gpointer data;
  GDestroyNotify notify;
  GstPromise *promise;
} GstWebRTCBinTask;

gboolean        gst_webrtc_bin_enqueue_task             (GstWebRTCBin * pc,
                                                         GstWebRTCBinFunc func,
                                                         gpointer data,
                                                         GDestroyNotify notify,
                                                         GstPromise *promise);

void            gst_webrtc_bin_get_peer_connection_stats(GstWebRTCBin * pc,
                                                         guint * data_channels_opened,
                                                         guint * data_channels_closed);

G_END_DECLS

#endif /* __GST_WEBRTC_BIN_H__ */
