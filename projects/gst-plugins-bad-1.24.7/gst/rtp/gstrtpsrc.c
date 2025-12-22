/* GStreamer
 * Copyright (C) <2018> Marc Leeman <marc.leeman@gmail.com>
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
 * SECTION: gstrtpsrc
 * @title: GstRtpSrc
 * @short description: element with Uri interface to get RTP data from
 * the network.
 *
 * RTP (RFC 3550) is a protocol to stream media over the network while
 * retaining the timing information and providing enough information to
 * reconstruct the correct timing domain by the receiver.
 *
 * The RTP data port should be even, while the RTCP port should be
 * odd. The URI that is entered defines the data port, the RTCP port will
 * be allocated to the next port.
 *
 * This element hooks up the correct sockets to support both RTP as the
 * accompanying RTCP layer.
 *
 * This Bin handles taking in of data from the network and provides the
 * RTP payloaded data.
 *
 * This element also implements the URI scheme `rtp://` allowing to render
 * RTP streams in GStreamer based media players. The RTP URI handler also
 * allows setting properties through the URI query.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#include <gst/net/net.h>
#include <gst/rtp/gstrtppayloads.h>

#include "gstrtpsrc.h"
#include "gstrtp-utils.h"

GST_DEBUG_CATEGORY_STATIC (gst_rtp_src_debug);
#define GST_CAT_DEFAULT gst_rtp_src_debug

#define DEFAULT_PROP_TTL              64
#define DEFAULT_PROP_TTL_MC           1
#define DEFAULT_PROP_ENCODING_NAME    NULL
#define DEFAULT_PROP_CAPS             NULL
#define DEFAULT_PROP_LATENCY          200

#define DEFAULT_PROP_ADDRESS          "0.0.0.0"
#define DEFAULT_PROP_PORT             5004
#define DEFAULT_PROP_URI              "rtp://"DEFAULT_PROP_ADDRESS":"G_STRINGIFY(DEFAULT_PROP_PORT)
#define DEFAULT_PROP_MULTICAST_IFACE  NULL

enum
{
  PROP_0,

  PROP_URI,
  PROP_ADDRESS,
  PROP_PORT,
  PROP_TTL,
  PROP_TTL_MC,
  PROP_ENCODING_NAME,
  PROP_LATENCY,
  PROP_MULTICAST_IFACE,
  PROP_CAPS,

  PROP_LAST
};

static void gst_rtp_src_uri_handler_init (gpointer g_iface,
    gpointer iface_data);

#define gst_rtp_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstRtpSrc, gst_rtp_src, GST_TYPE_BIN,
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER, gst_rtp_src_uri_handler_init);
    GST_DEBUG_CATEGORY_INIT (gst_rtp_src_debug, "rtpsrc", 0, "RTP Source"));
GST_ELEMENT_REGISTER_DEFINE (rtpsrc, "rtpsrc", GST_RANK_PRIMARY + 1,
    GST_TYPE_RTP_SRC);

#define GST_RTP_SRC_GET_LOCK(obj) (&((GstRtpSrc*)(obj))->lock)
#define GST_RTP_SRC_LOCK(obj) (g_mutex_lock (GST_RTP_SRC_GET_LOCK(obj)))
#define GST_RTP_SRC_UNLOCK(obj) (g_mutex_unlock (GST_RTP_SRC_GET_LOCK(obj)))

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src_%u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS ("application/x-rtp"));

static GstStateChangeReturn
gst_rtp_src_change_state (GstElement * element, GstStateChange transition);

/**
 * gst_rtp_src_rtpbin_request_pt_map_cb:
 * @self: The current #GstRtpSrc object
 *
 * #GstRtpBin callback to map a pt on RTP caps.
 *
 * Returns: (transfer none): the guess on the RTP caps based on the PT
 * and caps.
 */
static GstCaps *
gst_rtp_src_rtpbin_request_pt_map_cb (GstElement * rtpbin, guint session_id,
    guint pt, gpointer data)
{
  GstRtpSrc *self = GST_RTP_SRC (data);
  const GstRTPPayloadInfo *p = NULL;

  GST_DEBUG_OBJECT (self,
      "Requesting caps for session-id 0x%x and pt %u.", session_id, pt);

  if (G_UNLIKELY (self->caps)) {
    GST_DEBUG_OBJECT (self,
        "Full caps were set, no need for lookup %" GST_PTR_FORMAT, self->caps);
    return gst_caps_copy (self->caps);
  }

  /* the encoding-name has more relevant information */
  if (self->encoding_name != NULL) {
    /* Unfortunately, the media needs to be passed in the function. Since
     * it is not known, try for video if video not found. */
    p = gst_rtp_payload_info_for_name ("video", self->encoding_name);
    if (p == NULL)
      p = gst_rtp_payload_info_for_name ("audio", self->encoding_name);

  }

  /* If info has been found before based on the encoding-name, go with
   * it. If not, try to look it up on with a static one. Needs to be guarded
   * because some encoders do not use dynamic values for H.264 */
  if (p == NULL) {
    /* Static payload types, this is a simple lookup */
    if (!GST_RTP_PAYLOAD_IS_DYNAMIC (pt)) {
      p = gst_rtp_payload_info_for_pt (pt);
    }
  }

  if (p != NULL) {
    GstCaps *ret = gst_caps_new_simple ("application/x-rtp",
        "encoding-name", G_TYPE_STRING, p->encoding_name,
        "clock-rate", G_TYPE_INT, p->clock_rate,
        "media", G_TYPE_STRING, p->media, NULL);

    GST_DEBUG_OBJECT (self, "Decided on caps %" GST_PTR_FORMAT, ret);

    return ret;
  }

  GST_DEBUG_OBJECT (self, "Could not determine caps based on pt and"
      " the encoding-name was not set.");
  return NULL;
}

static void
gst_rtp_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRtpSrc *self = GST_RTP_SRC (object);
  GstCaps *caps;

  switch (prop_id) {
    case PROP_URI:{
      GstUri *uri = NULL;
      const gchar *str_uri = g_value_get_string (value);

      GST_RTP_SRC_LOCK (object);
      uri = gst_uri_from_string (str_uri);
      if (uri == NULL) {
        if (str_uri) {
          GST_RTP_SRC_UNLOCK (object);
          GST_ERROR_OBJECT (object, "Invalid uri: %s", str_uri);

          break;
        }
      }

      if (self->uri)
        gst_uri_unref (self->uri);
      self->uri = uri;

      if (!uri) {
        GST_RTP_SRC_UNLOCK (object);

        break;
      }

      /* Recursive set to self, do not use the same lock in all property
       * setters. */
      g_object_set (self, "address", gst_uri_get_host (self->uri), NULL);
      g_object_set (self, "port", gst_uri_get_port (self->uri), NULL);
      gst_rtp_utils_set_properties_from_uri_query (G_OBJECT (self), self->uri);
      GST_RTP_SRC_UNLOCK (object);
      break;
    }
    case PROP_ADDRESS:{
      gst_uri_set_host (self->uri, g_value_get_string (value));
      g_object_set_property (G_OBJECT (self->rtp_src), "address", value);
      g_object_set_property (G_OBJECT (self->rtcp_src), "address", value);
      break;
    }
    case PROP_PORT:{
      guint port = g_value_get_uint (value);

      /* According to RFC 3550, 11, RTCP receiver port should be even
       * number and RTCP port should be the RTP port + 1 */
      if (port & 0x1)
        GST_WARNING_OBJECT (self,
            "Port %u is odd, this is not standard (see RFC 3550).", port);

      gst_uri_set_port (self->uri, port);
      g_object_set (self->rtp_src, "port", port, NULL);
      g_object_set (self->rtcp_src, "port", port + 1, NULL);
      break;
    }
    case PROP_TTL:
      self->ttl = g_value_get_int (value);
      break;
    case PROP_TTL_MC:
      self->ttl_mc = g_value_get_int (value);
      break;
    case PROP_ENCODING_NAME:
      g_free (self->encoding_name);
      self->encoding_name = g_value_dup_string (value);
      if (self->rtp_src) {
        caps = gst_rtp_src_rtpbin_request_pt_map_cb (NULL, 0, 96, self);
        g_object_set (G_OBJECT (self->rtp_src), "caps", caps, NULL);
        gst_caps_unref (caps);
      }
      break;
    case PROP_LATENCY:
      g_object_set (self->rtpbin, "latency", g_value_get_uint (value), NULL);
      break;
    case PROP_MULTICAST_IFACE:
      g_free (self->multi_iface);

      if (g_value_get_string (value) == NULL)
        self->multi_iface = g_strdup (DEFAULT_PROP_MULTICAST_IFACE);
      else
        self->multi_iface = g_value_dup_string (value);
      break;
    case PROP_CAPS:
    {
      const GstCaps *new_caps_val = gst_value_get_caps (value);
      GstCaps *new_caps = NULL;
      GstCaps *old_caps = self->caps;

      if (new_caps_val != NULL) {
        new_caps = gst_caps_copy (new_caps_val);
      }

      self->caps = new_caps;

      if (old_caps)
        gst_caps_unref (old_caps);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rtp_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstRtpSrc *self = GST_RTP_SRC (object);

  switch (prop_id) {
    case PROP_URI:
      GST_RTP_SRC_LOCK (object);
      if (self->uri)
        g_value_take_string (value, gst_uri_to_string (self->uri));
      else
        g_value_set_string (value, NULL);
      GST_RTP_SRC_UNLOCK (object);
      break;
    case PROP_ADDRESS:
      g_value_set_string (value, gst_uri_get_host (self->uri));
      break;
    case PROP_PORT:
      g_value_set_uint (value, gst_uri_get_port (self->uri));
      break;
    case PROP_TTL:
      g_value_set_int (value, self->ttl);
      break;
    case PROP_TTL_MC:
      g_value_set_int (value, self->ttl_mc);
      break;
    case PROP_ENCODING_NAME:
      g_value_set_string (value, self->encoding_name);
      break;
    case PROP_LATENCY:
      g_object_get_property (G_OBJECT (self->rtpbin), "latency", value);
      break;
    case PROP_MULTICAST_IFACE:
      g_value_set_string (value, self->multi_iface);
      break;
    case PROP_CAPS:
      gst_value_set_caps (value, self->caps);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rtp_src_finalize (GObject * gobject)
{
  GstRtpSrc *self = GST_RTP_SRC (gobject);

  if (self->uri)
    gst_uri_unref (self->uri);
  g_free (self->encoding_name);

  g_free (self->multi_iface);

  if (self->caps)
    gst_caps_unref (self->caps);

  g_clear_object (&self->rtcp_send_addr);

  g_mutex_clear (&self->lock);
  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
gst_rtp_src_handle_message (GstBin * bin, GstMessage * message)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_STREAM_START:
    case GST_MESSAGE_EOS:
      /* drop stream-start & eos from our internal udp sink(s);
         https://gitlab.freedesktop.org/gstreamer/gst-plugins-bad/-/issues/1368 */
      gst_message_unref (message);
      break;
    default:
      GST_BIN_CLASS (parent_class)->handle_message (bin, message);
      break;
  }
}

static void
gst_rtp_src_constructed (GObject * object)
{
  gchar name[48];
  GstRtpSrc *self = GST_RTP_SRC (object);
  gchar *tmpname;

  if (!self->rtp_src || !self->rtcp_src || !self->rtcp_sink)
    return;

  tmpname = g_strdup_printf ("%s_rtp_udpsrc", GST_OBJECT_NAME (self));
  gst_object_set_name (GST_OBJECT (self->rtp_src), tmpname);
  g_free (tmpname);

  tmpname = g_strdup_printf ("%s_rtcp_udpsrc", GST_OBJECT_NAME (self));
  gst_object_set_name (GST_OBJECT (self->rtcp_src), tmpname);
  g_free (tmpname);

  tmpname = g_strdup_printf ("%s_rtcp_udpsink", GST_OBJECT_NAME (self));
  gst_object_set_name (GST_OBJECT (self->rtcp_sink), tmpname);
  g_free (tmpname);

  /* Add elements as needed, since udpsrc/udpsink for RTCP share a socket,
   * not all at the same moment */
  gst_bin_add (GST_BIN (self), self->rtp_src);
  gst_bin_add (GST_BIN (self), self->rtcp_src);
  gst_bin_add (GST_BIN (self), self->rtcp_sink);

  g_object_set (self->rtcp_sink, "sync", FALSE, "async", FALSE, NULL);
  gst_element_set_locked_state (self->rtcp_sink, TRUE);

  /* pads are all named */
  g_snprintf (name, 48, "recv_rtp_sink_%u", GST_ELEMENT (self)->numpads);
  gst_element_link_pads (self->rtp_src, "src", self->rtpbin, name);
  g_snprintf (name, 48, "recv_rtcp_sink_%u", GST_ELEMENT (self)->numpads);
  gst_element_link_pads (self->rtcp_src, "src", self->rtpbin, name);
  g_snprintf (name, 48, "send_rtcp_src_%u", GST_ELEMENT (self)->numpads);
  gst_element_link_pads (self->rtpbin, name, self->rtcp_sink, "sink");
}

static void
gst_rtp_src_class_init (GstRtpSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBinClass *gstbin_class = GST_BIN_CLASS (klass);

  gobject_class->set_property = gst_rtp_src_set_property;
  gobject_class->get_property = gst_rtp_src_get_property;
  gobject_class->constructed = gst_rtp_src_constructed;
  gobject_class->finalize = gst_rtp_src_finalize;
  gstelement_class->change_state = gst_rtp_src_change_state;
  gstbin_class->handle_message = gst_rtp_src_handle_message;

  /**
   * GstRtpSrc:uri:
   *
   * uri to an RTP from. All GStreamer parameters can be
   * encoded in the URI, this URI format is RFC compliant.
   */
  g_object_class_install_property (gobject_class, PROP_URI,
      g_param_spec_string ("uri", "URI",
          "URI in the form of rtp://host:port?query", DEFAULT_PROP_URI,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRtpSrc:address:
   *
   * Address to receive packets from (can be IPv4 or IPv6).
   */
  g_object_class_install_property (gobject_class, PROP_ADDRESS,
      g_param_spec_string ("address", "Address",
          "Address to receive packets from (can be IPv4 or IPv6).",
          DEFAULT_PROP_ADDRESS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRtpSrc:port:
   *
   * The port to listen to RTP packets, the RTCP port is this value
   * +1. This port must be an even number.
   */
  g_object_class_install_property (gobject_class, PROP_PORT,
      g_param_spec_uint ("port", "Port", "The port to listen for RTP packets, "
          "the RTCP port is this value + 1. This port must be an even number.",
          2, 65534, DEFAULT_PROP_PORT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

  /**
   * GstRtpSrc:ttl:
   *
   * Set the unicast TTL parameter. In RTP this of importance for RTCP.
   */
  g_object_class_install_property (gobject_class, PROP_TTL,
      g_param_spec_int ("ttl", "Unicast TTL",
          "Used for setting the unicast TTL parameter",
          0, 255, DEFAULT_PROP_TTL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRtpSrc:ttl-mc:
   *
   * Set the multicast TTL parameter. In RTP this of importance for RTCP.
   */
  g_object_class_install_property (gobject_class, PROP_TTL_MC,
      g_param_spec_int ("ttl-mc", "Multicast TTL",
          "Used for setting the multicast TTL parameter", 0, 255,
          DEFAULT_PROP_TTL_MC, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRtpSrc:encoding-name:
   *
   * Set the encoding name of the stream to use. This is a short-hand for
   * the full caps and maps typically to the encoding-name in the RTP caps.
   */
  g_object_class_install_property (gobject_class, PROP_ENCODING_NAME,
      g_param_spec_string ("encoding-name", "Caps encoding name",
          "Encoding name use to determine caps parameters",
          DEFAULT_PROP_ENCODING_NAME,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRtpSrc:latency:
   *
   * Set the size of the latency buffer in the
   * GstRtpBin/GstRtpJitterBuffer to compensate for network jitter.
   */
  g_object_class_install_property (gobject_class, PROP_LATENCY,
      g_param_spec_uint ("latency", "Buffer latency in ms",
          "Default amount of ms to buffer in the jitterbuffers", 0,
          G_MAXUINT, DEFAULT_PROP_LATENCY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRtpSink:multicast-iface:
   *
   * The networkinterface on which to join the multicast group
   */
  g_object_class_install_property (gobject_class, PROP_MULTICAST_IFACE,
      g_param_spec_string ("multicast-iface", "Multicast Interface",
          "The network interface on which to join the multicast group."
          "This allows multiple interfaces separated by comma. (\"eth0,eth1\")",
          DEFAULT_PROP_MULTICAST_IFACE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
  * GstRtpSrc:caps:
  *
  * The RTP caps of the incoming stream.
  *
  * Since: 1.20
  */
  g_object_class_install_property (gobject_class, PROP_CAPS,
      g_param_spec_boxed ("caps", "Caps",
          "The caps of the incoming stream", GST_TYPE_CAPS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "RTP Source element",
      "Generic/Bin/Src",
      "Simple RTP src", "Marc Leeman <marc.leeman@gmail.com>");
}

static void
clear_ssrc (GstElement * rtpbin, GstPad * gpad)
{
  GstPad *pad;
  gint pt;
  guint ssrc;

  pad = gst_ghost_pad_get_target (GST_GHOST_PAD (gpad));
  if (!pad)
    return;

  if (sscanf (GST_PAD_NAME (pad), "recv_rtp_src_0_%u_%d", &ssrc, &pt) != 2) {
    gst_object_unref (pad);
    return;
  }
  gst_object_unref (pad);

  g_signal_emit_by_name (rtpbin, "clear-ssrc", 0, ssrc);
}

static void
gst_rtp_src_rtpbin_pad_added_cb (GstElement * element, GstPad * pad,
    gpointer data)
{
  GstRtpSrc *self = GST_RTP_SRC (data);
  GstCaps *caps = gst_pad_query_caps (pad, NULL);
  const GstStructure *s;
  GstPad *upad = NULL;
  gint pt = -1;
  gchar name[48];

  /* Expose RTP data pad only */
  GST_INFO_OBJECT (self,
      "Element %" GST_PTR_FORMAT " added pad %" GST_PTR_FORMAT "with caps %"
      GST_PTR_FORMAT ".", element, pad, caps);

  /* Sanity checks */
  if (GST_PAD_DIRECTION (pad) == GST_PAD_SINK) {
    /* Sink pad, do not expose */
    gst_caps_unref (caps);
    return;
  }

  if (G_LIKELY (caps)) {
    GstCaps *ref_caps = gst_caps_new_empty_simple ("application/x-rtcp");

    if (gst_caps_can_intersect (caps, ref_caps)) {
      /* SRC RTCP caps, do not expose */
      gst_caps_unref (ref_caps);
      gst_caps_unref (caps);

      return;
    }
    gst_caps_unref (ref_caps);
  } else {
    GST_ERROR_OBJECT (self, "Pad with no caps detected.");
    gst_caps_unref (caps);

    return;
  }

  s = gst_caps_get_structure (caps, 0);
  gst_structure_get_int (s, "payload", &pt);
  gst_caps_unref (caps);

  GST_RTP_SRC_LOCK (self);
  g_snprintf (name, 48, "src_%u", pt);
  upad = gst_element_get_static_pad (GST_ELEMENT (self), name);

  if (!upad) {
    GST_DEBUG_OBJECT (self, "Adding new pad: %s", name);

    upad = gst_ghost_pad_new (name, pad);
    gst_pad_set_active (upad, TRUE);
    gst_element_add_pad (GST_ELEMENT (self), upad);
  } else {
    GST_DEBUG_OBJECT (self, "Re-using existing pad: %s", GST_PAD_NAME (upad));
    clear_ssrc (element, upad);
    gst_ghost_pad_set_target (GST_GHOST_PAD (upad), pad);
    gst_object_unref (upad);
  }
  GST_RTP_SRC_UNLOCK (self);
}

static void
gst_rtp_src_rtpbin_pad_removed_cb (GstElement * element, GstPad * pad,
    gpointer data)
{
  GstRtpSrc *self = GST_RTP_SRC (data);
  GST_INFO_OBJECT (self,
      "Element %" GST_PTR_FORMAT " removed pad %" GST_PTR_FORMAT ".", element,
      pad);
}

static void
gst_rtp_src_rtpbin_on_ssrc_collision_cb (GstElement * rtpbin, guint session_id,
    guint ssrc, gpointer data)
{
  GstRtpSrc *self = GST_RTP_SRC (data);

  GST_INFO_OBJECT (self,
      "Detected an SSRC collision: session-id 0x%x, ssrc 0x%x.", session_id,
      ssrc);
}

static void
gst_rtp_src_rtpbin_on_new_ssrc_cb (GstElement * rtpbin, guint session_id,
    guint ssrc, gpointer data)
{
  GstRtpSrc *self = GST_RTP_SRC (data);

  GST_INFO_OBJECT (self, "Detected a new SSRC: session-id 0x%x, ssrc 0x%x.",
      session_id, ssrc);
}

static GstPadProbeReturn
gst_rtp_src_on_recv_rtcp (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  GstRtpSrc *self = GST_RTP_SRC (user_data);
  GstBuffer *buffer;
  GstNetAddressMeta *meta;

  if (info->type == GST_PAD_PROBE_TYPE_BUFFER_LIST) {
    GstBufferList *buffer_list = info->data;
    buffer = gst_buffer_list_get (buffer_list, 0);
  } else {
    buffer = info->data;
  }

  meta = gst_buffer_get_net_address_meta (buffer);

  GST_OBJECT_LOCK (self);
  g_clear_object (&self->rtcp_send_addr);
  self->rtcp_send_addr = g_object_ref (meta->addr);
  GST_OBJECT_UNLOCK (self);

  return GST_PAD_PROBE_OK;
}

static inline void
gst_rtp_src_attach_net_address_meta (GstRtpSrc * self, GstBuffer * buffer)
{
  GST_OBJECT_LOCK (self);
  if (self->rtcp_send_addr)
    gst_buffer_add_net_address_meta (buffer, self->rtcp_send_addr);
  GST_OBJECT_UNLOCK (self);
}

static GstPadProbeReturn
gst_rtp_src_on_send_rtcp (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  GstRtpSrc *self = GST_RTP_SRC (user_data);

  if (info->type == GST_PAD_PROBE_TYPE_BUFFER_LIST) {
    GstBufferList *buffer_list = info->data;
    GstBuffer *buffer;
    gint i;

    info->data = buffer_list = gst_buffer_list_make_writable (buffer_list);
    for (i = 0; i < gst_buffer_list_length (buffer_list); i++) {
      buffer = gst_buffer_list_get (buffer_list, i);
      gst_rtp_src_attach_net_address_meta (self, buffer);
    }
  } else {
    GstBuffer *buffer = info->data;
    info->data = buffer = gst_buffer_make_writable (buffer);
    gst_rtp_src_attach_net_address_meta (self, buffer);
  }

  return GST_PAD_PROBE_OK;
}

static gboolean
gst_rtp_src_start (GstRtpSrc * self)
{
  GstPad *pad;
  GSocket *socket;
  GInetAddress *iaddr;
  GstCaps *caps;
  GError *error = NULL;

  /* Should not be NULL */
  g_return_val_if_fail (self->uri != NULL, FALSE);

  /* share the socket created by the source */
  g_object_get (G_OBJECT (self->rtcp_src), "used-socket", &socket, NULL);
  if (!G_IS_SOCKET (socket)) {
    GST_WARNING_OBJECT (self, "Could not retrieve RTCP src socket.");
  }

  iaddr = g_inet_address_new_from_string (gst_uri_get_host (self->uri));
  if (!iaddr) {
    GList *results;
    GResolver *resolver = NULL;

    resolver = g_resolver_get_default ();
    results =
        g_resolver_lookup_by_name (resolver, gst_uri_get_host (self->uri), NULL,
        &error);

    if (!results) {
      g_object_unref (resolver);
      goto dns_resolve_failed;
    }

    iaddr = G_INET_ADDRESS (g_object_ref (results->data));

    g_resolver_free_addresses (results);
    g_object_unref (resolver);
  }

  if (g_inet_address_get_is_multicast (iaddr)) {
    /* mc-ttl is not supported by dynudpsink */
    g_socket_set_multicast_ttl (socket, self->ttl_mc);
    /* In multicast, send RTCP to the multicast group */
    self->rtcp_send_addr =
        g_inet_socket_address_new (iaddr, gst_uri_get_port (self->uri) + 1);

    /* set multicast-iface on the udpsrc and udpsink elements */
    g_object_set (self->rtcp_src, "multicast-iface", self->multi_iface, NULL);
    g_object_set (self->rtp_src, "multicast-iface", self->multi_iface, NULL);
  } else {
    /* In unicast, send RTCP to the detected sender address */
    g_socket_set_ttl (socket, self->ttl);
    pad = gst_element_get_static_pad (self->rtcp_src, "src");
    self->rtcp_recv_probe = gst_pad_add_probe (pad,
        GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_BUFFER_LIST,
        gst_rtp_src_on_recv_rtcp, self, NULL);
    gst_object_unref (pad);
  }
  g_object_unref (iaddr);

  /* no need to set address if unicast */
  caps = gst_caps_new_empty_simple ("application/x-rtcp");
  g_object_set (self->rtcp_src, "caps", caps, NULL);
  gst_caps_unref (caps);

  pad = gst_element_get_static_pad (self->rtcp_sink, "sink");
  self->rtcp_send_probe = gst_pad_add_probe (pad,
      GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_BUFFER_LIST,
      gst_rtp_src_on_send_rtcp, self, NULL);
  gst_object_unref (pad);

  g_object_set (self->rtcp_sink, "socket", socket, "close-socket", FALSE, NULL);
  g_object_unref (socket);

  gst_element_set_locked_state (self->rtcp_sink, FALSE);
  gst_element_sync_state_with_parent (self->rtcp_sink);

  return TRUE;

dns_resolve_failed:
  GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND,
      ("Could not resolve hostname '%s'", gst_uri_get_host (self->uri)),
      ("DNS resolver reported: %s", error->message));
  g_error_free (error);
  return FALSE;
}

static void
gst_rtp_src_stop (GstRtpSrc * self)
{
  GstPad *pad;

  if (self->rtcp_recv_probe) {
    pad = gst_element_get_static_pad (self->rtcp_src, "src");
    gst_pad_remove_probe (pad, self->rtcp_recv_probe);
    self->rtcp_recv_probe = 0;
    gst_object_unref (pad);
  }

  pad = gst_element_get_static_pad (self->rtcp_sink, "sink");
  gst_pad_remove_probe (pad, self->rtcp_send_probe);
  self->rtcp_send_probe = 0;
  gst_object_unref (pad);
}

static GstStateChangeReturn
gst_rtp_src_change_state (GstElement * element, GstStateChange transition)
{
  GstRtpSrc *self = GST_RTP_SRC (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG_OBJECT (self, "Changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (gst_rtp_src_start (self) == FALSE)
        return GST_STATE_CHANGE_FAILURE;
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      ret = GST_STATE_CHANGE_NO_PREROLL;
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      ret = GST_STATE_CHANGE_NO_PREROLL;
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_rtp_src_stop (self);
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_rtp_src_init (GstRtpSrc * self)
{
  const gchar *missing_plugin = NULL;

  self->rtpbin = NULL;
  self->rtp_src = NULL;
  self->rtcp_src = NULL;
  self->rtcp_sink = NULL;
  self->multi_iface = g_strdup (DEFAULT_PROP_MULTICAST_IFACE);

  self->uri = gst_uri_from_string (DEFAULT_PROP_URI);
  self->ttl = DEFAULT_PROP_TTL;
  self->ttl_mc = DEFAULT_PROP_TTL_MC;
  self->encoding_name = DEFAULT_PROP_ENCODING_NAME;
  self->caps = DEFAULT_PROP_CAPS;

  GST_OBJECT_FLAG_SET (GST_OBJECT (self), GST_ELEMENT_FLAG_SOURCE);
  gst_bin_set_suppressed_flags (GST_BIN (self),
      GST_ELEMENT_FLAG_SOURCE | GST_ELEMENT_FLAG_SINK);

  g_mutex_init (&self->lock);

  /* Construct the RTP receiver pipeline.
   *
   * udpsrc -> [recv_rtp_sink_%u]  --------  [recv_rtp_src_%u_%u_%u]
   *                              | rtpbin |
   * udpsrc -> [recv_rtcp_sink_%u] --------  [send_rtcp_src_%u] -> udpsink
   *
   * This pipeline is fixed for now, note that optionally an FEC stream could
   * be added later.
   */

  self->rtpbin = gst_element_factory_make ("rtpbin", "rtp_recv_rtpbin0");
  if (self->rtpbin == NULL) {
    missing_plugin = "rtpmanager";
    goto missing_plugin;
  }
  g_object_set (self->rtpbin, "autoremove", TRUE, NULL);

  gst_bin_add (GST_BIN (self), self->rtpbin);

  /* Add rtpbin callbacks to monitor the operation of rtpbin */
  g_signal_connect_object (self->rtpbin, "pad-added",
      G_CALLBACK (gst_rtp_src_rtpbin_pad_added_cb), self, 0);
  g_signal_connect_object (self->rtpbin, "pad-removed",
      G_CALLBACK (gst_rtp_src_rtpbin_pad_removed_cb), self, 0);
  g_signal_connect_object (self->rtpbin, "request-pt-map",
      G_CALLBACK (gst_rtp_src_rtpbin_request_pt_map_cb), self, 0);
  g_signal_connect_object (self->rtpbin, "on-new-ssrc",
      G_CALLBACK (gst_rtp_src_rtpbin_on_new_ssrc_cb), self, 0);
  g_signal_connect_object (self->rtpbin, "on-ssrc-collision",
      G_CALLBACK (gst_rtp_src_rtpbin_on_ssrc_collision_cb), self, 0);

  self->rtp_src = gst_element_factory_make ("udpsrc", "rtp_rtp_udpsrc0");
  if (self->rtp_src == NULL) {
    missing_plugin = "udp";
    goto missing_plugin;
  }

  self->rtcp_src = gst_element_factory_make ("udpsrc", "rtp_rtcp_udpsrc0");
  if (self->rtcp_src == NULL) {
    missing_plugin = "udp";
    goto missing_plugin;
  }

  self->rtcp_sink =
      gst_element_factory_make ("dynudpsink", "rtp_rtcp_dynudpsink0");
  if (self->rtcp_sink == NULL) {
    missing_plugin = "udp";
    goto missing_plugin;
  }

  if (missing_plugin == NULL)
    return;

missing_plugin:
  {
    GST_ERROR_OBJECT (self, "'%s' plugin is missing.", missing_plugin);
  }
}

static GstURIType
gst_rtp_src_uri_get_type (GType type)
{
  return GST_URI_SRC;
}

static const gchar *const *
gst_rtp_src_uri_get_protocols (GType type)
{
  static const gchar *protocols[] = { (char *) "rtp", NULL };

  return protocols;
}

static gchar *
gst_rtp_src_uri_get_uri (GstURIHandler * handler)
{
  GstRtpSrc *self = (GstRtpSrc *) handler;

  return gst_uri_to_string (self->uri);
}

static gboolean
gst_rtp_src_uri_set_uri (GstURIHandler * handler, const gchar * uri,
    GError ** error)
{
  GstRtpSrc *self = (GstRtpSrc *) handler;

  g_object_set (G_OBJECT (self), "uri", uri, NULL);

  return TRUE;
}

static void
gst_rtp_src_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_rtp_src_uri_get_type;
  iface->get_protocols = gst_rtp_src_uri_get_protocols;
  iface->get_uri = gst_rtp_src_uri_get_uri;
  iface->set_uri = gst_rtp_src_uri_set_uri;
}

/* ex: set tabstop=2 shiftwidth=2 expandtab: */
