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
#include "gstdtlssrtpenc.h"
#include "gstdtlsconnection.h"

#include <stdio.h>

static GstStaticPadTemplate rtp_sink_template =
    GST_STATIC_PAD_TEMPLATE ("rtp_sink_%d",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("application/x-rtp;application/x-rtcp")
    );

static GstStaticPadTemplate rtcp_sink_template =
    GST_STATIC_PAD_TEMPLATE ("rtcp_sink_%d",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("application/x-rtp;application/x-rtcp")
    );

static GstStaticPadTemplate data_sink_template =
GST_STATIC_PAD_TEMPLATE ("data_sink",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC (gst_dtls_srtp_enc_debug);
#define GST_CAT_DEFAULT gst_dtls_srtp_enc_debug

#define gst_dtls_srtp_enc_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstDtlsSrtpEnc, gst_dtls_srtp_enc,
    GST_TYPE_DTLS_SRTP_BIN,
    GST_DEBUG_CATEGORY_INIT (gst_dtls_srtp_enc_debug,
        "dtlssrtpenc", 0, "DTLS-SRTP Encoder"));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (dtlssrtpenc, "dtlssrtpenc",
    GST_RANK_NONE, GST_TYPE_DTLS_SRTP_ENC, dtls_element_init (plugin));

enum
{
  SIGNAL_ON_KEY_SET,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS];

enum
{
  PROP_0,
  PROP_IS_CLIENT,
  PROP_CONNECTION_STATE,
  PROP_RTP_SYNC,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

#define DEFAULT_IS_CLIENT FALSE
#define DEFAULT_RTP_SYNC FALSE

static gboolean transform_enum (GBinding *, const GValue * source_value,
    GValue * target_value, GEnumClass *);

static void gst_dtls_srtp_enc_set_property (GObject *, guint prop_id,
    const GValue *, GParamSpec *);
static void gst_dtls_srtp_enc_get_property (GObject *, guint prop_id,
    GValue *, GParamSpec *);

static GstPad *add_ghost_pad (GstElement *, const gchar * name, GstPad *,
    GstPadTemplate *);
static GstPad *gst_dtls_srtp_enc_request_new_pad (GstElement *,
    GstPadTemplate *, const gchar * name, const GstCaps *);

static void on_key_received (GObject * encoder, GstDtlsSrtpEnc *);

static void gst_dtls_srtp_enc_remove_dtls_element (GstDtlsSrtpBin *);
static GstPadProbeReturn remove_dtls_encoder_probe_callback (GstPad *,
    GstPadProbeInfo *, GstElement *);

static void
gst_dtls_srtp_enc_class_init (GstDtlsSrtpEncClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstDtlsSrtpBinClass *dtls_srtp_bin_class;

  gobject_class = (GObjectClass *) klass;
  element_class = (GstElementClass *) klass;
  dtls_srtp_bin_class = (GstDtlsSrtpBinClass *) klass;

  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_dtls_srtp_enc_set_property);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_dtls_srtp_enc_get_property);

  element_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_dtls_srtp_enc_request_new_pad);

  dtls_srtp_bin_class->remove_dtls_element =
      GST_DEBUG_FUNCPTR (gst_dtls_srtp_enc_remove_dtls_element);

  signals[SIGNAL_ON_KEY_SET] =
      g_signal_new ("on-key-set", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  properties[PROP_IS_CLIENT] =
      g_param_spec_boolean ("is-client",
      "Is client",
      "Set to true if the decoder should act as "
      "client and initiate the handshake",
      DEFAULT_IS_CLIENT,
      GST_PARAM_MUTABLE_READY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_CONNECTION_STATE] =
      g_param_spec_enum ("connection-state",
      "Connection State",
      "Current connection state",
      GST_DTLS_TYPE_CONNECTION_STATE,
      GST_DTLS_CONNECTION_STATE_NEW, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);


  properties[PROP_RTP_SYNC] =
      g_param_spec_boolean ("rtp-sync", "Synchronize RTP",
      "Synchronize RTP to the pipeline clock before merging with RTCP",
      DEFAULT_RTP_SYNC, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);

  gst_element_class_add_static_pad_template (element_class, &rtp_sink_template);
  gst_element_class_add_static_pad_template (element_class,
      &rtcp_sink_template);
  gst_element_class_add_static_pad_template (element_class,
      &data_sink_template);
  gst_element_class_add_static_pad_template (element_class, &src_template);

  gst_element_class_set_static_metadata (element_class,
      "DTLS-SRTP Encoder",
      "Encoder/Network/DTLS/SRTP",
      "Encodes SRTP packets with a key received from DTLS",
      "Patrik Oldsberg patrik.oldsberg@ericsson.com");
}

static void
on_connection_state_changed (GObject * object, GParamSpec * pspec,
    gpointer user_data)
{
  GstDtlsSrtpEnc *self = GST_DTLS_SRTP_ENC (user_data);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CONNECTION_STATE]);
}

static void
gst_dtls_srtp_enc_init (GstDtlsSrtpEnc * self)
{
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (GST_ELEMENT (self));
  static GEnumClass *cipher_enum_class, *auth_enum_class;
  gboolean ret;

/*
                 +--------------------+    +--------------+     +-----------------+
     rtp_sink-R-o|rtp_sink     rtp_src|o-R-o   clocksync  |o-R-o|                 |
                 |       srtpenc      |    +--------------+     |                 |
                 |                    |                         |                 |
    rtcp_sink-R-o|srtcp_sink  rtcp_src|o-----------R-----------o|                 |
                 +--------------------+                         |     funnel      |o---src
                                                                |                 |
                 +--------------------+                         |                 |
    data_sink-R-o|       dtlsenc      |o-----------------------o|                 |
                 +--------------------+                         +-----------------+

    The clocksync element is tied to the sync property. If sync=true, RTP output will be
    synchronised to the clock, so it doesn't slow down RTCP traffic by being synched later
    in the pipeline
*/

  self->srtp_enc = gst_element_factory_make ("srtpenc", NULL);
  if (!self->srtp_enc) {
    GST_ERROR_OBJECT (self,
        "failed to create srtp encoder, is the srtp plugin registered?");
    return;
  }
  g_return_if_fail (self->srtp_enc);
  self->bin.dtls_element = gst_element_factory_make ("dtlsenc", NULL);
  if (!self->bin.dtls_element) {
    GST_ERROR_OBJECT (self, "failed to create dtls encoder");
    return;
  }
  self->funnel = gst_element_factory_make ("funnel", NULL);
  if (!self->funnel) {
    GST_ERROR_OBJECT (self, "failed to create funnel");
    return;
  }

  gst_bin_add_many (GST_BIN (self), self->bin.dtls_element, self->srtp_enc,
      self->funnel, NULL);

  ret = gst_element_link (self->bin.dtls_element, self->funnel);
  g_return_if_fail (ret);

  add_ghost_pad (GST_ELEMENT (self), "src",
      gst_element_get_static_pad (self->funnel, "src"),
      gst_element_class_get_pad_template (klass, "src"));

  g_signal_connect (self->bin.dtls_element, "on-key-received",
      G_CALLBACK (on_key_received), self);

  if (g_once_init_enter (&cipher_enum_class)) {
    GType type = g_type_from_name ("GstSrtpCipherType");
    g_assert (type);
    g_once_init_leave (&cipher_enum_class, g_type_class_peek (type));
  }
  if (g_once_init_enter (&auth_enum_class)) {
    GType type = g_type_from_name ("GstSrtpAuthType");
    g_assert (type);
    g_once_init_leave (&auth_enum_class, g_type_class_peek (type));
  }

  g_object_set (self->srtp_enc, "random-key", TRUE, NULL);

  g_signal_connect (self->bin.dtls_element, "notify::connection-state",
      G_CALLBACK (on_connection_state_changed), self);

  g_object_bind_property (G_OBJECT (self), "key", self->srtp_enc, "key",
      G_BINDING_DEFAULT);
  g_object_bind_property_full (G_OBJECT (self), "srtp-cipher", self->srtp_enc,
      "rtp-cipher", G_BINDING_DEFAULT, (GBindingTransformFunc) transform_enum,
      NULL, cipher_enum_class, NULL);
  g_object_bind_property_full (G_OBJECT (self), "srtcp-cipher", self->srtp_enc,
      "rtcp-cipher", G_BINDING_DEFAULT, (GBindingTransformFunc) transform_enum,
      NULL, cipher_enum_class, NULL);
  g_object_bind_property_full (G_OBJECT (self), "srtp-auth", self->srtp_enc,
      "rtp-auth", G_BINDING_DEFAULT, (GBindingTransformFunc) transform_enum,
      NULL, auth_enum_class, NULL);
  g_object_bind_property_full (G_OBJECT (self), "srtcp-auth", self->srtp_enc,
      "rtcp-auth", G_BINDING_DEFAULT, (GBindingTransformFunc) transform_enum,
      NULL, auth_enum_class, NULL);
}

#if GLIB_CHECK_VERSION(2,68,0)
#define binding_get_source(b) g_binding_dup_source(b)
#define unref_source(s) G_STMT_START { if(s) g_object_unref(s); } G_STMT_END
#else
#define binding_get_source(b) g_binding_get_source(b)
#define unref_source(s)         /* no op */
#endif

static gboolean
transform_enum (GBinding * binding, const GValue * source_value,
    GValue * target_value, GEnumClass * enum_class)
{
  GEnumValue *enum_value;
  const gchar *nick;
  GObject *bind_src;

  nick = g_value_get_string (source_value);
  g_return_val_if_fail (nick, FALSE);

  enum_value = g_enum_get_value_by_nick (enum_class, nick);
  g_return_val_if_fail (enum_value, FALSE);

  bind_src = binding_get_source (binding);

  GST_DEBUG_OBJECT (bind_src,
      "transforming enum from %s to %d", nick, enum_value->value);

  unref_source (bind_src);

  g_value_set_enum (target_value, enum_value->value);

  return TRUE;
}

static void
gst_dtls_srtp_enc_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstDtlsSrtpEnc *self = GST_DTLS_SRTP_ENC (object);

  switch (prop_id) {
    case PROP_IS_CLIENT:
      if (self->bin.dtls_element) {
        g_object_set_property (G_OBJECT (self->bin.dtls_element), "is-client",
            value);
      } else {
        GST_WARNING_OBJECT (self,
            "tried to set is-client after disabling DTLS");
      }
      break;
    case PROP_RTP_SYNC:
      self->rtp_sync = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
  }
}

static void
gst_dtls_srtp_enc_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstDtlsSrtpEnc *self = GST_DTLS_SRTP_ENC (object);

  switch (prop_id) {
    case PROP_IS_CLIENT:
      if (self->bin.dtls_element) {
        g_object_get_property (G_OBJECT (self->bin.dtls_element), "is-client",
            value);
      } else {
        GST_WARNING_OBJECT (self,
            "tried to get is-client after disabling DTLS");
      }
      break;
    case PROP_CONNECTION_STATE:
      if (self->bin.dtls_element) {
        g_object_get_property (G_OBJECT (self->bin.dtls_element),
            "connection-state", value);
      } else {
        GST_WARNING_OBJECT (self,
            "tried to get connection-state after disabling DTLS");
      }
      break;
    case PROP_RTP_SYNC:
      g_value_set_boolean (value, self->rtp_sync);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
  }
}

static GstPad *
add_ghost_pad (GstElement * element,
    const gchar * name, GstPad * target, GstPadTemplate * templ)
{
  GstPad *pad;
  gboolean ret;

  pad = gst_ghost_pad_new_from_template (name, target, templ);
  gst_object_unref (target);
  target = NULL;

  ret = gst_pad_set_active (pad, TRUE);
  g_warn_if_fail (ret);

  ret = gst_element_add_pad (element, pad);
  g_warn_if_fail (ret);

  return pad;
}

static GstPad *
gst_dtls_srtp_enc_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
  GstDtlsSrtpEnc *self = GST_DTLS_SRTP_ENC (element);
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (element);
  GstPad *target_pad;
  GstPad *ghost_pad = NULL;
  guint pad_n;
  gchar *srtp_src_name;

  GST_DEBUG_OBJECT (element, "pad requested");

  g_return_val_if_fail (templ->direction == GST_PAD_SINK, NULL);
  g_return_val_if_fail (self->srtp_enc, NULL);

  if (name == NULL)
    return NULL;

  if (templ == gst_element_class_get_pad_template (klass, "rtp_sink_%d")) {
    gchar *clocksync_name;
    GstElement *clocksync;

    sscanf (name, "rtp_sink_%d", &pad_n);

    clocksync_name = g_strdup_printf ("clocksync_%d", pad_n);
    clocksync = gst_element_factory_make ("clocksync", clocksync_name);
    g_free (clocksync_name);

    if (clocksync == NULL) {
      goto fail_create;
    }

    g_object_bind_property (self, "rtp-sync", clocksync, "sync",
        G_BINDING_SYNC_CREATE);

    gst_bin_add (GST_BIN (self), clocksync);
    gst_element_sync_state_with_parent (clocksync);

    target_pad = gst_element_request_pad_simple (self->srtp_enc, name);
    g_return_val_if_fail (target_pad, NULL);

    srtp_src_name = g_strdup_printf ("rtp_src_%d", pad_n);

    gst_element_link_pads (self->srtp_enc, srtp_src_name, clocksync, NULL);
    gst_element_link_pads (clocksync, "src", self->funnel, NULL);

    g_free (srtp_src_name);

    ghost_pad = add_ghost_pad (element, name, target_pad, templ);

    GST_LOG_OBJECT (self, "added rtp sink pad");
  } else if (templ == gst_element_class_get_pad_template (klass,
          "rtcp_sink_%d")) {
    target_pad = gst_element_request_pad_simple (self->srtp_enc, name);
    g_return_val_if_fail (target_pad, NULL);

    sscanf (GST_PAD_NAME (target_pad), "rtcp_sink_%d", &pad_n);
    srtp_src_name = g_strdup_printf ("rtcp_src_%d", pad_n);

    gst_element_link_pads (self->srtp_enc, srtp_src_name, self->funnel, NULL);

    g_free (srtp_src_name);

    ghost_pad = add_ghost_pad (element, name, target_pad, templ);

    GST_LOG_OBJECT (self, "added rtcp sink pad");
  } else if (templ == gst_element_class_get_pad_template (klass, "data_sink")) {
    g_return_val_if_fail (self->bin.dtls_element, NULL);
    target_pad =
        gst_element_request_pad_simple (self->bin.dtls_element, "sink");

    ghost_pad = add_ghost_pad (element, name, target_pad, templ);

    GST_LOG_OBJECT (self, "added data sink pad");
  } else {
    g_warn_if_reached ();
  }

  if (caps && ghost_pad) {
    g_object_set (ghost_pad, "caps", caps, NULL);
  }

  return ghost_pad;

fail_create:
  GST_ELEMENT_ERROR (self, CORE, MISSING_PLUGIN, NULL,
      ("%s", "Failed to create internal clocksync element"));
  return NULL;
}

static void
on_key_received (GObject * encoder, GstDtlsSrtpEnc * self)
{
  GstDtlsSrtpBin *bin = GST_DTLS_SRTP_BIN (self);
  GstBuffer *buffer = NULL;
  guint cipher, auth;

  if (!(bin->key_is_set || bin->srtp_cipher || bin->srtp_auth
          || bin->srtcp_cipher || bin->srtcp_auth)) {
    g_object_get (encoder,
        "encoder-key", &buffer,
        "srtp-cipher", &cipher, "srtp-auth", &auth, NULL);

    g_object_set (self->srtp_enc,
        "rtp-cipher", cipher,
        "rtcp-cipher", cipher,
        "rtp-auth", auth,
        "rtcp-auth", auth, "key", buffer, "random-key", FALSE, NULL);

    gst_buffer_unref (buffer);

    g_signal_emit (self, signals[SIGNAL_ON_KEY_SET], 0);
  } else {
    GST_DEBUG_OBJECT (self,
        "ignoring keys received from DTLS handshake, key struct is set");
  }
}

static void
gst_dtls_srtp_enc_remove_dtls_element (GstDtlsSrtpBin * bin)
{
  GstDtlsSrtpEnc *self = GST_DTLS_SRTP_ENC (bin);
  GstPad *dtls_sink_pad, *peer_pad;
  gulong id;
  guint rtp_cipher = 1, rtcp_cipher = 1, rtp_auth = 1, rtcp_auth = 1;

  if (!bin->dtls_element) {
    return;
  }

  g_object_get (self->srtp_enc,
      "rtp-cipher", &rtp_cipher,
      "rtcp-cipher", &rtcp_cipher,
      "rtp-auth", &rtp_auth, "rtcp-auth", &rtcp_auth, NULL);

  if (!rtp_cipher && !rtcp_cipher && !rtp_auth && !rtcp_auth) {
    g_object_set (self->srtp_enc, "random-key", FALSE, NULL);
  }

  dtls_sink_pad = gst_element_get_static_pad (bin->dtls_element, "sink");

  if (!dtls_sink_pad) {
    gst_element_set_state (GST_ELEMENT (bin->dtls_element), GST_STATE_NULL);
    gst_bin_remove (GST_BIN (self), bin->dtls_element);
    bin->dtls_element = NULL;
    return;
  }

  peer_pad = gst_pad_get_peer (dtls_sink_pad);
  g_return_if_fail (peer_pad);
  gst_object_unref (dtls_sink_pad);
  dtls_sink_pad = NULL;

  id = gst_pad_add_probe (peer_pad, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
      (GstPadProbeCallback) remove_dtls_encoder_probe_callback,
      bin->dtls_element, NULL);
  g_return_if_fail (id);
  bin->dtls_element = NULL;

  gst_pad_push_event (peer_pad,
      gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM,
          gst_structure_new_empty ("dummy")));

  gst_object_unref (peer_pad);
}

static GstPadProbeReturn
remove_dtls_encoder_probe_callback (GstPad * pad,
    GstPadProbeInfo * info, GstElement * element)
{
  gst_pad_remove_probe (pad, GST_PAD_PROBE_INFO_ID (info));

  gst_element_set_state (GST_ELEMENT (element), GST_STATE_NULL);
  gst_bin_remove (GST_BIN (GST_ELEMENT_PARENT (element)), element);

  return GST_PAD_PROBE_OK;
}
