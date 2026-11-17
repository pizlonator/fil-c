/*
 * Copyright © 2001, 2002 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2002 Sun Microsystems
 * Copyright © 2003 Mariano Suarez-Alvarez
 * Copyright © 2011, 2013 Christian Persch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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
#include "terminal-client-utils.hh"
#include "terminal-debug.hh"
#include "terminal-defines.hh"
#include "terminal-libgsystem.hh"

#ifdef TERMINAL_PREFERENCES
#include "terminal-debug.hh"
#endif

#include <string.h>

#include <gio/gio.h>

#ifndef TERMINAL_NAUTILUS
#include <gdk/gdk.h>
#if defined(TERMINAL_COMPILATION) && defined(GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#endif
#endif

#ifdef ENABLE_DEBUG

static char*
get_binary_path_if_uninstalled(char const* install_dir) noexcept
{
#ifdef __linux__
  char buf[1024];
  auto const r = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (r < 0 || r >= ssize_t(sizeof(buf)))
    return nullptr;

  buf[r] = '\0'; // nul terminate

  gs_free auto path = g_path_get_dirname(buf);
  if (!path)
    return nullptr;

  if (g_str_equal(path, install_dir))
    return nullptr;

  return reinterpret_cast<char*>(g_steal_pointer(&path));
#else
  return nullptr;
#endif /* __linux__ */
}

static char*
get_path_if_uninstalled(char const* exe_install_dir,
                        char const* file_name,
                        GFileTest tests)
{
  gs_free auto path = get_binary_path_if_uninstalled(exe_install_dir);
  if (!path)
    return nullptr;

  gs_free auto file = g_build_filename(path, file_name, nullptr);
  if (!g_file_test(file, GFileTest(tests | G_FILE_TEST_EXISTS)))
    return nullptr;

  return reinterpret_cast<char*>(g_steal_pointer(&path));
}

#endif /* ENABLE_DEBUG */

/**
 * terminal_client_find_file_uninstalled:
 * @exe_install_dir: the directory where the current exe is installed
 * @file_install_dir: the directory where the file to locate is installed
 * @file_name: the name of the file to locate
 * @tests: extra tests from #GFileTest
 *
 * Tries to locate the directory that contains @file_name in a build directory,
 * and returns the directory.  If @file_name is not found, returns the
 * installed location for it.
 */
char*
terminal_client_get_directory_uninstalled(char const* exe_install_dir,
                                          char const *file_install_dir,
                                          char const* file_name,
                                          GFileTest tests)
{
#ifdef ENABLE_DEBUG
  auto path = get_path_if_uninstalled(exe_install_dir, file_name, tests);
  if (path)
    return path;
#endif /* ENABLE_DEBUG */

  return g_strdup(file_install_dir);
}

/**
 * terminal_client_find_file_uninstalled:
 * @exe_install_dir: the directory where the current exe is installed
 * @file_install_dir: the directory where the file to locate is installed
 * @file_name: the name of the file to locate
 * @tests: extra tests from #GFileTest
 *
 * Tries to locate the file @file_name in a build directory, and
 * returns a full path to it.  If @file_name is not found, returns the
 * installed location for it.
 */
char*
terminal_client_get_file_uninstalled(char const* exe_install_dir,
                                     char const *file_install_dir,
                                     char const* file_name,
                                     GFileTest tests)
{
#ifdef ENABLE_DEBUG
  gs_free auto path = get_path_if_uninstalled(exe_install_dir, file_name, tests);
  if (path)
    return g_build_filename(path, file_name, nullptr);
#endif /* ENABLE_DEBUG */

  return g_build_filename(file_install_dir, file_name, nullptr);
}

/**
 * terminal_client_append_create_instance_options:
 * @builder: a #GVariantBuilder of #GVariantType "a{sv}"
 * @display: (array element-type=guint8):
 * @startup_id:
 * @activation_token:
 * @geometry:
 * @role:
 * @profile:
 * @title:
 * @maximise_window:
 * @fullscreen_window:
 *
 * Appends common options to @builder.
 */
void
terminal_client_append_create_instance_options (GVariantBuilder *builder,
                                                const char      *display_name,
                                                const char      *startup_id,
                                                const char      *activation_token,
                                                const char      *geometry,
                                                const char      *role,
                                                const char      *profile,
                                                const char      *encoding,
                                                const char      *title,
                                                gboolean         active,
                                                gboolean         maximise_window,
                                                gboolean         fullscreen_window)
{
  /* Bytestring options */
  if (display_name != nullptr)
    g_variant_builder_add (builder, "{sv}",
                           "display", g_variant_new_bytestring (display_name));
  if (startup_id)
    g_variant_builder_add (builder, "{sv}",
                           "desktop-startup-id", g_variant_new_bytestring (startup_id));
  if (activation_token)
    g_variant_builder_add (builder, "{sv}",
                           "activation-token", g_variant_new_string (activation_token));

  /* String options */
  if (profile)
    g_variant_builder_add (builder, "{sv}",
                           "profile", g_variant_new_string (profile));
  if (encoding)
    g_variant_builder_add (builder, "{sv}",
                           "encoding", g_variant_new_string (encoding));
  if (title)
    g_variant_builder_add (builder, "{sv}",
                           "title", g_variant_new_string (title));
  if (geometry)
    g_variant_builder_add (builder, "{sv}",
                           "geometry", g_variant_new_string (geometry));
  if (role)
    g_variant_builder_add (builder, "{sv}",
                           "role", g_variant_new_string (role));

  /* Boolean options */
  if (active)
    g_variant_builder_add (builder, "{sv}",
                           "active", g_variant_new_boolean (active));

  if (maximise_window)
    g_variant_builder_add (builder, "{sv}",
                           "maximize-window", g_variant_new_boolean (TRUE));
  if (fullscreen_window)
    g_variant_builder_add (builder, "{sv}",
                           "fullscreen-window", g_variant_new_boolean (TRUE));
}

char const* const*
terminal_client_get_environment_filters (void)
{
  static char const* filters[] = {
    "COLORFGBG",
    "COLORTERM",
    "COLUMNS",
    "DEFAULT_COLORS",
    "DESKTOP_STARTUP_ID",
    "EXIT_CODE",
    "EXIT_STATUS",
    "GIO_LAUNCHED_DESKTOP_FILE",
    "GIO_LAUNCHED_DESKTOP_FILE_PID",
    "GJS_DEBUG_OUTPUT",
    "GJS_DEBUG_TOPICS",
    "GNOME_DESKTOP_ICON",
    "INVOCATION_ID",
    "JOURNAL_STREAM",
    "LINES",
    "LISTEN_FDNAMES",
    "LISTEN_FDS",
    "LISTEN_PID",
    "MAINPID",
    "MANAGERPID",
    "NOTIFY_SOCKET",
    "NOTIFY_SOCKET",
    "PIDFILE",
    "PWD",
    "REMOTE_ADDR",
    "REMOTE_PORT",
    "SERVICE_RESULT",
    "SHLVL",
    "STY",
    "TERM",
    "TERMCAP",
    "TMUX",
    "TMUX_PANE",
    "VTE_VERSION",
    "WATCHDOG_PID",
    "WATCHDOG_USEC",
    "WCWIDTH_CJK_LEGACY",
    "WINDOWID",
    "XDG_ACTIVATION_TOKEN",
    nullptr
  };

  return filters;
}

char const* const*
terminal_client_get_environment_prefix_filters (void)
{
  static char const* filters[] = {
    "GNOME_TERMINAL_",
    // "VTE_", ?

    /* other terminals */
    "FOOT_",
    "ITERM2_",
    "MC_",
    "MINTTY_",
    "PUTTY_",
    "RXVT_",
    "TERM_",
    "URXVT_",
    "WEZTERM_",
    "XTERM_",
    nullptr
  };

  return filters;
}

static char const* const*
terminal_client_get_environment_prefix_filters_excludes(void)
{
  static char const* filters[] = {
    "MC_XDG_OPEN",

    nullptr,
  };

  return filters;
}

bool
terminal_client_get_environment_prefix_filters_is_excluded(char const* env)
{
  auto const excludes = terminal_client_get_environment_prefix_filters_excludes();
  for (auto j = 0; excludes[j]; ++j) {
    if (g_str_equal(excludes[j], env))
      return true;
  }

  return false;
}

static char**
terminal_environ_unsetenv_prefix (char** envv,
                                  char const* prefix)
{
  if (!envv)
    return envv;

  for (auto i = 0; envv[i]; ++i) {
    if (!g_str_has_prefix (envv[i], prefix))
      continue;

    auto const equal = strchr(envv[i], '=');
    g_assert(equal != nullptr);
    gs_free char* env = g_strndup(envv[i], equal - envv[i]);

    if (terminal_client_get_environment_prefix_filters_is_excluded(env))
      continue;

    envv = g_environ_unsetenv (envv, env);
  }

  return envv;
}

/**
 * terminal_client_filter_environment:
 * @envv: (transfer full): the environment
 *
 * Filters unwanted variables from @envv, and returns it.
 *
 * Returns: (transfer full): the filtered environment
 */
char**
terminal_client_filter_environment (char** envv)
{
  if (envv == nullptr)
    return nullptr;

  auto filters = terminal_client_get_environment_filters ();
  for (auto i = 0; filters[i]; ++i)
    envv = g_environ_unsetenv (envv, filters[i]);

  filters = terminal_client_get_environment_prefix_filters ();
  for (auto i = 0; filters[i]; ++i)
    envv = terminal_environ_unsetenv_prefix (envv, filters[i]);

  return envv;
}

/**
 * terminal_client_append_exec_options:
 * @builder: a #GVariantBuilder of #GVariantType "a{sv}"
 * @pass_environment: whether to pass the current environment
 * @working_directory: (allow-none): the cwd, or %nullptr
 * @fd_array: (array lenght=fd_array_len):
 * @fd_array_len:
 * @shell:
 *
 * Appends the environment and the working directory to @builder.
 */
void
terminal_client_append_exec_options (GVariantBuilder *builder,
                                     gboolean         pass_environment,
                                     const char      *working_directory,
                                     PassFdElement   *fd_array,
                                     gsize            fd_array_len,
                                     gboolean         shell)
{
  if (pass_environment) {
    gs_strfreev char **envv;

    envv = g_get_environ ();
    envv = terminal_client_filter_environment (envv);

    envv = g_environ_unsetenv (envv, TERMINAL_ENV_SERVICE_NAME);
    envv = g_environ_unsetenv (envv, TERMINAL_ENV_SCREEN);

    g_variant_builder_add (builder, "{sv}",
                           "environ",
                           g_variant_new_bytestring_array ((const char * const *) envv, -1));
  }

  if (working_directory)
    g_variant_builder_add (builder, "{sv}",
                           "cwd", g_variant_new_bytestring (working_directory));

  if (shell)
    g_variant_builder_add (builder, "{sv}",
                           "shell",
                           g_variant_new_boolean (TRUE));

  if (fd_array_len) {
    gsize i;

    g_variant_builder_open (builder, G_VARIANT_TYPE ("{sv}"));
    g_variant_builder_add (builder, "s", "fd-set");

    g_variant_builder_open (builder, G_VARIANT_TYPE ("v"));
    g_variant_builder_open (builder, G_VARIANT_TYPE ("a(ih)"));
    for (i = 0; i < fd_array_len; i++) {
      g_variant_builder_add (builder, "(ih)", fd_array[i].fd, fd_array[i].index);
    }
    g_variant_builder_close (builder); /* a(ih) */
    g_variant_builder_close (builder); /* v */

    g_variant_builder_close (builder); /* {sv} */
  }
}

#ifndef TERMINAL_NAUTILUS

/**
 * terminal_client_get_fallback_startup_id:
 *
 * Returns: a fallback startup ID, or %nullptr
 */
char *
terminal_client_get_fallback_startup_id  (void)
{
#if defined(TERMINAL_COMPILATION) && defined(GDK_WINDOWING_X11)
  GdkDisplay *display;
  Display *xdisplay;
  Window xwindow;
  XEvent event;

  display = gdk_display_get_default ();
  if (display == nullptr || !GDK_IS_X11_DISPLAY (display))
    goto out;

  xdisplay = GDK_DISPLAY_XDISPLAY (display);

  {
    XSetWindowAttributes attrs;
    Atom atom_name;
    Atom atom_type;
    const char *name;

    attrs.override_redirect = True;
    attrs.event_mask = PropertyChangeMask | StructureNotifyMask;

    xwindow =
      XCreateWindow (xdisplay,
                     RootWindow (xdisplay, 0),
                     -100, -100, 1, 1,
                     0,
                     CopyFromParent,
                     CopyFromParent,
                     (Visual *)CopyFromParent,
                     CWOverrideRedirect | CWEventMask,
                     &attrs);

    atom_name = XInternAtom (xdisplay, "WM_NAME", TRUE);
    g_assert (atom_name != None);
    atom_type = XInternAtom (xdisplay, "STRING", TRUE);
    g_assert (atom_type != None);

    name = "Fake Window";
    XChangeProperty (xdisplay,
                     xwindow, atom_name,
                     atom_type,
                     8, PropModeReplace, (unsigned char *)name, strlen (name));
  }

  XWindowEvent (xdisplay,
                xwindow,
                PropertyChangeMask,
                &event);

  XDestroyWindow(xdisplay, xwindow);

  return g_strdup_printf ("_TIME%lu", event.xproperty.time);
out:
#endif
  return nullptr;
}

#endif /* !TERMINAL_NAUTILUS */
