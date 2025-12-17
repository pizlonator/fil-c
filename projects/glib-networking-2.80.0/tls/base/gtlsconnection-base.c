/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2009-2011 Red Hat, Inc
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

#include "gtlsconnection-base.h"
#include "gtlsinputstream.h"
#include "gtlslog.h"
#include "gtlsoutputstream.h"

#include <glib/gi18n-lib.h>
#include <glib/gprintf.h>

/*
 * GTlsConnectionBase is the base abstract implementation of TLS and DTLS
 * support, for both the client and server side of a connection. The choice
 * between TLS and DTLS is made by setting the base-io-stream or
 * base-socket properties — exactly one of them must be set at
 * construction time.
 *
 * Client- and server-specific code is in the client and server concrete
 * subclasses, although the line about where code is put is a little blurry,
 * and there are various places in GTlsConnectionBase which check
 * G_IS_TLS_CLIENT_CONNECTION(self) to switch to a client-only code path.
 *
 * This abstract class implements a lot of interfaces:
 *  • Derived from GTlsConnection (itself from GIOStream), for TLS and streaming
 *    communications.
 *  • Implements GDtlsConnection and GDatagramBased, for DTLS and datagram
 *    communications.
 *  • Implements GInitable for failable initialisation.
 */

typedef struct
{
  /* When operating in stream mode, as a GTlsConnection. These are
   * mutually-exclusive with base_socket. There are two different
   * GIOStreams here: (a) base_io_stream and (b) the GTlsConnection
   * itself. base_io_stream is the GIOStream used to create the GTlsConnection,
   * and corresponds to the GTlsConnection::base-io-stream property.
   * base_istream and base_ostream are the GInputStream and GOutputStream,
   * respectively, of base_io_stream. These are for the underlying sockets that
   * don't know about TLS.
   *
   * Then the GTlsConnection also has tls_istream and tls_ostream, which
   * wrap the aforementioned base streams with a TLS session.
   *
   * When operating in datagram mode, none of these are used.
   */
  GIOStream             *base_io_stream;
  GPollableInputStream  *base_istream;
  GPollableOutputStream *base_ostream;
  GInputStream          *tls_istream;
  GOutputStream         *tls_ostream;

  /* When operating in datagram mode, as a GDtlsConnection, the
   * GTlsConnection is itself the DTLS GDatagramBased. It uses base_socket
   * for the underlying I/O. It is mutually-exclusive with base_io_stream and
   * the other streams.
   */
  GDatagramBased        *base_socket;

  GTlsDatabase          *database;
  GTlsInteraction       *interaction;

  GTlsCertificate       *certificate;
  gboolean               missing_requested_client_certificate;
  GError                *interaction_error;
  GTlsCertificate       *peer_certificate;
  GTlsCertificateFlags   peer_certificate_errors;

  GMutex                 verify_certificate_mutex;
  GCond                  verify_certificate_condition;
  gboolean               peer_certificate_accepted;
  gboolean               peer_certificate_examined;

  gboolean               require_close_notify;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  GTlsRehandshakeMode    rehandshake_mode;
G_GNUC_END_IGNORE_DEPRECATIONS

  /* need_handshake means the next claim_op() will get diverted into
   * an implicit handshake (unless it's an OP_HANDSHAKE or OP_CLOSE*).
   * need_finish_handshake means the next claim_op() will get diverted
   * into finish_handshake() (unless it's an OP_CLOSE*).
   *
   * handshaking is TRUE as soon as a handshake thread is queued. For
   * a sync handshake it becomes FALSE after finish_handshake()
   * completes in the calling thread, but for an async implicit
   * handshake, it becomes FALSE (and need_finish_handshake becomes
   * TRUE) at the end of the handshaking thread (and then the next
   * non-close op will call finish_handshake()). We can't just wait
   * for async_handshake_thread_completed() to run, because it's
   * possible that its main loop is being blocked by a synchronous op
   * which is waiting for handshaking to become FALSE...
   *
   * started_handshake indicates that the current handshake attempt
   * got at least as far as sending the first handshake packet (and so
   * any error should be copied to handshake_error and returned on all
   * future operations). ever_handshaked indicates that TLS has been
   * successfully negotiated at some point.
   */
  gboolean       need_handshake;
  gboolean       need_finish_handshake;
  gboolean       sync_handshake_in_progress;
  gboolean       started_handshake;
  gboolean       handshaking;
  gboolean       ever_handshaked;
  GMainContext  *handshake_context;
  GTask         *implicit_handshake;
  GError        *handshake_error;
  GByteArray    *app_data_buf;

  /* read_closed means the read direction has closed; write_closed similarly.
   * If (and only if) both are set, the entire GTlsConnection is closed. */
  gboolean       read_closing, read_closed;
  gboolean       write_closing, write_closed;

  gboolean       reading;
  gint64         read_timeout;
  GError        *read_error;
  GCancellable  *read_cancellable;

  gboolean       writing;
  gint64         write_timeout;
  GError        *write_error;
  GCancellable  *write_cancellable;

  gboolean       successful_read_op;

  gboolean       is_system_certdb;
  gboolean       database_is_unset;

  GMutex         op_mutex;
  GCancellable  *waiting_for_op;

  gchar        **advertised_protocols;
  gchar         *negotiated_protocol;

  GTlsProtocolVersion  protocol_version;
  gchar               *ciphersuite_name;

  gchar       *session_id;
  gboolean     session_resumption_enabled;
} GTlsConnectionBasePrivate;

static void g_tls_connection_base_dtls_connection_iface_init (GDtlsConnectionInterface *iface);

static void g_tls_connection_base_datagram_based_iface_init  (GDatagramBasedInterface  *iface);

static gboolean do_implicit_handshake (GTlsConnectionBase  *tls,
                                       gint64               timeout,
                                       GCancellable        *cancellable,
                                       GError             **error);

static gboolean finish_handshake (GTlsConnectionBase  *tls,
                                  GTask               *task,
                                  GError             **error);

static void g_tls_connection_base_handshake_async (GTlsConnection      *conn,
                                                   int                  io_priority,
                                                   GCancellable        *cancellable,
                                                   GAsyncReadyCallback  callback,
                                                   gpointer             user_data);

static gboolean g_tls_connection_base_handshake (GTlsConnection   *conn,
                                                 GCancellable     *cancellable,
                                                 GError          **error);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GTlsConnectionBase, g_tls_connection_base, G_TYPE_TLS_CONNECTION,
                                  G_ADD_PRIVATE (GTlsConnectionBase);
                                  G_IMPLEMENT_INTERFACE (G_TYPE_DATAGRAM_BASED,
                                                         g_tls_connection_base_datagram_based_iface_init);
                                  G_IMPLEMENT_INTERFACE (G_TYPE_DTLS_CONNECTION,
                                                         g_tls_connection_base_dtls_connection_iface_init);
                                  );


enum
{
  PROP_0,
  /* For this class: */
  PROP_BASE_IO_STREAM,
  PROP_BASE_SOCKET,
  /* For GTlsConnection and GDtlsConnection: */
  PROP_REQUIRE_CLOSE_NOTIFY,
  PROP_REHANDSHAKE_MODE,
  PROP_USE_SYSTEM_CERTDB,
  PROP_DATABASE,
  PROP_CERTIFICATE,
  PROP_INTERACTION,
  PROP_PEER_CERTIFICATE,
  PROP_PEER_CERTIFICATE_ERRORS,
  PROP_ADVERTISED_PROTOCOLS,
  PROP_NEGOTIATED_PROTOCOL,
  PROP_PROTOCOL_VERSION,
  PROP_CIPHERSUITE_NAME,
  PROP_SESSION_RESUMPTION_ENABLED,
  PROP_SESSION_REUSED
};

gboolean
g_tls_connection_base_is_dtls (GTlsConnectionBase *tls)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

  return priv->base_socket != NULL;
}

gboolean
g_tls_connection_base_get_session_resumption (GTlsConnectionBase *tls)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);
  return priv->session_resumption_enabled;
}

void
g_tls_connection_base_set_session_resumption (GTlsConnectionBase *tls, gboolean session_resumption_enabled)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);
  priv->session_resumption_enabled = session_resumption_enabled;
}

static void
g_tls_connection_base_init (GTlsConnectionBase *tls)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

  priv->need_handshake = TRUE;
  priv->database_is_unset = TRUE;
  priv->is_system_certdb = TRUE;

  /* The testsuite expects handshakes to actually happen. E.g. a test might
   * check to see that a handshake succeeds and then later check that a new
   * handshake fails. If we get really unlucky and the same port number is
   * reused for the server socket between connections, then we'll accidentally
   * resume the old session and skip certificate verification. Such failures
   * are difficult to debug because they require running the tests hundreds of
   * times simultaneously to reproduce (the port number does not get reused
   * quickly enough if the tests are run sequentially).
   *
   * On top of that if using a hostname the session id would be used for all
   * the connections in the tests.
   *
   * This variable allows tests to enable session resumption only when needed
   * whilst keeping the feature enabled for other uses of the library.
   */
  priv->session_resumption_enabled = !g_test_initialized ();

  g_mutex_init (&priv->verify_certificate_mutex);
  g_cond_init (&priv->verify_certificate_condition);

  g_mutex_init (&priv->op_mutex);

  priv->waiting_for_op = g_cancellable_new ();
}

static void
g_tls_connection_base_finalize (GObject *object)
{
  GTlsConnectionBase *tls = G_TLS_CONNECTION_BASE (object);
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

  g_clear_object (&priv->base_io_stream);
  g_clear_object (&priv->base_socket);

  g_clear_object (&priv->tls_istream);
  g_clear_object (&priv->tls_ostream);

  g_clear_object (&priv->database);
  g_clear_object (&priv->certificate);
  g_clear_error (&priv->interaction_error);
  g_clear_object (&priv->peer_certificate);

  g_mutex_clear (&priv->verify_certificate_mutex);
  g_cond_clear (&priv->verify_certificate_condition);

  g_clear_object (&priv->interaction);

  g_clear_pointer (&priv->handshake_context, g_main_context_unref);

  /* This must always be NULL at this point, as it holds a reference to @tls as
   * its source object. However, we clear it anyway just in case this changes
   * in future. */
  g_clear_object (&priv->implicit_handshake);

  g_clear_error (&priv->handshake_error);
  g_clear_error (&priv->read_error);
  g_clear_error (&priv->write_error);
  g_clear_object (&priv->read_cancellable);
  g_clear_object (&priv->write_cancellable);

  g_clear_object (&priv->waiting_for_op);
  g_mutex_clear (&priv->op_mutex);

  g_clear_pointer (&priv->app_data_buf, g_byte_array_unref);

  g_clear_pointer (&priv->advertised_protocols, g_strfreev);
  g_clear_pointer (&priv->negotiated_protocol, g_free);

  g_clear_pointer (&priv->ciphersuite_name, g_free);

  g_free (priv->session_id);

  G_OBJECT_CLASS (g_tls_connection_base_parent_class)->finalize (object);
}

static void
g_tls_connection_base_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GTlsConnectionBase *tls = G_TLS_CONNECTION_BASE (object);
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);
  GTlsBackend *backend;

  switch (prop_id)
    {
    case PROP_BASE_IO_STREAM:
      g_value_set_object (value, priv->base_io_stream);
      break;

    case PROP_BASE_SOCKET:
      g_value_set_object (value, priv->base_socket);
      break;

    case PROP_REQUIRE_CLOSE_NOTIFY:
      g_value_set_boolean (value, priv->require_close_notify);
      break;

    case PROP_REHANDSHAKE_MODE:
      g_value_set_enum (value, priv->rehandshake_mode);
      break;

    case PROP_USE_SYSTEM_CERTDB:
      g_value_set_boolean (value, priv->is_system_certdb);
      break;

    case PROP_DATABASE:
      if (priv->database_is_unset)
        {
          backend = g_tls_backend_get_default ();
          priv->database =  g_tls_backend_get_default_database (backend);
          priv->database_is_unset = FALSE;
        }
      g_value_set_object (value, priv->database);
      break;

    case PROP_CERTIFICATE:
      g_value_set_object (value, priv->certificate);
      break;

    case PROP_INTERACTION:
      g_value_set_object (value, priv->interaction);
      break;

    case PROP_PEER_CERTIFICATE:
      g_value_set_object (value, priv->peer_certificate);
      break;

    case PROP_PEER_CERTIFICATE_ERRORS:
      g_value_set_flags (value, priv->peer_certificate_errors);
      break;

    case PROP_ADVERTISED_PROTOCOLS:
      g_value_set_boxed (value, priv->advertised_protocols);
      break;

    case PROP_NEGOTIATED_PROTOCOL:
      g_value_set_string (value, priv->negotiated_protocol);
      break;

    case PROP_PROTOCOL_VERSION:
      g_value_set_enum (value, priv->protocol_version);
      break;

    case PROP_CIPHERSUITE_NAME:
      g_value_set_string (value, priv->ciphersuite_name);
      break;

    case PROP_SESSION_REUSED:
      g_value_set_boolean (value, FALSE);
      break;
    
    case PROP_SESSION_RESUMPTION_ENABLED:
      g_value_set_boolean (value, priv->session_resumption_enabled);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_tls_connection_base_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GTlsConnectionBase *tls = G_TLS_CONNECTION_BASE (object);
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);
  GInputStream *istream;
  GOutputStream *ostream;
  gboolean system_certdb;
  GTlsBackend *backend;

  switch (prop_id)
    {
    case PROP_BASE_IO_STREAM:
      g_assert (!g_value_get_object (value) || !priv->base_socket);

      if (priv->base_io_stream)
        {
          g_object_unref (priv->base_io_stream);
          priv->base_istream = NULL;
          priv->base_ostream = NULL;
        }
      priv->base_io_stream = g_value_dup_object (value);
      if (!priv->base_io_stream)
        return;

      istream = g_io_stream_get_input_stream (priv->base_io_stream);
      ostream = g_io_stream_get_output_stream (priv->base_io_stream);

      if (G_IS_POLLABLE_INPUT_STREAM (istream) &&
          g_pollable_input_stream_can_poll (G_POLLABLE_INPUT_STREAM (istream)))
        {
          priv->base_istream = G_POLLABLE_INPUT_STREAM (istream);
          priv->tls_istream = g_tls_input_stream_new (tls);
        }
      if (G_IS_POLLABLE_OUTPUT_STREAM (ostream) &&
          g_pollable_output_stream_can_poll (G_POLLABLE_OUTPUT_STREAM (ostream)))
        {
          priv->base_ostream = G_POLLABLE_OUTPUT_STREAM (ostream);
          priv->tls_ostream = g_tls_output_stream_new (tls);
        }
      break;

    case PROP_BASE_SOCKET:
      g_assert (!g_value_get_object (value) || !priv->base_io_stream);

      g_clear_object (&priv->base_socket);
      priv->base_socket = g_value_dup_object (value);
      break;

    case PROP_REQUIRE_CLOSE_NOTIFY:
      priv->require_close_notify = g_value_get_boolean (value);
      break;

    case PROP_REHANDSHAKE_MODE:
      priv->rehandshake_mode = g_value_get_enum (value);
      break;

    case PROP_USE_SYSTEM_CERTDB:
      system_certdb = g_value_get_boolean (value);
      if (system_certdb != priv->is_system_certdb)
        {
          g_clear_object (&priv->database);
          if (system_certdb)
            {
              backend = g_tls_backend_get_default ();
              priv->database = g_tls_backend_get_default_database (backend);
            }
          priv->is_system_certdb = system_certdb;
          priv->database_is_unset = FALSE;
        }
      break;

    case PROP_DATABASE:
      g_clear_object (&priv->database);
      priv->database = g_value_dup_object (value);
      priv->is_system_certdb = FALSE;
      priv->database_is_unset = FALSE;
      break;

    case PROP_CERTIFICATE:
      if (priv->certificate)
        g_object_unref (priv->certificate);
      priv->certificate = g_value_dup_object (value);
      break;

    case PROP_INTERACTION:
      g_clear_object (&priv->interaction);
      priv->interaction = g_value_dup_object (value);
      break;

    case PROP_ADVERTISED_PROTOCOLS:
      g_clear_pointer (&priv->advertised_protocols, g_strfreev);
      priv->advertised_protocols = g_value_dup_boxed (value);
      break;

    case PROP_SESSION_REUSED:
      g_assert_not_reached ();
      break;

    case PROP_SESSION_RESUMPTION_ENABLED:
      priv->session_resumption_enabled = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

typedef enum {
  G_TLS_CONNECTION_BASE_OP_HANDSHAKE,
  G_TLS_CONNECTION_BASE_OP_READ,
  G_TLS_CONNECTION_BASE_OP_WRITE,
  G_TLS_CONNECTION_BASE_OP_CLOSE_READ,
  G_TLS_CONNECTION_BASE_OP_CLOSE_WRITE,
  G_TLS_CONNECTION_BASE_OP_CLOSE_BOTH,
} GTlsConnectionBaseOp;

static const gchar *
op_to_string (GTlsConnectionBaseOp op)
{
  switch (op)
    {
    case G_TLS_CONNECTION_BASE_OP_HANDSHAKE:
      return "OP_HANDSHAKE";
    case G_TLS_CONNECTION_BASE_OP_READ:
      return "OP_READ";
    case G_TLS_CONNECTION_BASE_OP_WRITE:
      return "OP_WRITE";
    case G_TLS_CONNECTION_BASE_OP_CLOSE_READ:
      return "OP_CLOSE_READ";
    case G_TLS_CONNECTION_BASE_OP_CLOSE_WRITE:
      return "OP_CLOSE_WRITE";
    case G_TLS_CONNECTION_BASE_OP_CLOSE_BOTH:
      return "OP_CLOSE_BOTH";
    }

  g_assert_not_reached ();

  return "UNKNOWN_OP";
}

static const gchar *
status_to_string (GTlsConnectionBaseStatus st)
{
  switch (st)
    {
    case G_TLS_CONNECTION_BASE_OK:
      return "BASE_OK";
    case G_TLS_CONNECTION_BASE_WOULD_BLOCK:
      return "WOULD_BLOCK";
    case G_TLS_CONNECTION_BASE_TIMED_OUT:
      return "TIMED_OUT";
    case G_TLS_CONNECTION_BASE_REHANDSHAKE:
      return "REHANDSHAKE";
    case G_TLS_CONNECTION_BASE_TRY_AGAIN:
      return "TRY_AGAIN";
    case G_TLS_CONNECTION_BASE_ERROR:
      return "ERROR";
    }

  g_assert_not_reached ();

  return "UNKNOWN_STATUS";
}

static gboolean
claim_op (GTlsConnectionBase    *tls,
          GTlsConnectionBaseOp   op,
          gint64                 timeout,
          GCancellable          *cancellable,
          GError               **error)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

  g_tls_log_debug (tls, "claiming operation %s", op_to_string (op));

 try_again:
  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    {
      g_tls_log_debug (tls, "claim_op failed: cancelled");
      return FALSE;
    }

  g_mutex_lock (&priv->op_mutex);

  if (((op == G_TLS_CONNECTION_BASE_OP_HANDSHAKE ||
        op == G_TLS_CONNECTION_BASE_OP_READ) &&
       (priv->read_closing || priv->read_closed)) ||
      ((op == G_TLS_CONNECTION_BASE_OP_HANDSHAKE ||
        op == G_TLS_CONNECTION_BASE_OP_WRITE) &&
       (priv->write_closing || priv->write_closed)))
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CLOSED,
                           _("Connection is closed"));
      g_mutex_unlock (&priv->op_mutex);
      g_tls_log_debug (tls, "claim_op failed: connection is closed");
      return FALSE;
    }

  if (priv->handshake_error &&
      op != G_TLS_CONNECTION_BASE_OP_CLOSE_BOTH &&
      op != G_TLS_CONNECTION_BASE_OP_CLOSE_READ &&
      op != G_TLS_CONNECTION_BASE_OP_CLOSE_WRITE)
    {
      if (error)
        *error = g_error_copy (priv->handshake_error);
      g_mutex_unlock (&priv->op_mutex);
      g_tls_log_debug (tls, "claim_op failed: %s", priv->handshake_error->message);
      return FALSE;
    }

  if (op != G_TLS_CONNECTION_BASE_OP_HANDSHAKE)
    {
      if (op != G_TLS_CONNECTION_BASE_OP_CLOSE_BOTH &&
          op != G_TLS_CONNECTION_BASE_OP_CLOSE_READ &&
          op != G_TLS_CONNECTION_BASE_OP_CLOSE_WRITE &&
          priv->need_handshake && !priv->handshaking)
        {
          priv->handshaking = TRUE;
          if (!do_implicit_handshake (tls, timeout, cancellable, error))
            {
              g_mutex_unlock (&priv->op_mutex);
              g_tls_log_debug (tls, "claim_op failed: implicit handshake required");
              return FALSE;
            }
        }

      if (priv->need_finish_handshake &&
          priv->implicit_handshake)
        {
          GError *my_error = NULL;
          gboolean success;

          priv->need_finish_handshake = FALSE;

          g_mutex_unlock (&priv->op_mutex);
          success = finish_handshake (tls, priv->implicit_handshake, &my_error);
          g_clear_object (&priv->implicit_handshake);
          g_clear_pointer (&priv->handshake_context, g_main_context_unref);
          g_mutex_lock (&priv->op_mutex);

          if (op != G_TLS_CONNECTION_BASE_OP_CLOSE_BOTH &&
              op != G_TLS_CONNECTION_BASE_OP_CLOSE_READ &&
              op != G_TLS_CONNECTION_BASE_OP_CLOSE_WRITE &&
              (!success || g_cancellable_set_error_if_cancelled (cancellable, &my_error)))
            {
              g_propagate_error (error, my_error);
              g_mutex_unlock (&priv->op_mutex);
              g_tls_log_debug (tls, "claim_op failed: finish_handshake failed or operation has been cancelled");
              return FALSE;
            }

          g_clear_error (&my_error);
        }
    }

  if (priv->handshaking &&
      timeout != 0 &&
      g_main_context_is_owner (priv->handshake_context))
    {
      /* Cannot perform a blocking operation during a handshake on the
       * same thread that triggered the handshake. The only way this can
       * occur is if the application is doing something weird in its
       * accept-certificate callback. Allowing a blocking op would stall
       * the handshake (forever, if there's no timeout). Even a close
       * op would deadlock here.
       */
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, _("Cannot perform blocking operation during TLS handshake"));
      g_mutex_unlock (&priv->op_mutex);
      g_tls_log_debug (tls, "claim_op failed: cannot perform blocking operation during TLS handshake");
      return FALSE;
    }

  if ((op != G_TLS_CONNECTION_BASE_OP_WRITE && priv->reading) ||
      (op != G_TLS_CONNECTION_BASE_OP_READ && priv->writing) ||
      (op != G_TLS_CONNECTION_BASE_OP_HANDSHAKE && priv->handshaking))
    {
      GPollFD fds[2];
      int nfds;
      gint64 start_time;
      gint result = 1; /* if the loop is never entered, it's as if we cancelled early */

      g_cancellable_reset (priv->waiting_for_op);

      g_mutex_unlock (&priv->op_mutex);

      if (timeout == 0)
        {
          /* Intentionally not translated because this is not a fatal error to be
           * presented to the user, and to avoid this showing up in profiling. */
          g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK, "Operation would block");
          g_tls_log_debug (tls, "claim_op failed: operation would block");
          return FALSE;
        }

      g_cancellable_make_pollfd (priv->waiting_for_op, &fds[0]);
      if (g_cancellable_make_pollfd (cancellable, &fds[1]))
        nfds = 2;
      else
        nfds = 1;

      /* Convert from microseconds to milliseconds. */
      if (timeout != -1)
        timeout /= 1000;

      /* Poll until cancellation or the timeout is reached. */
      start_time = g_get_monotonic_time ();

      while (!g_cancellable_is_cancelled (priv->waiting_for_op) &&
             !g_cancellable_is_cancelled (cancellable))
        {
          result = g_poll (fds, nfds, timeout);

          if (result == 0)
            break;
          if (result != -1 || errno != EINTR)
            continue;

          if (timeout != -1)
            {
              timeout -= (g_get_monotonic_time () - start_time) / 1000;
              if (timeout < 0)
                timeout = 0;
            }
        }

      if (nfds > 1)
        g_cancellable_release_fd (cancellable);

      if (result == 0)
        {
          g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT,
                               _("Socket I/O timed out"));
          g_tls_log_debug (tls, "claim_op failed: socket I/O timed out");
          return FALSE;
        }

      goto try_again;
    }

  if (op == G_TLS_CONNECTION_BASE_OP_HANDSHAKE)
    priv->handshaking = TRUE;
  if (op == G_TLS_CONNECTION_BASE_OP_CLOSE_BOTH ||
      op == G_TLS_CONNECTION_BASE_OP_CLOSE_READ)
    priv->read_closing = TRUE;
  if (op == G_TLS_CONNECTION_BASE_OP_CLOSE_BOTH ||
      op == G_TLS_CONNECTION_BASE_OP_CLOSE_WRITE)
    priv->write_closing = TRUE;

  if (op != G_TLS_CONNECTION_BASE_OP_WRITE)
    priv->reading = TRUE;
  if (op != G_TLS_CONNECTION_BASE_OP_READ)
    priv->writing = TRUE;

  g_mutex_unlock (&priv->op_mutex);
  g_tls_log_debug (tls, "claiming operation %s succeeded", op_to_string (op));
  return TRUE;
}

static void
yield_op (GTlsConnectionBase       *tls,
          GTlsConnectionBaseOp      op,
          GTlsConnectionBaseStatus  status)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

  g_tls_log_debug (tls, "yielding operation %s", op_to_string (op));

  g_mutex_lock (&priv->op_mutex);

  if (op == G_TLS_CONNECTION_BASE_OP_HANDSHAKE)
    priv->handshaking = FALSE;
  else if (status == G_TLS_CONNECTION_BASE_REHANDSHAKE && !priv->handshaking)
    priv->need_handshake = TRUE;

  if (op == G_TLS_CONNECTION_BASE_OP_CLOSE_BOTH ||
      op == G_TLS_CONNECTION_BASE_OP_CLOSE_READ)
    priv->read_closing = FALSE;
  if (op == G_TLS_CONNECTION_BASE_OP_CLOSE_BOTH ||
      op == G_TLS_CONNECTION_BASE_OP_CLOSE_WRITE)
    priv->write_closing = FALSE;

  if (op != G_TLS_CONNECTION_BASE_OP_WRITE)
    priv->reading = FALSE;
  if (op != G_TLS_CONNECTION_BASE_OP_READ)
    priv->writing = FALSE;

  g_cancellable_cancel (priv->waiting_for_op);
  g_mutex_unlock (&priv->op_mutex);
}

static void
g_tls_connection_base_real_push_io (GTlsConnectionBase *tls,
                                    GIOCondition        direction,
                                    gint64              timeout,
                                    GCancellable       *cancellable)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

  if (direction & G_IO_IN)
    {
      priv->read_timeout = timeout;
      priv->read_cancellable = cancellable;
      g_clear_error (&priv->read_error);
    }

  if (direction & G_IO_OUT)
    {
      priv->write_timeout = timeout;
      priv->write_cancellable = cancellable;
      g_clear_error (&priv->write_error);
    }
}

void
g_tls_connection_base_push_io (GTlsConnectionBase *tls,
                               GIOCondition        direction,
                               gint64              timeout,
                               GCancellable       *cancellable)
{
  g_assert (direction & (G_IO_IN | G_IO_OUT));
  g_return_if_fail (G_IS_TLS_CONNECTION_BASE (tls));

  G_TLS_CONNECTION_BASE_GET_CLASS (tls)->push_io (tls, direction,
                                                  timeout, cancellable);
}

static GTlsConnectionBaseStatus
g_tls_connection_base_real_pop_io (GTlsConnectionBase  *tls,
                                   GIOCondition         direction,
                                   gboolean             success,
                                   GError             **error)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);
  GError *my_error = NULL;

  /* This function MAY or MAY NOT set error when it fails! */

  if (direction & G_IO_IN)
    {
      priv->read_cancellable = NULL;
      if (!success)
        {
          my_error = priv->read_error;
          priv->read_error = NULL;
        }
      else
        g_clear_error (&priv->read_error);
    }

  if (direction & G_IO_OUT)
    {
      priv->write_cancellable = NULL;
      if (!success && !my_error)
        {
          my_error = priv->write_error;
          priv->write_error = NULL;
        }
      else
        g_clear_error (&priv->write_error);
    }

  if (success)
    return G_TLS_CONNECTION_BASE_OK;

  if (g_error_matches (my_error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK))
    {
      g_propagate_error (error, my_error);
      return G_TLS_CONNECTION_BASE_WOULD_BLOCK;
    }

  if (g_error_matches (my_error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT))
    {
      g_propagate_error (error, my_error);
      return G_TLS_CONNECTION_BASE_TIMED_OUT;
    }

  if (priv->missing_requested_client_certificate &&
      !priv->successful_read_op)
    {
      g_assert (G_IS_TLS_CLIENT_CONNECTION (tls));

      /* Probably the server requires a client certificate, but we failed to
       * provide one. With TLS 1.3, the server is no longer able to tell us
       * this, so we just have to guess. If there is an error from the TLS
       * interaction (request for user certificate), we provide that. Otherwise,
       * guess that G_TLS_ERROR_CERTIFICATE_REQUIRED is probably appropriate.
       * This could be wrong, but only applies to the small minority of
       * connections where a client cert is requested but not provided, and then
       * then only if the client has never successfully read any data from the
       * connection. This should hopefully be a rare enough case that returning
       * G_TLS_ERROR_CERTIFICATE_REQUIRED incorrectly should not be common.
       * Beware that a successful write operation does *not* indicate that the
       * server has accepted our certificate: a write op can succeed on the
       * client side before the client notices that the server has closed the
       * connection.
       */
      if (priv->interaction_error)
        {
          g_propagate_error (error, priv->interaction_error);
          priv->interaction_error = NULL;
        }
      else
        {
          g_clear_error (error);
          g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_CERTIFICATE_REQUIRED,
                               _("Server required TLS certificate"));
        }
      g_clear_error (&my_error);
    }
  else if (my_error)
    {
      g_propagate_error (error, my_error);
    }

  return G_TLS_CONNECTION_BASE_ERROR;
}

GTlsConnectionBaseStatus
g_tls_connection_base_pop_io (GTlsConnectionBase  *tls,
                              GIOCondition         direction,
                              gboolean             success,
                              GError             **error)
{
  g_assert (direction & (G_IO_IN | G_IO_OUT));
  g_assert (!error || !*error);
  g_return_val_if_fail (G_IS_TLS_CONNECTION_BASE (tls), G_TLS_CONNECTION_BASE_ERROR);

  return G_TLS_CONNECTION_BASE_GET_CLASS (tls)->pop_io (tls, direction,
                                                        success, error);
}

/* Checks whether the underlying base stream or GDatagramBased meets
 * @condition. */
gboolean
g_tls_connection_base_base_check (GTlsConnectionBase *tls,
                                  GIOCondition        condition)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

  if (g_tls_connection_base_is_dtls (tls))
    return g_datagram_based_condition_check (priv->base_socket, condition);

  if (condition & G_IO_IN)
    return g_pollable_input_stream_is_readable (priv->base_istream);

  if (condition & G_IO_OUT)
    return g_pollable_output_stream_is_writable (priv->base_ostream);

  g_assert_not_reached ();
  return FALSE;
}

/* Checks whether the (D)TLS stream meets @condition; not the underlying base
 * stream or GDatagramBased. */
gboolean
g_tls_connection_base_check (GTlsConnectionBase  *tls,
                             GIOCondition         condition)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

  /* Racy, but worst case is that we just get WOULD_BLOCK back */
  if (priv->need_finish_handshake)
    return TRUE;

  /* If a handshake or close is in progress, then tls_istream and
   * tls_ostream are blocked, regardless of the base stream status.
   */
  if (priv->handshaking)
    return FALSE;

  if (((condition & G_IO_IN) && priv->read_closing) ||
      ((condition & G_IO_OUT) && priv->write_closing))
    return FALSE;

  /* Defer to the base stream or GDatagramBased. */
  return g_tls_connection_base_base_check (tls, condition);
}

typedef struct {
  GSource             source;

  GTlsConnectionBase *tls;

  /* Either a GDatagramBased (datagram mode), or a GPollableInputStream or
   * a GPollableOutputStream (streaming mode):
   */
  GObject            *base;

  GSource            *child_source;
  GIOCondition        condition;

  gboolean            io_waiting;
  gboolean            op_waiting;
} GTlsConnectionBaseSource;

/* Use a custom dummy callback instead of g_source_set_dummy_callback(), as that
 * uses a GClosure and is slow. (The GClosure is necessary to deal with any
 * function prototype.) */
static gboolean
dummy_callback (gpointer data)
{
  return G_SOURCE_CONTINUE;
}

static void
tls_source_sync (GTlsConnectionBaseSource *tls_source)
{
  GTlsConnectionBase *tls = tls_source->tls;
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);
  gboolean io_waiting, op_waiting;

  /* Was the source destroyed earlier in this main context iteration? */
  if (g_source_is_destroyed ((GSource *)tls_source))
    return;

  g_mutex_lock (&priv->op_mutex);
  if (((tls_source->condition & G_IO_IN) && priv->reading) ||
      ((tls_source->condition & G_IO_OUT) && priv->writing) ||
      (priv->handshaking && !priv->need_finish_handshake))
    op_waiting = TRUE;
  else
    op_waiting = FALSE;

  if (!op_waiting && !priv->need_handshake &&
      !priv->need_finish_handshake)
    io_waiting = TRUE;
  else
    io_waiting = FALSE;
  g_mutex_unlock (&priv->op_mutex);

  if (op_waiting == tls_source->op_waiting &&
      io_waiting == tls_source->io_waiting)
    return;
  tls_source->op_waiting = op_waiting;
  tls_source->io_waiting = io_waiting;

  if (tls_source->child_source)
    {
      g_source_remove_child_source ((GSource *)tls_source,
                                    tls_source->child_source);
      g_source_unref (tls_source->child_source);
    }

  if (op_waiting)
    tls_source->child_source = g_cancellable_source_new (priv->waiting_for_op);
  else if (io_waiting && G_IS_DATAGRAM_BASED (tls_source->base))
    tls_source->child_source = g_datagram_based_create_source (priv->base_socket, tls_source->condition, NULL);
  else if (io_waiting && G_IS_POLLABLE_INPUT_STREAM (tls_source->base))
    tls_source->child_source = g_pollable_input_stream_create_source (priv->base_istream, NULL);
  else if (io_waiting && G_IS_POLLABLE_OUTPUT_STREAM (tls_source->base))
    tls_source->child_source = g_pollable_output_stream_create_source (priv->base_ostream, NULL);
  else
    tls_source->child_source = g_timeout_source_new (0);

  g_source_set_callback (tls_source->child_source, dummy_callback, NULL, NULL);
  g_source_add_child_source ((GSource *)tls_source, tls_source->child_source);
}

static gboolean
tls_source_dispatch (GSource     *source,
                     GSourceFunc  callback,
                     gpointer     user_data)
{
  GDatagramBasedSourceFunc datagram_based_func = (GDatagramBasedSourceFunc)callback;
  GPollableSourceFunc pollable_func = (GPollableSourceFunc)callback;
  GTlsConnectionBaseSource *tls_source = (GTlsConnectionBaseSource *)source;
  gboolean ret;

  if (G_IS_DATAGRAM_BASED (tls_source->base))
    ret = (*datagram_based_func) (G_DATAGRAM_BASED (tls_source->base),
                                  tls_source->condition, user_data);
  else
    ret = (*pollable_func) (tls_source->base, user_data);

  if (ret)
    tls_source_sync (tls_source);

  return ret;
}

static void
tls_source_finalize (GSource *source)
{
  GTlsConnectionBaseSource *tls_source = (GTlsConnectionBaseSource *)source;

  g_object_unref (tls_source->tls);
  g_source_unref (tls_source->child_source);
}

static gboolean
g_tls_connection_tls_source_closure_callback (GObject  *stream,
                                              gpointer  data)
{
  GClosure *closure = data;

  GValue param = { 0, };
  GValue result_value = { 0, };
  gboolean result;

  g_value_init (&result_value, G_TYPE_BOOLEAN);

  g_value_init (&param, G_TYPE_OBJECT);
  g_value_set_object (&param, stream);

  g_closure_invoke (closure, &result_value, 1, &param, NULL);

  result = g_value_get_boolean (&result_value);
  g_value_unset (&result_value);
  g_value_unset (&param);

  return result;
}

static gboolean
g_tls_connection_tls_source_dtls_closure_callback (GDatagramBased *datagram_based,
                                                   GIOCondition    condition,
                                                   gpointer        data)
{
  GClosure *closure = data;

  GValue param[2] = { G_VALUE_INIT, G_VALUE_INIT };
  GValue result_value = G_VALUE_INIT;
  gboolean result;

  g_value_init (&result_value, G_TYPE_BOOLEAN);

  g_value_init (&param[0], G_TYPE_DATAGRAM_BASED);
  g_value_set_object (&param[0], datagram_based);
  g_value_init (&param[1], G_TYPE_IO_CONDITION);
  g_value_set_flags (&param[1], condition);

  g_closure_invoke (closure, &result_value, 2, param, NULL);

  result = g_value_get_boolean (&result_value);
  g_value_unset (&result_value);
  g_value_unset (&param[0]);
  g_value_unset (&param[1]);

  return result;
}

static GSourceFuncs tls_source_funcs =
{
  NULL,
  NULL,
  tls_source_dispatch,
  tls_source_finalize,
  (GSourceFunc)g_tls_connection_tls_source_closure_callback,
  (GSourceDummyMarshal)g_cclosure_marshal_generic
};

static GSourceFuncs dtls_source_funcs =
{
  NULL,
  NULL,
  tls_source_dispatch,
  tls_source_finalize,
  (GSourceFunc)g_tls_connection_tls_source_dtls_closure_callback,
  (GSourceDummyMarshal)g_cclosure_marshal_generic
};

GSource *
g_tls_connection_base_create_source (GTlsConnectionBase  *tls,
                                     GIOCondition         condition,
                                     GCancellable        *cancellable)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);
  GSource *source, *cancellable_source;
  GTlsConnectionBaseSource *tls_source;

  if (g_tls_connection_base_is_dtls (tls))
    {
      source = g_source_new (&dtls_source_funcs,
                             sizeof (GTlsConnectionBaseSource));
    }
  else
    {
      source = g_source_new (&tls_source_funcs,
                             sizeof (GTlsConnectionBaseSource));
    }
  g_source_set_static_name (source, "GTlsConnectionBaseSource");
  tls_source = (GTlsConnectionBaseSource *)source;
  tls_source->tls = g_object_ref (tls);
  tls_source->condition = condition;
  if (g_tls_connection_base_is_dtls (tls))
    tls_source->base = G_OBJECT (tls);
  else if (priv->tls_istream && condition & G_IO_IN)
    tls_source->base = G_OBJECT (priv->tls_istream);
  else if (priv->tls_ostream && condition & G_IO_OUT)
    tls_source->base = G_OBJECT (priv->tls_ostream);
  else
    g_assert_not_reached ();

  tls_source->op_waiting = (gboolean) -1;
  tls_source->io_waiting = (gboolean) -1;
  tls_source_sync (tls_source);

  if (cancellable)
    {
      cancellable_source = g_cancellable_source_new (cancellable);
      g_source_set_callback (cancellable_source, dummy_callback, NULL, NULL);
      g_source_add_child_source (source, cancellable_source);
      g_source_unref (cancellable_source);
    }

  return source;
}

static GSource *
g_tls_connection_base_dtls_create_source (GDatagramBased  *datagram_based,
                                          GIOCondition     condition,
                                          GCancellable    *cancellable)
{
  GTlsConnectionBase *tls = G_TLS_CONNECTION_BASE (datagram_based);

  return g_tls_connection_base_create_source (tls, condition, cancellable);
}

static GIOCondition
g_tls_connection_base_condition_check (GDatagramBased  *datagram_based,
                                         GIOCondition     condition)
{
  GTlsConnectionBase *tls = G_TLS_CONNECTION_BASE (datagram_based);

  return g_tls_connection_base_check (tls, condition) ? condition : 0;
}

static gboolean
g_tls_connection_base_condition_wait (GDatagramBased  *datagram_based,
                                      GIOCondition     condition,
                                      gint64           timeout,
                                      GCancellable    *cancellable,
                                      GError         **error)
{
  GTlsConnectionBase *tls = G_TLS_CONNECTION_BASE (datagram_based);
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);
  GPollFD fds[2];
  guint n_fds;
  gint result = 1; /* if the loop is never entered, it's as if we cancelled early */
  gint64 start_time;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;

  /* Convert from microseconds to milliseconds. */
  if (timeout != -1)
    timeout = timeout / 1000;

  start_time = g_get_monotonic_time ();

  g_cancellable_make_pollfd (priv->waiting_for_op, &fds[0]);
  n_fds = 1;

  if (g_cancellable_make_pollfd (cancellable, &fds[1]))
    n_fds++;

  while (!g_tls_connection_base_condition_check (datagram_based, condition) &&
         !g_cancellable_is_cancelled (cancellable))
    {
      result = g_poll (fds, n_fds, timeout);
      if (result == 0)
        break;
      if (result != -1 || errno != EINTR)
        continue;

      if (timeout != -1)
        {
          timeout -= (g_get_monotonic_time () - start_time) / 1000;
          if (timeout < 0)
            timeout = 0;
        }
    }

  if (n_fds > 1)
    g_cancellable_release_fd (cancellable);

  if (result == 0)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT,
                           _("Socket I/O timed out"));
      return FALSE;
    }

  return !g_cancellable_set_error_if_cancelled (cancellable, error);
}

static const gchar *
get_server_identity (GSocketConnectable *server_identity)
{
  if (G_IS_NETWORK_ADDRESS (server_identity))
    return g_network_address_get_hostname (G_NETWORK_ADDRESS (server_identity));
  else if (G_IS_NETWORK_SERVICE (server_identity))
    return g_network_service_get_domain (G_NETWORK_SERVICE (server_identity));
  else
    return NULL;
}

static GTlsCertificateFlags
verify_peer_certificate (GTlsConnectionBase *tls,
                         GTlsCertificate    *peer_certificate)
{
  GTlsConnectionBaseClass *tls_class = G_TLS_CONNECTION_BASE_GET_CLASS (tls);
  GSocketConnectable *peer_identity = NULL;
  GTlsDatabase *database;
  GTlsCertificateFlags errors = 0;
  gboolean is_client;

  is_client = G_IS_TLS_CLIENT_CONNECTION (tls);

  if (is_client)
    {
      if (!g_tls_connection_base_is_dtls (tls))
        peer_identity = g_tls_client_connection_get_server_identity (G_TLS_CLIENT_CONNECTION (tls));
      else
        peer_identity = g_dtls_client_connection_get_server_identity (G_DTLS_CLIENT_CONNECTION (tls));

      if (!peer_identity)
        errors |= G_TLS_CERTIFICATE_BAD_IDENTITY;
    }

  database = g_tls_connection_get_database (G_TLS_CONNECTION (tls));
  if (!database)
    {
      errors |= G_TLS_CERTIFICATE_UNKNOWN_CA;
      errors |= g_tls_certificate_verify (peer_certificate, peer_identity, NULL);
    }
  else
    {
      GError *error = NULL;

      g_assert (tls_class->verify_chain);
      errors |= tls_class->verify_chain (tls,
                                         peer_certificate,
                                         is_client ? G_TLS_DATABASE_PURPOSE_AUTHENTICATE_SERVER : G_TLS_DATABASE_PURPOSE_AUTHENTICATE_CLIENT,
                                         peer_identity,
                                         g_tls_connection_get_interaction (G_TLS_CONNECTION (tls)),
                                         G_TLS_DATABASE_VERIFY_NONE,
                                         NULL,
                                         &error);
      if (error)
        {
          g_tls_log_debug (tls, "failure verifying certificate chain: %s", error->message);
          g_assert (errors != 0);
          g_clear_error (&error);
        }
    }

  if (tls_class->verify_peer_certificate)
    errors |= tls_class->verify_peer_certificate (tls, peer_certificate, errors);

  return errors;
}

static gboolean
accept_or_reject_peer_certificate (gpointer user_data)
{
  GTlsConnectionBase *tls = user_data;
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);
  GTlsCertificate *peer_certificate = NULL;
  GTlsCertificateFlags peer_certificate_errors = 0;
  gboolean accepted = FALSE;

  /* This function must be called from the handshake context thread
   * (probably the main thread, NOT the handshake thread) because
   * it emits notifies that are application-visible.
   */
  g_assert (priv->handshake_context);
  g_assert (g_main_context_is_owner (priv->handshake_context));

  peer_certificate = G_TLS_CONNECTION_BASE_GET_CLASS (tls)->retrieve_peer_certificate (tls);

  if (peer_certificate)
    {
      peer_certificate_errors = verify_peer_certificate (tls, peer_certificate);

      if (G_IS_TLS_CLIENT_CONNECTION (tls))
        {
          GTlsCertificateFlags validation_flags;

          if (!g_tls_connection_base_is_dtls (tls))
            validation_flags =
              g_tls_client_connection_get_validation_flags (G_TLS_CLIENT_CONNECTION (tls));
          else
            validation_flags =
              g_dtls_client_connection_get_validation_flags (G_DTLS_CLIENT_CONNECTION (tls));

          if ((peer_certificate_errors & validation_flags) == 0)
            accepted = TRUE;
        }

      if (!accepted)
        {
          gboolean sync_handshake_in_progress;

          g_mutex_lock (&priv->op_mutex);
          sync_handshake_in_progress = priv->sync_handshake_in_progress;
          g_mutex_unlock (&priv->op_mutex);

          if (sync_handshake_in_progress)
            g_main_context_pop_thread_default (priv->handshake_context);

          accepted = g_tls_connection_emit_accept_certificate (G_TLS_CONNECTION (tls),
                                                               peer_certificate,
                                                               peer_certificate_errors);

          if (sync_handshake_in_progress)
            g_main_context_push_thread_default (priv->handshake_context);
        }
    }
  else if (G_IS_TLS_SERVER_CONNECTION (tls))
    {
      GTlsAuthenticationMode mode = 0;

      g_object_get (tls,
                    "authentication-mode", &mode,
                    NULL);

      if (mode != G_TLS_AUTHENTICATION_REQUIRED)
        accepted = TRUE;
    }

  g_mutex_lock (&priv->verify_certificate_mutex);

  priv->peer_certificate_accepted = accepted;

  /* Warning: the API documentation indicates that these properties are not
   * set until *after* accept-certificate.
   */
  g_clear_object (&priv->peer_certificate);
  priv->peer_certificate = g_steal_pointer (&peer_certificate);
  priv->peer_certificate_errors = peer_certificate_errors;

  g_object_notify (G_OBJECT (tls), "peer-certificate");
  g_object_notify (G_OBJECT (tls), "peer-certificate-errors");

  /* This has to be the very last statement before signaling the
   * condition variable because otherwise the code could spuriously
   * wakeup and continue before we are done here.
   */
  priv->peer_certificate_examined = TRUE;

  g_cond_signal (&priv->verify_certificate_condition);
  g_mutex_unlock (&priv->verify_certificate_mutex);

  return G_SOURCE_REMOVE;
}

gboolean
g_tls_connection_base_handshake_thread_verify_certificate (GTlsConnectionBase *tls)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);
  gboolean accepted;

  g_tls_log_debug (tls, "verifying peer certificate");

  g_mutex_lock (&priv->verify_certificate_mutex);
  priv->peer_certificate_examined = FALSE;
  priv->peer_certificate_accepted = FALSE;
  g_mutex_unlock (&priv->verify_certificate_mutex);

  /* Invoke the callback on the handshake context's thread. This is
   * necessary because we need to ensure the accept-certificate signal
   * is emitted on the original thread.
   */
  g_assert (priv->handshake_context);
  g_main_context_invoke (priv->handshake_context, accept_or_reject_peer_certificate, tls);

  /* We'll block the handshake thread until the original thread has
   * decided whether to accept the certificate.
   */
  g_mutex_lock (&priv->verify_certificate_mutex);
  while (!priv->peer_certificate_examined)
    g_cond_wait (&priv->verify_certificate_condition, &priv->verify_certificate_mutex);
  accepted = priv->peer_certificate_accepted;
  g_mutex_unlock (&priv->verify_certificate_mutex);

  return accepted;
}

static gboolean
g_tls_connection_base_get_binding_data (GTlsConnection          *conn,
                                        GTlsChannelBindingType   type,
                                        GByteArray              *data,
                                        GError                 **error)
{
  GTlsConnectionBase *tls = G_TLS_CONNECTION_BASE (conn);
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);
  GTlsConnectionBaseClass *tls_class = G_TLS_CONNECTION_BASE_GET_CLASS (tls);

  g_assert (tls_class->get_channel_binding_data);

  if (!priv->ever_handshaked || priv->need_handshake)
    {
      g_set_error (error, G_TLS_CHANNEL_BINDING_ERROR,
                   G_TLS_CHANNEL_BINDING_ERROR_INVALID_STATE,
                   _("Handshake is not finished, no channel binding information yet"));
      return FALSE;
    }

  return tls_class->get_channel_binding_data (tls, type, data, error);
}

static gboolean
g_tls_connection_base_dtls_get_binding_data (GDtlsConnection         *conn,
                                             GTlsChannelBindingType   type,
                                             GByteArray              *data,
                                             GError                 **error)
{
  GTlsConnectionBase *tls = G_TLS_CONNECTION_BASE (conn);

  return g_tls_connection_base_get_binding_data ((GTlsConnection *)tls,
                                                 type, data, error);
}

#if GLIB_CHECK_VERSION(2, 69, 0)
static const gchar *
g_tls_connection_base_get_negotiated_protocol (GTlsConnection *conn)
{
  GTlsConnectionBase *tls = G_TLS_CONNECTION_BASE (conn);
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

  return priv->negotiated_protocol;
}
#endif

static const gchar *
g_tls_connection_base_dtls_get_negotiated_protocol (GDtlsConnection *conn)
{
  GTlsConnectionBase *tls = G_TLS_CONNECTION_BASE (conn);
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

  return priv->negotiated_protocol;
}

static void
handshake_thread (GTask        *task,
                  gpointer      object,
                  gpointer      task_data,
                  GCancellable *cancellable)
{
  GTlsConnectionBase *tls = object;
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);
  GTlsConnectionBaseClass *tls_class = G_TLS_CONNECTION_BASE_GET_CLASS (tls);
  GError *error = NULL;
  gint64 start_time;
  gint64 timeout;

  g_tls_log_debug (tls, "TLS handshake thread starts");

  /* A timeout, in microseconds, must be provided as a gint64* task_data. */
  g_assert (task_data);
  start_time = g_get_monotonic_time ();
  timeout = *((gint64 *)task_data);

  priv->started_handshake = FALSE;
  priv->missing_requested_client_certificate = FALSE;

  if (!claim_op (tls, G_TLS_CONNECTION_BASE_OP_HANDSHAKE,
                 timeout, cancellable, &error))
    {
      g_task_return_error (task, error);
      g_tls_log_debug (tls, "TLS handshake thread failed: claiming op failed");
      return;
    }

  g_clear_error (&priv->handshake_error);

  if (priv->ever_handshaked && !priv->need_handshake)
    {
      GTlsConnectionBaseStatus status;

      if (tls_class->handshake_thread_safe_renegotiation_status (tls) != G_TLS_SAFE_RENEGOTIATION_SUPPORTED_BY_PEER)
        {
          g_task_return_new_error (task, G_TLS_ERROR, G_TLS_ERROR_MISC,
                                   _("Peer does not support safe renegotiation"));
          g_tls_log_debug (tls, "TLS handshake thread failed: peer does not support safe renegotiation");
          return;
        }

      /* Adjust the timeout for the next operation in the sequence. */
      if (timeout > 0)
        {
          timeout -= (g_get_monotonic_time () - start_time);
          if (timeout <= 0)
            timeout = 1;
        }

      status = tls_class->handshake_thread_request_rehandshake (tls, timeout, cancellable, &error);
      if (status != G_TLS_CONNECTION_BASE_OK)
        {
          g_task_return_error (task, error);
          g_tls_log_debug (tls, "TLS handshake thread failed: %s", error->message);
          return;
        }
    }

  /* Adjust the timeout for the next operation in the sequence. */
  if (timeout > 0)
    {
      timeout -= (g_get_monotonic_time () - start_time);
      if (timeout <= 0)
        timeout = 1;
    }

  priv->started_handshake = TRUE;
  tls_class->handshake_thread_handshake (tls, timeout, cancellable, &error);
  priv->need_handshake = FALSE;

  if (error)
    {
      g_task_return_error (task, error);
      g_tls_log_debug (tls, "TLS handshake thread failed: %s", error->message);
    }
  else
    {
      priv->ever_handshaked = TRUE;
      g_task_return_boolean (task, TRUE);
      g_tls_log_debug (tls, "TLS handshake thread succeeded");
    }
}

static void
sync_handshake_thread_completed (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  GTlsConnectionBase *tls = G_TLS_CONNECTION_BASE (object);
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);
  gpointer source_tag;

  g_tls_log_debug (tls, "synchronous TLS handshake thread completed");

  source_tag = g_task_get_source_tag (G_TASK (result));
  g_assert (source_tag == do_implicit_handshake || source_tag == g_tls_connection_base_handshake);
  g_assert (g_task_is_valid (result, object));

  g_assert (g_main_context_is_owner (priv->handshake_context));

  g_mutex_lock (&priv->op_mutex);
  priv->sync_handshake_in_progress = FALSE;
  g_mutex_unlock (&priv->op_mutex);

  g_main_context_wakeup (priv->handshake_context);
}

static void
crank_sync_handshake_context (GTlsConnectionBase *tls,
                              GCancellable       *cancellable)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

  /* need_finish_handshake will be set inside sync_handshake_thread_completed(),
   * which should only ever be invoked while iterating the handshake context
   * here. So need_finish_handshake should only change on this thread.
   *
   * FIXME: This function is not cancellable. We should figure out how to
   * support cancellation. We must not return from this function before it is
   * safe to destroy handshake_context, but it's not safe to destroy
   * handshake_context until after the handshake has completed. And the
   * handshake operation is not cancellable, so we have a problem.
   */
  g_mutex_lock (&priv->op_mutex);
  priv->sync_handshake_in_progress = TRUE;
  while (priv->sync_handshake_in_progress)
    {
      g_mutex_unlock (&priv->op_mutex);
      g_main_context_iteration (priv->handshake_context, TRUE);
      g_mutex_lock (&priv->op_mutex);
    }
  g_mutex_unlock (&priv->op_mutex);
}

static gboolean
finish_handshake (GTlsConnectionBase  *tls,
                  GTask               *task,
                  GError             **error)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);
  GTlsConnectionBaseClass *tls_class = G_TLS_CONNECTION_BASE_GET_CLASS (tls);
  gchar *original_negotiated_protocol;
  gchar *original_ciphersuite_name;
  GTlsProtocolVersion original_protocol_version;
  gboolean success;
  GError *my_error = NULL;

  g_tls_log_debug (tls, "finishing TLS handshake");

  original_negotiated_protocol = g_steal_pointer (&priv->negotiated_protocol);
  original_ciphersuite_name = g_steal_pointer (&priv->ciphersuite_name);
  original_protocol_version = priv->protocol_version;

  success = g_task_propagate_boolean (task, &my_error);
  if (success)
    {
      if (tls_class->is_session_resumed && tls_class->is_session_resumed (tls))
        {
          /* Because this session was resumed, we skipped certificate
           * verification on this handshake, so we missed our earlier
           * chance to set peer_certificate and peer_certificate_errors.
           * Do so here instead.
           *
           * The certificate has already been accepted, so we don't do
           * anything with the result here.
           */
          g_mutex_lock (&priv->verify_certificate_mutex);

          g_clear_object (&priv->peer_certificate);
          priv->peer_certificate = G_TLS_CONNECTION_BASE_GET_CLASS (tls)->retrieve_peer_certificate (tls);
          priv->peer_certificate_errors = verify_peer_certificate (tls, priv->peer_certificate);

          g_object_notify (G_OBJECT (tls), "peer-certificate");
          g_object_notify (G_OBJECT (tls), "peer-certificate-errors");

          priv->peer_certificate_examined = TRUE;
          priv->peer_certificate_accepted = TRUE;
          g_mutex_unlock (&priv->verify_certificate_mutex);
        }

      /* FIXME: Return an error from the handshake thread instead. */
      if (priv->peer_certificate && !priv->peer_certificate_accepted)
        {
          g_set_error_literal (&my_error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                               _("Unacceptable TLS certificate"));
          success = FALSE;
        }
    }

  tls_class->complete_handshake (tls,
                                 success,
                                 &priv->negotiated_protocol,
                                 &priv->protocol_version,
                                 &priv->ciphersuite_name,
                                 /* If we already have an error, ignore further errors. */
                                 my_error ? NULL : &my_error);

  if (g_strcmp0 (original_negotiated_protocol, priv->negotiated_protocol) != 0)
    g_object_notify (G_OBJECT (tls), "negotiated-protocol");
  g_free (original_negotiated_protocol);

  if (original_protocol_version != priv->protocol_version)
    g_object_notify (G_OBJECT (tls), "protocol-version");

  if (g_strcmp0 (original_ciphersuite_name, priv->ciphersuite_name) != 0)
    g_object_notify (G_OBJECT (tls), "ciphersuite-name");
  g_free (original_ciphersuite_name);

  if (my_error && priv->started_handshake)
    priv->handshake_error = g_error_copy (my_error);

  if (!my_error) {
    g_tls_log_debug (tls, "TLS handshake has finished successfully");
    return TRUE;
  }

  g_tls_log_debug (tls, "TLS handshake has finished with error: %s", my_error->message);
  g_propagate_error (error, my_error);
  return FALSE;
}

static gboolean
g_tls_connection_base_handshake (GTlsConnection   *conn,
                                 GCancellable     *cancellable,
                                 GError          **error)
{
  GTlsConnectionBase *tls = G_TLS_CONNECTION_BASE (conn);
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);
  GTlsConnectionBaseClass *tls_class = G_TLS_CONNECTION_BASE_GET_CLASS (tls);
  GTask *task;
  gboolean success;
  gint64 *timeout = NULL;
  GError *my_error = NULL;

  g_tls_log_debug (tls, "Starting synchronous TLS handshake");

  g_assert (!priv->handshake_context);
  priv->handshake_context = g_main_context_new ();

  g_main_context_push_thread_default (priv->handshake_context);

  if (tls_class->prepare_handshake)
    tls_class->prepare_handshake (tls, priv->advertised_protocols);

  task = g_task_new (conn, cancellable, sync_handshake_thread_completed, NULL);
  g_task_set_source_tag (task, g_tls_connection_base_handshake);
  g_task_set_name (task, "[glib-networking] g_tls_connection_base_handshake");

  timeout = g_new0 (gint64, 1);
  *timeout = -1; /* blocking */
  g_task_set_task_data (task, timeout, g_free);

  g_task_run_in_thread (task, handshake_thread);
  crank_sync_handshake_context (tls, cancellable);

  success = finish_handshake (tls, task, &my_error);
  g_object_unref (task);

  g_main_context_pop_thread_default (priv->handshake_context);
  g_clear_pointer (&priv->handshake_context, g_main_context_unref);

  yield_op (tls, G_TLS_CONNECTION_BASE_OP_HANDSHAKE,
            G_TLS_CONNECTION_BASE_OK);

  if (my_error)
    g_propagate_error (error, my_error);
  return success;
}

static gboolean
g_tls_connection_base_dtls_handshake (GDtlsConnection  *conn,
                                      GCancellable     *cancellable,
                                      GError          **error)
{
  return g_tls_connection_base_handshake (G_TLS_CONNECTION (conn),
                                          cancellable, error);
}

/* In the async version we use two GTasks; one to run
 * handshake_thread() and then call async_handshake_thread_completed(),
 * and a second to call the caller's original callback after we call
 * finish_handshake().
 */

static void
async_handshake_thread_completed (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  GTask *caller_task = user_data;
  GTlsConnectionBase *tls = g_task_get_source_object (caller_task);
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);
  GError *error = NULL;
  gboolean need_finish_handshake, success;

  g_tls_log_debug (tls, "Asynchronous TLS handshake thread completed");

  g_assert (g_task_is_valid (result, object));
  g_assert (g_task_get_source_tag (G_TASK (result)) == g_tls_connection_base_handshake_async);

  g_mutex_lock (&priv->op_mutex);
  if (priv->need_finish_handshake)
    {
      need_finish_handshake = TRUE;
      priv->need_finish_handshake = FALSE;
    }
  else
    need_finish_handshake = FALSE;
  g_mutex_unlock (&priv->op_mutex);

  /* We have to clear handshake_context before g_task_return_* because it can
   * return immediately to application code inside g_task_return_*,
   * and the application code could then start a new TLS operation.
   *
   * But we can't clear until after finish_handshake().
   */
  if (need_finish_handshake)
    {
      success = finish_handshake (tls, G_TASK (result), &error);

      g_clear_pointer (&priv->handshake_context, g_main_context_unref);

      if (success)
        g_task_return_boolean (caller_task, TRUE);
      else
        g_task_return_error (caller_task, error);
    }
  else
    {
      g_clear_pointer (&priv->handshake_context, g_main_context_unref);

      if (priv->handshake_error)
        g_task_return_error (caller_task, g_error_copy (priv->handshake_error));
      else
        g_task_return_boolean (caller_task, TRUE);
    }

  g_object_unref (caller_task);
}

static void
async_handshake_thread (GTask        *task,
                        gpointer      object,
                        gpointer      task_data,
                        GCancellable *cancellable)
{
  GTlsConnectionBase *tls = object;
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

  g_tls_log_debug (tls, "Asynchronous TLS handshake thread starts");

  handshake_thread (task, object, task_data, cancellable);

  g_mutex_lock (&priv->op_mutex);
  priv->need_finish_handshake = TRUE;
  /* yield_op will clear handshaking too, but we don't want the
   * connection to be briefly "handshaking && need_finish_handshake"
   * after we unlock the mutex.
   */
  priv->handshaking = FALSE;
  g_mutex_unlock (&priv->op_mutex);

  yield_op (tls, G_TLS_CONNECTION_BASE_OP_HANDSHAKE,
            G_TLS_CONNECTION_BASE_OK);
}

static void
g_tls_connection_base_handshake_async (GTlsConnection      *conn,
                                       int                  io_priority,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  GTlsConnectionBase *tls = G_TLS_CONNECTION_BASE (conn);
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);
  GTlsConnectionBaseClass *tls_class = G_TLS_CONNECTION_BASE_GET_CLASS (tls);
  GTask *thread_task, *caller_task;
  gint64 *timeout = NULL;

  g_tls_log_debug (tls, "Starting asynchronous TLS handshake");

  g_assert (!priv->handshake_context);
  priv->handshake_context = g_main_context_ref_thread_default ();

  if (tls_class->prepare_handshake)
    tls_class->prepare_handshake (tls, priv->advertised_protocols);

  caller_task = g_task_new (conn, cancellable, callback, user_data);
  g_task_set_source_tag (caller_task, g_tls_connection_base_handshake_async);
  g_task_set_name (caller_task, "[glib-networking] g_tls_connection_base_handshake_async (caller task)");
  g_task_set_priority (caller_task, io_priority);

  thread_task = g_task_new (conn, cancellable, async_handshake_thread_completed, caller_task);
  g_task_set_source_tag (thread_task, g_tls_connection_base_handshake_async);
  g_task_set_name (caller_task, "[glib-networking] g_tls_connection_base_handshake_async (thread task)");
  g_task_set_priority (thread_task, io_priority);

  timeout = g_new0 (gint64, 1);
  *timeout = -1; /* blocking */
  g_task_set_task_data (thread_task, timeout, g_free);

  g_task_run_in_thread (thread_task, async_handshake_thread);
  g_object_unref (thread_task);
}

static gboolean
g_tls_connection_base_handshake_finish (GTlsConnection  *conn,
                                        GAsyncResult    *result,
                                        GError         **error)
{
  g_return_val_if_fail (g_task_is_valid (result, conn), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == g_tls_connection_base_handshake_async, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
g_tls_connection_base_dtls_handshake_async (GDtlsConnection     *conn,
                                            int                  io_priority,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{
  g_tls_connection_base_handshake_async (G_TLS_CONNECTION (conn), io_priority,
                                         cancellable, callback, user_data);
}

static gboolean
g_tls_connection_base_dtls_handshake_finish (GDtlsConnection  *conn,
                                             GAsyncResult     *result,
                                             GError          **error)
{
  return g_tls_connection_base_handshake_finish (G_TLS_CONNECTION (conn),
                                                 result, error);
}

static gboolean
do_implicit_handshake (GTlsConnectionBase  *tls,
                       gint64               timeout,
                       GCancellable        *cancellable,
                       GError             **error)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);
  GTlsConnectionBaseClass *tls_class = G_TLS_CONNECTION_BASE_GET_CLASS (tls);
  gint64 *thread_timeout = NULL;

  g_tls_log_debug (tls, "Implicit TLS handshaking starts");

  /* We have op_mutex */

  g_assert (!priv->handshake_context);
  if (timeout != 0)
    {
      priv->handshake_context = g_main_context_new ();
      g_main_context_push_thread_default (priv->handshake_context);
    }
  else
    {
      priv->handshake_context = g_main_context_ref_thread_default ();
    }

  g_assert (!priv->implicit_handshake);
  priv->implicit_handshake = g_task_new (tls, cancellable,
                                         timeout ? sync_handshake_thread_completed : NULL,
                                         NULL);
  g_task_set_source_tag (priv->implicit_handshake, do_implicit_handshake);
  g_task_set_name (priv->implicit_handshake, "[glib-networking] do_implicit_handshake");

  thread_timeout = g_new0 (gint64, 1);
  g_task_set_task_data (priv->implicit_handshake,
                        thread_timeout, g_free);

  if (tls_class->prepare_handshake)
    tls_class->prepare_handshake (tls, priv->advertised_protocols);

  if (timeout != 0)
    {
      GError *my_error = NULL;
      gboolean success;

      /* In the blocking case, run the handshake operation synchronously in
       * another thread, and delegate handling the timeout to that thread; it
       * should return G_IO_ERROR_TIMED_OUT iff (timeout > 0) and the operation
       * times out. If (timeout < 0) it should block indefinitely until the
       * operation is complete or errors. */
      *thread_timeout = timeout;

      g_mutex_unlock (&priv->op_mutex);

      g_task_run_in_thread (priv->implicit_handshake, handshake_thread);

      crank_sync_handshake_context (tls, cancellable);

      success = finish_handshake (tls,
                                  priv->implicit_handshake,
                                  &my_error);

      g_main_context_pop_thread_default (priv->handshake_context);
      g_clear_pointer (&priv->handshake_context, g_main_context_unref);
      g_clear_object (&priv->implicit_handshake);

      yield_op (tls, G_TLS_CONNECTION_BASE_OP_HANDSHAKE,
                G_TLS_CONNECTION_BASE_OK);

      g_mutex_lock (&priv->op_mutex);

      if (my_error)
        g_propagate_error (error, my_error);
      return success;
    }
  else
    {
      /* In the non-blocking case, start the asynchronous handshake operation
       * and return EWOULDBLOCK to the caller, who will handle polling for
       * completion of the handshake and whatever operation they actually cared
       * about. Run the actual operation as blocking in its thread. */
      *thread_timeout = -1; /* blocking */

      g_task_run_in_thread (priv->implicit_handshake,
                            async_handshake_thread);

      /* Intentionally not translated because this is not a fatal error to be
       * presented to the user, and to avoid this showing up in profiling. */
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK, "Operation would block");
      return FALSE;
    }
}

gssize
g_tls_connection_base_read (GTlsConnectionBase  *tls,
                            void                *buffer,
                            gsize                count,
                            gint64               timeout,
                            GCancellable        *cancellable,
                            GError             **error)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);
  GTlsConnectionBaseStatus status;
  gssize nread;

  g_tls_log_debug (tls, "starting to read data from TLS connection");

  do
    {
      if (!claim_op (tls, G_TLS_CONNECTION_BASE_OP_READ,
                     timeout, cancellable, error))
        return -1;

      if (priv->app_data_buf && !priv->handshaking)
        {
          nread = MIN (count, priv->app_data_buf->len);
          memcpy (buffer, priv->app_data_buf->data, nread);
          if (nread == priv->app_data_buf->len)
            g_clear_pointer (&priv->app_data_buf, g_byte_array_unref);
          else
            g_byte_array_remove_range (priv->app_data_buf, 0, nread);
          status = G_TLS_CONNECTION_BASE_OK;
        }
      else
        {
          status = G_TLS_CONNECTION_BASE_GET_CLASS (tls)->
            read_fn (tls, buffer, count, timeout, &nread, cancellable, error);
        }

      yield_op (tls, G_TLS_CONNECTION_BASE_OP_READ, status);
    }
  while (status == G_TLS_CONNECTION_BASE_REHANDSHAKE);

  if (status == G_TLS_CONNECTION_BASE_OK)
    {
      priv->successful_read_op = TRUE;
      g_tls_log_debug (tls, "successfully read %" G_GSSIZE_FORMAT " bytes from TLS connection", nread);
      return nread;
    }

  g_tls_log_debug (tls, "reading data from TLS connection has failed: %s", status_to_string (status));
  return -1;
}

static gssize
g_tls_connection_base_read_message (GTlsConnectionBase  *tls,
                                    GInputVector        *vectors,
                                    guint                num_vectors,
                                    gint64               timeout,
                                    GCancellable        *cancellable,
                                    GError             **error)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);
  GTlsConnectionBaseStatus status = G_TLS_CONNECTION_BASE_OK;
  gssize nread;

  g_tls_log_debug (tls, "starting to read messages from TLS connection");

  do {
    if (!claim_op (tls, G_TLS_CONNECTION_BASE_OP_READ,
                   timeout, cancellable, error))
      return -1;

    /* Copy data out of the app data buffer first. */
    if (priv->app_data_buf && !priv->handshaking)
      {
        nread = 0;

        for (guint i = 0; i < num_vectors && priv->app_data_buf; i++)
          {
            gsize count;
            GInputVector *vec = &vectors[i];

            count = MIN (vec->size, priv->app_data_buf->len);
            nread += count;

            memcpy (vec->buffer, priv->app_data_buf->data, count);
            if (count == priv->app_data_buf->len)
              g_clear_pointer (&priv->app_data_buf, g_byte_array_unref);
            else
              g_byte_array_remove_range (priv->app_data_buf, 0, count);
          }
      }
    else
      {
        g_assert (G_TLS_CONNECTION_BASE_GET_CLASS (tls)->read_message_fn);
        status = G_TLS_CONNECTION_BASE_GET_CLASS (tls)->
          read_message_fn (tls, vectors, num_vectors, timeout, &nread, cancellable, error);
      }

    yield_op (tls, G_TLS_CONNECTION_BASE_OP_READ, status);
  } while (status == G_TLS_CONNECTION_BASE_REHANDSHAKE);

  if (status == G_TLS_CONNECTION_BASE_OK)
    {
      priv->successful_read_op = TRUE;
      g_tls_log_debug (tls, "successfully read %" G_GSSIZE_FORMAT " bytes from TLS connection", nread);
      return nread;
    }

  g_tls_log_debug (tls, "reading message from TLS connection has failed: %s", status_to_string (status));
  return -1;
}

static gint
g_tls_connection_base_receive_messages (GDatagramBased  *datagram_based,
                                        GInputMessage   *messages,
                                        guint            num_messages,
                                        gint             flags,
                                        gint64           timeout,
                                        GCancellable    *cancellable,
                                        GError         **error)
{
  GTlsConnectionBase *tls = G_TLS_CONNECTION_BASE (datagram_based);
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);
  guint i;
  GError *child_error = NULL;

  if (flags != G_SOCKET_MSG_NONE)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                   _("Receive flags are not supported"));
      return -1;
    }

  for (i = 0; i < num_messages && !child_error; i++)
    {
      GInputMessage *message = &messages[i];
      gssize n_bytes_read;

      n_bytes_read = g_tls_connection_base_read_message (tls,
                                                         message->vectors,
                                                         message->num_vectors,
                                                         timeout,
                                                         cancellable,
                                                         &child_error);

      if (message->address)
        *message->address = NULL;
      message->flags = G_SOCKET_MSG_NONE;
      if (message->control_messages)
        *message->control_messages = NULL;
      message->num_control_messages = 0;

      if (n_bytes_read > 0)
        {
          message->bytes_received = n_bytes_read;
        }
      else if (n_bytes_read == 0)
        {
          /* EOS. */
          break;
        }
      else if (i > 0 &&
               (g_error_matches (child_error,
                                 G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK) ||
                g_error_matches (child_error,
                                 G_IO_ERROR, G_IO_ERROR_TIMED_OUT)))
        {
          /* Blocked or timed out after receiving some messages successfully. */
          g_clear_error (&child_error);
          break;
        }
      else
        {
          /* Error, including G_IO_ERROR_WOULD_BLOCK or G_IO_ERROR_TIMED_OUT on
           * the first message; or G_IO_ERROR_CANCELLED at any time. */
          break;
        }
    }

  if (child_error)
    {
      g_propagate_error (error, child_error);
      return -1;
    }

  priv->successful_read_op = TRUE;
  return i;
}

gssize
g_tls_connection_base_write (GTlsConnectionBase  *tls,
                             const void          *buffer,
                             gsize                count,
                             gint64               timeout,
                             GCancellable        *cancellable,
                             GError             **error)
{
  GTlsConnectionBaseStatus status;
  gssize nwrote;

  g_tls_log_debug (tls, "starting to write %" G_GSIZE_FORMAT " bytes to TLS connection", count);

  do
    {
      if (!claim_op (tls, G_TLS_CONNECTION_BASE_OP_WRITE,
                     timeout, cancellable, error))
        return -1;

      status = G_TLS_CONNECTION_BASE_GET_CLASS (tls)->
        write_fn (tls, buffer, count, timeout, &nwrote, cancellable, error);

      yield_op (tls, G_TLS_CONNECTION_BASE_OP_WRITE, status);
    }
  while (status == G_TLS_CONNECTION_BASE_REHANDSHAKE);

  if (status == G_TLS_CONNECTION_BASE_OK)
    {
      g_tls_log_debug (tls, "successfully write %" G_GSSIZE_FORMAT " bytes to TLS connection", nwrote);
      return nwrote;
    }

  g_tls_log_debug (tls, "writing data to TLS connection has failed: %s", status_to_string (status));
  return -1;
}

static gssize
g_tls_connection_base_write_message (GTlsConnectionBase  *tls,
                                     GOutputVector       *vectors,
                                     guint                num_vectors,
                                     gint64               timeout,
                                     GCancellable        *cancellable,
                                     GError             **error)
{
  GTlsConnectionBaseStatus status;
  gssize nwrote;

  g_tls_log_debug (tls, "starting to write messages to TLS connection");

  do {
    if (!claim_op (tls, G_TLS_CONNECTION_BASE_OP_WRITE,
                   timeout, cancellable, error))
      return -1;

    g_assert (G_TLS_CONNECTION_BASE_GET_CLASS (tls)->read_message_fn);
    status = G_TLS_CONNECTION_BASE_GET_CLASS (tls)->
      write_message_fn (tls, vectors, num_vectors, timeout, &nwrote, cancellable, error);

    yield_op (tls, G_TLS_CONNECTION_BASE_OP_WRITE, status);
  } while (status == G_TLS_CONNECTION_BASE_REHANDSHAKE);

  if (status == G_TLS_CONNECTION_BASE_OK)
    {
      g_tls_log_debug (tls, "successfully write %" G_GSSIZE_FORMAT " bytes to TLS connection", nwrote);
      return nwrote;
    }

  g_tls_log_debug (tls, "writing messages to TLS connection has failed: %s", status_to_string (status));
  return -1;
}

static gint
g_tls_connection_base_send_messages (GDatagramBased  *datagram_based,
                                     GOutputMessage  *messages,
                                     guint            num_messages,
                                     gint             flags,
                                     gint64           timeout,
                                     GCancellable    *cancellable,
                                     GError         **error)
{
  GTlsConnectionBase *tls = G_TLS_CONNECTION_BASE (datagram_based);
  guint i;
  GError *child_error = NULL;

  if (flags != G_SOCKET_MSG_NONE)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                   _("Send flags are not supported"));
      return -1;
    }

  for (i = 0; i < num_messages && !child_error; i++)
    {
      GOutputMessage *message = &messages[i];
      gssize n_bytes_sent;

      n_bytes_sent = g_tls_connection_base_write_message (tls,
                                                          message->vectors,
                                                          message->num_vectors,
                                                          timeout,
                                                          cancellable,
                                                          &child_error);

      if (n_bytes_sent >= 0)
        {
          message->bytes_sent = n_bytes_sent;
        }
      else if (i > 0 &&
               (g_error_matches (child_error,
                                 G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK) ||
                g_error_matches (child_error,
                                 G_IO_ERROR, G_IO_ERROR_TIMED_OUT)))
        {
          /* Blocked or timed out after sending some messages successfully. */
          g_clear_error (&child_error);
          break;
        }
      else
        {
          /* Error, including G_IO_ERROR_WOULD_BLOCK or G_IO_ERROR_TIMED_OUT
           * on the first message; or G_IO_ERROR_CANCELLED at any time. */
          break;
        }
    }

  if (child_error)
    {
      g_propagate_error (error, child_error);
      return -1;
    }

  return i;
}

static GInputStream *
g_tls_connection_base_get_input_stream (GIOStream *stream)
{
  GTlsConnectionBase *tls = G_TLS_CONNECTION_BASE (stream);
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

  return priv->tls_istream;
}

static GOutputStream *
g_tls_connection_base_get_output_stream (GIOStream *stream)
{
  GTlsConnectionBase *tls = G_TLS_CONNECTION_BASE (stream);
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

  return priv->tls_ostream;
}

gboolean
g_tls_connection_base_close_internal (GIOStream      *stream,
                                      GTlsDirection   direction,
                                      gint64          timeout,
                                      GCancellable   *cancellable,
                                      GError        **error)
{
  GTlsConnectionBase *tls = G_TLS_CONNECTION_BASE (stream);
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);
  GTlsConnectionBaseOp op;
  GTlsConnectionBaseStatus status;
  gboolean success = TRUE;
  GError *close_error = NULL, *stream_error = NULL;

  g_tls_log_debug (tls, "starting to close the TLS connection");

  /* This can be called from g_io_stream_close(), g_input_stream_close(),
   * g_output_stream_close(), or g_tls_connection_close(). In all cases, we only
   * do the close_fn() for writing. The difference is how we set the flags on
   * this class and how the underlying stream is closed.
   */

  g_return_val_if_fail (direction != G_TLS_DIRECTION_NONE, FALSE);

  if (direction == G_TLS_DIRECTION_BOTH)
    op = G_TLS_CONNECTION_BASE_OP_CLOSE_BOTH;
  else if (direction == G_TLS_DIRECTION_READ)
    op = G_TLS_CONNECTION_BASE_OP_CLOSE_READ;
  else
    op = G_TLS_CONNECTION_BASE_OP_CLOSE_WRITE;

  if (!claim_op (tls, op, timeout, cancellable, error))
    return FALSE;

  if (priv->ever_handshaked && !priv->write_closed &&
      direction & G_TLS_DIRECTION_WRITE)
    {
      status = G_TLS_CONNECTION_BASE_GET_CLASS (tls)->
        close_fn (tls, timeout, cancellable, &close_error);

      priv->write_closed = TRUE;
    }
  else
    status = G_TLS_CONNECTION_BASE_OK;

  if (!priv->read_closed && direction & G_TLS_DIRECTION_READ)
    priv->read_closed = TRUE;

  /* Close the underlying streams. Do this even if the close_fn() call failed,
   * as the parent GIOStream will have set its internal closed flag and hence
   * this implementation will never be called again. */
  if (priv->base_io_stream)
    {
      if (direction == G_TLS_DIRECTION_BOTH)
        success = g_io_stream_close (priv->base_io_stream,
                                     cancellable, &stream_error);
      else if (direction & G_TLS_DIRECTION_READ)
        success = g_input_stream_close (g_io_stream_get_input_stream (priv->base_io_stream),
                                        cancellable, &stream_error);
      else if (direction & G_TLS_DIRECTION_WRITE)
        success = g_output_stream_close (g_io_stream_get_output_stream (priv->base_io_stream),
                                         cancellable, &stream_error);
    }
  else if (g_tls_connection_base_is_dtls (tls))
    {
      /* We do not close underlying #GDatagramBaseds. There is no
       * g_datagram_based_close() method since different datagram-based
       * protocols vary wildly in how they close. */
      success = TRUE;
    }
  else
    {
      g_assert_not_reached ();
    }

  yield_op (tls, op, status);

  /* Propagate errors. */
  if (status != G_TLS_CONNECTION_BASE_OK)
    {
      g_tls_log_debug (tls, "error closing TLS connection: %s", close_error->message);
      g_propagate_error (error, close_error);
      g_clear_error (&stream_error);
    }
  else if (!success)
    {
      g_tls_log_debug (tls, "error closing TLS connection: %s", stream_error->message);
      g_propagate_error (error, stream_error);
      g_clear_error (&close_error);
    }
  else
    {
      g_tls_log_debug (tls, "the TLS connection has been closed successfully");
    }

  return success && status == G_TLS_CONNECTION_BASE_OK;
}

static gboolean
g_tls_connection_base_close (GIOStream     *stream,
                             GCancellable  *cancellable,
                             GError       **error)
{
  return g_tls_connection_base_close_internal (stream,
                                               G_TLS_DIRECTION_BOTH,
                                               -1,  /* blocking */
                                               cancellable, error);
}

static gboolean
g_tls_connection_base_dtls_shutdown (GDtlsConnection  *conn,
                                     gboolean          shutdown_read,
                                     gboolean          shutdown_write,
                                     GCancellable     *cancellable,
                                     GError          **error)
{
  GTlsDirection direction = G_TLS_DIRECTION_NONE;

  if (shutdown_read)
    direction |= G_TLS_DIRECTION_READ;
  if (shutdown_write)
    direction |= G_TLS_DIRECTION_WRITE;

  return g_tls_connection_base_close_internal (G_IO_STREAM (conn),
                                               direction,
                                               -1, /* blocking */
                                               cancellable, error);
}

/* We do async close as synchronous-in-a-thread so we don't need to
 * implement G_IO_IN/G_IO_OUT flip-flopping just for this one case
 * (since handshakes are also done synchronously now).
 */
static void
close_thread (GTask        *task,
              gpointer      object,
              gpointer      task_data,
              GCancellable *cancellable)
{
  GIOStream *stream = object;
  GTlsDirection direction;
  GError *error = NULL;

  direction = GPOINTER_TO_INT (g_task_get_task_data (task));

  if (!g_tls_connection_base_close_internal (stream, direction,
                                             -1, /* blocking */
                                             cancellable, &error))
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);
}

static void
g_tls_connection_base_close_internal_async (GIOStream           *stream,
                                            GTlsDirection        direction,
                                            int                  io_priority,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{
  GTask *task;

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_tls_connection_base_close_internal_async);
  g_task_set_name (task, "[glib-networking] g_tls_connection_base_close_internal_async");
  g_task_set_priority (task, io_priority);
  g_task_set_task_data (task, GINT_TO_POINTER (direction), NULL);
  g_task_run_in_thread (task, close_thread);
  g_object_unref (task);
}

static void
g_tls_connection_base_close_async (GIOStream           *stream,
                                   int                  io_priority,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_tls_connection_base_close_internal_async (stream, G_TLS_DIRECTION_BOTH,
                                              io_priority, cancellable,
                                              callback, user_data);
}

static gboolean
g_tls_connection_base_close_finish (GIOStream           *stream,
                                    GAsyncResult        *result,
                                    GError             **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == g_tls_connection_base_close_internal_async, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
g_tls_connection_base_dtls_shutdown_async (GDtlsConnection     *conn,
                                           gboolean             shutdown_read,
                                           gboolean             shutdown_write,
                                           int                  io_priority,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  GTlsDirection direction = G_TLS_DIRECTION_NONE;

  if (shutdown_read)
    direction |= G_TLS_DIRECTION_READ;
  if (shutdown_write)
    direction |= G_TLS_DIRECTION_WRITE;

  g_tls_connection_base_close_internal_async (G_IO_STREAM (conn), direction,
                                              io_priority, cancellable,
                                              callback, user_data);
}

static gboolean
g_tls_connection_base_dtls_shutdown_finish (GDtlsConnection  *conn,
                                            GAsyncResult     *result,
                                            GError          **error)
{
  g_return_val_if_fail (g_task_is_valid (result, conn), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == g_tls_connection_base_close_internal_async, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
g_tls_connection_base_dtls_set_advertised_protocols (GDtlsConnection     *conn,
                                                     const gchar * const *protocols)
{
  g_object_set (conn, "advertised-protocols", protocols, NULL);
}

GDatagramBased *
g_tls_connection_base_get_base_socket (GTlsConnectionBase *tls)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

 g_assert (g_tls_connection_base_is_dtls (tls));

  return priv->base_socket;
}

GIOStream *
g_tls_connection_base_get_base_iostream (GTlsConnectionBase *tls)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

 g_assert (!g_tls_connection_base_is_dtls (tls));

  return priv->base_io_stream;
}

GPollableInputStream *
g_tls_connection_base_get_base_istream (GTlsConnectionBase *tls)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

  g_assert (!g_tls_connection_base_is_dtls (tls));

  return priv->base_istream;
}

GPollableOutputStream *
g_tls_connection_base_get_base_ostream (GTlsConnectionBase *tls)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

  g_assert (!g_tls_connection_base_is_dtls (tls));

  return priv->base_ostream;
}

void
g_tls_connection_base_handshake_thread_set_missing_requested_client_certificate (GTlsConnectionBase *tls)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

  priv->missing_requested_client_certificate = TRUE;
}

GError **
g_tls_connection_base_get_read_error (GTlsConnectionBase *tls)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

  return &priv->read_error;
}

GError **
g_tls_connection_base_get_write_error (GTlsConnectionBase *tls)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

  return &priv->write_error;
}

gint64
g_tls_connection_base_get_read_timeout (GTlsConnectionBase *tls)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

  return priv->read_timeout;
}

gint64
g_tls_connection_base_get_write_timeout (GTlsConnectionBase *tls)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

  return priv->write_timeout;
}

GCancellable *
g_tls_connection_base_get_read_cancellable (GTlsConnectionBase *tls)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

  return priv->read_cancellable;
}

GCancellable *
g_tls_connection_base_get_write_cancellable (GTlsConnectionBase *tls)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

  return priv->write_cancellable;
}

gboolean
g_tls_connection_base_is_handshaking (GTlsConnectionBase *tls)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

  return priv->handshaking;
}

gboolean
g_tls_connection_base_ever_handshaked (GTlsConnectionBase *tls)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

  return priv->ever_handshaked;
}

gboolean
g_tls_connection_base_handshake_thread_request_certificate (GTlsConnectionBase *tls)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);
  GTlsInteractionResult res = G_TLS_INTERACTION_UNHANDLED;
  GTlsInteraction *interaction;
  GTlsConnection *conn;

  g_return_val_if_fail (G_IS_TLS_CONNECTION_BASE (tls), FALSE);

  conn = G_TLS_CONNECTION (tls);

  g_clear_error (&priv->interaction_error);

  interaction = g_tls_connection_get_interaction (conn);
  if (!interaction)
    return FALSE;

  res = g_tls_interaction_invoke_request_certificate (interaction, conn, 0,
                                                      priv->read_cancellable,
                                                      &priv->interaction_error);
  return res != G_TLS_INTERACTION_FAILED;
}

gboolean
g_tls_connection_base_handshake_thread_ask_password (GTlsConnectionBase *tls,
                                                     GTlsPassword       *password)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);
  GTlsInteractionResult res = G_TLS_INTERACTION_UNHANDLED;
  GTlsInteraction *interaction;

  g_return_val_if_fail (G_IS_TLS_CONNECTION_BASE (tls), FALSE);

  g_clear_error (&priv->interaction_error);

  interaction = g_tls_connection_get_interaction (G_TLS_CONNECTION (tls));
  if (!interaction)
    return FALSE;

  res = g_tls_interaction_invoke_ask_password (interaction, password,
                                               priv->read_cancellable,
                                               &priv->interaction_error);
  return res != G_TLS_INTERACTION_FAILED;
}

void
g_tls_connection_base_handshake_thread_buffer_application_data (GTlsConnectionBase *tls,
                                                                guint8             *data,
                                                                gsize               length)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);

  if (!priv->app_data_buf)
    priv->app_data_buf = g_byte_array_new ();

  g_byte_array_append (priv->app_data_buf, data, length);
}

gchar *
g_tls_connection_base_get_session_id (GTlsConnectionBase *tls)
{
  GTlsConnectionBasePrivate *priv = g_tls_connection_base_get_instance_private (tls);
  return priv->session_id;
}

static void
g_tls_connection_base_constructed (GObject *object)
{
  GTlsConnectionBase *tls = G_TLS_CONNECTION_BASE (object);
  if (G_IS_TLS_CLIENT_CONNECTION (tls))
    {
      GSocketConnection *base_conn;

      /* Create a TLS "session ID." We base it on the IP address since
       * different hosts serving the same hostname/service will probably
       * not share the same session cache. We base it on the
       * server-identity because at least some servers will fail (rather
       * than just failing to resume the session) if we don't.
       * (https://bugs.launchpad.net/bugs/823325)
       *
       * Note that our session IDs have no relation to TLS protocol
       * session IDs.
       */
      g_object_get (G_OBJECT (tls), "base-io-stream", &base_conn, NULL);
      if (G_IS_SOCKET_CONNECTION (base_conn))
        {
          GSocketAddress *remote_addr;
          remote_addr = g_socket_connection_get_remote_address (base_conn, NULL);
          if (G_IS_INET_SOCKET_ADDRESS (remote_addr))
            {
              gchar *cert_hash = NULL;
              GTlsCertificate *cert = NULL;
              GTlsConnectionBasePrivate *priv = NULL;
              const gchar *server_hostname = get_server_identity (!g_tls_connection_base_is_dtls (tls) ?
                                                                  g_tls_client_connection_get_server_identity (G_TLS_CLIENT_CONNECTION (tls)) :
                                                                  g_dtls_client_connection_get_server_identity (G_DTLS_CLIENT_CONNECTION (tls)));
              priv = g_tls_connection_base_get_instance_private (tls);

              /* If we have a certificate, make its hash part of the session ID, so
               * that different connections to the same server can use different
               * certificates.
               */
              g_object_get (G_OBJECT (tls), "certificate", &cert, NULL);
              if (cert)
                {
                  GByteArray *der = NULL;
                  g_object_get (G_OBJECT (cert), "certificate", &der, NULL);
                  if (der)
                    {
                      cert_hash = g_compute_checksum_for_data (G_CHECKSUM_SHA256, der->data, der->len);
                      g_byte_array_unref (der);
                    }
                  g_object_unref (cert);
                }

              if (server_hostname)
                {
                  priv->session_id = g_strdup_printf ("%s/%s", server_hostname,
                                                      cert_hash ? cert_hash : "");
                }
              else
                {
                  guint port;
                  GInetAddress *iaddr;
                  gchar *addrstr = NULL;
                  GInetSocketAddress *isaddr = G_INET_SOCKET_ADDRESS (remote_addr);

                  port = g_inet_socket_address_get_port (isaddr);
                  iaddr = g_inet_socket_address_get_address (isaddr);
                  addrstr = g_inet_address_to_string (iaddr);

                  priv->session_id = g_strdup_printf ("%s/%d/%s", addrstr,
                                                      port,
                                                      cert_hash ? cert_hash : "");
                  g_free (addrstr);
                }
              g_free (cert_hash);
            }
          g_object_unref (remote_addr);
        }
      g_object_unref (base_conn);
    }

  if (G_OBJECT_CLASS (g_tls_connection_base_parent_class)->constructed)
    G_OBJECT_CLASS (g_tls_connection_base_parent_class)->constructed (object);
}

static void
g_tls_connection_base_class_init (GTlsConnectionBaseClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GTlsConnectionClass *connection_class = G_TLS_CONNECTION_CLASS (klass);
  GIOStreamClass *iostream_class = G_IO_STREAM_CLASS (klass);

  gobject_class->get_property = g_tls_connection_base_get_property;
  gobject_class->set_property = g_tls_connection_base_set_property;
  gobject_class->finalize     = g_tls_connection_base_finalize;
  gobject_class->constructed  = g_tls_connection_base_constructed;

  connection_class->handshake               = g_tls_connection_base_handshake;
  connection_class->handshake_async         = g_tls_connection_base_handshake_async;
  connection_class->handshake_finish        = g_tls_connection_base_handshake_finish;
  connection_class->get_binding_data        = g_tls_connection_base_get_binding_data;
#if GLIB_CHECK_VERSION(2, 69, 0)
  connection_class->get_negotiated_protocol = g_tls_connection_base_get_negotiated_protocol;
#endif

  iostream_class->get_input_stream  = g_tls_connection_base_get_input_stream;
  iostream_class->get_output_stream = g_tls_connection_base_get_output_stream;
  iostream_class->close_fn          = g_tls_connection_base_close;
  iostream_class->close_async       = g_tls_connection_base_close_async;
  iostream_class->close_finish      = g_tls_connection_base_close_finish;

  klass->push_io = g_tls_connection_base_real_push_io;
  klass->pop_io = g_tls_connection_base_real_pop_io;

  g_object_class_install_property (gobject_class, PROP_SESSION_REUSED,
    g_param_spec_boolean ("session-reused",
                  NULL, NULL,
                  FALSE,
                  G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SESSION_RESUMPTION_ENABLED,
    g_param_spec_boolean ("session-resumption-enabled",
                  NULL, NULL,
                  !g_test_initialized (),
                  G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  /* For GTlsConnection and GDtlsConnection: */
  g_object_class_override_property (gobject_class, PROP_BASE_IO_STREAM, "base-io-stream");
  g_object_class_override_property (gobject_class, PROP_BASE_SOCKET, "base-socket");
  g_object_class_override_property (gobject_class, PROP_REQUIRE_CLOSE_NOTIFY, "require-close-notify");
  g_object_class_override_property (gobject_class, PROP_REHANDSHAKE_MODE, "rehandshake-mode");
  g_object_class_override_property (gobject_class, PROP_USE_SYSTEM_CERTDB, "use-system-certdb");
  g_object_class_override_property (gobject_class, PROP_DATABASE, "database");
  g_object_class_override_property (gobject_class, PROP_CERTIFICATE, "certificate");
  g_object_class_override_property (gobject_class, PROP_INTERACTION, "interaction");
  g_object_class_override_property (gobject_class, PROP_PEER_CERTIFICATE, "peer-certificate");
  g_object_class_override_property (gobject_class, PROP_PEER_CERTIFICATE_ERRORS, "peer-certificate-errors");
  g_object_class_override_property (gobject_class, PROP_ADVERTISED_PROTOCOLS, "advertised-protocols");
  g_object_class_override_property (gobject_class, PROP_NEGOTIATED_PROTOCOL, "negotiated-protocol");
  g_object_class_override_property (gobject_class, PROP_PROTOCOL_VERSION, "protocol-version");
  g_object_class_override_property (gobject_class, PROP_CIPHERSUITE_NAME, "ciphersuite-name");
}

static void
g_tls_connection_base_dtls_connection_iface_init (GDtlsConnectionInterface *iface)
{
  iface->handshake = g_tls_connection_base_dtls_handshake;
  iface->handshake_async = g_tls_connection_base_dtls_handshake_async;
  iface->handshake_finish = g_tls_connection_base_dtls_handshake_finish;
  iface->shutdown = g_tls_connection_base_dtls_shutdown;
  iface->shutdown_async = g_tls_connection_base_dtls_shutdown_async;
  iface->shutdown_finish = g_tls_connection_base_dtls_shutdown_finish;
  iface->set_advertised_protocols = g_tls_connection_base_dtls_set_advertised_protocols;
  iface->get_negotiated_protocol = g_tls_connection_base_dtls_get_negotiated_protocol;
  iface->get_binding_data = g_tls_connection_base_dtls_get_binding_data;
}

static void
g_tls_connection_base_datagram_based_iface_init (GDatagramBasedInterface *iface)
{
  iface->receive_messages = g_tls_connection_base_receive_messages;
  iface->send_messages = g_tls_connection_base_send_messages;
  iface->create_source = g_tls_connection_base_dtls_create_source;
  iface->condition_check = g_tls_connection_base_condition_check;
  iface->condition_wait = g_tls_connection_base_condition_wait;
}
