/* testtreeflow.c
 * Copyright (C) 2001 Red Hat, Inc
 * Author: Jonathan Blandford
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

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

GtkTreeModel *model = NULL;
static GRand *grand = NULL;
GtkTreeSelection *selection = NULL;
enum
{
  TEXT_COLUMN,
  NUM_COLUMNS
};

static const char *words[] =
{
  "Boom",
  "Borp",
  "Multiline\ntext",
  "Bingo",
  "Veni\nVedi\nVici",
  NULL
};


#define NUM_WORDS 5
#define NUM_ROWS 100


static void
initialize_model (void)
{
  int i;
  GtkTreeIter iter;

  model = (GtkTreeModel *) gtk_list_store_new (NUM_COLUMNS, G_TYPE_STRING);
  grand = g_rand_new ();
  for (i = 0; i < NUM_ROWS; i++)
    {
      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			  TEXT_COLUMN, words[g_rand_int_range (grand, 0, NUM_WORDS)],
			  -1);
    }
}

static void
futz_row (void)
{
  int i;
  GtkTreePath *path;
  GtkTreeIter iter;
  GtkTreeIter iter2;

  i = g_rand_int_range (grand, 0,
			gtk_tree_model_iter_n_children (model, NULL));
  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, i);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_path_free (path);

  if (gtk_tree_selection_iter_is_selected (selection, &iter))
    return;
  switch (g_rand_int_range (grand, 0, 3))
    {
    case 0:
      /* insert */
            gtk_list_store_insert_after (GTK_LIST_STORE (model),
            				   &iter2, &iter);
            gtk_list_store_set (GTK_LIST_STORE (model), &iter2,
            			  TEXT_COLUMN, words[g_rand_int_range (grand, 0, NUM_WORDS)],
            			  -1);
      break;
    case 1:
      /* delete */
      if (gtk_tree_model_iter_n_children (model, NULL) == 0)
	return;
      gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
      break;
    case 2:
      /* modify */
      return;
      if (gtk_tree_model_iter_n_children (model, NULL) == 0)
	return;
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
      			  TEXT_COLUMN, words[g_rand_int_range (grand, 0, NUM_WORDS)],
      			  -1);
      break;
    default:
      g_assert_not_reached ();
    }
}

static gboolean
futz (void)
{
  int i;

  for (i = 0; i < 15; i++)
    futz_row ();
  g_print ("Number of rows: %d\n", gtk_tree_model_iter_n_children (model, NULL));
  return TRUE;
}

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *scrolled_window;
  GtkWidget *tree_view;
  GtkWidget *hbox;
  GtkWidget *button;
  GtkTreePath *path;
  gboolean done = FALSE;

  gtk_init ();

  path = gtk_tree_path_new_from_string ("80");
  window = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), "Reflow test");
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_box_append (GTK_BOX (vbox), gtk_label_new ("Incremental Reflow Test"));
  gtk_window_set_child (GTK_WINDOW (window), vbox);
  scrolled_window = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  gtk_widget_set_vexpand (scrolled_window, TRUE);
  gtk_box_append (GTK_BOX (vbox), scrolled_window);

  initialize_model ();
  tree_view = gtk_tree_view_new_with_model (model);
  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (tree_view), path, NULL, TRUE, 0.5, 0.0);
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
  gtk_tree_selection_select_path (selection, path);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree_view), FALSE);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       -1,
					       NULL,
					       gtk_cell_renderer_text_new (),
					       "text", TEXT_COLUMN,
					       NULL);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window), tree_view);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_append (GTK_BOX (vbox), hbox);
  button = gtk_button_new_with_mnemonic ("<b>_Futz!!</b>");
  gtk_box_append (GTK_BOX (hbox), button);
  gtk_label_set_use_markup (GTK_LABEL (gtk_button_get_child (GTK_BUTTON (button))), TRUE);
  g_signal_connect (button, "clicked", G_CALLBACK (futz), NULL);
  g_signal_connect (button, "realize", G_CALLBACK (gtk_widget_grab_focus), NULL);
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 400);
  gtk_window_present (GTK_WINDOW (window));
  g_timeout_add (1000, (GSourceFunc) futz, NULL);
  while (!done)
    g_main_context_iteration (NULL, TRUE);
  return 0;
}
