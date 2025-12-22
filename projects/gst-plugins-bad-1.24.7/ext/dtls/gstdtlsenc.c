/*
 * Copyright (c) 2014, Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or other
 * materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstdtlselements.h"
#include "gstdtlsenc.h"

#include "gstdtlsdec.h"

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-dtls")
    );

GST_DEBUG_CATEGORY_STATIC (gst_dtls_enc_debug);
#define GST_CAT_DEFAULT gst_dtls_enc_debug

#define gst_dtls_enc_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstDtlsEnc, gst_dtls_enc, GST_TYPE_ELEMENT,
    GST_DEBUG_CATEGORY_INIT (gst_dtls_enc_debug, "dtlsenc", 0, "DTLS Encoder"));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (dtlsenc, "dtlsenc", GST_RANK_NONE,
    GST_TYPE_DTLS_ENC, dtls_element_init (plugin));

enum
{
  SIGNAL_ON_KEY_RECEIVED,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS];

enum
{
  PROP_0,
  PROP_CONNECTION_ID,
  PROP_IS_CLIENT,
  PROP_ENCODER_KEY,
  PROP_SRTP_CIPHER,
  PROP_SRTP_AUTH,
  PROP_CONNECTION_STATE,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

#define DEFAULT_CONNECTION_ID NULL
#define DEFAULT_IS_CLIENT FALSE

#define DEFAULT_ENCODER_KEY NULL
#define DEFAULT_SRTP_CIPHER 0
#define DEFAULT_SRTP_AUTH 0

#define INITIAL_QUEUE_SIZE 64

static void gst_dtls_enc_finalize (GObject *);
static void gst_dtls_enc_set_property (GObject *, guint prop_id,
    const GValue *, GParamSpec *);
static void gst_dtls_enc_get_property (GObject *, guint prop_id, GValue *,
    GParamSpec *);

static GstStateChangeReturn gst_dtls_enc_change_state (GstElement *,
    GstStateChange);
static GstPad *gst_dtls_enc_request_new_pad (GstElement *, GstPadTemplate *,
    const gchar * name, const GstCaps *);

static gboolean src_activate_mode (GstPad *, GstObject *, GstPadMode,
    gboolean active);
static void src_task_loop (GstPad *);

static GstFlowReturn sink_chain (GstPad *, GstObject *, GstBuffer *);
static gboolean sink_event (GstPad * pad, GstObject * parent, GstEvent * event);

static void on_key_received (GstDtlsConnection *, gpointer key, guint cipher,
    guint auth, GstDtlsEnc *);
static gboolean on_send_data (GstDtlsConnection *, gconstpointer data,
    gsize length, GstDtlsEnc *);

static void
gst_dtls_enc_class_init (GstDtlsEncClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;

  gobject_class = (GObjectClass *) klass;
  element_class = (GstElementClass *) klass;

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_dtls_enc_finalize);
  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_dtls_enc_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_dtls_enc_get_property);

  element_class->change_state = GST_DEBUG_FUNCPTR (gst_dtls_enc_change_state);
  element_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_dtls_enc_request_new_pad);

  signals[SIGNAL_ON_KEY_RECEIVED] =
      g_signal_new ("on-key-received", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  properties[PROP_CONNECTION_ID] =
      g_param_spec_string ("connection-id",
      "Connection id",
      "Every encoder/decoder pair should have the same, unique, connection-id",
      DEFAULT_CONNECTION_ID, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_IS_CLIENT] =
      g_param_spec_boolean ("is-client",
      "Is client",
      "Set to true if the decoder should act as "
      "client and initiate the handshake",
      DEFAULT_IS_CLIENT,
      GST_PARAM_MUTABLE_READY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_ENCODER_KEY] =
      g_param_spec_boxed ("encoder-key",
      "Encoder key",
      "Master key that should be used by the SRTP encoder",
      GST_TYPE_BUFFER, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_SRTP_CIPHER] =
      g_param_spec_uint ("srtp-cipher",
      "SRTP cipher",
      "The SRTP cipher selected in the DTLS handshake. "
      "The value will be set to an GstDtlsSrtpCipher.",
      0, GST_DTLS_SRTP_CIPHER_AES_128_ICM, DEFAULT_SRTP_CIPHER,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_SRTP_AUTH] =
      g_param_spec_uint ("srtp-auth",
      "SRTP authentication",
      "The SRTP authentication selected in the DTLS handshake. "
      "The value will be set to an GstDtlsSrtpAuth.",
      0, GST_DTLS_SRTP_AUTH_HMAC_SHA1_80, DEFAULT_SRTP_AUTH,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_CONNECTION_STATE] =
      g_param_spec_enum ("connection-state",
      "Connection State",
      "Current connection state",
      GST_DTLS_TYPE_CONNECTION_STATE,
      GST_DTLS_CONNECTION_STATE_NEW, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);

  gst_element_class_add_static_pad_template (element_class, &src_template);
  gst_element_class_add_static_pad_template (element_class, &sink_template);

  gst_element_class_set_static_metadata (element_class,
      "DTLS Encoder",
      "Encoder/Network/DTLS",
      "Encodes packets with DTLS",
      "Patrik Oldsberg patrik.oldsberg@ericsson.com");
}

static void
gst_dtls_enc_init (GstDtlsEnc * self)
{
  self->connection_id = NULL;
  self->connection = NULL;

  self->is_client = DEFAULT_IS_CLIENT;

  self->encoder_key = NULL;
  self->srtp_cipher = DEFAULT_SRTP_CIPHER;
  self->srtp_auth = DEFAULT_SRTP_AUTH;

  g_queue_init (&self->queue);
  g_mutex_init (&self->queue_lock);
  g_cond_init (&self->queue_cond_add);

  self->src = gst_pad_new_from_static_template (&src_template, "src");
  g_return_if_fail (self->src);

  gst_pad_set_activatemode_function (self->src,
      GST_DEBUG_FUNCPTR (src_activate_mode));

  gst_element_add_pad (GST_ELEMENT (self), self->src);
}

static void
gst_dtls_enc_finalize (GObject * object)
{
  GstDtlsEnc *self = GST_DTLS_ENC (object);

  if (self->encoder_key) {
    gst_buffer_unref (self->encoder_key);
    self->encoder_key = NULL;
  }

  if (self->connection_id) {
    g_free (self->connection_id);
    self->connection_id = NULL;
  }

  g_mutex_lock (&self->queue_lock);
  g_queue_foreach (&self->queue, (GFunc) gst_buffer_unref, NULL);
  g_queue_clear (&self->queue);
  g_mutex_unlock (&self->queue_lock);

  g_mutex_clear (&self->queue_lock);
  g_cond_clear (&self->queue_cond_add);

  GST_LOG_OBJECT (self, "finalized");

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_dtls_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDtlsEnc *self = GST_DTLS_ENC (object);

  switch (prop_id) {
    case PROP_CONNECTION_ID:
      if (self->connection_id != NULL) {
        g_free (self->connection_id);
        self->connection_id = NULL;
      }
      self->connection_id = g_value_dup_string (value);
      break;
    case PROP_IS_CLIENT:
      self->is_client = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
  }
}

static void
gst_dtls_enc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstDtlsEnc *self = GST_DTLS_ENC (object);

  switch (prop_id) {
    case PROP_CONNECTION_ID:
      g_value_set_string (value, self->connection_id);
      break;
    case PROP_IS_CLIENT:
      g_value_set_boolean (value, self->is_client);
      break;
    case PROP_ENCODER_KEY:
      g_value_set_boxed (value, self->encoder_key);
      break;
    case PROP_SRTP_CIPHER:
      g_value_set_uint (value, self->srtp_cipher);
      break;
    case PROP_SRTP_AUTH:
      g_value_set_uint (value, self->srtp_auth);
      break;
    case PROP_CONNECTION_STATE:
      if (self->connection)
        g_object_get_property (G_OBJECT (self->connection), "connection-state",
            value);
      else
        g_value_set_enum (value, GST_DTLS_CONNECTION_STATE_CLOSED);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
  }
}

static void
on_connection_state_changed (GObject * object, GParamSpec * pspec,
    gpointer user_data)
{
  GstDtlsEnc *self = GST_DTLS_ENC (user_data);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CONNECTION_STATE]);
}

static GstStateChangeReturn
gst_dtls_enc_change_state (GstElement * element, GstStateChange transition)
{
  GstDtlsEnc *self = GST_DTLS_ENC (element);
  GstStateChangeReturn ret;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (self->connection_id) {
        self->connection = gst_dtls_dec_fetch_connection (self->connection_id);

        if (!self->connection) {
          GST_WARNING_OBJECT (self,
              "invalid connection id: '%s', connection not found or already in use",
              self->connection_id);
          return GST_STATE_CHANGE_FAILURE;
        }

        g_signal_connect_object (self->connection,
            "on-encoder-key", G_CALLBACK (on_key_received), self, 0);
        g_signal_connect_object (self->connection,
            "notify::connection-state",
            G_CALLBACK (on_connection_state_changed), self, 0);
        on_connection_state_changed (NULL, NULL, self);

        gst_dtls_connection_set_send_callback (self->connection,
            (GstDtlsConnectionSendCallback) on_send_data, self, NULL);
      } else {
        GST_WARNING_OBJECT (self,
            "trying to change state to ready without connection id");
        return GST_STATE_CHANGE_FAILURE;
      }
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_DEBUG_OBJECT (self, "stopping connection %s", self->connection_id);

      gst_dtls_connection_stop (self->connection);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      GST_DEBUG_OBJECT (self, "closing connection %s", self->connection_id);

      if (self->connection) {
        gst_dtls_connection_close (self->connection);
        gst_dtls_connection_set_send_callback (self->connection, NULL, NULL,
            NULL);
        g_object_unref (self->connection);
        self->connection = NULL;
      }
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:{
      GError *err = NULL;

      GST_DEBUG_OBJECT (self, "starting connection %s", self->connection_id);
      if (!gst_dtls_connection_start (self->connection, self->is_client, &err)) {
        GST_ELEMENT_ERROR (self, RESOURCE, OPEN_WRITE, (NULL), ("%s",
                err->message));
        g_clear_error (&err);
      }
      break;
    }
    default:
      break;
  }

  return ret;
}

static GstPad *
gst_dtls_enc_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
  GstPad *sink;
  gboolean ret;

  GST_DEBUG_OBJECT (element, "sink pad requested");

  g_return_val_if_fail (templ->direction == GST_PAD_SINK, NULL);

  sink = gst_pad_new_from_template (templ, name);
  g_return_val_if_fail (sink, NULL);

  if (caps) {
    g_object_set (sink, "caps", caps, NULL);
  }

  gst_pad_set_chain_function (sink, GST_DEBUG_FUNCPTR (sink_chain));
  gst_pad_set_event_function (sink, GST_DEBUG_FUNCPTR (sink_event));

  ret = gst_pad_set_active (sink, TRUE);
  g_warn_if_fail (ret);

  gst_element_add_pad (element, sink);

  return sink;
}

static gboolean
src_activate_mode (GstPad * pad, GstObject * parent, GstPadMode mode,
    gboolean active)
{
  GstDtlsEnc *self = GST_DTLS_ENC (parent);
  gboolean success = TRUE;
  g_return_val_if_fail (mode == GST_PAD_MODE_PUSH, FALSE);

  if (active) {
    GST_DEBUG_OBJECT (self, "src pad activating in push mode");

    self->flushing = FALSE;
    self->src_ret = GST_FLOW_OK;
    self->send_initial_events = TRUE;
    success =
        gst_pad_start_task (pad, (GstTaskFunction) src_task_loop, self->src,
        NULL);
    if (!success) {
      GST_WARNING_OBJECT (self, "failed to activate pad task");
    }
  } else {
    GST_DEBUG_OBJECT (self, "deactivating src pad");

    g_mutex_lock (&self->queue_lock);
    g_queue_foreach (&self->queue, (GFunc) gst_buffer_unref, NULL);
    g_queue_clear (&self->queue);
    self->flushing = TRUE;
    self->src_ret = GST_FLOW_FLUSHING;
    g_cond_signal (&self->queue_cond_add);
    g_mutex_unlock (&self->queue_lock);
    success = gst_pad_stop_task (pad);
    if (!success) {
      GST_WARNING_OBJECT (self, "failed to deactivate pad task");
    }
  }

  return success;
}

static void
src_task_loop (GstPad * pad)
{
  GstDtlsEnc *self = GST_DTLS_ENC (GST_PAD_PARENT (pad));
  GstFlowReturn ret;
  GstBuffer *buffer;
  gboolean check_connection_timeout = FALSE;

  GST_TRACE_OBJECT (self, "src loop: acquiring lock");
  g_mutex_lock (&self->queue_lock);
  GST_TRACE_OBJECT (self, "src loop: acquired lock");

  if (self->flushing) {
    GST_LOG_OBJECT (self, "src task loop entered on inactive pad");
    GST_TRACE_OBJECT (self, "src loop: releasing lock");
    g_mutex_unlock (&self->queue_lock);
    return;
  }

  while (g_queue_is_empty (&self->queue)) {
    GST_TRACE_OBJECT (self, "src loop: queue empty, waiting for add");
    g_cond_wait (&self->queue_cond_add, &self->queue_lock);
    GST_TRACE_OBJECT (self, "src loop: add signaled");

    if (self->flushing) {
      GST_LOG_OBJECT (self, "pad inactive, task returning");
      GST_TRACE_OBJECT (self, "src loop: releasing lock");
      g_mutex_unlock (&self->queue_lock);
      return;
    }
  }
  GST_TRACE_OBJECT (self, "src loop: queue has element");

  buffer = g_queue_pop_head (&self->queue);
  g_mutex_unlock (&self->queue_lock);

  if (self->send_initial_events) {
    GstSegment segment;
    gchar *stream_id;
    GstCaps *caps;
    GstEvent *stream_start_event;

    self->send_initial_events = FALSE;

    stream_id =
        gst_pad_create_stream_id (self->src, GST_ELEMENT_CAST (self), NULL);
    stream_start_event = gst_event_new_stream_start (stream_id);
    gst_event_set_group_id (stream_start_event, gst_util_group_id_next ());
    gst_pad_push_event (self->src, stream_start_event);
    g_free (stream_id);
    caps = gst_caps_new_empty_simple ("application/x-dtls");
    gst_pad_push_event (self->src, gst_event_new_caps (caps));
    gst_caps_unref (caps);
    gst_segment_init (&segment, GST_FORMAT_BYTES);
    gst_pad_push_event (self->src, gst_event_new_segment (&segment));
    check_connection_timeout = TRUE;
  }

  GST_TRACE_OBJECT (self, "src loop: releasing lock");

  if (buffer) {
    ret = gst_pad_push (self->src, buffer);
    if (check_connection_timeout)
      gst_dtls_connection_check_timeout (self->connection);

    if (G_UNLIKELY (ret == GST_FLOW_NOT_LINKED || ret < GST_FLOW_EOS)) {
      GST_WARNING_OBJECT (self, "failed to push buffer on src pad: %s",
          gst_flow_get_name (ret));
    }
    g_mutex_lock (&self->queue_lock);
    self->src_ret = ret;
    g_mutex_unlock (&self->queue_lock);
  } else {
    GST_DEBUG_OBJECT (self, "Peer and us closed the connection, sending EOS");
    gst_pad_push_event (self->src, gst_event_new_eos ());
    g_mutex_lock (&self->queue_lock);
    self->src_ret = GST_FLOW_EOS;
    g_mutex_unlock (&self->queue_lock);
  }
}

static GstFlowReturn
sink_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstDtlsEnc *self = GST_DTLS_ENC (parent);
  GstMapInfo map_info;
  GError *err = NULL;
  gsize to_write, written = 0;
  GstFlowReturn ret = GST_FLOW_OK;

  g_mutex_lock (&self->queue_lock);
  if (self->src_ret != GST_FLOW_OK) {
    if (G_UNLIKELY (self->src_ret == GST_FLOW_NOT_LINKED
            || self->src_ret < GST_FLOW_EOS))
      GST_ERROR_OBJECT (self, "Pushing previous data returned an error: %s",
          gst_flow_get_name (self->src_ret));

    gst_buffer_unref (buffer);
    g_mutex_unlock (&self->queue_lock);
    return self->src_ret;
  }
  g_mutex_unlock (&self->queue_lock);

  gst_buffer_map (buffer, &map_info, GST_MAP_READ);

  to_write = map_info.size;

  while (to_write > 0 && ret == GST_FLOW_OK) {
    ret =
        gst_dtls_connection_send (self->connection, map_info.data,
        map_info.size, &written, &err);

    switch (ret) {
      case GST_FLOW_OK:
        GST_DEBUG_OBJECT (self,
            "Wrote %" G_GSIZE_FORMAT " B of %" G_GSIZE_FORMAT " B", written,
            map_info.size);
        g_assert (written <= to_write);
        to_write -= written;
        break;
      case GST_FLOW_EOS:
        GST_INFO_OBJECT (self, "Received data after the connection was closed");
        break;
      case GST_FLOW_ERROR:
        GST_WARNING_OBJECT (self, "error sending data: %s", err->message);
        GST_ELEMENT_ERROR (self, RESOURCE, WRITE, (NULL), ("%s", err->message));
        g_clear_error (&err);
        break;
      case GST_FLOW_FLUSHING:
        GST_INFO_OBJECT (self, "Flushing");
        break;
      default:
        g_assert_not_reached ();
        break;
    }

    g_assert (err == NULL);
  }

  gst_buffer_unmap (buffer, &map_info);
  gst_buffer_unref (buffer);

  return ret;
}


static gboolean
sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstDtlsEnc *self = GST_DTLS_ENC (parent);
  gboolean ret = FALSE;

  switch (GST_EVENT_TYPE (event)) {
      /* Drop segment, stream-start as we will push our own from the src pad
       * task.
       * FIXME: do we need any information from upstream for pushing our own? */
    case GST_EVENT_SEGMENT:
    case GST_EVENT_STREAM_START:
      gst_event_unref (event);
      ret = TRUE;
      break;
    case GST_EVENT_EOS:{
      GstFlowReturn flow_ret;

      /* Close the write side of the connection now */
      flow_ret =
          gst_dtls_connection_send (self->connection, NULL, 0, NULL, NULL);

      if (flow_ret != GST_FLOW_OK)
        GST_ERROR_OBJECT (self, "Failed to send close_notify");

      /* Do not forward the EOS event unless the peer already closed to the
       * connection itself. If it didn't yet then we'll later get the send
       * callback called with no data and send EOS from there */
      if (flow_ret == GST_FLOW_EOS) {
        ret = gst_pad_event_default (pad, parent, event);
      } else {
        gst_event_unref (event);
        ret = TRUE;
      }

      break;
    }
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }

  return ret;
}

static void
on_key_received (GstDtlsConnection * connection, gpointer key, guint cipher,
    guint auth, GstDtlsEnc * self)
{
  GstBuffer *new_encoder_key;
  gchar *key_str;

  g_return_if_fail (GST_IS_DTLS_ENC (self));
  g_return_if_fail (GST_IS_DTLS_CONNECTION (connection));

  self->srtp_cipher = cipher;
  self->srtp_auth = auth;

  new_encoder_key =
      gst_buffer_new_memdup (key, GST_DTLS_SRTP_MASTER_KEY_LENGTH);

  if (self->encoder_key)
    gst_buffer_unref (self->encoder_key);

  self->encoder_key = new_encoder_key;

  key_str = g_base64_encode (key, GST_DTLS_SRTP_MASTER_KEY_LENGTH);
  GST_INFO_OBJECT (self, "received key: %s", key_str);
  g_free (key_str);

  g_signal_emit (self, signals[SIGNAL_ON_KEY_RECEIVED], 0);
}

static gboolean
on_send_data (GstDtlsConnection * connection, gconstpointer data, gsize length,
    GstDtlsEnc * self)
{
  GstBuffer *buffer;
  gboolean ret;

  GST_DEBUG_OBJECT (self, "sending data from %s with length %" G_GSIZE_FORMAT,
      self->connection_id, length);

  buffer = data ? gst_buffer_new_memdup (data, length) : NULL;

  GST_TRACE_OBJECT (self, "send data: acquiring lock");
  g_mutex_lock (&self->queue_lock);
  GST_TRACE_OBJECT (self, "send data: acquired lock");

  g_queue_push_tail (&self->queue, buffer);

  GST_TRACE_OBJECT (self, "send data: signaling add");
  g_cond_signal (&self->queue_cond_add);

  GST_TRACE_OBJECT (self, "send data: releasing lock");

  ret = self->src_ret == GST_FLOW_OK;
  if (self->src_ret == GST_FLOW_FLUSHING)
    gst_dtls_connection_set_flow_return (connection, self->src_ret);
  g_mutex_unlock (&self->queue_lock);

  return ret;
}
