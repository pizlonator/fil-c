/*
 * Copyright © 2001, 2002 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2002 Sun Microsystems
 * Copyright © 2003 Mariano Suarez-Alvarez
 * Copyright © 2008, 2010, 2011 Christian Persch
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

#include <config.h>

#include <errno.h>
#include <locale.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/types.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include "terminal-app.hh"
#include "terminal-debug.hh"
#include "terminal-gdbus.hh"
#include "terminal-i18n.hh"
#include "terminal-defines.hh"
#include "terminal-libgsystem.hh"

static char *app_id = nullptr;

#define INACTIVITY_TIMEOUT (100 /* ms */)


/* Fil-C: this used to interpose g_module_open with a dlsym(RTLD_NEXT)
 * wrapper to blocklist the pk-gtk module (which tries to automagically
 * install fonts).  The pizlonated libc's dlsym goes through zsys_dlsym,
 * which rejects the RTLD_NEXT pseudo-handle ("cannot access pointer with
 * null object (ptr = 0xffffffffffffffff,<null>)" filc panic), and there is
 * no pk-gtk module in the pizlonated system anyway, so the interposer is
 * removed and g_module_open calls go straight to libgmodule. */

static gboolean
option_app_id_cb (const gchar *option_name,
                    const gchar *value,
                    gpointer     data,
                    GError     **error)
{
  if (!g_application_id_is_valid (value) ||
      !g_dbus_is_name (value)) {
    g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                 "\"%s\" is not a valid application ID", value);
    return FALSE;
  }

  g_free (app_id);
  app_id = g_strdup (value);

  return TRUE;
}

static const GOptionEntry options[] = {
  { "app-id", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_CALLBACK, (void*)option_app_id_cb, "Application ID", "ID" },
  { nullptr }
};

/* We use up to 8 FDs per terminal, so let's bump the limit way up.
 * However we need to restore the original limit for the child processes.
 */

static struct rlimit sv_rlimit_nofile;

static void
atfork_child_restore_rlimit_nofile (void)
{
  if (setrlimit (RLIMIT_NOFILE, &sv_rlimit_nofile) < 0)
    _exit (127);
}

static gboolean
increase_rlimit_nofile (void)
{
  struct rlimit l;

  if (getrlimit (RLIMIT_NOFILE, &sv_rlimit_nofile) < 0)
    return FALSE;

  if (pthread_atfork (nullptr, nullptr, atfork_child_restore_rlimit_nofile) != 0)
    return FALSE;

  l.rlim_cur = l.rlim_max = sv_rlimit_nofile.rlim_max;
  if (setrlimit (RLIMIT_NOFILE, &l) < 0)
    return FALSE;

  return TRUE;
}

static int
init_server (int argc,
             char *argv[],
             GApplication **application)
{
  if (G_UNLIKELY ((getuid () != geteuid () ||
                  getgid () != getegid ()) &&
                  geteuid () == 0 &&
                  getegid () == 0)) {
    g_printerr ("Wrong euid/egid, exiting.\n");
    return _EXIT_FAILURE_WRONG_ID;
  }

  if (setlocale (LC_ALL, "") == nullptr) {
    g_printerr ("Locale not supported.\n");
    return _EXIT_FAILURE_UNSUPPORTED_LOCALE;
  }

  terminal_i18n_init (TRUE);

  g_unsetenv ("CHARSET");
  g_unsetenv ("OUTPUT_CHARSET");
  const char *charset;
  if (!g_get_charset (&charset)) {
    g_printerr ("Non UTF-8 locale (%s) is not supported!\n", charset);
    return _EXIT_FAILURE_NO_UTF8;
  }

  /* Sanitise environment */
  g_unsetenv ("DBUS_STARTER_BUS_TYPE");

  /* Not interested in silly debug spew polluting the journal, bug #749195 */
  if (g_getenv ("G_ENABLE_DIAGNOSTIC") == nullptr)
    g_setenv ("G_ENABLE_DIAGNOSTIC", "0", TRUE);

  _terminal_debug_init ();

  /* Change directory to $HOME so we don't prevent unmounting, e.g. if the
   * factory is started by nautilus-open-terminal. See bug #565328.
   * On failure back to /.
   */
  const char *home_dir = g_get_home_dir ();
  if (home_dir == nullptr || chdir (home_dir) < 0)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
    chdir ("/");
#pragma GCC diagnostic pop

  g_set_prgname (app_id ? app_id : TERMINAL_APPLICATION_ID);
  g_set_application_name (_("Terminal"));

  GError *error = nullptr;
  if (!gtk_init_with_args (&argc, &argv, nullptr, options, nullptr, &error)) {
    if (error != nullptr) {
      g_printerr ("Failed to parse arguments: %s\n", error->message);
      g_error_free (error);
    }

    return _EXIT_FAILURE_GTK_INIT;
  }

  if (!increase_rlimit_nofile ()) {
    auto const errsv = errno;
    g_printerr ("Failed to increase RLIMIT_NOFILE: %s\n", g_strerror(errsv));
  }

  /* Now we can create the app */
  auto const app = terminal_app_new (app_id, G_APPLICATION_IS_SERVICE, nullptr);
  g_free (app_id);
  app_id = nullptr;

  /* We stay around a bit after the last window closed */
  g_application_set_inactivity_timeout (app, INACTIVITY_TIMEOUT);

  *application = app;
  return 0;
}

int
main (int argc,
      char *argv[])
{
  gs_unref_object GApplication *app = nullptr;
  int r = init_server (argc, argv, &app);
  if (r != 0)
    return r;

  /* Note that this flushes the D-Bus connection just before quitting,
   * thus ensuring that all pending signal emissions (e.g. child-exited)
   * are delivered.
   */
  return g_application_run (app, 0, nullptr);
}
