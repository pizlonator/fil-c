#include <gtk/gtk.h>

int main (int argc, char *argv[])
{
  GtkWidget *window, *sw, *viewport, *grid;
  GtkWidget *entry;
  int i, j;
  char *text;

  gtk_init ();

  window = gtk_window_new ();
  sw = gtk_scrolled_window_new ();
  gtk_window_set_child (GTK_WINDOW (window), sw);
  viewport = gtk_viewport_new (gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (sw)),
                               gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (sw)));
  gtk_viewport_set_scroll_to_focus (GTK_VIEWPORT (viewport), TRUE);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), viewport);
  grid = gtk_grid_new ();
  gtk_widget_set_margin_start (grid, 20);
  gtk_widget_set_margin_end (grid, 20);
  gtk_widget_set_margin_top (grid, 20);
  gtk_widget_set_margin_bottom (grid, 20);
  gtk_viewport_set_child (GTK_VIEWPORT (viewport), grid);

  for (i = 0; i < 20; i++)
    for (j = 0; j < 20; j++)
      {
        entry = gtk_entry_new ();
        text = g_strdup_printf ("(%d, %d)", i, j);
        gtk_editable_set_text (GTK_EDITABLE (entry), text);
        g_free (text);
        gtk_editable_set_width_chars (GTK_EDITABLE (entry), 6);
        gtk_grid_attach (GTK_GRID (grid), entry, i, j, 1, 1);
      }
 
  gtk_window_present (GTK_WINDOW (window));

  while (1)
    g_main_context_iteration (NULL, FALSE);

  return 0;
}
