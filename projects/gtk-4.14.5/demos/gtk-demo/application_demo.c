/* Application Class
 *
 * Demonstrates a simple application.
 *
 * This example uses GtkApplication, GtkApplicationWindow, GtkBuilder
 * as well as GMenu and GResource. Due to the way GtkApplication is structured,
 * it is run as a separate process.
 */

#include "config.h"

#include <gtk/gtk.h>

static gboolean name_seen;
static GtkWidget *placeholder;

static void
on_name_appeared (GDBusConnection *connection,
                  const char      *name,
                  const char      *name_owner,
                  gpointer         user_data)
{
  name_seen = TRUE;
}

static void
on_name_vanished (GDBusConnection *connection,
                  const char      *name,
                  gpointer         user_data)
{
  if (!name_seen)
    return;

  g_clear_object (&placeholder);
}

#ifdef G_OS_WIN32
#define APP_EXTENSION ".exe"
#else
#define APP_EXTENSION
#endif

GtkWidget *
do_application_demo (GtkWidget *toplevel)
{
  static guint watch = 0;

  if (watch == 0)
    watch = g_bus_watch_name (G_BUS_TYPE_SESSION,
                              "org.gtk.Demo4.App",
                              0,
                              on_name_appeared,
                              on_name_vanished,
                              NULL, NULL);

  if (placeholder == NULL)
    {
      const char *command;
      GError *error = NULL;

      if (g_file_test ("./gtk4-demo-application" APP_EXTENSION, G_FILE_TEST_IS_EXECUTABLE))
        command = "./gtk4-demo-application" APP_EXTENSION;
      else
        command = "gtk4-demo-application";

      if (!g_spawn_command_line_async (command, &error))
        {
          g_warning ("%s", error->message);
          g_error_free (error);
        }

      placeholder = gtk_label_new ("");
      g_object_ref_sink (placeholder);
    }
  else
    {
      g_dbus_connection_call_sync (g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL),
                                   "org.gtk.Demo4.App",
                                   "/org/gtk/Demo4/App",
                                   "org.gtk.Actions",
                                   "Activate",
                                   g_variant_new ("(sava{sv})", "quit", NULL, NULL),
                                   NULL,
                                   0,
                                   G_MAXINT,
                                   NULL, NULL);
    }

  return placeholder;
}
