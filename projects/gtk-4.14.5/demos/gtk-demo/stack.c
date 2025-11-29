/* Stack
 *
 * GtkStack is a container that shows a single child at a time,
 * with nice transitions when the visible child changes.
 *
 * GtkStackSwitcher adds buttons to control which child is visible.
 */

#include <gtk/gtk.h>

GtkWidget *
do_stack (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkBuilder *builder;

      builder = gtk_builder_new_from_resource ("/stack/stack.ui");
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window1"));
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));


  return window;
}
