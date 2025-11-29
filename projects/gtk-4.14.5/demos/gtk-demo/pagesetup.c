/* Printing/Page Setup
 * #Keywords: GtkPageSetup
 *
 * GtkPageSetupUnixDialog can be used if page setup is needed
 * independent of a full printing dialog.
 */

#include <math.h>
#include <gtk/gtk.h>
#include <gtk/gtkunixprint.h>

static void
done_cb (GtkDialog *dialog, int response, gpointer data)
{
  gtk_window_destroy (GTK_WINDOW (dialog));
}

GtkWidget *
do_pagesetup (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      window = gtk_page_setup_unix_dialog_new ("Page Setup", GTK_WINDOW (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);
      g_signal_connect (window, "response", G_CALLBACK (done_cb), NULL);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
