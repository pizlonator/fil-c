/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gtlsserverconnection-openssl.c
 *
 * Copyright (C) 2015 NICE s.r.l.
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, when the library is used with OpenSSL, a special
 * exception applies. Refer to the LICENSE_EXCEPTION file for details.
 *
 * Authors: Ignacio Casal Quinteiro
 */

#include "config.h"
#include "glib.h"
#include "gtlsserverconnection-openssl.h"
#include "gtlscertificate-openssl.h"

#include "openssl-include.h"
#include <glib/gi18n-lib.h>

struct _GTlsServerConnectionOpenssl
{
  GTlsConnectionOpenssl parent_instance;

  GTlsAuthenticationMode authentication_mode;
  SSL_SESSION *session;
  SSL *ssl;
  SSL_CTX *ssl_ctx;
};

enum
{
  PROP_0,
  PROP_AUTHENTICATION_MODE
};

static void g_tls_server_connection_openssl_initable_interface_init (GInitableIface  *iface);

static void g_tls_server_connection_openssl_server_connection_interface_init (GTlsServerConnectionInterface *iface);

static GInitableIface *g_tls_server_connection_openssl_parent_initable_iface;

G_DEFINE_FINAL_TYPE_WITH_CODE (GTlsServerConnectionOpenssl, g_tls_server_connection_openssl, G_TYPE_TLS_CONNECTION_OPENSSL,
                               G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                      g_tls_server_connection_openssl_initable_interface_init)
                               G_IMPLEMENT_INTERFACE (G_TYPE_TLS_SERVER_CONNECTION,
                                                      g_tls_server_connection_openssl_server_connection_interface_init)
                               G_IMPLEMENT_INTERFACE (G_TYPE_DTLS_SERVER_CONNECTION,
                                                      NULL));

static void
g_tls_server_connection_openssl_finalize (GObject *object)
{
  GTlsServerConnectionOpenssl *openssl = G_TLS_SERVER_CONNECTION_OPENSSL (object);

  SSL_free (openssl->ssl);
  SSL_CTX_free (openssl->ssl_ctx);
  SSL_SESSION_free (openssl->session);

  G_OBJECT_CLASS (g_tls_server_connection_openssl_parent_class)->finalize (object);
}

static void
g_tls_server_connection_openssl_get_property (GObject    *object,
                                              guint       prop_id,
                                              GValue     *value,
                                              GParamSpec *pspec)
{
  GTlsServerConnectionOpenssl *openssl = G_TLS_SERVER_CONNECTION_OPENSSL (object);

  switch (prop_id)
    {
    case PROP_AUTHENTICATION_MODE:
      g_value_set_enum (value, openssl->authentication_mode);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_tls_server_connection_openssl_set_property (GObject      *object,
                                              guint         prop_id,
                                              const GValue *value,
                                              GParamSpec   *pspec)
{
  GTlsServerConnectionOpenssl *openssl = G_TLS_SERVER_CONNECTION_OPENSSL (object);

  switch (prop_id)
    {
    case PROP_AUTHENTICATION_MODE:
      openssl->authentication_mode = g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static int
verify_callback (int             preverify_ok,
                 X509_STORE_CTX *ctx)
{
  return 1;
}

static void
g_tls_server_connection_openssl_prepare_handshake (GTlsConnectionBase  *tls,
                                                   gchar              **advertised_protocols)
{
  GTlsServerConnectionOpenssl *openssl = G_TLS_SERVER_CONNECTION_OPENSSL (tls);
  GTlsConnectionBaseClass *base_class = G_TLS_CONNECTION_BASE_CLASS (g_tls_server_connection_openssl_parent_class);
  int req_mode = 0;

  switch (openssl->authentication_mode)
    {
    case G_TLS_AUTHENTICATION_REQUIRED:
      req_mode = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
      break;
    case G_TLS_AUTHENTICATION_REQUESTED:
      req_mode = SSL_VERIFY_PEER;
      break;
    case G_TLS_AUTHENTICATION_NONE:
    default:
      req_mode = SSL_VERIFY_NONE;
      break;
    }

  SSL_set_verify (openssl->ssl, req_mode, verify_callback);
  /* FIXME: is this ok? */
  SSL_set_verify_depth (openssl->ssl, 0);

  if (base_class->prepare_handshake)
    base_class->prepare_handshake (tls, advertised_protocols);
}

static SSL *
g_tls_server_connection_openssl_get_ssl (GTlsConnectionOpenssl *connection)
{
  return G_TLS_SERVER_CONNECTION_OPENSSL (connection)->ssl;
}

static gboolean
ssl_set_certificate (SSL              *ssl,
                     GTlsCertificate  *cert,
                     GError          **error)
{
  EVP_PKEY *key;
  X509 *x;
  GTlsCertificate *issuer;
  char error_buffer[256];

  key = g_tls_certificate_openssl_get_key (G_TLS_CERTIFICATE_OPENSSL (cert));

  if (!key)
    {
      g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                           _("Certificate has no private key"));
      return FALSE;
    }

  /* Note, order is important. If a certificate has been set previously,
   * OpenSSL requires that the new certificate is set _before_ the new
   * private key is set. */
  x = g_tls_certificate_openssl_get_cert (G_TLS_CERTIFICATE_OPENSSL (cert));
  if (SSL_use_certificate (ssl, x) <= 0)
    {
      ERR_error_string_n (ERR_get_error (), error_buffer, sizeof (error_buffer));
      g_set_error (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                   _("There is a problem with the certificate: %s"),
                   error_buffer);
      return FALSE;
    }

  if (SSL_use_PrivateKey (ssl, key) <= 0)
    {
      ERR_error_string_n (ERR_get_error (), error_buffer, sizeof (error_buffer));
      g_set_error (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                   _("There is a problem with the certificate private key: %s"),
                   error_buffer);
      return FALSE;
    }

  if (SSL_clear_chain_certs (ssl) == 0)
    {
      ERR_error_string_n (ERR_get_error (), error_buffer, sizeof (error_buffer));
      g_warning ("There was a problem clearing the chain certificates: %s",
                 error_buffer);
    }

  /* Add all the issuers to create the full certificate chain */
  for (issuer = g_tls_certificate_get_issuer (G_TLS_CERTIFICATE (cert));
       issuer;
       issuer = g_tls_certificate_get_issuer (issuer))
    {
      X509 *issuer_x;

      issuer_x = g_tls_certificate_openssl_get_cert (G_TLS_CERTIFICATE_OPENSSL (issuer));

      /* Be careful here and duplicate the certificate since the ssl object
       * will take the ownership
       */
      if (SSL_add1_chain_cert (ssl, issuer_x) == 0)
        {
          ERR_error_string_n (ERR_get_error (), error_buffer, sizeof (error_buffer));
          g_warning ("There was a problem adding the chain certificate: %s",
                     error_buffer);
        }
    }

  return TRUE;
}

static void
on_certificate_changed (GObject    *object,
                        GParamSpec *spec,
                        gpointer    user_data)
{
  SSL *ssl;
  GTlsCertificate *cert;

  ssl = g_tls_server_connection_openssl_get_ssl (G_TLS_CONNECTION_OPENSSL (object));
  cert = g_tls_connection_get_certificate (G_TLS_CONNECTION (object));

  if (ssl && cert)
    ssl_set_certificate (ssl, cert, NULL);
}

static void
g_tls_server_connection_openssl_class_init (GTlsServerConnectionOpensslClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GTlsConnectionBaseClass *base_class = G_TLS_CONNECTION_BASE_CLASS (klass);
  GTlsConnectionOpensslClass *connection_class = G_TLS_CONNECTION_OPENSSL_CLASS (klass);

  gobject_class->finalize = g_tls_server_connection_openssl_finalize;
  gobject_class->get_property = g_tls_server_connection_openssl_get_property;
  gobject_class->set_property = g_tls_server_connection_openssl_set_property;

  base_class->prepare_handshake = g_tls_server_connection_openssl_prepare_handshake;

  connection_class->get_ssl = g_tls_server_connection_openssl_get_ssl;

  g_object_class_override_property (gobject_class, PROP_AUTHENTICATION_MODE, "authentication-mode");
}

static void
g_tls_server_connection_openssl_init (GTlsServerConnectionOpenssl *openssl)
{
}

static void
g_tls_server_connection_openssl_server_connection_interface_init (GTlsServerConnectionInterface *iface)
{
}

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined (LIBRESSL_VERSION_NUMBER)
static void
ssl_info_callback (const SSL *ssl,
                   int        type,
                   int        val)
{
  if ((type & SSL_CB_HANDSHAKE_DONE) != 0)
    {
      /* Disable renegotiation (CVE-2009-3555) */
      ssl->s3->flags |= SSL3_FLAGS_NO_RENEGOTIATE_CIPHERS;
    }
}
#endif

static gboolean
set_cipher_list (GTlsServerConnectionOpenssl  *server,
                 GError                      **error)
{
  const gchar *cipher_list;

  cipher_list = g_getenv ("G_TLS_OPENSSL_CIPHER_LIST");
  if (cipher_list)
    {
      if (!SSL_CTX_set_cipher_list (server->ssl_ctx, cipher_list))
        {
          char error_buffer[256];
          ERR_error_string_n (ERR_get_error (), error_buffer, sizeof (error_buffer));
          g_set_error (error, G_TLS_ERROR, G_TLS_ERROR_MISC,
                       _("Could not set TLS cipher list: %s"),
                       error_buffer);
          return FALSE;
        }
    }

  return TRUE;
}

static gboolean
set_max_protocol (GTlsServerConnectionOpenssl  *server,
                  GError                      **error)
{
#ifdef SSL_CTX_set_max_proto_version
  const gchar *proto;

  proto = g_getenv ("G_TLS_OPENSSL_MAX_PROTO");
  if (proto)
    {
      gint64 version = g_ascii_strtoll (proto, NULL, 0);

      if (version > 0 && version < G_MAXINT)
        {
          if (!SSL_CTX_set_max_proto_version (server->ssl_ctx, (int)version))
            {
              char error_buffer[256];
              ERR_error_string_n (ERR_get_error (), error_buffer, sizeof (error_buffer));
              g_set_error (error, G_TLS_ERROR, G_TLS_ERROR_MISC,
                           _("Could not set MAX protocol to %d: %s"),
                           (int)version, error_buffer);
              return FALSE;
            }
        }
    }
#endif

  return TRUE;
}

#ifdef SSL_CTX_set1_sigalgs_list
static void
set_signature_algorithm_list (GTlsServerConnectionOpenssl *server)
{
  const gchar *signature_algorithm_list;

  signature_algorithm_list = g_getenv ("G_TLS_OPENSSL_SIGNATURE_ALGORITHM_LIST");
  if (!signature_algorithm_list)
    return;

  SSL_CTX_set1_sigalgs_list (server->ssl_ctx, signature_algorithm_list);
}
#endif

#ifdef SSL_CTX_set1_curves_list
static void
set_curve_list (GTlsServerConnectionOpenssl *server)
{
  const gchar *curve_list;

  curve_list = g_getenv ("G_TLS_OPENSSL_CURVE_LIST");
  if (!curve_list)
    return;

  SSL_CTX_set1_curves_list (server->ssl_ctx, curve_list);
}
#endif

static gboolean
g_tls_server_connection_openssl_initable_init (GInitable       *initable,
                                               GCancellable    *cancellable,
                                               GError         **error)
{
  GTlsServerConnectionOpenssl *server = G_TLS_SERVER_CONNECTION_OPENSSL (initable);
  GTlsCertificate *cert;
  long options;
  char error_buffer[256];

  server->session = SSL_SESSION_new ();

  server->ssl_ctx = SSL_CTX_new (g_tls_connection_base_is_dtls (G_TLS_CONNECTION_BASE (server))
#if OPENSSL_VERSION_NUMBER >= 0x10100000L || defined (LIBRESSL_VERSION_NUMBER)
                                 ? DTLS_server_method ()
                                 : TLS_server_method ());
#else
                                 ? DTLSv1_server_method ()
                                 : SSLv23_server_method ());
#endif
  if (!server->ssl_ctx)
    {
      ERR_error_string_n (ERR_get_error (), error_buffer, sizeof (error_buffer));
      g_set_error (error, G_TLS_ERROR, G_TLS_ERROR_MISC,
                   _("Could not create TLS context: %s"),
                   error_buffer);
      return FALSE;
    }

  if (!set_cipher_list (server, error))
    return FALSE;

  if (!set_max_protocol (server, error))
    return FALSE;

  /* Only TLS 1.2 or higher */
  options = SSL_OP_NO_COMPRESSION |
            SSL_OP_CIPHER_SERVER_PREFERENCE |
            SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION |
            SSL_OP_SINGLE_ECDH_USE |
#ifdef SSL_OP_NO_TLSv1_1
            SSL_OP_NO_TLSv1_1 |
#endif
            SSL_OP_NO_SSLv2 |
            SSL_OP_NO_SSLv3 |
            SSL_OP_NO_TLSv1;

#ifdef SSL_OP_NO_RENEGOTIATION
  options |= SSL_OP_NO_RENEGOTIATION;
#endif

  SSL_CTX_set_options (server->ssl_ctx, options);

  SSL_CTX_add_session (server->ssl_ctx, server->session);

#ifdef SSL_CTX_set1_sigalgs_list
  set_signature_algorithm_list (server);
#endif

#ifdef SSL_CTX_set1_curves_list
  set_curve_list (server);
#endif

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined (LIBRESSL_VERSION_NUMBER)
# ifdef SSL_CTX_set_ecdh_auto
  SSL_CTX_set_ecdh_auto (server->ssl_ctx, 1);
# else
  {
    EC_KEY *ecdh;

    ecdh = EC_KEY_new_by_curve_name (NID_X9_62_prime256v1);
    if (ecdh)
      {
        SSL_CTX_set_tmp_ecdh (server->ssl_ctx, ecdh);
        EC_KEY_free (ecdh);
      }
  }
# endif

  SSL_CTX_set_info_callback (server->ssl_ctx, ssl_info_callback);
#endif

  cert = g_tls_connection_get_certificate (G_TLS_CONNECTION (initable));

  server->ssl = SSL_new (server->ssl_ctx);
  if (!server->ssl)
    {
      ERR_error_string_n (ERR_get_error (), error_buffer, sizeof (error_buffer));
      g_set_error (error, G_TLS_ERROR, G_TLS_ERROR_MISC,
                   _("Could not create TLS connection: %s"),
                   error_buffer);
      return FALSE;
    }

  if (cert && !ssl_set_certificate (server->ssl, cert, error))
    return FALSE;

  SSL_set_accept_state (server->ssl);

  if (!g_tls_server_connection_openssl_parent_initable_iface->
      init (initable, cancellable, error))
    return FALSE;

  g_signal_connect (server, "notify::certificate", G_CALLBACK (on_certificate_changed), NULL);

  return TRUE;
}

static void
g_tls_server_connection_openssl_initable_interface_init (GInitableIface  *iface)
{
  g_tls_server_connection_openssl_parent_initable_iface = g_type_interface_peek_parent (iface);

  iface->init = g_tls_server_connection_openssl_initable_init;
}
