/*  Copyright 2015 Red Hat, Inc.
 *
 * GTK is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GLib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GTK; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <glib/gi18n-lib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include "gtkbuilderprivate.h"
#include "gtk-builder-tool.h"
#include "fake-scope.h"


static GType
make_fake_type (const char *type_name,
                const char *parent_name)
{
  GType parent_type;
  GTypeQuery query;

  parent_type = g_type_from_name (parent_name);
  if (parent_type == G_TYPE_INVALID)
    {
      g_printerr (_("Failed to lookup template parent type %s\n"), parent_name);
      exit (1);
    }

  g_type_query (parent_type, &query);
  return g_type_register_static_simple (parent_type,
                                        type_name,
                                        query.class_size,
                                        NULL,
                                        query.instance_size,
                                        NULL,
                                        0);
}

/* {{{ Deprecations */

static gboolean
is_deprecated (const char *name)
{
  const char *names[] = {
    "GtkAppChooser",
    "GtkAppChooserButton",
    "GtkAppChooserDialog",
    "GtkAppChooserWidget",
    "GtkCellAreaBox",
    "GtkCellAreaBoxContext",
    "GtkCellArea",
    "GtkCellEditable",
    "GtkCellLayout",
    "GtkCellRendererAccel",
    "GtkCellRenderer",
    "GtkCellRendererCombo",
    "GtkCellRendererPixbuf",
    "GtkCellRendererProgress",
    "GtkCellRendererSpin",
    "GtkCellRendererSpinner",
    "GtkCellRendererText",
    "GtkCellRendererToggle",
    "GtkCellView",
    "GtkComboBox",
    "GtkComboBoxText",
    "GtkEntryCompletion",
    "GtkIconView",
    "GtkListStore",
    "GtkStyleContext",
    "GtkTreeModel",
    "GtkTreeModelFilter",
    "GtkTreeModelSort",
    "GtkTreePopover",
    "GtkTreeSelection",
    "GtkTreeSortable",
    "GtkTreeStore",
    "GtkTreeView",
    "GtkTreeViewColumn",
    NULL
  };

  return g_strv_contains (names, name);
}

static gboolean
fake_scope_check_deprecations (FakeScope  *self,
                               GError    **error)
{
  GPtrArray *types;
  GString *s;

  types = fake_scope_get_types (self);

  s = g_string_new ("");

  for (int i = 0; i < types->len; i++)
    {
      const char *name = g_ptr_array_index (types, i);

      if (is_deprecated (name))
        {
          if (s->len == 0)
            g_string_append (s, _("Deprecated types:\n"));
          g_string_append_printf (s, "%s", name);
          g_string_append (s, "\n");
        }
    }

  if (s->len > 0)
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, s->str);

  g_string_free (s, TRUE);

  return *error == NULL;
}

/* }}} */

static gboolean
validate_template (const char *filename,
                   const char *type_name,
                   const char *parent_name,
                   gboolean    deprecations)
{
  GType template_type;
  GObject *object;
  FakeScope *scope;
  GtkBuilder *builder;
  GError *error = NULL;
  gboolean ret;

  builder = gtk_builder_new ();
  scope = fake_scope_new ();
  gtk_builder_set_scope (builder, GTK_BUILDER_SCOPE (scope));
  g_object_unref (scope);

  /* Only make a fake type if it doesn't exist yet.
   * This lets us e.g. validate the GtkFileChooserWidget template.
   */
  template_type = gtk_builder_get_type_from_name (builder, type_name);
  if (template_type == G_TYPE_INVALID)
    template_type = make_fake_type (type_name, parent_name);

  object = g_object_new (template_type, NULL);
  if (!object)
    {
      g_printerr (_("Failed to create an instance of the template type %s\n"), type_name);
      return FALSE;
    }

  ret = gtk_builder_extend_with_template (builder, object, template_type, " ", 1, &error);
  if (ret)
    ret = gtk_builder_add_from_file (builder, filename, &error);
  if (ret && deprecations)
    ret = fake_scope_check_deprecations (scope, &error);
  g_object_unref (builder);

  if (!ret)
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
    }

  return ret;
}

static gboolean
parse_template_error (const char   *message,
                      char        **class_name,
                      char        **parent_name)
{
  char *p;

  p = strstr (message, "(class '");
  if (p)
    {
      *class_name = g_strdup (p + strlen ("(class '"));
      p = strstr (*class_name, "'");
      if (p)
        *p = '\0';
    }
  p = strstr (message, ", parent '");
  if (p)
    {
      *parent_name = g_strdup (p + strlen (", parent '"));
      p = strstr (*parent_name, "'");
      if (p)
        *p = '\0';
    }

  return *class_name && *parent_name;
}

static gboolean
validate_file (const char *filename,
               gboolean    deprecations)
{
  FakeScope *scope;
  GtkBuilder *builder;
  GError *error = NULL;
  gboolean ret;
  char *class_name = NULL;
  char *parent_name = NULL;

  builder = gtk_builder_new ();
  scope = fake_scope_new ();
  gtk_builder_set_scope (builder, GTK_BUILDER_SCOPE (scope));
  ret = gtk_builder_add_from_file (builder, filename, &error);
  if (ret && deprecations)
    ret = fake_scope_check_deprecations (scope, &error);
  g_object_unref (scope);
  g_object_unref (builder);

  if (!ret)
    {
      if (g_error_matches (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_UNHANDLED_TAG) &&
          parse_template_error (error->message, &class_name, &parent_name))
        {
          ret = validate_template (filename, class_name, parent_name, deprecations);
        }
      else
        {
          g_printerr ("%s\n", error->message);
        }

      g_error_free (error);
    }

  return ret;
}

void
do_validate (int *argc, const char ***argv)
{
  GError *error = NULL;
  char **filenames = NULL;
  gboolean deprecations = FALSE;
  GOptionContext *context;
  const GOptionEntry entries[] = {
    { "deprecations", 0, 0, G_OPTION_ARG_NONE, &deprecations, NULL, NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("FILE") },
    { NULL, }
  };
  int i;

  if (gdk_display_get_default () == NULL)
    {
      g_printerr (_("Could not initialize windowing system\n"));
      exit (1);
    }

  g_set_prgname ("gtk4-builder-tool validate");
  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Validate the file."));

  if (!g_option_context_parse (context, argc, (char ***)argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  if (!filenames)
    {
      g_printerr (_("No .ui file specified\n"));
      exit (1);
    }

  g_option_context_free (context);

  for (i = 0; filenames[i]; i++)
    {
      if (!validate_file (filenames[i], deprecations))
        exit (1);
    }

  g_strfreev (filenames);
}

/* vim:set foldmethod=marker expandtab: */
