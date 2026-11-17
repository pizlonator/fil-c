/*
 * Copyright © 2020, 2022 Christian Persch
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

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>

#include <glib.h>

#include "terminal-settings-bridge-impl.hh"

#include "terminal-app.hh"
#include "terminal-client-utils.hh"
#include "terminal-debug.hh"
#include "terminal-defines.hh"
#include "terminal-intl.hh"
#include "terminal-libgsystem.hh"
#include "terminal-prefs-process.hh"

#include "terminal-settings-bridge-generated.h"

struct _TerminalPrefsProcess {
  GObject parent_instance;

  GSubprocess* subprocess;
  GCancellable* cancellable;
  GDBusConnection *connection;
  TerminalSettingsBridgeImpl* bridge_impl;
};

struct _TerminalPrefsProcessClass {
  GObjectClass parent_class;

  // Signals
  void (*exited)(TerminalPrefsProcess* process,
                 int status);
};

enum {
  SIGNAL_EXITED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

// helper functions

template<typename T>
inline constexpr auto
IMPL(T* that) noexcept
{
  return reinterpret_cast<TerminalPrefsProcess*>(that);
}

// BEGIN copied from vte/src/libc-glue.hh

static inline int
fd_get_descriptor_flags(int fd) noexcept
{
        auto flags = int{};
        do {
                flags = fcntl(fd, F_GETFD);
        } while (flags == -1 && errno == EINTR);

        return flags;
}

static inline int
fd_set_descriptor_flags(int fd,
                        int flags) noexcept
{
        auto r = int{};
        do {
                r = fcntl(fd, F_SETFD, flags);
        } while (r == -1 && errno == EINTR);

        return r;
}

static inline int
fd_change_descriptor_flags(int fd,
                           int set_flags,
                           int unset_flags) noexcept
{
        auto const flags = fd_get_descriptor_flags(fd);
        if (flags == -1)
                return -1;

        auto const new_flags = (flags | set_flags) & ~unset_flags;
        if (new_flags == flags)
                return 0;

        return fd_set_descriptor_flags(fd, new_flags);
}

static inline int
fd_get_status_flags(int fd) noexcept
{
        auto flags = int{};
        do {
                flags = fcntl(fd, F_GETFL, 0);
        } while (flags == -1 && errno == EINTR);

        return flags;
}

static inline int
fd_set_status_flags(int fd,
                    int flags) noexcept
{
        auto r = int{};
        do {
                r = fcntl(fd, F_SETFL, flags);
        } while (r == -1 && errno == EINTR);

        return r;
}

static inline int
fd_change_status_flags(int fd,
                       int set_flags,
                       int unset_flags) noexcept
{
        auto const flags = fd_get_status_flags(fd);
        if (flags == -1)
                return -1;

        auto const new_flags = (flags | set_flags) & ~unset_flags;
        if (new_flags == flags)
                return 0;

        return fd_set_status_flags(fd, new_flags);
}

static inline int
fd_set_cloexec(int fd) noexcept
{
        return fd_change_descriptor_flags(fd, FD_CLOEXEC, 0);
}

static inline int
fd_set_nonblock(int fd) noexcept
{
        return fd_change_status_flags(fd, O_NONBLOCK, 0);
}

// END copied from vte

static int
socketpair_cloexec_nonblock(int domain,
                            int type,
                            int protocol,
                            int sv[2]) noexcept
{
  auto r = int{};
#if defined(SOCK_CLOEXEC) && defined(SOCK_NONBLOCK)
  r = socketpair(domain,
                 type | SOCK_CLOEXEC | SOCK_NONBLOCK,
                 protocol,
                 sv);
  if (r != -1)
    return r;

  // Maybe cloexec and/or nonblock aren't supported by the kernel
  if (errno != EINVAL && errno != EPROTOTYPE)
    return r;

  // If so, fall back to applying the flags after the socketpair() call
#endif /* SOCK_CLOEXEC && SOCK_NONBLOCK */

  r = socketpair(domain, type, protocol, sv);
  if (r == -1)
    return r;

  if (fd_set_cloexec(sv[0]) == -1 ||
      fd_set_nonblock(sv[0]) == -1 ||
      fd_set_cloexec(sv[1]) == -1 ||
      fd_set_nonblock(sv[1]) == -1) {
    close(sv[0]);
    close(sv[1]);
    return -1;
  }

  return r;
}

static void
subprocess_wait_cb(GObject* source,
                   GAsyncResult* result,
                   void* user_data)
{
  gs_unref_object auto impl = IMPL(user_data); // ref added on g_subprocess_wait_async

  gs_free_error GError* error = nullptr;
  if (g_subprocess_wait_finish(impl->subprocess, result, &error)) {
  } // else: @cancellable was cancelled

  auto const status = g_subprocess_get_status(impl->subprocess);

  g_signal_emit(impl, signals[SIGNAL_EXITED], 0, status);

  g_clear_object(&impl->subprocess);
}

// GInitable implementation

static gboolean
terminal_prefs_process_initable_init(GInitable* initable,
                                     GCancellable* cancellable,
                                     GError** error) noexcept
{
  auto const impl = IMPL(initable);

  // Create a private D-Bus connection between the server and the preferences
  // process, over which we proxy the settings (since otherwise there would
  // be no way to modify the server's settings on backends other than dconf).

  int socket_fds[2]{-1, -1};
  auto r = socketpair_cloexec_nonblock(AF_UNIX, SOCK_STREAM, 0, socket_fds);
  if (r != 0) {
    auto const errsv = errno;
    g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errsv),
                "Failed to create bridge socketpair: %s",
                g_strerror(errsv));
    return false;
  }

  // Launch process
  auto const launcher = g_subprocess_launcher_new(GSubprocessFlags(0));
  // or use G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE ?

  // Note that g_subprocess_launcher_set_cwd() is not necessary since
  // the server's cwd is already $HOME.
  g_subprocess_launcher_set_environ(launcher, nullptr); // inherit server's environment
  g_subprocess_launcher_unsetenv(launcher, "DBUS_SESSION_BUS_ADDRESS");
  g_subprocess_launcher_unsetenv(launcher, "DBUS_STARTER_BUS_TYPE");
  // ? g_subprocess_launcher_setenv(launcher, "GSETTINGS_BACKEND", "bridge", true);
  g_subprocess_launcher_take_fd(launcher, socket_fds[1], 3);
  socket_fds[1] = -1;

  gs_free auto exe = terminal_client_get_file_uninstalled(TERM_LIBEXECDIR,
                                                          TERM_LIBEXECDIR,
                                                          TERMINAL_PREFERENCES_BINARY_NAME,
                                                          G_FILE_TEST_IS_EXECUTABLE);

  char *argv[16];
  auto argc = 0;
  _TERMINAL_DEBUG_IF(TERMINAL_DEBUG_BRIDGE) {
    argv[argc++] = (char*)"vte-2.91";
    argv[argc++] = (char*)"--fd=3";
    argv[argc++] = (char*)"--";
    argv[argc++] = (char*)"gdb";
    argv[argc++] = (char*)"--args";
  }
  argv[argc++] = exe;
  argv[argc++] = (char*)"--bus-fd=3";
  argv[argc++] = nullptr;
  g_assert(argc <= int(G_N_ELEMENTS(argv)));

  impl->subprocess = g_subprocess_launcher_spawnv(launcher, // consumed
                                                  argv,
                                                  error);
  if (!impl->subprocess) {
    close(socket_fds[0]);
    return false;
  }

  g_subprocess_wait_async(impl->subprocess,
                          impl->cancellable,
                          GAsyncReadyCallback(subprocess_wait_cb),
                          g_object_ref(initable));

  // Create server end of the D-Bus connection
  gs_unref_object auto socket = g_socket_new_from_fd(socket_fds[0], error);
  socket_fds[0] = -1;

  if (!socket) {
    g_prefix_error(error, "Failed to create bridge GSocket: ");
    g_subprocess_force_exit(impl->subprocess);
    return false;
  }

  gs_unref_object auto sockconn =
    g_socket_connection_factory_create_connection(socket);
  if (!G_IS_IO_STREAM(sockconn)) {
    g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                "Bridge socket has incorrect type %s", G_OBJECT_TYPE_NAME(sockconn));
    g_subprocess_force_exit(impl->subprocess);
    return false;
  }

  gs_free auto guid = g_dbus_generate_guid();

  impl->connection =
    g_dbus_connection_new_sync(G_IO_STREAM(sockconn),
                               guid,
                               GDBusConnectionFlags(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_SERVER |
                                                    G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_ALLOW_ANONYMOUS),
                               nullptr, // auth observer,
                               cancellable,
                               error);
  if (!impl->connection) {
    g_prefix_error(error, "Failed to create bridge D-Bus connection: ");
    g_subprocess_force_exit(impl->subprocess);
    return false;
  }

  g_dbus_connection_set_exit_on_close(impl->connection, false);

  auto const app = terminal_app_get();
  impl->bridge_impl =
    terminal_settings_bridge_impl_new(terminal_app_get_settings_backend(app));
  if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(impl->bridge_impl),
                                        impl->connection,
                                        TERMINAL_SETTINGS_BRIDGE_OBJECT_PATH,
                                        error)) {
    g_prefix_error(error, "Failed to export D-Bus skeleton: ");
    g_subprocess_force_exit(impl->subprocess);
    return false;
  }

  return true;
}

static void
terminal_prefs_process_initable_iface_init(GInitableIface* iface) noexcept
{
  iface->init = terminal_prefs_process_initable_init;
}

static void
terminal_prefs_process_async_initable_iface_init(GAsyncInitableIface* iface) noexcept
{
  // Use the default implementation which runs the GInitiable in a thread.
}

// Class Implementation

G_DEFINE_TYPE_WITH_CODE(TerminalPrefsProcess,
                        terminal_prefs_process,
                        G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(G_TYPE_INITABLE,
                                              terminal_prefs_process_initable_iface_init)
                        G_IMPLEMENT_INTERFACE(G_TYPE_ASYNC_INITABLE,
                                              terminal_prefs_process_async_initable_iface_init))

static void
terminal_prefs_process_init(TerminalPrefsProcess* process) /* noexcept */
{
  g_application_hold(g_application_get_default());
}

static void
terminal_prefs_process_finalize(GObject* object) noexcept
{
  auto const impl = IMPL(object);
  g_clear_object(&impl->bridge_impl);
  g_clear_object(&impl->connection);
  g_clear_object(&impl->cancellable);
  g_clear_object(&impl->subprocess);

  G_OBJECT_CLASS(terminal_prefs_process_parent_class)->finalize(object);

  g_application_release(g_application_get_default());
}

static void
terminal_prefs_process_class_init(TerminalPrefsProcessClass* klass) /* noexcept */
{
  auto const gobject_class = G_OBJECT_CLASS(klass);
  gobject_class->finalize = terminal_prefs_process_finalize;

  signals[SIGNAL_EXITED] =
    g_signal_new(I_("exited"),
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET(TerminalPrefsProcessClass, exited),
                 nullptr, nullptr,
                 g_cclosure_marshal_VOID__INT,
                 G_TYPE_NONE,
                 1,
                 G_TYPE_INT);
  g_signal_set_va_marshaller(signals[SIGNAL_EXITED],
                             G_OBJECT_CLASS_TYPE(klass),
                             g_cclosure_marshal_VOID__INTv);
}

// public API

TerminalPrefsProcess*
terminal_prefs_process_new_finish(GAsyncResult* result,
                                  GError** error)
{
  gs_unref_object auto source = G_ASYNC_INITABLE(g_async_result_get_source_object(result));
  auto const o = g_async_initable_new_finish(source, result, error);
  return reinterpret_cast<TerminalPrefsProcess*>(o);
}

void
terminal_prefs_process_new_async(GCancellable* cancellable,
                                 GAsyncReadyCallback callback,
                                 void* user_data)
{
  g_async_initable_new_async(TERMINAL_TYPE_PREFS_PROCESS,
                             G_PRIORITY_DEFAULT,
                             cancellable,
                             callback,
                             user_data,
                             // properties,
                             nullptr);
}

TerminalPrefsProcess*
terminal_prefs_process_new_sync(GCancellable* cancellable,
                                 GError** error)
{
  return reinterpret_cast<TerminalPrefsProcess*>
    (g_initable_new(TERMINAL_TYPE_PREFS_PROCESS,
                    cancellable,
                    error,
                    // properties,
                    nullptr));
}

void
terminal_prefs_process_abort(TerminalPrefsProcess* process)
{
  g_return_if_fail(TERMINAL_IS_PREFS_PROCESS(process));

  auto const impl = IMPL(process);
  if (impl->subprocess)
    g_subprocess_force_exit(impl->subprocess);
}

#ifdef ENABLE_DEBUG

static void
show_cb(GObject* source,
        GAsyncResult* result,
        void* user_data)
{
  gs_unref_object auto process = IMPL(user_data); // added on g_dbus_connection_call

  gs_free_error GError *error = nullptr;
  gs_unref_variant auto rv = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source),
                                                           result,
                                                           &error);

  if (error)
    _terminal_debug_print(TERMINAL_DEBUG_BRIDGE, "terminal_prefs_process_show failed: %s\n", error->message);
}

#endif /* ENABLE_DEBUG */

void
terminal_prefs_process_show(TerminalPrefsProcess* process,
                            char const* profile_uuid,
                            char const* hint,
                            unsigned timestamp)
{
  auto const impl = IMPL(process);

  auto builder = GVariantBuilder{};
  g_variant_builder_init(&builder, G_VARIANT_TYPE("(sava{sv})"));
  g_variant_builder_add(&builder, "s", "preferences");
  g_variant_builder_open(&builder, G_VARIANT_TYPE("av")); // parameter
  g_variant_builder_open(&builder, G_VARIANT_TYPE("v"));
  g_variant_builder_open(&builder, G_VARIANT_TYPE("a{sv}"));
  if (profile_uuid)
    g_variant_builder_add(&builder, "{sv}", "profile", g_variant_new_string(profile_uuid));
  if (hint)
    g_variant_builder_add(&builder, "{sv}", "hint", g_variant_new_string(hint));
  g_variant_builder_add(&builder, "{sv}", "timestamp", g_variant_new_uint32(timestamp));
  g_variant_builder_close(&builder); // a{sv}
  g_variant_builder_close(&builder); // v
  g_variant_builder_close(&builder); // av
  g_variant_builder_open(&builder, G_VARIANT_TYPE("a{sv}")); // platform data
  g_variant_builder_close(&builder); // a{sv}

  g_dbus_connection_call(impl->connection,
                         nullptr, // since not on a message bus
                         TERMINAL_PREFERENCES_OBJECT_PATH,
                         "org.gtk.Actions",
                         "Activate",
                         g_variant_builder_end(&builder),
                         G_VARIANT_TYPE("()"),
                         G_DBUS_CALL_FLAGS_NO_AUTO_START,
                         30 * 1000, // ms timeout
                         impl->cancellable, // cancelleable
#ifdef ENABLE_DEBUG
                         show_cb, // callback
                         g_object_ref(process) // callback data
#else
                         nullptr, nullptr
#endif
                         );
}
