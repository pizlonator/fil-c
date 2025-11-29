/* Overlay/Transparency
 * #Keywords: GtkOverlay, GtkSnapshot, blur
 *
 * Blur the background behind an overlay.
 */

#include <gtk/gtk.h>
#include "bluroverlay.h"

GtkWidget *
do_transparent (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *overlay;
      GtkWidget *button;
      GtkWidget *label;
      GtkWidget *picture;

      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_default_size (GTK_WINDOW (window), 450, 450);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      gtk_window_set_title (GTK_WINDOW (window), "Transparency");

      overlay = blur_overlay_new ();
      gtk_window_set_child (GTK_WINDOW (window), overlay);

      button = gtk_button_new_with_label ("Don't click this button!");
      label = gtk_button_get_child (GTK_BUTTON (button));
      gtk_widget_set_margin_start (label, 50);
      gtk_widget_set_margin_end (label, 50);
      gtk_widget_set_margin_top (label, 50);
      gtk_widget_set_margin_bottom (label, 50);

      gtk_widget_set_opacity (button, 0.7);
      gtk_widget_set_halign (button, GTK_ALIGN_FILL);
      gtk_widget_set_valign (button, GTK_ALIGN_START);

      blur_overlay_add_overlay (BLUR_OVERLAY (overlay), button, 5.0);

      button = gtk_button_new_with_label ("Maybe this one?");
      label = gtk_button_get_child (GTK_BUTTON (button));
      gtk_widget_set_margin_start (label, 50);
      gtk_widget_set_margin_end (label, 50);
      gtk_widget_set_margin_top (label, 50);
      gtk_widget_set_margin_bottom (label, 50);

      gtk_widget_set_opacity (button, 0.7);
      gtk_widget_set_halign (button, GTK_ALIGN_FILL);
      gtk_widget_set_valign (button, GTK_ALIGN_END);

      blur_overlay_add_overlay (BLUR_OVERLAY (overlay), button, 5.0);

      picture = gtk_picture_new_for_resource ("/transparent/portland-rose.jpg");
      blur_overlay_set_child (BLUR_OVERLAY (overlay), picture);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
