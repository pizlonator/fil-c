/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

#include "test-utils.h"
#include "soup-session-private.h"

static GUri *base_uri;
static gboolean server_processed_message;
static gboolean timeout;
static GMainLoop *loop;
static SoupMessagePriority expected_priorities[3];
static GBytes *index_bytes;

static gboolean
timeout_cb (gpointer user_data)
{
	gboolean *timeout = user_data;

	*timeout = TRUE;
	return FALSE;
}

static void
server_handler (SoupServer        *server,
		SoupServerMessage *msg,
		const char        *path,
		GHashTable        *query,
		gpointer           user_data)
{
	if (!strcmp (path, "/request-timeout")) {
		GMainContext *context = g_main_context_get_thread_default ();
		GSource *timer;

		timer = g_timeout_source_new (100);
		g_source_set_callback (timer, timeout_cb, &timeout, NULL);
		g_source_attach (timer, context);
		g_source_unref (timer);
	} else if (!strcmp (path, "/index.txt")) {
		soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);
		soup_server_message_set_response (msg, "text/plain",
						  SOUP_MEMORY_STATIC,
						  g_bytes_get_data (index_bytes, NULL),
						  g_bytes_get_size (index_bytes));
		return;
	} else
		server_processed_message = TRUE;

	soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);
	soup_server_message_set_response (msg, "text/plain",
					  SOUP_MEMORY_STATIC,
					  "ok\r\n", 4);
}

static void
finished_cb (SoupMessage *msg,
	     gboolean    *finished)
{
	*finished = TRUE;
}

static void
cancel_message_cb (SoupMessage  *msg,
		   GCancellable *cancellable)
{
	g_cancellable_cancel (cancellable);
}

static void
cancel_message_send_done (SoupSession  *session,
			  GAsyncResult *result,
			  GError      **error)
{
	g_assert_null (soup_session_send_finish (session, result, error));
	g_main_loop_quit (loop);
}

static void
do_test_for_session (SoupSession *session,
		     gboolean queue_is_async,
		     gboolean send_is_blocking)
{
	SoupMessage *msg;
	gboolean finished, local_timeout;
	guint timeout_id;
	GUri *timeout_uri;
	GBytes *body;
	GCancellable *cancellable;
	GError *error = NULL;

	debug_printf (1, "  queue_message\n");
	debug_printf (2, "    requesting timeout\n");
	timeout_uri = g_uri_parse_relative (base_uri, "/request-timeout", SOUP_HTTP_URI_FLAGS, NULL);
	msg = soup_message_new_from_uri ("GET", timeout_uri);
	body = soup_session_send_and_read (session, msg, NULL, NULL);
	g_bytes_unref (body);
	g_uri_unref (timeout_uri);

	msg = soup_message_new_from_uri ("GET", base_uri);
	server_processed_message = timeout = finished = FALSE;
	g_signal_connect (msg, "finished",
			  G_CALLBACK (finished_cb), &finished);
	soup_session_send_async (session, msg, G_PRIORITY_DEFAULT, NULL, NULL, NULL);
	g_object_unref (msg);
	while (!timeout)
		g_usleep (100);
	debug_printf (2, "    got timeout\n");

	if (queue_is_async) {
		g_assert_false (server_processed_message);
		debug_printf (2, "    waiting for finished\n");
		while (!finished)
			g_main_context_iteration (NULL, TRUE);
		g_assert_true (server_processed_message);
	} else {
		g_assert_true (server_processed_message);
		g_assert_false (finished);
		debug_printf (2, "    waiting for finished\n");
		while (!finished)
			g_main_context_iteration (NULL, TRUE);
	}

	debug_printf (1, "  send_message\n");
	msg = soup_message_new_from_uri ("GET", base_uri);
	server_processed_message = local_timeout = FALSE;
	timeout_id = g_idle_add_full (G_PRIORITY_HIGH, timeout_cb, &local_timeout, NULL);
	body = soup_session_send_and_read (session, msg, NULL, NULL);
        g_bytes_unref (body);
	g_object_unref (msg);

	g_assert_true (server_processed_message);

	if (send_is_blocking) {
		soup_test_assert (!local_timeout,
				  "send_message ran main loop");
	} else {
		soup_test_assert (local_timeout,
				  "send_message didn't run main loop");
	}

	if (!local_timeout)
		g_source_remove (timeout_id);

	if (!queue_is_async)
		return;

	debug_printf (1, "  cancel_message\n");
	msg = soup_message_new_from_uri ("GET", base_uri);
	cancellable = g_cancellable_new ();
	soup_session_send_async (session, msg, G_PRIORITY_DEFAULT, cancellable,
				 (GAsyncReadyCallback)cancel_message_send_done,
				 &error);
	g_signal_connect (msg, "wrote-headers",
			  G_CALLBACK (cancel_message_cb), cancellable);

	loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (loop);

	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);

	g_main_loop_unref (loop);
	g_clear_error (&error);
	g_object_unref (cancellable);
	g_object_unref (msg);
}

static void
do_plain_tests (void)
{
	SoupSession *session;

	session = soup_test_session_new (NULL);
	do_test_for_session (session, TRUE, TRUE);
	soup_test_session_abort_unref (session);
}

static void
priority_test_finished_cb (SoupMessage *msg,
			   guint       *finished_count)
{
	SoupMessagePriority priority = soup_message_get_priority (msg);

	debug_printf (1, "  received message %d with priority %d\n",
		      *finished_count, priority);

	soup_test_assert (priority == expected_priorities[*finished_count],
			  "message %d should have priority %d (%d found)",
			  *finished_count, expected_priorities[*finished_count], priority);

	(*finished_count)++;
}

static void
do_priority_tests (void)
{
	SoupSession *session;
	int i, finished_count = 0;
	SoupMessagePriority priorities[] =
		{ SOUP_MESSAGE_PRIORITY_LOW,
		  SOUP_MESSAGE_PRIORITY_HIGH,
		  SOUP_MESSAGE_PRIORITY_NORMAL };

	g_test_bug ("696277");

	session = soup_test_session_new ("max-conns", 1, NULL);

	expected_priorities[0] = SOUP_MESSAGE_PRIORITY_HIGH;
	expected_priorities[1] = SOUP_MESSAGE_PRIORITY_NORMAL;
	expected_priorities[2] = SOUP_MESSAGE_PRIORITY_LOW;

	for (i = 0; i < 3; i++) {
		GUri *msg_uri;
		SoupMessage *msg;
		char buf[5];

		g_snprintf (buf, sizeof (buf), "%d", i);
		msg_uri = g_uri_parse_relative (base_uri, buf, SOUP_HTTP_URI_FLAGS, NULL);
		msg = soup_message_new_from_uri ("GET", msg_uri);
		g_uri_unref (msg_uri);

		soup_message_set_priority (msg, priorities[i]);
		g_signal_connect (msg, "finished",
				  G_CALLBACK (priority_test_finished_cb), &finished_count);
		soup_session_send_async (session, msg, G_PRIORITY_DEFAULT, NULL, NULL, NULL);
		g_object_unref (msg);
	}

	debug_printf (2, "    waiting for finished\n");
	while (finished_count != 3)
		g_main_context_iteration (NULL, TRUE);

	soup_test_session_abort_unref (session);
}

static void
do_priority_change_test (void)
{
        SoupSession *session;
        SoupMessage *msgs[3];
        int i, finished_count = 0;
        SoupMessagePriority priorities[] =
                { SOUP_MESSAGE_PRIORITY_LOW,
                  SOUP_MESSAGE_PRIORITY_HIGH,
                  SOUP_MESSAGE_PRIORITY_NORMAL };

        session = soup_test_session_new ("max-conns", 1, NULL);

        expected_priorities[0] = SOUP_MESSAGE_PRIORITY_HIGH;
        expected_priorities[1] = SOUP_MESSAGE_PRIORITY_LOW;
        expected_priorities[2] = SOUP_MESSAGE_PRIORITY_VERY_LOW;

        for (i = 0; i < 3; i++) {
                GUri *msg_uri;
                char buf[5];

                g_snprintf (buf, sizeof (buf), "%d", i);
                msg_uri = g_uri_parse_relative (base_uri, buf, SOUP_HTTP_URI_FLAGS, NULL);
                msgs[i] = soup_message_new_from_uri ("GET", msg_uri);
                g_uri_unref (msg_uri);

                soup_message_set_priority (msgs[i], priorities[i]);
                g_signal_connect (msgs[i], "finished",
                                  G_CALLBACK (priority_test_finished_cb), &finished_count);
                soup_session_send_async (session, msgs[i], G_PRIORITY_DEFAULT, NULL, NULL, NULL);
        }

        soup_message_set_priority (msgs[2], SOUP_MESSAGE_PRIORITY_VERY_LOW);

        debug_printf (2, "    waiting for finished\n");
        while (finished_count != 3)
                g_main_context_iteration (NULL, TRUE);

        for (i = 0; i < 3; i++)
                g_object_unref (msgs[i]);

        soup_test_session_abort_unref (session);
}

static void
test_session_properties (const char *name,
			 SoupSession *session,
			 GProxyResolver *expected_proxy_resolver,
			 GTlsDatabase *expected_tls_database)
{
	GProxyResolver *proxy_resolver = soup_session_get_proxy_resolver (session);
	GTlsDatabase *tlsdb = soup_session_get_tls_database (session);

	soup_test_assert (proxy_resolver == expected_proxy_resolver,
			  "%s has %s proxy resolver",
			  name, proxy_resolver ? (expected_proxy_resolver ? "wrong" : "a") : "no");
	soup_test_assert (tlsdb == expected_tls_database,
			  "%s has %s TLS database",
			  name, tlsdb ? (expected_tls_database ? "wrong" : "a") : "no");
}

static void
do_property_tests (void)
{
	SoupSession *session;
	GProxyResolver *proxy_resolver, *default_proxy_resolver;
	GTlsDatabase *tlsdb, *default_tlsdb;

	g_test_bug ("708696");

	default_proxy_resolver = g_proxy_resolver_get_default ();
	default_tlsdb = g_tls_backend_get_default_database (g_tls_backend_get_default ());

	/* NOTE: We intentionally do not use soup_test_session_new() here */

	session = g_object_new (SOUP_TYPE_SESSION,
				NULL);
	test_session_properties ("Base plain session", session,
				 default_proxy_resolver, default_tlsdb);
	g_object_unref (session);

	session = g_object_new (SOUP_TYPE_SESSION,
				"proxy-resolver", NULL,
				NULL);
	test_session_properties ("Session with NULL :proxy-resolver", session,
				 NULL, default_tlsdb);
	g_object_unref (session);

	proxy_resolver = g_simple_proxy_resolver_new (NULL, NULL);
	session = g_object_new (SOUP_TYPE_SESSION,
				"proxy-resolver", proxy_resolver,
				NULL);
	test_session_properties ("Session with non-NULL :proxy-resolver", session,
				 proxy_resolver, default_tlsdb);
	g_object_unref (proxy_resolver);
	g_object_unref (session);

	session = g_object_new (SOUP_TYPE_SESSION,
				"tls-database", NULL,
				NULL);
	test_session_properties ("Session with NULL :tls-database", session,
				 default_proxy_resolver, NULL);
	g_object_unref (session);

	/* g_tls_file_database_new() will fail with the dummy backend,
	 * so we can only do this test if we have a real TLS backend.
	 */
	if (tls_available) {
		GError *error = NULL;
                char *db_path;

                db_path = soup_test_build_filename_abs (G_TEST_DIST, "test-cert.pem", NULL);
		tlsdb = g_tls_file_database_new (db_path, &error);
		g_assert_no_error (error);
                g_free (db_path);

		session = g_object_new (SOUP_TYPE_SESSION,
					"tls-database", tlsdb,
					NULL);
		test_session_properties ("Session with non-NULL :tls-database", session,
					 default_proxy_resolver, tlsdb);
		g_object_unref (tlsdb);
		g_object_unref (session);
	}
}

static gint
compare_by_gtype (gconstpointer a,
		  gconstpointer b)
{
	return G_TYPE_CHECK_INSTANCE_TYPE (a, b) ? 0 : 1;
}

static void
do_features_test (void)
{
	SoupSession *session;
	GSList *features;
	SoupSessionFeature *feature;

	session = soup_test_session_new (NULL);

	features = soup_session_get_features (session, SOUP_TYPE_SESSION_FEATURE);
	/* SoupAuthManager is always added */
	g_assert_cmpuint (g_slist_length (features), >=, 1);
	g_assert_nonnull (g_slist_find_custom (features, GSIZE_TO_POINTER (SOUP_TYPE_AUTH_MANAGER), compare_by_gtype));
	g_assert_true (soup_session_has_feature (session, SOUP_TYPE_AUTH_MANAGER));
	feature = soup_session_get_feature (session, SOUP_TYPE_AUTH_MANAGER);
	g_assert_true (SOUP_IS_AUTH_MANAGER (feature));
	soup_session_remove_feature (session, feature);
	g_assert_false (soup_session_has_feature (session, SOUP_TYPE_AUTH_MANAGER));
	g_assert_null (soup_session_get_feature (session, SOUP_TYPE_AUTH_MANAGER));
	g_slist_free (features);

	soup_test_session_abort_unref (session);
}

static void
queue_order_test_message_finished (SoupMessage *msg,
				   guint       *finished_count)
{
	(*finished_count)++;
}

static void
queue_order_test_message_network_event (SoupMessage       *msg,
					GSocketClientEvent event,
					GIOStream         *connection,
					SoupMessage      **queue)
{
	int i;

	if (event != G_SOCKET_CLIENT_RESOLVING)
                return;

	for (i = 0; i < 3; i++) {
		if (queue[i] == NULL) {
			queue[i] = msg;
			return;
		}
	}
	g_assert_not_reached ();
}

static void
do_queue_order_test (void)
{
	SoupSession *session;
	SoupMessage *msg1, *msg2, *msg3;
	guint finished_count = 0;
	SoupMessage *queue[3] = { NULL, NULL, NULL };

	session = soup_test_session_new (NULL);

	msg1 = soup_message_new_from_uri ("GET", base_uri);
	g_signal_connect (msg1, "network-event",
			  G_CALLBACK (queue_order_test_message_network_event),
			  queue);
	g_signal_connect (msg1, "finished",
			  G_CALLBACK (queue_order_test_message_finished),
			  &finished_count);
	msg2 = soup_message_new_from_uri ("GET", base_uri);
	g_signal_connect (msg2, "network-event",
			  G_CALLBACK (queue_order_test_message_network_event),
			  queue);
	g_signal_connect (msg2, "finished",
			  G_CALLBACK (queue_order_test_message_finished),
			  &finished_count);
	msg3 = soup_message_new_from_uri ("GET", base_uri);
	g_signal_connect (msg3, "network-event",
			  G_CALLBACK (queue_order_test_message_network_event),
			  queue);
	g_signal_connect (msg3, "finished",
			  G_CALLBACK (queue_order_test_message_finished),
			  &finished_count);

	soup_session_send_async (session, msg1, G_PRIORITY_DEFAULT, NULL, NULL, NULL);
	soup_session_send_async (session, msg2, G_PRIORITY_DEFAULT, NULL, NULL, NULL);
	soup_session_send_async (session, msg3, G_PRIORITY_DEFAULT, NULL, NULL, NULL);
	while (queue[2] == NULL)
		g_main_context_iteration (NULL, TRUE);

	g_assert_true (queue[0] == msg1);
	g_assert_true (queue[1] == msg2);
	g_assert_true (queue[2] == msg3);
	g_object_unref (msg1);
	g_object_unref (msg2);
	g_object_unref (msg3);

	while (finished_count != 3)
		g_main_context_iteration (NULL, TRUE);

	soup_test_session_abort_unref (session);
}

int
main (int argc, char **argv)
{
	SoupServer *server;
	int ret;

	test_init (argc, argv, NULL);

	server = soup_test_server_new (SOUP_TEST_SERVER_IN_THREAD);
	soup_server_add_handler (server, NULL, server_handler, NULL, NULL);
	base_uri = soup_test_server_get_uri (server, "http", NULL);
	index_bytes = soup_test_get_index ();
 	soup_test_register_resources ();

	g_test_add_func ("/session/SoupSession", do_plain_tests);
	g_test_add_func ("/session/priority", do_priority_tests);
        g_test_add_func ("/session/priority-change", do_priority_change_test);
	g_test_add_func ("/session/property", do_property_tests);
	g_test_add_func ("/session/features", do_features_test);
	g_test_add_func ("/session/queue-order", do_queue_order_test);

	ret = g_test_run ();

	g_uri_unref (base_uri);
	soup_test_server_quit_unref (server);

	test_cleanup ();
	return ret;
}
