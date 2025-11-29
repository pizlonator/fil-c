/*
 * Copyright (C) 2011 Red Hat Inc.
 *
 * Author:
 *      Benjamin Otte <otte@redhat.com>
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
 */

#include "config.h"

#include <gtk/gtk.h>

static char *
test_get_reference_file (const char *node_file)
{
  GString *file = g_string_new (NULL);

  if (g_str_has_suffix (node_file, ".node"))
    g_string_append_len (file, node_file, strlen (node_file) - 5);
  else
    g_string_append (file, node_file);
  
  g_string_append (file, ".ref.node");

  if (!g_file_test (file->str, G_FILE_TEST_EXISTS))
    {
      g_string_free (file, TRUE);
      return g_strdup (node_file);
    }

  return g_string_free (file, FALSE);
}

static char *
test_get_errors_file (const char *node_file)
{
  GString *file = g_string_new (NULL);

  if (g_str_has_suffix (node_file, ".node"))
    g_string_append_len (file, node_file, strlen (node_file) - 5);
  else
    g_string_append (file, node_file);
  
  g_string_append (file, ".errors");

  if (!g_file_test (file->str, G_FILE_TEST_EXISTS))
    {
      g_string_free (file, TRUE);
      return NULL;
    }

  return g_string_free (file, FALSE);
}

static GBytes *
diff_with_file (const char  *file1,
                GBytes      *input,
                GError     **error)
{
  GSubprocess *process;
  GBytes *output;

  process = g_subprocess_new (G_SUBPROCESS_FLAGS_STDIN_PIPE
                              | G_SUBPROCESS_FLAGS_STDOUT_PIPE,
                              error,
                              "diff", "-u", file1, "-", NULL);
  if (process == NULL)
    return NULL;

  if (!g_subprocess_communicate (process,
                                 input,
                                 NULL,
                                 &output,
                                 NULL,
                                 error))
    {
      g_object_unref (process);
      return NULL;
    }

  if (!g_subprocess_get_successful (process) &&
      /* this is the condition when the files differ */
      !(g_subprocess_get_if_exited (process) && g_subprocess_get_exit_status (process) == 1))
    {
      g_clear_pointer (&output, g_bytes_unref);
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "The `diff' process exited with error status %d",
                   g_subprocess_get_exit_status (process));
    }

  g_object_unref (process);

  return output;
}

static void
append_error_value (GString *string,
                    GType    enum_type,
                    guint    value)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  enum_class = g_type_class_ref (enum_type);
  enum_value = g_enum_get_value (enum_class, value);

  g_string_append (string, enum_value->value_name);

  g_type_class_unref (enum_class);
}

static void
deserialize_error_func (const GskParseLocation *start,
                        const GskParseLocation *end,
                        const GError           *error,
                        gpointer                user_data)
{
  GString *errors = user_data;
  GString *string = g_string_new ("<data>");

  g_string_append_printf (string, ":%zu:%zu",
                          start->lines + 1, start->line_chars + 1);
  if (start->lines != end->lines || start->line_chars != end->line_chars)
    {
      g_string_append (string, "-");
      if (start->lines != end->lines)
        g_string_append_printf (string, "%zu:", end->lines + 1);
      g_string_append_printf (string, "%zu", end->line_chars + 1);
    }

  g_string_append_printf (errors, "%s: error: ", string->str);
  g_string_free (string, TRUE);

  if (error->domain == GTK_CSS_PARSER_ERROR)
    append_error_value (errors, GTK_TYPE_CSS_PARSER_ERROR, error->code);
  else if (error->domain == GTK_CSS_PARSER_WARNING)
    append_error_value (errors, GTK_TYPE_CSS_PARSER_WARNING, error->code);
  else
    g_string_append_printf (errors, 
                            "%s %u",
                            g_quark_to_string (error->domain),
                            error->code);

  g_string_append_c (errors, '\n');
}

static gboolean
parse_node_file (GFile *file, gboolean generate)
{
  char *node_file, *reference_file, *errors_file;
  GskRenderNode *node;
  GString *errors;
  GBytes *diff, *bytes;
  GError *error = NULL;
  gboolean result = TRUE;

  bytes = g_file_load_bytes (file, NULL, NULL, &error);
  if (error)
    {
      g_print ("Error loading file: %s\n", error->message);
      g_clear_error (&error);
      return FALSE;
    }
  g_assert_nonnull (bytes);

  errors = g_string_new ("");

  node = gsk_render_node_deserialize (bytes, deserialize_error_func, errors);
  g_bytes_unref (bytes);
  bytes = gsk_render_node_serialize (node);
  gsk_render_node_unref (node);

  if (generate)
    {
      g_print ("%s", (char *) g_bytes_get_data (bytes, NULL));
      g_bytes_unref (bytes);
      g_string_free (errors, TRUE);
      return TRUE;
    }

  node_file = g_file_get_path (file);
  reference_file = test_get_reference_file (node_file);

  diff = diff_with_file (reference_file, bytes, &error);
  g_assert_no_error (error);

  if (diff && g_bytes_get_size (diff) > 0)
    {
      g_print ("Resulting file doesn't match reference:\n%s\n",
               (const char *) g_bytes_get_data (diff, NULL));
      result = FALSE;
    }
  g_free (reference_file);
  g_clear_pointer (&diff, g_bytes_unref);

  errors_file = test_get_errors_file (node_file);

  if (errors_file)
    {
      GBytes *error_bytes = g_string_free_to_bytes (errors);
      diff = diff_with_file (errors_file, error_bytes, &error);
      g_assert_no_error (error);

      if (diff && g_bytes_get_size (diff) > 0)
        {
          g_print ("Errors don't match expected errors:\n%s\n",
                   (const char *) g_bytes_get_data (diff, NULL));
          result = FALSE;
        }
      g_clear_pointer (&diff, g_bytes_unref);
      g_clear_pointer (&error_bytes, g_bytes_unref);
    }
  else if (errors->str[0])
    {
      g_print ("Unexpected errors:\n%s\n", errors->str);
      result = FALSE;
      g_string_free (errors, TRUE);
    }
  else
    {
      g_string_free (errors, TRUE);
    }

  g_free (errors_file);
  g_free (node_file);
  g_bytes_unref (bytes);

  return result;
}

static gboolean
test_file (GFile *file)
{
  return parse_node_file (file, FALSE);
}

static int
compare_files (gconstpointer a, gconstpointer b)
{
  GFile *file1 = G_FILE (a);
  GFile *file2 = G_FILE (b);
  char *path1, *path2;
  int result;

  path1 = g_file_get_path (file1);
  path2 = g_file_get_path (file2);

  result = strcmp (path1, path2);

  g_free (path1);
  g_free (path2);

  return result;
}

static gboolean
test_files_in_directory (GFile *dir)
{
  GFileEnumerator *enumerator;
  GFileInfo *info;
  GList *l, *files;
  GError *error = NULL;
  gboolean result = TRUE;

  enumerator = g_file_enumerate_children (dir, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, &error);
  g_assert_no_error (error);
  files = NULL;

  while ((info = g_file_enumerator_next_file (enumerator, NULL, &error)))
    {
      const char *filename;

      filename = g_file_info_get_name (info);

      if (!g_str_has_suffix (filename, ".node") ||
          g_str_has_suffix (filename, ".out.node") ||
          g_str_has_suffix (filename, ".ref.node"))
        {
          g_object_unref (info);
          continue;
        }

      files = g_list_prepend (files, g_file_get_child (dir, filename));

      g_object_unref (info);
    }
  
  g_assert_no_error (error);
  g_object_unref (enumerator);

  files = g_list_sort (files, compare_files);
  for (l = files; l; l = l->next)
    {
      result &= test_file (l->data);
    }
  g_list_free_full (files, g_object_unref);

  return result;
}

int
main (int argc, char **argv)
{
  gboolean success;

  if (argc < 2)
    {
      const char *basedir;
      GFile *dir;

      gtk_test_init (&argc, &argv);

      basedir = g_test_get_dir (G_TEST_DIST);
      dir = g_file_new_for_path (basedir);
      success = test_files_in_directory (dir);

      g_object_unref (dir);
    }
  else if (strcmp (argv[1], "--generate") == 0)
    {
      if (argc >= 3)
        {
          GFile *file = g_file_new_for_commandline_arg (argv[2]);

          gtk_init ();

          success = parse_node_file (file, TRUE);

          g_object_unref (file);
        }
      else
        success = FALSE;
    }
  else
    {
      guint i;

      gtk_test_init (&argc, &argv);

      success = TRUE;

      for (i = 1; i < argc; i++)
        {
          GFile *file = g_file_new_for_commandline_arg (argv[i]);

          success &= test_file (file);

          g_object_unref (file);
        }
    }

  return success ? 0 : 1;
}

