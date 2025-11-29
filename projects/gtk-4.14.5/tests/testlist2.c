#include <gtk/gtk.h>

static void
row_unrevealed (GObject *revealer, GParamSpec *pspec, gpointer data)
{
  GtkWidget *row, *list;

  row = gtk_widget_get_parent (GTK_WIDGET (revealer));
  list = gtk_widget_get_parent (row);

  gtk_list_box_remove (GTK_LIST_BOX (list), row);
}

static void
remove_this_row (GtkButton *button, GtkWidget *child)
{
  GtkWidget *row, *revealer;

  row = gtk_widget_get_parent (child);
  revealer = gtk_revealer_new ();
  gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), TRUE);
  g_object_ref (child);
  gtk_box_remove (GTK_BOX (gtk_widget_get_parent (child)), child);
  gtk_revealer_set_child (GTK_REVEALER (revealer), child);
  g_object_unref (child);
  gtk_box_append (GTK_BOX (row), revealer);
  g_signal_connect (revealer, "notify::child-revealed",
                    G_CALLBACK (row_unrevealed), NULL);
  gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), FALSE);
}

static GtkWidget *create_row (const char *label);

static void
row_revealed (GObject *revealer, GParamSpec *pspec, gpointer data)
{
  GtkWidget *row, *child;

  row = gtk_widget_get_parent (GTK_WIDGET (revealer));
  child = gtk_revealer_get_child (GTK_REVEALER (revealer));
  g_object_ref (child);
  gtk_revealer_set_child (GTK_REVEALER (revealer), NULL);

  gtk_widget_unparent (GTK_WIDGET (revealer));
  gtk_box_append (GTK_BOX (row), child);
  g_object_unref (child);
}

static void
add_row_below (GtkButton *button, GtkWidget *child)
{
  GtkWidget *revealer, *row, *list;
  int index;

  row = gtk_widget_get_parent (child);
  index = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (row));
  list = gtk_widget_get_parent (row);
  row = create_row ("Extra row");
  revealer = gtk_revealer_new ();
  gtk_revealer_set_child (GTK_REVEALER (revealer), row);
  g_signal_connect (revealer, "notify::child-revealed",
                    G_CALLBACK (row_revealed), NULL);
  gtk_list_box_insert (GTK_LIST_BOX (list), revealer, index + 1);
  gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), TRUE);
}

static void
add_separator (GtkListBoxRow *row, GtkListBoxRow *before, gpointer data)
{
  if (!before)
    return;

  gtk_list_box_row_set_header (row, gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
}

static GtkWidget *
create_row (const char *text)
{
  GtkWidget *row, *label, *button;

  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  label = gtk_label_new (text);
  gtk_box_append (GTK_BOX (row), label);
  button = gtk_button_new_with_label ("x");
  gtk_widget_set_hexpand (button, TRUE);
  gtk_widget_set_halign (button, GTK_ALIGN_END);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (row), button);
  g_signal_connect (button, "clicked", G_CALLBACK (remove_this_row), row);
  button = gtk_button_new_with_label ("+");
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (row), button);
  g_signal_connect (button, "clicked", G_CALLBACK (add_row_below), row);

  return row;
}

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int main (int argc, char *argv[])
{
  GtkWidget *window, *list, *sw, *row;
  int i;
  char *text;
  gboolean done = FALSE;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 300);

  list = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
  gtk_list_box_set_header_func (GTK_LIST_BOX (list), add_separator, NULL, NULL);
  sw = gtk_scrolled_window_new ();
  gtk_window_set_child (GTK_WINDOW (window), sw);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), list);

  for (i = 0; i < 20; i++)
    {
      text = g_strdup_printf ("Row %d", i);
      row = create_row (text);
      gtk_list_box_insert (GTK_LIST_BOX (list), row, -1);
    }

  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);
  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
