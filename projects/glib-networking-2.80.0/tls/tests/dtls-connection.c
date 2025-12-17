/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * GIO TLS tests
 *
 * Copyright 2011, 2015, 2016 Collabora, Ltd.
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
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 *         Philip Withnall <philip.withnall@collabora.co.uk>
 */

#include "config.h"

#include "lossy-socket.h"
#include "mock-interaction.h"

#include <gio/gio.h>
#ifdef BACKEND_IS_GNUTLS
#include <gnutls/gnutls.h>
#endif

#include <sys/types.h>
#include <string.h>

static const gchar *
tls_test_file_path (const char *name)
{
  const gchar *const_path;
  gchar *path;

  path = g_test_build_filename (G_TEST_DIST, "files", name, NULL);
  if (!g_path_is_absolute (path))
    {
      gchar *cwd, *abs;

      cwd = g_get_current_dir ();
      abs = g_build_filename (cwd, path, NULL);
      g_free (cwd);
      g_free (path);
      path = abs;
    }

  const_path = g_intern_string (path);
  g_free (path);
  return const_path;
}

#define TEST_DATA "You win again, gravity!\n"
#define TEST_DATA_LENGTH 24

/* Static test parameters. */
typedef struct {
  gint64 server_timeout;  /* microseconds */
  gint64 client_timeout;  /* microseconds */
  gboolean server_should_disappear;  /* whether the server should stop responding before sending a message */
  gboolean server_should_close;  /* whether the server should close gracefully once it’s sent a message */
  GTlsAuthenticationMode auth_mode;
  IOPredicateFunc client_loss_inducer;
  IOPredicateFunc server_loss_inducer;
} TestData;

typedef struct {
  const TestData *test_data;

  GMainContext *client_context;
  GMainContext *server_context;
  gboolean loop_finished;
  GSocket *server_socket;
  GDatagramBased *server_transport;
  GSource *server_source;
  GTlsDatabase *database;
  GDatagramBased *server_connection;
  GDatagramBased *client_connection;
  GSocketConnectable *identity;
  GSocketAddress *address;
  gboolean rehandshake;
  GTlsCertificateFlags accept_flags;
  GError *read_error;
  gboolean expect_server_error;
  GError *server_error;
  gboolean server_running;
  const gchar * const *server_protocols;

  char buf[128];
  gssize nread, nwrote;
} TestConnection;

static void
setup_connection (TestConnection *test, gconstpointer data)
{
  test->test_data = data;

  test->client_context = g_main_context_default ();
  test->loop_finished = FALSE;
}

/* Waits about 10 seconds for @var to be NULL/FALSE */
#define WAIT_UNTIL_UNSET(var)                                     \
  if (var)                                                        \
    {                                                             \
      int i;                                                      \
                                                                  \
      for (i = 0; i < 13 && (var); i++)                           \
        {                                                         \
          g_usleep (1000 * (1 << i));                             \
          g_main_context_iteration (test->client_context, FALSE); \
        }                                                         \
                                                                  \
      g_assert_true (!(var));                                     \
    }

/* Waits about 10 seconds for @var's ref_count to drop to 1 */
#define WAIT_UNTIL_UNREFFED(var)                                  \
  if (var)                                                        \
    {                                                             \
      int i;                                                      \
                                                                  \
      for (i = 0; i < 13 && G_OBJECT (var)->ref_count > 1; i++)   \
        {                                                         \
          g_usleep (1000 * (1 << i));                             \
          g_main_context_iteration (test->client_context, FALSE); \
        }                                                         \
                                                                  \
      g_assert_cmpuint (G_OBJECT (var)->ref_count, ==, 1);        \
    }

static void
teardown_connection (TestConnection *test, gconstpointer data)
{
  GError *error = NULL;

  if (test->server_source)
    {
      g_source_destroy (test->server_source);
      g_source_unref (test->server_source);
      test->server_source = NULL;
    }

  if (test->server_connection)
    {
      WAIT_UNTIL_UNSET (test->server_running);

      WAIT_UNTIL_UNREFFED (test->server_connection);
      g_object_unref (test->server_connection);
      test->server_connection = NULL;
    }

  g_clear_object (&test->server_transport);

  if (test->server_socket)
    {
      g_socket_close (test->server_socket, &error);
      g_assert_no_error (error);

      /* The outstanding accept_async will hold a ref on test->server_socket,
       * which we want to wait for it to release if we're valgrinding.
       */
      WAIT_UNTIL_UNREFFED (test->server_socket);
      g_object_unref (test->server_socket);
      test->server_socket = NULL;
    }

  if (test->client_connection)
    {
      WAIT_UNTIL_UNREFFED (test->client_connection);
      g_object_unref (test->client_connection);
      test->client_connection = NULL;
    }

  if (test->database)
    {
      WAIT_UNTIL_UNREFFED (test->database);
      g_object_unref (test->database);
      test->database = NULL;
    }

  g_clear_object (&test->address);
  g_clear_object (&test->identity);
  g_clear_error (&test->read_error);
  g_clear_error (&test->server_error);
}

static void
start_server (TestConnection *test)
{
  GInetAddress *inet;
  GSocketAddress *addr;
  GInetSocketAddress *iaddr;
  GSocket *socket = NULL;
  GError *error = NULL;

  inet = g_inet_address_new_from_string ("127.0.0.1");
  addr = g_inet_socket_address_new (inet, 0);
  g_object_unref (inet);

  socket = g_socket_new (G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_DATAGRAM,
                         G_SOCKET_PROTOCOL_UDP, &error);
  g_assert_no_error (error);

  g_socket_bind (socket, addr, FALSE, &error);
  g_assert_no_error (error);

  test->address = g_socket_get_local_address (socket, &error);
  g_assert_no_error (error);

  g_object_unref (addr);

  /* The hostname in test->identity matches the server certificate. */
  iaddr = G_INET_SOCKET_ADDRESS (test->address);
  test->identity = g_network_address_new ("server.example.com",
                                          g_inet_socket_address_get_port (iaddr));

  test->server_socket = socket;
  if (test->test_data->server_loss_inducer)
    {
      test->server_transport = lossy_socket_new (G_DATAGRAM_BASED (socket),
                                                 test->test_data->server_loss_inducer,
                                                 test);
    }
  else
    {
      test->server_transport = G_DATAGRAM_BASED (g_object_ref (socket));
    }
  test->server_running = TRUE;
}

static gboolean
on_accept_certificate (GTlsClientConnection *conn, GTlsCertificate *cert,
                       GTlsCertificateFlags errors, gpointer user_data)
{
  TestConnection *test = user_data;
  return errors == test->accept_flags;
}

static void close_server_connection (TestConnection *test,
                                     gboolean        graceful);

static void
on_rehandshake_finish (GObject        *object,
                       GAsyncResult   *res,
                       gpointer        user_data)
{
  TestConnection *test = user_data;
  GError *error = NULL;
  GOutputVector vectors[2] = {
    { TEST_DATA + TEST_DATA_LENGTH / 2, TEST_DATA_LENGTH / 4 },
    { TEST_DATA + 3 * TEST_DATA_LENGTH / 4, TEST_DATA_LENGTH / 4},
  };
  GOutputMessage message = { NULL, vectors, G_N_ELEMENTS (vectors), 0, NULL, 0 };
  gint n_sent;

  g_dtls_connection_handshake_finish (G_DTLS_CONNECTION (object), res, &error);
  g_assert_no_error (error);

  do
    {
      g_clear_error (&test->server_error);
      n_sent = g_datagram_based_send_messages (test->server_connection,
                                               &message, 1,
                                               G_SOCKET_MSG_NONE, 0, NULL,
                                               &test->server_error);
      g_main_context_iteration (test->server_context, FALSE);
    }
  while (g_error_matches (test->server_error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK));

  if (!test->server_error)
    {
      g_assert_cmpint (n_sent, ==, 1);
      g_assert_cmpuint (message.bytes_sent, ==, TEST_DATA_LENGTH / 2);
    }

  if (!test->server_error && test->rehandshake)
    {
      test->rehandshake = FALSE;
      g_dtls_connection_handshake_async (G_DTLS_CONNECTION (test->server_connection),
                                         G_PRIORITY_DEFAULT, NULL,
                                         on_rehandshake_finish, test);
      return;
    }

  if (test->test_data->server_should_close)
    close_server_connection (test, TRUE);
}

static void
on_rehandshake_finish_threaded (GObject      *object,
                                GAsyncResult *res,
                                gpointer      user_data)
{
  TestConnection *test = user_data;
  GError *error = NULL;
  GOutputVector vectors[2] = {
    { TEST_DATA + TEST_DATA_LENGTH / 2, TEST_DATA_LENGTH / 4 },
    { TEST_DATA + 3 * TEST_DATA_LENGTH / 4, TEST_DATA_LENGTH / 4},
  };
  GOutputMessage message = { NULL, vectors, G_N_ELEMENTS (vectors), 0, NULL, 0 };
  gint n_sent;

  g_dtls_connection_handshake_finish (G_DTLS_CONNECTION (object), res, &error);
  g_assert_no_error (error);

  do
    {
      g_clear_error (&test->server_error);
      n_sent = g_datagram_based_send_messages (test->server_connection,
                                               &message, 1,
                                               G_SOCKET_MSG_NONE, 0, NULL,
                                               &test->server_error);
      g_main_context_iteration (test->server_context, FALSE);
    }
  while (g_error_matches (test->server_error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK));

  if (!test->server_error)
    {
      g_assert_cmpint (n_sent, ==, 1);
      g_assert_cmpuint (message.bytes_sent, ==, TEST_DATA_LENGTH / 2);
    }

  if (!test->server_error && test->rehandshake)
    {
      test->rehandshake = FALSE;
      g_dtls_connection_handshake_async (G_DTLS_CONNECTION (test->server_connection),
                                         G_PRIORITY_DEFAULT, NULL,
                                         on_rehandshake_finish_threaded, test);
      return;
    }

  if (test->test_data->server_should_close)
    close_server_connection (test, TRUE);
}

static void
close_server_connection (TestConnection *test,
                         gboolean        graceful)
{
  GError *error = NULL;

  if (graceful)
    g_dtls_connection_close (G_DTLS_CONNECTION (test->server_connection),
                             NULL, &error);

  /* Clear pending dispatches from the context. */
  while (g_main_context_iteration (test->server_context, FALSE));

  if (graceful && test->expect_server_error)
    g_assert_nonnull (error);
  else if (graceful)
    g_assert_no_error (error);

  test->server_running = FALSE;
}

static gboolean
on_incoming_connection (GSocket       *socket,
                        GIOCondition   condition,
                        gpointer       user_data)
{
  TestConnection *test = user_data;
  GTlsCertificate *cert;
  GError *error = NULL;
  GOutputVector vector = {
    TEST_DATA,
    test->rehandshake ? TEST_DATA_LENGTH / 2 : TEST_DATA_LENGTH
  };
  GOutputMessage message = { NULL, &vector, 1, 0, NULL, 0 };
  gint n_sent;
  GSocketAddress *addr = NULL;  /* owned */
  guint8 databuf[65536];
  GInputVector vec = {databuf, sizeof (databuf)};
  gint flags = G_SOCKET_MSG_PEEK;
  gssize ret;

  /* Ignore this if the source has already been destroyed. */
  if (g_source_is_destroyed (test->server_source))
    return G_SOURCE_REMOVE;

  /* Remove the source as the first thing. */
  g_source_destroy (test->server_source);
  g_source_unref (test->server_source);
  test->server_source = NULL;

  /* Peek at the incoming packet to get the peer’s address. */
  ret = g_socket_receive_message (socket, &addr, &vec, 1, NULL, NULL,
                                  &flags, NULL, NULL);

  if (ret <= 0)
    return G_SOURCE_REMOVE;

  if (!g_socket_connect (socket, addr, NULL, NULL))
    {
      g_object_unref (addr);
      return G_SOURCE_CONTINUE;
    }

  g_clear_object (&addr);

  /* Wrap the socket in a GDtlsServerConnection. */
  cert = g_tls_certificate_new_from_file (tls_test_file_path ("server-and-key.pem"), &error);
  g_assert_no_error (error);

  test->server_connection = g_dtls_server_connection_new (test->server_transport,
                                                          cert, &error);
  g_debug ("%s: Server connection %p on socket %p", G_STRFUNC, test->server_connection, socket);
  g_assert_no_error (error);
  g_object_unref (cert);

  g_object_set (test->server_connection, "authentication-mode",
                test->test_data->auth_mode, NULL);
  g_signal_connect (test->server_connection, "accept-certificate",
                    G_CALLBACK (on_accept_certificate), test);

  if (test->database)
    g_dtls_connection_set_database (G_DTLS_CONNECTION (test->server_connection), test->database);

  if (test->server_protocols)
    {
      g_dtls_connection_set_advertised_protocols (G_DTLS_CONNECTION (test->server_connection),
                                                  test->server_protocols);
    }

  if (test->test_data->server_should_disappear)
    {
      close_server_connection (test, FALSE);
      return G_SOURCE_REMOVE;
    }

  do
    {
      g_clear_error (&test->server_error);
      n_sent = g_datagram_based_send_messages (test->server_connection,
                                               &message, 1,
                                               G_SOCKET_MSG_NONE, 0, NULL,
                                               &test->server_error);
      g_main_context_iteration (test->server_context, FALSE);
    }
  while (g_error_matches (test->server_error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK));

  if (!test->server_error)
    {
      g_assert_cmpint (n_sent, ==, 1);
      g_assert_cmpuint (message.bytes_sent, ==, vector.size);
    }

  if (!test->server_error && test->rehandshake)
    {
      test->rehandshake = FALSE;
      g_dtls_connection_handshake_async (G_DTLS_CONNECTION (test->server_connection),
                                         G_PRIORITY_DEFAULT, NULL,
                                         on_rehandshake_finish, test);
      return G_SOURCE_REMOVE;
    }

  if (test->test_data->server_should_close)
    close_server_connection (test, TRUE);

  return G_SOURCE_REMOVE;
}

static gboolean
on_incoming_connection_threaded (GSocket      *socket,
                                 GIOCondition  condition,
                                 gpointer      user_data)
{
  TestConnection *test = user_data;
  GTlsCertificate *cert;
  GError *error = NULL;
  GOutputVector vector = {
    TEST_DATA,
    test->rehandshake ? TEST_DATA_LENGTH / 2 : TEST_DATA_LENGTH
  };
  GOutputMessage message = { NULL, &vector, 1, 0, NULL, 0 };
  gint n_sent;
  GSocketAddress *addr = NULL;  /* owned */
  guint8 databuf[65536];
  GInputVector vec = {databuf, sizeof (databuf)};
  gint flags = G_SOCKET_MSG_PEEK;
  gssize ret;

  /* Ignore this if the source has already been destroyed. */
  if (g_source_is_destroyed (test->server_source))
    return G_SOURCE_REMOVE;

  /* Remove the source as the first thing. */
  g_source_destroy (test->server_source);
  g_source_unref (test->server_source);
  test->server_source = NULL;

  /* Peek at the incoming packet to get the peer’s address. */
  ret = g_socket_receive_message (socket, &addr, &vec, 1, NULL, NULL,
                                  &flags, NULL, NULL);

  if (ret <= 0)
    return G_SOURCE_REMOVE;

  if (!g_socket_connect (socket, addr, NULL, NULL))
    {
      g_object_unref (addr);
      return G_SOURCE_CONTINUE;
    }

  g_clear_object (&addr);

  /* Wrap the socket in a GDtlsServerConnection. */
  cert = g_tls_certificate_new_from_file (tls_test_file_path ("server-and-key.pem"), &error);
  g_assert_no_error (error);

  test->server_connection = g_dtls_server_connection_new (test->server_transport,
                                                          cert, &error);
  g_debug ("%s: Server connection %p on socket %p", G_STRFUNC, test->server_connection, socket);
  g_assert_no_error (error);
  g_object_unref (cert);

  g_object_set (test->server_connection, "authentication-mode",
                test->test_data->auth_mode, NULL);
  g_signal_connect (test->server_connection, "accept-certificate",
                    G_CALLBACK (on_accept_certificate), test);

  if (test->database)
    g_dtls_connection_set_database (G_DTLS_CONNECTION (test->server_connection), test->database);

  if (test->test_data->server_should_disappear)
    {
      close_server_connection (test, FALSE);
      return G_SOURCE_REMOVE;
    }

  do
    {
      g_clear_error (&test->server_error);
      n_sent = g_datagram_based_send_messages (test->server_connection,
                                               &message, 1,
                                               G_SOCKET_MSG_NONE,
                                               test->test_data->server_timeout, NULL,
                                               &test->server_error);
      g_main_context_iteration (test->server_context, FALSE);
    }
  while (g_error_matches (test->server_error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK));

  if (!test->server_error)
    {
      g_assert_cmpint (n_sent, ==, 1);
      g_assert_cmpuint (message.bytes_sent, ==, vector.size);
    }

  if (!test->server_error && test->rehandshake)
    {
      test->rehandshake = FALSE;
      g_dtls_connection_handshake_async (G_DTLS_CONNECTION (test->server_connection),
                                         G_PRIORITY_DEFAULT, NULL,
                                         on_rehandshake_finish_threaded, test);
      return G_SOURCE_REMOVE;
    }

  if (test->test_data->server_should_close)
    close_server_connection (test, TRUE);

  return G_SOURCE_REMOVE;
}

static gpointer
server_service_cb (gpointer user_data)
{
  TestConnection *test = user_data;

  test->server_context = g_main_context_new ();
  g_main_context_push_thread_default (test->server_context);

  test->server_source = g_socket_create_source (test->server_socket, G_IO_IN,
                                                NULL);
  g_source_set_callback (test->server_source,
                         (GSourceFunc) on_incoming_connection_threaded, test, NULL);
  g_source_attach (test->server_source, test->server_context);

  /* Run the server until it should stop. */
  while (test->server_running)
    g_main_context_iteration (test->server_context, TRUE);

  g_main_context_pop_thread_default (test->server_context);

  return NULL;
}

static void
start_server_service (TestConnection         *test,
                      gboolean                threaded)
{
  start_server (test);

  if (threaded)
    {
      g_thread_new ("dtls-server", server_service_cb, test);
      return;
    }

  test->server_source = g_socket_create_source (test->server_socket, G_IO_IN,
                                                NULL);
  g_source_set_callback (test->server_source,
                         (GSourceFunc) on_incoming_connection, test, NULL);
  g_source_attach (test->server_source, NULL);
}

static GDatagramBased *
start_server_and_connect_to_it (TestConnection         *test,
                                gboolean                threaded)
{
  GError *error = NULL;
  GSocket *socket;
  GDatagramBased *transport;

  start_server_service (test, threaded);

  socket = g_socket_new (G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_DATAGRAM,
                         G_SOCKET_PROTOCOL_UDP, &error);
  g_assert_no_error (error);

  g_socket_connect (socket, test->address, NULL, &error);
  g_assert_no_error (error);

  if (test->test_data->client_loss_inducer)
    {
      transport = lossy_socket_new (G_DATAGRAM_BASED (socket),
                                    test->test_data->client_loss_inducer,
                                    test);
      g_object_unref (socket);
    }
  else
    {
      transport = G_DATAGRAM_BASED (socket);
    }

  return transport;
}

static void
read_test_data_async (TestConnection *test)
{
  gchar *check;
  GError *error = NULL;
  guint8 buf[TEST_DATA_LENGTH * 2];
  GInputVector vectors[2] = {
    { buf, sizeof (buf) / 2 },
    { buf + sizeof (buf) / 2, sizeof (buf) / 2 },
  };
  GInputMessage message = { NULL, vectors, G_N_ELEMENTS (vectors), 0, 0, NULL, NULL };
  gint n_read;

  do
    {
      g_clear_error (&test->read_error);
      n_read = g_datagram_based_receive_messages (test->client_connection,
                                                  &message, 1,
                                                  G_SOCKET_MSG_NONE,
                                                  test->test_data->client_timeout,
                                                  NULL, &test->read_error);
      g_main_context_iteration (test->client_context, FALSE);
    }
  while (g_error_matches (test->read_error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK));

  if (!test->read_error)
    {
      g_assert_cmpint (n_read, ==, 1);

      check = g_strdup (TEST_DATA);
      g_assert_cmpuint (strlen (check), ==, message.bytes_received);
      g_assert_cmpint (strncmp (check, (const char *)buf, message.bytes_received), ==, 0);
      g_free (check);
    }

  g_dtls_connection_close (G_DTLS_CONNECTION (test->client_connection),
                           NULL, &error);
  g_assert_no_error (error);

  test->loop_finished = TRUE;
}

/* Test that connecting a client to a server, both using main contexts in the
 * same thread, works; and that sending a message from the server to the client
 * before shutting down gracefully works. */
static void
test_basic_connection (TestConnection *test,
                       gconstpointer   data)
{
  GDatagramBased *connection;
  GError *error = NULL;

  connection = start_server_and_connect_to_it (test, FALSE);
  test->client_connection = g_dtls_client_connection_new (connection, test->identity, &error);
  g_debug ("%s: Client connection %p on socket %p", G_STRFUNC, test->client_connection, connection);
  g_assert_no_error (error);
  g_object_unref (connection);

  /* No validation at all in this test */
  g_dtls_client_connection_set_validation_flags (G_DTLS_CLIENT_CONNECTION (test->client_connection),
                                                 0);

  read_test_data_async (test);
  while (!test->loop_finished)
    g_main_context_iteration (test->client_context, TRUE);

  g_assert_no_error (test->server_error);
  g_assert_no_error (test->read_error);
}

/* Test that connecting a client to a server, both using separate threads,
 * works; and that sending a message from the server to the client before
 * shutting down gracefully works. */
static void
test_threaded_connection (TestConnection *test,
                          gconstpointer   data)
{
  GDatagramBased *connection;
  GError *error = NULL;

  connection = start_server_and_connect_to_it (test, TRUE);
  test->client_connection = g_dtls_client_connection_new (connection, test->identity, &error);
  g_debug ("%s: Client connection %p on socket %p", G_STRFUNC, test->client_connection, connection);
  g_assert_no_error (error);
  g_object_unref (connection);

  /* No validation at all in this test */
  g_dtls_client_connection_set_validation_flags (G_DTLS_CLIENT_CONNECTION (test->client_connection),
                                                 0);

  read_test_data_async (test);
  while (!test->loop_finished)
    g_main_context_iteration (test->client_context, TRUE);

  g_assert_no_error (test->server_error);
  g_assert_no_error (test->read_error);
}

/* Test that a client can successfully connect to a server, then the server
 * disappears, and when the client tries to read from it, the client hits a
 * timeout error (rather than blocking indefinitely or returning another
 * error). */
static void
test_connection_timeouts_read (TestConnection *test,
                               gconstpointer   data)
{
  GDatagramBased *connection;
  GError *error = NULL;

  connection = start_server_and_connect_to_it (test, TRUE);
  test->client_connection = g_dtls_client_connection_new (connection,
                                                          test->identity, &error);
  g_debug ("%s: Client connection %p on socket %p", G_STRFUNC,
           test->client_connection, connection);
  g_assert_no_error (error);
  g_object_unref (connection);

  /* No validation at all in this test */
  g_dtls_client_connection_set_validation_flags (G_DTLS_CLIENT_CONNECTION (test->client_connection),
                                                 0);

  read_test_data_async (test);
  while (!test->loop_finished)
    g_main_context_iteration (test->client_context, TRUE);

  g_assert_no_error (test->server_error);
  g_assert_error (test->read_error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT);
}

static IODecision
drop_first_outgoing (const IODetails *io,
                     gpointer         user_data)
{
  if (io->direction == IO_OUT && io->serial == 1)
    return IO_DROP;

  return IO_KEEP;
}

int
main (int   argc,
      char *argv[])
{
  const TestData blocking = {
    -1,  /* server_timeout */
    0,  /* client_timeout */
    FALSE,  /* server_should_disappear */
    TRUE, /* server_should_close */
    G_TLS_AUTHENTICATION_NONE,  /* auth_mode */
    NULL, NULL, /* loss inducers */
  };
  const TestData server_timeout = {
    1000 * G_USEC_PER_SEC,  /* server_timeout */
    0,  /* client_timeout */
    FALSE,  /* server_should_disappear */
    TRUE, /* server_should_close */
    G_TLS_AUTHENTICATION_NONE,  /* auth_mode */
    NULL, NULL, /* loss inducers */
  };
  const TestData nonblocking = {
    0,  /* server_timeout */
    0,  /* client_timeout */
    FALSE,  /* server_should_disappear */
    TRUE, /* server_should_close */
    G_TLS_AUTHENTICATION_NONE,  /* auth_mode */
    NULL, NULL, /* loss inducers */
  };
  const TestData client_timeout = {
    0,  /* server_timeout */
    (gint64) (0.5 * G_USEC_PER_SEC),  /* client_timeout */
    TRUE,  /* server_should_disappear */
    TRUE, /* server_should_close */
    G_TLS_AUTHENTICATION_NONE,  /* auth_mode */
    NULL, NULL, /* loss inducers */
  };
  const TestData client_loss = {
    -1,  /* server_timeout */
    0,  /* client_timeout */
    FALSE,  /* server_should_disappear */
    TRUE, /* server_should_close */
    G_TLS_AUTHENTICATION_NONE,  /* auth_mode */
    drop_first_outgoing, NULL, /* loss inducers */
  };
  const TestData server_loss = {
    -1,  /* server_timeout */
    0,  /* client_timeout */
    FALSE,  /* server_should_disappear */
    TRUE, /* server_should_close */
    G_TLS_AUTHENTICATION_NONE,  /* auth_mode */
    NULL, drop_first_outgoing, /* loss inducers */
  };
  int ret;
  int i;

  /* Check if this is a subprocess, and set G_TLS_GNUTLS_PRIORITY
   * appropriately if so.
   */
  for (i = 1; i < argc - 1; i++)
    {
      if (!strcmp (argv[i], "-p"))
        {
          const char *priority = argv[i + 1];

          priority = strrchr (priority, '/');
          if (priority++ &&
              (g_str_has_prefix (priority, "NORMAL:") ||
               g_str_has_prefix (priority, "NONE:")))
            g_setenv ("G_TLS_GNUTLS_PRIORITY", priority, TRUE);
          break;
        }
    }

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);
  g_setenv ("GIO_USE_TLS", BACKEND, TRUE);
  g_assert_cmpint (g_ascii_strcasecmp (G_OBJECT_TYPE_NAME (g_tls_backend_get_default ()), "GTlsBackend" BACKEND), ==, 0);

  g_test_add ("/dtls/" BACKEND "/connection/basic/blocking", TestConnection, &blocking,
              setup_connection, test_basic_connection, teardown_connection);
  g_test_add ("/dtls/" BACKEND "/connection/basic/timeout", TestConnection, &server_timeout,
              setup_connection, test_basic_connection, teardown_connection);
  g_test_add ("/dtls/" BACKEND "/connection/basic/nonblocking",
              TestConnection, &nonblocking,
              setup_connection, test_basic_connection, teardown_connection);

  g_test_add ("/dtls/" BACKEND "/connection/threaded/blocking", TestConnection, &blocking,
              setup_connection, test_threaded_connection, teardown_connection);
  g_test_add ("/dtls/" BACKEND "/connection/threaded/timeout",
              TestConnection, &server_timeout,
              setup_connection, test_threaded_connection, teardown_connection);
  g_test_add ("/dtls/" BACKEND "/connection/threaded/nonblocking",
              TestConnection, &nonblocking,
              setup_connection, test_threaded_connection, teardown_connection);

  g_test_add ("/dtls/" BACKEND "/connection/timeouts/read", TestConnection, &client_timeout,
              setup_connection, test_connection_timeouts_read,
              teardown_connection);

  g_test_add ("/dtls/" BACKEND "/connection/lossy/client", TestConnection, &client_loss,
              setup_connection, test_basic_connection, teardown_connection);
  g_test_add ("/dtls/" BACKEND "/connection/lossy/server", TestConnection, &server_loss,
              setup_connection, test_basic_connection, teardown_connection);

  ret = g_test_run ();

  /* for valgrinding */
  g_main_context_unref (g_main_context_default ());

  return ret;
}
