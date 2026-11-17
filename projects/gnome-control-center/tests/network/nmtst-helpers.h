/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright 2010 - 2014, 2018 Red Hat, Inc.
 *
 */

/* Static functions to help with testing */


/* nmtst_create_minimal_connection is copied from nm-test-utils.h. */

static inline NMConnection *
nmtst_create_minimal_connection (const char *id, const char *uuid, const char *type, NMSettingConnection **out_s_con)
{
	NMConnection *con;
	NMSetting *s_base = NULL;
	NMSettingConnection *s_con;
	gs_free char *uuid_free = NULL;

	g_assert (id);

	if (uuid)
		g_assert (nm_utils_is_uuid (uuid));
	else
		uuid = uuid_free = nm_utils_uuid_generate ();

	if (type) {
		GType type_g;

		type_g = nm_setting_lookup_type (type);

		g_assert (type_g != G_TYPE_INVALID);

		s_base = g_object_new (type_g, NULL);
		g_assert (NM_IS_SETTING (s_base));
	}

	con = nm_simple_connection_new ();

	s_con = NM_SETTING_CONNECTION (nm_setting_connection_new ());

	g_object_set (s_con,
	              NM_SETTING_CONNECTION_ID, id,
	              NM_SETTING_CONNECTION_UUID, uuid,
	              NM_SETTING_CONNECTION_TYPE, type,
	              NULL);
	nm_connection_add_setting (con, NM_SETTING (s_con));

	if (s_base)
		nm_connection_add_setting (con, s_base);

	if (out_s_con)
		*out_s_con = s_con;
	return con;
}


typedef struct {
	GMainLoop *loop;
	NMDevice *device;
	NMClient *client;
	NMConnection *connection;

	NMActiveConnection *ac;
	NMRemoteConnection *rc;

	const gchar * const *client_props;
	const gchar * const *device_props;

	int client_remaining;
	int device_remaining;
	int connection_remaining;
	int other_remaining;
} EventWaitInfo;


#define WAIT_CHECK_REMAINING() \
	if (info->client_remaining == 0 && info->device_remaining == 0 && info->connection_remaining == 0 && info->other_remaining == 0) { \
		g_debug ("Got expected events, quitting mainloop"); \
		g_main_loop_quit (info->loop); \
	} \
	if (info->client_remaining < 0 || info->device_remaining < 0 || info->connection_remaining < 0 || info->other_remaining < 0) { \
		g_error ("Pending events are negative: client: %d, device: %d, connection: %d, other: %d", info->client_remaining, info->device_remaining, info->connection_remaining, info->other_remaining); \
		g_assert_not_reached (); \
	}

#define WAIT_DECL() \
	EventWaitInfo info = {0}; \
	gint _timeout_id;
#define WAIT_DEVICE(_device, count, ...) \
	info.device = (_device); \
	info.device_remaining = (count); \
	info.device_props = (const char *[]) {__VA_ARGS__, NULL}; \
	g_signal_connect ((_device), "notify", G_CALLBACK (device_notify_cb), &info); \
	connect_signals (&info, G_OBJECT (_device), info.device_props, G_CALLBACK (device_signal_cb));
#define WAIT_CLIENT(_client, count, ...) \
	info.client = (_client); \
	info.client_remaining = (count); \
	info.client_props = (const char *[]) {__VA_ARGS__, NULL}; \
	g_signal_connect ((_client), "notify", G_CALLBACK (client_notify_cb), &info); \
	connect_signals (&info, G_OBJECT (_client), info.client_props, G_CALLBACK (client_signal_cb));
#define WAIT_CONNECTION(_connection, count, ...) \
	info.connection = NM_CONNECTION (_connection); \
	info.connection_remaining = (count); \
	{ const gchar* const *_signals = (const char *[]) {__VA_ARGS__, NULL}; \
	connect_signals (&info, G_OBJECT (_connection), _signals, G_CALLBACK (connection_signal_cb)); }

#define WAIT_DESTROY() \
	g_source_remove (_timeout_id); \
	if (info.device) { \
		g_signal_handlers_disconnect_by_func (info.device, G_CALLBACK (device_notify_cb), &info); \
		g_signal_handlers_disconnect_by_func (info.device, G_CALLBACK (device_signal_cb), &info); \
	} \
	if (info.client) { \
		g_signal_handlers_disconnect_by_func (info.client, G_CALLBACK (client_notify_cb), &info); \
		g_signal_handlers_disconnect_by_func (info.client, G_CALLBACK (client_signal_cb), &info); \
	} \
	if (info.connection) { \
		g_signal_handlers_disconnect_by_func (info.connection, G_CALLBACK (connection_signal_cb), &info); \
	} \
	g_main_loop_unref (info.loop);

#define WAIT_FINISHED(timeout) \
	info.loop = g_main_loop_new (NULL, FALSE); \
	_timeout_id = g_timeout_add_seconds ((timeout), timeout_cb, &info); \
	g_main_loop_run (info.loop); \
	WAIT_DESTROY()

static gboolean
timeout_cb (gpointer user_data)
{
	EventWaitInfo *info = user_data;

	if (info)
		g_error ("Pending events are: client: %d, device: %d, connection: %d, other: %d", info->client_remaining, info->device_remaining, info->connection_remaining, info->other_remaining); \
	g_assert_not_reached ();
	return G_SOURCE_REMOVE;
}

static void
connect_signals (EventWaitInfo *info, GObject *object, const gchar* const* signals, GCallback handler)
{
	const gchar * const* signal;
	GObjectClass *class = G_OBJECT_GET_CLASS(object);

	for (signal = signals; *signal != NULL; signal++) {
		if (g_object_class_find_property (class, *signal))
			continue;

		g_debug ("Connecting signal handler for %s", *signal);
		g_signal_connect_data (object, *signal, handler, info, NULL, G_CONNECT_AFTER | G_CONNECT_SWAPPED);
	}
}

static void
device_signal_cb (gpointer user_data)
{
	EventWaitInfo *info = user_data;

	g_debug ("Counting signal for device");

	info->device_remaining--;
	WAIT_CHECK_REMAINING()
}

static void
client_signal_cb (gpointer user_data)
{
	EventWaitInfo *info = user_data;

	g_debug ("Counting signal for client");

	info->client_remaining--;
	WAIT_CHECK_REMAINING()
}

static void
connection_signal_cb (gpointer user_data)
{
	EventWaitInfo *info = user_data;

	g_debug ("Counting signal for connection");

	info->connection_remaining--;
	WAIT_CHECK_REMAINING()
}

static void
device_notify_cb (NMDevice *device, GParamSpec *pspec, gpointer user_data)
{
	EventWaitInfo *info = user_data;

	g_assert (device == info->device);

	if (!g_strv_contains (info->device_props, g_param_spec_get_name (pspec))) {
		g_debug ("Ignoring notification for device property %s", g_param_spec_get_name (pspec));
		return;
	}

	g_debug ("Counting notification for device property %s", g_param_spec_get_name (pspec));

	info->device_remaining--;
	WAIT_CHECK_REMAINING()
}

static void
client_notify_cb (NMClient *client, GParamSpec *pspec, gpointer user_data)
{
	EventWaitInfo *info = user_data;

	g_assert (client == info->client);

	if (!g_strv_contains (info->client_props, g_param_spec_get_name (pspec))) {
		g_debug ("Ignoring notification for client property %s", g_param_spec_get_name (pspec));
		return;
	}

	g_debug ("Counting notification for client property %s", g_param_spec_get_name (pspec));

	info->client_remaining--;
	WAIT_CHECK_REMAINING()
}

static void
nmtst_set_device_state (NMTstcServiceInfo *sinfo, NMDevice *device, NMDeviceState state, NMDeviceStateReason reason)
{
	GError *error = NULL;
	WAIT_DECL()

	g_debug ("Setting device %s state to %d with reason %d", nm_device_get_iface (device), state, reason);

	g_dbus_proxy_call_sync (sinfo->proxy,
	                        "SetDeviceState",
	                        g_variant_new ("(suu)", nm_object_get_path (NM_OBJECT (device)), state, reason),
	                        G_DBUS_CALL_FLAGS_NO_AUTO_START,
	                        3000,
	                        NULL,
	                        &error);
	g_assert_no_error (error);

	WAIT_DEVICE(device, 3, "state-reason", "state", "state-changed")
	WAIT_FINISHED(5)
}

static void
nmtst_set_wired_speed (NMTstcServiceInfo *sinfo, NMDevice *device, guint32 speed)
{
	GError *error = NULL;
	WAIT_DECL()

	g_debug ("Setting device %s speed to %d", nm_device_get_iface (device), speed);

	g_dbus_proxy_call_sync (sinfo->proxy,
	                        "SetWiredSpeed",
	                        g_variant_new ("(su)", nm_object_get_path (NM_OBJECT (device)), speed),
	                        G_DBUS_CALL_FLAGS_NO_AUTO_START,
	                        3000,
	                        NULL,
	                        &error);

	g_assert_no_error (error);

	WAIT_DEVICE(device, 2, "speed", "carrier")
	WAIT_FINISHED(5)
}


static void
device_removed_cb (NMClient *client,
                   NMDevice *device,
                   gpointer user_data)
{
	EventWaitInfo *info = user_data;

	g_assert (device);
	g_assert (device == info->device);

	info->other_remaining--;
	WAIT_CHECK_REMAINING()
}

static void
nmtst_remove_device (NMTstcServiceInfo *sinfo, NMClient *client, NMDevice *device)
{
	GError *error = NULL;
	WAIT_DECL()

	g_object_ref (device);

	g_dbus_proxy_call_sync (sinfo->proxy,
	                        "RemoveDevice",
	                        g_variant_new ("(s)", nm_object_get_path (NM_OBJECT (device))),
	                        G_DBUS_CALL_FLAGS_NO_AUTO_START,
	                        3000,
	                        NULL,
	                        &error);
	g_assert_no_error (error);

	info.device = device;
	info.client = client;
	info.other_remaining = 1;
	g_signal_connect (client, "device-removed",
	                  G_CALLBACK (device_removed_cb), &info);

	WAIT_FINISHED(5)

	g_object_unref(device);
	g_signal_handlers_disconnect_by_func (client, device_removed_cb, &info);
}

static void
add_and_activate_cb (GObject *object,
                     GAsyncResult *result,
                     gpointer user_data)
{
	NMClient *client = NM_CLIENT (object);
	EventWaitInfo *info = user_data;
	GError *error = NULL;

	info->ac = nm_client_add_and_activate_connection_finish (client, result, &error);
	g_assert_no_error (error);
	g_assert (info->ac != NULL);

	info->other_remaining--;
	WAIT_CHECK_REMAINING()
}

static NMActiveConnection*
nmtst_add_and_activate_connection (NMTstcServiceInfo *sinfo, NMClient *client, NMDevice *device, NMConnection *conn)
{
	WAIT_DECL()

	nm_client_add_and_activate_connection_async (client, conn, device, NULL,
	                                             NULL, add_and_activate_cb, &info);

	info.other_remaining = 1;
	WAIT_CLIENT(client, 3, NM_CLIENT_CONNECTIONS, NM_CLIENT_ACTIVE_CONNECTIONS, NM_CLIENT_CONNECTION_ADDED);
	WAIT_DEVICE(device, 1, NM_DEVICE_ACTIVE_CONNECTION);

	g_object_unref (conn);

	WAIT_FINISHED(5)

	g_assert (info.ac != NULL);

	return info.ac;
}
