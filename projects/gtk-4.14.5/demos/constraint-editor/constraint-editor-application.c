/*
 * Copyright © 2019 Red Hat, Inc.
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
 * Authors: Matthias Clasen
 */

#include "config.h"

#include "constraint-editor-application.h"
#include "constraint-editor-window.h"

struct _ConstraintEditorApplication
{
  GtkApplication parent_instance;
};

G_DEFINE_TYPE(ConstraintEditorApplication, constraint_editor_application, GTK_TYPE_APPLICATION);

static void
constraint_editor_application_init (ConstraintEditorApplication *app)
{
}

static void
quit_activated (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       data)
{
  g_application_quit (G_APPLICATION (data));
}

static GActionEntry app_entries[] =
{
  { "quit", quit_activated, NULL, NULL, NULL }
};

static void
constraint_editor_application_startup (GApplication *app)
{
  const char *quit_accels[2] = { "<Ctrl>Q", NULL };
  const char *open_accels[2] = { "<Ctrl>O", NULL };
  GtkCssProvider *provider;

  G_APPLICATION_CLASS (constraint_editor_application_parent_class)->startup (app);

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   app_entries, G_N_ELEMENTS (app_entries),
                                   app);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "app.quit", quit_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "win.open", open_accels);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gtk/gtk4/constraint-editor/constraint-editor.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
constraint_editor_application_activate (GApplication *app)
{
  ConstraintEditorWindow *win;

  win = constraint_editor_window_new (CONSTRAINT_EDITOR_APPLICATION (app));
  gtk_window_present (GTK_WINDOW (win));
}

static void
constraint_editor_application_open (GApplication  *app,
                                    GFile        **files,
                                    int            n_files,
                                    const char    *hint)
{
  ConstraintEditorWindow *win;
  int i;

  for (i = 0; i < n_files; i++)
    {
      win = constraint_editor_window_new (CONSTRAINT_EDITOR_APPLICATION (app));
      constraint_editor_window_load (win, files[i]);
      gtk_window_present (GTK_WINDOW (win));
    }
}

static void
constraint_editor_application_class_init (ConstraintEditorApplicationClass *class)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);

  application_class->startup = constraint_editor_application_startup;
  application_class->activate = constraint_editor_application_activate;
  application_class->open = constraint_editor_application_open;
}

ConstraintEditorApplication *
constraint_editor_application_new (void)
{
  return g_object_new (CONSTRAINT_EDITOR_APPLICATION_TYPE,
                       "application-id", "org.gtk.gtk4.ConstraintEditor",
                       "flags", G_APPLICATION_HANDLES_OPEN,
                       NULL);
}
