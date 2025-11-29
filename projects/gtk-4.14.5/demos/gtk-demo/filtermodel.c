/* Tree View/Filter Model
 * #Keywords: GtkTreeView
 *
 * This example demonstrates how GtkTreeModelFilter can be used not
 * just to show a subset of the rows, but also to compute columns
 * that are not actually present in the underlying model.
 */

#include <gtk/gtk.h>
#include <stdlib.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

enum {
  WIDTH_COLUMN,
  HEIGHT_COLUMN,
  AREA_COLUMN,
  SQUARE_COLUMN
};

static void
format_number (GtkTreeViewColumn *col,
               GtkCellRenderer   *cell,
               GtkTreeModel      *model,
               GtkTreeIter       *iter,
               gpointer           data)
{
  int num;
  char *text;

  gtk_tree_model_get (model, iter, GPOINTER_TO_INT (data), &num, -1);
  text = g_strdup_printf ("%d", num);
  g_object_set (cell, "text", text, NULL);
  g_free (text);
}

static void
filter_modify_func (GtkTreeModel *model,
                    GtkTreeIter  *iter,
                    GValue       *value,
                    int           column,
                    gpointer      data)
{
  GtkTreeModelFilter *filter_model = GTK_TREE_MODEL_FILTER (model);
  int width, height;
  GtkTreeModel *child_model;
  GtkTreeIter child_iter;

  child_model = gtk_tree_model_filter_get_model (filter_model);
  gtk_tree_model_filter_convert_iter_to_child_iter (filter_model, &child_iter, iter);

  gtk_tree_model_get (child_model, &child_iter,
                      WIDTH_COLUMN, &width,
                      HEIGHT_COLUMN, &height,
                      -1);

  switch (column)
    {
    case WIDTH_COLUMN:
      g_value_set_int (value, width);
      break;
    case HEIGHT_COLUMN:
      g_value_set_int (value, height);
      break;
    case AREA_COLUMN:
      g_value_set_int (value, width * height);
      break;
    case SQUARE_COLUMN:
      g_value_set_boolean (value, width == height);
      break;
    default:
      g_assert_not_reached ();
    }
}

static gboolean
visible_func (GtkTreeModel *model,
              GtkTreeIter  *iter,
              gpointer      data)
{
  int width;

  gtk_tree_model_get (model, iter,
                      WIDTH_COLUMN, &width,
                      -1);

  return width < 10;
}

static void
cell_edited (GtkCellRendererSpin *cell,
             const char          *path_string,
             const char          *new_text,
             GtkListStore        *store)
{
  int val;
  GtkTreePath *path;
  GtkTreeIter iter;
  int column;

  path = gtk_tree_path_new_from_string (path_string);
  gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path);
  gtk_tree_path_free (path);

  column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), "column"));

  val = atoi (new_text);

  gtk_list_store_set (store, &iter, column, val, -1);
}

GtkWidget *
do_filtermodel (GtkWidget *do_widget)
{
  static GtkWidget *window;
  GtkWidget *tree;
  GtkListStore *store;
  GtkTreeModel *model;
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;
  GType types[4];

  if (!window)
    {
      GtkBuilder *builder;

      builder = gtk_builder_new_from_resource ("/filtermodel/filtermodel.ui");
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window1"));
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      store = (GtkListStore*)gtk_builder_get_object (builder, "liststore1");

      column = (GtkTreeViewColumn*)gtk_builder_get_object (builder, "treeviewcolumn1");
      cell = (GtkCellRenderer*)gtk_builder_get_object (builder, "cellrenderertext1");
      gtk_tree_view_column_set_cell_data_func (column, cell,
                                               format_number, GINT_TO_POINTER (WIDTH_COLUMN), NULL);
      g_object_set_data (G_OBJECT (cell), "column", GINT_TO_POINTER (WIDTH_COLUMN));
      g_signal_connect (cell, "edited", G_CALLBACK (cell_edited), store);

      column = (GtkTreeViewColumn*)gtk_builder_get_object (builder, "treeviewcolumn2");
      cell = (GtkCellRenderer*)gtk_builder_get_object (builder, "cellrenderertext2");
      gtk_tree_view_column_set_cell_data_func (column, cell,
                                               format_number, GINT_TO_POINTER (HEIGHT_COLUMN), NULL);
      g_object_set_data (G_OBJECT (cell), "column", GINT_TO_POINTER (HEIGHT_COLUMN));
      g_signal_connect (cell, "edited", G_CALLBACK (cell_edited), store);

      column = (GtkTreeViewColumn*)gtk_builder_get_object (builder, "treeviewcolumn3");
      cell = (GtkCellRenderer*)gtk_builder_get_object (builder, "cellrenderertext3");
      gtk_tree_view_column_set_cell_data_func (column, cell,
                                               format_number, GINT_TO_POINTER (WIDTH_COLUMN), NULL);

      column = (GtkTreeViewColumn*)gtk_builder_get_object (builder, "treeviewcolumn4");
      cell = (GtkCellRenderer*)gtk_builder_get_object (builder, "cellrenderertext4");
      gtk_tree_view_column_set_cell_data_func (column, cell,
                                               format_number, GINT_TO_POINTER (HEIGHT_COLUMN), NULL);

      column = (GtkTreeViewColumn*)gtk_builder_get_object (builder, "treeviewcolumn5");
      cell = (GtkCellRenderer*)gtk_builder_get_object (builder, "cellrenderertext5");
      gtk_tree_view_column_set_cell_data_func (column, cell,
                                               format_number, GINT_TO_POINTER (AREA_COLUMN), NULL);

      column = (GtkTreeViewColumn*)gtk_builder_get_object (builder, "treeviewcolumn6");
      cell = (GtkCellRenderer*)gtk_builder_get_object (builder, "cellrendererpixbuf1");
      gtk_tree_view_column_add_attribute (column, cell, "visible", SQUARE_COLUMN);

      tree = (GtkWidget*)gtk_builder_get_object (builder, "treeview2");

      types[WIDTH_COLUMN] = G_TYPE_INT;
      types[HEIGHT_COLUMN] = G_TYPE_INT;
      types[AREA_COLUMN] = G_TYPE_INT;
      types[SQUARE_COLUMN] = G_TYPE_BOOLEAN;
      model = gtk_tree_model_filter_new (GTK_TREE_MODEL (store), NULL);
      gtk_tree_model_filter_set_modify_func (GTK_TREE_MODEL_FILTER (model),
                                             G_N_ELEMENTS (types), types,
                                             filter_modify_func, NULL, NULL);

      gtk_tree_view_set_model (GTK_TREE_VIEW (tree), model);

      column = (GtkTreeViewColumn*)gtk_builder_get_object (builder, "treeviewcolumn7");
      cell = (GtkCellRenderer*)gtk_builder_get_object (builder, "cellrenderertext6");
      gtk_tree_view_column_set_cell_data_func (column, cell,
                                               format_number, GINT_TO_POINTER (WIDTH_COLUMN), NULL);

      column = (GtkTreeViewColumn*)gtk_builder_get_object (builder, "treeviewcolumn8");
      cell = (GtkCellRenderer*)gtk_builder_get_object (builder, "cellrenderertext7");
      gtk_tree_view_column_set_cell_data_func (column, cell,
                                               format_number, GINT_TO_POINTER (HEIGHT_COLUMN), NULL);

      tree = (GtkWidget*)gtk_builder_get_object (builder, "treeview3");

      model = gtk_tree_model_filter_new (GTK_TREE_MODEL (store), NULL);
      gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (model),
                                              visible_func, NULL, NULL);
      gtk_tree_view_set_model (GTK_TREE_VIEW (tree), model);

      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
