/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2009 Red Hat, Inc
 * Copyright 2015, 2016 Collabora, Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 *
 * In addition, when the library is used with OpenSSL, a special
 * exception applies. Refer to the LICENSE_EXCEPTION file for details.
 */

#include "config.h"
#include "glib.h"

#include <errno.h>
#include <stdarg.h>
#include <gnutls/dtls.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#include "gtlsconnection-gnutls.h"
#include "gtlsbackend-gnutls.h"
#include "gtlscertificate-gnutls.h"
#include "gtlsclientconnection-gnutls.h"
#include "gtlsdatabase-gnutls.h"
#include "gtlslog.h"
#include "gtlsgnutls-version.h"

#ifdef G_OS_WIN32
#include <winsock2.h>
#include <winerror.h>

/* It isn’t clear whether MinGW always defines EMSGSIZE. */
#ifndef EMSGSIZE
#define EMSGSIZE WSAEMSGSIZE
#endif
#endif

#include <glib/gi18n-lib.h>
#include <glib/gprintf.h>

static ssize_t g_tls_connection_gnutls_push_func (gnutls_transport_ptr_t  transport_data,
                                                  const void             *buf,
                                                  size_t                  buflen);
static ssize_t g_tls_connection_gnutls_vec_push_func (gnutls_transport_ptr_t  transport_data,
                                                      const giovec_t         *iov,
                                                      int                     iovcnt);
static ssize_t g_tls_connection_gnutls_pull_func (gnutls_transport_ptr_t  transport_data,
                                                  void                   *buf,
                                                  size_t                  buflen);

static int     g_tls_connection_gnutls_pull_timeout_func (gnutls_transport_ptr_t transport_data,
                                                          unsigned int           ms);

static void g_tls_connection_gnutls_initable_iface_init (GInitableIface *iface);

static int verify_certificate_cb (gnutls_session_t session);

static gnutls_priority_t priority;

typedef struct
{
  GGnutlsCertificateCredentials *credentials;
  gnutls_session_t session;
  gchar *interaction_id;
  GCancellable *cancellable;
} GTlsConnectionGnutlsPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GTlsConnectionGnutls, g_tls_connection_gnutls, G_TYPE_TLS_CONNECTION_BASE,
                                  G_ADD_PRIVATE (GTlsConnectionGnutls);
                                  G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                         g_tls_connection_gnutls_initable_iface_init);
                                  );

static gint unique_interaction_id = 0;

static void
g_tls_connection_gnutls_init (GTlsConnectionGnutls *gnutls)
{
  GTlsConnectionGnutlsPrivate *priv = g_tls_connection_gnutls_get_instance_private (gnutls);
  int unique_id;

  unique_id = g_atomic_int_add (&unique_interaction_id, 1);
  priv->interaction_id = g_strdup_printf ("gtls:%d", unique_id);

  priv->cancellable = g_cancellable_new ();
}

static void
g_tls_connection_gnutls_set_handshake_priority (GTlsConnectionGnutls *gnutls)
{
  GTlsConnectionGnutlsPrivate *priv = g_tls_connection_gnutls_get_instance_private (gnutls);
  int ret;

  if (!priority)
    {
      /* initialize_gnutls_priority() previously failed and printed a warning,
       * so no need for further warnings here.
       */
      return;
    }

  ret = gnutls_priority_set (priv->session, priority);
  if (ret != GNUTLS_E_SUCCESS)
    g_warning ("Failed to set GnuTLS session priority: %s", gnutls_strerror (ret));
}

static int
handshake_thread_retrieve_function (gnutls_session_t              session,
                                    const gnutls_datum_t         *req_ca_rdn,
                                    int                           nreqs,
                                    const gnutls_pk_algorithm_t  *pk_algos,
                                    int                           pk_algos_length,
                                    gnutls_pcert_st             **pcert,
                                    unsigned int                 *pcert_length,
                                    gnutls_privkey_t             *pkey)
{
  GTlsConnectionGnutls *gnutls = gnutls_transport_get_ptr (session);
  GTlsConnectionGnutlsClass *connection_class = G_TLS_CONNECTION_GNUTLS_GET_CLASS (gnutls);

  g_assert (connection_class->handshake_thread_retrieve_function);
  return connection_class->handshake_thread_retrieve_function (gnutls, session, req_ca_rdn, nreqs, pk_algos, pk_algos_length, pcert, pcert_length, pkey);
}

static void
update_credentials_cb (GObject    *gobject,
                       GParamSpec *pspec,
                       gpointer    user_data)
{
  GTlsConnectionGnutls *gnutls = G_TLS_CONNECTION_GNUTLS (gobject);
  GTlsConnectionGnutlsPrivate *priv = g_tls_connection_gnutls_get_instance_private (gnutls);
  GGnutlsCertificateCredentials *credentials;
  GTlsDatabase *database;
  GError *error = NULL;
  int ret;

  database = g_tls_connection_get_database (G_TLS_CONNECTION (gnutls));
  if (database && G_IS_TLS_DATABASE_GNUTLS (database))
    {
      credentials = g_tls_database_gnutls_get_credentials (G_TLS_DATABASE_GNUTLS (database), &error);
      if (credentials)
        g_gnutls_certificate_credentials_ref (credentials);
    }
  else
    credentials = g_gnutls_certificate_credentials_new (&error);

  if (!credentials)
    {
      g_warning ("Failed to update credentials: %s", error->message);
      g_error_free (error);
      return;
    }

  ret = gnutls_credentials_set (priv->session, GNUTLS_CRD_CERTIFICATE, credentials->credentials);
  if (ret != 0)
    {
      g_warning ("Failed to update credentials: %s", gnutls_strerror (ret));
      return;
    }

  g_gnutls_certificate_credentials_unref (priv->credentials);
  priv->credentials = g_steal_pointer (&credentials);
  gnutls_certificate_set_retrieve_function2 (priv->credentials->credentials, handshake_thread_retrieve_function);
}

static gboolean
g_tls_connection_gnutls_initable_init (GInitable     *initable,
                                       GCancellable  *cancellable,
                                       GError       **error)
{
  GTlsConnectionGnutls *gnutls = G_TLS_CONNECTION_GNUTLS (initable);
  GTlsConnectionGnutlsPrivate *priv = g_tls_connection_gnutls_get_instance_private (gnutls);
  GTlsDatabase *database;
  GIOStream *base_io_stream = NULL;
  GDatagramBased *base_socket = NULL;
  gboolean client = G_IS_TLS_CLIENT_CONNECTION (gnutls);
  guint flags = client ? GNUTLS_CLIENT : GNUTLS_SERVER;
  GError *my_error = NULL;
  gboolean success = FALSE;
  int ret;

  g_object_get (gnutls,
                "base-io-stream", &base_io_stream,
                "base-socket", &base_socket,
                NULL);

  /* Ensure we are in TLS mode or DTLS mode. */
  g_return_val_if_fail (!!base_io_stream != !!base_socket, FALSE);

  if (base_socket)
    flags |= GNUTLS_DATAGRAM;

  database = g_tls_connection_get_database (G_TLS_CONNECTION (gnutls));
  if (database && G_IS_TLS_DATABASE_GNUTLS (database))
    {
      priv->credentials = g_tls_database_gnutls_get_credentials (G_TLS_DATABASE_GNUTLS (database), &my_error);
      if (!priv->credentials)
        {
          g_propagate_prefixed_error (error, my_error, _("Could not create TLS connection:"));
          goto out;
        }
      g_gnutls_certificate_credentials_ref (priv->credentials);
    }
  else
    {
      priv->credentials = g_gnutls_certificate_credentials_new (&my_error);
      if (!priv->credentials)
        {
          g_propagate_prefixed_error (error, my_error, _("Could not create TLS connection:"));
          goto out;
        }
    }
  gnutls_certificate_set_retrieve_function2 (priv->credentials->credentials, handshake_thread_retrieve_function);

  g_signal_connect (gnutls, "notify::database", G_CALLBACK (update_credentials_cb), NULL);
  g_signal_connect (gnutls, "notify::use-system-certdb", G_CALLBACK (update_credentials_cb), NULL);

  gnutls_init (&priv->session, flags);

  gnutls_session_set_ptr (priv->session, gnutls);
  gnutls_session_set_verify_function (priv->session, verify_certificate_cb);

  ret = gnutls_credentials_set (priv->session,
                                GNUTLS_CRD_CERTIFICATE,
                                priv->credentials->credentials);
  if (ret != 0)
    {
      g_set_error (error, G_TLS_ERROR, G_TLS_ERROR_MISC,
                   _("Could not create TLS connection: %s"),
                   gnutls_strerror (ret));
      goto out;
    }

  gnutls_transport_set_push_function (priv->session,
                                      g_tls_connection_gnutls_push_func);
  gnutls_transport_set_pull_function (priv->session,
                                      g_tls_connection_gnutls_pull_func);
  gnutls_transport_set_pull_timeout_function (priv->session,
                                              g_tls_connection_gnutls_pull_timeout_func);
  gnutls_transport_set_ptr (priv->session, gnutls);

  /* GDatagramBased supports vectored I/O; GPollableOutputStream does not. */
  if (base_socket)
    {
      gnutls_transport_set_vec_push_function (priv->session,
                                              g_tls_connection_gnutls_vec_push_func);
    }

  /* Set reasonable MTU */
  if (flags & GNUTLS_DATAGRAM)
    gnutls_dtls_set_mtu (priv->session, 1400);

  success = TRUE;

out:
  g_clear_object (&base_io_stream);
  g_clear_object (&base_socket);

  return success;
}

static void
g_tls_connection_gnutls_finalize (GObject *object)
{
  GTlsConnectionGnutls *gnutls = G_TLS_CONNECTION_GNUTLS (object);
  GTlsConnectionGnutlsPrivate *priv = g_tls_connection_gnutls_get_instance_private (gnutls);

  g_clear_pointer (&priv->session, gnutls_deinit);
  g_clear_pointer (&priv->credentials, g_gnutls_certificate_credentials_unref);

  if (priv->cancellable)
    {
      g_cancellable_cancel (priv->cancellable);
      g_clear_object (&priv->cancellable);
    }

  g_free (priv->interaction_id);

  G_OBJECT_CLASS (g_tls_connection_gnutls_parent_class)->finalize (object);
}

gnutls_session_t
g_tls_connection_gnutls_get_session (GTlsConnectionGnutls *gnutls)
{
  GTlsConnectionGnutlsPrivate *priv = g_tls_connection_gnutls_get_instance_private (gnutls);

  return priv->session;
}

static int
on_pin_request (void         *userdata,
                int           attempt,
                const char   *token_url,
                const char   *token_label,
                unsigned int  callback_flags,
                char         *pin,
                size_t        pin_max)
{
  GTlsConnection *connection = G_TLS_CONNECTION (userdata);
  GTlsInteraction *interaction = g_tls_connection_get_interaction (connection);
  GTlsPassword *password;
  GTlsPasswordFlags password_flags = 0;
  gchar *description;
  int ret = -1;

  if (!interaction)
    return -1;

  if (callback_flags & GNUTLS_PIN_WRONG)
    password_flags |= G_TLS_PASSWORD_RETRY;
  if (callback_flags & GNUTLS_PIN_COUNT_LOW)
    password_flags |= G_TLS_PASSWORD_MANY_TRIES;
  if (callback_flags & GNUTLS_PIN_FINAL_TRY || attempt > 5) /* Give up at some point */
    password_flags |= G_TLS_PASSWORD_FINAL_TRY;

  if (callback_flags & GNUTLS_PIN_USER)
    password_flags |= G_TLS_PASSWORD_PKCS11_USER;
  if (callback_flags & GNUTLS_PIN_SO)
    password_flags |= G_TLS_PASSWORD_PKCS11_SECURITY_OFFICER;
  if (callback_flags & GNUTLS_PIN_CONTEXT_SPECIFIC)
    password_flags |= G_TLS_PASSWORD_PKCS11_CONTEXT_SPECIFIC;

  description = g_strdup_printf (" %s (%s)", token_label, token_url);
  password = g_tls_password_new (password_flags, description);
  if (g_tls_connection_base_handshake_thread_ask_password (G_TLS_CONNECTION_BASE (connection), password))
    {
      gsize password_size;
      const guchar *password_data = g_tls_password_get_value (password, &password_size);
      if (password_size > pin_max - 1)
        g_info ("PIN is larger than max PIN size");

      /* Ensure NUL-termination */
      memset (pin, 0, pin_max);
      memcpy (pin, password_data, MIN (password_size, pin_max - 1));

      ret = GNUTLS_E_SUCCESS;
    }

  g_free (description);
  g_object_unref (password);
  return ret;
}

void
g_tls_connection_gnutls_handshake_thread_get_certificate (GTlsConnectionGnutls  *gnutls,
                                                          gnutls_pcert_st      **pcert,
                                                          unsigned int          *pcert_length,
                                                          gnutls_privkey_t      *pkey)
{
  GTlsConnectionGnutlsPrivate *priv = g_tls_connection_gnutls_get_instance_private (gnutls);
  GTlsCertificate *cert;

  cert = g_tls_connection_get_certificate (G_TLS_CONNECTION (gnutls));

  if (cert)
    {
      /* Send along a pre-initialized privkey so we can handle the callback here. */
      gnutls_privkey_t privkey;
      gnutls_privkey_init (&privkey);
      gnutls_privkey_set_pin_function (privkey, on_pin_request, gnutls);

      g_tls_certificate_gnutls_copy (G_TLS_CERTIFICATE_GNUTLS (cert),
                                     priv->interaction_id,
                                     pcert, pcert_length, &privkey);
      *pkey = privkey;
    }
  else
    {
      *pcert = NULL;
      *pcert_length = 0;
      *pkey = NULL;
    }
}

static GTlsConnectionBaseStatus
end_gnutls_io (GTlsConnectionGnutls  *gnutls,
               GIOCondition           direction,
               int                    ret,
               GError               **error,
               const char            *err_prefix)
{
  GTlsConnectionGnutlsPrivate *priv = g_tls_connection_gnutls_get_instance_private (gnutls);
  GTlsConnectionBase *tls = G_TLS_CONNECTION_BASE (gnutls);
  GTlsConnectionBaseStatus status;
  gboolean handshaking;
  gboolean ever_handshaked;
  GError *my_error = NULL;

  /* We intentionally do not check for GNUTLS_E_INTERRUPTED here
   * Instead, the caller may poll for the source to become ready again.
   * (Note that GTlsOutputStreamGnutls and GTlsInputStreamGnutls inherit
   * from GPollableOutputStream and GPollableInputStream, respectively.)
   * See also the comment in set_gnutls_error().
   */
  if (ret == GNUTLS_E_AGAIN ||
      ret == GNUTLS_E_WARNING_ALERT_RECEIVED)
    return G_TLS_CONNECTION_BASE_TRY_AGAIN;

  status = g_tls_connection_base_pop_io (tls, direction, ret >= 0, &my_error);
  if (status == G_TLS_CONNECTION_BASE_OK ||
      status == G_TLS_CONNECTION_BASE_WOULD_BLOCK ||
      status == G_TLS_CONNECTION_BASE_TIMED_OUT)
    {
      if (my_error)
        g_propagate_error (error, my_error);
      return status;
    }

  g_assert (status == G_TLS_CONNECTION_BASE_ERROR);

  handshaking = g_tls_connection_base_is_handshaking (tls);
  ever_handshaked = g_tls_connection_base_ever_handshaked (tls);

  if (handshaking && !ever_handshaked)
    {
      if (g_error_matches (my_error, G_IO_ERROR, G_IO_ERROR_FAILED) ||
          g_error_matches (my_error, G_IO_ERROR, G_IO_ERROR_BROKEN_PIPE))
        {
          g_set_error (error, G_TLS_ERROR, G_TLS_ERROR_NOT_TLS,
                       _("Peer failed to perform TLS handshake: %s"), my_error->message);
          g_clear_error (&my_error);
          return G_TLS_CONNECTION_BASE_ERROR;
        }

      if (ret == GNUTLS_E_UNEXPECTED_PACKET_LENGTH ||
          ret == GNUTLS_E_DECRYPTION_FAILED ||
          ret == GNUTLS_E_UNSUPPORTED_VERSION_PACKET)
        {
          g_clear_error (&my_error);
          g_set_error (error, G_TLS_ERROR, G_TLS_ERROR_NOT_TLS,
                       _("Peer failed to perform TLS handshake: %s"), gnutls_strerror (ret));
          return G_TLS_CONNECTION_BASE_ERROR;
        }
    }

  if (ret == GNUTLS_E_REHANDSHAKE)
    return G_TLS_CONNECTION_BASE_REHANDSHAKE;

  if (ret == GNUTLS_E_PREMATURE_TERMINATION)
    {
      if (handshaking && !ever_handshaked)
        {
          g_clear_error (&my_error);
          g_set_error (error, G_TLS_ERROR, G_TLS_ERROR_NOT_TLS,
                       _("Peer failed to perform TLS handshake: %s"), gnutls_strerror (ret));
          return G_TLS_CONNECTION_BASE_ERROR;
        }

      if (g_tls_connection_get_require_close_notify (G_TLS_CONNECTION (gnutls)))
        {
          g_clear_error (&my_error);
          g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_EOF,
                               _("TLS connection closed unexpectedly"));
          return G_TLS_CONNECTION_BASE_ERROR;
        }

      return G_TLS_CONNECTION_BASE_OK;
    }

  if (ret == GNUTLS_E_NO_CERTIFICATE_FOUND
#ifdef GNUTLS_E_CERTIFICATE_REQUIRED
           || ret == GNUTLS_E_CERTIFICATE_REQUIRED /* Added in GnuTLS 3.6.7 */
#endif
          )
    {
      g_clear_error (&my_error);
      g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_CERTIFICATE_REQUIRED,
                           _("TLS connection peer did not send a certificate"));
      return G_TLS_CONNECTION_BASE_ERROR;
    }

  if (ret == GNUTLS_E_CERTIFICATE_ERROR)
    {
      g_clear_error (&my_error);
      g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                           _("Unacceptable TLS certificate"));
      return G_TLS_CONNECTION_BASE_ERROR;
    }

  if (ret == GNUTLS_E_FATAL_ALERT_RECEIVED)
    {
      g_clear_error (&my_error);
      g_set_error (error, G_TLS_ERROR, G_TLS_ERROR_MISC,
                   _("Peer sent fatal TLS alert: %s"),
                   gnutls_alert_get_name (gnutls_alert_get (priv->session)));
      return G_TLS_CONNECTION_BASE_ERROR;
    }

  if (ret == GNUTLS_E_INAPPROPRIATE_FALLBACK)
    {
      g_clear_error (&my_error);
      g_set_error_literal (error, G_TLS_ERROR,
                           G_TLS_ERROR_INAPPROPRIATE_FALLBACK,
                           _("Protocol version downgrade attack detected"));
      return G_TLS_CONNECTION_BASE_ERROR;
    }

  if (ret == GNUTLS_E_LARGE_PACKET)
    {
      guint mtu = gnutls_dtls_get_data_mtu (priv->session);
      g_clear_error (&my_error);
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_MESSAGE_TOO_LARGE,
                   ngettext ("Message is too large for DTLS connection; maximum is %u byte",
                             "Message is too large for DTLS connection; maximum is %u bytes", mtu), mtu);
      return G_TLS_CONNECTION_BASE_ERROR;
    }

  if (ret == GNUTLS_E_TIMEDOUT)
    {
      g_clear_error (&my_error);
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT,
                           _("The operation timed out"));
      return G_TLS_CONNECTION_BASE_ERROR;
    }

  if (error && my_error)
    g_propagate_error (error, my_error);

  if (error && !*error)
    {
      *error = g_error_new (G_TLS_ERROR, G_TLS_ERROR_MISC, "%s: %s",
                            gettext (err_prefix), gnutls_strerror (ret));
    }

  return G_TLS_CONNECTION_BASE_ERROR;
}

#define BEGIN_GNUTLS_IO(gnutls, direction, timeout, cancellable)        \
  g_tls_connection_base_push_io (G_TLS_CONNECTION_BASE (gnutls),        \
                                 direction, timeout, cancellable);      \
  do {

#define END_GNUTLS_IO(gnutls, direction, ret, status, errmsg, err)      \
    status = end_gnutls_io (gnutls, direction, ret, err, errmsg);       \
  } while (status == G_TLS_CONNECTION_BASE_TRY_AGAIN);

static void
set_gnutls_error (GTlsConnectionGnutls *gnutls,
                  GError               *error)
{
  GTlsConnectionBase *tls = G_TLS_CONNECTION_BASE (gnutls);
  GTlsConnectionGnutlsPrivate *priv = g_tls_connection_gnutls_get_instance_private (gnutls);

  /* We set EINTR rather than EAGAIN for G_IO_ERROR_WOULD_BLOCK so
   * that GNUTLS_E_AGAIN only gets returned for gnutls-internal
   * reasons, not for actual socket EAGAINs (and we have access
   * to @error at the higher levels, so we can distinguish them
   * that way later).
   */

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    gnutls_transport_set_errno (priv->session, EINTR);
  else if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK))
    {
      /* Return EAGAIN while handshaking so that GnuTLS handles retries for us
       * internally in its handshaking code. */
      if (g_tls_connection_base_is_dtls (tls) && g_tls_connection_base_is_handshaking (tls))
        gnutls_transport_set_errno (priv->session, EAGAIN);
      else
        gnutls_transport_set_errno (priv->session, EINTR);
    }
  else if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT))
    gnutls_transport_set_errno (priv->session, EINTR);
  else if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_MESSAGE_TOO_LARGE))
    gnutls_transport_set_errno (priv->session, EMSGSIZE);
  else
    gnutls_transport_set_errno (priv->session, EIO);
}

static ssize_t
g_tls_connection_gnutls_pull_func (gnutls_transport_ptr_t  transport_data,
                                   void                   *buf,
                                   size_t                  buflen)
{
  GTlsConnectionBase *tls = transport_data;
  GTlsConnectionGnutls *gnutls = transport_data;
  ssize_t ret;

  /* If read_error is nonnull when we're called, it means
   * that an error previously occurred, but GnuTLS decided not to
   * propagate it. So it's correct for us to just clear it. (Usually
   * this means it ignored an EAGAIN after a short read, and now
   * we'll return EAGAIN again, which it will obey this time.)
   */
  g_clear_error (g_tls_connection_base_get_read_error (tls));

  if (g_tls_connection_base_is_dtls (tls))
    {
      GInputVector vector = { buf, buflen };
      GInputMessage message = { NULL, &vector, 1, 0, 0, NULL, NULL };

      ret = g_datagram_based_receive_messages (g_tls_connection_base_get_base_socket (tls),
                                               &message, 1, 0,
                                               g_tls_connection_base_is_handshaking (tls) ? 0 : g_tls_connection_base_get_read_timeout (tls),
                                               g_tls_connection_base_get_read_cancellable (tls),
                                               g_tls_connection_base_get_read_error (tls));

      if (ret > 0)
        ret = message.bytes_received;
    }
  else
    {
      ret = g_pollable_stream_read (G_INPUT_STREAM (g_tls_connection_base_get_base_istream (tls)),
                                    buf, buflen,
                                    g_tls_connection_base_get_read_timeout (tls) != 0,
                                    g_tls_connection_base_get_read_cancellable (tls),
                                    g_tls_connection_base_get_read_error (tls));
    }

  if (ret < 0)
    set_gnutls_error (gnutls, *g_tls_connection_base_get_read_error (tls));

  return ret;
}

static ssize_t
g_tls_connection_gnutls_push_func (gnutls_transport_ptr_t  transport_data,
                                   const void             *buf,
                                   size_t                  buflen)
{
  GTlsConnectionBase *tls = transport_data;
  GTlsConnectionGnutls *gnutls = transport_data;
  ssize_t ret;

  /* See comment in pull_func. */
  g_clear_error (g_tls_connection_base_get_write_error (tls));

  if (g_tls_connection_base_is_dtls (tls))
    {
      GOutputVector vector = { buf, buflen };
      GOutputMessage message = { NULL, &vector, 1, 0, NULL, 0 };

      ret = g_datagram_based_send_messages (g_tls_connection_base_get_base_socket (tls),
                                            &message, 1, 0,
                                            g_tls_connection_base_get_write_timeout (tls),
                                            g_tls_connection_base_get_write_cancellable (tls),
                                            g_tls_connection_base_get_write_error (tls));

      if (ret > 0)
        ret = message.bytes_sent;
    }
  else
    {
      ret = g_pollable_stream_write (G_OUTPUT_STREAM (g_tls_connection_base_get_base_ostream (tls)),
                                     buf, buflen,
                                     g_tls_connection_base_get_write_timeout (tls) != 0,
                                     g_tls_connection_base_get_write_cancellable (tls),
                                     g_tls_connection_base_get_write_error (tls));
    }

  if (ret < 0)
    set_gnutls_error (gnutls, *g_tls_connection_base_get_write_error (tls));

  return ret;
}

static ssize_t
g_tls_connection_gnutls_vec_push_func (gnutls_transport_ptr_t  transport_data,
                                       const giovec_t         *iov,
                                       int                     iovcnt)
{
  GTlsConnectionBase *tls = transport_data;
  GTlsConnectionGnutls *gnutls = transport_data;
  ssize_t ret;
  GOutputMessage message = { NULL, };
  GOutputVector *vectors;

  g_assert (g_tls_connection_base_is_dtls (tls));

  /* See comment in pull_func. */
  g_clear_error (g_tls_connection_base_get_write_error (tls));

  /* this entire expression will be evaluated at compile time */
  if (sizeof *iov == sizeof *vectors &&
      sizeof iov->iov_base == sizeof vectors->buffer &&
      G_STRUCT_OFFSET (giovec_t, iov_base) ==
      G_STRUCT_OFFSET (GOutputVector, buffer) &&
      sizeof iov->iov_len == sizeof vectors->size &&
      G_STRUCT_OFFSET (giovec_t, iov_len) ==
      G_STRUCT_OFFSET (GOutputVector, size))
    /* ABI is compatible */
    {
      message.vectors = (GOutputVector *)iov;
      message.num_vectors = iovcnt;
    }
  else
    /* ABI is incompatible */
    {
      gint i;

      message.vectors = g_newa (GOutputVector, iovcnt);
      for (i = 0; i < iovcnt; i++)
        {
          message.vectors[i].buffer = (void *)iov[i].iov_base;
          message.vectors[i].size = iov[i].iov_len;
        }
      message.num_vectors = iovcnt;
    }

  ret = g_datagram_based_send_messages (g_tls_connection_base_get_base_socket (tls),
                                        &message, 1, 0,
                                        g_tls_connection_base_get_write_timeout (tls),
                                        g_tls_connection_base_get_write_cancellable (tls),
                                        g_tls_connection_base_get_write_error (tls));

  if (ret > 0)
    ret = message.bytes_sent;
  else if (ret < 0)
    set_gnutls_error (gnutls, *g_tls_connection_base_get_write_error (tls));

  return ret;
}

static gboolean
read_pollable_cb (GPollableInputStream *istream,
                  gpointer              user_data)
{
  gboolean *done = user_data;

  *done = TRUE;

  return G_SOURCE_REMOVE;
}

static gboolean
read_datagram_based_cb (GDatagramBased *datagram_based,
                        GIOCondition    condition,
                        gpointer        user_data)
{
  gboolean *done = user_data;

  *done = TRUE;

  return G_SOURCE_REMOVE;
}

static gboolean
read_timeout_cb (gpointer user_data)
{
  gboolean *done = user_data;

  *done = TRUE;

  return G_SOURCE_REMOVE;
}

static gboolean
read_cancelled_cb (GCancellable *cancellable,
                   gpointer      user_data)
{
  gboolean *done = user_data;

  *done = TRUE;

  return G_SOURCE_REMOVE;
}

static int
g_tls_connection_gnutls_pull_timeout_func (gnutls_transport_ptr_t transport_data,
                                           unsigned int           ms)
{
  GTlsConnectionBase *tls = transport_data;

  /* Fast path. */
  if (g_tls_connection_base_base_check (tls, G_IO_IN) ||
      g_cancellable_is_cancelled (g_tls_connection_base_get_read_cancellable (tls)))
    return 1;

  /* If @ms is 0, GnuTLS wants an instant response, so there’s no need to
   * construct and query a #GSource. */
  if (ms > 0)
    {
      GMainContext *ctx = NULL;
      GSource *read_source = NULL;
      GSource *timeout_source = NULL;
      GSource *cancellable_source = NULL;
      gboolean done = FALSE;

      ctx = g_main_context_new ();

      /* Create a timeout source. */
      timeout_source = g_timeout_source_new (ms);
      g_source_set_callback (timeout_source, (GSourceFunc)read_timeout_cb,
                             &done, NULL);

      /* Create a read source. We cannot use g_source_set_ready_time() on this
       * to combine it with the @timeout_source, as that could mess with the
       * internals of the #GDatagramBased’s #GSource implementation. */
      if (g_tls_connection_base_is_dtls (tls))
        {
          read_source = g_datagram_based_create_source (g_tls_connection_base_get_base_socket (tls),
                                                        G_IO_IN, NULL);
          g_source_set_callback (read_source, (GSourceFunc)read_datagram_based_cb,
                                 &done, NULL);
        }
      else
        {
          read_source = g_pollable_input_stream_create_source (g_tls_connection_base_get_base_istream (tls),
                                                               NULL);
          g_source_set_callback (read_source, (GSourceFunc)read_pollable_cb,
                                 &done, NULL);
        }

      cancellable_source = g_cancellable_source_new (g_tls_connection_base_get_read_cancellable (tls));
      g_source_set_callback (cancellable_source, (GSourceFunc)read_cancelled_cb,
                             &done, NULL);

      g_source_attach (read_source, ctx);
      g_source_attach (timeout_source, ctx);
      g_source_attach (cancellable_source, ctx);

      while (!done)
        g_main_context_iteration (ctx, TRUE);

      g_source_destroy (read_source);
      g_source_destroy (timeout_source);
      g_source_destroy (cancellable_source);

      g_main_context_unref (ctx);
      g_source_unref (read_source);
      g_source_unref (timeout_source);
      g_source_unref (cancellable_source);

      /* If @read_source was dispatched due to cancellation, the resulting error
       * will be handled in g_tls_connection_gnutls_pull_func(). */
      if (g_tls_connection_base_base_check (tls, G_IO_IN) ||
          g_cancellable_is_cancelled (g_tls_connection_base_get_read_cancellable (tls)))
        return 1;
    }

  return 0;
}

static GTlsSafeRenegotiationStatus
g_tls_connection_gnutls_handshake_thread_safe_renegotiation_status (GTlsConnectionBase *tls)
{
  GTlsConnectionGnutls *gnutls = G_TLS_CONNECTION_GNUTLS (tls);
  GTlsConnectionGnutlsPrivate *priv = g_tls_connection_gnutls_get_instance_private (gnutls);

  return gnutls_safe_renegotiation_status (priv->session) ? G_TLS_SAFE_RENEGOTIATION_SUPPORTED_BY_PEER
                                                          : G_TLS_SAFE_RENEGOTIATION_UNSUPPORTED;
}

static GTlsConnectionBaseStatus
g_tls_connection_gnutls_handshake_thread_request_rehandshake (GTlsConnectionBase  *tls,
                                                              gint64               timeout,
                                                              GCancellable        *cancellable,
                                                              GError             **error)
{
  GTlsConnectionGnutls *gnutls = G_TLS_CONNECTION_GNUTLS (tls);
  GTlsConnectionGnutlsPrivate *priv = g_tls_connection_gnutls_get_instance_private (gnutls);
  GTlsConnectionBaseStatus status;
  int ret;

  /* On a client-side connection, gnutls_handshake() itself will start
   * a rehandshake, so we only need to do something special here for
   * server-side connections.
   */
  if (!G_IS_TLS_SERVER_CONNECTION (tls))
    return G_TLS_CONNECTION_BASE_OK;

  BEGIN_GNUTLS_IO (gnutls, G_IO_IN | G_IO_OUT, timeout, cancellable);
  ret = gnutls_rehandshake (priv->session);
  END_GNUTLS_IO (gnutls, G_IO_IN | G_IO_OUT, ret, status, N_("Error performing TLS handshake: %s"), error);

  return status;
}

static GTlsCertificate *
g_tls_connection_gnutls_retrieve_peer_certificate (GTlsConnectionBase *tls)
{
  GTlsConnectionGnutls *gnutls = G_TLS_CONNECTION_GNUTLS (tls);
  GTlsConnectionGnutlsPrivate *priv = g_tls_connection_gnutls_get_instance_private (gnutls);
  const gnutls_datum_t *certs;
  GTlsCertificateGnutls *chain;
  unsigned int num_certs;

  if (gnutls_certificate_type_get (priv->session) != GNUTLS_CRT_X509)
    return NULL;

  certs = gnutls_certificate_get_peers (priv->session, &num_certs);
  if (!certs || !num_certs)
    return NULL;

  chain = g_tls_certificate_gnutls_build_chain (certs, num_certs, GNUTLS_X509_FMT_DER);
  if (!chain)
    return NULL;

  return G_TLS_CERTIFICATE (chain);
}

static int
verify_certificate_cb (gnutls_session_t session)
{
  GTlsConnectionBase *tls = gnutls_session_get_ptr (session);

  /* Return 0 for the handshake to continue, non-zero to terminate.
   * Complete opposite of what OpenSSL does. */
  return !g_tls_connection_base_handshake_thread_verify_certificate (tls);
}

static void
g_tls_connection_gnutls_prepare_handshake (GTlsConnectionBase  *tls,
                                           gchar              **advertised_protocols)
{
  GTlsConnectionGnutls *gnutls = G_TLS_CONNECTION_GNUTLS (tls);
  GTlsConnectionGnutlsPrivate *priv = g_tls_connection_gnutls_get_instance_private (gnutls);

  if (advertised_protocols)
    {
      gnutls_datum_t *protocols;
      int n_protos, i;

      n_protos = g_strv_length (advertised_protocols);
      protocols = g_new (gnutls_datum_t, n_protos);
      for (i = 0; advertised_protocols[i]; i++)
        {
          protocols[i].size = strlen (advertised_protocols[i]);
          protocols[i].data = (guchar *)advertised_protocols[i];
        }
      gnutls_alpn_set_protocols (priv->session, protocols, n_protos, 0);
      g_free (protocols);
    }
}

static GTlsConnectionBaseStatus
g_tls_connection_gnutls_handshake_thread_handshake (GTlsConnectionBase  *tls,
                                                    gint64               timeout,
                                                    GCancellable        *cancellable,
                                                    GError             **error)
{
  GTlsConnectionGnutls *gnutls = G_TLS_CONNECTION_GNUTLS (tls);
  GTlsConnectionGnutlsPrivate *priv = g_tls_connection_gnutls_get_instance_private (gnutls);
  GTlsConnectionBaseStatus status;
  int ret;

  if (!g_tls_connection_base_ever_handshaked (tls))
    g_tls_connection_gnutls_set_handshake_priority (gnutls);

  if (timeout > 0)
    {
      unsigned int timeout_ms;

      /* Convert from microseconds to milliseconds, but ensure the timeout
       * remains positive. */
      timeout_ms = (timeout + 999) / 1000;

      gnutls_handshake_set_timeout (priv->session, timeout_ms);
      gnutls_dtls_set_timeouts (priv->session, 1000 /* default */, timeout_ms);
    }

  BEGIN_GNUTLS_IO (gnutls, G_IO_IN | G_IO_OUT, timeout, cancellable);
  ret = gnutls_handshake (priv->session);
  if (ret == GNUTLS_E_GOT_APPLICATION_DATA)
    {
      guint8 buf[1024];

      /* Got app data while waiting for rehandshake; buffer it and try again */
      ret = gnutls_record_recv (priv->session, buf, sizeof (buf));
      if (ret > -1)
        {
          g_tls_connection_base_handshake_thread_buffer_application_data (tls, buf, ret);
          ret = GNUTLS_E_AGAIN;
        }
    }
  END_GNUTLS_IO (gnutls, G_IO_IN | G_IO_OUT, ret, status,
                 N_("Error performing TLS handshake"), error);

  return status;
}

static GTlsCertificateFlags
g_tls_connection_gnutls_verify_chain (GTlsConnectionBase       *tls,
                                      GTlsCertificate          *chain,
                                      const gchar              *purpose,
                                      GSocketConnectable       *identity,
                                      GTlsInteraction          *interaction,
                                      GTlsDatabaseVerifyFlags   flags,
                                      GCancellable             *cancellable,
                                      GError                  **error)
{
  GTlsConnectionGnutls *gnutls = G_TLS_CONNECTION_GNUTLS (tls);
  GTlsConnectionGnutlsPrivate *priv = g_tls_connection_gnutls_get_instance_private (gnutls);
  GTlsCertificateFlags errors = 0;
  const char *hostname = NULL;
  char *free_hostname = NULL;
  GTlsDatabase *database;
  guint gnutls_result;
  int ret;

  /* There are several different ways to perform certificate verification with
   * GnuTLS, but they all fall into one of two categories:
   *
   * (a) outside the context of a TLS session
   * (b) within the context of a TLS session
   *
   * (a) is done by g_tls_database_verify_chain() and implemented using one of
   * several different functions of gnutls_x509_trust_list_t, e.g.
   * gnutls_x509_trust_list_verify_crt2() or one of the related functions.
   * This is the best we can do if we have to use a GTlsDatabase that is not a
   * GTlsDatabaseGnutls.
   */
  database = g_tls_connection_get_database (G_TLS_CONNECTION (gnutls));
  if (!G_IS_TLS_DATABASE_GNUTLS (database))
    {
      return g_tls_database_verify_chain (database,
                                          chain,
                                          G_IS_TLS_CLIENT_CONNECTION (tls) ? G_TLS_DATABASE_PURPOSE_AUTHENTICATE_SERVER : G_TLS_DATABASE_PURPOSE_AUTHENTICATE_CLIENT,
                                          identity,
                                          g_tls_connection_get_interaction (G_TLS_CONNECTION (tls)),
                                          G_TLS_DATABASE_VERIFY_NONE,
                                          NULL,
                                          error);
    }

  /* Now for (b). The recommended way is gnutls_session_set_verify_cert(), but
   * we can't use that because that would leave no way to implement the
   * GTlsConnection::accept-certificate signal. The other way is to use
   * gnutls_certificate_verify_peers3() or one of the related functions. This
   * adds additional smarts that are not possible when using GTlsDatabase
   * directly. For example, it checks name constraints, key usage, and basic
   * constraints. It also checks for stapled OCSP responses. Verification will
   * fail if the OCSP response indicates the certificate has been revoked.
   * Verification will also fail if the Must-Staple flag is set but the OCSP
   * response is missing. Nice! This uses the gnutls_certificate_credentials_t
   * set on the gnutls_session_t by gnutls_credentials_set().
   */

  if (G_IS_NETWORK_ADDRESS (identity))
    hostname = g_network_address_get_hostname (G_NETWORK_ADDRESS (identity));
  else if (G_IS_NETWORK_SERVICE (identity))
    hostname = g_network_service_get_domain (G_NETWORK_SERVICE (identity));
  else if (G_IS_INET_SOCKET_ADDRESS (identity))
    {
      GInetAddress *addr;

      addr = g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (identity));
      hostname = free_hostname = g_inet_address_to_string (addr);
    }
  else if (identity)
    {
      g_set_error (error, G_TLS_ERROR, G_TLS_ERROR_MISC,
                   _("Cannot verify peer identity of unexpected type %s"), G_OBJECT_TYPE_NAME (identity));
      errors |= G_TLS_CERTIFICATE_BAD_IDENTITY;
    }

  ret = gnutls_certificate_verify_peers3 (priv->session, hostname, &gnutls_result);
  if (ret != 0)
    errors |= G_TLS_CERTIFICATE_GENERIC_ERROR;
  else
    errors |= g_tls_certificate_gnutls_convert_flags (gnutls_result);

  g_free (free_hostname);
  return errors;
}

GTlsProtocolVersion
glib_protocol_version_from_gnutls (gnutls_protocol_t protocol_version)
{
  switch (protocol_version)
    {
    case GNUTLS_SSL3:
      return G_TLS_PROTOCOL_VERSION_SSL_3_0;
    case GNUTLS_TLS1_0:
      return G_TLS_PROTOCOL_VERSION_TLS_1_0;
    case GNUTLS_TLS1_1:
      return G_TLS_PROTOCOL_VERSION_TLS_1_1;
    case GNUTLS_TLS1_2:
      return G_TLS_PROTOCOL_VERSION_TLS_1_2;
    case GNUTLS_TLS1_3:
      return G_TLS_PROTOCOL_VERSION_TLS_1_3;
    case GNUTLS_DTLS0_9:
      return G_TLS_PROTOCOL_VERSION_UNKNOWN;
    case GNUTLS_DTLS1_0:
      return G_TLS_PROTOCOL_VERSION_DTLS_1_0;
    case GNUTLS_DTLS1_2:
      return G_TLS_PROTOCOL_VERSION_DTLS_1_2;
    default:
      return G_TLS_PROTOCOL_VERSION_UNKNOWN;
    }
}

static gchar *
get_ciphersuite_name (gnutls_session_t session)
{
  return g_strdup (gnutls_ciphersuite_get (session));
}

static void
g_tls_connection_gnutls_complete_handshake (GTlsConnectionBase   *tls,
                                            gboolean              handshake_succeeded,
                                            gchar               **negotiated_protocol,
                                            GTlsProtocolVersion  *protocol_version,
                                            gchar               **ciphersuite_name,
                                            GError              **error)
{
  GTlsConnectionGnutls *gnutls = G_TLS_CONNECTION_GNUTLS (tls);
  GTlsConnectionGnutlsPrivate *priv = g_tls_connection_gnutls_get_instance_private (gnutls);
  gnutls_datum_t protocol;

  if (!handshake_succeeded)
    return;

  if (gnutls_alpn_get_selected_protocol (priv->session, &protocol) == 0 &&
      protocol.size > 0)
    {
      g_assert (!*negotiated_protocol);
      *negotiated_protocol = g_strndup ((gchar *)protocol.data, protocol.size);
    }

  *protocol_version = glib_protocol_version_from_gnutls (gnutls_protocol_get_version (priv->session));
  *ciphersuite_name = get_ciphersuite_name (priv->session);
}

static gboolean
g_tls_connection_gnutls_is_session_resumed (GTlsConnectionBase *tls)
{
  GTlsConnectionGnutls *gnutls = G_TLS_CONNECTION_GNUTLS (tls);
  GTlsConnectionGnutlsPrivate *priv = g_tls_connection_gnutls_get_instance_private (gnutls);

  return gnutls_session_is_resumed (priv->session);
}

static gboolean
gnutls_get_binding (GTlsConnectionGnutls      *gnutls,
                    GByteArray                *data,
                    gnutls_channel_binding_t   binding,
                    GError                   **error)
{
  GTlsConnectionGnutlsPrivate *priv = g_tls_connection_gnutls_get_instance_private (gnutls);
  gnutls_datum_t cb;
  int ret = gnutls_session_channel_binding (priv->session, binding, &cb);

  if (ret == GNUTLS_E_SUCCESS)
    {
      /* Older GnuTLS versions are known to return SUCCESS and empty data for TLSv1.3 tls-unique binding.
       * While it may look prudent to catch here that specific corner case, the empty binding data is
       * definitely not a SUCCESS, regardless of the version and type. */
      if (cb.size == 0)
        {
          g_set_error (error, G_TLS_CHANNEL_BINDING_ERROR, G_TLS_CHANNEL_BINDING_ERROR_GENERAL_ERROR,
                       _("Empty channel binding data indicates a bug in the TLS library implementation"));
          return FALSE;
        }

      if (data != NULL)
        {
          g_tls_log_debug (gnutls, "binding size %d", cb.size);
          g_free (g_byte_array_steal (data, NULL));
          g_byte_array_append (data, cb.data, cb.size);
        }
      g_free (cb.data);
      return TRUE;
    }

  switch (ret)
    {
    case GNUTLS_E_UNIMPLEMENTED_FEATURE:
      g_set_error (error, G_TLS_CHANNEL_BINDING_ERROR, G_TLS_CHANNEL_BINDING_ERROR_NOT_IMPLEMENTED,
                   _("Channel binding type is not implemented in the TLS library"));
      break;
    case GNUTLS_E_CHANNEL_BINDING_NOT_AVAILABLE:
      g_set_error (error, G_TLS_CHANNEL_BINDING_ERROR, G_TLS_CHANNEL_BINDING_ERROR_NOT_AVAILABLE,
                   _("Channel binding data is not yet available"));
      break;
    default:
      g_set_error (error, G_TLS_CHANNEL_BINDING_ERROR, G_TLS_CHANNEL_BINDING_ERROR_GENERAL_ERROR,
                   "%s", gnutls_strerror (ret));
    }
  return FALSE;
}

static gboolean
gnutls_get_binding_tls_unique (GTlsConnectionGnutls  *gnutls,
                               GByteArray            *data,
                               GError               **error)
{
  return gnutls_get_binding (gnutls, data, GNUTLS_CB_TLS_UNIQUE, error);
}

static gboolean
gnutls_get_binding_tls_server_end_point (GTlsConnectionGnutls  *gnutls,
                                         GByteArray            *data,
                                         GError               **error)
{
  return gnutls_get_binding (gnutls, data, GNUTLS_CB_TLS_SERVER_END_POINT, error);
}

static gboolean
gnutls_get_binding_tls_exporter (GTlsConnectionGnutls  *gnutls,
                                 GByteArray            *data,
                                 GError               **error)
{
  return gnutls_get_binding (gnutls, data, GNUTLS_CB_TLS_EXPORTER, error);
}

static gboolean
g_tls_connection_gnutls_get_channel_binding_data (GTlsConnectionBase      *tls,
                                                  GTlsChannelBindingType   type,
                                                  GByteArray              *data,
                                                  GError                 **error)
{
  GTlsConnectionGnutls *gnutls = G_TLS_CONNECTION_GNUTLS (tls);

  switch (type)
    {
    case G_TLS_CHANNEL_BINDING_TLS_UNIQUE:
      return gnutls_get_binding_tls_unique (gnutls, data, error);
    case G_TLS_CHANNEL_BINDING_TLS_SERVER_END_POINT:
      return gnutls_get_binding_tls_server_end_point (gnutls, data, error);
    case G_TLS_CHANNEL_BINDING_TLS_EXPORTER:
      return gnutls_get_binding_tls_exporter (gnutls, data, error);
    default:
      /* Anyone to implement tls-unique-for-telnet? */
      g_set_error (error, G_TLS_CHANNEL_BINDING_ERROR, G_TLS_CHANNEL_BINDING_ERROR_NOT_IMPLEMENTED,
                   _("Requested channel binding type is not implemented"));
    }
  return FALSE;
}

static GTlsConnectionBaseStatus
g_tls_connection_gnutls_read (GTlsConnectionBase  *tls,
                              void                *buffer,
                              gsize                count,
                              gint64               timeout,
                              gssize              *nread,
                              GCancellable        *cancellable,
                              GError             **error)
{
  GTlsConnectionGnutls *gnutls = G_TLS_CONNECTION_GNUTLS (tls);
  GTlsConnectionGnutlsPrivate *priv = g_tls_connection_gnutls_get_instance_private (gnutls);
  GTlsConnectionBaseStatus status;
  gssize ret;

  BEGIN_GNUTLS_IO (gnutls, G_IO_IN, timeout, cancellable);
  ret = gnutls_record_recv (priv->session, buffer, count);
  END_GNUTLS_IO (gnutls, G_IO_IN, ret, status, N_("Error reading data from TLS socket"), error);

  *nread = MAX (ret, 0);
  return status;
}

static gsize
input_vectors_from_gnutls_datum_t (GInputVector         *vectors,
                                   guint                 num_vectors,
                                   const gnutls_datum_t *datum)
{
  guint i;
  gsize total = 0;

  /* Copy into the receive vectors. */
  for (i = 0; i < num_vectors && total < datum->size; i++)
    {
      gsize count;
      GInputVector *vec = &vectors[i];

      count = MIN (vec->size, datum->size - total);

      memcpy (vec->buffer, datum->data + total, count);
      total += count;
    }

  g_assert (total <= datum->size);

  return total;
}

static GTlsConnectionBaseStatus
g_tls_connection_gnutls_read_message (GTlsConnectionBase  *tls,
                                      GInputVector        *vectors,
                                      guint                num_vectors,
                                      gint64               timeout,
                                      gssize              *nread,
                                      GCancellable        *cancellable,
                                      GError             **error)
{
  GTlsConnectionGnutls *gnutls = G_TLS_CONNECTION_GNUTLS (tls);
  GTlsConnectionGnutlsPrivate *priv = g_tls_connection_gnutls_get_instance_private (gnutls);
  GTlsConnectionBaseStatus status;
  gssize ret;
  gnutls_packet_t packet = { 0, };

  BEGIN_GNUTLS_IO (gnutls, G_IO_IN, timeout, cancellable);

  /* Receive the entire datagram (zero-copy). */
  ret = gnutls_record_recv_packet (priv->session, &packet);

  if (ret > 0)
    {
      gnutls_datum_t data = { 0, };

      gnutls_packet_get (packet, &data, NULL);
      ret = input_vectors_from_gnutls_datum_t (vectors, num_vectors, &data);
      gnutls_packet_deinit (packet);
    }

  END_GNUTLS_IO (gnutls, G_IO_IN, ret, status, N_("Error reading data from TLS socket"), error);

  *nread = MAX (ret, 0);
  return status;
}

static GTlsConnectionBaseStatus
g_tls_connection_gnutls_write (GTlsConnectionBase  *tls,
                               const void          *buffer,
                               gsize                count,
                               gint64               timeout,
                               gssize              *nwrote,
                               GCancellable        *cancellable,
                               GError             **error)
{
  GTlsConnectionGnutls *gnutls = G_TLS_CONNECTION_GNUTLS (tls);
  GTlsConnectionGnutlsPrivate *priv = g_tls_connection_gnutls_get_instance_private (gnutls);
  GTlsConnectionBaseStatus status;
  gssize ret;

  BEGIN_GNUTLS_IO (gnutls, G_IO_OUT, timeout, cancellable);
  ret = gnutls_record_send (priv->session, buffer, count);
  END_GNUTLS_IO (gnutls, G_IO_OUT, ret, status, N_("Error writing data to TLS socket"), error);

  *nwrote = MAX (ret, 0);
  return status;
}

static GTlsConnectionBaseStatus
g_tls_connection_gnutls_write_message (GTlsConnectionBase  *tls,
                                       GOutputVector       *vectors,
                                       guint                num_vectors,
                                       gint64               timeout,
                                       gssize              *nwrote,
                                       GCancellable        *cancellable,
                                       GError             **error)
{
  GTlsConnectionGnutls *gnutls = G_TLS_CONNECTION_GNUTLS (tls);
  GTlsConnectionGnutlsPrivate *priv = g_tls_connection_gnutls_get_instance_private (gnutls);
  GTlsConnectionBaseStatus status;
  gssize ret;
  guint i;
  gsize total_message_size;

  /* Calculate the total message size and check it’s not too big. */
  for (i = 0, total_message_size = 0; i < num_vectors; i++)
    total_message_size += vectors[i].size;

  if (g_tls_connection_base_is_dtls (tls) &&
      gnutls_dtls_get_data_mtu (priv->session) < total_message_size)
    {
      char *message;
      guint mtu = gnutls_dtls_get_data_mtu (priv->session);

      message = g_strdup_printf("%s %s",
                                ngettext ("Message of size %lu byte is too large for DTLS connection",
                                          "Message of size %lu bytes is too large for DTLS connection", total_message_size),
                                ngettext ("(maximum is %u byte)", "(maximum is %u bytes)", mtu));
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_MESSAGE_TOO_LARGE,
                   message,
                   total_message_size,
                   mtu);
      g_free (message);

      return G_TLS_CONNECTION_BASE_ERROR;
    }

  /* Queue up the data from all the vectors. */
  gnutls_record_cork (priv->session);

  for (i = 0; i < num_vectors; i++)
    {
      ret = gnutls_record_send (priv->session,
                                vectors[i].buffer, vectors[i].size);

      if (ret < 0 || ret < vectors[i].size)
        {
          /* Uncork to restore state, then bail. The peer will receive a
           * truncated datagram. */
          break;
        }
    }

  BEGIN_GNUTLS_IO (gnutls, G_IO_OUT, timeout, cancellable);
  ret = gnutls_record_uncork (priv->session, 0  /* flags */);
  END_GNUTLS_IO (gnutls, G_IO_OUT, ret, status, N_("Error writing data to TLS socket"), error);

  *nwrote = MAX (ret, 0);
  return status;
}

static GTlsConnectionBaseStatus
g_tls_connection_gnutls_close (GTlsConnectionBase  *tls,
                               gint64               timeout,
                               GCancellable        *cancellable,
                               GError             **error)
{
  GTlsConnectionGnutls *gnutls = G_TLS_CONNECTION_GNUTLS (tls);
  GTlsConnectionGnutlsPrivate *priv = g_tls_connection_gnutls_get_instance_private (gnutls);
  GTlsConnectionBaseStatus status;
  int ret;

  BEGIN_GNUTLS_IO (gnutls, G_IO_IN | G_IO_OUT, timeout, cancellable);
  ret = gnutls_bye (priv->session, GNUTLS_SHUT_WR);
  END_GNUTLS_IO (gnutls, G_IO_IN | G_IO_OUT, ret, status, N_("Error performing TLS close: %s"), error);

  return status;
}

static void
initialize_gnutls_priority (void)
{
  const gchar *priority_override;
  const gchar *error_pos = NULL;
  int ret;

  g_assert (!priority);

  priority_override = g_getenv ("G_TLS_GNUTLS_PRIORITY");
  if (priority_override)
    {
      ret = gnutls_priority_init2 (&priority, priority_override, &error_pos, 0);
      if (ret != GNUTLS_E_SUCCESS)
        g_warning ("Failed to set GnuTLS session priority with beginning at %s: %s", error_pos, gnutls_strerror (ret));
      return;
    }

  ret = gnutls_priority_init2 (&priority, "%COMPAT", &error_pos, GNUTLS_PRIORITY_INIT_DEF_APPEND);
  if (ret != GNUTLS_E_SUCCESS)
    g_warning ("Failed to set GnuTLS session priority with error beginning at %s: %s", error_pos, gnutls_strerror (ret));
}

static void
g_tls_connection_gnutls_class_init (GTlsConnectionGnutlsClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GTlsConnectionBaseClass *base_class = G_TLS_CONNECTION_BASE_CLASS (klass);

  gobject_class->finalize                                = g_tls_connection_gnutls_finalize;

  base_class->prepare_handshake                          = g_tls_connection_gnutls_prepare_handshake;
  base_class->handshake_thread_safe_renegotiation_status = g_tls_connection_gnutls_handshake_thread_safe_renegotiation_status;
  base_class->handshake_thread_request_rehandshake       = g_tls_connection_gnutls_handshake_thread_request_rehandshake;
  base_class->handshake_thread_handshake                 = g_tls_connection_gnutls_handshake_thread_handshake;
  base_class->retrieve_peer_certificate                  = g_tls_connection_gnutls_retrieve_peer_certificate;
  base_class->verify_chain                               = g_tls_connection_gnutls_verify_chain;
  base_class->complete_handshake                         = g_tls_connection_gnutls_complete_handshake;
  base_class->is_session_resumed                         = g_tls_connection_gnutls_is_session_resumed;
  base_class->get_channel_binding_data                   = g_tls_connection_gnutls_get_channel_binding_data;
  base_class->read_fn                                    = g_tls_connection_gnutls_read;
  base_class->read_message_fn                            = g_tls_connection_gnutls_read_message;
  base_class->write_fn                                   = g_tls_connection_gnutls_write;
  base_class->write_message_fn                           = g_tls_connection_gnutls_write_message;
  base_class->close_fn                                   = g_tls_connection_gnutls_close;

  initialize_gnutls_priority ();
}

static void
g_tls_connection_gnutls_initable_iface_init (GInitableIface *iface)
{
  iface->init = g_tls_connection_gnutls_initable_init;
}
