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

#include <gst/gst.h>

#include "gstdtlsagent.h"

#ifdef __APPLE__
# define __AVAILABILITYMACROS__
# define DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER
#endif

#include <openssl/err.h>
#include <openssl/ssl.h>

GST_DEBUG_CATEGORY_STATIC (gst_dtls_agent_debug);
#define GST_CAT_DEFAULT gst_dtls_agent_debug

enum
{
  PROP_0,
  PROP_CERTIFICATE,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

struct _GstDtlsAgentPrivate
{
  SSL_CTX *ssl_context;

  GstDtlsCertificate *certificate;
};

G_DEFINE_TYPE_WITH_PRIVATE (GstDtlsAgent, gst_dtls_agent, GST_TYPE_OBJECT);

static void gst_dtls_agent_finalize (GObject * gobject);
static void gst_dtls_agent_set_property (GObject *, guint prop_id,
    const GValue *, GParamSpec *);
const gchar *gst_dtls_agent_peek_id (GstDtlsAgent *);

#if OPENSSL_VERSION_NUMBER < 0x10100000L
static GRWLock *ssl_locks;

static void
ssl_locking_function (gint mode, gint lock_num, const gchar * file, gint line)
{
  gboolean locking;
  gboolean reading;
  GRWLock *lock;

  locking = mode & CRYPTO_LOCK;
  reading = mode & CRYPTO_READ;
  lock = &ssl_locks[lock_num];

  GST_TRACE_OBJECT (NULL, "%s SSL lock for %s, thread=%p location=%s:%d",
      locking ? "locking" : "unlocking", reading ? "reading" : "writing",
      g_thread_self (), file, line);

  if (locking) {
    if (reading) {
      g_rw_lock_reader_lock (lock);
    } else {
      g_rw_lock_writer_lock (lock);
    }
  } else {
    if (reading) {
      g_rw_lock_reader_unlock (lock);
    } else {
      g_rw_lock_writer_unlock (lock);
    }
  }
}

static void
ssl_thread_id_function (CRYPTO_THREADID * id)
{
  CRYPTO_THREADID_set_pointer (id, g_thread_self ());
}
#endif

void
_gst_dtls_init_openssl (void)
{
  static gsize is_init = 0;

  if (g_once_init_enter (&is_init)) {
    GST_DEBUG_CATEGORY_INIT (gst_dtls_agent_debug, "dtlsagent", 0,
        "DTLS Agent");

    if (OPENSSL_VERSION_NUMBER < 0x1000100fL) {
      GST_WARNING_OBJECT (NULL,
          "Incorrect OpenSSL version, should be >= 1.0.1, is %s",
          OPENSSL_VERSION_TEXT);
      g_assert_not_reached ();
    }
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    GST_INFO_OBJECT (NULL, "initializing openssl %lx", OPENSSL_VERSION_NUMBER);
    SSL_library_init ();
    SSL_load_error_strings ();
    ERR_load_BIO_strings ();

    if (!CRYPTO_get_locking_callback ()) {
      gint i;
      gint num_locks;
      num_locks = CRYPTO_num_locks ();
      ssl_locks = g_new (GRWLock, num_locks);
      for (i = 0; i < num_locks; ++i) {
        g_rw_lock_init (&ssl_locks[i]);
      }
      CRYPTO_set_locking_callback (ssl_locking_function);
      CRYPTO_THREADID_set_callback (ssl_thread_id_function);
    }
#endif

    g_once_init_leave (&is_init, 1);
  }
}

static void
gst_dtls_agent_class_init (GstDtlsAgentClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gst_dtls_agent_set_property;
  gobject_class->finalize = gst_dtls_agent_finalize;

  properties[PROP_CERTIFICATE] =
      g_param_spec_object ("certificate",
      "GstDtlsCertificate",
      "Sets the certificate of the agent",
      GST_TYPE_DTLS_CERTIFICATE,
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);

  _gst_dtls_init_openssl ();
}

static int
ssl_warn_cb (const char *str, size_t len, void *u)
{
  GstDtlsAgent *self = u;
  GST_WARNING_OBJECT (self, "ssl error: %s", str);
  return 0;
}

static void
gst_dtls_agent_init (GstDtlsAgent * self)
{
  GstDtlsAgentPrivate *priv = gst_dtls_agent_get_instance_private (self);
  self->priv = priv;

  ERR_clear_error ();

#if OPENSSL_VERSION_NUMBER >= 0x1000200fL
  priv->ssl_context = SSL_CTX_new (DTLS_method ());
#else
  priv->ssl_context = SSL_CTX_new (DTLSv1_method ());
#endif
  if (!priv->ssl_context) {
    GST_WARNING_OBJECT (self, "Error creating SSL Context");
    ERR_print_errors_cb (ssl_warn_cb, self);

    g_return_if_reached ();
  }
  /* If any non-fatal issues happened, print them out and carry on */
  if (ERR_peek_error ()) {
    ERR_print_errors_cb (ssl_warn_cb, self);
    ERR_clear_error ();
  }

  SSL_CTX_set_verify_depth (priv->ssl_context, 2);
  SSL_CTX_set_tlsext_use_srtp (priv->ssl_context, "SRTP_AES128_CM_SHA1_80");
  SSL_CTX_set_cipher_list (priv->ssl_context,
      "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
  SSL_CTX_set_read_ahead (priv->ssl_context, 1);
#if (OPENSSL_VERSION_NUMBER >= 0x1000200fL) && (OPENSSL_VERSION_NUMBER < 0x10100000L)
  SSL_CTX_set_ecdh_auto (priv->ssl_context, 1);
#endif
}

static void
gst_dtls_agent_finalize (GObject * gobject)
{
  GstDtlsAgentPrivate *priv = GST_DTLS_AGENT (gobject)->priv;

  SSL_CTX_free (priv->ssl_context);
  priv->ssl_context = NULL;

  g_clear_object (&priv->certificate);

  GST_DEBUG_OBJECT (gobject, "finalized");

  G_OBJECT_CLASS (gst_dtls_agent_parent_class)->finalize (gobject);
}

static void
gst_dtls_agent_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDtlsAgent *self = GST_DTLS_AGENT (object);
  GstDtlsCertificate *certificate;

  switch (prop_id) {
    case PROP_CERTIFICATE:
      certificate = GST_DTLS_CERTIFICATE (g_value_get_object (value));
      g_return_if_fail (GST_IS_DTLS_CERTIFICATE (certificate));
      g_return_if_fail (self->priv->ssl_context);

      self->priv->certificate = certificate;
      g_object_ref (certificate);

      if (!SSL_CTX_use_certificate (self->priv->ssl_context,
              _gst_dtls_certificate_get_internal_certificate (certificate))) {
        GST_WARNING_OBJECT (self, "could not use certificate");
        g_return_if_reached ();
      }

      if (!SSL_CTX_use_PrivateKey (self->priv->ssl_context,
              _gst_dtls_certificate_get_internal_key (certificate))) {
        GST_WARNING_OBJECT (self, "could not use private key");
        g_return_if_reached ();
      }

      if (!SSL_CTX_check_private_key (self->priv->ssl_context)) {
        GST_WARNING_OBJECT (self, "invalid private key");
        g_return_if_reached ();
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
  }
}

GstDtlsCertificate *
gst_dtls_agent_get_certificate (GstDtlsAgent * self)
{
  g_return_val_if_fail (GST_IS_DTLS_AGENT (self), NULL);
  if (self->priv->certificate) {
    g_object_ref (self->priv->certificate);
  }
  return self->priv->certificate;
}

gchar *
gst_dtls_agent_get_certificate_pem (GstDtlsAgent * self)
{
  gchar *pem;
  g_return_val_if_fail (GST_IS_DTLS_AGENT (self), NULL);
  g_return_val_if_fail (GST_IS_DTLS_CERTIFICATE (self->priv->certificate),
      NULL);

  g_object_get (self->priv->certificate, "pem", &pem, NULL);

  return pem;
}

const GstDtlsAgentContext
_gst_dtls_agent_peek_context (GstDtlsAgent * self)
{
  g_return_val_if_fail (GST_IS_DTLS_AGENT (self), NULL);
  return self->priv->ssl_context;
}
