/*
 * Copyright © 2019 Benjamin Otte
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include <glib/gstdio.h>

#include "node-editor-application.h"

#include "node-editor-window.h"

#include "profile_conf.h"

static const char *css =
"textview.editor {"
"  color: rgb(192, 197, 206);"
"  caret-color: currentColor;"
"}"
"textview.editor > text {"
"  background-color: rgb(43, 48, 59);"
"}"
;

struct _NodeEditorApplication
{
  GtkApplication parent;
};

struct _NodeEditorApplicationClass
{
  GtkApplicationClass parent_class;
};

G_DEFINE_TYPE(NodeEditorApplication, node_editor_application, GTK_TYPE_APPLICATION);

static void
node_editor_application_init (NodeEditorApplication *app)
{
}

static void
activate_about (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  GtkApplication *app = user_data;
  char *version;
  GString *s;
  GskRenderer *gsk_renderer;
  const char *renderer;
  char *os_name;
  char *os_version;
  GtkWidget *dialog;

  os_name = g_get_os_info (G_OS_INFO_KEY_NAME);
  os_version = g_get_os_info (G_OS_INFO_KEY_VERSION_ID);
  s = g_string_new ("");
  if (os_name && os_version)
    g_string_append_printf (s, "OS\t%s %s\n\n", os_name, os_version);

  g_string_append (s, "System libraries\n");
  g_string_append_printf (s, "\tGLib\t%d.%d.%d\n",
                          glib_major_version,
                          glib_minor_version,
                          glib_micro_version);
  g_string_append_printf (s, "\tPango\t%s\n",
                          pango_version_string ());
  g_string_append_printf (s, "\tGTK \t%d.%d.%d\n",
                          gtk_get_major_version (),
                          gtk_get_minor_version (),
                          gtk_get_micro_version ());

  gsk_renderer = gtk_native_get_renderer (GTK_NATIVE (gtk_application_get_active_window (app)));
  if (strcmp (G_OBJECT_TYPE_NAME (gsk_renderer), "GskVulkanRenderer") == 0)
    renderer = "Vulkan";
  else if (strcmp (G_OBJECT_TYPE_NAME (gsk_renderer), "GskGLRenderer") == 0)
    renderer = "OpenGL";
  else if (strcmp (G_OBJECT_TYPE_NAME (gsk_renderer), "GskCairoRenderer") == 0)
    renderer = "Cairo";
  else
    renderer = "Unknown";

  g_string_append_printf (s, "\nRenderer\n\t%s", renderer);

  version = g_strdup_printf ("%s%s%s\nRunning against GTK %d.%d.%d",
                             PACKAGE_VERSION,
                             g_strcmp0 (PROFILE, "devel") == 0 ? "-" : "",
                             g_strcmp0 (PROFILE, "devel") == 0 ? VCS_TAG : "",
                             gtk_get_major_version (),
                             gtk_get_minor_version (),
                             gtk_get_micro_version ());

  dialog = g_object_new (GTK_TYPE_ABOUT_DIALOG,
                         "transient-for", gtk_application_get_active_window (app),
                         "program-name", g_strcmp0 (PROFILE, "devel") == 0
                                         ? "GTK Node Editor (Development)"
                                         : "GTK Node Editor",
                         "version", version,
                         "copyright", "© 2019—2021 The GTK Team",
                         "license-type", GTK_LICENSE_LGPL_2_1,
                         "website", "http://www.gtk.org",
                         "comments", "Program to test GTK rendering",
                         "authors", (const char *[]){ "Benjamin Otte", "Timm Bäder", NULL},
                         "logo-icon-name", "org.gtk.gtk4.NodeEditor",
                         "title", "About GTK Node Editor",
                         "system-information", s->str,
                         NULL);
    gtk_about_dialog_add_credit_section (GTK_ABOUT_DIALOG (dialog),
                                         "Artwork by", (const char *[]) { "Jakub Steiner", NULL });

  gtk_window_present (GTK_WINDOW (dialog));

  g_string_free (s, TRUE);
  g_free (version);
  g_free (os_name);
  g_free (os_version);
}

static void
activate_quit (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       data)
{
  g_application_quit (G_APPLICATION (data));
}

static void
activate_inspector (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  gtk_window_set_interactive_debugging (TRUE);
}

static void
activate_help (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  GtkBuilder *builder;
  GtkWidget *window;
  GtkTextBuffer *buffer;
  GBytes *bytes;
  const char *text;
  gsize len;

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, "/org/gtk/gtk4/node-editor/help-window.ui", NULL);
  window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
  buffer = GTK_TEXT_BUFFER (gtk_builder_get_object (builder, "buffer"));

  bytes = g_resources_lookup_data ("/org/gtk/gtk4/node-editor/node-format.md",
                                   G_RESOURCE_LOOKUP_FLAGS_NONE,
                                   NULL);
  text = g_bytes_get_data (bytes, &len);
  gtk_text_buffer_set_text (buffer, text, len);
  g_bytes_unref (bytes);

  gtk_window_present (GTK_WINDOW (window));
  g_object_unref (builder);
}

static GActionEntry app_entries[] =
{
  { "about", activate_about, NULL, NULL, NULL },
  { "quit", activate_quit, NULL, NULL, NULL },
  { "inspector", activate_inspector, NULL, NULL, NULL },
  { "help", activate_help, NULL, NULL, NULL },
};

static void
node_editor_application_startup (GApplication *app)
{
  const char *help_accels[2] = { "F1", NULL };
  const char *quit_accels[2] = { "<Ctrl>Q", NULL };
  const char *open_accels[2] = { "<Ctrl>O", NULL };
  GtkCssProvider *provider;

  G_APPLICATION_CLASS (node_editor_application_parent_class)->startup (app);

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   app_entries, G_N_ELEMENTS (app_entries),
                                   app);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "app.help", help_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "app.quit", quit_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "win.open", open_accels);


  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_string (provider, css);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
node_editor_application_activate (GApplication *app)
{
  NodeEditorWindow *win;

  win = node_editor_window_new (NODE_EDITOR_APPLICATION (app));

  if (g_strcmp0 (PROFILE, "devel") == 0)
    gtk_widget_add_css_class (GTK_WIDGET (win), "devel");

  gtk_window_present (GTK_WINDOW (win));
}

static void
node_editor_application_open (GApplication  *app,
                              GFile        **files,
                              int            n_files,
                              const char    *hint)
{
  NodeEditorWindow *win;
  int i;

  for (i = 0; i < n_files; i++)
    {
      win = node_editor_window_new (NODE_EDITOR_APPLICATION (app));
      node_editor_window_load (win, files[i]);
      gtk_window_present (GTK_WINDOW (win));
    }
}

static void
node_editor_application_class_init (NodeEditorApplicationClass *class)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);

  application_class->startup = node_editor_application_startup;
  application_class->activate = node_editor_application_activate;
  application_class->open = node_editor_application_open;
}

static void
print_version (void)
{
  g_print ("gtk4-node-editor %s%s%s\n",
           PACKAGE_VERSION,
           g_strcmp0 (PROFILE, "devel") == 0 ? "-" : "",
           g_strcmp0 (PROFILE, "devel") == 0 ? VCS_TAG : "");
}

static int
local_options (GApplication *app,
               GVariantDict *options,
               gpointer      data)
{
  gboolean version = FALSE;
  gboolean reset = FALSE;

  g_variant_dict_lookup (options, "version", "b", &version);

  if (version)
    {
      print_version ();
      return 0;
    }

  g_variant_dict_lookup (options, "reset", "b", &reset);

  if (reset)
    {
      char *path;

      path = get_autosave_path ("-unsafe");
      g_remove (path);
      g_free (path);
      path = get_autosave_path (NULL);
      g_remove (path);
      g_free (path);
    }

  return -1;
}


NodeEditorApplication *
node_editor_application_new (void)
{
  NodeEditorApplication *app;

  app = g_object_new (NODE_EDITOR_APPLICATION_TYPE,
                      "application-id", "org.gtk.gtk4.NodeEditor",
                      "flags", G_APPLICATION_HANDLES_OPEN,
                      NULL);

  g_application_add_main_option (G_APPLICATION (app), "version", 0, 0,G_OPTION_ARG_NONE, "Show program version", NULL);
  g_application_add_main_option (G_APPLICATION (app), "reset", 0, 0,G_OPTION_ARG_NONE, "Remove autosave content", NULL);

  g_signal_connect (app, "handle-local-options", G_CALLBACK (local_options), NULL);

  return app;
}
