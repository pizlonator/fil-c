/*
 * builderparser.c: Test GtkBuilder parser
 *
 * Copyright (C) 2014 Red Hat, Inc
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <string.h>
#include <gtk/gtk.h>

static void
test_file (const char *filename, GString *string)
{
  char *contents;
  gsize  length;
  GError *error = NULL;
  gboolean ret;
  GtkBuilder *builder;

  if (!g_file_get_contents (filename, &contents, &length, &error))
    {
      fprintf (stderr, "%s\n", error->message);
      g_error_free (error);
      return;
    }

  builder = gtk_builder_new ();
  ret = gtk_builder_add_from_string (builder, contents, length, &error);
  g_free (contents);

  if (ret)
    {
      g_assert_no_error (error);
      g_string_append_printf (string, "SUCCESS\n");
    }
  else
    {
      g_string_append_printf (string, "ERROR: %s %d\n%s\n",
                              g_quark_to_string (error->domain),
                              error->code,
                              error->message);
      g_error_free (error);
    }

  g_object_unref (builder);
}

static char *
get_expected_filename (const char *filename)
{
  char *f, *p, *expected;

  f = g_strdup (filename);
  p = strstr (f, ".ui");
  if (p)
    *p = 0;
  expected = g_strconcat (f, ".expected", NULL);

  g_free (f);

  return expected;
}

static void
test_parse (gconstpointer d)
{
  const char *filename = d;
  char *expected_file;
  char *expected;
  GError *error = NULL;
  GString *string;

  g_test_message ("filename: %s", filename);
  expected_file = get_expected_filename (filename);

  string = g_string_sized_new (0);

  test_file (filename, string);

  g_file_get_contents (expected_file, &expected, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (string->str, ==, expected);
  g_free (expected);

  g_string_free (string, TRUE);

  g_free (expected_file);
}

int
main (int argc, char *argv[])
{
  GDir *dir;
  GError *error = NULL;
  const char *name;
  char *path;

  gtk_test_init (&argc, &argv, NULL);

  /* allow to easily generate expected output for new test cases */
  if (argc > 1)
    {
      GString *string;

      string = g_string_sized_new (0);
      test_file (argv[1], string);
      g_print ("%s", string->str);

      return 0;
    }

  path = g_test_build_filename (G_TEST_DIST, "ui", NULL);
  dir = g_dir_open (path, 0, &error);
  g_free (path);
  g_assert_no_error (error);
  while ((name = g_dir_read_name (dir)) != NULL)
    {
      if (!g_str_has_suffix (name, ".ui"))
        continue;

      path = g_strdup_printf ("/builder/parse/%s", name);
      g_test_add_data_func_full (path, g_test_build_filename (G_TEST_DIST, "ui", name, NULL),
                                 test_parse, g_free);
      g_free (path);
    }
  g_dir_close (dir);

  return g_test_run ();
}

