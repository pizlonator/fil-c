#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static void
on_button_clicked (GtkWidget *widget, gpointer data)
{
  g_print ("Button %d clicked\n", GPOINTER_TO_INT (data));
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
kinetic_scrolling (void)
{
  GtkWidget *window, *swindow, *grid;
  GtkWidget *label;
  GtkWidget *button_grid, *button;
  GtkWidget *treeview;
  GtkCellRenderer *renderer;
  GtkListStore *store;
  GtkWidget *textview;
  GdkContentFormats *targets;
  int i;

  window = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 400);
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);

  grid = gtk_grid_new ();

  label = gtk_label_new ("Non scrollable widget using viewport");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
  gtk_widget_set_hexpand (label, TRUE);

  label = gtk_label_new ("Scrollable widget: TreeView");
  gtk_grid_attach (GTK_GRID (grid), label, 1, 0, 1, 1);
  gtk_widget_set_hexpand (label, TRUE);

  label = gtk_label_new ("Scrollable widget: TextView");
  gtk_grid_attach (GTK_GRID (grid), label, 2, 0, 1, 1);
  gtk_widget_set_hexpand (label, TRUE);

  button_grid = gtk_grid_new ();
  for (i = 0; i < 80; i++)
    {
      char *button_label = g_strdup_printf ("Button number %d", i);

      button = gtk_button_new_with_label (button_label);
      gtk_grid_attach (GTK_GRID (button_grid), button, 0, i, 1, 1);
      gtk_widget_set_hexpand (button, TRUE);
      g_signal_connect (button, "clicked",
                        G_CALLBACK (on_button_clicked),
                        GINT_TO_POINTER (i));
      g_free (button_label);
    }

  swindow = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_kinetic_scrolling (GTK_SCROLLED_WINDOW (swindow), TRUE);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (swindow), button_grid);

  gtk_grid_attach (GTK_GRID (grid), swindow, 0, 1, 1, 1);

  treeview = gtk_tree_view_new ();
  targets = gdk_content_formats_new_for_gtype (GTK_TYPE_TREE_ROW_DATA);
  gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (treeview),
                                          GDK_BUTTON1_MASK,
                                          targets,
                                          GDK_ACTION_MOVE | GDK_ACTION_COPY);
  gtk_tree_view_enable_model_drag_dest (GTK_TREE_VIEW (treeview),
                                        targets,
                                        GDK_ACTION_MOVE | GDK_ACTION_COPY);
  gdk_content_formats_unref (targets);

  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer, "editable", TRUE, NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
                                               0, "Title",
                                               renderer,
                                               "text", 0,
                                               NULL);
  store = gtk_list_store_new (1, G_TYPE_STRING);
  for (i = 0; i < 80; i++)
    {
      GtkTreeIter iter;
      char *iter_label = g_strdup_printf ("Row number %d", i);

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, iter_label, -1);
      g_free (iter_label);
    }
  gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (store));
  g_object_unref (store);

  swindow = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_kinetic_scrolling (GTK_SCROLLED_WINDOW (swindow), TRUE);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (swindow), treeview);

  gtk_grid_attach (GTK_GRID (grid), swindow, 1, 1, 1, 1);
  gtk_widget_set_hexpand (swindow, TRUE);
  gtk_widget_set_vexpand (swindow, TRUE);

  textview = gtk_text_view_new ();
  swindow = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_kinetic_scrolling (GTK_SCROLLED_WINDOW (swindow), TRUE);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (swindow), textview);

  gtk_grid_attach (GTK_GRID (grid), swindow, 2, 1, 1, 1);
  gtk_widget_set_hexpand (swindow, TRUE);
  gtk_widget_set_vexpand (swindow, TRUE);

  gtk_window_set_child (GTK_WINDOW (window), grid);

  gtk_window_present (GTK_WINDOW (window));
}

int
main (int argc, char **argv)
{
  gtk_init ();

  kinetic_scrolling ();

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
