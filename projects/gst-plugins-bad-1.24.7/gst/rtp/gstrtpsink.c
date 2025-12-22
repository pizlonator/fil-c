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
 * SECTION: gstrtsinkp
 * @title: GstRtpSink
 * @short description: element with Uri interface to stream RTP data to
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
 * This Bin handles streaming RTP payloaded data on the network.
 *
 * This element also implements the URI scheme `rtp://` allowing to send
 * data on the network by bins that allow use the URI to determine the sink.
 * The RTP URI handler also allows setting properties through the URI query.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>

#include "gstrtpsink.h"
#include "gstrtp-utils.h"

GST_DEBUG_CATEGORY_STATIC (gst_rtp_sink_debug);
#define GST_CAT_DEFAULT gst_rtp_sink_debug

#define DEFAULT_PROP_TTL              64
#define DEFAULT_PROP_TTL_MC           1

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
  PROP_MULTICAST_IFACE,

  PROP_LAST
};

static void gst_rtp_sink_uri_handler_init (gpointer g_iface,
    gpointer iface_data);

#define gst_rtp_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstRtpSink, gst_rtp_sink, GST_TYPE_BIN,
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER, gst_rtp_sink_uri_handler_init);
    GST_DEBUG_CATEGORY_INIT (gst_rtp_sink_debug, "rtpsink", 0, "RTP Sink"));
GST_ELEMENT_REGISTER_DEFINE (rtpsink, "rtpsink", GST_RANK_PRIMARY + 1,
    GST_TYPE_RTP_SINK);

#define GST_RTP_SINK_GET_LOCK(obj) (&((GstRtpSink*)(obj))->lock)
#define GST_RTP_SINK_LOCK(obj) (g_mutex_lock (GST_RTP_SINK_GET_LOCK(obj)))
#define GST_RTP_SINK_UNLOCK(obj) (g_mutex_unlock (GST_RTP_SINK_GET_LOCK(obj)))

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("application/x-rtp"));

static GstStateChangeReturn
gst_rtp_sink_change_state (GstElement * element, GstStateChange transition);

static void
gst_rtp_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRtpSink *self = GST_RTP_SINK (object);

  switch (prop_id) {
    case PROP_URI:{
      GstUri *uri = NULL;

      GST_RTP_SINK_LOCK (object);
      uri = gst_uri_from_string (g_value_get_string (value));
      if (uri == NULL)
        break;

      if (self->uri)
        gst_uri_unref (self->uri);
      self->uri = uri;

      gst_rtp_utils_set_properties_from_uri_query (G_OBJECT (self), self->uri);

      g_object_set (self, "address", gst_uri_get_host (self->uri), NULL);
      g_object_set (self, "port", gst_uri_get_port (self->uri), NULL);

      GST_RTP_SINK_UNLOCK (object);
      break;
    }
    case PROP_ADDRESS:
      gst_uri_set_host (self->uri, g_value_get_string (value));
      g_object_set_property (G_OBJECT (self->rtp_sink), "host", value);
      g_object_set_property (G_OBJECT (self->rtcp_sink), "host", value);
      break;

    case PROP_PORT:{
      guint port = g_value_get_uint (value);

      /* According to RFC 3550, 11, RTCP receiver port should be even
       * number and RTCP port should be the RTP port + 1 */
      if (port & 0x1)
        GST_WARNING_OBJECT (self,
            "Port %u is odd, this is not standard (see RFC 3550).", port);

      gst_uri_set_port (self->uri, port);
      g_object_set (self->rtp_sink, "port", port, NULL);
      g_object_set (self->rtcp_sink, "port", port + 1, NULL);
      break;
    }
    case PROP_TTL:
      self->ttl = g_value_get_int (value);
      g_object_set (self->rtp_sink, "ttl", self->ttl, NULL);
      g_object_set (self->rtcp_sink, "ttl", self->ttl, NULL);
      break;
    case PROP_TTL_MC:
      self->ttl_mc = g_value_get_int (value);
      g_object_set (self->rtp_sink, "ttl-mc", self->ttl_mc, NULL);
      g_object_set (self->rtcp_sink, "ttl-mc", self->ttl_mc, NULL);
      break;
    case PROP_MULTICAST_IFACE:
      g_free (self->multi_iface);

      if (g_value_get_string (value) == NULL)
        self->multi_iface = g_strdup (DEFAULT_PROP_MULTICAST_IFACE);
      else
        self->multi_iface = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rtp_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstRtpSink *self = GST_RTP_SINK (object);

  switch (prop_id) {
    case PROP_URI:
      GST_RTP_SINK_LOCK (object);
      if (self->uri)
        g_value_take_string (value, gst_uri_to_string (self->uri));
      else
        g_value_set_string (value, NULL);
      GST_RTP_SINK_UNLOCK (object);
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
    case PROP_MULTICAST_IFACE:
      g_value_set_string (value, self->multi_iface);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rtp_sink_finalize (GObject * gobject)
{
  GstRtpSink *self = GST_RTP_SINK (gobject);

  if (self->uri)
    gst_uri_unref (self->uri);

  g_free (self->multi_iface);

  g_mutex_clear (&self->lock);
  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static gboolean
gst_rtp_sink_setup_elements (GstRtpSink * self)
{
  /*GstPad *pad; */
  gchar name[48];

  /* pads are all named */
  g_snprintf (name, 48, "send_rtp_src_%u", GST_ELEMENT (self)->numpads);
  gst_element_link_pads (self->rtpbin, name, self->funnel_rtp, "sink_%u");

  g_snprintf (name, 48, "send_rtcp_src_%u", GST_ELEMENT (self)->numpads);
  gst_element_link_pads (self->rtpbin, name, self->funnel_rtcp, "sink_%u");

  g_snprintf (name, 48, "recv_rtcp_sink_%u", GST_ELEMENT (self)->numpads);
  gst_element_link_pads (self->rtcp_src, "src", self->rtpbin, name);

  return TRUE;
}

static GstPad *
gst_rtp_sink_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
  GstRtpSink *self = GST_RTP_SINK (element);
  GstPad *rpad, *pad = NULL;

  if (self->rtpbin == NULL) {
    GST_ELEMENT_ERROR (self, CORE, MISSING_PLUGIN, (NULL),
        ("%s", "rtpbin element is not available"));
    return NULL;
  }

  if (gst_rtp_sink_setup_elements (self) == FALSE)
    return NULL;

  GST_RTP_SINK_LOCK (self);
  rpad = gst_element_request_pad_simple (self->rtpbin, "send_rtp_sink_%u");
  if (rpad) {
    pad = gst_ghost_pad_new (GST_PAD_NAME (rpad), rpad);
    gst_element_add_pad (element, pad);
    gst_clear_object (&rpad);
  }
  GST_RTP_SINK_UNLOCK (self);

  g_return_val_if_fail (pad != NULL, NULL);


  return pad;
}

static void
gst_rtp_sink_release_pad (GstElement * element, GstPad * pad)
{
  GstRtpSink *self = GST_RTP_SINK (element);
  GstPad *rpad = gst_ghost_pad_get_target (GST_GHOST_PAD (pad));

  GST_RTP_SINK_LOCK (self);
  gst_element_release_request_pad (self->rtpbin, rpad);
  gst_object_unref (rpad);

  gst_pad_set_active (pad, FALSE);
  gst_element_remove_pad (GST_ELEMENT (self), pad);

  GST_RTP_SINK_UNLOCK (self);
}

static void
gst_rtp_sink_class_init (GstRtpSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->set_property = gst_rtp_sink_set_property;
  gobject_class->get_property = gst_rtp_sink_get_property;
  gobject_class->finalize = gst_rtp_sink_finalize;
  gstelement_class->change_state = gst_rtp_sink_change_state;

  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_rtp_sink_request_new_pad);
  gstelement_class->release_pad = GST_DEBUG_FUNCPTR (gst_rtp_sink_release_pad);

  /**
   * GstRtpSink:uri:
   *
   * uri to stream RTP to. All GStreamer parameters can be
   * encoded in the URI, this URI format is RFC compliant.
   */
  g_object_class_install_property (gobject_class, PROP_URI,
      g_param_spec_string ("uri", "URI",
          "URI in the form of rtp://host:port?query", DEFAULT_PROP_URI,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRtpSink:address:
   *
   * Address to receive packets from (can be IPv4 or IPv6).
   */
  g_object_class_install_property (gobject_class, PROP_ADDRESS,
      g_param_spec_string ("address", "Address",
          "Address to send packets to (can be IPv4 or IPv6).",
          DEFAULT_PROP_ADDRESS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRtpSink:port:
   *
   * The port to listen to RTP packets, the RTCP port is this value
   * +1. This port must be an even number.
   */
  g_object_class_install_property (gobject_class, PROP_PORT,
      g_param_spec_uint ("port", "Port", "The port RTP packets will be sent, "
          "the RTCP port is this value + 1. This port must be an even number.",
          2, 65534, DEFAULT_PROP_PORT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

  /**
   * GstRtpSink:ttl:
   *
   * Set the unicast TTL parameter.
   */
  g_object_class_install_property (gobject_class, PROP_TTL,
      g_param_spec_int ("ttl", "Unicast TTL",
          "Used for setting the unicast TTL parameter",
          0, 255, DEFAULT_PROP_TTL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRtpSink:ttl-mc:
   *
   * Set the multicast TTL parameter.
   */
  g_object_class_install_property (gobject_class, PROP_TTL_MC,
      g_param_spec_int ("ttl-mc", "Multicast TTL",
          "Used for setting the multicast TTL parameter", 0, 255,
          DEFAULT_PROP_TTL_MC, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* GstRtpSink:multicast-iface:
   *
   * The networkinterface on which to join the multicast group
   */
  g_object_class_install_property (gobject_class, PROP_MULTICAST_IFACE,
      g_param_spec_string ("multicast-iface", "Multicast Interface",
          "The network interface on which to join the multicast group."
          "This allows multiple interfaces separated by comma. (\"eth0,eth1\")",
          DEFAULT_PROP_MULTICAST_IFACE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "RTP Sink element",
      "Generic/Bin/Sink",
      "Simple RTP sink", "Marc Leeman <marc.leeman@gmail.com>");
}

static void
gst_rtp_sink_rtpbin_element_added_cb (GstBin * element,
    GstElement * new_element, gpointer data)
{
  GstRtpSink *self = GST_RTP_SINK (data);
  GST_INFO_OBJECT (self,
      "Element %" GST_PTR_FORMAT " added element %" GST_PTR_FORMAT ".", element,
      new_element);
}

static void
gst_rtp_sink_rtpbin_pad_added_cb (GstElement * element, GstPad * pad,
    gpointer data)
{
  GstRtpSink *self = GST_RTP_SINK (data);
  GstCaps *caps = gst_pad_query_caps (pad, NULL);
  GstPad *upad;

  /* Expose RTP data pad only */
  GST_INFO_OBJECT (self,
      "Element %" GST_PTR_FORMAT " added pad %" GST_PTR_FORMAT "with caps %"
      GST_PTR_FORMAT ".", element, pad, caps);

  /* Sanity checks */
  if (GST_PAD_DIRECTION (pad) == GST_PAD_SINK) {
    /* Src pad, do not expose */
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
  gst_caps_unref (caps);

  upad = gst_element_get_compatible_pad (self->funnel_rtp, pad, NULL);
  if (upad == NULL) {
    GST_ERROR_OBJECT (self, "No compatible pad found to link pad.");
    gst_caps_unref (caps);

    return;
  }
  GST_INFO_OBJECT (self, "Linking with pad %" GST_PTR_FORMAT ".", upad);
  gst_pad_link (pad, upad);
  gst_object_unref (upad);
}

static void
gst_rtp_sink_rtpbin_pad_removed_cb (GstElement * element, GstPad * pad,
    gpointer data)
{
  GstRtpSink *self = GST_RTP_SINK (data);
  GST_INFO_OBJECT (self,
      "Element %" GST_PTR_FORMAT " removed pad %" GST_PTR_FORMAT ".", element,
      pad);
}

static gboolean
gst_rtp_sink_reuse_socket (GstRtpSink * self)
{
  GSocket *socket = NULL;

  gst_element_set_locked_state (self->rtcp_src, FALSE);
  gst_element_sync_state_with_parent (self->rtcp_src);

  /* share the socket created by the sink */
  g_object_get (self->rtcp_src, "used-socket", &socket, NULL);
  g_object_set (self->rtcp_sink, "socket", socket, "auto-multicast", FALSE,
      "close-socket", FALSE, NULL);
  g_object_unref (socket);

  g_object_set (self->rtcp_sink, "sync", FALSE, "async", FALSE, NULL);
  gst_element_set_locked_state (self->rtcp_sink, FALSE);
  gst_element_sync_state_with_parent (self->rtcp_sink);

  return TRUE;

}

static gboolean
gst_rtp_sink_start (GstRtpSink * self)
{
  GInetAddress *iaddr = NULL;
  gchar *remote_addr = NULL;
  GError *error = NULL;

  /* Should not be NULL */
  g_return_val_if_fail (self->uri != NULL, FALSE);

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
  remote_addr = g_inet_address_to_string (iaddr);

  if (g_inet_address_get_is_multicast (iaddr)) {
    g_object_set (self->rtcp_src, "address", remote_addr, "port",
        gst_uri_get_port (self->uri) + 1, NULL);

    /* set multicast-iface on the udpsrc and udpsink elements */
    g_object_set (self->rtcp_src, "multicast-iface", self->multi_iface, NULL);
    g_object_set (self->rtcp_sink, "multicast-iface", self->multi_iface, NULL);
    g_object_set (self->rtp_sink, "multicast-iface", self->multi_iface, NULL);
  } else {
    const gchar *any_addr;

    if (g_inet_address_get_family (iaddr) == G_SOCKET_FAMILY_IPV6)
      any_addr = "::";
    else
      any_addr = "0.0.0.0";

    g_object_set (self->rtcp_src, "address", any_addr, "port", 0, NULL);
  }
  g_free (remote_addr);
  g_object_unref (iaddr);

  return TRUE;

dns_resolve_failed:
  GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND,
      ("Could not resolve hostname '%s'", gst_uri_get_host (self->uri)),
      ("DNS resolver reported: %s", error->message));
  g_error_free (error);
  return FALSE;
}

static GstStateChangeReturn
gst_rtp_sink_change_state (GstElement * element, GstStateChange transition)
{
  GstRtpSink *self = GST_RTP_SINK (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG_OBJECT (self, "changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      /* Set the properties to the child elements to avoid binding to
       * a NULL interface on a network without a default gateway */
      if (gst_rtp_sink_start (self) == FALSE)
        return GST_STATE_CHANGE_FAILURE;
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      /* re-use the sockets after they have been initialised */
      if (gst_rtp_sink_reuse_socket (self) == FALSE)
        return GST_STATE_CHANGE_FAILURE;
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    default:
      break;
  }

  return ret;
}


static void
gst_rtp_sink_init (GstRtpSink * self)
{
  const gchar *missing_plugin = NULL;
  GstCaps *caps;

  self->rtpbin = NULL;
  self->funnel_rtp = NULL;
  self->funnel_rtcp = NULL;
  self->rtp_sink = NULL;
  self->rtcp_src = NULL;
  self->rtcp_sink = NULL;

  self->uri = gst_uri_from_string (DEFAULT_PROP_URI);
  self->ttl = DEFAULT_PROP_TTL;
  self->ttl_mc = DEFAULT_PROP_TTL_MC;
  self->multi_iface = g_strdup (DEFAULT_PROP_MULTICAST_IFACE);

  g_mutex_init (&self->lock);

  /* Construct the RTP sender pipeline.
   *
   *           *-> [send_rtp_sink_%u]   --------  [send_rtp_src_%u]  -> udpsink
   *                                   | rtpbin |
   * udpsrc     -> [recv_rtcp_sink_%u]  --------  [send_rtcp_src_%u] -> * udpsink
   */
  self->rtpbin = gst_element_factory_make ("rtpbin", "rtp_send_rtpbin0");
  if (self->rtpbin == NULL) {
    missing_plugin = "rtpmanager";
    goto missing_plugin;
  }

  gst_bin_add (GST_BIN (self), self->rtpbin);

  /* Add rtpbin callbacks to monitor the operation of rtpbin */
  g_signal_connect_object (self->rtpbin, "element-added",
      G_CALLBACK (gst_rtp_sink_rtpbin_element_added_cb), self, 0);
  g_signal_connect_object (self->rtpbin, "pad-added",
      G_CALLBACK (gst_rtp_sink_rtpbin_pad_added_cb), self, 0);
  g_signal_connect_object (self->rtpbin, "pad-removed",
      G_CALLBACK (gst_rtp_sink_rtpbin_pad_removed_cb), self, 0);

  GST_OBJECT_FLAG_SET (GST_OBJECT (self), GST_ELEMENT_FLAG_SINK);
  gst_bin_set_suppressed_flags (GST_BIN (self),
      GST_ELEMENT_FLAG_SOURCE | GST_ELEMENT_FLAG_SINK);

  self->funnel_rtp = gst_element_factory_make ("funnel", "rtp_rtp_funnel0");
  if (self->funnel_rtp == NULL) {
    missing_plugin = "funnel";
    goto missing_plugin;
  }

  self->funnel_rtcp = gst_element_factory_make ("funnel", "rtp_rtcp_funnel0");
  if (self->funnel_rtcp == NULL) {
    missing_plugin = "funnel";
    goto missing_plugin;
  }

  self->rtp_sink = gst_element_factory_make ("udpsink", "rtp_rtp_udpsink0");
  if (self->rtp_sink == NULL) {
    missing_plugin = "udp";
    goto missing_plugin;
  }

  self->rtcp_src = gst_element_factory_make ("udpsrc", "rtp_rtcp_udpsrc0");
  if (self->rtcp_src == NULL) {
    missing_plugin = "udp";
    goto missing_plugin;
  }

  self->rtcp_sink = gst_element_factory_make ("udpsink", "rtp_rtcp_udpsink0");
  if (self->rtcp_sink == NULL) {
    missing_plugin = "udp";
    goto missing_plugin;
  }

  gst_bin_add (GST_BIN (self), self->funnel_rtp);
  gst_bin_add (GST_BIN (self), self->funnel_rtcp);

  gst_bin_add (GST_BIN (self), self->rtp_sink);
  gst_bin_add (GST_BIN (self), self->rtcp_src);
  gst_bin_add (GST_BIN (self), self->rtcp_sink);

  gst_element_set_locked_state (self->rtcp_src, TRUE);
  gst_element_set_locked_state (self->rtcp_sink, TRUE);

  /* no need to set address if unicast */
  caps = gst_caps_new_empty_simple ("application/x-rtcp");
  g_object_set (self->rtcp_src, "caps", caps, NULL);
  gst_caps_unref (caps);

  gst_element_link (self->funnel_rtp, self->rtp_sink);
  gst_element_link (self->funnel_rtcp, self->rtcp_sink);

  if (missing_plugin == NULL)
    return;

missing_plugin:
  {
    GST_ERROR_OBJECT (self, "'%s' plugin is missing.", missing_plugin);
    /* Just make our element valid, so we fail cleanly */
    gst_element_add_pad (GST_ELEMENT (self),
        gst_pad_new_from_static_template (&sink_template, "sink_%u"));
  }
}

static GstURIType
gst_rtp_sink_uri_get_type (GType type)
{
  return GST_URI_SINK;
}

static const gchar *const *
gst_rtp_sink_uri_get_protocols (GType type)
{
  static const gchar *protocols[] = { (char *) "rtp", NULL };

  return protocols;
}

static gchar *
gst_rtp_sink_uri_get_uri (GstURIHandler * handler)
{
  GstRtpSink *self = (GstRtpSink *) handler;

  return gst_uri_to_string (self->uri);
}

static gboolean
gst_rtp_sink_uri_set_uri (GstURIHandler * handler, const gchar * uri,
    GError ** error)
{
  GstRtpSink *self = (GstRtpSink *) handler;

  g_object_set (G_OBJECT (self), "uri", uri, NULL);

  return TRUE;
}

static void
gst_rtp_sink_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_rtp_sink_uri_get_type;
  iface->get_protocols = gst_rtp_sink_uri_get_protocols;
  iface->get_uri = gst_rtp_sink_uri_get_uri;
  iface->set_uri = gst_rtp_sink_uri_set_uri;
}

/* ex: set tabstop=2 shiftwidth=2 expandtab: */
