/* Theming/Style Classes
 *
 * GTK uses CSS for theming. Style classes can be associated
 * with widgets to inform the theme about intended rendering.
 *
 * This demo shows some common examples where theming features
 * of GTK are used for certain effects: primary toolbars
 * and linked buttons.
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;

GtkWidget *
do_theming_style_classes (GtkWidget *do_widget)
{
  GtkWidget *grid;
  GtkBuilder *builder;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Style Classes");
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      builder = gtk_builder_new_from_resource ("/theming_style_classes/theming.ui");

      grid = (GtkWidget *)gtk_builder_get_object (builder, "grid");
      gtk_window_set_child (GTK_WINDOW (window), grid);
      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
