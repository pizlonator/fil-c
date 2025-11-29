/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Tomas Bzatek <tbzatek@redhat.com>
 */

#include <config.h>

#include <stdio.h>
#include <unistd.h>
#include <locale.h>
#include <errno.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#if defined(HAVE_GIO_UNIX) && !defined(__APPLE__)
#include <gio/gdesktopappinfo.h>
#endif
#include <gtk.h>

static gboolean show_version;
static char **args = NULL;

static GOptionEntry entries[] = {
  { "version", 0, 0, G_OPTION_ARG_NONE, &show_version, N_("Show program version"), NULL },
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &args, NULL, NULL },
  { NULL}
};

int
main (int argc, char *argv[])
{
  GError *error = NULL;
  GOptionContext *context = NULL;
  char *summary;
  char *app_name;
#if defined(HAVE_GIO_UNIX) && !defined(__APPLE__)
  char *desktop_file_name;
#endif
#ifdef G_OS_UNIX
  char *bus_name = NULL;
#endif
  GAppInfo *info = NULL;
  GAppLaunchContext *launch_context;
  GList *l;
  GFile *f;

  setlocale (LC_ALL, "");

  bindtextdomain (GETTEXT_PACKAGE, GTK_LOCALEDIR);
  textdomain (GETTEXT_PACKAGE);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

  context =
  /* Translators: this message will appear immediately after the */
  /* usage string - Usage: COMMAND [OPTION…] <THIS_MESSAGE>    */
    g_option_context_new (_("APPLICATION [URI…] — launch an APPLICATION"));

  /* Translators: this message will appear after the usage string */
  /* and before the list of options.                              */
  summary = _("Launch an application (specified by its desktop file name),\n"
              "optionally passing one or more URIs as arguments.");
  g_option_context_set_summary (context, summary);
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  g_option_context_parse (context, &argc, &argv, &error);

  g_option_context_free (context);

  if (error != NULL)
    {
      g_printerr (_("Error parsing commandline options: %s\n"), error->message);
      g_printerr ("\n");
      g_printerr (_("Try “%s --help” for more information."), g_get_prgname ());
      g_printerr ("\n");
      g_error_free (error);
      return 1;
    }

  if (show_version)
    {
      g_print ("%d.%d.%d\n",
               gtk_get_major_version (),
               gtk_get_minor_version (),
               gtk_get_micro_version ());
      return 0;
    }

  if (!args)
    {
      /* Translators: the %s is the program name. This error message */
      /* means the user is calling gtk-launch without any argument.  */
      g_printerr (_("%s: missing application name"), g_get_prgname ());
      g_printerr ("\n");
      g_printerr (_("Try “%s --help” for more information."), g_get_prgname ());
      g_printerr ("\n");
      return 1;
    }


  gtk_init ();

  app_name = *args;
#if defined(HAVE_GIO_UNIX) && !defined(__APPLE__)
  bus_name = g_strdup (app_name);
  if (g_str_has_suffix (app_name, ".desktop"))
    {
      desktop_file_name = g_strdup (app_name);
      bus_name[strlen (bus_name) - strlen(".desktop")] = '\0';
    }
  else
    {
      desktop_file_name = g_strconcat (app_name, ".desktop", NULL);
    }

  if (!g_dbus_is_name (bus_name))
    g_clear_pointer (&bus_name, g_free);
  info = G_APP_INFO (g_desktop_app_info_new (desktop_file_name));
  g_free (desktop_file_name);
#else
  g_printerr (_("Creating AppInfo from id not supported on non unix operating systems"));
#endif
  args++;

  if (!info)
    {
      /* Translators: the first %s is the program name, the second one */
      /* is the application name.                                      */
      g_printerr (_("%s: no such application %s"),
                  g_get_prgname (), app_name);
      g_printerr ("\n");
      return 2;
    }

  l = NULL;
  for (; *args; args++)
    {
      f = g_file_new_for_commandline_arg (*args);
      l = g_list_append (l, f);
    }

  launch_context = (GAppLaunchContext*) gdk_display_get_app_launch_context (gdk_display_get_default ());
  if (!g_app_info_launch (info, l, launch_context, &error))
    {
       /* Translators: the first %s is the program name, the second one  */
       /* is the error message.                                          */
       g_printerr (_("%s: error launching application: %s\n"),
                   g_get_prgname (), error->message);
       return 3;
    }
  g_object_unref (info);
  g_object_unref (launch_context);

#ifdef G_OS_UNIX
  if (bus_name != NULL)
    {
      GDBusConnection *connection;
      char *object_path, *p;

      connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

      object_path = g_strdup_printf ("/%s", bus_name);
      for (p = object_path; *p != '\0'; p++)
          if (*p == '.')
              *p = '/';

      if (connection)
        g_dbus_connection_call_sync (connection,
                                     bus_name,
                                     object_path,
                                     "org.freedesktop.DBus.Peer",
                                     "Ping",
                                     NULL, NULL,
                                     G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);
      g_clear_pointer (&object_path, g_free);
      g_clear_object (&connection);
      g_clear_pointer (&bus_name, g_free);
    }
#endif
  g_list_free_full (l, g_object_unref);

  return 0;
}
