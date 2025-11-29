/* Cursors
 *
 * Demonstrates a useful set of available cursors. The cursors shown here are the
 * ones defined by CSS, which we assume to be available. The example shows creating
 * cursors by name or from an image, with or without a fallback.
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;

static void
on_destroy (gpointer data)
{
  window = NULL;
}

GtkWidget *
do_cursors (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkBuilder *builder;

      builder = gtk_builder_new_from_resource ("/cursors/cursors.ui");
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (on_destroy), NULL);
      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
