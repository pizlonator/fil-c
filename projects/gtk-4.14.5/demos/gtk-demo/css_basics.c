/* Theming/CSS Basics
 *
 * GTK themes are written using CSS. Every widget is build of multiple items
 * that you can style very similarly to a regular website.
 */

#include <gtk/gtk.h>

static void
show_parsing_error (GtkCssProvider *provider,
                    GtkCssSection  *section,
                    const GError   *error,
                    GtkTextBuffer  *buffer)
{
  const GtkCssLocation *start_location, *end_location;
  GtkTextIter start, end;
  const char *tag_name;

  start_location = gtk_css_section_get_start_location (section);
  gtk_text_buffer_get_iter_at_line_index (buffer,
                                          &start,
                                          start_location->lines,
                                          start_location->line_bytes);
  end_location = gtk_css_section_get_end_location (section);
  gtk_text_buffer_get_iter_at_line_index (buffer,
                                          &end,
                                          end_location->lines,
                                          end_location->line_bytes);

  if (error->domain == GTK_CSS_PARSER_WARNING)
    tag_name = "warning";
  else
    tag_name = "error";

  gtk_text_buffer_apply_tag_by_name (buffer, tag_name, &start, &end);
}

static void
css_text_changed (GtkTextBuffer  *buffer,
                  GtkCssProvider *provider)
{
  GtkTextIter start, end;
  char *text;

  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_get_end_iter (buffer, &end);
  gtk_text_buffer_remove_all_tags (buffer, &start, &end);

  text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
  gtk_css_provider_load_from_string (provider, text);
  g_free (text);
}

static void
clear_provider (gpointer data)
{
  GtkStyleProvider *provider = data;

  gtk_style_context_remove_provider_for_display (gdk_display_get_default (), provider);
}

static void
apply_css (GtkWidget *widget, GtkStyleProvider *provider)
{
  gtk_style_context_add_provider_for_display (gdk_display_get_default (), provider, G_MAXUINT);
  g_object_set_data_full (G_OBJECT (widget), "provider", provider, clear_provider);
}

GtkWidget *
do_css_basics (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *container, *child;
      GtkStyleProvider *provider;
      GtkTextBuffer *text;
      GBytes *bytes;

      window = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window), "CSS Basics");
      gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (do_widget));
      gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);
      gtk_widget_add_css_class (window, "demo");
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      text = gtk_text_buffer_new (NULL);
      gtk_text_buffer_create_tag (text,
                                  "warning",
                                  "underline", PANGO_UNDERLINE_SINGLE,
                                  NULL);
      gtk_text_buffer_create_tag (text,
                                  "error",
                                  "underline", PANGO_UNDERLINE_ERROR,
                                  NULL);

      provider = GTK_STYLE_PROVIDER (gtk_css_provider_new ());

      container = gtk_scrolled_window_new ();
      gtk_window_set_child (GTK_WINDOW (window), container);
      child = gtk_text_view_new_with_buffer (text);
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (container), child);
      g_signal_connect (text, "changed",
                        G_CALLBACK (css_text_changed), provider);

      bytes = g_resources_lookup_data ("/css_basics/css_basics.css", 0, NULL);
      gtk_text_buffer_set_text (text, g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes));
      g_bytes_unref (bytes);

      g_signal_connect (provider,
                        "parsing-error",
                        G_CALLBACK (show_parsing_error),
                        gtk_text_view_get_buffer (GTK_TEXT_VIEW (child)));

      apply_css (window, provider);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
