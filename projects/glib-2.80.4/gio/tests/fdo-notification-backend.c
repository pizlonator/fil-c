/*
 * Copyright © 2022 Endless OS Foundation, LLC
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Philip Withnall <pwithnall@endlessos.org>
 */

#include <gio/gio.h>
#include <locale.h>

#include <gio/giomodule-priv.h>
#include "gio/gnotificationbackend.h"


static GNotificationBackend *
construct_backend (GApplication **app_out)
{
  GApplication *app = NULL;
  GType fdo_type = G_TYPE_INVALID;
  GNotificationBackend *backend = NULL;
  GError *local_error = NULL;

  /* Construct the app first and withdraw a notification, to ensure that IO modules are loaded. */
  app = g_application_new ("org.gtk.TestApplication", G_APPLICATION_DEFAULT_FLAGS);
  g_application_register (app, NULL, &local_error);
  g_assert_no_error (local_error);
  g_application_withdraw_notification (app, "org.gtk.TestApplication.NonexistentNotification");

  fdo_type = g_type_from_name ("GFdoNotificationBackend");
  g_assert_cmpuint (fdo_type, !=, G_TYPE_INVALID);

  /* Replicate the behaviour from g_notification_backend_new_default(), which is
   * not exported publicly so can‘t be easily used in the test. */
  backend = g_object_new (fdo_type, NULL);
  backend->application = app;
  backend->dbus_connection = g_application_get_dbus_connection (app);
  if (backend->dbus_connection)
    g_object_ref (backend->dbus_connection);

  if (app_out != NULL)
    *app_out = g_object_ref (app);

  g_clear_object (&app);

  return g_steal_pointer (&backend);
}

static void
test_construction (void)
{
  GNotificationBackend *backend = NULL;
  GApplication *app = NULL;
  GTestDBus *bus = NULL;

  g_test_message ("Test constructing a GFdoNotificationBackend");

  /* Set up a test session bus and connection. */
  bus = g_test_dbus_new (G_TEST_DBUS_NONE);
  g_test_dbus_up (bus);

  backend = construct_backend (&app);
  g_assert_nonnull (backend);

  g_application_quit (app);
  g_clear_object (&app);
  g_clear_object (&backend);

  g_test_dbus_down (bus);
  g_clear_object (&bus);
}

static void
daemon_method_call_cb (GDBusConnection       *connection,
                       const gchar           *sender,
                       const gchar           *object_path,
                       const gchar           *interface_name,
                       const gchar           *method_name,
                       GVariant              *parameters,
                       GDBusMethodInvocation *invocation,
                       gpointer               user_data)
{
  GDBusMethodInvocation **current_method_invocation_out = user_data;

  g_assert_null (*current_method_invocation_out);
  *current_method_invocation_out = g_steal_pointer (&invocation);

  g_main_context_wakeup (NULL);
}

static void
name_acquired_or_lost_cb (GDBusConnection *connection,
                          const gchar     *name,
                          gpointer         user_data)
{
  gboolean *name_acquired = user_data;

  *name_acquired = !*name_acquired;

  g_main_context_wakeup (NULL);
}

static void
dbus_activate_action_cb (GSimpleAction *action,
                         GVariant      *parameter,
                         gpointer       user_data)
{
  guint *n_activations = user_data;

  *n_activations = *n_activations + 1;
  g_main_context_wakeup (NULL);
}

static void
assert_send_notification (GNotificationBackend   *backend,
                          GDBusMethodInvocation **current_method_invocation,
                          guint32                 notify_id)
{
  GNotification *notification = NULL;

  notification = g_notification_new ("Some Notification");
  G_NOTIFICATION_BACKEND_GET_CLASS (backend)->send_notification (backend, "notification1", notification);
  g_clear_object (&notification);

  while (*current_method_invocation == NULL)
    g_main_context_iteration (NULL, TRUE);

  g_assert_cmpstr (g_dbus_method_invocation_get_interface_name (*current_method_invocation), ==, "org.freedesktop.Notifications");
  g_assert_cmpstr (g_dbus_method_invocation_get_method_name (*current_method_invocation), ==, "Notify");
  g_dbus_method_invocation_return_value (g_steal_pointer (current_method_invocation), g_variant_new ("(u)", notify_id));
}

static void
assert_emit_action_invoked (GDBusConnection *daemon_connection,
                            GVariant        *parameters)
{
  GError *local_error = NULL;

  g_dbus_connection_emit_signal (daemon_connection,
                                 NULL,
                                 "/org/freedesktop/Notifications",
                                 "org.freedesktop.Notifications",
                                 "ActionInvoked",
                                 parameters,
                                 &local_error);
  g_assert_no_error (local_error);
}

static void
test_dbus_activate_action (void)
{
  /* Very trimmed down version of
   * https://specifications.freedesktop.org/notification-spec/notification-spec-latest.html */
  const GDBusArgInfo daemon_notify_in_app_name = { -1, "AppName", "s", NULL };
  const GDBusArgInfo daemon_notify_in_replaces_id = { -1, "ReplacesId", "u", NULL };
  const GDBusArgInfo daemon_notify_in_app_icon = { -1, "AppIcon", "s", NULL };
  const GDBusArgInfo daemon_notify_in_summary = { -1, "Summary", "s", NULL };
  const GDBusArgInfo daemon_notify_in_body = { -1, "Body", "s", NULL };
  const GDBusArgInfo daemon_notify_in_actions = { -1, "Actions", "as", NULL };
  const GDBusArgInfo daemon_notify_in_hints = { -1, "Hints", "a{sv}", NULL };
  const GDBusArgInfo daemon_notify_in_expire_timeout = { -1, "ExpireTimeout", "i", NULL };
  const GDBusArgInfo *daemon_notify_in_args[] =
    {
      &daemon_notify_in_app_name,
      &daemon_notify_in_replaces_id,
      &daemon_notify_in_app_icon,
      &daemon_notify_in_summary,
      &daemon_notify_in_body,
      &daemon_notify_in_actions,
      &daemon_notify_in_hints,
      &daemon_notify_in_expire_timeout,
      NULL
    };
  const GDBusArgInfo daemon_notify_out_id = { -1, "Id", "u", NULL };
  const GDBusArgInfo *daemon_notify_out_args[] = { &daemon_notify_out_id, NULL };
  const GDBusMethodInfo daemon_notify_info = { -1, "Notify", (GDBusArgInfo **) daemon_notify_in_args, (GDBusArgInfo **) daemon_notify_out_args, NULL };
  const GDBusMethodInfo *daemon_methods[] = { &daemon_notify_info, NULL };
  const GDBusInterfaceInfo daemon_interface_info = { -1, "org.freedesktop.Notifications", (GDBusMethodInfo **) daemon_methods, NULL, NULL, NULL };

  GTestDBus *bus = NULL;
  GDBusConnection *daemon_connection = NULL;
  guint daemon_object_id = 0, daemon_name_id = 0;
  const GDBusInterfaceVTable vtable = { daemon_method_call_cb, NULL, NULL, { NULL, } };
  GDBusMethodInvocation *current_method_invocation = NULL;
  GApplication *app = NULL;
  GNotificationBackend *backend = NULL;
  guint32 notify_id;
  GError *local_error = NULL;
  const GActionEntry entries[] =
    {
      { "undo", dbus_activate_action_cb, NULL, NULL,      NULL, { 0 } },
      { "lang", dbus_activate_action_cb, "s",  "'latin'", NULL, { 0 } },
    };
  guint n_activations = 0;
  gboolean name_acquired = FALSE;

  g_test_summary ("Test how the backend handles valid and invalid ActionInvoked signals from the daemon");

  /* Set up a test session bus and connection. */
  bus = g_test_dbus_new (G_TEST_DBUS_NONE);
  g_test_dbus_up (bus);

  /* Create a mock org.freedesktop.Notifications daemon */
  daemon_connection = g_dbus_connection_new_for_address_sync (g_test_dbus_get_bus_address (bus),
                                                              G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                                              G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                                              NULL, NULL, &local_error);
  g_assert_no_error (local_error);

  daemon_object_id = g_dbus_connection_register_object (daemon_connection,
                                                        "/org/freedesktop/Notifications",
                                                        (GDBusInterfaceInfo *) &daemon_interface_info,
                                                        &vtable,
                                                        &current_method_invocation, NULL, &local_error);
  g_assert_no_error (local_error);

  daemon_name_id = g_bus_own_name_on_connection (daemon_connection,
                                                 "org.freedesktop.Notifications",
                                                 G_BUS_NAME_OWNER_FLAGS_DO_NOT_QUEUE,
                                                 name_acquired_or_lost_cb,
                                                 name_acquired_or_lost_cb,
                                                 &name_acquired, NULL);

  while (!name_acquired)
    g_main_context_iteration (NULL, TRUE);

  /* Construct our FDO backend under test */
  backend = construct_backend (&app);
  g_action_map_add_action_entries (G_ACTION_MAP (app), entries, G_N_ELEMENTS (entries), &n_activations);

  /* Send a notification to ensure that the backend is listening for D-Bus action signals. */
  notify_id = 1233;
  assert_send_notification (backend, &current_method_invocation, ++notify_id);

  /* Send a valid fake action signal. */
  n_activations = 0;
  assert_emit_action_invoked (daemon_connection, g_variant_new ("(us)", notify_id, "app.undo"));

  while (n_activations == 0)
    g_main_context_iteration (NULL, TRUE);

  g_assert_cmpuint (n_activations, ==, 1);

  /* Send a valid fake action signal with a target. We have to create a new
   * notification first, as invoking an action on a notification removes it, and
   * the GLib implementation of org.freedesktop.Notifications doesn’t currently
   * support the `resident` hint to avoid that. */
  assert_send_notification (backend, &current_method_invocation, ++notify_id);
  n_activations = 0;
  assert_emit_action_invoked (daemon_connection, g_variant_new ("(us)", notify_id, "app.lang::spanish"));

  while (n_activations == 0)
    g_main_context_iteration (NULL, TRUE);

  g_assert_cmpuint (n_activations, ==, 1);

  /* Send a series of invalid action signals, followed by one valid one which
   * we should be able to detect. */
  assert_send_notification (backend, &current_method_invocation, ++notify_id);
  n_activations = 0;
  assert_emit_action_invoked (daemon_connection, g_variant_new ("(us)", notify_id, "app.nonexistent"));
  assert_emit_action_invoked (daemon_connection, g_variant_new ("(us)", notify_id, "app.lang(13)"));
  assert_emit_action_invoked (daemon_connection, g_variant_new ("(us)", notify_id, "app.undo::should-have-no-parameter"));
  assert_emit_action_invoked (daemon_connection, g_variant_new ("(us)", notify_id, "app.lang"));
  assert_emit_action_invoked (daemon_connection, g_variant_new ("(us)", notify_id, "undo"));  /* no `app.` prefix */
  assert_emit_action_invoked (daemon_connection, g_variant_new ("(us)", notify_id, "app.lang("));  /* invalid parse format */
  assert_emit_action_invoked (daemon_connection, g_variant_new ("(us)", notify_id, "app.undo"));

  while (n_activations == 0)
    g_main_context_iteration (NULL, TRUE);

  g_assert_cmpuint (n_activations, ==, 1);

  /* Shut down. */
  g_assert_null (current_method_invocation);

  g_application_quit (app);
  g_clear_object (&app);
  g_clear_object (&backend);

  g_dbus_connection_unregister_object (daemon_connection, daemon_object_id);
  g_bus_unown_name (daemon_name_id);

  g_dbus_connection_flush_sync (daemon_connection, NULL, &local_error);
  g_assert_no_error (local_error);
  g_dbus_connection_close_sync (daemon_connection, NULL, &local_error);
  g_assert_no_error (local_error);

  g_clear_object (&daemon_connection);

  g_test_dbus_down (bus);
  g_clear_object (&bus);
}

int
main (int   argc,
      char *argv[])
{
  setlocale (LC_ALL, "");

  /* Force use of the FDO backend */
  g_setenv ("GNOTIFICATION_BACKEND", "freedesktop", TRUE);

  g_test_init (&argc, &argv, NULL);

  /* Make sure we don’t send notifications to the actual D-Bus session. */
  g_test_dbus_unset ();

  g_test_add_func ("/fdo-notification-backend/construction", test_construction);
  g_test_add_func ("/fdo-notification-backend/dbus/activate-action", test_dbus_activate_action);

  return g_test_run ();
}
