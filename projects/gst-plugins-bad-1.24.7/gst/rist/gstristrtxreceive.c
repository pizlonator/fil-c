/* RTP Retransmission receiver element for GStreamer
 *
 * gstrtprtxreceive.c:
 *
 * Copyright (C) 2013-2019 Collabora Ltd.
 *   @author Julien Isorce <julien.isorce@collabora.co.uk>
 *           Nicolas Dufresne <nicolas.dufresne@collabora.com>
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

/**
 * SECTION:element-ristrtxreceive
 * @title: ristrtxreceive
 * @see_also: ristrtxsend
 *
 * This element translates RIST RTX packets into its original form with the
 * %GST_RTP_BUFFER_FLAG_RETRANSMISSION flag set. This element is intented to
 * be used by ristsrc element.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/rtp/gstrtpbuffer.h>

#include "gstrist.h"

GST_DEBUG_CATEGORY_STATIC (gst_rist_rtx_receive_debug);
#define GST_CAT_DEFAULT gst_rist_rtx_receive_debug

enum
{
  PROP_0,
  PROP_NUM_RTX_REQUESTS,
  PROP_NUM_RTX_PACKETS,
  PROP_RIST
};

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp")
    );

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp")
    );

struct _GstRistRtxReceive
{
  GstElement element;

  /* pad */
  GstPad *sinkpad;
  GstPad *srcpad;

  /* statistics */
  guint num_rtx_requests;
  guint num_rtx_packets;

  GstClockTime last_time;
};

static gboolean gst_rist_rtx_receive_src_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static GstFlowReturn gst_rist_rtx_receive_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);
static GstStateChangeReturn gst_rist_rtx_receive_change_state (GstElement *
    element, GstStateChange transition);
static void gst_rist_rtx_receive_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

G_DEFINE_TYPE_WITH_CODE (GstRistRtxReceive, gst_rist_rtx_receive,
    GST_TYPE_ELEMENT, GST_DEBUG_CATEGORY_INIT (gst_rist_rtx_receive_debug,
        "ristrtxreceive", 0, "RIST retransmission receiver"));
GST_ELEMENT_REGISTER_DEFINE (ristrtxreceive, "ristrtxreceive",
    GST_RANK_NONE, GST_TYPE_RIST_RTX_RECEIVE);

static void
gst_rist_rtx_receive_class_init (GstRistRtxReceiveClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->get_property = gst_rist_rtx_receive_get_property;

  g_object_class_install_property (gobject_class, PROP_NUM_RTX_REQUESTS,
      g_param_spec_uint ("num-rtx-requests", "Num RTX Requests",
          "Number of retransmission events received", 0, G_MAXUINT,
          0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_NUM_RTX_PACKETS,
      g_param_spec_uint ("num-rtx-packets", "Num RTX Packets",
          " Number of retransmission packets received", 0, G_MAXUINT,
          0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (gstelement_class, &src_factory);
  gst_element_class_add_static_pad_template (gstelement_class, &sink_factory);

  gst_element_class_set_static_metadata (gstelement_class,
      "RIST Retransmission receiver", "Codec",
      "Receive retransmitted RIST packets according to VSF TR-06-1",
      "Nicolas Dufresne <nicolas.dufresne@collabora.com>");

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_rist_rtx_receive_change_state);
}

static void
gst_rist_rtx_receive_reset (GstRistRtxReceive * rtx)
{
  GST_OBJECT_LOCK (rtx);
  rtx->num_rtx_requests = 0;
  rtx->num_rtx_packets = 0;
  GST_OBJECT_UNLOCK (rtx);
}

static void
gst_rist_rtx_receive_init (GstRistRtxReceive * rtx)
{
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (rtx);

  rtx->srcpad =
      gst_pad_new_from_template (gst_element_class_get_pad_template (klass,
          "src"), "src");
  GST_PAD_SET_PROXY_CAPS (rtx->srcpad);
  GST_PAD_SET_PROXY_ALLOCATION (rtx->srcpad);
  gst_pad_set_event_function (rtx->srcpad,
      GST_DEBUG_FUNCPTR (gst_rist_rtx_receive_src_event));
  gst_element_add_pad (GST_ELEMENT (rtx), rtx->srcpad);

  rtx->sinkpad =
      gst_pad_new_from_template (gst_element_class_get_pad_template (klass,
          "sink"), "sink");
  GST_PAD_SET_PROXY_CAPS (rtx->sinkpad);
  GST_PAD_SET_PROXY_ALLOCATION (rtx->sinkpad);
  gst_pad_set_chain_function (rtx->sinkpad,
      GST_DEBUG_FUNCPTR (gst_rist_rtx_receive_chain));
  gst_element_add_pad (GST_ELEMENT (rtx), rtx->sinkpad);
}

static gboolean
gst_rist_rtx_receive_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstRistRtxReceive *rtx = GST_RIST_RTX_RECEIVE (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_UPSTREAM:
    {
      const GstStructure *s = gst_event_get_structure (event);

      /* This event usually comes from the downstream gstrtpjitterbuffer */
      if (gst_structure_has_name (s, "GstRTPRetransmissionRequest")) {
#ifndef GST_DISABLE_GST_DEBUG
        guint seqnum = 0;
        guint ssrc = 0;

        /* retrieve seqnum of the packet that need to be retransmitted */
        if (!gst_structure_get_uint (s, "seqnum", &seqnum))
          seqnum = -1;

        /* retrieve ssrc of the packet that need to be retransmitted
         * it's useful when reconstructing the original packet from the rtx packet */
        if (!gst_structure_get_uint (s, "ssrc", &ssrc))
          ssrc = -1;

        GST_DEBUG_OBJECT (rtx, "got rtx request for seqnum: %u, ssrc: %X",
            seqnum, ssrc);
#endif

        GST_OBJECT_LOCK (rtx);
        /* increase number of seen requests for our statistics */
        ++rtx->num_rtx_requests;
        GST_OBJECT_UNLOCK (rtx);
      }
      break;
    }
    default:
      break;
  }

  return gst_pad_event_default (pad, parent, event);
}

static GstFlowReturn
gst_rist_rtx_receive_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer)
{
  GstRistRtxReceive *rtx = GST_RIST_RTX_RECEIVE (parent);
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  guint32 ssrc = 0;
  guint16 seqnum = 0;
  gboolean is_rtx;

  /* map current rtp packet to parse its header */
  if (!gst_rtp_buffer_map (buffer, GST_MAP_READ, &rtp))
    goto invalid_buffer;

  ssrc = gst_rtp_buffer_get_ssrc (&rtp);
  seqnum = gst_rtp_buffer_get_seq (&rtp);

  /* check if we have a retransmission packet (this information comes from SDP) */
  GST_OBJECT_LOCK (rtx);

  /* RIST sets SSRC LSB to 1 to indicate an RTC packet */
  is_rtx = ssrc & 0x1;
  rtx->last_time = GST_BUFFER_PTS (buffer);

  if (is_rtx)
    /* increase our statistic */
    ++rtx->num_rtx_packets;

  GST_OBJECT_UNLOCK (rtx);

  /* create the retransmission packet */
  if (is_rtx) {
    GST_DEBUG_OBJECT (rtx,
        "Recovered packet from RIST RTX seqnum:%u ssrc: %u",
        gst_rtp_buffer_get_seq (&rtp), gst_rtp_buffer_get_ssrc (&rtp));
    gst_rtp_buffer_set_ssrc (&rtp, ssrc & 0xFFFFFFFE);
    GST_BUFFER_FLAG_SET (buffer, GST_RTP_BUFFER_FLAG_RETRANSMISSION);
  }

  gst_rtp_buffer_unmap (&rtp);

  GST_TRACE_OBJECT (rtx, "pushing packet seqnum:%u from master stream "
      "ssrc: %X", seqnum, ssrc);
  return gst_pad_push (rtx->srcpad, buffer);

invalid_buffer:
  {
    GST_ELEMENT_WARNING (rtx, STREAM, DECODE, (NULL),
        ("Received invalid RTP payload, dropping"));
    gst_buffer_unref (buffer);
    return GST_FLOW_OK;
  }
}

static void
gst_rist_rtx_receive_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstRistRtxReceive *rtx = GST_RIST_RTX_RECEIVE (object);

  switch (prop_id) {
    case PROP_NUM_RTX_REQUESTS:
      GST_OBJECT_LOCK (rtx);
      g_value_set_uint (value, rtx->num_rtx_requests);
      GST_OBJECT_UNLOCK (rtx);
      break;
    case PROP_NUM_RTX_PACKETS:
      GST_OBJECT_LOCK (rtx);
      g_value_set_uint (value, rtx->num_rtx_packets);
      GST_OBJECT_UNLOCK (rtx);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_rist_rtx_receive_change_state (GstElement * element,
    GstStateChange transition)
{
  GstRistRtxReceive *rtx = GST_RIST_RTX_RECEIVE (element);
  GstStateChangeReturn ret;

  ret =
      GST_ELEMENT_CLASS (gst_rist_rtx_receive_parent_class)->change_state
      (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_rist_rtx_receive_reset (rtx);
      break;
    default:
      break;
  }

  return ret;
}
