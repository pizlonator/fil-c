/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2004> Thomas Vander Stichele <thomas at apestaart dot org>
 * Copyright (C) <2011> Collabora Ltd.
 *     Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) <2014> William Manley <will@williammanley.net>
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
 * SECTION:element-socketsrc
 * @title: socketsrc
 *
 * Receive data from a socket.
 *
 * As compared to other elements:
 *
 * socketsrc can be considered a source counterpart to the #multisocketsink
 * sink.
 *
 * socketsrc can also be considered a generalization of #tcpclientsrc and
 * #tcpserversrc: it contains all the logic required to communicate over the
 * socket but none of the logic for creating the sockets/establishing the
 * connection in the first place, allowing the user to accomplish this
 * externally in whatever manner they wish making it applicable to other types
 * of sockets besides TCP.
 *
 * As compared to #fdsrc socketsrc is socket specific and deals with #GSocket
 * objects rather than sockets via integer file-descriptors.
 *
 * @see_also: #multisocketsink
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>
#include <gst/net/gstnetcontrolmessagemeta.h>
#include "gsttcpelements.h"
#include "gstsocketsrc.h"

GST_DEBUG_CATEGORY_STATIC (socketsrc_debug);
#define GST_CAT_DEFAULT socketsrc_debug

#define MAX_READ_SIZE                   4 * 1024


static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);


#define DEFAULT_SEND_MESSAGES FALSE

enum
{
  PROP_0,
  PROP_SOCKET,
  PROP_CAPS,
  PROP_SEND_MESSAGES
};

enum
{
  CONNECTION_CLOSED_BY_PEER,
  LAST_SIGNAL
};

static guint gst_socket_src_signals[LAST_SIGNAL] = { 0 };

#define gst_socket_src_parent_class parent_class
G_DEFINE_TYPE (GstSocketSrc, gst_socket_src, GST_TYPE_PUSH_SRC);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (socketsrc, "socketsrc",
    GST_RANK_NONE, GST_TYPE_SOCKET_SRC, tcp_element_init (plugin));

static void gst_socket_src_finalize (GObject * gobject);

static GstCaps *gst_socketsrc_getcaps (GstBaseSrc * src, GstCaps * filter);
static gboolean gst_socketsrc_event (GstBaseSrc * src, GstEvent * event);
static GstFlowReturn gst_socket_src_fill (GstPushSrc * psrc,
    GstBuffer * outbuf);
static gboolean gst_socket_src_unlock (GstBaseSrc * bsrc);
static gboolean gst_socket_src_unlock_stop (GstBaseSrc * bsrc);

static void gst_socket_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_socket_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

#define SWAP(a, b) do { GSocket* _swap_tmp = a; a = b; b = _swap_tmp; } while (0);

static void
gst_socket_src_class_init (GstSocketSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstPushSrcClass *gstpush_src_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) klass;
  gstpush_src_class = (GstPushSrcClass *) klass;

  gobject_class->set_property = gst_socket_src_set_property;
  gobject_class->get_property = gst_socket_src_get_property;
  gobject_class->finalize = gst_socket_src_finalize;

  g_object_class_install_property (gobject_class, PROP_SOCKET,
      g_param_spec_object ("socket", "Socket",
          "The socket to receive packets from", G_TYPE_SOCKET,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CAPS,
      g_param_spec_boxed ("caps", "Caps",
          "The caps of the source pad", GST_TYPE_CAPS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstSocketSrc:send-messages:
   *
   * Control if the source will handle GstNetworkMessage events.
   * The event is a CUSTOM event named 'GstNetworkMessage' and contains:
   *
   *   "buffer", GST_TYPE_BUFFER    : the buffer with data to send
   *
   * The buffer in the event will be sent on the socket. This allows
   * for simple bidirectional communication.
   *
   * Since: 1.8.0
   **/
  g_object_class_install_property (gobject_class, PROP_SEND_MESSAGES,
      g_param_spec_boolean ("send-messages", "Send Messages",
          "If GstNetworkMessage events should be handled",
          DEFAULT_SEND_MESSAGES, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_socket_src_signals[CONNECTION_CLOSED_BY_PEER] =
      g_signal_new ("connection-closed-by-peer", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (GstSocketSrcClass,
          connection_closed_by_peer), NULL, NULL, NULL, G_TYPE_NONE, 0);

  gst_element_class_add_static_pad_template (gstelement_class, &srctemplate);

  gst_element_class_set_static_metadata (gstelement_class,
      "socket source", "Source/Network",
      "Receive data from a socket",
      "Thomas Vander Stichele <thomas at apestaart dot org>, "
      "William Manley <will@williammanley.net>");

  gstbasesrc_class->event = gst_socketsrc_event;
  gstbasesrc_class->get_caps = gst_socketsrc_getcaps;
  gstbasesrc_class->unlock = gst_socket_src_unlock;
  gstbasesrc_class->unlock_stop = gst_socket_src_unlock_stop;

  gstpush_src_class->fill = gst_socket_src_fill;

  GST_DEBUG_CATEGORY_INIT (socketsrc_debug, "socketsrc", 0, "Socket Source");
}

static void
gst_socket_src_init (GstSocketSrc * this)
{
  this->socket = NULL;
  this->cancellable = g_cancellable_new ();
  this->send_messages = DEFAULT_SEND_MESSAGES;
}

static void
gst_socket_src_finalize (GObject * gobject)
{
  GstSocketSrc *this = GST_SOCKET_SRC (gobject);

  if (this->caps)
    gst_caps_unref (this->caps);
  g_clear_object (&this->cancellable);
  g_clear_object (&this->socket);

  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static gboolean
gst_socketsrc_event (GstBaseSrc * bsrc, GstEvent * event)
{
  GstSocketSrc *src;
  gboolean res = FALSE;

  src = GST_SOCKET_SRC (bsrc);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_UPSTREAM:
      if (src->send_messages && gst_event_has_name (event, "GstNetworkMessage")) {
        const GstStructure *str = gst_event_get_structure (event);
        GSocket *socket;

        GST_OBJECT_LOCK (src);
        if ((socket = src->socket))
          g_object_ref (socket);
        GST_OBJECT_UNLOCK (src);

        if (socket) {
          GstBuffer *buf;
          GstMapInfo map;
          GError *err = NULL;
          gssize ret;

          gst_structure_get (str, "buffer", GST_TYPE_BUFFER, &buf, NULL);

          if (buf) {
            gst_buffer_map (buf, &map, GST_MAP_READ);
            GST_LOG ("sending buffer of size %" G_GSIZE_FORMAT, map.size);
            ret = g_socket_send_with_blocking (socket, (gchar *) map.data,
                map.size, FALSE, src->cancellable, &err);
            gst_buffer_unmap (buf, &map);

            if (ret == -1) {
              GST_WARNING ("could not send message: %s", err->message);
              g_clear_error (&err);
              res = FALSE;
            } else
              res = TRUE;
            gst_buffer_unref (buf);
          }
          g_object_unref (socket);
        }
      }
      break;
    default:
      res = GST_BASE_SRC_CLASS (parent_class)->event (bsrc, event);
      break;
  }
  return res;
}

static GstCaps *
gst_socketsrc_getcaps (GstBaseSrc * src, GstCaps * filter)
{
  GstSocketSrc *socketsrc;
  GstCaps *caps, *result;

  socketsrc = GST_SOCKET_SRC (src);

  GST_OBJECT_LOCK (src);
  if ((caps = socketsrc->caps))
    gst_caps_ref (caps);
  GST_OBJECT_UNLOCK (src);

  if (caps) {
    if (filter) {
      result = gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
      gst_caps_unref (caps);
    } else {
      result = caps;
    }
  } else {
    result = (filter) ? gst_caps_ref (filter) : gst_caps_new_any ();
  }
  return result;
}

static GstFlowReturn
gst_socket_src_fill (GstPushSrc * psrc, GstBuffer * outbuf)
{
  GstSocketSrc *src;
  GstFlowReturn ret = GST_FLOW_OK;
  gssize rret;
  GError *err = NULL;
  GstMapInfo map;
  GSocket *socket = NULL;
  GSocketControlMessage **messages = NULL;
  gint num_messages = 0;
  gint i;
  GInputVector ivec;
  gint flags = 0;

  src = GST_SOCKET_SRC (psrc);

  GST_OBJECT_LOCK (src);

  if (src->socket)
    socket = g_object_ref (src->socket);

  GST_OBJECT_UNLOCK (src);

  if (socket == NULL)
    goto no_socket;

  GST_LOG_OBJECT (src, "asked for a buffer");

retry:
  gst_buffer_map (outbuf, &map, GST_MAP_READWRITE);
  ivec.buffer = map.data;
  ivec.size = map.size;
  rret =
      g_socket_receive_message (socket, NULL, &ivec, 1, &messages,
      &num_messages, &flags, src->cancellable, &err);
  gst_buffer_unmap (outbuf, &map);

  for (i = 0; i < num_messages; i++) {
    gst_buffer_add_net_control_message_meta (outbuf, messages[i]);
    g_object_unref (messages[i]);
    messages[i] = NULL;
  }
  g_free (messages);

  if (rret == 0) {
    GSocket *tmp = NULL;
    GST_DEBUG_OBJECT (src, "Received EOS on socket %p fd %i", socket,
        g_socket_get_fd (socket));

    /* We've hit EOS but we'll send this signal to allow someone to change
     * our socket before we send EOS downstream. */
    g_signal_emit (src, gst_socket_src_signals[CONNECTION_CLOSED_BY_PEER], 0);

    GST_OBJECT_LOCK (src);

    if (src->socket)
      tmp = g_object_ref (src->socket);

    GST_OBJECT_UNLOCK (src);

    /* Do this dance with tmp to avoid unreffing with the lock held */
    if (tmp != NULL && tmp != socket) {
      SWAP (socket, tmp);
      g_clear_object (&tmp);

      GST_INFO_OBJECT (src, "New socket available after EOS %p fd %i: Retrying",
          socket, g_socket_get_fd (socket));

      /* retry with our new socket: */
      goto retry;
    } else {
      g_clear_object (&tmp);
      GST_INFO_OBJECT (src, "Forwarding EOS downstream");
      ret = GST_FLOW_EOS;
    }
  } else if (rret < 0) {
    if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      ret = GST_FLOW_FLUSHING;
      GST_DEBUG_OBJECT (src, "Cancelled reading from socket");
    } else {
      ret = GST_FLOW_ERROR;
      GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
          ("Failed to read from socket: %s", err->message));
    }
  } else {
    ret = GST_FLOW_OK;
    gst_buffer_resize (outbuf, 0, rret);

    GST_LOG_OBJECT (src,
        "Returning buffer from _get of size %" G_GSIZE_FORMAT ", ts %"
        GST_TIME_FORMAT ", dur %" GST_TIME_FORMAT
        ", offset %" G_GINT64_FORMAT ", offset_end %" G_GINT64_FORMAT,
        gst_buffer_get_size (outbuf),
        GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (outbuf)),
        GST_TIME_ARGS (GST_BUFFER_DURATION (outbuf)),
        GST_BUFFER_OFFSET (outbuf), GST_BUFFER_OFFSET_END (outbuf));
  }
  g_clear_error (&err);
  g_clear_object (&socket);

  return ret;

no_socket:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND, (NULL),
        ("Cannot receive: No socket set on socketsrc"));
    return GST_FLOW_ERROR;
  }
}

static void
gst_socket_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSocketSrc *socketsrc = GST_SOCKET_SRC (object);

  switch (prop_id) {
    case PROP_SOCKET:{
      GSocket *socket = G_SOCKET (g_value_dup_object (value));
      GST_OBJECT_LOCK (socketsrc);
      SWAP (socket, socketsrc->socket);
      GST_OBJECT_UNLOCK (socketsrc);
      g_clear_object (&socket);
      break;
    }
    case PROP_CAPS:
    {
      const GstCaps *new_caps_val = gst_value_get_caps (value);
      GstCaps *new_caps;
      GstCaps *old_caps;

      if (new_caps_val == NULL) {
        new_caps = gst_caps_new_any ();
      } else {
        new_caps = gst_caps_copy (new_caps_val);
      }

      GST_OBJECT_LOCK (socketsrc);
      old_caps = socketsrc->caps;
      socketsrc->caps = new_caps;
      GST_OBJECT_UNLOCK (socketsrc);

      if (old_caps)
        gst_caps_unref (old_caps);

      gst_pad_mark_reconfigure (GST_BASE_SRC_PAD (socketsrc));
      break;
    }
    case PROP_SEND_MESSAGES:
      socketsrc->send_messages = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_socket_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstSocketSrc *socketsrc = GST_SOCKET_SRC (object);

  switch (prop_id) {
    case PROP_SOCKET:
      g_value_set_object (value, socketsrc->socket);
      break;
    case PROP_CAPS:
      GST_OBJECT_LOCK (socketsrc);
      gst_value_set_caps (value, socketsrc->caps);
      GST_OBJECT_UNLOCK (socketsrc);
      break;
    case PROP_SEND_MESSAGES:
      g_value_set_boolean (value, socketsrc->send_messages);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_socket_src_unlock (GstBaseSrc * bsrc)
{
  GstSocketSrc *src = GST_SOCKET_SRC (bsrc);

  GST_DEBUG_OBJECT (src, "set to flushing");
  g_cancellable_cancel (src->cancellable);

  return TRUE;
}

static gboolean
gst_socket_src_unlock_stop (GstBaseSrc * bsrc)
{
  GstSocketSrc *src = GST_SOCKET_SRC (bsrc);

  GST_DEBUG_OBJECT (src, "unset flushing");
  g_cancellable_reset (src->cancellable);

  return TRUE;
}
