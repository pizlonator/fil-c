/*  Copyright 2023 Red Hat, Inc.
 *
 * GTK+ is free software; you can redistribute it and/or modify it
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
 * License along with GTK+; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen
 */

#include "config.h"

#include <gtk/gtk.h>
#include <gio/gio.h>
#ifdef G_OS_UNIX
#include <gio/gunixinputstream.h>
#endif
#include "gtk-path-tool.h"

#include <glib/gi18n-lib.h>

GskPath *
get_path (const char *arg)
{
  char *buffer = NULL;
  gsize len;
  GError *error = NULL;
  GskPath *path;

  if (arg[0] == '.' || arg[0] == '/')
    {
      if (!g_file_get_contents (arg, &buffer, &len, &error))
        {
          g_printerr ("%s\n", error->message);
          exit (1);
        }
    }
#ifdef G_OS_UNIX
  else if (strcmp (arg, "-") == 0)
    {
      GInputStream *in;
      GOutputStream *out;

      in = g_unix_input_stream_new (0, FALSE);
      out = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);

      if (g_output_stream_splice (out, in, 0, NULL, &error) < 0)
        {
          g_printerr (_("Failed to read from standard input: %s\n"), error->message);
          exit (1);
        }

      if (!g_output_stream_close (out, NULL, &error))
        {
          g_printerr (_("Error reading from standard input: %s\n"), error->message);
          exit (1);
        }

      buffer = g_memory_output_stream_steal_data (G_MEMORY_OUTPUT_STREAM (out));

      g_object_unref (out);
      g_object_unref (in);
    }
#endif
  else
    buffer = g_strdup (arg);

  g_strstrip (buffer);

  path = gsk_path_parse (buffer);

  if (path == NULL)
    {
      g_printerr (_("Failed to parse '%s' as path.\n"), arg);
      exit (1);
    }

  g_free (buffer);

  return path;
}

int
get_enum_value (GType       type,
                const char *type_nick,
                const char *str)
{
  GEnumClass *class = g_type_class_ref (type);
  GEnumValue *value;
  int val;

  value = g_enum_get_value_by_nick (class, str);
  if (value)
    val = value->value;
  else
    {
      GString *s;

      s = g_string_new ("");
      g_string_append_printf (s, _("Failed to parse '%s' as %s."), str, type_nick);
      g_string_append (s, "\n");
      g_string_append (s, _("Possible values: "));

      for (unsigned int i = 0; i < class->n_values; i++)
        {
          if (i > 0)
            g_string_append (s, ", ");
          g_string_append (s, class->values[i].value_nick);
        }
      g_printerr ("%s\n", s->str);
      g_string_free (s, TRUE);
      exit (1);
    }

  g_type_class_unref (class);

  return val;
}

void
get_color (GdkRGBA    *rgba,
           const char *str)
{
  if (!gdk_rgba_parse (rgba, str))
    {
      char *msg = g_strdup_printf (_("Could not parse '%s' as color"), str);
      g_printerr ("%s\n", msg);
      exit (1);
    }
}

void
_gsk_stroke_set_dashes (GskStroke  *stroke,
                        const char *dashes)
{
  GArray *d;
  char **strings;

  if (!dashes)
    return;

  d = g_array_new (FALSE, FALSE, sizeof (float));
  strings = g_strsplit (dashes, ",", 0);

  for (unsigned int i = 0; strings[i]; i++)
    {
      char *end = NULL;
      float f;

      f = (float) g_ascii_strtod (strings[i], &end);

      if (*end != '\0')
        {
          char *msg = g_strdup_printf (_("Failed to parse '%s' as number"), strings[i]);
          g_printerr ("%s\n", msg);
          exit (1);
        }

      g_array_append_val (d, f);
    }

  g_strfreev (strings);

  gsk_stroke_set_dash (stroke, (const float *)d->data, d->len);

  g_array_unref (d);
}
