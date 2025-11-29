/* Tree View/Editable Cells
 *
 * This demo demonstrates the use of editable cells in a GtkTreeView. If
 * you're new to the GtkTreeView widgets and associates, look into
 * the GtkListStore example first. It also shows how to use the
 * GtkCellRenderer::editing-started signal to do custom setup of the
 * editable widget.
 *
 * The cell renderers used in this demo are GtkCellRendererText,
 * GtkCellRendererCombo and GtkCellRendererProgress.
 */

#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

typedef struct
{
  int    number;
  char *product;
  int    yummy;
}
Item;

enum
{
  COLUMN_ITEM_NUMBER,
  COLUMN_ITEM_PRODUCT,
  COLUMN_ITEM_YUMMY,
  NUM_ITEM_COLUMNS
};

enum
{
  COLUMN_NUMBER_TEXT,
  NUM_NUMBER_COLUMNS
};

static GArray *articles = NULL;

static void
add_items (void)
{
  Item foo;

  g_return_if_fail (articles != NULL);

  foo.number = 3;
  foo.product = g_strdup ("bottles of coke");
  foo.yummy = 20;
  g_array_append_vals (articles, &foo, 1);

  foo.number = 5;
  foo.product = g_strdup ("packages of noodles");
  foo.yummy = 50;
  g_array_append_vals (articles, &foo, 1);

  foo.number = 2;
  foo.product = g_strdup ("packages of chocolate chip cookies");
  foo.yummy = 90;
  g_array_append_vals (articles, &foo, 1);

  foo.number = 1;
  foo.product = g_strdup ("can vanilla ice cream");
  foo.yummy = 60;
  g_array_append_vals (articles, &foo, 1);

  foo.number = 6;
  foo.product = g_strdup ("eggs");
  foo.yummy = 10;
  g_array_append_vals (articles, &foo, 1);
}

static GtkTreeModel *
create_items_model (void)
{
  int i = 0;
  GtkListStore *model;
  GtkTreeIter iter;

  /* create array */
  articles = g_array_sized_new (FALSE, FALSE, sizeof (Item), 1);

  add_items ();

  /* create list store */
  model = gtk_list_store_new (NUM_ITEM_COLUMNS, G_TYPE_INT, G_TYPE_STRING,
                              G_TYPE_INT, G_TYPE_BOOLEAN);

  /* add items */
  for (i = 0; i < articles->len; i++)
    {
      gtk_list_store_append (model, &iter);

      gtk_list_store_set (model, &iter,
                          COLUMN_ITEM_NUMBER,
                          g_array_index (articles, Item, i).number,
                          COLUMN_ITEM_PRODUCT,
                          g_array_index (articles, Item, i).product,
                          COLUMN_ITEM_YUMMY,
                          g_array_index (articles, Item, i).yummy,
                          -1);
    }

  return GTK_TREE_MODEL (model);
}

static GtkTreeModel *
create_numbers_model (void)
{
#define N_NUMBERS 10
  int i = 0;
  GtkListStore *model;
  GtkTreeIter iter;

  /* create list store */
  model = gtk_list_store_new (NUM_NUMBER_COLUMNS, G_TYPE_STRING, G_TYPE_INT);

  /* add numbers */
  for (i = 0; i < N_NUMBERS; i++)
    {
      char str[2];

      str[0] = '0' + i;
      str[1] = '\0';

      gtk_list_store_append (model, &iter);

      gtk_list_store_set (model, &iter,
                          COLUMN_NUMBER_TEXT, str,
                          -1);
    }

  return GTK_TREE_MODEL (model);

#undef N_NUMBERS
}

static void
add_item (GtkWidget *button, gpointer data)
{
  Item foo;
  GtkTreeIter current, iter;
  GtkTreePath *path;
  GtkTreeModel *model;
  GtkTreeViewColumn *column;
  GtkTreeView *treeview = (GtkTreeView *)data;

  g_return_if_fail (articles != NULL);

  foo.number = 0;
  foo.product = g_strdup ("Description here");
  foo.yummy = 50;
  g_array_append_vals (articles, &foo, 1);

  /* Insert a new row below the current one */
  gtk_tree_view_get_cursor (treeview, &path, NULL);
  model = gtk_tree_view_get_model (treeview);
  if (path)
    {
      gtk_tree_model_get_iter (model, &current, path);
      gtk_tree_path_free (path);
      gtk_list_store_insert_after (GTK_LIST_STORE (model), &iter, &current);
    }
  else
    {
      gtk_list_store_insert (GTK_LIST_STORE (model), &iter, -1);
    }

  /* Set the data for the new row */
  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                      COLUMN_ITEM_NUMBER, foo.number,
                      COLUMN_ITEM_PRODUCT, foo.product,
                      COLUMN_ITEM_YUMMY, foo.yummy,
                      -1);

  /* Move focus to the new row */
  path = gtk_tree_model_get_path (model, &iter);
  column = gtk_tree_view_get_column (treeview, 0);
  gtk_tree_view_set_cursor (treeview, path, column, FALSE);

  gtk_tree_path_free (path);
}

static void
remove_item (GtkWidget *widget, gpointer data)
{
  GtkTreeIter iter;
  GtkTreeView *treeview = (GtkTreeView *)data;
  GtkTreeModel *model = gtk_tree_view_get_model (treeview);
  GtkTreeSelection *selection = gtk_tree_view_get_selection (treeview);

  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      int i;
      GtkTreePath *path;

      path = gtk_tree_model_get_path (model, &iter);
      i = gtk_tree_path_get_indices (path)[0];
      gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

      g_array_remove_index (articles, i);

      gtk_tree_path_free (path);
    }
}

static gboolean
separator_row (GtkTreeModel *model,
               GtkTreeIter  *iter,
               gpointer      data)
{
  GtkTreePath *path;
  int idx;

  path = gtk_tree_model_get_path (model, iter);
  idx = gtk_tree_path_get_indices (path)[0];

  gtk_tree_path_free (path);

  return idx == 5;
}

static void
editing_started (GtkCellRenderer *cell,
                 GtkCellEditable *editable,
                 const char      *path,
                 gpointer         data)
{
  gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (editable),
                                        separator_row, NULL, NULL);
}

static void
cell_edited (GtkCellRendererText *cell,
             const char          *path_string,
             const char          *new_text,
             gpointer             data)
{
  GtkTreeModel *model = (GtkTreeModel *)data;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  GtkTreeIter iter;

  int column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), "column"));

  gtk_tree_model_get_iter (model, &iter, path);

  switch (column)
    {
    case COLUMN_ITEM_NUMBER:
      {
        int i;

        i = gtk_tree_path_get_indices (path)[0];
        g_array_index (articles, Item, i).number = atoi (new_text);

        gtk_list_store_set (GTK_LIST_STORE (model), &iter, column,
                            g_array_index (articles, Item, i).number, -1);
      }
      break;

    case COLUMN_ITEM_PRODUCT:
      {
        int i;
        char *old_text;

        gtk_tree_model_get (model, &iter, column, &old_text, -1);
        g_free (old_text);

        i = gtk_tree_path_get_indices (path)[0];
        g_free (g_array_index (articles, Item, i).product);
        g_array_index (articles, Item, i).product = g_strdup (new_text);

        gtk_list_store_set (GTK_LIST_STORE (model), &iter, column,
                            g_array_index (articles, Item, i).product, -1);
      }
      break;

    default:
      g_assert_not_reached ();
    }

  gtk_tree_path_free (path);
}

static void
add_columns (GtkTreeView  *treeview,
             GtkTreeModel *items_model,
             GtkTreeModel *numbers_model)
{
  GtkCellRenderer *renderer;

  /* number column */
  renderer = gtk_cell_renderer_combo_new ();
  g_object_set (renderer,
                "model", numbers_model,
                "text-column", COLUMN_NUMBER_TEXT,
                "has-entry", FALSE,
                "editable", TRUE,
                NULL);
  g_signal_connect (renderer, "edited",
                    G_CALLBACK (cell_edited), items_model);
  g_signal_connect (renderer, "editing-started",
                    G_CALLBACK (editing_started), NULL);
  g_object_set_data (G_OBJECT (renderer), "column", GINT_TO_POINTER (COLUMN_ITEM_NUMBER));

  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
                                               -1, "Number", renderer,
                                               "text", COLUMN_ITEM_NUMBER,
                                               NULL);

  /* product column */
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer,
                "editable", TRUE,
                NULL);
  g_signal_connect (renderer, "edited",
                    G_CALLBACK (cell_edited), items_model);
  g_object_set_data (G_OBJECT (renderer), "column", GINT_TO_POINTER (COLUMN_ITEM_PRODUCT));

  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
                                               -1, "Product", renderer,
                                               "text", COLUMN_ITEM_PRODUCT,
                                               NULL);

  /* yummy column */
  renderer = gtk_cell_renderer_progress_new ();
  g_object_set_data (G_OBJECT (renderer), "column", GINT_TO_POINTER (COLUMN_ITEM_YUMMY));

  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
                                               -1, "Yummy", renderer,
                                               "value", COLUMN_ITEM_YUMMY,
                                               NULL);
}

GtkWidget *
do_editable_cells (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *vbox;
      GtkWidget *hbox;
      GtkWidget *sw;
      GtkWidget *treeview;
      GtkWidget *button;
      GtkTreeModel *items_model;
      GtkTreeModel *numbers_model;

      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Editable Cells");
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      gtk_widget_set_margin_start (vbox, 5);
      gtk_widget_set_margin_end (vbox, 5);
      gtk_widget_set_margin_top (vbox, 5);
      gtk_widget_set_margin_bottom (vbox, 5);
      gtk_window_set_child (GTK_WINDOW (window), vbox);

      gtk_box_append (GTK_BOX (vbox),
                          gtk_label_new ("Shopping list (you can edit the cells!)"));

      sw = gtk_scrolled_window_new ();
      gtk_scrolled_window_set_has_frame (GTK_SCROLLED_WINDOW (sw), TRUE);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);
      gtk_box_append (GTK_BOX (vbox), sw);

      /* create models */
      items_model = create_items_model ();
      numbers_model = create_numbers_model ();

      /* create tree view */
      treeview = gtk_tree_view_new_with_model (items_model);
      gtk_widget_set_vexpand (treeview, TRUE);
      gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)),
                                   GTK_SELECTION_SINGLE);

      add_columns (GTK_TREE_VIEW (treeview), items_model, numbers_model);

      g_object_unref (numbers_model);
      g_object_unref (items_model);

      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), treeview);

      /* some buttons */
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
      gtk_box_set_homogeneous (GTK_BOX (hbox), TRUE);
      gtk_box_append (GTK_BOX (vbox), hbox);

      button = gtk_button_new_with_label ("Add item");
      g_signal_connect (button, "clicked",
                        G_CALLBACK (add_item), treeview);
      gtk_box_append (GTK_BOX (hbox), button);

      button = gtk_button_new_with_label ("Remove item");
      g_signal_connect (button, "clicked",
                        G_CALLBACK (remove_item), treeview);
      gtk_box_append (GTK_BOX (hbox), button);

      gtk_window_set_default_size (GTK_WINDOW (window), 320, 200);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
