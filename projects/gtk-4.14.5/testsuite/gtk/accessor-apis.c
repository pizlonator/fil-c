/*
 * Copyright © 2020 Benjamin Otte
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

#include <gtk/gtk.h>

static struct
{
  const char *expected;
  const char *alternative;
} exceptions[] = {
  /* keep this sorted please */
  { "gdk_device_get_tool", "gdk_device_get_device_tool" },
  { "gdk_display_get_input_shapes", "gdk_display_supports_input_shapes" },
  { "gtk_constraint_guide_get_max_height", "gtk_constraint_guide_get_max_size" },
  { "gtk_constraint_guide_get_max_width", "gtk_constraint_guide_get_max_size" },
  { "gtk_constraint_guide_get_min_height", "gtk_constraint_guide_get_min_size" },
  { "gtk_constraint_guide_get_min_width", "gtk_constraint_guide_get_min_size" },
  { "gtk_constraint_guide_get_nat_height", "gtk_constraint_guide_get_nat_size" },
  { "gtk_constraint_guide_get_nat_width", "gtk_constraint_guide_get_nat_size" },
  { "gtk_constraint_guide_set_max_height", "gtk_constraint_guide_set_max_size" },
  { "gtk_constraint_guide_set_max_width", "gtk_constraint_guide_set_max_size" },
  { "gtk_constraint_guide_set_min_height", "gtk_constraint_guide_set_min_size" },
  { "gtk_constraint_guide_set_min_width", "gtk_constraint_guide_set_min_size" },
  { "gtk_constraint_guide_set_nat_height", "gtk_constraint_guide_set_nat_size" },
  { "gtk_constraint_guide_set_nat_width", "gtk_constraint_guide_set_nat_size" },
  { "gtk_tree_view_get_enable_grid_lines", "gtk_tree_view_get_grid_lines" },
  { "gtk_tree_view_set_enable_grid_lines", "gtk_tree_view_set_grid_lines" },
  { "gtk_widget_get_height_request", "gtk_widget_get_size_request" },
  { "gtk_widget_get_width_request", "gtk_widget_get_size_request" },
  { "gtk_widget_set_height_request", "gtk_widget_set_size_request" },
  { "gtk_widget_set_width_request", "gtk_widget_set_size_request" },
  { "gtk_window_get_default_height", "gtk_window_get_default_size" },
  { "gtk_window_get_default_width", "gtk_window_get_default_size" },
  { "gtk_window_set_default_height", "gtk_window_set_default_size" },
  { "gtk_window_set_default_width", "gtk_window_set_default_size" },
  { "gtk_window_get_display", "gtk_widget_get_display" },
  { "gtk_window_get_focus_widget", "gtk_window_get_focus" },
  { "gtk_window_set_focus_widget", "gtk_window_set_focus" },
};

static const char *type_exceptions[] = {
  "GtkCellRenderer",
  "GtkSettings",
  "GtkTextTag",
};

static GModule *module;

static gboolean
function_exists (const char *function_name)
{
  gpointer func;
  guint i;

  if (g_module_symbol (module, function_name, &func) && func)
    return TRUE;

  for (i = 0; i < G_N_ELEMENTS(exceptions); i++)
    {
      if (g_str_equal (function_name, exceptions[i].expected))
        {
          if (exceptions[i].alternative)
            return function_exists (exceptions[i].alternative);
          else
            return TRUE;
        }
    }

  return FALSE;
}

/* Keep in sync with gtkbuilder.c ! */
static char *
type_name_mangle (const char *name,
                  gboolean    split_first_cap)
{
  GString *symbol_name = g_string_new ("");
  int i;

  for (i = 0; name[i] != '\0'; i++)
    {
      /* skip if uppercase, first or previous is uppercase */
      if ((name[i] == g_ascii_toupper (name[i]) &&
             ((i > 0 && name[i-1] != g_ascii_toupper (name[i-1])) ||
              (i == 1 && name[0] == g_ascii_toupper (name[0]) && split_first_cap))) ||
           (i > 2 && name[i]  == g_ascii_toupper (name[i]) &&
           name[i-1] == g_ascii_toupper (name[i-1]) &&
           name[i-2] == g_ascii_toupper (name[i-2])))
        g_string_append_c (symbol_name, '_');
      g_string_append_c (symbol_name, g_ascii_tolower (name[i]));
    }

  return g_string_free (symbol_name, FALSE);
}

static void
property_name_mangle (GString    *symbol_name,
                      const char *name)
{
  guint i;

  for (i = 0; name[i] != '\0'; i++)
    {
      if (g_ascii_isalnum (name[i]))
        g_string_append_c (symbol_name, g_ascii_tolower (name[i]));
      else
        g_string_append_c (symbol_name, '_');
    }
}

static void
add_type_name (GPtrArray *type_names,
               GType      type)
{
  char *options[2];

  options[0] = type_name_mangle (g_type_name (type), FALSE);
  g_ptr_array_add (type_names, options[0]);

  options[1] = type_name_mangle (g_type_name (type), TRUE);
  if (g_str_equal (options[0], options[1]))
    g_free (options[1]);
  else
    g_ptr_array_add (type_names, options[1]);
}

const char *getters[] = { "get", "is", "ref" };
const char *setters[] = { "set" };

static char **
get_potential_names (GType       type,
                     gboolean    get,
                     const char *property_name)
{
  GPtrArray *options;
  GPtrArray *type_names;
  GType *interfaces;
  guint n_verbs, n_interfaces;
  const char **verbs;
  guint i, j;

  if (get)
    {
      verbs = getters;
      n_verbs = G_N_ELEMENTS (getters);
    }
  else
    {
      verbs = setters;
      n_verbs = G_N_ELEMENTS (setters);
    }

  type_names = g_ptr_array_new_with_free_func (g_free);
  add_type_name (type_names, type);
  interfaces = g_type_interfaces (type, &n_interfaces);
  for (i = 0; i < n_interfaces; i++)
    add_type_name (type_names, interfaces[i]);
  g_free (interfaces);

  options = g_ptr_array_new ();

  for (i = 0; i < type_names->len; i++)
    {
      for (j = 0; j < n_verbs; j++)
        {
          GString *str;

          str = g_string_new (g_ptr_array_index (type_names, i));
          g_string_append_c (str, '_');
          g_string_append (str, verbs[j]);
          g_string_append_c (str, '_');
          property_name_mangle (str, property_name);

          g_ptr_array_add (options, g_string_free (str, FALSE));
        }

      if (g_str_has_prefix (property_name, "is-") ||
          g_str_has_prefix (property_name, "has-") ||
          g_str_has_prefix (property_name, "contains-"))
        {
          GString *str;

          /* try without a verb */
          str = g_string_new (g_ptr_array_index (type_names, i));
          g_string_append_c (str, '_');
          property_name_mangle (str, property_name);

          g_ptr_array_add (options, g_string_free (str, FALSE));
        }
    }

  g_ptr_array_free (type_names, TRUE);

  g_ptr_array_add (options, NULL);

  return (char **) g_ptr_array_free (options, FALSE);
}

static void
check_function_name (GType       type,
                     gboolean    get,
                     const char *property_name)
{
  guint i;
  char **names;

  names = get_potential_names (type, get, property_name);

  for (i = 0; names[i] != NULL; i++)
    {
      if (function_exists (names[i]))
        {
          g_strfreev (names);
          return;
        }
    }

  g_test_message ("No %s for property %s::%s", get ? "getter" : "setter", g_type_name (type), property_name);
  if (g_test_verbose ())
    {
      for (i = 0; names[i] != NULL; i++)
        {
          g_test_message ("    %s", names[i]);
        }
    }

  g_test_fail ();

  g_strfreev (names);
}

static void
check_property (GParamSpec *pspec)
{
  if (pspec->flags & G_PARAM_READABLE)
    {
      check_function_name (pspec->owner_type,
                           TRUE,
                           pspec->name);
    }
  if (pspec->flags & G_PARAM_WRITABLE &&
      !(pspec->flags & G_PARAM_CONSTRUCT_ONLY))
    {
      check_function_name (pspec->owner_type,
                           FALSE,
                           pspec->name);
    }
}

static void
test_accessors (gconstpointer data)
{
  GType type = GPOINTER_TO_SIZE (data);
  GObjectClass *klass;
  GParamSpec **pspecs;
  guint i, n_pspecs;

  klass = g_type_class_ref (type);
  pspecs = g_object_class_list_properties (klass, &n_pspecs);

  for (i = 0; i < n_pspecs; ++i)
    {
      GParamSpec *pspec = pspecs[i];

      if (pspec->owner_type != type)
	continue;

      check_property (pspec);
    }

  g_free (pspecs);
  g_type_class_unref (klass);
}

static gboolean
type_is_whitelisted (GType type)
{
  guint i;

  if (!G_TYPE_IS_INSTANTIATABLE (type) ||
      !g_type_is_a (type, G_TYPE_OBJECT))
    return TRUE;

  for (i = 0; i < G_N_ELEMENTS (type_exceptions); i++)
    {
      GType exception = g_type_from_name (type_exceptions[i]);

      /* type hasn't been registered yet */
      if (exception == G_TYPE_INVALID)
        continue;

      if (g_type_is_a (type, exception))
        return TRUE;
    }

  return FALSE;
}

int
main (int argc, char **argv)
{
  const GType *all_types;
  guint n_types = 0, i;
  int result;

  /* These must be set before before gtk_test_init */
  g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);

  /* initialize test program */
  gtk_test_init (&argc, &argv);
  gtk_test_register_all_types ();

  module = g_module_open (NULL, G_MODULE_BIND_LAZY);

  all_types = gtk_test_list_all_types (&n_types);

  for (i = 0; i < n_types; i++)
    {
      char *test_path;

      if (type_is_whitelisted (all_types[i]))
        continue;

      test_path = g_strdup_printf ("/accessor-apis/%s", g_type_name (all_types[i]));

      g_test_add_data_func (test_path, GSIZE_TO_POINTER (all_types[i]), test_accessors);

      g_free (test_path);
    }

  result = g_test_run();

  g_module_close (module);

  return result;
}
