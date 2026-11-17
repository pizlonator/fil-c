/*
 * Copyright © 2008, 2010, 2011, 2021, 2022 Christian Persch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 *(at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "terminal-app.hh"
#include "terminal-debug.hh"
#include "terminal-i18n.hh"
#include "terminal-defines.hh"
#include "terminal-libgsystem.hh"
#include "terminal-settings-bridge-backend.hh"
#include "terminal-settings-bridge-generated.h"

// Reduce the default timeout to something that should still always work,
// but not hang the process for long periods of time if something does
// go wrong. See issue #7935.
#define BRIDGE_TIMEOUT 5000 /* ms */

static char* arg_profile_uuid = nullptr;
static char* arg_hint = nullptr;
static int arg_bus_fd = -1;
static int arg_timestamp = -1;

static const GOptionEntry options[] = {
  {"profile", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &arg_profile_uuid, "Profile", "UUID"},
  {"hint", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &arg_hint, "Hint", "HINT"},
  {"bus-fd", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_INT, &arg_bus_fd, "Bus FD", "FD"},
  {"timestamp", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_INT, &arg_timestamp, "Timestamp", "VALUE"},
  {nullptr}
};

static GSettings*
profile_from_uuid(TerminalApp* app,
                  char const* uuid_str) noexcept
{
  if (!uuid_str)
    return nullptr;

  auto const profiles_list = terminal_app_get_profiles_list(app);

  GSettings* profile = nullptr;
  if (g_str_equal(uuid_str, "default"))
    profile = terminal_settings_list_ref_default_child(profiles_list);
  else if (terminal_settings_list_valid_uuid(uuid_str))
    profile = terminal_settings_list_ref_child(profiles_list, uuid_str);

  return profile;
}

static void
preferences_cb(GSimpleAction* action,
               GVariant* parameter,
               void* user_data)
{
  auto const app = terminal_app_get();

  gs_free char* uuid_str = nullptr;
  g_variant_lookup(parameter, "profile", "s", &uuid_str);

  gs_unref_object auto profile = profile_from_uuid(app, uuid_str);

  gs_free char* hint_str = nullptr;
  g_variant_lookup(parameter, "hint", "s", &hint_str);

  guint32 ts = 0;
  g_variant_lookup(parameter, "timestamp", "u", &ts);

  terminal_app_edit_preferences(app, profile, hint_str, ts);
}

static void
connection_closed_cb(GDBusConnection* connection,
                     gboolean peer_vanished,
                     GError* error,
                     void* user_data)
{
  auto ptr = reinterpret_cast<void**>(user_data);

  if (error)
    g_printerr("D-Bus connection closed: %s\n", error->message);

  // As per glib docs, unref the connection at this point
  g_object_unref(g_steal_pointer(ptr));

  // Exit cleanly
  auto const app = g_application_get_default();
  if (app)
    g_application_quit(app);
}

int
main(int argc,
     char* argv[])
{
  // Sanitise environment
  g_unsetenv("CHARSET");
  g_unsetenv("DBUS_STARTER_BUS_TYPE");
  // Not interested in silly debug spew polluting the journal, bug #749195
  if (g_getenv("G_ENABLE_DIAGNOSTIC") == nullptr)
    g_setenv("G_ENABLE_DIAGNOSTIC", "0", true);
  // Maybe: g_setenv("GSETTINGS_BACKEND", "bridge", true);

  if (setlocale(LC_ALL, "") == nullptr) {
    g_printerr("Locale not supported.\n");
    return _EXIT_FAILURE_UNSUPPORTED_LOCALE;
  }

  terminal_i18n_init(true);

  char const* charset = nullptr;
  if (!g_get_charset(&charset)) {
    g_printerr("Non UTF-8 locale (%s) is not supported!\n", charset);
    return _EXIT_FAILURE_NO_UTF8;
  }

  _terminal_debug_init();

  auto const home_dir = g_get_home_dir();
  if (home_dir == nullptr || chdir(home_dir) < 0)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
    chdir("/");
#pragma GCC diagnostic pop

  g_set_prgname(TERMINAL_PREFERENCES_APPLICATION_ID);
  g_set_application_name(_("Terminal Preferences"));

  gs_free_error GError *error = nullptr;
  if (!gtk_init_with_args(&argc, &argv, nullptr, options, nullptr, &error)) {
    g_printerr("Failed to parse arguments: %s\n", error->message);
    return _EXIT_FAILURE_GTK_INIT;
  }

  gs_unref_object GDBusConnection* connection = nullptr;
  gs_unref_object GSettingsBackend* backend = nullptr;
  gs_unref_object GSimpleActionGroup* action_group = nullptr;
  auto export_id = 0u;
  if (arg_bus_fd != -1) {
    gs_unref_object auto socket = g_socket_new_from_fd(arg_bus_fd, &error);
    if (!socket) {
      g_printerr("Failed to create bridge socket: %s\n", error->message);
      close(arg_bus_fd);
      return EXIT_FAILURE;
    }

    gs_unref_object auto sockconn =
      g_socket_connection_factory_create_connection(socket);
    if (!G_IS_IO_STREAM(sockconn)) {
      g_printerr("Bridge socket has incorrect type %s\n", G_OBJECT_TYPE_NAME(sockconn));
      return EXIT_FAILURE;
    }

    connection =
      g_dbus_connection_new_sync(G_IO_STREAM(sockconn),
                                 nullptr, // guid=nullptr for the client
                                 GDBusConnectionFlags(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                                      G_DBUS_CONNECTION_FLAGS_DELAY_MESSAGE_PROCESSING),
                                 nullptr, // auth observer,
                                 nullptr, // cancellable,
                                 &error);
    if (!connection) {
      g_printerr("Failed to create bus: %s\n", error->message);
      return EXIT_FAILURE;
    }

    GActionEntry const action_entries[] = {
      { "preferences", preferences_cb, "a{sv}", nullptr, nullptr },
    };

    action_group = g_simple_action_group_new();
    g_action_map_add_action_entries(G_ACTION_MAP(action_group),
                                    action_entries, G_N_ELEMENTS(action_entries),
                                    nullptr);

    export_id = g_dbus_connection_export_action_group(connection,
                                                      TERMINAL_PREFERENCES_OBJECT_PATH,
                                                      G_ACTION_GROUP(action_group),
                                                      &error);
    if (export_id == 0) {
      g_printerr("Failed to export actions: %s\n", error->message);
      return EXIT_FAILURE;
    }

    g_dbus_connection_start_message_processing(connection);

    gs_unref_object auto bridge =
      terminal_settings_bridge_proxy_new_sync
      (connection,
       GDBusProxyFlags(
                       G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START |
                       G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START_AT_CONSTRUCTION |
                       G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS |
                       G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES),
       nullptr, // no name
       TERMINAL_SETTINGS_BRIDGE_OBJECT_PATH,
       nullptr, // cancellable
       &error);
    if (!bridge) {
      g_printerr("Failed to create settings bridge proxy: %s\n", error->message);
      return EXIT_FAILURE;
    }

    if (!_terminal_debug_on(TERMINAL_DEBUG_BRIDGE)) {
      g_dbus_proxy_set_default_timeout(G_DBUS_PROXY(bridge), BRIDGE_TIMEOUT);
    }

    backend = terminal_settings_bridge_backend_new(bridge);

    g_dbus_connection_set_exit_on_close(connection, false);
    g_signal_connect(connection, "closed", G_CALLBACK(connection_closed_cb), &connection);
  }

  gs_unref_object auto app =
    terminal_app_new(TERMINAL_PREFERENCES_APPLICATION_ID,
                     GApplicationFlags(G_APPLICATION_NON_UNIQUE |
                                       G_APPLICATION_IS_SERVICE),
                     backend);

  // Need to startup the application now, otherwise we can't use
  // gtk_application_add_window() before g_application_run() below.
  // This should always succeed.
  if (!g_application_register(G_APPLICATION(app), nullptr, &error)) {
    g_printerr("Failed to register application: %s\n", error->message);
    return EXIT_FAILURE;
  }

  // If started from gnome-terminal-server, the "preferences" action
  // will be activated to actually show the preferences dialogue. However
  // if started directly, need to show the dialogue right now.
  if (!connection) {
    gs_unref_object GSettings* profile = profile_from_uuid(TERMINAL_APP(app),
                                                           arg_profile_uuid);
    if (arg_profile_uuid && !profile) {
      g_printerr("No profile with UUID \"%s\": %s\n", arg_profile_uuid, error->message);
      return EXIT_FAILURE;
    }

    terminal_app_edit_preferences(TERMINAL_APP(app),
                                  profile,
                                  arg_hint,
                                  unsigned(arg_timestamp));
  }

  auto const r = g_application_run(app, 0, nullptr);

  if (connection && export_id != 0) {
    g_dbus_connection_unexport_action_group(connection, export_id);
    export_id = 0;
  }

  if (connection &&
      !g_dbus_connection_flush_sync(connection, nullptr, &error)) {
      g_printerr("Failed to flush D-Bus connection: %s\n", error->message);
  }

  return r;
}
