/* testtreecolumns.c
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

/*
 * README README README README README README README README README README
 * README README README README README README README README README README
 * README README README README README README README README README README
 * README README README README README README README README README README
 * README README README README README README README README README README
 * README README README README README README README README README README
 * README README README README README README README README README README
 * README README README README README README README README README README
 * README README README README README README README README README README
 * README README README README README README README README README README
 * README README README README README README README README README README
 * README README README README README README README README README README
 * README README README README README README README README README README
 *
 * DO NOT!!! I REPEAT DO NOT!  EVER LOOK AT THIS CODE AS AN EXAMPLE OF WHAT YOUR
 * CODE SHOULD LOOK LIKE.
 *
 * IT IS VERY CONFUSING, AND IS MEANT TO TEST A LOT OF CODE IN THE TREE.  WHILE
 * IT IS ACTUALLY CORRECT CODE, IT IS NOT USEFUL.
 */

GtkWidget *left_tree_view;
GtkWidget *top_right_tree_view;
GtkWidget *bottom_right_tree_view;
GtkTreeModel *left_tree_model;
GtkTreeModel *top_right_tree_model;
GtkTreeModel *bottom_right_tree_model;
GtkWidget *sample_tree_view_top;
GtkWidget *sample_tree_view_bottom;

#define column_data "my_column_data"

static void move_row  (GtkTreeModel *src,
		       GtkTreeIter  *src_iter,
		       GtkTreeModel *dest,
		       GtkTreeIter  *dest_iter);

/* Kids, don't try this at home.  */

/* Small GtkTreeModel to model columns */
typedef struct _ViewColumnModel ViewColumnModel;
typedef struct _ViewColumnModelClass ViewColumnModelClass;

struct _ViewColumnModel
{
  GtkListStore parent;
  GtkTreeView *view;
  GList *columns;
  int stamp;
};

struct _ViewColumnModelClass
{
  GtkListStoreClass parent_class;
};

static void view_column_model_tree_model_init (GtkTreeModelIface *iface);
static void view_column_model_drag_source_init (GtkTreeDragSourceIface *iface);
static void view_column_model_drag_dest_init (GtkTreeDragDestIface *iface);


static GType view_column_model_get_type (void);
G_DEFINE_TYPE_WITH_CODE (ViewColumnModel, view_column_model, GTK_TYPE_LIST_STORE,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL, view_column_model_tree_model_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE, view_column_model_drag_source_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_DEST, view_column_model_drag_dest_init))



static void view_column_model_init (ViewColumnModel *model)
{
  model->stamp = g_random_int ();
}

static int
view_column_model_get_n_columns (GtkTreeModel *tree_model)
{
  return 2;
}

static GType
view_column_model_get_column_type (GtkTreeModel *tree_model,
				   int           index)
{
  switch (index)
    {
    case 0:
      return G_TYPE_STRING;
    case 1:
      return GTK_TYPE_TREE_VIEW_COLUMN;
    default:
      return G_TYPE_INVALID;
    }
}

static gboolean
view_column_model_get_iter (GtkTreeModel *tree_model,
			    GtkTreeIter  *iter,
			    GtkTreePath  *path)

{
  ViewColumnModel *view_model = (ViewColumnModel *)tree_model;
  GList *list;
  int i;

  g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  i = gtk_tree_path_get_indices (path)[0];
  list = g_list_nth (view_model->columns, i);

  if (list == NULL)
    return FALSE;

  iter->stamp = view_model->stamp;
  iter->user_data = list;

  return TRUE;
}

static GtkTreePath *
view_column_model_get_path (GtkTreeModel *tree_model,
			    GtkTreeIter  *iter)
{
  ViewColumnModel *view_model = (ViewColumnModel *)tree_model;
  GtkTreePath *retval;
  GList *list;
  int i = 0;

  g_return_val_if_fail (iter->stamp == view_model->stamp, NULL);

  for (list = view_model->columns; list; list = list->next)
    {
      if (list == (GList *)iter->user_data)
	break;
      i++;
    }
  if (list == NULL)
    return NULL;

  retval = gtk_tree_path_new ();
  gtk_tree_path_append_index (retval, i);
  return retval;
}

static void
view_column_model_get_value (GtkTreeModel *tree_model,
			     GtkTreeIter  *iter,
			     int           column,
			     GValue       *value)
{
#ifndef G_DISABLE_CHECKS
  ViewColumnModel *view_model = (ViewColumnModel *)tree_model;

  g_return_if_fail (column < 2);
  g_return_if_fail (view_model->stamp == iter->stamp);
  g_return_if_fail (iter->user_data != NULL);
#endif

  if (column == 0)
    {
      g_value_init (value, G_TYPE_STRING);
      g_value_set_string (value, gtk_tree_view_column_get_title (GTK_TREE_VIEW_COLUMN (((GList *)iter->user_data)->data)));
    }
  else
    {
      g_value_init (value, GTK_TYPE_TREE_VIEW_COLUMN);
      g_value_set_object (value, ((GList *)iter->user_data)->data);
    }
}

static gboolean
view_column_model_iter_next (GtkTreeModel  *tree_model,
			     GtkTreeIter   *iter)
{
#ifndef G_DISABLE_CHECKS
  ViewColumnModel *view_model = (ViewColumnModel *)tree_model;

  g_return_val_if_fail (view_model->stamp == iter->stamp, FALSE);
  g_return_val_if_fail (iter->user_data != NULL, FALSE);
#endif

  iter->user_data = ((GList *)iter->user_data)->next;
  return iter->user_data != NULL;
}

static gboolean
view_column_model_iter_children (GtkTreeModel *tree_model,
				 GtkTreeIter  *iter,
				 GtkTreeIter  *parent)
{
  ViewColumnModel *view_model = (ViewColumnModel *)tree_model;

  /* this is a list, nodes have no children */
  if (parent)
    return FALSE;

  /* but if parent == NULL we return the list itself as children of the
   * "root"
   */

  if (view_model->columns)
    {
      iter->stamp = view_model->stamp;
      iter->user_data = view_model->columns;
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
view_column_model_iter_has_child (GtkTreeModel *tree_model,
				  GtkTreeIter  *iter)
{
  return FALSE;
}

static int
view_column_model_iter_n_children (GtkTreeModel *tree_model,
				   GtkTreeIter  *iter)
{
  return g_list_length (((ViewColumnModel *)tree_model)->columns);
}

static int
view_column_model_iter_nth_child (GtkTreeModel *tree_model,
 				  GtkTreeIter  *iter,
				  GtkTreeIter  *parent,
				  int           n)
{
  ViewColumnModel *view_model = (ViewColumnModel *)tree_model;

  if (parent)
    return FALSE;

  iter->stamp = view_model->stamp;
  iter->user_data = g_list_nth ((GList *)view_model->columns, n);

  return (iter->user_data != NULL);
}

static gboolean
view_column_model_iter_parent (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter,
			       GtkTreeIter  *child)
{
  return FALSE;
}

static void
view_column_model_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_n_columns = view_column_model_get_n_columns;
  iface->get_column_type = view_column_model_get_column_type;
  iface->get_iter = view_column_model_get_iter;
  iface->get_path = view_column_model_get_path;
  iface->get_value = view_column_model_get_value;
  iface->iter_next = view_column_model_iter_next;
  iface->iter_children = view_column_model_iter_children;
  iface->iter_has_child = view_column_model_iter_has_child;
  iface->iter_n_children = view_column_model_iter_n_children;
  iface->iter_nth_child = view_column_model_iter_nth_child;
  iface->iter_parent = view_column_model_iter_parent;
}

static GdkContentProvider *
view_column_model_drag_data_get (GtkTreeDragSource *drag_source,
				 GtkTreePath       *path)
{
  return gtk_tree_create_row_drag_content (GTK_TREE_MODEL (drag_source), path);
}

static gboolean
view_column_model_drag_data_delete (GtkTreeDragSource *drag_source,
				    GtkTreePath       *path)
{
  /* Nothing -- we handle moves on the dest side */
  
  return TRUE;
}

static gboolean
view_column_model_row_drop_possible (GtkTreeDragDest *drag_dest,
				     GtkTreePath     *dest_path,
				     const GValue    *value)
{
  GtkTreeModel *src_model;
  
  if (gtk_tree_get_row_drag_data (value,
				  &src_model,
				  NULL))
    {
      if (src_model == left_tree_model ||
	  src_model == top_right_tree_model ||
	  src_model == bottom_right_tree_model)
	return TRUE;
    }

  return FALSE;
}

static gboolean
view_column_model_drag_data_received (GtkTreeDragDest *drag_dest,
				      GtkTreePath     *dest,
				      const GValue    *value)
{
  GtkTreeModel *src_model;
  GtkTreePath *src_path = NULL;
  gboolean retval = FALSE;
  
  if (gtk_tree_get_row_drag_data (value,
				  &src_model,
				  &src_path))
    {
      GtkTreeIter src_iter;
      GtkTreeIter dest_iter;
      gboolean have_dest;

      /* We are a little lazy here, and assume if we can't convert dest
       * to an iter, we need to append. See gtkliststore.c for a more
       * careful handling of this.
       */
      have_dest = gtk_tree_model_get_iter (GTK_TREE_MODEL (drag_dest), &dest_iter, dest);

      if (gtk_tree_model_get_iter (src_model, &src_iter, src_path))
	{
	  if (src_model == left_tree_model ||
	      src_model == top_right_tree_model ||
	      src_model == bottom_right_tree_model)
	    {
	      move_row (src_model, &src_iter, GTK_TREE_MODEL (drag_dest),
			have_dest ? &dest_iter : NULL);
	      retval = TRUE;
	    }
	}

      gtk_tree_path_free (src_path);
    }
  
  return retval;
}

static void
view_column_model_drag_source_init (GtkTreeDragSourceIface *iface)
{
  iface->drag_data_get = view_column_model_drag_data_get;
  iface->drag_data_delete = view_column_model_drag_data_delete;
}

static void
view_column_model_drag_dest_init (GtkTreeDragDestIface *iface)
{
  iface->drag_data_received = view_column_model_drag_data_received;
  iface->row_drop_possible = view_column_model_row_drop_possible;
}

static void
view_column_model_class_init (ViewColumnModelClass *klass)
{
}

static void
update_columns (GtkTreeView *view, ViewColumnModel *view_model)
{
  GList *old_columns = view_model->columns;
  int old_length, length;
  GList *a, *b;

  view_model->columns = gtk_tree_view_get_columns (view_model->view);

  /* As the view tells us one change at a time, we can do this hack. */
  length = g_list_length (view_model->columns);
  old_length = g_list_length (old_columns);
  if (length != old_length)
    {
      GtkTreePath *path;
      int i = 0;

      /* where are they different */
      for (a = old_columns, b = view_model->columns; a && b; a = a->next, b = b->next)
	{
	  if (a->data != b->data)
	    break;
	  i++;
	}
      path = gtk_tree_path_new ();
      gtk_tree_path_append_index (path, i);
      if (length < old_length)
	{
	  view_model->stamp++;
	  gtk_tree_model_row_deleted (GTK_TREE_MODEL (view_model), path);
	}
      else
	{
	  GtkTreeIter iter;
	  iter.stamp = view_model->stamp;
	  iter.user_data = b;
	  gtk_tree_model_row_inserted (GTK_TREE_MODEL (view_model), path, &iter);
	}
      gtk_tree_path_free (path);
    }
  else
    {
      int i;
      int m = 0, n = 1;
      int *new_order;
      GtkTreePath *path;

      a = old_columns; b = view_model->columns;

      while (a->data == b->data)
	{
	  a = a->next;
	  b = b->next;
	  if (a == NULL)
	    return;
	  m++;
	}

      new_order = g_new (int, length);

      if (a->next->data == b->data)
	{
	  b = b->next;
	  while (b->data != a->data)
	    {
	      b = b->next;
	      n++;
	    }
	  for (i = 0; i < m; i++)
	    new_order[i] = i;
	  for (i = m; i < m+n; i++)
	    new_order[i] = i+1;
	  new_order[i] = m;
	  for (i = m + n +1; i < length; i++)
	    new_order[i] = i;
	}
      else
	{
	  a = a->next;
	  while (a->data != b->data)
	    {
	      a = a->next;
	      n++;
	    }
	  for (i = 0; i < m; i++)
	    new_order[i] = i;
	  new_order[m] = m+n;
	  for (i = m+1; i < m + n+ 1; i++)
	    new_order[i] = i - 1;
	  for (i = m + n + 1; i < length; i++)
	    new_order[i] = i;
	}

      path = gtk_tree_path_new ();
      gtk_tree_model_rows_reordered (GTK_TREE_MODEL (view_model),
				     path,
				     NULL,
				     new_order);
      gtk_tree_path_free (path);
      g_free (new_order);
    }
  if (old_columns)
    g_list_free (old_columns);
}

static GtkTreeModel *
view_column_model_new (GtkTreeView *view)
{
  GtkTreeModel *retval;

  retval = g_object_new (view_column_model_get_type (), NULL);
  ((ViewColumnModel *)retval)->view = view;
  ((ViewColumnModel *)retval)->columns = gtk_tree_view_get_columns (view);

  g_signal_connect (view, "columns_changed", G_CALLBACK (update_columns), retval);

  return retval;
}

/* Back to sanity.
 */

static void
add_clicked (GtkWidget *button, gpointer data)
{
  static int i = 0;

  GtkTreeIter iter;
  GtkTreeViewColumn *column;
  GtkTreeSelection *selection;
  GtkCellRenderer *cell;
  char *label = g_strdup_printf ("Column %d", i);

  cell = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (label, cell, "text", 0, NULL);
  g_object_set_data_full (G_OBJECT (column), column_data, label, g_free);
  gtk_tree_view_column_set_reorderable (column, TRUE);
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_list_store_append (GTK_LIST_STORE (left_tree_model), &iter);
  gtk_list_store_set (GTK_LIST_STORE (left_tree_model), &iter, 0, label, 1, column, -1);
  i++;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (left_tree_view));
  gtk_tree_selection_select_iter (selection, &iter);
}

static void
get_visible (GtkTreeViewColumn *tree_column,
	     GtkCellRenderer   *cell,
	     GtkTreeModel      *tree_model,
	     GtkTreeIter       *iter,
	     gpointer           data)
{
  GtkTreeViewColumn *column;

  gtk_tree_model_get (tree_model, iter, 1, &column, -1);
  if (column)
    {
      gtk_cell_renderer_toggle_set_active (GTK_CELL_RENDERER_TOGGLE (cell),
					   gtk_tree_view_column_get_visible (column));
    }
}

static void
set_visible (GtkCellRendererToggle *cell,
	     char                  *path_str,
	     gpointer               data)
{
  GtkTreeView *tree_view = (GtkTreeView *) data;
  GtkTreeViewColumn *column;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_str);

  model = gtk_tree_view_get_model (tree_view);

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, 1, &column, -1);

  if (column)
    {
      gtk_tree_view_column_set_visible (column, ! gtk_tree_view_column_get_visible (column));
      gtk_tree_model_row_changed (model, path, &iter);
    }
  gtk_tree_path_free (path);
}

static void
move_to_left (GtkTreeModel *src,
	      GtkTreeIter  *src_iter,
	      GtkTreeIter  *dest_iter)
{
  GtkTreeIter iter;
  GtkTreeViewColumn *column;
  GtkTreeSelection *selection;
  char *label;

  gtk_tree_model_get (src, src_iter, 0, &label, 1, &column, -1);

  if (src == top_right_tree_model)
    gtk_tree_view_remove_column (GTK_TREE_VIEW (sample_tree_view_top), column);
  else
    gtk_tree_view_remove_column (GTK_TREE_VIEW (sample_tree_view_bottom), column);

  /*  gtk_list_store_remove (GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (data))), &iter);*/

  /* Put it back on the left */
  if (dest_iter)
    gtk_list_store_insert_before (GTK_LIST_STORE (left_tree_model),
				  &iter, dest_iter);
  else
    gtk_list_store_append (GTK_LIST_STORE (left_tree_model), &iter);
  
  gtk_list_store_set (GTK_LIST_STORE (left_tree_model), &iter, 0, label, 1, column, -1);
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (left_tree_view));
  gtk_tree_selection_select_iter (selection, &iter);

  g_free (label);
}

static void
move_to_right (GtkTreeIter  *src_iter,
	       GtkTreeModel *dest,
	       GtkTreeIter  *dest_iter)
{
  char *label;
  GtkTreeViewColumn *column;
  int before = -1;

  gtk_tree_model_get (GTK_TREE_MODEL (left_tree_model),
		      src_iter, 0, &label, 1, &column, -1);
  gtk_list_store_remove (GTK_LIST_STORE (left_tree_model), src_iter);

  if (dest_iter)
    {
      GtkTreePath *path = gtk_tree_model_get_path (dest, dest_iter);
      before = (gtk_tree_path_get_indices (path))[0];
      gtk_tree_path_free (path);
    }
  
  if (dest == top_right_tree_model)
    gtk_tree_view_insert_column (GTK_TREE_VIEW (sample_tree_view_top), column, before);
  else
    gtk_tree_view_insert_column (GTK_TREE_VIEW (sample_tree_view_bottom), column, before);

  g_free (label);
}

static void
move_up_or_down (GtkTreeModel *src,
		 GtkTreeIter  *src_iter,
		 GtkTreeModel *dest,
		 GtkTreeIter  *dest_iter)
{
  GtkTreeViewColumn *column;
  char *label;
  int before = -1;
  
  gtk_tree_model_get (src, src_iter, 0, &label, 1, &column, -1);

  if (dest_iter)
    {
      GtkTreePath *path = gtk_tree_model_get_path (dest, dest_iter);
      before = (gtk_tree_path_get_indices (path))[0];
      gtk_tree_path_free (path);
    }
  
  if (src == top_right_tree_model)
    gtk_tree_view_remove_column (GTK_TREE_VIEW (sample_tree_view_top), column);
  else
    gtk_tree_view_remove_column (GTK_TREE_VIEW (sample_tree_view_bottom), column);

  if (dest == top_right_tree_model)
    gtk_tree_view_insert_column (GTK_TREE_VIEW (sample_tree_view_top), column, before);
  else
    gtk_tree_view_insert_column (GTK_TREE_VIEW (sample_tree_view_bottom), column, before);

  g_free (label);
}

static void
move_row  (GtkTreeModel *src,
	   GtkTreeIter  *src_iter,
	   GtkTreeModel *dest,
	   GtkTreeIter  *dest_iter)
{
  if (src == left_tree_model)
    move_to_right (src_iter, dest, dest_iter);
  else if (dest == left_tree_model)
    move_to_left (src, src_iter, dest_iter);
  else 
    move_up_or_down (src, src_iter, dest, dest_iter);
}

static void
add_left_clicked (GtkWidget *button,
		  gpointer data)
{
  GtkTreeIter iter;

  GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (data));

  gtk_tree_selection_get_selected (selection, NULL, &iter);

  move_to_left (gtk_tree_view_get_model (GTK_TREE_VIEW (data)), &iter, NULL);
}

static void
add_right_clicked (GtkWidget *button, gpointer data)
{
  GtkTreeIter iter;

  GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (left_tree_view));

  gtk_tree_selection_get_selected (selection, NULL, &iter);

  move_to_right (&iter, gtk_tree_view_get_model (GTK_TREE_VIEW (data)), NULL);
}

static void
selection_changed (GtkTreeSelection *selection, GtkWidget *button)
{
  if (gtk_tree_selection_get_selected (selection, NULL, NULL))
    gtk_widget_set_sensitive (button, TRUE);
  else
    gtk_widget_set_sensitive (button, FALSE);
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
  GtkWidget *hbox, *vbox;
  GtkWidget *vbox2, *bbox;
  GtkWidget *button;
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;
  GtkWidget *swindow;
  GtkTreeModel *sample_model;
  GdkContentFormats *targets;
  int i;
  gboolean done = FALSE;

  gtk_init ();

  /* First initialize all the models for signal purposes */
  left_tree_model = (GtkTreeModel *) gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
  sample_model = (GtkTreeModel *) gtk_list_store_new (1, G_TYPE_STRING);
  sample_tree_view_top = gtk_tree_view_new_with_model (sample_model);
  sample_tree_view_bottom = gtk_tree_view_new_with_model (sample_model);
  top_right_tree_model = (GtkTreeModel *) view_column_model_new (GTK_TREE_VIEW (sample_tree_view_top));
  bottom_right_tree_model = (GtkTreeModel *) view_column_model_new (GTK_TREE_VIEW (sample_tree_view_bottom));
  top_right_tree_view = gtk_tree_view_new_with_model (top_right_tree_model);
  bottom_right_tree_view = gtk_tree_view_new_with_model (bottom_right_tree_model);

  for (i = 0; i < 10; i++)
    {
      GtkTreeIter iter;
      char *string = g_strdup_printf ("%d", i);
      gtk_list_store_append (GTK_LIST_STORE (sample_model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (sample_model), &iter, 0, string, -1);
      g_free (string);
    }

  /* Set up the test windows. */
  window = gtk_window_new ();
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done); 
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 300);
  gtk_window_set_title (GTK_WINDOW (window), "Top Window");
  swindow = gtk_scrolled_window_new ();
  gtk_window_set_child (GTK_WINDOW (window), swindow);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (swindow), sample_tree_view_top);
  gtk_window_present (GTK_WINDOW (window));

  window = gtk_window_new ();
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done); 
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 300);
  gtk_window_set_title (GTK_WINDOW (window), "Bottom Window");
  swindow = gtk_scrolled_window_new ();
  gtk_window_set_child (GTK_WINDOW (window), swindow);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (swindow), sample_tree_view_bottom);
  gtk_window_present (GTK_WINDOW (window));

  /* Set up the main window */
  window = gtk_window_new ();
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);
  gtk_window_set_default_size (GTK_WINDOW (window), 500, 300);
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_window_set_child (GTK_WINDOW (window), vbox);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_append (GTK_BOX (vbox), hbox);

  /* Left Pane */
  cell = gtk_cell_renderer_text_new ();

  swindow = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  left_tree_view = gtk_tree_view_new_with_model (left_tree_model);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (swindow), left_tree_view);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (left_tree_view), -1,
					       "Unattached Columns", cell, "text", 0, NULL);
  cell = gtk_cell_renderer_toggle_new ();
  g_signal_connect (cell, "toggled", G_CALLBACK (set_visible), left_tree_view);
  column = gtk_tree_view_column_new_with_attributes ("Visible", cell, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (left_tree_view), column);

  gtk_tree_view_column_set_cell_data_func (column, cell, get_visible, NULL, NULL);
  gtk_box_append (GTK_BOX (hbox), swindow);

  /* Middle Pane */
  vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_box_append (GTK_BOX (hbox), vbox2);

  bbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append (GTK_BOX (vbox2), bbox);

  button = gtk_button_new_with_mnemonic ("<< (_Q)");
  gtk_widget_set_sensitive (button, FALSE);
  g_signal_connect (button, "clicked", G_CALLBACK (add_left_clicked), top_right_tree_view);
  g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (top_right_tree_view)),
                    "changed", G_CALLBACK (selection_changed), button);
  gtk_box_append (GTK_BOX (bbox), button);

  button = gtk_button_new_with_mnemonic (">> (_W)");
  gtk_widget_set_sensitive (button, FALSE);
  g_signal_connect (button, "clicked", G_CALLBACK (add_right_clicked), top_right_tree_view);
  g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (left_tree_view)),
                    "changed", G_CALLBACK (selection_changed), button);
  gtk_box_append (GTK_BOX (bbox), button);

  bbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append (GTK_BOX (vbox2), bbox);

  button = gtk_button_new_with_mnemonic ("<< (_E)");
  gtk_widget_set_sensitive (button, FALSE);
  g_signal_connect (button, "clicked", G_CALLBACK (add_left_clicked), bottom_right_tree_view);
  g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (bottom_right_tree_view)),
                    "changed", G_CALLBACK (selection_changed), button);
  gtk_box_append (GTK_BOX (bbox), button);

  button = gtk_button_new_with_mnemonic (">> (_R)");
  gtk_widget_set_sensitive (button, FALSE);
  g_signal_connect (button, "clicked", G_CALLBACK (add_right_clicked), bottom_right_tree_view);
  g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (left_tree_view)),
                    "changed", G_CALLBACK (selection_changed), button);
  gtk_box_append (GTK_BOX (bbox), button);


  /* Right Pane */
  vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_box_append (GTK_BOX (hbox), vbox2);

  swindow = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (top_right_tree_view), FALSE);
  cell = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (top_right_tree_view), -1,
					       NULL, cell, "text", 0, NULL);
  cell = gtk_cell_renderer_toggle_new ();
  g_signal_connect (cell, "toggled", G_CALLBACK (set_visible), top_right_tree_view);
  column = gtk_tree_view_column_new_with_attributes (NULL, cell, NULL);
  gtk_tree_view_column_set_cell_data_func (column, cell, get_visible, NULL, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (top_right_tree_view), column);

  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (swindow), top_right_tree_view);
  gtk_box_append (GTK_BOX (vbox2), swindow);

  swindow = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (bottom_right_tree_view), FALSE);
  cell = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (bottom_right_tree_view), -1,
					       NULL, cell, "text", 0, NULL);
  cell = gtk_cell_renderer_toggle_new ();
  g_signal_connect (cell, "toggled", G_CALLBACK (set_visible), bottom_right_tree_view);
  column = gtk_tree_view_column_new_with_attributes (NULL, cell, NULL);
  gtk_tree_view_column_set_cell_data_func (column, cell, get_visible, NULL, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (bottom_right_tree_view), column);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (swindow), bottom_right_tree_view);
  gtk_box_append (GTK_BOX (vbox2), swindow);


  /* Drag and Drop */
  targets = gdk_content_formats_new_for_gtype (GTK_TYPE_TREE_ROW_DATA);
  gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (left_tree_view),
					  GDK_BUTTON1_MASK,
                                          targets,
					  GDK_ACTION_MOVE);
  gtk_tree_view_enable_model_drag_dest (GTK_TREE_VIEW (left_tree_view),
					targets,
					GDK_ACTION_MOVE);

  gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (top_right_tree_view),
					  GDK_BUTTON1_MASK,
					  targets,
					  GDK_ACTION_MOVE);
  gtk_tree_view_enable_model_drag_dest (GTK_TREE_VIEW (top_right_tree_view),
					targets,
					GDK_ACTION_MOVE);

  gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (bottom_right_tree_view),
					  GDK_BUTTON1_MASK,
					  targets,
					  GDK_ACTION_MOVE);
  gtk_tree_view_enable_model_drag_dest (GTK_TREE_VIEW (bottom_right_tree_view),
					targets,
					GDK_ACTION_MOVE);
  gdk_content_formats_unref (targets);

  gtk_box_append (GTK_BOX (vbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_append (GTK_BOX (vbox), hbox);
  button = gtk_button_new_with_mnemonic ("_Add new Column");
  g_signal_connect (button, "clicked", G_CALLBACK (add_clicked), left_tree_model);
  gtk_box_append (GTK_BOX (hbox), button);

  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
