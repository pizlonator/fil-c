/* Overlay/Interactive Overlay
 * #Keywords: GtkOverlay
 *
 * Shows widgets in static positions over a main widget.
 *
 * The overlaid widgets can be interactive controls such
 * as the entry in this example, or just decorative, like
 * the big blue label.
 */

#include <gtk/gtk.h>

static void
do_number (GtkButton *button, GtkEntry *entry)
{
  gtk_editable_set_text (GTK_EDITABLE (entry), gtk_button_get_label (button));
}

GtkWidget *
do_overlay (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *overlay;
      GtkWidget *grid;
      GtkWidget *button;
      GtkWidget *vbox;
      GtkWidget *label;
      GtkWidget *entry;
      int i, j;
      char *text;

      window = gtk_window_new ();
      gtk_window_set_default_size (GTK_WINDOW (window), 500, 510);
      gtk_window_set_title (GTK_WINDOW (window), "Interactive Overlay");

      overlay = gtk_overlay_new ();
      grid = gtk_grid_new ();
      gtk_overlay_set_child (GTK_OVERLAY (overlay), grid);

      entry = gtk_entry_new ();

      for (j = 0; j < 5; j++)
        {
          for (i = 0; i < 5; i++)
            {
              text = g_strdup_printf ("%d", 5*j + i);
              button = gtk_button_new_with_label (text);
              g_free (text);
              gtk_widget_set_hexpand (button, TRUE);
              gtk_widget_set_vexpand (button, TRUE);
              g_signal_connect (button, "clicked", G_CALLBACK (do_number), entry);
              gtk_grid_attach (GTK_GRID (grid), button, i, j, 1, 1);
            }
        }

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_widget_set_can_target (vbox, FALSE);
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), vbox);
      gtk_widget_set_halign (vbox, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (vbox, GTK_ALIGN_START);

      label = gtk_label_new ("<span foreground='blue' weight='ultrabold' font='40'>Numbers</span>");
      gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
      gtk_widget_set_can_target (label, FALSE);
      gtk_widget_set_margin_top (label, 8);
      gtk_widget_set_margin_bottom (label, 8);
      gtk_box_append (GTK_BOX (vbox), label);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), vbox);
      gtk_widget_set_halign (vbox, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (vbox, GTK_ALIGN_CENTER);

      gtk_entry_set_placeholder_text (GTK_ENTRY (entry), "Your Lucky Number");
      gtk_widget_set_margin_top (entry, 8);
      gtk_widget_set_margin_bottom (entry, 8);
      gtk_box_append (GTK_BOX (vbox), entry);

      gtk_window_set_child (GTK_WINDOW (window), overlay);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
