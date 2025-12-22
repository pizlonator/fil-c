/* GStreamer
 * Copyright (C) 2018, Collabora Ltd.
 * Copyright (C) 2018, SK Telecom, Co., Ltd.
 *   Author: Jeongseok Kim <jeongseok.kim@sk.com>
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
 * SECTION:element-srtsrc
 * @title: srtsrc
 *
 * srtsrc is a network source that reads [SRT](http://www.srtalliance.org/)
 * packets from the network.
 *
 * ## Examples
 * |[
 * gst-launch-1.0 -v srtsrc uri="srt://127.0.0.1:7001" ! fakesink
 * ]| This pipeline shows how to connect SRT server by setting #GstSRTSrc:uri property.
 *
 * |[
 * gst-launch-1.0 -v srtsrc uri="srt://:7001?mode=listener" ! fakesink
 * ]| This pipeline shows how to wait SRT connection by setting #GstSRTSrc:uri property.
 *
 * |[
 * gst-launch-1.0 -v srtclientsrc uri="srt://192.168.1.10:7001?mode=rendez-vous" ! fakesink
 * ]| This pipeline shows how to connect SRT server by setting #GstSRTSrc:uri property and using the rendez-vous mode.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstsrtelements.h"
#include "gstsrtsrc.h"

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

#define GST_CAT_DEFAULT gst_debug_srt_src
GST_DEBUG_CATEGORY (GST_CAT_DEFAULT);

enum
{
  SIG_CALLER_ADDED,
  SIG_CALLER_REMOVED,
  SIG_CALLER_REJECTED,
  SIG_CALLER_CONNECTING,
  LAST_SIGNAL
};

enum
{
  PROP_KEEP_LISTENING = 128
};

static guint signals[LAST_SIGNAL] = { 0 };

static void gst_srt_src_uri_handler_init (gpointer g_iface,
    gpointer iface_data);
static gchar *gst_srt_src_uri_get_uri (GstURIHandler * handler);
static gboolean gst_srt_src_uri_set_uri (GstURIHandler * handler,
    const gchar * uri, GError ** error);
static gboolean src_default_caller_connecting (GstSRTSrc * self,
    GSocketAddress * addr, const gchar * username, gpointer data);
static gboolean src_authentication_accumulator (GSignalInvocationHint * ihint,
    GValue * return_accu, const GValue * handler_return, gpointer data);

#define gst_srt_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstSRTSrc, gst_srt_src,
    GST_TYPE_PUSH_SRC,
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER, gst_srt_src_uri_handler_init)
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "srtsrc", 0, "SRT Source"));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (srtsrc, "srtsrc", GST_RANK_PRIMARY,
    GST_TYPE_SRT_SRC, srt_element_init (plugin));

static gboolean
src_default_caller_connecting (GstSRTSrc * self,
    GSocketAddress * addr, const gchar * stream_id, gpointer data)
{
  /* Accept all connections. */
  return TRUE;
}

static gboolean
src_authentication_accumulator (GSignalInvocationHint * ihint,
    GValue * return_accu, const GValue * handler_return, gpointer data)
{
  gboolean ret = g_value_get_boolean (handler_return);
  /* Handlers return TRUE on authentication success and we want to stop on
   * the first failure. */
  g_value_set_boolean (return_accu, ret);
  return ret;
}

static gboolean
gst_srt_src_start (GstBaseSrc * bsrc)
{
  GstSRTSrc *self = GST_SRT_SRC (bsrc);
  GError *error = NULL;
  gboolean ret = FALSE;

  ret = gst_srt_object_open (self->srtobject, &error);

  if (!ret) {
    /* ensure error is posted since state change will fail */
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ, (NULL),
        ("Failed to open SRT: %s", error->message));
    g_clear_error (&error);
  }

  /* Reset expected pktseq */
  self->next_pktseq = 0;

  return ret;
}

static gboolean
gst_srt_src_stop (GstBaseSrc * bsrc)
{
  GstSRTSrc *self = GST_SRT_SRC (bsrc);

  gst_srt_object_close (self->srtobject);

  return TRUE;
}

static GstFlowReturn
gst_srt_src_fill (GstPushSrc * src, GstBuffer * outbuf)
{
  GstSRTSrc *self = GST_SRT_SRC (src);
  GstFlowReturn ret = GST_FLOW_OK;
  GstMapInfo info;
  GError *err = NULL;
  gssize recv_len;
  GstClock *clock;
  GstClockTime base_time;
  GstClockTime capture_time;
  GstClockTimeDiff delay;
  int64_t srt_time;
  SRT_MSGCTRL mctrl;

retry:
  if (g_cancellable_is_cancelled (self->srtobject->cancellable)) {
    ret = GST_FLOW_FLUSHING;
  }

  if (!gst_buffer_map (outbuf, &info, GST_MAP_WRITE)) {
    GST_ELEMENT_ERROR (src, RESOURCE, READ,
        ("Could not map the buffer for writing "), (NULL));
    ret = GST_FLOW_ERROR;
    goto out;
  }

  /* Get clock and values */
  clock = gst_element_get_clock (GST_ELEMENT (src));
  if (!clock) {
    GST_DEBUG_OBJECT (src, "Clock missing, flushing");
    return GST_FLOW_FLUSHING;
  }

  base_time = gst_element_get_base_time (GST_ELEMENT (src));

  recv_len = gst_srt_object_read (self->srtobject, info.data,
      gst_buffer_get_size (outbuf), &err, &mctrl);

  /* Capture clock values ASAP */
  capture_time = gst_clock_get_time (clock);
#if SRT_VERSION_VALUE >= 0x10402
  /* Use SRT clock value if available (SRT > 1.4.2) */
  srt_time = srt_time_now ();
#else
  /* Else use the unix epoch monotonic clock */
  srt_time = g_get_real_time ();
#endif
  gst_object_unref (clock);

  gst_buffer_unmap (outbuf, &info);

  GST_LOG_OBJECT (src,
      "recv_len:%" G_GSIZE_FORMAT " pktseq:%d msgno:%d srctime:%"
      G_GINT64_FORMAT, recv_len, mctrl.pktseq, mctrl.msgno, mctrl.srctime);

  if (g_cancellable_is_cancelled (self->srtobject->cancellable)) {
    ret = GST_FLOW_FLUSHING;
    goto out;
  }

  if (recv_len < 0) {
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL), ("%s", err->message));
    ret = GST_FLOW_ERROR;
    g_clear_error (&err);
    goto out;
  } else if (recv_len == 0) {
    gst_srt_src_stop (GST_BASE_SRC (self));
    if (self->keep_listening && gst_srt_src_start (GST_BASE_SRC (self))) {
      /* FIXME: Should send GAP event(s) downstream */
      gst_element_post_message (GST_ELEMENT_CAST (self),
          gst_message_new_element (GST_OBJECT_CAST (self),
              gst_structure_new_empty ("connection-removed")));
      goto retry;
    } else {
      ret = GST_FLOW_EOS;
      goto out;
    }
  }

  /* Detect discontinuities */
  if (mctrl.pktseq != self->next_pktseq) {
    GST_WARNING_OBJECT (src, "discont detected %d (expected: %d)",
        mctrl.pktseq, self->next_pktseq);
    GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_DISCONT);
  }
  /* pktseq is a 31bit field */
  self->next_pktseq = (mctrl.pktseq + 1) % G_MAXINT32;

  /* 0 means we do not have a srctime */
  if (mctrl.srctime != 0)
    delay = (srt_time - mctrl.srctime) * GST_USECOND;
  else
    delay = 0;

  GST_LOG_OBJECT (src, "delay: %" GST_STIME_FORMAT, GST_STIME_ARGS (delay));

  if (delay < 0) {
    GST_WARNING_OBJECT (src,
        "Calculated SRT delay %" GST_STIME_FORMAT " is negative, clamping to 0",
        GST_STIME_ARGS (delay));
    delay = 0;
  }

  /* Subtract the base_time (since the pipeline started) ... */
  if (capture_time > base_time)
    capture_time -= base_time;
  else
    capture_time = 0;
  /* And adjust by the delay */
  if (capture_time > delay)
    capture_time -= delay;
  else
    capture_time = 0;
  GST_BUFFER_TIMESTAMP (outbuf) = capture_time;

  gst_buffer_resize (outbuf, 0, recv_len);

  GST_LOG_OBJECT (src,
      "filled buffer from _get of size %" G_GSIZE_FORMAT ", ts %"
      GST_TIME_FORMAT ", dur %" GST_TIME_FORMAT
      ", offset %" G_GINT64_FORMAT ", offset_end %" G_GINT64_FORMAT,
      gst_buffer_get_size (outbuf),
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (outbuf)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (outbuf)),
      GST_BUFFER_OFFSET (outbuf), GST_BUFFER_OFFSET_END (outbuf));

out:
  return ret;
}

static void
gst_srt_src_init (GstSRTSrc * self)
{
  self->srtobject = gst_srt_object_new (GST_ELEMENT (self));

  gst_base_src_set_format (GST_BASE_SRC (self), GST_FORMAT_TIME);
  gst_base_src_set_live (GST_BASE_SRC (self), TRUE);
  /* We do the timing ourselves */
  gst_base_src_set_do_timestamp (GST_BASE_SRC (self), FALSE);

  gst_srt_object_set_uri (self->srtobject, GST_SRT_DEFAULT_URI, NULL);

}

static void
gst_srt_src_finalize (GObject * object)
{
  GstSRTSrc *self = GST_SRT_SRC (object);

  gst_srt_object_destroy (self->srtobject);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_srt_src_unlock (GstBaseSrc * bsrc)
{
  GstSRTSrc *self = GST_SRT_SRC (bsrc);

  gst_srt_object_unlock (self->srtobject);

  return TRUE;
}

static gboolean
gst_srt_src_unlock_stop (GstBaseSrc * bsrc)
{
  GstSRTSrc *self = GST_SRT_SRC (bsrc);

  gst_srt_object_unlock_stop (self->srtobject);

  return TRUE;
}

static void
gst_srt_src_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstSRTSrc *self = GST_SRT_SRC (object);

  if (!gst_srt_object_set_property_helper (self->srtobject, prop_id, value,
          pspec)) {
    switch (prop_id) {
      case PROP_KEEP_LISTENING:
        self->keep_listening = g_value_get_boolean (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
  }
}

static void
gst_srt_src_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstSRTSrc *self = GST_SRT_SRC (object);

  if (!gst_srt_object_get_property_helper (self->srtobject, prop_id, value,
          pspec)) {
    switch (prop_id) {
      case PROP_KEEP_LISTENING:
        g_value_set_boolean (value, self->keep_listening);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
  }
}

static gboolean
gst_srt_src_query (GstBaseSrc * basesrc, GstQuery * query)
{
  GstSRTSrc *self = GST_SRT_SRC (basesrc);

  if (GST_QUERY_TYPE (query) == GST_QUERY_LATENCY) {
    gint latency;
    if (!gst_structure_get_int (self->srtobject->parameters, "latency",
            &latency))
      latency = GST_SRT_DEFAULT_LATENCY;
    gst_query_set_latency (query, TRUE, latency * GST_MSECOND,
        latency * GST_MSECOND);
    return TRUE;
  } else {
    return GST_BASE_SRC_CLASS (parent_class)->query (basesrc, query);
  }
}

static void
gst_srt_src_class_init (GstSRTSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *gstpushsrc_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->set_property = gst_srt_src_set_property;
  gobject_class->get_property = gst_srt_src_get_property;
  gobject_class->finalize = gst_srt_src_finalize;
  klass->caller_connecting = src_default_caller_connecting;

  /**
   * GstSRTSrc::caller-added:
   * @gstsrtsrc: the srtsrc element that emitted this signal
   * @unused: always zero (for ABI compatibility with previous versions)
   * @addr: the #GSocketAddress of the new caller
   * 
   * A new caller has connected to srtsrc.
   */
  signals[SIG_CALLER_ADDED] =
      g_signal_new ("caller-added", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstSRTSrcClass, caller_added),
      NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_SOCKET_ADDRESS);

  /**
   * GstSRTSrc::caller-removed:
   * @gstsrtsrc: the srtsrc element that emitted this signal
   * @unused: always zero (for ABI compatibility with previous versions)
   * @addr: the #GSocketAddress of the caller
   *
   * The given caller has disconnected.
   */
  signals[SIG_CALLER_REMOVED] =
      g_signal_new ("caller-removed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstSRTSrcClass,
          caller_added), NULL, NULL, NULL, G_TYPE_NONE,
      2, G_TYPE_INT, G_TYPE_SOCKET_ADDRESS);

  /**
   * GstSRTSrc::caller-rejected:
   * @gstsrtsrc: the srtsrc element that emitted this signal
   * @addr: the #GSocketAddress that describes the client socket
   * @stream_id: the stream Id to which the caller wants to connect
   *
   * A caller's connection to srtsrc in listener mode has been rejected.
   *
   * Since: 1.20
   *
   */
  signals[SIG_CALLER_REJECTED] =
      g_signal_new ("caller-rejected", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstSRTSrcClass, caller_rejected),
      NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_SOCKET_ADDRESS, G_TYPE_STRING);

  /**
   * GstSRTSrc::caller-connecting:
   * @gstsrtsrc: the srtsrc element that emitted this signal
   * @addr: the #GSocketAddress that describes the client socket
   * @stream_id: the stream Id to which the caller wants to connect
   *
   * Whether to accept or reject a caller's connection to srtsrc in listener mode.
   * The Caller's connection is rejected if the callback returns FALSE, else
   * the connection is accepeted.
   *
   * Since: 1.20
   *
   */
  signals[SIG_CALLER_CONNECTING] =
      g_signal_new ("caller-connecting", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstSRTSrcClass, caller_connecting),
      src_authentication_accumulator, NULL, NULL, G_TYPE_BOOLEAN,
      2, G_TYPE_SOCKET_ADDRESS, G_TYPE_STRING);

  gst_srt_object_install_properties_helper (gobject_class);

  /**
   * GstSRTSrc:keep-listening:
   *
   * If FALSE, the element will return GST_FLOW_EOS when the remote client disconnects.
   * If TRUE, the element will keep waiting for the client to reconnect. An element
   * message named 'connection-removed' will be sent on disconnection.
   *
   * Since: 1.22
   *
   */
  g_object_class_install_property (gobject_class, PROP_KEEP_LISTENING,
      g_param_spec_boolean ("keep-listening",
          "Keep listening",
          "Toggle keep-listening for connection reuse",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (gstelement_class, &src_template);
  gst_element_class_set_metadata (gstelement_class,
      "SRT source", "Source/Network",
      "Receive data over the network via SRT",
      "Justin Kim <justin.joy.9to5@gmail.com>");

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_srt_src_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_srt_src_stop);
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_srt_src_unlock);
  gstbasesrc_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_srt_src_unlock_stop);
  gstbasesrc_class->query = GST_DEBUG_FUNCPTR (gst_srt_src_query);

  gstpushsrc_class->fill = GST_DEBUG_FUNCPTR (gst_srt_src_fill);

  gst_type_mark_as_plugin_api (GST_TYPE_SRT_SRC, 0);
}

static GstURIType
gst_srt_src_uri_get_type (GType type)
{
  return GST_URI_SRC;
}

static const gchar *const *
gst_srt_src_uri_get_protocols (GType type)
{
  static const gchar *protocols[] = { GST_SRT_DEFAULT_URI_SCHEME, NULL };

  return protocols;
}

static gchar *
gst_srt_src_uri_get_uri (GstURIHandler * handler)
{
  gchar *uri_str;
  GstSRTSrc *self = GST_SRT_SRC (handler);

  GST_OBJECT_LOCK (self);
  uri_str = gst_uri_to_string (self->srtobject->uri);
  GST_OBJECT_UNLOCK (self);

  return uri_str;
}

static gboolean
gst_srt_src_uri_set_uri (GstURIHandler * handler,
    const gchar * uri, GError ** error)
{
  GstSRTSrc *self = GST_SRT_SRC (handler);
  gboolean ret;

  GST_OBJECT_LOCK (self);
  ret = gst_srt_object_set_uri (self->srtobject, uri, error);
  GST_OBJECT_UNLOCK (self);

  return ret;
}

static void
gst_srt_src_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_srt_src_uri_get_type;
  iface->get_protocols = gst_srt_src_uri_get_protocols;
  iface->get_uri = gst_srt_src_uri_get_uri;
  iface->set_uri = gst_srt_src_uri_set_uri;
}
