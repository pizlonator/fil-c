/* Masking
 *
 * Demonstrates mask nodes.
 *
 * This demo uses a text node as mask for
 * an animated linear gradient.
 */

#include <gtk/gtk.h>
#include "demo4widget.h"


GtkWidget *
do_mask (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *box;
      GtkWidget *demo;
      GtkWidget *scale;

      window = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window), "Mask Nodes");
      gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_window_set_child (GTK_WINDOW (window), box);

      demo = demo4_widget_new ();
      gtk_widget_set_hexpand (demo, TRUE);
      gtk_widget_set_vexpand (demo, TRUE);

      gtk_box_append (GTK_BOX (box), demo);

      scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 1, 0.1);
      gtk_range_set_value (GTK_RANGE (scale), 0.5);
      g_object_bind_property (gtk_range_get_adjustment (GTK_RANGE (scale)), "value", demo, "progress", 0);

      gtk_box_append (GTK_BOX (box), scale);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
