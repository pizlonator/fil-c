#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static GtkTreeModel *
create_model (void)
{
  GtkTreeStore *store;
  GtkTreeIter iter;
  GtkTreeIter parent;

  store = gtk_tree_store_new (1, G_TYPE_STRING);

  gtk_tree_store_insert_with_values (store, &parent, NULL, 0,
				     0, "Applications", -1);

  gtk_tree_store_insert_with_values (store, &iter, &parent, 0,
				     0, "File Manager", -1);
  gtk_tree_store_insert_with_values (store, &iter, &parent, 0,
				     0, "Gossip", -1);
  gtk_tree_store_insert_with_values (store, &iter, &parent, 0,
				     0, "System Settings", -1);
  gtk_tree_store_insert_with_values (store, &iter, &parent, 0,
				     0, "The GIMP", -1);
  gtk_tree_store_insert_with_values (store, &iter, &parent, 0,
				     0, "Terminal", -1);
  gtk_tree_store_insert_with_values (store, &iter, &parent, 0,
				     0, "Word Processor", -1);


  gtk_tree_store_insert_with_values (store, &parent, NULL, 1,
				     0, "Documents", -1);

  gtk_tree_store_insert_with_values (store, &iter, &parent, 0,
				     0, "blaat.txt", -1);
  gtk_tree_store_insert_with_values (store, &iter, &parent, 0,
				     0, "sliff.txt", -1);
  gtk_tree_store_insert_with_values (store, &iter, &parent, 0,
				     0, "test.txt", -1);
  gtk_tree_store_insert_with_values (store, &iter, &parent, 0,
				     0, "blaat.txt", -1);
  gtk_tree_store_insert_with_values (store, &iter, &parent, 0,
				     0, "brrrr.txt", -1);
  gtk_tree_store_insert_with_values (store, &iter, &parent, 0,
				     0, "hohoho.txt", -1);


  gtk_tree_store_insert_with_values (store, &parent, NULL, 2,
				     0, "Images", -1);

  gtk_tree_store_insert_with_values (store, &iter, &parent, 0,
				     0, "image1.png", -1);
  gtk_tree_store_insert_with_values (store, &iter, &parent, 0,
				     0, "image2.png", -1);
  gtk_tree_store_insert_with_values (store, &iter, &parent, 0,
				     0, "image3.jpg", -1);

  return GTK_TREE_MODEL (store);
}

static void
set_color_func (GtkTreeViewColumn *column,
		GtkCellRenderer   *cell,
		GtkTreeModel      *model,
		GtkTreeIter       *iter,
		gpointer           data)
{
  if (gtk_tree_model_iter_has_child (model, iter))
    g_object_set (cell, "cell-background", "Grey", NULL);
  else
    g_object_set (cell, "cell-background", NULL, NULL);
}

static void
tree_view_row_activated (GtkTreeView       *tree_view,
			 GtkTreePath       *path,
			 GtkTreeViewColumn *column)
{
  if (gtk_tree_path_get_depth (path) > 1)
    return;

  if (gtk_tree_view_row_expanded (GTK_TREE_VIEW (tree_view), path))
    gtk_tree_view_collapse_row (GTK_TREE_VIEW (tree_view), path);
  else
    gtk_tree_view_expand_row (GTK_TREE_VIEW (tree_view), path, FALSE);
}

static gboolean
tree_view_select_func (GtkTreeSelection *selection,
		       GtkTreeModel     *model,
		       GtkTreePath      *path,
		       gboolean          path_currently_selected,
		       gpointer          data)
{
  if (gtk_tree_path_get_depth (path) > 1)
    return TRUE;

  return FALSE;
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
main (int argc, char **argv)
{
  GtkWidget *window, *sw, *tv;
  GtkTreeModel *model;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  gboolean done = FALSE;

  gtk_init ();

  model = create_model ();

  window = gtk_window_new ();
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);
  gtk_window_set_default_size (GTK_WINDOW (window), 320, 480);

  sw = gtk_scrolled_window_new ();
  gtk_window_set_child (GTK_WINDOW (window), sw);

  tv = gtk_tree_view_new_with_model (model);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), tv);

  g_signal_connect (tv, "row-activated",
		    G_CALLBACK (tree_view_row_activated), tv);
  g_object_set (tv,
		"show-expanders", FALSE,
		"level-indentation", 10,
		NULL);

  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tv), FALSE);
  gtk_tree_view_expand_all (GTK_TREE_VIEW (tv));

  gtk_tree_selection_set_select_function (gtk_tree_view_get_selection (GTK_TREE_VIEW (tv)),
					  tree_view_select_func,
					  NULL,
					  NULL);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("(none)",
						     renderer,
						     "text", 0,
						     NULL);
  gtk_tree_view_column_set_cell_data_func (column,
					   renderer,
					   set_color_func,
					   NULL,
					   NULL);
  gtk_tree_view_insert_column (GTK_TREE_VIEW (tv), column, 0);

  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
