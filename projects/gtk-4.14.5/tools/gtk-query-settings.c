/*  Copyright 2015 Timm Bäder
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
 */

#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>


int
main (int argc, char **argv)
{
  GtkSettings  *settings;
  GParamSpec  **props;
  guint         n_properties;
  guint         i;
  int           max_prop_name_length = 0;
  char         *pattern = NULL;

  gtk_init ();

  if (argc > 1)
    pattern = argv[1];

  settings = gtk_settings_get_default ();
  props = g_object_class_list_properties (G_OBJECT_GET_CLASS (settings), &n_properties);

  for (i = 0; i < n_properties; i ++)
    {
      int len = strlen (props[i]->name);

      if (len > max_prop_name_length)
        max_prop_name_length = len;
    }


  for (i = 0; i < n_properties; i ++)
    {
      GValue      value = {0};
      GParamSpec *prop = props[i];
      char       *value_str;
      int         spacing = max_prop_name_length - strlen (prop->name) + 1;
      gboolean    deprecated;

      if (pattern && !g_strrstr (prop->name, pattern))
        continue;

      g_value_init (&value, prop->value_type);
      g_object_get_property (G_OBJECT (settings), prop->name, &value);
      deprecated = prop->flags & G_PARAM_DEPRECATED;

      if (G_VALUE_HOLDS_ENUM (&value))
        {
          GEnumClass *enum_class = G_PARAM_SPEC_ENUM (prop)->enum_class;
          GEnumValue *enum_value = g_enum_get_value (enum_class, g_value_get_enum (&value));

          value_str = g_strdup (enum_value->value_name);
        }
      else
        {
          value_str = g_strdup_value_contents (&value);
        }

      if (deprecated)
        {
          printf ("!");
          spacing --;
        }

      for (; spacing >= 0; spacing --)
        printf (" ");

      printf ("%s: %s\n", prop->name, value_str);

      g_free (value_str);
      g_value_unset (&value);
    }

  g_free (props);

  return 0;
}
