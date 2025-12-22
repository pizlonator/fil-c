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
#include "gstdtlsdec.h"

#include "gstdtlscertificate.h"

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-dtls")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC (gst_dtls_dec_debug);
#define GST_CAT_DEFAULT gst_dtls_dec_debug

#define gst_dtls_dec_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstDtlsDec, gst_dtls_dec, GST_TYPE_ELEMENT,
    GST_DEBUG_CATEGORY_INIT (gst_dtls_dec_debug, "dtlsdec", 0, "DTLS Decoder"));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (dtlsdec, "dtlsdec", GST_RANK_NONE,
    GST_TYPE_DTLS_DEC, dtls_element_init (plugin));

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
  PROP_PEM,
  PROP_PEER_PEM,
  PROP_DECODER_KEY,
  PROP_SRTP_CIPHER,
  PROP_SRTP_AUTH,
  PROP_CONNECTION_STATE,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

#define DEFAULT_CONNECTION_ID NULL
#define DEFAULT_PEM NULL
#define DEFAULT_PEER_PEM NULL

#define DEFAULT_DECODER_KEY NULL
#define DEFAULT_SRTP_CIPHER 0
#define DEFAULT_SRTP_AUTH 0


static void gst_dtls_dec_finalize (GObject *);
static void gst_dtls_dec_dispose (GObject *);
static void gst_dtls_dec_set_property (GObject *, guint prop_id,
    const GValue *, GParamSpec *);
static void gst_dtls_dec_get_property (GObject *, guint prop_id, GValue *,
    GParamSpec *);

static GstStateChangeReturn gst_dtls_dec_change_state (GstElement *,
    GstStateChange);
static GstPad *gst_dtls_dec_request_new_pad (GstElement *, GstPadTemplate *,
    const gchar * name, const GstCaps *);
static void gst_dtls_dec_release_pad (GstElement *, GstPad *);

static void on_key_received (GstDtlsConnection *, gpointer key, guint cipher,
    guint auth, GstDtlsDec *);
static gboolean on_peer_certificate_received (GstDtlsConnection *, gchar * pem,
    GstDtlsDec *);
static GstFlowReturn sink_chain (GstPad *, GstObject * parent, GstBuffer *);
static GstFlowReturn sink_chain_list (GstPad *, GstObject * parent,
    GstBufferList *);

static GstDtlsAgent *get_agent_by_pem (const gchar * pem);
static void agent_weak_ref_notify (gchar * pem, GstDtlsAgent *);
static void create_connection (GstDtlsDec *, gchar * id);
static void connection_weak_ref_notify (gchar * id, GstDtlsConnection *);

static void
gst_dtls_dec_class_init (GstDtlsDecClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;

  gobject_class = (GObjectClass *) klass;
  element_class = (GstElementClass *) klass;

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_dtls_dec_finalize);
  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_dtls_dec_dispose);
  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_dtls_dec_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_dtls_dec_get_property);

  element_class->change_state = GST_DEBUG_FUNCPTR (gst_dtls_dec_change_state);
  element_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_dtls_dec_request_new_pad);
  element_class->release_pad = GST_DEBUG_FUNCPTR (gst_dtls_dec_release_pad);

  signals[SIGNAL_ON_KEY_RECEIVED] =
      g_signal_new ("on-key-received", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  properties[PROP_CONNECTION_ID] =
      g_param_spec_string ("connection-id",
      "Connection id",
      "Every encoder/decoder pair should have the same, unique, connection-id",
      DEFAULT_CONNECTION_ID, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_PEM] =
      g_param_spec_string ("pem",
      "PEM string",
      "A string containing a X509 certificate and RSA private key in PEM format",
      DEFAULT_PEM,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_DOC_SHOW_DEFAULT);

  properties[PROP_PEER_PEM] =
      g_param_spec_string ("peer-pem",
      "Peer PEM string",
      "The X509 certificate received in the DTLS handshake, in PEM format",
      DEFAULT_PEER_PEM, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_DECODER_KEY] =
      g_param_spec_boxed ("decoder-key",
      "Decoder key",
      "SRTP key that should be used by the decoder",
      GST_TYPE_CAPS, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

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
      "DTLS Decoder",
      "Decoder/Network/DTLS",
      "Decodes DTLS packets", "Patrik Oldsberg patrik.oldsberg@ericsson.com");
}

static void
gst_dtls_dec_init (GstDtlsDec * self)
{
  self->agent = get_agent_by_pem (NULL);
  self->connection_id = NULL;
  self->connection = NULL;
  self->peer_pem = NULL;

  self->decoder_key = NULL;
  self->srtp_cipher = DEFAULT_SRTP_CIPHER;
  self->srtp_auth = DEFAULT_SRTP_AUTH;

  g_mutex_init (&self->src_mutex);

  self->src = NULL;
  self->sink = gst_pad_new_from_static_template (&sink_template, "sink");
  g_return_if_fail (self->sink);

  gst_pad_set_chain_function (self->sink, GST_DEBUG_FUNCPTR (sink_chain));
  gst_pad_set_chain_list_function (self->sink,
      GST_DEBUG_FUNCPTR (sink_chain_list));

  gst_element_add_pad (GST_ELEMENT (self), self->sink);
}

static void
gst_dtls_dec_finalize (GObject * object)
{
  GstDtlsDec *self = GST_DTLS_DEC (object);

  if (self->decoder_key) {
    gst_buffer_unref (self->decoder_key);
    self->decoder_key = NULL;
  }

  g_free (self->connection_id);
  self->connection_id = NULL;

  g_free (self->peer_pem);
  self->peer_pem = NULL;

  g_mutex_clear (&self->src_mutex);

  GST_LOG_OBJECT (self, "finalized");

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_dtls_dec_dispose (GObject * object)
{
  GstDtlsDec *self = GST_DTLS_DEC (object);

  if (self->agent) {
    g_object_unref (self->agent);
    self->agent = NULL;
  }

  if (self->connection) {
    g_object_unref (self->connection);
    self->connection = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_dtls_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDtlsDec *self = GST_DTLS_DEC (object);

  switch (prop_id) {
    case PROP_CONNECTION_ID:
      g_free (self->connection_id);
      self->connection_id = g_value_dup_string (value);
      g_return_if_fail (self->agent);
      create_connection (self, self->connection_id);
      break;
    case PROP_PEM:
      if (self->agent) {
        g_object_unref (self->agent);
      }
      self->agent = get_agent_by_pem (g_value_get_string (value));
      if (self->connection_id) {
        create_connection (self, self->connection_id);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
  }
}

static void
gst_dtls_dec_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstDtlsDec *self = GST_DTLS_DEC (object);

  switch (prop_id) {
    case PROP_CONNECTION_ID:
      g_value_set_string (value, self->connection_id);
      break;
    case PROP_PEM:
      g_value_take_string (value,
          gst_dtls_agent_get_certificate_pem (self->agent));
      break;
    case PROP_PEER_PEM:
      g_value_set_string (value, self->peer_pem);
      break;
    case PROP_DECODER_KEY:
      g_value_set_boxed (value, self->decoder_key);
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

static GstStateChangeReturn
gst_dtls_dec_change_state (GstElement * element, GstStateChange transition)
{
  GstDtlsDec *self = GST_DTLS_DEC (element);
  GstStateChangeReturn ret;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (self->connection) {
        g_signal_connect_object (self->connection,
            "on-decoder-key", G_CALLBACK (on_key_received), self, 0);
        g_signal_connect_object (self->connection,
            "on-peer-certificate", G_CALLBACK (on_peer_certificate_received),
            self, 0);
      } else {
        GST_WARNING_OBJECT (self,
            "trying to change state to ready without connection id and pem");
        return GST_STATE_CHANGE_FAILURE;
      }
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  return ret;
}

static gboolean
forward_sticky_events (GstPad * pad, GstEvent ** event, gpointer user_data)
{
  GstPad *srcpad = GST_PAD_CAST (user_data);
  GstFlowReturn ret;

  ret = gst_pad_store_sticky_event (srcpad, *event);
  if (ret != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (srcpad, "storing sticky event %p (%s) failed: %s", *event,
        GST_EVENT_TYPE_NAME (*event), gst_flow_get_name (ret));
  }

  return TRUE;
}

static GstPad *
gst_dtls_dec_request_new_pad (GstElement * element,
    GstPadTemplate * tmpl, const gchar * name, const GstCaps * caps)
{
  GstDtlsDec *self = GST_DTLS_DEC (element);
  GstPad *pad;

  GST_DEBUG_OBJECT (element, "requesting pad");

  g_return_val_if_fail (!self->src, NULL);
  g_return_val_if_fail (tmpl->direction == GST_PAD_SRC, NULL);

  g_mutex_lock (&self->src_mutex);
  if (self->src) {
    GST_ERROR_OBJECT (self, "Pad %s:%s exists already",
        GST_DEBUG_PAD_NAME (self->src));
    g_mutex_unlock (&self->src_mutex);
    return NULL;
  }

  self->src = pad = gst_pad_new_from_template (tmpl, name);

  g_mutex_unlock (&self->src_mutex);

  gst_pad_set_active (pad, TRUE);

  if (caps)
    gst_pad_set_caps (pad, (GstCaps *) caps);

  /* Forward sticky events to the new srcpad */
  gst_pad_sticky_events_foreach (self->sink, forward_sticky_events, self->src);

  gst_element_add_pad (element, pad);

  return pad;
}

static void
gst_dtls_dec_release_pad (GstElement * element, GstPad * pad)
{
  GstDtlsDec *self = GST_DTLS_DEC (element);

  g_return_if_fail (self->src == pad);

  g_mutex_lock (&self->src_mutex);

  self->src = NULL;
  g_mutex_unlock (&self->src_mutex);

  GST_DEBUG_OBJECT (self, "releasing src pad");

  gst_element_remove_pad (element, pad);
}

static void
on_key_received (GstDtlsConnection * connection, gpointer key, guint cipher,
    guint auth, GstDtlsDec * self)
{
  GstBuffer *new_decoder_key;
  gchar *key_str;

  g_return_if_fail (GST_IS_DTLS_DEC (self));

  self->srtp_cipher = cipher;
  self->srtp_auth = auth;

  new_decoder_key =
      gst_buffer_new_memdup (key, GST_DTLS_SRTP_MASTER_KEY_LENGTH);

  if (self->decoder_key)
    gst_buffer_unref (self->decoder_key);

  self->decoder_key = new_decoder_key;

  key_str = g_base64_encode (key, GST_DTLS_SRTP_MASTER_KEY_LENGTH);
  GST_INFO_OBJECT (self, "received key: %s", key_str);
  g_free (key_str);

  g_signal_emit (self, signals[SIGNAL_ON_KEY_RECEIVED], 0);
}

static gboolean
on_peer_certificate_received (GstDtlsConnection * connection, gchar * pem,
    GstDtlsDec * self)
{
  g_return_val_if_fail (GST_IS_DTLS_DEC (self), TRUE);

  GST_DEBUG_OBJECT (self, "Received peer certificate PEM: \n%s", pem);

  if (self->peer_pem != NULL) {
    g_free (self->peer_pem);
    self->peer_pem = NULL;
  }
  self->peer_pem = g_strdup (pem);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PEER_PEM]);

  return TRUE;
}

static GstFlowReturn
process_buffer (GstDtlsDec * self, GstBuffer * buffer)
{
  GstFlowReturn flow_ret;
  GstMapInfo map_info;
  GError *err = NULL;
  gsize written = 0;

  if (!gst_buffer_map (buffer, &map_info, GST_MAP_READWRITE))
    return GST_FLOW_ERROR;

  if (!map_info.size) {
    gst_buffer_unmap (buffer, &map_info);
    return GST_FLOW_ERROR;
  }

  flow_ret =
      gst_dtls_connection_process (self->connection, map_info.data,
      map_info.size, &written, &err);
  gst_buffer_unmap (buffer, &map_info);

  switch (flow_ret) {
    case GST_FLOW_OK:
      GST_LOG_OBJECT (self,
          "Decoded buffer of size %" G_GSIZE_FORMAT " B to %" G_GSIZE_FORMAT,
          map_info.size, written);
      gst_buffer_set_size (buffer, written);
      break;
    case GST_FLOW_EOS:
      gst_buffer_set_size (buffer, written);
      GST_DEBUG_OBJECT (self, "Peer closed the connection");
      break;
    case GST_FLOW_ERROR:
      GST_ERROR_OBJECT (self, "Error processing buffer: %s", err->message);
      GST_ELEMENT_ERROR (self, RESOURCE, READ, (NULL), ("%s", err->message));
      g_clear_error (&err);
      break;
    default:
      g_assert_not_reached ();
  }
  g_assert (err == NULL);

  return flow_ret;
}

typedef struct
{
  GstDtlsDec *self;
  GstFlowReturn flow_ret;
  guint processed;
} ProcessListData;

static gboolean
process_buffer_from_list (GstBuffer ** buffer, guint idx, gpointer user_data)
{
  ProcessListData *process_list_data = user_data;
  GstDtlsDec *self = GST_DTLS_DEC (process_list_data->self);
  GstFlowReturn flow_ret;

  *buffer = gst_buffer_make_writable (*buffer);
  flow_ret = process_buffer (self, *buffer);

  process_list_data->flow_ret = flow_ret;
  if (gst_buffer_get_size (*buffer) == 0)
    gst_buffer_replace (buffer, NULL);
  else if (flow_ret != GST_FLOW_ERROR)
    process_list_data->processed++;

  return flow_ret == GST_FLOW_OK;
}

static GstFlowReturn
sink_chain_list (GstPad * pad, GstObject * parent, GstBufferList * list)
{
  GstDtlsDec *self = GST_DTLS_DEC (parent);
  GstPad *other_pad;
  ProcessListData process_list_data = { self, GST_FLOW_OK, 0 };

  list = gst_buffer_list_make_writable (list);
  gst_buffer_list_foreach (list, process_buffer_from_list, &process_list_data);

  /* If we successfully processed at least some buffers then forward those */
  if (process_list_data.flow_ret != GST_FLOW_OK
      && process_list_data.processed == 0) {
    GST_ERROR_OBJECT (self, "Failed to process buffer list: %s",
        gst_flow_get_name (process_list_data.flow_ret));
    gst_buffer_list_unref (list);
    return process_list_data.flow_ret;
  }

  /* Remove all buffers after the first one that failed to be processed */
  gst_buffer_list_remove (list, process_list_data.processed,
      gst_buffer_list_length (list) - process_list_data.processed);

  if (gst_buffer_list_length (list) == 0) {
    GST_DEBUG_OBJECT (self, "Not produced any buffers");
    gst_buffer_list_unref (list);

    return process_list_data.flow_ret;
  }

  g_mutex_lock (&self->src_mutex);
  other_pad = self->src;
  if (other_pad)
    gst_object_ref (other_pad);
  g_mutex_unlock (&self->src_mutex);

  if (other_pad) {
    gboolean was_eos = process_list_data.flow_ret == GST_FLOW_EOS;

    GST_LOG_OBJECT (self, "pushing buffer list with length %u",
        gst_buffer_list_length (list));
    process_list_data.flow_ret = gst_pad_push_list (other_pad, list);

    /* If the peer closed the connection, signal that we're done here now */
    if (was_eos)
      gst_pad_push_event (other_pad, gst_event_new_eos ());

    gst_object_unref (other_pad);
  } else {
    GST_LOG_OBJECT (self,
        "dropping buffer list with length %d, have no source pad",
        gst_buffer_list_length (list));
    gst_buffer_list_unref (list);
  }

  return process_list_data.flow_ret;
}

static GstFlowReturn
sink_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstDtlsDec *self = GST_DTLS_DEC (parent);
  GstFlowReturn ret = GST_FLOW_OK;
  GstPad *other_pad;

  if (!self->agent) {
    gst_buffer_unref (buffer);
    return GST_FLOW_OK;
  }

  GST_DEBUG_OBJECT (self,
      "received buffer from %s with length %" G_GSIZE_FORMAT,
      self->connection_id, gst_buffer_get_size (buffer));

  buffer = gst_buffer_make_writable (buffer);
  ret = process_buffer (self, buffer);
  if (ret == GST_FLOW_ERROR) {
    GST_ERROR_OBJECT (self, "Failed to process buffer: %s",
        gst_flow_get_name (ret));
    gst_buffer_unref (buffer);
    return ret;
  }

  g_mutex_lock (&self->src_mutex);
  other_pad = self->src;
  if (other_pad)
    gst_object_ref (other_pad);
  g_mutex_unlock (&self->src_mutex);

  if (other_pad) {
    gboolean was_eos = (ret == GST_FLOW_EOS);

    if (gst_buffer_get_size (buffer) > 0) {
      GST_LOG_OBJECT (self, "pushing buffer");
      ret = gst_pad_push (other_pad, buffer);
    } else {
      gst_buffer_unref (buffer);
    }

    /* If the peer closed the connection, signal that we're done here now */
    if (was_eos) {
      gst_pad_push_event (other_pad, gst_event_new_eos ());
      if (ret == GST_FLOW_OK)
        ret = GST_FLOW_EOS;
    }

    gst_object_unref (other_pad);
  } else {
    GST_LOG_OBJECT (self, "dropping buffer, have no source pad");
    gst_buffer_unref (buffer);
  }

  return ret;
}

static GHashTable *agent_table = NULL;
G_LOCK_DEFINE_STATIC (agent_table);

static GstDtlsAgent *generated_cert_agent = NULL;

static GstDtlsAgent *
get_agent_by_pem (const gchar * pem)
{
  GstDtlsAgent *agent;

  if (!pem) {
    if (g_once_init_enter (&generated_cert_agent)) {
      GstDtlsAgent *new_agent;
      GObject *certificate;

      certificate = g_object_new (GST_TYPE_DTLS_CERTIFICATE, NULL);
      new_agent = g_object_new (GST_TYPE_DTLS_AGENT, "certificate",
          certificate, NULL);
      g_object_unref (certificate);

      GST_DEBUG_OBJECT (generated_cert_agent,
          "no agent with generated cert found, creating new");
      g_once_init_leave (&generated_cert_agent, new_agent);
    } else {
      GST_DEBUG_OBJECT (generated_cert_agent,
          "using agent with generated cert");
    }

    agent = generated_cert_agent;
    g_object_ref (agent);
  } else {
    G_LOCK (agent_table);

    if (!agent_table) {
      agent_table =
          g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    }

    agent = GST_DTLS_AGENT (g_hash_table_lookup (agent_table, pem));

    if (!agent) {
      GObject *certificate;

      certificate = g_object_new (GST_TYPE_DTLS_CERTIFICATE, "pem", pem, NULL);
      agent = g_object_new (GST_TYPE_DTLS_AGENT, "certificate", certificate,
          NULL);
      g_object_unref (certificate);

      g_object_weak_ref (G_OBJECT (agent), (GWeakNotify) agent_weak_ref_notify,
          (gpointer) g_strdup (pem));

      g_hash_table_insert (agent_table, g_strdup (pem), agent);

      GST_DEBUG_OBJECT (agent, "no agent found, created new");
    } else {
      g_object_ref (agent);
      GST_DEBUG_OBJECT (agent, "agent found");
    }

    G_UNLOCK (agent_table);
  }


  return agent;
}

static void
agent_weak_ref_notify (gchar * pem, GstDtlsAgent * agent)
{
  G_LOCK (agent_table);
  g_hash_table_remove (agent_table, pem);
  G_UNLOCK (agent_table);

  g_free (pem);
  pem = NULL;
}

static GHashTable *connection_table = NULL;
G_LOCK_DEFINE_STATIC (connection_table);

GstDtlsConnection *
gst_dtls_dec_fetch_connection (gchar * id)
{
  GstDtlsConnection *connection;
  g_return_val_if_fail (id, NULL);

  GST_DEBUG ("fetching '%s' from connection table, size is %d",
      id, g_hash_table_size (connection_table));

  G_LOCK (connection_table);

  connection = g_hash_table_lookup (connection_table, id);

  if (connection) {
    g_object_ref (connection);
    g_hash_table_remove (connection_table, id);
  } else {
    GST_WARNING ("no connection with id '%s' found", id);
  }

  G_UNLOCK (connection_table);

  return connection;
}

static void
on_connection_state_changed (GObject * object, GParamSpec * pspec,
    gpointer user_data)
{
  GstDtlsDec *self = GST_DTLS_DEC (user_data);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CONNECTION_STATE]);
}

static void
create_connection (GstDtlsDec * self, gchar * id)
{
  g_return_if_fail (GST_IS_DTLS_DEC (self));
  g_return_if_fail (GST_IS_DTLS_AGENT (self->agent));

  if (self->connection) {
    g_signal_handlers_disconnect_by_func (self->connection,
        on_connection_state_changed, self);
    g_object_unref (self->connection);
    self->connection = NULL;
  }

  G_LOCK (connection_table);

  if (!connection_table) {
    connection_table =
        g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  }

  if (g_hash_table_contains (connection_table, id)) {
    G_UNLOCK (connection_table);

    g_return_if_reached ();
  }

  self->connection =
      g_object_new (GST_TYPE_DTLS_CONNECTION, "agent", self->agent, NULL);
  g_signal_connect_object (self->connection,
      "notify::connection-state", G_CALLBACK (on_connection_state_changed),
      self, 0);
  on_connection_state_changed (NULL, NULL, self);

  g_object_weak_ref (G_OBJECT (self->connection),
      (GWeakNotify) connection_weak_ref_notify, g_strdup (id));

  g_hash_table_insert (connection_table, g_strdup (id), self->connection);

  G_UNLOCK (connection_table);
}

static void
connection_weak_ref_notify (gchar * id, GstDtlsConnection * connection)
{
  G_LOCK (connection_table);
  g_hash_table_remove (connection_table, id);
  G_UNLOCK (connection_table);

  g_free (id);
  id = NULL;
}
