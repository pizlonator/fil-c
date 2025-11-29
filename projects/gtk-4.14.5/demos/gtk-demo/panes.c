/* Paned Widgets
 *
 * The GtkPaned Widget divides its content area into two panes
 * with a divider in between that the user can adjust. A separate
 * child is placed into each pane. GtkPaned widgets can be split
 * horizontally or vertically. This test contains both a horizontal
 * and a vertical GtkPaned widget.
 *
 * There are a number of options that can be set for each pane.
 * You can use the Inspector to adjust the options for each side
 * of each widget.
 */

#include <gtk/gtk.h>

GtkWidget *
do_panes (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *frame;
  GtkWidget *hpaned;
  GtkWidget *vpaned;
  GtkWidget *label;
  GtkWidget *vbox;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window), "Paned Widgets");
      gtk_window_set_default_size (GTK_WINDOW (window), 330, 250);
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      gtk_widget_set_margin_start (vbox, 8);
      gtk_widget_set_margin_end (vbox, 8);
      gtk_widget_set_margin_top (vbox, 8);
      gtk_widget_set_margin_bottom (vbox, 8);
      gtk_window_set_child (GTK_WINDOW (window), vbox);

      frame = gtk_frame_new (NULL);
      gtk_box_append (GTK_BOX (vbox), frame);

      vpaned = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
      gtk_frame_set_child (GTK_FRAME (frame), vpaned);

      hpaned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_paned_set_start_child (GTK_PANED (vpaned), hpaned);
      gtk_paned_set_shrink_start_child (GTK_PANED (vpaned), FALSE);

      label = gtk_label_new ("Hi there");
      gtk_widget_set_margin_start (label, 4);
      gtk_widget_set_margin_end (label, 4);
      gtk_widget_set_margin_top (label, 4);
      gtk_widget_set_margin_bottom (label, 4);
      gtk_widget_set_hexpand (label, TRUE);
      gtk_widget_set_vexpand (label, TRUE);
      gtk_paned_set_start_child (GTK_PANED (hpaned), label);
      gtk_paned_set_shrink_start_child (GTK_PANED (hpaned), FALSE);

      label = gtk_label_new ("Hello");
      gtk_widget_set_margin_start (label, 4);
      gtk_widget_set_margin_end (label, 4);
      gtk_widget_set_margin_top (label, 4);
      gtk_widget_set_margin_bottom (label, 4);
      gtk_widget_set_hexpand (label, TRUE);
      gtk_widget_set_vexpand (label, TRUE);
      gtk_paned_set_end_child (GTK_PANED (hpaned), label);
      gtk_paned_set_shrink_end_child (GTK_PANED (hpaned), FALSE);

      label = gtk_label_new ("Goodbye");
      gtk_widget_set_margin_start (label, 4);
      gtk_widget_set_margin_end (label, 4);
      gtk_widget_set_margin_top (label, 4);
      gtk_widget_set_margin_bottom (label, 4);
      gtk_widget_set_hexpand (label, TRUE);
      gtk_widget_set_vexpand (label, TRUE);
      gtk_paned_set_end_child (GTK_PANED (vpaned), label);
      gtk_paned_set_shrink_end_child (GTK_PANED (vpaned), FALSE);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
