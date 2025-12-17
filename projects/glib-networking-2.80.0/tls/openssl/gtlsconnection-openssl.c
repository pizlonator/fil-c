/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gtlsconnection-openssl.c
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

#include <errno.h>
#include <stdarg.h>
#include "openssl-include.h"

#include "gtlsconnection-openssl.h"
#include "gtlsbackend-openssl.h"
#include "gtlscertificate-openssl.h"
#include "gtlsdatabase-openssl.h"
#include "gtlsbio.h"
#include "gtlslog.h"

#include <glib/gi18n-lib.h>

#define DTLS_MESSAGE_MAX_SIZE 65536

typedef struct _GTlsConnectionOpensslPrivate
{
  BIO *bio;
  guint8 *dtls_rx;
  guint8 *dtls_tx;
  GMutex ssl_mutex;

  gboolean shutting_down;
} GTlsConnectionOpensslPrivate;

typedef int (*GTlsOpensslIOFunc) (SSL *ssl, gpointer user_data);

typedef struct _ReadRequest
{
  void *buffer;
  gsize count;
} ReadRequest;

typedef struct _WriteRequest
{
  const void *buffer;
  gsize count;
} WriteRequest;

static void g_tls_connection_openssl_initable_iface_init (GInitableIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GTlsConnectionOpenssl, g_tls_connection_openssl, G_TYPE_TLS_CONNECTION_BASE,
                                  G_ADD_PRIVATE (GTlsConnectionOpenssl)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                         g_tls_connection_openssl_initable_iface_init))

static void
g_tls_connection_openssl_finalize (GObject *object)
{
  GTlsConnectionOpenssl *openssl = G_TLS_CONNECTION_OPENSSL (object);
  GTlsConnectionOpensslPrivate *priv;

  priv = g_tls_connection_openssl_get_instance_private (openssl);

  g_free (priv->dtls_rx);
  g_free (priv->dtls_tx);
  g_mutex_clear (&priv->ssl_mutex);

  G_OBJECT_CLASS (g_tls_connection_openssl_parent_class)->finalize (object);
}

static GTlsSafeRenegotiationStatus
g_tls_connection_openssl_handshake_thread_safe_renegotiation_status (GTlsConnectionBase *tls)
{
  GTlsConnectionOpenssl *openssl = G_TLS_CONNECTION_OPENSSL (tls);
  SSL *ssl;

  ssl = g_tls_connection_openssl_get_ssl (openssl);

  return SSL_get_secure_renegotiation_support (ssl) ? G_TLS_SAFE_RENEGOTIATION_SUPPORTED_BY_PEER
                                                    : G_TLS_SAFE_RENEGOTIATION_UNSUPPORTED;
}

static GTlsConnectionBaseStatus
end_openssl_io (GTlsConnectionOpenssl  *openssl,
                GIOCondition            direction,
                int                     ret,
                gboolean                blocking,
                GError                **error,
                const char             *err_prefix,
                const char             *err_str)
{
  GTlsConnectionBase *tls = G_TLS_CONNECTION_BASE (openssl);
  GTlsConnectionOpensslPrivate *priv;
  int err_code, err, err_lib, reason;
  GError *my_error = NULL;
  GTlsConnectionBaseStatus status;
  SSL *ssl;

  priv = g_tls_connection_openssl_get_instance_private (openssl);

  ssl = g_tls_connection_openssl_get_ssl (openssl);

  err_code = SSL_get_error (ssl, ret);

  status = g_tls_connection_base_pop_io (tls, direction, ret > 0, &my_error);

  if ((err_code == SSL_ERROR_WANT_READ ||
       err_code == SSL_ERROR_WANT_WRITE) &&
      blocking)
    {
      if (my_error)
        g_error_free (my_error);
      return G_TLS_CONNECTION_BASE_TRY_AGAIN;
    }

  if (err_code == SSL_ERROR_ZERO_RETURN)
    return G_TLS_CONNECTION_BASE_OK;

  if (status == G_TLS_CONNECTION_BASE_OK ||
      status == G_TLS_CONNECTION_BASE_WOULD_BLOCK ||
      status == G_TLS_CONNECTION_BASE_TIMED_OUT)
    {
      if (my_error)
        g_propagate_error (error, my_error);
      return status;
    }

  /* This case is documented that it may happen and that is perfectly fine */
  if (err_code == SSL_ERROR_SYSCALL &&
      (priv->shutting_down && (!my_error || g_error_matches (my_error, G_IO_ERROR, G_IO_ERROR_BROKEN_PIPE))))
    {
      g_clear_error (&my_error);
      return G_TLS_CONNECTION_BASE_OK;
    }

  err = ERR_get_error ();
  err_lib = ERR_GET_LIB (err);
  reason = ERR_GET_REASON (err);

  if (g_tls_connection_base_is_handshaking (tls) && !g_tls_connection_base_ever_handshaked (tls))
    {
      if (reason == SSL_R_SSLV3_ALERT_HANDSHAKE_FAILURE && my_error)
        {
          g_propagate_error (error, my_error);
          return G_TLS_CONNECTION_BASE_ERROR;
        }
      else if (reason == SSL_R_BAD_PACKET_LENGTH ||
               reason == SSL_R_UNKNOWN_ALERT_TYPE ||
               reason == SSL_R_DECRYPTION_FAILED ||
               reason == SSL_R_DECRYPTION_FAILED_OR_BAD_RECORD_MAC ||
               reason == SSL_R_BAD_PROTOCOL_VERSION_NUMBER ||
               reason == SSL_R_SSLV3_ALERT_HANDSHAKE_FAILURE ||
               reason == SSL_R_UNKNOWN_PROTOCOL)
        {
          g_clear_error (&my_error);
          g_set_error (error, G_TLS_ERROR, G_TLS_ERROR_NOT_TLS,
                       _("Peer failed to perform TLS handshake: %s"), ERR_reason_error_string (err));
          return G_TLS_CONNECTION_BASE_ERROR;
        }
    }

#ifdef SSL_R_SHUTDOWN_WHILE_IN_INIT
  /* XXX: this error happens on ubuntu when shutting down the connection, it
   * seems to be a bug in a specific version of openssl, so let's handle it
   * gracefully
   */
  if (reason == SSL_R_SHUTDOWN_WHILE_IN_INIT)
    {
      g_clear_error (&my_error);
      return G_TLS_CONNECTION_BASE_OK;
    }
#endif

  if (reason == SSL_R_PEER_DID_NOT_RETURN_A_CERTIFICATE
#ifdef SSL_R_TLSV13_ALERT_CERTIFICATE_REQUIRED
      || reason == SSL_R_TLSV13_ALERT_CERTIFICATE_REQUIRED
#endif
     )
    {
      g_clear_error (&my_error);
      g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_CERTIFICATE_REQUIRED,
                           _("TLS connection peer did not send a certificate"));
      return status;
    }

  if (reason == SSL_R_CERTIFICATE_VERIFY_FAILED)
    {
      g_clear_error (&my_error);
      g_set_error (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                   _("Unacceptable TLS certificate"));
      return G_TLS_CONNECTION_BASE_ERROR;
    }

  if (reason == SSL_R_TLSV1_ALERT_UNKNOWN_CA)
    {
      g_clear_error (&my_error);
      g_set_error (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                   _("Unacceptable TLS certificate authority"));
      return G_TLS_CONNECTION_BASE_ERROR;
    }

  if (err_lib == ERR_LIB_RSA && reason == RSA_R_DIGEST_TOO_BIG_FOR_RSA_KEY)
    {
      g_clear_error (&my_error);
      g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                           _("Digest too big for RSA key"));
      return G_TLS_CONNECTION_BASE_ERROR;
    }

#ifdef SSL_R_NO_RENEGOTIATION
  if (reason == SSL_R_NO_RENEGOTIATION)
    {
      g_clear_error (&my_error);
      g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_MISC,
                           _("Secure renegotiation is disabled"));
      return G_TLS_CONNECTION_BASE_REHANDSHAKE;
    }
#endif

#ifdef SSL_R_UNEXPECTED_EOF_WHILE_READING
  if (reason == SSL_R_UNEXPECTED_EOF_WHILE_READING)
    {
      if (g_tls_connection_get_require_close_notify (G_TLS_CONNECTION (openssl)))
        {
          g_clear_error (&my_error);
          g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_EOF,
                               _("TLS connection closed unexpectedly"));
          return G_TLS_CONNECTION_BASE_ERROR;
        }
      else
        return G_TLS_CONNECTION_BASE_OK;
    }
#endif

  if (my_error)
    g_propagate_error (error, my_error);

  if (ret == 0 && err == 0 && err_lib == 0 && err_code == SSL_ERROR_SYSCALL
      && (direction == G_IO_IN || direction == G_IO_OUT))
    {
      /* SSL_ERROR_SYSCALL usually means we have no bloody idea what has happened
       * but when ret for read or write is 0 and all others error codes as well
       * - this is normally Early EOF condition
       */
      if (!g_tls_connection_get_require_close_notify (G_TLS_CONNECTION (openssl)))
        return G_TLS_CONNECTION_BASE_OK;

      if (error && !*error)
        *error = g_error_new (G_TLS_ERROR, G_TLS_ERROR_EOF, _("%s: The connection is broken"), gettext (err_prefix));
    }
  else if (error && !*error)
    *error = g_error_new (G_TLS_ERROR, G_TLS_ERROR_MISC, "%s: %s", gettext (err_prefix), err_str);

  return G_TLS_CONNECTION_BASE_ERROR;
}

static GTlsConnectionBaseStatus
perform_openssl_io (GTlsConnectionOpenssl  *openssl,
                    GIOCondition            direction,
                    GTlsOpensslIOFunc       perform_func,
                    gpointer                perform_data,
                    gint64                  timeout,
                    GCancellable           *cancellable,
                    int                    *out_ret,
                    GError                **error,
                    const char             *err_prefix)
{
  GTlsConnectionBaseStatus status;
  GTlsConnectionBase *tls;
  GTlsConnectionOpensslPrivate *priv;
  SSL *ssl;
  gint64 deadline;
  int ret;

  tls = G_TLS_CONNECTION_BASE (openssl);
  priv = g_tls_connection_openssl_get_instance_private (openssl);
  ssl = g_tls_connection_openssl_get_ssl (openssl);

  if (timeout >= 0)
    deadline = g_get_monotonic_time () + timeout;
  else
    deadline = -1;

  while (TRUE)
    {
      GIOCondition io_needed;
      char error_str[256];
      struct timeval tv;
      gint64 io_timeout;

      g_tls_connection_base_push_io (tls, direction, 0, cancellable);

      if (g_tls_connection_base_is_dtls (tls))
        DTLSv1_handle_timeout (ssl);

      ret = perform_func (ssl, perform_data);

      switch (SSL_get_error (ssl, ret))
        {
          case SSL_ERROR_WANT_READ:
            io_needed = G_IO_IN;
            break;
          case SSL_ERROR_WANT_WRITE:
            io_needed = G_IO_OUT;
            break;
          default:
            io_needed = 0;
            break;
        }

      ERR_error_string_n (SSL_get_error (ssl, ret), error_str,
                          sizeof (error_str));
      status = end_openssl_io (openssl, direction, ret, TRUE, error, err_prefix,
                               error_str);

      if (status != G_TLS_CONNECTION_BASE_TRY_AGAIN)
        break;

      if (g_tls_connection_base_is_dtls (tls) && DTLSv1_get_timeout (ssl, &tv))
        io_timeout = (tv.tv_sec * G_USEC_PER_SEC) + tv.tv_usec;
      else
        io_timeout = -1;

      if (deadline != -1)
        {
          gint64 remaining = MAX (deadline - g_get_monotonic_time (), 0);

          if (io_timeout != -1)
            io_timeout = MIN (io_timeout, remaining);
          else
            io_timeout = remaining;
        }

      if (io_timeout == 0)
        break;

      g_tls_bio_wait_available (priv->bio, io_needed, io_timeout, cancellable);
    }

  if (status == G_TLS_CONNECTION_BASE_TRY_AGAIN)
    {
      if (timeout == 0)
        {
          status = G_TLS_CONNECTION_BASE_WOULD_BLOCK;
          g_clear_error (error);
          g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK,
                               "Operation would block");
        }
      else if (timeout > 0)
        {
          status = G_TLS_CONNECTION_BASE_TIMED_OUT;
          g_clear_error (error);
          g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT,
                               _("Socket I/O timed out"));
        }
    }

  if (out_ret)
    *out_ret = ret;

  return status;
}

static int
_openssl_alpn_select_cb (SSL                  *ssl,
                         const unsigned char **out,
                         unsigned char        *outlen,
                         const unsigned char  *in,
                         unsigned int          inlen,
                         void                 *arg)
{
  GTlsConnectionBase *tls = arg;
  int ret = SSL_TLSEXT_ERR_NOACK;
  gchar **advertised_protocols = NULL;
  gchar *logbuf;

  logbuf = g_strndup ((const gchar *)in, inlen);
  g_tls_log_debug (tls, "ALPN their protocols: %s", logbuf);
  g_free (logbuf);

  g_object_get (G_OBJECT (tls),
                "advertised-protocols", &advertised_protocols,
                NULL);

  if (!advertised_protocols)
    return ret;

  if (g_strv_length (advertised_protocols) > 0)
    {
      GByteArray *protocols = g_byte_array_new ();
      int i;
      guint8 slen = 0;
      guint8 *spd = NULL;

      for (i = 0; advertised_protocols[i]; i++)
        {
          guint8 len = strlen (advertised_protocols[i]);
          g_byte_array_append (protocols, &len, 1);
          g_byte_array_append (protocols,
                               (guint8 *)advertised_protocols[i],
                               len);
        }
      logbuf = g_strndup ((const gchar *)protocols->data, protocols->len);
      g_tls_log_debug (tls, "ALPN our protocols: %s", logbuf);
      g_free (logbuf);

      /* pointer to memory inside in[0..inlen] is returned on success
       * pointer to protocols->data is returned on failure */
      ret = SSL_select_next_proto (&spd, &slen,
                                   in, inlen,
                                   protocols->data, protocols->len);
      if (ret == OPENSSL_NPN_NEGOTIATED)
        {
          logbuf = g_strndup ((const gchar *)spd, slen);
          g_tls_log_debug (tls, "ALPN selected protocol %s", logbuf);
          g_free (logbuf);

          ret = SSL_TLSEXT_ERR_OK;
          *out = spd;
          *outlen = slen;
        }
      else
        {
          g_tls_log_debug (tls, "ALPN no matching protocol");
          ret = SSL_TLSEXT_ERR_NOACK;
        }

      g_byte_array_unref (protocols);
    }

  g_strfreev (advertised_protocols);
  return ret;
}

static void
g_tls_connection_openssl_prepare_handshake (GTlsConnectionBase  *tls,
                                            gchar              **advertised_protocols)
{
  SSL *ssl;

  if (!advertised_protocols)
    return;

  ssl = g_tls_connection_openssl_get_ssl (G_TLS_CONNECTION_OPENSSL (tls));

  if (G_IS_TLS_SERVER_CONNECTION (tls))
    {
      SSL_CTX *ctx = SSL_get_SSL_CTX(ssl);

      g_tls_log_debug (tls, "Setting ALPN Callback on %p", ctx);
      SSL_CTX_set_alpn_select_cb (ctx, _openssl_alpn_select_cb, tls);

      return;
    }

  if (g_strv_length (advertised_protocols) > 0)
    {
      GByteArray *protocols = g_byte_array_new ();
      int ret, i;

      for (i = 0; advertised_protocols[i]; i++)
        {
          guint8 len = strlen (advertised_protocols[i]);
          g_byte_array_append (protocols, &len, 1);
          g_byte_array_append (protocols, (guint8 *)advertised_protocols[i], len);
        }
      ret = SSL_set_alpn_protos (ssl, protocols->data, protocols->len);
      if (ret)
        g_tls_log_debug (tls, "Error setting ALPN protocols: %d", ret);
      else
        {
          gchar *logbuf = g_strndup ((const gchar *)protocols->data, protocols->len);

          g_tls_log_debug (tls, "Setting ALPN protocols to %s", logbuf);
          g_free (logbuf);
        }
      g_byte_array_unref (protocols);
    }
}

static GTlsCertificateFlags
g_tls_connection_openssl_verify_chain (GTlsConnectionBase       *tls,
                                       GTlsCertificate          *chain,
                                       const gchar              *purpose,
                                       GSocketConnectable       *identity,
                                       GTlsInteraction          *interaction,
                                       GTlsDatabaseVerifyFlags   flags,
                                       GCancellable             *cancellable,
                                       GError                  **error)
{
  GTlsDatabase *database;
  GTlsCertificateFlags errors = 0;
  gboolean is_client = G_IS_TLS_CLIENT_CONNECTION (tls);

  database = g_tls_connection_get_database (G_TLS_CONNECTION (tls));
  if (database)
    {
      errors |= g_tls_database_verify_chain (database,
                                             chain,
                                             is_client ? G_TLS_DATABASE_PURPOSE_AUTHENTICATE_SERVER : G_TLS_DATABASE_PURPOSE_AUTHENTICATE_CLIENT,
                                             identity,
                                             g_tls_connection_get_interaction (G_TLS_CONNECTION (tls)),
                                             G_TLS_DATABASE_VERIFY_NONE,
                                             NULL,
                                             error);
    }
  else
    {
      errors |= G_TLS_CERTIFICATE_UNKNOWN_CA;
      errors |= g_tls_certificate_verify (chain, identity, NULL);
    }

  return errors;
}

GTlsProtocolVersion
glib_protocol_version_from_openssl (int protocol_version)
{
  switch (protocol_version)
    {
    case SSL3_VERSION:
      return G_TLS_PROTOCOL_VERSION_SSL_3_0;
    case TLS1_VERSION:
      return G_TLS_PROTOCOL_VERSION_TLS_1_0;
    case TLS1_1_VERSION:
      return G_TLS_PROTOCOL_VERSION_TLS_1_1;
    case TLS1_2_VERSION:
      return G_TLS_PROTOCOL_VERSION_TLS_1_2;
    case TLS1_3_VERSION:
      return G_TLS_PROTOCOL_VERSION_TLS_1_3;
    case DTLS1_VERSION:
      return G_TLS_PROTOCOL_VERSION_DTLS_1_0;
    case DTLS1_2_VERSION:
      return G_TLS_PROTOCOL_VERSION_DTLS_1_2;
    default:
      return G_TLS_PROTOCOL_VERSION_UNKNOWN;
    }
}

static void
g_tls_connection_openssl_complete_handshake (GTlsConnectionBase   *tls,
                                             gboolean              handshake_succeeded,
                                             gchar               **negotiated_protocol,
                                             GTlsProtocolVersion  *protocol_version,
                                             gchar               **ciphersuite_name,
                                             GError              **error)
{
  SSL *ssl;
  SSL_SESSION *session;
  unsigned int len = 0;
  const unsigned char *data = NULL;

  if (!handshake_succeeded)
    return;

  ssl = g_tls_connection_openssl_get_ssl (G_TLS_CONNECTION_OPENSSL (tls));
  session = SSL_get_session (ssl);

  SSL_get0_alpn_selected (ssl, &data, &len);

  g_tls_log_debug (tls, "negotiated ALPN protocols: [%d]%p", len, data);

  if (data && len > 0)
    {
      g_assert (!*negotiated_protocol);
      *negotiated_protocol = g_strndup ((gchar *)data, len);
    }

  *protocol_version = glib_protocol_version_from_openssl (SSL_SESSION_get_protocol_version (session));
  *ciphersuite_name = g_strdup (SSL_get_cipher_name (ssl));
}

static int
perform_rehandshake (SSL      *ssl,
                     gpointer  user_data)
{
  GTlsConnectionBase *tls = user_data;
  int ret = 1; /* always look on the bright side of life */

#if OPENSSL_VERSION_NUMBER >= 0x10101000L
  if (SSL_version(ssl) >= TLS1_3_VERSION)
    ret = SSL_key_update (ssl, SSL_KEY_UPDATE_REQUESTED);
  else if (SSL_get_secure_renegotiation_support (ssl) && !(SSL_get_options(ssl) & SSL_OP_NO_RENEGOTIATION))
    /* remote and local peers both can rehandshake */
    ret = SSL_renegotiate (ssl);
  else
    g_tls_log_debug (tls, "Secure renegotiation is not supported");
#else
  ret = SSL_renegotiate (ssl);
#endif

  return ret;
}

static GTlsConnectionBaseStatus
g_tls_connection_openssl_handshake_thread_request_rehandshake (GTlsConnectionBase  *tls,
                                                               gint64               timeout,
                                                               GCancellable        *cancellable,
                                                               GError             **error)
{
  /* On a client-side connection, SSL_renegotiate() itself will start
   * a rehandshake, so we only need to do something special here for
   * server-side connections.
   */
  if (!G_IS_TLS_SERVER_CONNECTION (tls))
    return G_TLS_CONNECTION_BASE_OK;

  return perform_openssl_io (G_TLS_CONNECTION_OPENSSL (tls), G_IO_IN | G_IO_OUT,
                             perform_rehandshake, tls, timeout, cancellable,
                             NULL, error, N_("Error performing TLS handshake"));
}

static GTlsCertificate *
g_tls_connection_openssl_retrieve_peer_certificate (GTlsConnectionBase *tls)
{
  X509 *peer;
  STACK_OF (X509) *certs;
  GTlsCertificateOpenssl *chain;
  SSL *ssl;

  ssl = g_tls_connection_openssl_get_ssl (G_TLS_CONNECTION_OPENSSL (tls));

  peer = SSL_get_peer_certificate (ssl);
  if (!peer)
    return NULL;

  certs = SSL_get_peer_cert_chain (ssl);
  if (!certs)
    {
      X509_free (peer);
      return NULL;
    }

  chain = g_tls_certificate_openssl_build_chain (peer, certs);
  X509_free (peer);
  if (!chain)
    return NULL;

  return G_TLS_CERTIFICATE (chain);
}

static gboolean
openssl_get_binding_tls_unique (GTlsConnectionOpenssl  *tls,
                                GByteArray             *data,
                                GError                **error)
{
  SSL *ssl = g_tls_connection_openssl_get_ssl (tls);
  gboolean is_client = G_IS_TLS_CLIENT_CONNECTION (tls);
  gboolean resumed = SSL_session_reused (ssl);
  size_t len = 64;

#if OPENSSL_VERSION_NUMBER >= 0x10101000L
  if (SSL_version (ssl) >= TLS1_3_VERSION)
    {
      g_set_error (error, G_TLS_CHANNEL_BINDING_ERROR, G_TLS_CHANNEL_BINDING_ERROR_GENERAL_ERROR,
                   _("The request is invalid."));
      return FALSE;
    }
#endif

  /* This is a drill */
  if (!data)
    return TRUE;

  do {
    g_byte_array_set_size (data, len);
    if ((resumed && is_client) || (!resumed && !is_client))
      len = SSL_get_peer_finished (ssl, data->data, data->len);
    else
      len = SSL_get_finished (ssl, data->data, data->len);
  } while (len > data->len);

  if (len > 0)
    {
      g_byte_array_set_size (data, len);
      return TRUE;
    }
  g_set_error (error, G_TLS_CHANNEL_BINDING_ERROR, G_TLS_CHANNEL_BINDING_ERROR_NOT_AVAILABLE,
               _("Channel binding data tls-unique is not available"));
  return FALSE;
}

static gboolean
openssl_get_binding_tls_server_end_point (GTlsConnectionOpenssl  *tls,
                                          GByteArray             *data,
                                          GError                **error)
{
  SSL *ssl = g_tls_connection_openssl_get_ssl (tls);
  gboolean is_client = G_IS_TLS_CLIENT_CONNECTION (tls);
  int algo_nid;
  const EVP_MD *algo = NULL;
  X509 *crt;

  if (is_client)
    crt = SSL_get_peer_certificate (ssl);
  else
    crt = SSL_get_certificate (ssl);

  if (!crt)
    {
      g_set_error (error, G_TLS_CHANNEL_BINDING_ERROR, G_TLS_CHANNEL_BINDING_ERROR_NOT_AVAILABLE,
                   _("X.509 Certificate is not available on the connection"));
      return FALSE;
    }

  if (!OBJ_find_sigid_algs (X509_get_signature_nid (crt), &algo_nid, NULL))
    {
      X509_free (crt);
      g_set_error (error, G_TLS_CHANNEL_BINDING_ERROR, G_TLS_CHANNEL_BINDING_ERROR_GENERAL_ERROR,
                   _("Unable to obtain certificate signature algorithm"));
      return FALSE;
    }

  /* This is a drill */
  if (!data)
    {
      if (is_client)
        X509_free (crt);
      return TRUE;
    }

  switch (algo_nid)
    {
    case NID_md5:
    case NID_sha1:
      algo_nid = NID_sha256;
      break;
    case NID_md5_sha1:
      g_set_error (error, G_TLS_CHANNEL_BINDING_ERROR, G_TLS_CHANNEL_BINDING_ERROR_NOT_SUPPORTED,
                   _("Current X.509 certificate uses unknown or unsupported signature algorithm"));
      if (is_client)
        X509_free (crt);
      return FALSE;
    }

  g_byte_array_set_size (data, EVP_MAX_MD_SIZE);
  algo = EVP_get_digestbynid (algo_nid);
  if (X509_digest (crt, algo, data->data, &(data->len)))
    {
      if (is_client)
        X509_free (crt);
      return TRUE;
    }

  if (is_client)
    X509_free (crt);
  g_set_error (error, G_TLS_CHANNEL_BINDING_ERROR, G_TLS_CHANNEL_BINDING_ERROR_GENERAL_ERROR,
               _("Failed to generate X.509 certificate digest"));
  return FALSE;
}

#define RFC5705_LABEL_DATA "EXPORTER-Channel-Binding"
#define RFC5705_LABEL_LEN 24
static gboolean
openssl_get_binding_tls_exporter (GTlsConnectionOpenssl  *tls,
                                  GByteArray             *data,
                                  GError                **error)
{
  SSL *ssl = g_tls_connection_openssl_get_ssl (tls);
  size_t  ctx_len = 0;
  guint8 *context = (guint8 *)"";
  int ret;

  if (!data)
    return TRUE;

  g_byte_array_set_size (data, 32);
  ret = SSL_export_keying_material (ssl,
                                    data->data, data->len,
                                    RFC5705_LABEL_DATA, RFC5705_LABEL_LEN,
                                    context, ctx_len,
                                    1 /* use context */);

  if (ret > 0)
    return TRUE;

  if (ret < 0)
    g_set_error (error, G_TLS_CHANNEL_BINDING_ERROR, G_TLS_CHANNEL_BINDING_ERROR_NOT_SUPPORTED,
                 _("TLS Connection does not support TLS-Exporter feature"));
  else
    g_set_error (error, G_TLS_CHANNEL_BINDING_ERROR, G_TLS_CHANNEL_BINDING_ERROR_GENERAL_ERROR,
                 _("Unexpected error while exporting keying data"));

  return FALSE;
}

static gboolean
g_tls_connection_openssl_get_channel_binding_data (GTlsConnectionBase      *tls,
                                                   GTlsChannelBindingType   type,
                                                   GByteArray              *data,
                                                   GError                 **error)
{
  GTlsConnectionOpenssl *openssl = G_TLS_CONNECTION_OPENSSL (tls);

  switch (type)
    {
    case G_TLS_CHANNEL_BINDING_TLS_UNIQUE:
      return openssl_get_binding_tls_unique (openssl, data, error);
    case G_TLS_CHANNEL_BINDING_TLS_SERVER_END_POINT:
      return openssl_get_binding_tls_server_end_point (openssl, data, error);
    case G_TLS_CHANNEL_BINDING_TLS_EXPORTER:
      return openssl_get_binding_tls_exporter (openssl, data, error);
    default:
      /* Anyone to implement tls-unique-for-telnet? */
      g_set_error (error, G_TLS_CHANNEL_BINDING_ERROR, G_TLS_CHANNEL_BINDING_ERROR_NOT_IMPLEMENTED,
                   _("Requested channel binding type is not implemented"));
    }
  return FALSE;
}

static GTlsConnectionBaseStatus
g_tls_connection_openssl_handshake_thread_handshake (GTlsConnectionBase  *tls,
                                                     gint64               timeout,
                                                     GCancellable        *cancellable,
                                                     GError             **error)
{
  GTlsConnectionBaseStatus status;
  int ret;

  status = perform_openssl_io (G_TLS_CONNECTION_OPENSSL (tls),
                               G_IO_IN | G_IO_OUT,
                               (GTlsOpensslIOFunc) SSL_do_handshake,
                               NULL, timeout, cancellable, &ret, error,
                               N_("Error reading data from TLS socket"));

  if (ret > 0)
    {
      if (!g_tls_connection_base_handshake_thread_verify_certificate (tls))
        {
          g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                               _("Unacceptable TLS certificate"));
          return G_TLS_CONNECTION_BASE_ERROR;
        }
    }

  return status;
}

static void
g_tls_connection_openssl_push_io (GTlsConnectionBase *tls,
                                  GIOCondition        direction,
                                  gint64              timeout,
                                  GCancellable       *cancellable)
{
  GTlsConnectionOpenssl *openssl = G_TLS_CONNECTION_OPENSSL (tls);
  GTlsConnectionOpensslPrivate *priv;
  GError **error;

  priv = g_tls_connection_openssl_get_instance_private (openssl);

  G_TLS_CONNECTION_BASE_CLASS (g_tls_connection_openssl_parent_class)->push_io (tls, direction,
                                                                                timeout, cancellable);

  if (direction & G_IO_IN)
    {
      error = g_tls_connection_base_get_read_error (tls);
      g_tls_bio_set_read_cancellable (priv->bio, cancellable);
      g_clear_error (error);
      g_tls_bio_set_read_error (priv->bio, error);
    }

  if (direction & G_IO_OUT)
    {
      error = g_tls_connection_base_get_write_error (tls);
      g_tls_bio_set_write_cancellable (priv->bio, cancellable);
      g_clear_error (error);
      g_tls_bio_set_write_error (priv->bio, error);
    }

  g_mutex_lock (&priv->ssl_mutex);
}

static GTlsConnectionBaseStatus
g_tls_connection_openssl_pop_io (GTlsConnectionBase  *tls,
                                 GIOCondition         direction,
                                 gboolean             success,
                                 GError             **error)
{
  GTlsConnectionOpenssl *openssl = G_TLS_CONNECTION_OPENSSL (tls);
  GTlsConnectionOpensslPrivate *priv;

  priv = g_tls_connection_openssl_get_instance_private (openssl);

  g_mutex_unlock (&priv->ssl_mutex);

  if (direction & G_IO_IN)
    g_tls_bio_set_read_cancellable (priv->bio, NULL);

  if (direction & G_IO_OUT)
    g_tls_bio_set_write_cancellable (priv->bio, NULL);

  return G_TLS_CONNECTION_BASE_CLASS (g_tls_connection_openssl_parent_class)->pop_io (tls, direction,
                                                                                      success, error);
}

static int
perform_read (SSL      *ssl,
              gpointer  user_data)
{
  ReadRequest *req = user_data;

  return SSL_read (ssl, req->buffer, req->count);
}

static GTlsConnectionBaseStatus
g_tls_connection_openssl_read (GTlsConnectionBase    *tls,
                               void                  *buffer,
                               gsize                  count,
                               gint64                 timeout,
                               gssize                *nread,
                               GCancellable          *cancellable,
                               GError               **error)
{
  GTlsConnectionBaseStatus status;
  ReadRequest req = { buffer, count };
  int ret;

  status = perform_openssl_io (G_TLS_CONNECTION_OPENSSL (tls), G_IO_IN,
                               perform_read, &req, timeout, cancellable, &ret,
                               error, N_("Error reading data from TLS socket"));

  *nread = MAX (ret, 0);
  return status;
}

static GTlsConnectionBaseStatus
g_tls_connection_openssl_read_message (GTlsConnectionBase  *tls,
                                       GInputVector        *vectors,
                                       guint                num_vectors,
                                       gint64               timeout,
                                       gssize              *nread,
                                       GCancellable        *cancellable,
                                       GError             **error)
{
  GTlsConnectionOpenssl *openssl = G_TLS_CONNECTION_OPENSSL (tls);
  GTlsConnectionOpensslPrivate *priv;
  GTlsConnectionBaseStatus status;
  gssize bytes_read;
  gsize bytes_copied, bytes_remaining;
  guint i;

  *nread = 0;

  priv = g_tls_connection_openssl_get_instance_private (openssl);

  if (!priv->dtls_rx)
    priv->dtls_rx = g_malloc (DTLS_MESSAGE_MAX_SIZE);

  status = g_tls_connection_openssl_read (tls, priv->dtls_rx,
                                          DTLS_MESSAGE_MAX_SIZE, timeout,
                                          &bytes_read, cancellable, error);
  if (status != G_TLS_CONNECTION_BASE_OK)
    return status;

  bytes_copied = 0;
  bytes_remaining = bytes_read;
  for (i = 0; i < num_vectors && bytes_remaining > 0; i++)
    {
      GInputVector *vector = &vectors[i];
      gsize n;

      n = MIN (bytes_remaining, vector->size);

      memcpy (vector->buffer, priv->dtls_rx + bytes_copied, n);

      bytes_copied += n;
      bytes_remaining -= n;
    }

  *nread = bytes_copied;

  return status;
}

static int
perform_write (SSL      *ssl,
               gpointer  user_data)
{
  WriteRequest *req = user_data;

  return SSL_write (ssl, req->buffer, req->count);
}

static GTlsConnectionBaseStatus
g_tls_connection_openssl_write (GTlsConnectionBase    *tls,
                                const void            *buffer,
                                gsize                  count,
                                gint64                 timeout,
                                gssize                *nwrote,
                                GCancellable          *cancellable,
                                GError               **error)
{
  GTlsConnectionBaseStatus status;
  WriteRequest req = { buffer, count };
  int ret;

  status = perform_openssl_io (G_TLS_CONNECTION_OPENSSL (tls), G_IO_OUT,
                               perform_write, &req, timeout, cancellable, &ret,
                               error, N_("Error writing data to TLS socket"));

  *nwrote = MAX (ret, 0);
  return status;
}

static GTlsConnectionBaseStatus
g_tls_connection_openssl_write_message (GTlsConnectionBase  *tls,
                                        GOutputVector       *vectors,
                                        guint                num_vectors,
                                        gint64               timeout,
                                        gssize              *nwrote,
                                        GCancellable        *cancellable,
                                        GError             **error)
{
  GTlsConnectionOpenssl *openssl = G_TLS_CONNECTION_OPENSSL (tls);
  GTlsConnectionOpensslPrivate *priv;
  gsize bytes_copied, bytes_available;
  guint i;

  priv = g_tls_connection_openssl_get_instance_private (openssl);

  if (!priv->dtls_tx)
    priv->dtls_tx = g_malloc (DTLS_MESSAGE_MAX_SIZE);

  bytes_copied = 0;
  bytes_available = DTLS_MESSAGE_MAX_SIZE;
  for (i = 0; i < num_vectors && bytes_available > 0; i++)
    {
      GOutputVector *vector = &vectors[i];
      gsize n;

      n = MIN (vector->size, bytes_available);

      memcpy (priv->dtls_tx + bytes_copied, vector->buffer, n);

      bytes_copied += n;
      bytes_available -= n;
    }

  return g_tls_connection_openssl_write (tls, priv->dtls_tx, bytes_copied,
                                         timeout, nwrote, cancellable, error);
}

static GTlsConnectionBaseStatus
g_tls_connection_openssl_close (GTlsConnectionBase  *tls,
                                gint64               timeout,
                                GCancellable        *cancellable,
                                GError             **error)
{
  GTlsConnectionOpenssl *openssl = G_TLS_CONNECTION_OPENSSL (tls);
  GTlsConnectionOpensslPrivate *priv;

  priv = g_tls_connection_openssl_get_instance_private (openssl);

  priv->shutting_down = TRUE;

  return perform_openssl_io (G_TLS_CONNECTION_OPENSSL (tls),
                             G_IO_IN | G_IO_OUT,
                             (GTlsOpensslIOFunc) SSL_shutdown,
                             NULL, timeout, cancellable, NULL, error,
                             N_("Error performing TLS close"));
}

static void
g_tls_connection_openssl_class_init (GTlsConnectionOpensslClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GTlsConnectionBaseClass *base_class = G_TLS_CONNECTION_BASE_CLASS (klass);

  object_class->finalize                                 = g_tls_connection_openssl_finalize;

  base_class->prepare_handshake                          = g_tls_connection_openssl_prepare_handshake;
  base_class->verify_chain                               = g_tls_connection_openssl_verify_chain;
  base_class->complete_handshake                         = g_tls_connection_openssl_complete_handshake;
  base_class->handshake_thread_safe_renegotiation_status = g_tls_connection_openssl_handshake_thread_safe_renegotiation_status;
  base_class->handshake_thread_request_rehandshake       = g_tls_connection_openssl_handshake_thread_request_rehandshake;
  base_class->handshake_thread_handshake                 = g_tls_connection_openssl_handshake_thread_handshake;
  base_class->retrieve_peer_certificate                  = g_tls_connection_openssl_retrieve_peer_certificate;
  base_class->get_channel_binding_data                   = g_tls_connection_openssl_get_channel_binding_data;
  base_class->push_io                                    = g_tls_connection_openssl_push_io;
  base_class->pop_io                                     = g_tls_connection_openssl_pop_io;
  base_class->read_fn                                    = g_tls_connection_openssl_read;
  base_class->read_message_fn                            = g_tls_connection_openssl_read_message;
  base_class->write_fn                                   = g_tls_connection_openssl_write;
  base_class->write_message_fn                           = g_tls_connection_openssl_write_message;
  base_class->close_fn                                   = g_tls_connection_openssl_close;
}

static int data_index = -1;

static gboolean
g_tls_connection_openssl_initable_init (GInitable     *initable,
                                        GCancellable  *cancellable,
                                        GError       **error)
{
  GTlsConnectionOpenssl *openssl = G_TLS_CONNECTION_OPENSSL (initable);
  GTlsConnectionOpensslPrivate *priv;
  GTlsConnectionBase *tls = G_TLS_CONNECTION_BASE (initable);
  GIOStream *base_io_stream;
  GDatagramBased *base_socket;
  SSL *ssl;

  g_object_get (tls,
                "base-io-stream", &base_io_stream,
                "base-socket", &base_socket,
                NULL);

  /* Ensure we are in TLS mode or DTLS mode. */
  g_return_val_if_fail (!!base_io_stream != !!base_socket, FALSE);

  priv = g_tls_connection_openssl_get_instance_private (openssl);

  ssl = g_tls_connection_openssl_get_ssl (openssl);
  g_assert (ssl);

  if (data_index == -1) {
      data_index = SSL_get_ex_new_index (0, (void *)"gtlsconnection", NULL, NULL, NULL);
  }
  SSL_set_ex_data (ssl, data_index, openssl);

  if (base_io_stream)
    priv->bio = g_tls_bio_new_from_iostream (base_io_stream);
  else
    priv->bio = g_tls_bio_new_from_datagram_based (base_socket);

  SSL_set_bio (ssl, priv->bio, priv->bio);

  g_clear_object (&base_io_stream);
  g_clear_object (&base_socket);

  return TRUE;
}

static void
g_tls_connection_openssl_initable_iface_init (GInitableIface *iface)
{
  iface->init = g_tls_connection_openssl_initable_init;
}

static void
g_tls_connection_openssl_init (GTlsConnectionOpenssl *openssl)
{
  GTlsConnectionOpensslPrivate *priv;

  priv = g_tls_connection_openssl_get_instance_private (openssl);

  g_mutex_init (&priv->ssl_mutex);
}

SSL *
g_tls_connection_openssl_get_ssl (GTlsConnectionOpenssl *openssl)
{
  g_return_val_if_fail (G_IS_TLS_CONNECTION_OPENSSL (openssl), NULL);

  return G_TLS_CONNECTION_OPENSSL_GET_CLASS (openssl)->get_ssl (openssl);
}

GTlsConnectionOpenssl *
g_tls_connection_openssl_get_connection_from_ssl (SSL *ssl)
{
  g_return_val_if_fail (ssl, NULL);

  return SSL_get_ex_data (ssl, data_index);
}
