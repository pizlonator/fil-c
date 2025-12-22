/*
 * Copyright (c) 2014, Ericsson AB. All rights reserved.
 * All rights reserved.
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

#include "gstdtlssrtpbin.h"

#define gst_dtls_srtp_bin_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE (GstDtlsSrtpBin, gst_dtls_srtp_bin, GST_TYPE_BIN);

enum
{
  PROP_0,
  PROP_CONNECTION_ID,
  PROP_KEY,
  PROP_SRTP_AUTH,
  PROP_SRTP_CIPHER,
  PROP_SRTCP_AUTH,
  PROP_SRTCP_CIPHER,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

#define DEFAULT_CONNECTION_ID NULL
#define DEFAULT_KEY NULL
#define DEFAULT_SRTP_AUTH NULL
#define DEFAULT_SRTP_CIPHER NULL
#define DEFAULT_SRTCP_AUTH NULL
#define DEFAULT_SRTCP_CIPHER NULL

static void gst_dtls_srtp_bin_finalize (GObject *);
static void gst_dtls_srtp_bin_set_property (GObject *, guint prop_id,
    const GValue *, GParamSpec *);
static void gst_dtls_srtp_bin_get_property (GObject *, guint prop_id,
    GValue *, GParamSpec *);

static void
gst_dtls_srtp_bin_class_init (GstDtlsSrtpBinClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_dtls_srtp_bin_finalize);
  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_dtls_srtp_bin_set_property);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_dtls_srtp_bin_get_property);

  klass->remove_dtls_element = NULL;

  properties[PROP_CONNECTION_ID] =
      g_param_spec_string ("connection-id",
      "Connection id",
      "Every encoder/decoder pair should have the same, unique, connection-id",
      DEFAULT_CONNECTION_ID, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_KEY] =
      g_param_spec_boxed ("key",
      "Key",
      "SRTP master key, if this property is set, DTLS will be disabled",
      GST_TYPE_BUFFER,
      G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING | G_PARAM_STATIC_STRINGS);

  properties[PROP_SRTP_CIPHER] =
      g_param_spec_string ("srtp-cipher",
      "SRTP Cipher",
      "SRTP cipher name, should be 'null' or 'aes-128-icm', "
      "if this property is set, DTLS will be disabled",
      DEFAULT_SRTP_CIPHER,
      G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING | G_PARAM_STATIC_STRINGS);

  properties[PROP_SRTCP_CIPHER] =
      g_param_spec_string ("srtcp-cipher",
      "SRTCP Cipher",
      "SRTCP cipher name, should be 'null' or 'aes-128-icm', "
      "if this property is set, DTLS will be disabled",
      DEFAULT_SRTCP_CIPHER,
      G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING | G_PARAM_STATIC_STRINGS);

  properties[PROP_SRTP_AUTH] =
      g_param_spec_string ("srtp-auth",
      "SRTP Auth",
      "SRTP auth name, should be 'null', 'hmac-sha1-32' or 'hmac-sha1-80', "
      "if this property is set, DTLS will be disabled",
      DEFAULT_SRTP_AUTH,
      G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING | G_PARAM_STATIC_STRINGS);

  properties[PROP_SRTCP_AUTH] =
      g_param_spec_string ("srtcp-auth",
      "SRTCP Auth",
      "SRTCP auth name, should be 'null', 'hmac-sha1-32' or 'hmac-sha1-80', "
      "if this property is set, DTLS will be disabled",
      DEFAULT_SRTCP_AUTH,
      G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);

  gst_type_mark_as_plugin_api (GST_TYPE_DTLS_SRTP_BIN, 0);
}

static void
gst_dtls_srtp_bin_init (GstDtlsSrtpBin * self)
{
  self->key = NULL;
  self->key_is_set = FALSE;
  self->srtp_auth = NULL;
  self->srtp_cipher = NULL;
  self->srtcp_auth = NULL;
  self->srtcp_cipher = NULL;
}

static void
gst_dtls_srtp_bin_finalize (GObject * object)
{
  GstDtlsSrtpBin *self = GST_DTLS_SRTP_BIN (object);

  if (self->key) {
    gst_buffer_unref (self->key);
    self->key = NULL;
  }
  g_free (self->srtp_auth);
  self->srtp_auth = NULL;
  g_free (self->srtp_cipher);
  self->srtp_cipher = NULL;
  g_free (self->srtcp_auth);
  self->srtcp_auth = NULL;
  g_free (self->srtcp_cipher);
  self->srtcp_cipher = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_dtls_srtp_bin_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstDtlsSrtpBin *self = GST_DTLS_SRTP_BIN (object);
  GstDtlsSrtpBinClass *klass = GST_DTLS_SRTP_BIN_GET_CLASS (self);

  switch (prop_id) {
    case PROP_CONNECTION_ID:
      if (self->dtls_element) {
        g_object_set_property (G_OBJECT (self->dtls_element), "connection-id",
            value);
      } else {
        g_warning ("tried to set connection-id after disabling DTLS");
      }
      break;
    case PROP_KEY:
      if (self->key) {
        gst_buffer_unref (self->key);
      }
      self->key = g_value_dup_boxed (value);
      self->key_is_set = TRUE;
      klass->remove_dtls_element (self);
      break;
    case PROP_SRTP_AUTH:
      g_free (self->srtp_auth);
      self->srtp_auth = g_value_dup_string (value);
      klass->remove_dtls_element (self);
      break;
    case PROP_SRTP_CIPHER:
      g_free (self->srtp_cipher);
      self->srtp_cipher = g_value_dup_string (value);
      klass->remove_dtls_element (self);
      break;
    case PROP_SRTCP_AUTH:
      g_free (self->srtcp_auth);
      self->srtcp_auth = g_value_dup_string (value);
      klass->remove_dtls_element (self);
      break;
    case PROP_SRTCP_CIPHER:
      g_free (self->srtcp_cipher);
      self->srtcp_cipher = g_value_dup_string (value);
      klass->remove_dtls_element (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
  }
}

static void
gst_dtls_srtp_bin_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstDtlsSrtpBin *self = GST_DTLS_SRTP_BIN (object);

  switch (prop_id) {
    case PROP_CONNECTION_ID:
      if (self->dtls_element) {
        g_object_get_property (G_OBJECT (self->dtls_element), "connection-id",
            value);
      } else {
        GST_WARNING_OBJECT (self,
            "tried to get connection-id after disabling DTLS");
      }
      break;
    case PROP_KEY:
      if (self->key) {
        g_value_set_boxed (value, self->key);
      }
      break;
    case PROP_SRTP_AUTH:
      g_value_set_string (value, self->srtp_auth);
      break;
    case PROP_SRTP_CIPHER:
      g_value_set_string (value, self->srtp_cipher);
      break;
    case PROP_SRTCP_AUTH:
      g_value_set_string (value, self->srtcp_auth);
      break;
    case PROP_SRTCP_CIPHER:
      g_value_set_string (value, self->srtcp_cipher);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
  }
}
