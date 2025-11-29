/*
 * Copyright (C) 2019 Red Hat Inc.
 *
 * Author:
 *      Matthias Clasen <mclasen@redhat.com>
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

#include <string.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include "testsuite/testutils.h"

#ifdef G_OS_WIN32
# include <io.h>
#endif

struct {
  GtkDirectionType dir;
  const char *ext;
} extensions[] = {
  { GTK_DIR_TAB_FORWARD, "tab" },
  { GTK_DIR_TAB_BACKWARD, "tab-backward" },
  { GTK_DIR_UP, "up" },
  { GTK_DIR_DOWN, "down" },
  { GTK_DIR_LEFT, "left" },
  { GTK_DIR_RIGHT, "right" }
};

static void
check_focus_states (GtkWidget *focus_widget)
{
  GtkStateFlags state;
  GtkWidget *parent;

  if (focus_widget == NULL)
    return;

  /* Check that we set :focus and :focus-within on the focus_widget,
   * and :focus-within on its ancestors
   */

  state = gtk_widget_get_state_flags (focus_widget);
  g_assert_true ((state & (GTK_STATE_FLAG_FOCUSED|GTK_STATE_FLAG_FOCUS_WITHIN)) ==
                  (GTK_STATE_FLAG_FOCUSED|GTK_STATE_FLAG_FOCUS_WITHIN));

  parent = gtk_widget_get_parent (focus_widget);
  while (parent)
    {
      state = gtk_widget_get_state_flags (parent);
      g_assert_true ((state & GTK_STATE_FLAG_FOCUS_WITHIN) == GTK_STATE_FLAG_FOCUS_WITHIN);
      g_assert_true ((state & GTK_STATE_FLAG_FOCUSED) == 0);

      parent = gtk_widget_get_parent (parent);
    }
}

static gboolean
quit_iteration_loop (gpointer user_data)
{
  gboolean *keep_running = user_data;

  *keep_running = FALSE;

  return G_SOURCE_REMOVE;
}

static void
timed_loop (guint millis)
{
  gboolean keep_running = TRUE;

  g_timeout_add (millis, quit_iteration_loop, &keep_running);
  while (keep_running)
    g_main_context_iteration (NULL, TRUE);
}

static char *
generate_focus_chain (GtkWidget        *window,
                      GtkDirectionType  dir)
{
  char *first = NULL;
  char *last = NULL;
  char *name = NULL;
  char *key = NULL;
  GString *output = g_string_new ("");
  GtkWidget *focus;
  int count = 0;

  gtk_window_present (GTK_WINDOW (window));

  /* start without focus */
  gtk_window_set_focus (GTK_WINDOW (window), NULL);

  while (TRUE)
    {
      g_signal_emit_by_name (window, "move-focus", dir);

      focus = gtk_window_get_focus (GTK_WINDOW (window));

      check_focus_states (focus);

      if (focus)
        {
          /* ui files can't put a name on the embedded text,
           * so include the parent entry here
           */
          if (GTK_IS_TEXT (focus))
            name = g_strdup_printf ("%s %s",
                                    gtk_widget_get_name (gtk_widget_get_parent (focus)),
                                    gtk_widget_get_name (focus));
          else
            name = g_strdup (gtk_widget_get_name (focus));

          key = g_strdup_printf ("%s %p", name, focus);
        }
      else
        {
          name = g_strdup ("NONE");
          key = g_strdup (key);
        }

      if (first && g_str_equal (key, first))
        {
          g_string_append (output, "WRAP\n");
          break; /* cycle completed */
        }

      if (last && g_str_equal (key, last))
        {
          g_string_append (output, "STOP\n");
          break; /* dead end */
        }

      g_string_append_printf (output, "%s\n", name);
      timed_loop (100);
      count++;

      if (!first)
        first = g_strdup (key);

      g_free (last);
      last = key;

      if (count == 100)
        {
          g_string_append (output, "ABORT\n");
          break;
        }

      g_free (name);
    }

  g_free (name);
  g_free (key);
  g_free (first);
  g_free (last);

  return g_string_free (output, FALSE);
}

static GtkDirectionType
get_dir_for_file (const char *path)
{
  char *p;
  int i;

  p = strrchr (path, '.');
  p++;

  for (i = 0; i < G_N_ELEMENTS (extensions); i++)
    {
      if (strcmp (p, extensions[i].ext) == 0)
        return extensions[i].dir;
    }

  g_error ("Could not find direction for %s", path);

  return 0;
}

static gboolean
load_ui_file (GFile *ui_file,
              GFile *ref_file,
              const char *ext)
{
  GtkBuilder *builder;
  GtkWidget *window;
  char *output;
  char *diff;
  char *ui_path, *ref_path;
  GError *error = NULL;
  GtkDirectionType dir;
  gboolean success = FALSE;
  gboolean keep_running = TRUE;
  guint timeout_handle_id;

  ui_path = g_file_get_path (ui_file);

  builder = gtk_builder_new_from_file (ui_path);
  window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));

  g_assert_nonnull (window);

  gtk_window_present (GTK_WINDOW (window));

  timeout_handle_id = g_timeout_add (2000,
                                     quit_iteration_loop,
                                     &keep_running);
  while (keep_running && !gtk_window_is_active (GTK_WINDOW (window)))
    g_main_context_iteration (NULL, TRUE);

  if (keep_running)
    g_source_remove (timeout_handle_id);

  if (!gtk_window_is_active (GTK_WINDOW (window)))
    {
      g_print ("Skipping focus tests because window did not get focus. Headless display?");
      exit (77);
    }

  if (ext)
    {
      int i;

      for (i = 0; i < G_N_ELEMENTS (extensions); i++)
        {
          if (g_str_equal (ext, extensions[i].ext))
            {
              output = generate_focus_chain (window, extensions[i].dir);
              g_print ("%s", output);
              success = TRUE;
              goto out;
            }
        }

      g_error ("Not a supported direction: %s", ext);
      goto out;
    }

  g_assert_nonnull (ref_file);

  ref_path = g_file_get_path (ref_file);

  dir = get_dir_for_file (ref_path);
  output = generate_focus_chain (window, dir);
  diff = diff_with_file (ref_path, output, -1, &error);
  g_assert_no_error (error);

  if (diff && diff[0])
    g_print ("Resulting output doesn't match reference:\n%s", diff);
  else
     success = TRUE;
  g_free (ref_path);
  g_free (diff);

out:
  g_free (output);
  g_free (ui_path);

  return success;
}

static const char *arg_generate;

static const GOptionEntry options[] = {
  { "generate", 0, 0, G_OPTION_ARG_STRING, &arg_generate, "DIRECTION" },
  { NULL }
};

int
main (int argc, char **argv)
{
  GOptionContext *context;
  GFile *ui_file;
  GFile *ref_file;
  GError *error = NULL;
  gboolean success;

  context = g_option_context_new ("- run focus chain tests");
  g_option_context_add_main_entries (context, options, NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_error ("Option parsing failed: %s\n", error->message);
      return 1;
    }
  g_option_context_free (context);

  gtk_init ();

  if (arg_generate)
    {
      g_assert_cmpint (argc, ==, 2);

      ui_file = g_file_new_for_commandline_arg (argv[1]);

      success = load_ui_file (ui_file, NULL, arg_generate);

      g_object_unref (ui_file);
    }
  else
    {
      g_assert_cmpint (argc, ==, 3);

      ui_file = g_file_new_for_commandline_arg (argv[1]);
      ref_file = g_file_new_for_commandline_arg (argv[2]);

      success = load_ui_file (ui_file, ref_file, NULL);

      g_object_unref (ui_file);
      g_object_unref (ref_file);
    }

  return success ? 0 : 1;
}

