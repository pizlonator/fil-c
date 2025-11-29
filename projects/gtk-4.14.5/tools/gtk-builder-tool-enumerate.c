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

static const char *
object_get_id (GObject *object)
{
  if (GTK_IS_BUILDABLE (object))
    return gtk_buildable_get_buildable_id (GTK_BUILDABLE (object));
  else
    return g_object_get_data (object, "gtk-builder-id");
}

void
do_enumerate (int *argc, const char ***argv)
{
  FakeScope *scope;
  GtkBuilder *builder;
  GError *error = NULL;
  int ret;
  GSList *list, *l;
  gboolean callbacks = FALSE;
  char **filenames = NULL;
  GOptionContext *context;
  const GOptionEntry entries[] = {
    { "callbacks", 0, 0, G_OPTION_ARG_NONE, &callbacks, "Also print callbacks", NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("FILE") },
    { NULL, }
  };

  g_set_prgname ("gtk4-builder-tool enumerate");
  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Print all named objects."));

  if (!g_option_context_parse (context, argc, (char ***)argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  g_option_context_free (context);

  if (filenames == NULL)
    {
      g_printerr ("No .ui file specified\n");
      exit (1);
    }

  if (g_strv_length (filenames) > 1)
    {
      g_printerr ("Can only enumerate a single .ui file\n");
      exit (1);
    }

  builder = gtk_builder_new ();
  scope = fake_scope_new ();
  gtk_builder_set_scope (builder, GTK_BUILDER_SCOPE (scope));

  ret = gtk_builder_add_from_file (builder, filenames[0], &error);

  if (ret == 0)
    {
      g_printerr ("%s\n", error->message);
      exit (1);
    }

  if (callbacks)
    g_print ("Objects:\n");

  list = gtk_builder_get_objects (builder);
  for (l = list; l; l = l->next)
    {
      GObject *object = l->data;
      const char *name = object_get_id (object);
      if (g_str_has_prefix (name, "___") && g_str_has_suffix (name, "___"))
        continue;

      g_printf ("%s (%s)\n", name, g_type_name_from_instance ((GTypeInstance*)object));
    }
  g_slist_free (list);

  if (callbacks)
    {
      GPtrArray *names;
      gboolean need_prefix = TRUE;

      names = fake_scope_get_callbacks (scope);
      for (int i = 0; i < names->len; i++)
        {
          const char *name = g_ptr_array_index (names, i);

          if (need_prefix)
            {
              need_prefix = FALSE;
              g_print ("\nCallbacks:\n");
            }

          g_print ("%s\n", name);
        }
    }

  g_object_unref (scope);
  g_object_unref (builder);

  g_strfreev (filenames);
}
