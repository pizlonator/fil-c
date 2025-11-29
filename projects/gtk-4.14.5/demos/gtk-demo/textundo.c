/* Text View/Undo and Redo
 *
 * The GtkTextView supports undo and redo through the use of a
 * GtkTextBuffer. You can enable or disable undo support using
 * gtk_text_buffer_set_enable_undo().
 *
 * Use Control+z to undo and Control+Shift+z or Control+y to
 * redo previously undone operations.
 */

#include <gtk/gtk.h>
#include <stdlib.h> /* for exit() */

GtkWidget *
do_textundo (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *view;
      GtkWidget *sw;
      GtkTextBuffer *buffer;
      GtkTextIter iter;

      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_default_size (GTK_WINDOW (window), 330, 330);
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      gtk_window_set_title (GTK_WINDOW (window), "Undo and Redo");

      view = gtk_text_view_new ();
      gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (view), GTK_WRAP_WORD);
      gtk_text_view_set_pixels_below_lines (GTK_TEXT_VIEW (view), 10);
      gtk_text_view_set_left_margin (GTK_TEXT_VIEW (view), 20);
      gtk_text_view_set_right_margin (GTK_TEXT_VIEW (view), 20);
      gtk_text_view_set_top_margin (GTK_TEXT_VIEW (view), 20);
      gtk_text_view_set_bottom_margin (GTK_TEXT_VIEW (view), 20);

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
      gtk_text_buffer_set_enable_undo (buffer, TRUE);

      /* this text cannot be undone */
      gtk_text_buffer_begin_irreversible_action (buffer);
      gtk_text_buffer_get_start_iter (buffer, &iter);
      gtk_text_buffer_insert (buffer, &iter,
          "The GtkTextView supports undo and redo through the use of a "
          "GtkTextBuffer. You can enable or disable undo support using "
          "gtk_text_buffer_set_enable_undo().\n"
          "Type to add more text.\n"
          "Use Control+z to undo and Control+Shift+z or Control+y to "
          "redo previously undone operations.",
          -1);
      gtk_text_buffer_end_irreversible_action (buffer);

      sw = gtk_scrolled_window_new ();
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);
      gtk_window_set_child (GTK_WINDOW (window), sw);
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), view);
    }

  if (!gtk_widget_get_visible (window))
    {
      gtk_widget_set_visible (window, TRUE);
    }
  else
    {
      gtk_window_destroy (GTK_WINDOW (window));
      window = NULL;
    }

  return window;
}
