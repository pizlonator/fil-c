/* GStreamer
 * Copyright (C) <2007> Wim Taymans <wim.taymans@gmail.com>
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

#ifndef __GST_SDP_DEMUX_H__
#define __GST_SDP_DEMUX_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GST_TYPE_SDP_DEMUX \
  (gst_sdp_demux_get_type())
#define GST_SDP_DEMUX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SDP_DEMUX,GstSDPDemux))
#define GST_SDP_DEMUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SDP_DEMUX,GstSDPDemuxClass))
#define GST_IS_SDP_DEMUX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SDP_DEMUX))
#define GST_IS_SDP_DEMUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SDP_DEMUX))
#define GST_SDP_DEMUX_CAST(obj) \
  ((GstSDPDemux *)(obj))

typedef struct _GstSDPDemux GstSDPDemux;
typedef struct _GstSDPDemuxClass GstSDPDemuxClass;

#define GST_SDP_STREAM_GET_LOCK(sdp)   (&GST_SDP_DEMUX_CAST(sdp)->stream_rec_lock)
#define GST_SDP_STREAM_LOCK(sdp)       (g_rec_mutex_lock (GST_SDP_STREAM_GET_LOCK(sdp)))
#define GST_SDP_STREAM_UNLOCK(sdp)     (g_rec_mutex_unlock (GST_SDP_STREAM_GET_LOCK(sdp)))

typedef struct _GstSDPStream GstSDPStream;

struct _GstSDPStream {
  gint          id;
  guint32       ssrc;

  GstSDPDemux    *parent; /* parent, no extra ref to parent is taken */

  /* pad we expose or NULL when it does not have an actual pad */
  GstPad       *srcpad;
  GstFlowReturn last_ret;
  gboolean      added;
  gboolean      disabled;
  GstCaps      *caps;
  gboolean      eos;

  /* our udp sources */
  GstElement   *udpsrc[2];
  GstPad       *channelpad[2];
  guint         rtp_port;
  guint         rtcp_port;

  gchar        *destination;
  guint         ttl;
  gboolean      multicast;

  /* source-filter */
  gchar        *src_list;
  gchar        *src_incl_list;

  /* our udp sink back to the server */
  GstElement   *udpsink;
  GstPad       *rtcppad;

  /* state */
  gint          pt;
  gboolean      container;
};

/**
 * GstSDPDemuxRTCPMode:
 * @GST_SDP_DEMUX_RTCP_MODE_INACTIVE: Don't send or receive RTCP packets
 * @GST_SDP_DEMUX_RTCP_MODE_RECVONLY: Only receive RTCP packets
 * @GST_SDP_DEMUX_RTCP_MODE_SENDONLY: Only send RTCP packets
 * @GST_SDP_DEMUX_RTCP_MODE_SENDRECV: Send and receive RTCP packets
 *
 * RTCP configuration.
 *
 * Since: 1.24
 */
typedef enum {
  GST_SDP_DEMUX_RTCP_MODE_INACTIVE = 0,
  GST_SDP_DEMUX_RTCP_MODE_RECVONLY = 1,
  GST_SDP_DEMUX_RTCP_MODE_SENDONLY = 2,
  GST_SDP_DEMUX_RTCP_MODE_SENDRECV = 3,
} GstSDPDemuxRTCPMode;

struct _GstSDPDemux {
  GstBin           parent;

  GstPad          *sinkpad;
  GstAdapter      *adapter;
  GstState         target;

  /* task for UDP loop */
  gboolean         ignore_timeout;

  gint             numstreams;
  GRecMutex        stream_rec_lock;
  GList           *streams;

  /* properties */
  gboolean          debug;
  guint64           udp_timeout;
  guint             latency;
  gboolean          redirect;
  const gchar      *media; /* if non-NULL only hook up these kinds of media (video/audio) */ /* interned string */
  GstSDPDemuxRTCPMode rtcp_mode;
  gboolean          timeout_inactive_rtp_sources;

  /* session management */
  GstElement      *session;
  gulong           session_sig_id;
  gulong           session_ptmap_id;
  gulong           session_nmp_id;
};

struct _GstSDPDemuxClass {
  GstBinClass parent_class;
};

GType gst_sdp_demux_get_type(void);
GST_ELEMENT_REGISTER_DECLARE (sdpdemux);

G_END_DECLS

#endif /* __GST_SDP_DEMUX_H__ */
