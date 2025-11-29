/* Theming/CSS Accordion
 *
 * A simple accordion demo written using CSS transitions and multiple backgrounds
 *
 */

#include <gtk/gtk.h>

static void
destroy_provider (GtkWidget        *window,
                  GtkStyleProvider *provider)
{
  gtk_style_context_remove_provider_for_display (gtk_widget_get_display (window), provider);
}

GtkWidget *
do_css_accordion (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *container, *styled_box, *child;
      GtkCssProvider *provider;

      window = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window), "CSS Accordion");
      gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (do_widget));
      gtk_window_set_default_size (GTK_WINDOW (window), 600, 300);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      styled_box = gtk_frame_new (NULL);
      gtk_window_set_child (GTK_WINDOW (window), styled_box);
      gtk_widget_add_css_class (styled_box, "accordion");
      container = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_widget_set_halign (container, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (container, GTK_ALIGN_CENTER);
      gtk_frame_set_child (GTK_FRAME (styled_box), container);

      child = gtk_button_new_with_label ("This");
      gtk_box_append (GTK_BOX (container), child);

      child = gtk_button_new_with_label ("Is");
      gtk_box_append (GTK_BOX (container), child);

      child = gtk_button_new_with_label ("A");
      gtk_box_append (GTK_BOX (container), child);

      child = gtk_button_new_with_label ("CSS");
      gtk_box_append (GTK_BOX (container), child);

      child = gtk_button_new_with_label ("Accordion");
      gtk_box_append (GTK_BOX (container), child);

      child = gtk_button_new_with_label (":-)");
      gtk_box_append (GTK_BOX (container), child);

      provider = gtk_css_provider_new ();
      gtk_css_provider_load_from_resource (provider, "/css_accordion/css_accordion.css");

      gtk_style_context_add_provider_for_display (gtk_widget_get_display (window),
                                                  GTK_STYLE_PROVIDER (provider),
                                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

      g_signal_connect (window, "destroy",
                        G_CALLBACK (destroy_provider), provider);
      g_object_unref (provider);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
