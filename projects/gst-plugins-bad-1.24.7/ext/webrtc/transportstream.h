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

#ifndef __TRANSPORT_STREAM_H__
#define __TRANSPORT_STREAM_H__

#include "fwd.h"
#include <gst/rtp/rtp.h>
#include <gst/webrtc/rtptransceiver.h>

G_BEGIN_DECLS

GType transport_stream_get_type(void);
#define GST_TYPE_WEBRTC_TRANSPORT_STREAM (transport_stream_get_type())
#define TRANSPORT_STREAM(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_WEBRTC_TRANSPORT_STREAM,TransportStream))
#define TRANSPORT_STREAM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_WEBRTC_TRANSPORT_STREAM,TransportStreamClass))
#define TRANSPORT_STREAM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_WEBRTC_TRANSPORT_STREAM,TransportStreamClass))

typedef struct
{
  guint8 pt;
  guint media_idx;
  GstCaps *caps;
} PtMapItem;

typedef struct
{
  GstWebRTCRTPTransceiverDirection direction;
  guint32 ssrc;
  guint media_idx;
  char *mid;
  char *rid;
  GWeakRef rtpjitterbuffer; /* for stats */
} SsrcMapItem;

struct _TransportStream
{
  GstObject                 parent;

  guint                     session_id;             /* session_id */
  gboolean                  dtls_client;
  gboolean                  active;                 /* TRUE if any mline in the bundle/transport is active */
  TransportSendBin         *send_bin;               /* bin containing all the sending transport elements */
  TransportReceiveBin      *receive_bin;            /* bin containing all the receiving transport elements */
  GstWebRTCICEStream       *stream;

  GstWebRTCDTLSTransport   *transport;

  GArray                   *ptmap;                  /* array of PtMapItem's */
  GPtrArray                *ssrcmap;                /* array of SsrcMapItem's */
  gboolean                  output_connected;       /* whether receive bin is connected to rtpbin */

  guint                     rtphdrext_id_stream_id;
  guint                     rtphdrext_id_repaired_stream_id;
  GstElement               *rtxsend;
  GstRTPHeaderExtension    *rtxsend_stream_id;
  GstRTPHeaderExtension    *rtxsend_repaired_stream_id;
  GstElement               *rtxreceive;
  GstRTPHeaderExtension    *rtxreceive_stream_id;
  GstRTPHeaderExtension    *rtxreceive_repaired_stream_id;

  GstElement               *reddec;
  GList                    *fecdecs;
};

struct _TransportStreamClass
{
  GstObjectClass            parent_class;
};

TransportStream *       transport_stream_new        (GstWebRTCBin * webrtc,
                                                     guint session_id);
int                     transport_stream_get_pt     (TransportStream * stream,
                                                     const gchar * encoding_name,
                                                     guint media_idx);
int *                   transport_stream_get_all_pt (TransportStream * stream,
                                                     const gchar * encoding_name,
                                                     gsize * pt_len);
GstCaps *               transport_stream_get_caps_for_pt    (TransportStream * stream,
                                                             guint pt);

typedef gboolean (*FindSsrcMapFunc) (SsrcMapItem * e1, gconstpointer data);

SsrcMapItem *           transport_stream_find_ssrc_map_item (TransportStream * stream,
                                                      gconstpointer data,
                                                      FindSsrcMapFunc func);

void                    transport_stream_filter_ssrc_map_item (TransportStream * stream,
                                                      gconstpointer data,
                                                      FindSsrcMapFunc func);

SsrcMapItem *           transport_stream_add_ssrc_map_item (TransportStream * stream,
                                                      GstWebRTCRTPTransceiverDirection direction,
                                                      guint32 ssrc,
                                                      guint media_idx);

G_END_DECLS

#endif /* __TRANSPORT_STREAM_H__ */
