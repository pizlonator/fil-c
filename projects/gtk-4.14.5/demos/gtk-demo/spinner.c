/* Spinner
 *
 * GtkSpinner allows to show that background activity is on-going.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

static GtkWidget *spinner_sensitive = NULL;
static GtkWidget *spinner_unsensitive = NULL;

static void
on_play_clicked (GtkButton *button, gpointer user_data)
{
  gtk_spinner_start (GTK_SPINNER (spinner_sensitive));
  gtk_spinner_start (GTK_SPINNER (spinner_unsensitive));
}

static void
on_stop_clicked (GtkButton *button, gpointer user_data)
{
  gtk_spinner_stop (GTK_SPINNER (spinner_sensitive));
  gtk_spinner_stop (GTK_SPINNER (spinner_unsensitive));
}

GtkWidget *
do_spinner (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *button;
  GtkWidget *spinner;

  if (!window)
  {
    window = gtk_window_new ();
    gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (do_widget));
    gtk_window_set_title (GTK_WINDOW (window), "Spinner");
    gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
    g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_top (vbox, 5);
    gtk_widget_set_margin_bottom (vbox, 5);
    gtk_widget_set_margin_start (vbox, 5);
    gtk_widget_set_margin_end (vbox, 5);

    gtk_window_set_child (GTK_WINDOW (window), vbox);

    /* Sensitive */
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
    spinner = gtk_spinner_new ();
    gtk_box_append (GTK_BOX (hbox), spinner);
    gtk_box_append (GTK_BOX (hbox), gtk_entry_new ());
    gtk_box_append (GTK_BOX (vbox), hbox);
    spinner_sensitive = spinner;

    /* Disabled */
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
    spinner = gtk_spinner_new ();
    gtk_box_append (GTK_BOX (hbox), spinner);
    gtk_box_append (GTK_BOX (hbox), gtk_entry_new ());
    gtk_box_append (GTK_BOX (vbox), hbox);
    spinner_unsensitive = spinner;
    gtk_widget_set_sensitive (hbox, FALSE);

    button = gtk_button_new_with_label (_("Play"));
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_play_clicked), spinner);
    gtk_box_append (GTK_BOX (vbox), button);

    button = gtk_button_new_with_label (_("Stop"));
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_stop_clicked), spinner);
    gtk_box_append (GTK_BOX (vbox), button);

    /* Start by default to test for:
     * https://bugzilla.gnome.org/show_bug.cgi?id=598496 */
    on_play_clicked (NULL, NULL);
  }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
