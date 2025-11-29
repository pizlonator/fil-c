/*
 * Copyright (C) 2011 Red Hat Inc.
 *
 * Author:
 *      Benjamin Otte <otte@gnome.org>
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

#include "reftest-compare.h"
#include "reftest-module.h"
#include "reftest-snapshot.h"

#ifndef G_OS_WIN32
#include <execinfo.h>
#endif
#include <string.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#ifdef G_OS_WIN32
# include <direct.h>
#endif

typedef enum {
  SNAPSHOT_WINDOW,
  SNAPSHOT_DRAW
} SnapshotMode;

/* This is exactly the style information you've been looking for */
#define GTK_STYLE_PROVIDER_PRIORITY_FORCE G_MAXUINT

static char *arg_output_dir = NULL;
static char *arg_base_dir = NULL;
static char *arg_direction = NULL;
static char *arg_compare_dir = NULL;

static const GOptionEntry test_args[] = {
  { "output",         'o', 0, G_OPTION_ARG_FILENAME, &arg_output_dir,
    "Directory to save image files to", "DIR" },
  { "directory",        'd', 0, G_OPTION_ARG_FILENAME, &arg_base_dir,
    "Directory to run tests from", "DIR" },
  { "direction",       0, 0, G_OPTION_ARG_STRING, &arg_direction,
    "Set text direction", "ltr|rtl" },
  { "compare-with",    0, 0, G_OPTION_ARG_FILENAME, &arg_compare_dir,
    "Directory to compare with", "DIR" },
  { NULL }
};

static gboolean using_tap;

/* A simplified version of g_log_writer_default_would_drop(), to avoid
 * bumping up the required version of GLib to 2.68
 */
static gboolean
would_drop (GLogLevelFlags  level,
            const char     *domain)
{
  return g_log_writer_default_would_drop (level, domain);
}

static gboolean
parse_command_line (int *argc, char ***argv)
{
  GError *error = NULL;
  GOptionContext *context;
  int i;

  context = g_option_context_new ("- run GTK reftests");
  g_option_context_add_main_entries (context, test_args, NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);

  if (!g_option_context_parse (context, argc, argv, &error))
    {
      g_print ("option parsing failed: %s\n", error->message);
      return FALSE;
    }
  g_option_context_free (context);

  for (i = 0; i < *argc; i++)
    {
      if (strcmp ((*argv)[i], "--tap") == 0)
        using_tap = TRUE;
    }

  gtk_test_init (argc, argv);

  if (g_strcmp0 (arg_direction, "rtl") == 0)
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);
  else if (g_strcmp0 (arg_direction, "ltr") == 0)
    gtk_widget_set_default_direction (GTK_TEXT_DIR_LTR);
  else if (arg_direction != NULL)
    g_printerr ("Invalid argument passed to --direction argument. Valid arguments are 'ltr' and 'rtl'\n");

  return TRUE;
}

static const char *
get_output_dir (GError **error)
{
  static const char *output_dir = NULL;

  if (output_dir)
    return output_dir;

  if (arg_output_dir)
    {
      GError *err = NULL;
      GFile *file;
      const char *subdir;

      file = g_file_new_for_commandline_arg (arg_output_dir);

      subdir = g_getenv ("TEST_OUTPUT_SUBDIR");
      if (subdir)
        {
          GFile *child = g_file_get_child (file, subdir);
          g_object_unref (file);
          file = child;
        }

      if (!g_file_make_directory_with_parents (file, NULL, &err))
        {
          if (!g_error_matches (err, G_IO_ERROR, G_IO_ERROR_EXISTS))
            {
              g_propagate_error (error, err);
              g_object_unref (file);
              return NULL;
            }
          g_clear_error (&err);
        }

      output_dir = g_file_get_path (file);
      g_object_unref (file);
    }
  else
    {
      output_dir = g_get_tmp_dir ();
    }

  return output_dir;
}

static void
get_components_of_test_file (const char  *test_file,
                             char       **directory,
                             char       **basename)
{
  if (directory)
    {
      *directory = g_path_get_dirname (test_file);
    }

  if (basename)
    {
      char *base = g_path_get_basename (test_file);

      if (g_str_has_suffix (base, ".ui"))
        base[strlen (base) - strlen (".ui")] = '\0';

      *basename = base;
    }
}

static char *
get_output_file (const char  *test_file,
                 const char  *extension,
                 GError     **error)
{
  const char *output_dir;
  char *result, *base;

  output_dir = get_output_dir (error);
  if (output_dir == NULL)
    return NULL;

  get_components_of_test_file (test_file, NULL, &base);

  result = g_strconcat (output_dir, G_DIR_SEPARATOR_S, base, extension, NULL);
  g_free (base);

  return result;
}

static char *
get_test_file (const char *test_file,
               const char *extension,
               gboolean    must_exist)
{
  GString *file = g_string_new (NULL);
  char *dir, *base;

  get_components_of_test_file (test_file, &dir, &base);

  g_string_append (file, dir);
  g_string_append (file, G_DIR_SEPARATOR_S);
  g_string_append (file, base);
  g_string_append (file, extension);

  g_free (dir);
  g_free (base);

  if (must_exist &&
      !g_file_test (file->str, G_FILE_TEST_EXISTS))
    {
      g_string_free (file, TRUE);
      return NULL;
    }

  return g_string_free (file, FALSE);
}

static char *
get_reference_image (const char *ui_file)
{
  char *base;
  char *reference_image;

  if (!arg_compare_dir)
    return NULL;

  get_components_of_test_file (ui_file, NULL, &base);
  reference_image = g_strconcat (arg_compare_dir, G_DIR_SEPARATOR_S, base, ".out.png", NULL);
  g_free (base);

  if (!g_file_test (reference_image, G_FILE_TEST_EXISTS))
    {
      g_free (reference_image);
      return NULL;
    }

  return reference_image;
}

static GtkStyleProvider *
add_extra_css (const char *testname,
               const char *extension)
{
  GtkStyleProvider *provider = NULL;
  char *css_file;

  css_file = get_test_file (testname, extension, TRUE);
  if (css_file == NULL)
    return NULL;

  provider = GTK_STYLE_PROVIDER (gtk_css_provider_new ());
  gtk_css_provider_load_from_path (GTK_CSS_PROVIDER (provider),
                                   css_file);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              provider,
                                              GTK_STYLE_PROVIDER_PRIORITY_FORCE);

  g_free (css_file);

  return provider;
}

static void
remove_extra_css (GtkStyleProvider *provider)
{
  if (provider == NULL)
    return;

  gtk_style_context_remove_provider_for_display (gdk_display_get_default (),
                                                 provider);
}

static void
save_image (GdkTexture *texture,
            const char *test_name,
            const char *extension)
{
  GError *error = NULL;
  char *filename;
  gboolean ret;

  filename = get_output_file (test_name, extension, &error);
  if (filename == NULL)
    {
      g_test_message ("Not storing test result image: %s", error->message);
      g_error_free (error);
      return;
    }

  g_test_message ("Storing test result image at %s", filename);
  ret = gdk_texture_save_to_png (texture, filename);
  g_assert_true (ret);

  g_free (filename);
}

static void
save_node (GskRenderNode *node,
           const char    *test_name,
           const char    *extension)
{
  GError *error = NULL;
  char *filename;
  gboolean ret;
  GBytes *bytes;

  filename = get_output_file (test_name, extension, &error);
  if (filename == NULL)
    {
      g_test_message ("Not storing test result node: %s", error->message);
      g_error_free (error);
      return;
    }

  g_test_message ("Storing test result node at %s", filename);
  if (node)
    bytes = gsk_render_node_serialize (node);
  else
    bytes = g_bytes_new ("", 0);
  ret = g_file_set_contents (filename,
                             g_bytes_get_data (bytes, NULL),
                             g_bytes_get_size (bytes),
                             NULL);
  g_assert_true (ret);

  g_bytes_unref (bytes);
  g_free (filename);
}

static void
test_ui_file (GFile *file)
{
  char *ui_file, *reference_file;
  GdkTexture *ui_image, *reference_image, *diff_image;
  GtkStyleProvider *provider;

  ui_file = g_file_get_path (file);

  provider = add_extra_css (ui_file, ".css");

  ui_image = reftest_snapshot_ui_file (ui_file);

  if ((reference_file = get_reference_image (ui_file)) != NULL)
    {
      GError *error = NULL;

      reference_image = gdk_texture_new_from_filename (reference_file, &error);
      if (reference_image == NULL)
        {
          g_test_message ("Failed to load reference image: %s", error->message);
          g_clear_error (&error);
          g_test_fail ();
        }
    }
  else if ((reference_file = get_test_file (ui_file, ".ref.ui", TRUE)) != NULL)
    reference_image = reftest_snapshot_ui_file (reference_file);
  else
    {
      reference_image = NULL;
      g_test_message ("No reference image.");
      g_test_fail ();
    }
  g_free (reference_file);
  if (reference_image == NULL)
    reference_image = gdk_memory_texture_new (1, 1, GDK_MEMORY_DEFAULT, g_bytes_new ((guchar[4]) {0, 0, 0, 0}, 4), 4);

  diff_image = reftest_compare_textures (ui_image, reference_image);

  save_image (ui_image, ui_file, ".out.png");
  save_image (reference_image, ui_file, ".ref.png");
  if (diff_image)
    {
      save_node (g_object_get_data (G_OBJECT (ui_image), "source-render-node"), ui_file, ".out.node");
      save_node (g_object_get_data (G_OBJECT (reference_image), "source-render-node"), ui_file, ".ref.node");
      save_image (diff_image, ui_file, ".diff.png");
      g_object_unref (diff_image);
      g_test_fail ();
    }

  remove_extra_css (provider);

  g_free (ui_file);

  g_clear_object (&ui_image);
  g_clear_object (&reference_image);
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

static void
add_test_for_file (GFile *file)
{
  GFileEnumerator *enumerator;
  GFileInfo *info;
  GList *files;
  GError *error = NULL;


  if (g_file_query_file_type (file, 0, NULL) != G_FILE_TYPE_DIRECTORY)
    {
      g_test_add_vtable (g_file_peek_path (file),
                         0,
                         g_object_ref (file),
                         NULL,
                         (GTestFixtureFunc) test_ui_file,
                         (GTestFixtureFunc) g_object_unref);
      return;
    }


  enumerator = g_file_enumerate_children (file, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, &error);
  g_assert_no_error (error);
  files = NULL;

  while ((info = g_file_enumerator_next_file (enumerator, NULL, &error)))
    {
      const char *filename;

      filename = g_file_info_get_name (info);

      if (!g_str_has_suffix (filename, ".ui") ||
          g_str_has_suffix (filename, ".ref.ui"))
        {
          g_object_unref (info);
          continue;
        }

      files = g_list_prepend (files, g_file_get_child (file, filename));

      g_object_unref (info);
    }

  g_assert_no_error (error);
  g_object_unref (enumerator);

  files = g_list_sort (files, compare_files);
  g_list_foreach (files, (GFunc) add_test_for_file, NULL);
  g_list_free_full (files, g_object_unref);
}

static GLogWriterOutput
log_writer (GLogLevelFlags   log_level,
            const GLogField *fields,
            gsize            n_fields,
            gpointer         user_data)
{
#ifndef G_OS_WIN32
  if (log_level & G_LOG_LEVEL_CRITICAL)
    {
      void *buffer[1024];
      int size, i;
      char **symbols;
      GString *s;
      GLogField *my_fields;

      my_fields = g_alloca (sizeof (GLogField) * n_fields);

      s = g_string_new ("");

      size = backtrace (buffer, 1024);
      symbols = backtrace_symbols (buffer, size);
      for (i = 0; i < size; i++)
        {
          g_string_append (s, symbols[i]);
          g_string_append_c (s, '\n');
        }
      free (symbols);

      for (i = 0; i < n_fields; i++)
        {
          my_fields[i] = fields[i];

          if (strcmp (fields[i].key, "MESSAGE") == 0)
            {
              my_fields[i].value = g_strconcat (fields[i].value, "\nBacktrace:\n", s->str, NULL);
              my_fields[i].length = strlen (my_fields[i].value);
            }
        }
      g_string_free (s, TRUE);

      fields = my_fields;
    }
#endif

  if (!would_drop (log_level, NULL))
    return g_log_writer_standard_streams (log_level, fields, n_fields, user_data);

  return G_LOG_WRITER_HANDLED;
}

int
main (int argc, char **argv)
{
  const char *basedir;
  int result;

  if (!parse_command_line (&argc, &argv))
    return 1;

  /* Override some settings that otherwise might affect
   * the reliability of our output.
   */
  g_object_set (gtk_settings_get_default (),
                "gtk-cursor-blink", FALSE,
                NULL);

  if (arg_base_dir)
    basedir = arg_base_dir;
  else
    basedir = g_test_get_dir (G_TEST_DIST);

  if (argc < 2)
    {
      GFile *dir;

      dir = g_file_new_for_path (basedir);

      add_test_for_file (dir);

      g_object_unref (dir);
    }
  else
    {
      guint i;

      for (i = 1; i < argc; i++)
        {
          GFile *file = g_file_new_for_commandline_arg (argv[i]);

          add_test_for_file (file);

          g_object_unref (file);
        }
    }

  /* We need to ensure the process' current working directory
   * is the same as the reftest data, because we're using the
   * "file" property of GtkImage as a relative path in builder files.
   *
   * The g_assert() is needed to ensure GNU libc does not complain
   * about the unused return value, and the G_GNUC_UNUSED is needed
   * to avoid compiler warnings when g_assert() is compiled out
   * during the release build.
   */
  int res G_GNUC_UNUSED;
  res = chdir (basedir);
  g_assert (res == 0);

  g_log_set_writer_func (log_writer, NULL, NULL);

  result = g_test_run ();

  if (using_tap)
    return 0;

  return result;
}

