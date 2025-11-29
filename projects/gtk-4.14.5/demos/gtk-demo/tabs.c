/* Text View/Tabs
 *
 * GtkTextView can position text at fixed positions, using tabs.
 * Tabs can specify alignment, and also allow aligning numbers
 * on the decimal point.
 *
 * The example here has three tabs, with left, numeric and right
 * alignment.
 */

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

GtkWidget *
do_tabs (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *view;
      GtkWidget *sw;
      GtkTextBuffer *buffer;
      PangoTabArray *tabs;

      window = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window), "Tabs");
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_default_size (GTK_WINDOW (window), 330, 130);
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      view = gtk_text_view_new ();
      gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (view), GTK_WRAP_WORD);
      gtk_text_view_set_top_margin (GTK_TEXT_VIEW (view), 20);
      gtk_text_view_set_bottom_margin (GTK_TEXT_VIEW (view), 20);
      gtk_text_view_set_left_margin (GTK_TEXT_VIEW (view), 20);
      gtk_text_view_set_right_margin (GTK_TEXT_VIEW (view), 20);

      tabs = pango_tab_array_new (3, TRUE);
      pango_tab_array_set_tab (tabs, 0, PANGO_TAB_LEFT, 0);
      pango_tab_array_set_tab (tabs, 1, PANGO_TAB_DECIMAL, 150);
      pango_tab_array_set_decimal_point (tabs, 1, '.');
      pango_tab_array_set_tab (tabs, 2, PANGO_TAB_RIGHT, 290);
      gtk_text_view_set_tabs (GTK_TEXT_VIEW (view), tabs);
      pango_tab_array_free (tabs);

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
      gtk_text_buffer_set_text (buffer, "one\t2.0\tthree\nfour\t5.555\tsix\nseven\t88.88\tnine", -1);

      sw = gtk_scrolled_window_new ();
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_NEVER,
                                      GTK_POLICY_AUTOMATIC);
      gtk_window_set_child (GTK_WINDOW (window), sw);
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), view);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
