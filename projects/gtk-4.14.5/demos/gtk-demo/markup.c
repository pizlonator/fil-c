/* Text View/Markup
 * #Keywords: GtkTextView
 *
 * GtkTextBuffer lets you define your own tags that can influence
 * text formatting in a variety of ways. In this example, we show
 * that GtkTextBuffer can load Pango markup and automatically generate
 * suitable tags.
 */

#include <gtk/gtk.h>

static GtkWidget *stack;
static GtkWidget *view;
static GtkWidget *view2;

static void
source_toggled (GtkCheckButton *button)
{
  if (gtk_check_button_get_active (button))
    gtk_stack_set_visible_child_name (GTK_STACK (stack), "source");
  else
    {
      GtkTextBuffer *buffer;
      GtkTextIter start, end;
      char *markup;

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view2));
      gtk_text_buffer_get_bounds (buffer, &start, &end);
      markup = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
      gtk_text_buffer_get_bounds (buffer, &start, &end);
      gtk_text_buffer_begin_irreversible_action (buffer);
      gtk_text_buffer_delete (buffer, &start, &end);
      gtk_text_buffer_insert_markup (buffer, &start, markup, -1);
      gtk_text_buffer_end_irreversible_action (buffer);
      g_free (markup);

      gtk_stack_set_visible_child_name (GTK_STACK (stack), "formatted");
    }
}

GtkWidget *
do_markup (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *sw;
      GtkTextBuffer *buffer;
      GtkTextIter iter;
      GBytes *bytes;
      const char *markup;
      GtkWidget *header;
      GtkWidget *show_source;

      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_default_size (GTK_WINDOW (window), 600, 680);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      stack = gtk_stack_new ();
      gtk_window_set_child (GTK_WINDOW (window), stack);

      show_source = gtk_check_button_new_with_label ("Source");
      gtk_widget_set_valign (show_source, GTK_ALIGN_CENTER);
      g_signal_connect (show_source, "toggled", G_CALLBACK (source_toggled), stack);

      header = gtk_header_bar_new ();
      gtk_header_bar_pack_start (GTK_HEADER_BAR (header), show_source);
      gtk_window_set_titlebar (GTK_WINDOW (window), header);

      gtk_window_set_title (GTK_WINDOW (window), "Markup");

      view = gtk_text_view_new ();
      gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);
      gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (view), GTK_WRAP_WORD_CHAR);
      gtk_text_view_set_left_margin (GTK_TEXT_VIEW (view), 10);
      gtk_text_view_set_right_margin (GTK_TEXT_VIEW (view), 10);

      sw = gtk_scrolled_window_new ();
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), view);

      gtk_stack_add_named (GTK_STACK (stack), sw, "formatted");

      view2 = gtk_text_view_new ();
      gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (view2), GTK_WRAP_WORD);
      gtk_text_view_set_left_margin (GTK_TEXT_VIEW (view2), 10);
      gtk_text_view_set_right_margin (GTK_TEXT_VIEW (view2), 10);

      sw = gtk_scrolled_window_new ();
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), view2);

      gtk_stack_add_named (GTK_STACK (stack), sw, "source");

      bytes = g_resources_lookup_data ("/markup/markup.txt", 0, NULL);
      markup = (const char *)g_bytes_get_data (bytes, NULL);

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
      gtk_text_buffer_get_start_iter (buffer, &iter);
      gtk_text_buffer_begin_irreversible_action (buffer);
      gtk_text_buffer_insert_markup (buffer, &iter, markup, -1);
      gtk_text_buffer_end_irreversible_action (buffer);

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view2));
      gtk_text_buffer_get_start_iter (buffer, &iter);
      gtk_text_buffer_begin_irreversible_action (buffer);
      gtk_text_buffer_insert (buffer, &iter, markup, -1);
      gtk_text_buffer_end_irreversible_action (buffer);

      g_bytes_unref (bytes);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
