/* testtreecolumnsizing.c: Test case for tree view column resizing.
 *
 * Copyright (C) 2008  Kristian Rietveld  <kris@gtk.org>
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

#include <gtk/gtk.h>
#include <string.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

#define NO_EXPAND "No expandable columns"
#define SINGLE_EXPAND "One expandable column"
#define MULTI_EXPAND "Multiple expandable columns"
#define LAST_EXPAND "Last column is expandable"
#define BORDER_EXPAND "First and last columns are expandable"
#define ALL_EXPAND "All columns are expandable"

#define N_ROWS 10


static GtkTreeModel *
create_model (void)
{
  int i;
  GtkListStore *store;

  store = gtk_list_store_new (5,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_STRING);

  for (i = 0; i < N_ROWS; i++)
    {
      char *str;

      str = g_strdup_printf ("Row %d", i);
      gtk_list_store_insert_with_values (store, NULL, i,
                                         0, str,
                                         1, "Blah blah blah blah blah",
                                         2, "Less blah",
                                         3, "Medium length",
                                         4, "Eek",
                                         -1);
      g_free (str);
    }

  return GTK_TREE_MODEL (store);
}

static void
toggle_long_content_row (GtkToggleButton *button,
                         gpointer         user_data)
{
  GtkTreeModel *model;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (user_data));
  if (gtk_tree_model_iter_n_children (model, NULL) == N_ROWS)
    {
      gtk_list_store_insert_with_values (GTK_LIST_STORE (model), NULL, N_ROWS,
                                         0, "Very very very very longggggg",
                                         1, "Blah blah blah blah blah",
                                         2, "Less blah",
                                         3, "Medium length",
                                         4, "Eek we make the scrollbar appear",
                                         -1);
    }
  else
    {
      GtkTreeIter iter;

      gtk_tree_model_iter_nth_child (model, &iter, NULL, N_ROWS);
      gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
    }
}

static void
combo_box_changed (GtkComboBox *combo_box,
                   gpointer     user_data)
{
  char *str;
  GList *list;
  GList *columns;

  str = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (combo_box));
  if (!str)
    return;

  columns = gtk_tree_view_get_columns (GTK_TREE_VIEW (user_data));

  if (!strcmp (str, NO_EXPAND))
    {
      for (list = columns; list; list = list->next)
        gtk_tree_view_column_set_expand (list->data, FALSE);
    }
  else if (!strcmp (str, SINGLE_EXPAND))
    {
      for (list = columns; list; list = list->next)
        {
          if (list->prev && !list->prev->prev)
            /* This is the second column */
            gtk_tree_view_column_set_expand (list->data, TRUE);
          else
            gtk_tree_view_column_set_expand (list->data, FALSE);
        }
    }
  else if (!strcmp (str, MULTI_EXPAND))
    {
      for (list = columns; list; list = list->next)
        {
          if (list->prev && !list->prev->prev)
            /* This is the second column */
            gtk_tree_view_column_set_expand (list->data, TRUE);
          else if (list->prev && !list->prev->prev->prev)
            /* This is the third column */
            gtk_tree_view_column_set_expand (list->data, TRUE);
          else
            gtk_tree_view_column_set_expand (list->data, FALSE);
        }
    }
  else if (!strcmp (str, LAST_EXPAND))
    {
      for (list = columns; list->next; list = list->next)
        gtk_tree_view_column_set_expand (list->data, FALSE);
      /* This is the last column */
      gtk_tree_view_column_set_expand (list->data, TRUE);
    }
  else if (!strcmp (str, BORDER_EXPAND))
    {
      gtk_tree_view_column_set_expand (columns->data, TRUE);
      for (list = columns->next; list->next; list = list->next)
        gtk_tree_view_column_set_expand (list->data, FALSE);
      /* This is the last column */
      gtk_tree_view_column_set_expand (list->data, TRUE);
    }
  else if (!strcmp (str, ALL_EXPAND))
    {
      for (list = columns; list; list = list->next)
        gtk_tree_view_column_set_expand (list->data, TRUE);
    }

  g_free (str);
  g_list_free (columns);
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
  int i;
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *combo_box;
  GtkWidget *sw;
  GtkWidget *tree_view;
  GtkWidget *button;
  gboolean done = FALSE;

  gtk_init ();

  /* Window and box */
  window = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  gtk_window_set_child (GTK_WINDOW (window), vbox);

  /* Option menu contents */
  combo_box = gtk_combo_box_text_new ();

  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), NO_EXPAND);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), SINGLE_EXPAND);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), MULTI_EXPAND);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), LAST_EXPAND);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), BORDER_EXPAND);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), ALL_EXPAND);

  gtk_box_append (GTK_BOX (vbox), combo_box);

  /* Scrolled window and tree view */
  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_widget_set_vexpand (sw, TRUE);
  gtk_box_append (GTK_BOX (vbox), sw);

  tree_view = gtk_tree_view_new_with_model (create_model ());
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), tree_view);

  for (i = 0; i < 5; i++)
    {
      GtkTreeViewColumn *column;

      gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
                                                   i, "Header",
                                                   gtk_cell_renderer_text_new (),
                                                   "text", i,
                                                   NULL);

      column = gtk_tree_view_get_column (GTK_TREE_VIEW (tree_view), i);
      gtk_tree_view_column_set_resizable (column, TRUE);
    }

  /* Toggle button for long content row */
  button = gtk_toggle_button_new_with_label ("Toggle long content row");
  g_signal_connect (button, "toggled",
                    G_CALLBACK (toggle_long_content_row), tree_view);
  gtk_box_append (GTK_BOX (vbox), button);

  /* Set up option menu callback and default item */
  g_signal_connect (combo_box, "changed",
                    G_CALLBACK (combo_box_changed), tree_view);
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), 0);

  /* Done */
  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
