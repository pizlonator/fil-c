/* Header Bar
 * #Keywords: GtkWindowHandle, GtkWindowControls
 *
 * GtkHeaderBar is a container that is suitable for implementing
 * window titlebars. One of its features is that it can position
 * a title centered with regard to the full width, regardless of
 * variable-width content at the left or right.
 *
 * It is commonly used with gtk_window_set_titlebar()
 */

#include <gtk/gtk.h>

GtkWidget *
do_headerbar (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *header;
  GtkWidget *button;
  GtkWidget *box;
  GtkWidget *content;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),  gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Welcome to the Hotel California");
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);

      header = gtk_header_bar_new ();

      button = gtk_button_new_from_icon_name ("mail-send-receive-symbolic");
      gtk_widget_set_tooltip_text (button, "Check out");
      gtk_header_bar_pack_end (GTK_HEADER_BAR (header), button);

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_widget_add_css_class (box, "linked");
      button = gtk_button_new_from_icon_name ("go-previous-symbolic");
      gtk_widget_set_tooltip_text (button, "Back");
      gtk_box_append (GTK_BOX (box), button);
      button = gtk_button_new_from_icon_name ("go-next-symbolic");
      gtk_widget_set_tooltip_text (button, "Forward");
      gtk_box_append (GTK_BOX (box), button);

      gtk_header_bar_pack_start (GTK_HEADER_BAR (header), box);
      button = gtk_switch_new ();
      gtk_accessible_update_property (GTK_ACCESSIBLE (button),
                                      GTK_ACCESSIBLE_PROPERTY_LABEL, "Change something",
                                      -1);
      gtk_header_bar_pack_start (GTK_HEADER_BAR (header), button);

      gtk_window_set_titlebar (GTK_WINDOW (window), header);

      content = gtk_text_view_new ();
      gtk_accessible_update_property (GTK_ACCESSIBLE (content),
                                      GTK_ACCESSIBLE_PROPERTY_LABEL, "Content",
                                      -1);
      gtk_window_set_child (GTK_WINDOW (window), content);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
