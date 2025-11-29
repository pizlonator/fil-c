/* objects-finalize.c
 * Copyright (C) 2013 Openismus GmbH
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
 *
 * Authors: Tristan Van Berkom <tristanvb@openismus.com>
 */
#include <gtk/gtk.h>
#include <string.h>

#ifdef GDK_WINDOWING_X11
# include <gdk/x11/gdkx.h>
#endif


typedef GType (*GTypeGetFunc) (void);

static gboolean finalized = FALSE;

static gboolean
main_loop_quit_cb (gpointer data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);

  g_assert_true (finalized);
  return FALSE;
}

static void
check_finalized (gpointer data,
		 GObject *where_the_object_was)
{
  gboolean *did_finalize = (gboolean *)data;

  *did_finalize = TRUE;
}

static void
test_finalize_object (gconstpointer data)
{
  GType test_type = GPOINTER_TO_SIZE (data);
  GObject *object;
  gboolean done;

  if (g_str_equal (g_type_name (test_type), "GdkClipboard"))
    object = g_object_new (test_type, "display", gdk_display_get_default (), NULL);
  else if (g_str_equal (g_type_name (test_type), "GdkDrag") ||
           g_str_equal (g_type_name (test_type), "GdkDrop"))
    {
      GdkContentFormats *formats = gdk_content_formats_new_for_gtype (G_TYPE_STRING);
      object = g_object_new (test_type,
                             "device", gdk_seat_get_pointer (gdk_display_get_default_seat (gdk_display_get_default ())),
                             "formats", formats,
                             NULL);
      gdk_content_formats_unref (formats);
    }
  else if (g_type_is_a (test_type, GSK_TYPE_GL_SHADER))
    {
      GBytes *bytes = g_bytes_new_static ("", 0);
      object = g_object_new (test_type, "source", bytes, NULL);
      g_bytes_unref (bytes);
    }
  else if (g_type_is_a (test_type, GTK_TYPE_FILTER_LIST_MODEL) ||
           g_type_is_a (test_type, GTK_TYPE_NO_SELECTION) ||
           g_type_is_a (test_type, GTK_TYPE_SINGLE_SELECTION) ||
           g_type_is_a (test_type, GTK_TYPE_MULTI_SELECTION))
    {
      GListStore *list_store = g_list_store_new (G_TYPE_OBJECT);
      object = g_object_new (test_type,
                             "model", list_store,
                             NULL);
      g_object_unref (list_store);
    }
  else if (g_type_is_a (test_type, GTK_TYPE_LAYOUT_CHILD))
    {
#if 0
      // See https://github.com/mesonbuild/meson/issues/7515
      char *msg = g_strdup_printf ("Skipping %s", g_type_name (test_type));
      g_test_skip (msg);
      g_free (msg);
#endif
      return;
    }
  else
    object = g_object_new (test_type, NULL);
  g_assert_true (G_IS_OBJECT (object));

  /* Make sure we have the only reference */
  if (g_object_is_floating (object))
    g_object_ref_sink (object);

  /* Assert that the object finalizes properly */
  g_object_weak_ref (object, check_finalized, &finalized);

  /* Toplevels are owned by GTK, just tell GTK to destroy it */
  if (GTK_IS_WINDOW (object))
    gtk_window_destroy (GTK_WINDOW (object));
  else
    g_object_unref (object);

  /* Even if the object did finalize, it may have left some dangerous stuff in the GMainContext */
  done = FALSE;
  g_timeout_add (50, main_loop_quit_cb, &done);
  while (!done)
    g_main_context_iteration (NULL, TRUE);
}

static gboolean
dbind_warning_handler (const char     *log_domain,
                       GLogLevelFlags  log_level,
                       const char     *message,
                       gpointer        user_data)
{
  if (strcmp (log_domain, "dbind") == 0 &&
      log_level == (G_LOG_LEVEL_WARNING|G_LOG_FLAG_FATAL))
    return FALSE;

  return TRUE;
}

int
main (int argc, char **argv)
{
  const GType *all_types;
  guint n_types = 0, i;
  int result;
  const char *display, *x_r_d;

  /* These must be set before gtk_test_init */
  g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);

  /* g_test_dbus_up() helpfully clears these, so we have to re-set it */
  display = g_getenv ("DISPLAY");
  x_r_d = g_getenv ("XDG_RUNTIME_DIR");

  if (display)
    g_setenv ("DISPLAY", display, TRUE);
  if (x_r_d)
    g_setenv ("XDG_RUNTIME_DIR", x_r_d, TRUE);

  g_test_log_set_fatal_handler (dbind_warning_handler, NULL);

  /* initialize test program */
  gtk_test_init (&argc, &argv);
  gtk_test_register_all_types ();

  all_types = gtk_test_list_all_types (&n_types);

  for (i = 0; i < n_types; i++)
    {
      if (g_type_is_a (all_types[i], G_TYPE_OBJECT) &&
	  G_TYPE_IS_INSTANTIATABLE (all_types[i]) &&
	  !G_TYPE_IS_ABSTRACT (all_types[i]) &&
#ifdef GDK_WINDOWING_X11
	  all_types[i] != GDK_TYPE_X11_SURFACE &&
	  all_types[i] != GDK_TYPE_X11_SCREEN &&
	  all_types[i] != GDK_TYPE_X11_DISPLAY &&
	  all_types[i] != GDK_TYPE_X11_DEVICE_MANAGER_XI2 &&
	  all_types[i] != GDK_TYPE_X11_GL_CONTEXT &&
#endif
	  /* Not allowed to finalize a GdkPixbufLoader without calling gdk_pixbuf_loader_close() */
	  all_types[i] != GDK_TYPE_PIXBUF_LOADER &&
	  all_types[i] != gdk_pixbuf_simple_anim_iter_get_type() &&
          !g_type_is_a (all_types[i], GTK_TYPE_SHORTCUT_TRIGGER) &&
          !g_type_is_a (all_types[i], GTK_TYPE_SHORTCUT_ACTION) &&
          /* can't instantiate empty stack pages */
          all_types[i] != GTK_TYPE_STACK_PAGE)
	{
	  char *test_path = g_strdup_printf ("/FinalizeObject/%s", g_type_name (all_types[i]));

	  g_test_add_data_func (test_path, GSIZE_TO_POINTER (all_types[i]), test_finalize_object);

	  g_free (test_path);
	}
    }

  result = g_test_run();

  return result;
}
