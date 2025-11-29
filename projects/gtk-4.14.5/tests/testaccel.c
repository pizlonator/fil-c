/* gtkcellrendereraccel.h
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
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

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static void
accel_edited_callback (GtkCellRendererText *cell,
                       const char          *path_string,
                       guint                keyval,
                       GdkModifierType      mask,
                       guint                hardware_keycode,
                       gpointer             data)
{
  GtkTreeModel *model = (GtkTreeModel *)data;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  GtkTreeIter iter;

  gtk_tree_model_get_iter (model, &iter, path);

  g_print ("%u %d %u\n", keyval, mask, hardware_keycode);
  
  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                      0, (int)mask,
                      1, keyval,
                      2, hardware_keycode,
                      -1);
  gtk_tree_path_free (path);
}

static void
accel_cleared_callback (GtkCellRendererText *cell,
                        const char          *path_string,
                        gpointer             data)
{
  GtkTreeModel *model = (GtkTreeModel *)data;
  GtkTreePath *path;
  GtkTreeIter iter;

  path = gtk_tree_path_new_from_string (path_string);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, 0, 1, 0, 2, 0, -1);
  gtk_tree_path_free (path);
}
static GtkWidget *
key_test (void)
{
  GtkWidget *window, *sw, *tv;
  GtkListStore *store;
  GtkTreeViewColumn *column;
  GtkCellRenderer *rend;
  int i;
  GtkWidget *box, *entry;

  /* create window */
  window = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 400);

  sw = gtk_scrolled_window_new ();
  gtk_widget_set_vexpand (sw, TRUE);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_window_set_child (GTK_WINDOW (window), box);
  gtk_box_append (GTK_BOX (box), sw);

  store = gtk_list_store_new (3, G_TYPE_INT, G_TYPE_UINT, G_TYPE_UINT);
  tv = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), tv);
  column = gtk_tree_view_column_new ();
  rend = gtk_cell_renderer_accel_new ();
  g_object_set (G_OBJECT (rend),
                "accel-mode", GTK_CELL_RENDERER_ACCEL_MODE_GTK,
                "editable", TRUE,
                NULL);
  g_signal_connect (G_OBJECT (rend),
                    "accel-edited",
                    G_CALLBACK (accel_edited_callback),
                    store);
  g_signal_connect (G_OBJECT (rend),
                    "accel-cleared",
                    G_CALLBACK (accel_cleared_callback),
                    store);

  gtk_tree_view_column_pack_start (column, rend,
                                   TRUE);
  gtk_tree_view_column_set_attributes (column, rend,
                                      "accel-mods", 0,
                                      "accel-key", 1,
                                      "keycode", 2,
                                      NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tv), column);

  for (i = 0; i < 10; i++)
    {
      GtkTreeIter iter;

      gtk_list_store_append (store, &iter);
    }

  entry = gtk_entry_new ();
  gtk_box_append (GTK_BOX (box), entry);

  return window;
}

int
main (int argc, char **argv)
{
  GtkWidget *dialog;
  
  gtk_init ();

  dialog = key_test ();

  gtk_window_present (GTK_WINDOW (dialog));

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
