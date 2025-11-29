/* treestoretest.c
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
#include <stdlib.h>
#include <string.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

GtkTreeStore *base_model;
static int node_count = 0;

static void
selection_changed (GtkTreeSelection *selection,
		   GtkWidget        *button)
{
  if (gtk_tree_selection_get_selected (selection, NULL, NULL))
    gtk_widget_set_sensitive (button, TRUE);
  else
    gtk_widget_set_sensitive (button, FALSE);
}

static void
node_set (GtkTreeIter *iter)
{
  int n;
  char *str;

  str = g_strdup_printf ("Row (<span color=\"red\">%d</span>)", node_count++);
  gtk_tree_store_set (base_model, iter, 0, str, -1);
  g_free (str);

  n = g_random_int_range (10000,99999);
  if (n < 0)
    n *= -1;
  str = g_strdup_printf ("%d", n);

  gtk_tree_store_set (base_model, iter, 1, str, -1);
  g_free (str);
}

static void
iter_remove (GtkWidget *button, GtkTreeView *tree_view)
{
  GtkTreeIter selected;
  GtkTreeModel *model;

  model = gtk_tree_view_get_model (tree_view);

  if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (tree_view),
				       NULL,
				       &selected))
    {
      if (GTK_IS_TREE_STORE (model))
	{
	  gtk_tree_store_remove (GTK_TREE_STORE (model), &selected);
	}
    }
}

static void
iter_insert (GtkWidget *button, GtkTreeView *tree_view)
{
  GtkWidget *entry;
  GtkTreeIter iter;
  GtkTreeIter selected;
  GtkTreeModel *model = gtk_tree_view_get_model (tree_view);

  entry = g_object_get_data (G_OBJECT (button), "user_data");
  if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)),
				       NULL,
				       &selected))
    {
      gtk_tree_store_insert (GTK_TREE_STORE (model),
			     &iter,
			     &selected,
			     atoi (gtk_editable_get_text (GTK_EDITABLE (entry))));
    }
  else
    {
      gtk_tree_store_insert (GTK_TREE_STORE (model),
			     &iter,
			     NULL,
			     atoi (gtk_editable_get_text (GTK_EDITABLE (entry))));
    }

  node_set (&iter);
}

static void
iter_change (GtkWidget *button, GtkTreeView *tree_view)
{
  GtkWidget *entry;
  GtkTreeIter selected;
  GtkTreeModel *model = gtk_tree_view_get_model (tree_view);

  entry = g_object_get_data (G_OBJECT (button), "user_data");
  if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)),
				       NULL, &selected))
    {
      gtk_tree_store_set (GTK_TREE_STORE (model),
			  &selected,
			  1,
			  gtk_editable_get_text (GTK_EDITABLE (entry)),
			  -1);
    }
}

static void
iter_insert_with_values (GtkWidget *button, GtkTreeView *tree_view)
{
  GtkWidget *entry;
  GtkTreeIter iter;
  GtkTreeIter selected;
  GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
  char *str1, *str2;

  entry = g_object_get_data (G_OBJECT (button), "user_data");
  str1 = g_strdup_printf ("Row (<span color=\"red\">%d</span>)", node_count++);
  str2 = g_strdup_printf ("%d", atoi (gtk_editable_get_text (GTK_EDITABLE (entry))));

  if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)),
				       NULL,
				       &selected))
    {
      gtk_tree_store_insert_with_values (GTK_TREE_STORE (model),
					 &iter,
					 &selected,
					 -1,
					 0, str1,
					 1, str2,
					 -1);
    }
  else
    {
      gtk_tree_store_insert_with_values (GTK_TREE_STORE (model),
					 &iter,
					 NULL,
					 -1,
					 0, str1,
					 1, str2,
					 -1);
    }

  g_free (str1);
  g_free (str2);
}

static void
iter_insert_before  (GtkWidget *button, GtkTreeView *tree_view)
{
  GtkTreeIter iter;
  GtkTreeIter selected;
  GtkTreeModel *model = gtk_tree_view_get_model (tree_view);

  if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)),
				       NULL,
				       &selected))
    {
      gtk_tree_store_insert_before (GTK_TREE_STORE (model),
				    &iter,
				    NULL,
				    &selected);
    }
  else
    {
      gtk_tree_store_insert_before (GTK_TREE_STORE (model),
				    &iter,
				    NULL,
				    NULL);
    }

  node_set (&iter);
}

static void
iter_insert_after (GtkWidget *button, GtkTreeView *tree_view)
{
  GtkTreeIter iter;
  GtkTreeIter selected;
  GtkTreeModel *model = gtk_tree_view_get_model (tree_view);

  if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)),
				       NULL,
				       &selected))
    {
      if (GTK_IS_TREE_STORE (model))
	{
	  gtk_tree_store_insert_after (GTK_TREE_STORE (model),
				       &iter,
				       NULL,
				       &selected);
	  node_set (&iter);
	}
    }
  else
    {
      if (GTK_IS_TREE_STORE (model))
	{
	  gtk_tree_store_insert_after (GTK_TREE_STORE (model),
				       &iter,
				       NULL,
				       NULL);
	  node_set (&iter);
	}
    }
}

static void
iter_prepend (GtkWidget *button, GtkTreeView *tree_view)
{
  GtkTreeIter iter;
  GtkTreeIter selected;
  GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
  GtkTreeSelection *selection = gtk_tree_view_get_selection (tree_view);

  if (gtk_tree_selection_get_selected (selection, NULL, &selected))
    {
      if (GTK_IS_TREE_STORE (model))
	{
	  gtk_tree_store_prepend (GTK_TREE_STORE (model),
				  &iter,
				  &selected);
	  node_set (&iter);
	}
    }
  else
    {
      if (GTK_IS_TREE_STORE (model))
	{
	  gtk_tree_store_prepend (GTK_TREE_STORE (model),
				  &iter,
				  NULL);
	  node_set (&iter);
	}
    }
}

static void
iter_append (GtkWidget *button, GtkTreeView *tree_view)
{
  GtkTreeIter iter;
  GtkTreeIter selected;
  GtkTreeModel *model = gtk_tree_view_get_model (tree_view);

  if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)),
				       NULL,
				       &selected))
    {
      if (GTK_IS_TREE_STORE (model))
	{
	  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, &selected);
	  node_set (&iter);
	}
    }
  else
    {
      if (GTK_IS_TREE_STORE (model))
	{
	  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
	  node_set (&iter);
	}
    }
}

static gboolean done = FALSE;

static void
quit_cb (GtkWidget *widget,
         gpointer   user_data)
{
  gboolean *is_done = user_data;

  *is_done = TRUE;

  g_main_context_wakeup (NULL);
}

static void
make_window (int view_type)
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *hbox, *entry;
  GtkWidget *button;
  GtkWidget *scrolled_window;
  GtkWidget *tree_view;
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;
  GObject *selection;

  /* Make the Widgets/Objects */
  window = gtk_window_new ();
  switch (view_type)
    {
    case 0:
      gtk_window_set_title (GTK_WINDOW (window), "Unsorted list");
      break;
    case 1:
      gtk_window_set_title (GTK_WINDOW (window), "Sorted list");
      break;
    default:
      g_assert_not_reached ();
    }

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 350);
  scrolled_window = gtk_scrolled_window_new ();
  switch (view_type)
    {
    case 0:
      tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (base_model));
      break;
    case 1:
      {
	GtkTreeModel *sort_model;
	
	sort_model = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (base_model));
	tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (sort_model));
      }
      break;
    default:
      g_assert_not_reached ();
      tree_view = NULL; /* Quiet compiler */
      break;
    }

  selection = G_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)));
  gtk_tree_selection_set_mode (GTK_TREE_SELECTION (selection), GTK_SELECTION_SINGLE);

  /* Put them together */
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window), tree_view);
  gtk_widget_set_vexpand (scrolled_window, TRUE);
  gtk_box_append (GTK_BOX (vbox), scrolled_window);
  gtk_window_set_child (GTK_WINDOW (window), vbox);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);

  /* buttons */
  button = gtk_button_new_with_label ("gtk_tree_store_remove");
  gtk_box_append (GTK_BOX (vbox), button);
  g_signal_connect (selection, "changed",
                    G_CALLBACK (selection_changed),
                    button);
  g_signal_connect (button, "clicked",
                    G_CALLBACK (iter_remove),
                    tree_view);
  gtk_widget_set_sensitive (button, FALSE);

  button = gtk_button_new_with_label ("gtk_tree_store_insert");
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  entry = gtk_entry_new ();
  gtk_box_append (GTK_BOX (vbox), hbox);
  gtk_box_append (GTK_BOX (hbox), button);
  gtk_box_append (GTK_BOX (hbox), entry);
  g_object_set_data (G_OBJECT (button), "user_data", entry);
  g_signal_connect (button, "clicked",
                    G_CALLBACK (iter_insert),
                    tree_view);

  button = gtk_button_new_with_label ("gtk_tree_store_set");
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  entry = gtk_entry_new ();
  gtk_box_append (GTK_BOX (vbox), hbox);
  gtk_box_append (GTK_BOX (hbox), button);
  gtk_box_append (GTK_BOX (hbox), entry);
  g_object_set_data (G_OBJECT (button), "user_data", entry);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (iter_change),
		    tree_view);

  button = gtk_button_new_with_label ("gtk_tree_store_insert_with_values");
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  entry = gtk_entry_new ();
  gtk_box_append (GTK_BOX (vbox), hbox);
  gtk_box_append (GTK_BOX (hbox), button);
  gtk_box_append (GTK_BOX (hbox), entry);
  g_object_set_data (G_OBJECT (button), "user_data", entry);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (iter_insert_with_values),
		    tree_view);

  button = gtk_button_new_with_label ("gtk_tree_store_insert_before");
  gtk_box_append (GTK_BOX (vbox), button);
  g_signal_connect (button, "clicked",
                    G_CALLBACK (iter_insert_before),
                    tree_view);
  g_signal_connect (selection, "changed",
                    G_CALLBACK (selection_changed),
                    button);
  gtk_widget_set_sensitive (button, FALSE);

  button = gtk_button_new_with_label ("gtk_tree_store_insert_after");
  gtk_box_append (GTK_BOX (vbox), button);
  g_signal_connect (button, "clicked",
                    G_CALLBACK (iter_insert_after),
                    tree_view);
  g_signal_connect (selection, "changed",
                    G_CALLBACK (selection_changed),
                    button);
  gtk_widget_set_sensitive (button, FALSE);

  button = gtk_button_new_with_label ("gtk_tree_store_prepend");
  gtk_box_append (GTK_BOX (vbox), button);
  g_signal_connect (button, "clicked",
                    G_CALLBACK (iter_prepend),
                    tree_view);

  button = gtk_button_new_with_label ("gtk_tree_store_append");
  gtk_box_append (GTK_BOX (vbox), button);
  g_signal_connect (button, "clicked",
                    G_CALLBACK (iter_append),
                    tree_view);

  /* The selected column */
  cell = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Node ID", cell, "markup", 0, NULL);
  gtk_tree_view_column_set_sort_column_id (column, 0);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

  cell = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Random Number", cell, "text", 1, NULL);
  gtk_tree_view_column_set_sort_column_id (column, 1);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

  /* A few to start */
  if (view_type == 0)
    {
      iter_append (NULL, GTK_TREE_VIEW (tree_view));
      iter_append (NULL, GTK_TREE_VIEW (tree_view));
      iter_append (NULL, GTK_TREE_VIEW (tree_view));
      iter_append (NULL, GTK_TREE_VIEW (tree_view));
      iter_append (NULL, GTK_TREE_VIEW (tree_view));
      iter_append (NULL, GTK_TREE_VIEW (tree_view));
    }
  /* Show it all */
  gtk_window_present (GTK_WINDOW (window));
}

int
main (int argc, char *argv[])
{
  gtk_init ();

  base_model = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

  /* FIXME: reverse this */
  make_window (0);
  make_window (1);

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}

