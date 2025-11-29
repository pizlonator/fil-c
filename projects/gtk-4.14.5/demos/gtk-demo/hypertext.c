/* Text View/Hypertext
 * #Keywords: GtkTextView, GtkTextBuffer
 *
 * Usually, tags modify the appearance of text in the view, e.g. making it
 * bold or colored or underlined. But tags are not restricted to appearance.
 * They can also affect the behavior of mouse and key presses, as this demo
 * shows.
 *
 * We also demonstrate adding other things to a text view, such as
 * clickable icons and widgets which can also replace a character
 * (try copying the ghost text).
 */

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

/* Inserts a piece of text into the buffer, giving it the usual
 * appearance of a hyperlink in a web browser: blue and underlined.
 * Additionally, attaches some data on the tag, to make it recognizable
 * as a link.
 */
static void
insert_link (GtkTextBuffer *buffer,
             GtkTextIter   *iter,
             const char    *text,
             int            page)
{
  GtkTextTag *tag;

  tag = gtk_text_buffer_create_tag (buffer, NULL,
                                    "foreground", "blue",
                                    "underline", PANGO_UNDERLINE_SINGLE,
                                    NULL);
  g_object_set_data (G_OBJECT (tag), "page", GINT_TO_POINTER (page));
  gtk_text_buffer_insert_with_tags (buffer, iter, text, -1, tag, NULL);
}

/* Quick-and-dirty text-to-speech for a single word. If you don't hear
 * anything, you are missing espeak-ng on your system.
 */
static void
say_word (GtkGestureClick *gesture,
          guint            n_press,
          double           x,
          double           y,
          const char      *word)
{
  const char *argv[3];

  argv[0] = "espeak-ng";
  argv[1] = word;
  argv[2] = NULL;

  g_spawn_async (NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
}

/* Fills the buffer with text and interspersed links. In any real
 * hypertext app, this method would parse a file to identify the links.
 */
static void
show_page (GtkTextView *text_view,
           int          page)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter, start;
  GtkTextMark *mark;
  GtkWidget *child;
  GtkTextChildAnchor *anchor;
  GtkEventController *controller;
  GtkTextTag *bold, *mono, *nobreaks;

  buffer = gtk_text_view_get_buffer (text_view);

  bold = gtk_text_buffer_create_tag (buffer, NULL,
                                     "weight", PANGO_WEIGHT_BOLD,
                                     "scale", PANGO_SCALE_X_LARGE,
                                     NULL);
  mono = gtk_text_buffer_create_tag (buffer, NULL,
                                     "family", "monospace",
                                     NULL);
  nobreaks = gtk_text_buffer_create_tag (buffer, NULL,
                                         "allow-breaks", FALSE,
                                         NULL);

  gtk_text_buffer_set_text (buffer, "", 0);
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);
  gtk_text_buffer_begin_irreversible_action (buffer);
  if (page == 1)
    {
      GtkIconPaintable *icon;
      GtkIconTheme *theme;

      gtk_text_buffer_insert (buffer, &iter, "Some text to show that simple ", -1);
      insert_link (buffer, &iter, "hypertext", 3);
      gtk_text_buffer_insert (buffer, &iter, " can easily be realized with ", -1);
      insert_link (buffer, &iter, "tags", 2);
      gtk_text_buffer_insert (buffer, &iter, ".\n", -1);
      gtk_text_buffer_insert (buffer, &iter, "Of course you can also embed Emoji 😋, ", -1);
      gtk_text_buffer_insert (buffer, &iter, "icons ", -1);

      theme = gtk_icon_theme_get_for_display (gtk_widget_get_display (GTK_WIDGET (text_view)));
      icon = gtk_icon_theme_lookup_icon (theme,
                                         "view-conceal-symbolic",
                                         NULL,
                                         16,
                                         1,
                                         GTK_TEXT_DIR_LTR,
                                         0);
      gtk_text_buffer_insert_paintable (buffer, &iter, GDK_PAINTABLE (icon));
      g_object_unref (icon);
      gtk_text_buffer_insert (buffer, &iter, ", or even widgets ", -1);
      anchor = gtk_text_buffer_create_child_anchor (buffer, &iter);
      child = gtk_level_bar_new_for_interval (0, 100);
      gtk_level_bar_set_value (GTK_LEVEL_BAR (child), 50);
      gtk_widget_set_size_request (child, 100, -1);
      gtk_text_view_add_child_at_anchor (text_view, child, anchor);
      gtk_text_buffer_insert (buffer, &iter, " and labels with ", -1);
      anchor = gtk_text_child_anchor_new_with_replacement ("👻");
      gtk_text_buffer_insert_child_anchor (buffer, &iter, anchor);
      child = gtk_label_new ("ghost");
      gtk_text_view_add_child_at_anchor (text_view, child, anchor);
      gtk_text_buffer_insert (buffer, &iter, " text.", -1);
    }
  else if (page == 2)
    {
      mark = gtk_text_buffer_create_mark (buffer, "mark", &iter, TRUE);

      gtk_text_buffer_insert_with_tags (buffer, &iter, "tag", -1, bold, NULL);
      gtk_text_buffer_insert (buffer, &iter, " /", -1);

      gtk_text_buffer_get_iter_at_mark (buffer, &start, mark);
      gtk_text_buffer_apply_tag (buffer, nobreaks, &start, &iter);
      gtk_text_buffer_insert (buffer, &iter, " ", -1);

      gtk_text_buffer_move_mark (buffer, mark, &iter);
      gtk_text_buffer_insert_with_tags (buffer, &iter, "tag", -1, mono, NULL);
      gtk_text_buffer_insert (buffer, &iter, " /", -1);

      gtk_text_buffer_get_iter_at_mark (buffer, &start, mark);
      gtk_text_buffer_apply_tag (buffer, nobreaks, &start, &iter);
      gtk_text_buffer_insert (buffer, &iter, " ", -1);

      anchor = gtk_text_buffer_create_child_anchor (buffer, &iter);
      child = gtk_image_new_from_icon_name ("audio-volume-high-symbolic");
      gtk_widget_set_cursor_from_name (child, "pointer");
      controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
      g_signal_connect (controller, "pressed", G_CALLBACK (say_word), (gpointer)"tag");
      gtk_widget_add_controller (child, controller);
      gtk_text_view_add_child_at_anchor (text_view, child, anchor);

      gtk_text_buffer_insert (buffer, &iter, "\n"
          "An attribute that can be applied to some range of text. For example, "
          "a tag might be called “bold” and make the text inside the tag bold.\n"
          "However, the tag concept is more general than that; "
          "tags don't have to affect appearance. They can instead affect the "
          "behavior of mouse and key presses, “lock” a range of text so the "
          "user can't edit it, or countless other things.\n", -1);
      insert_link (buffer, &iter, "Go back", 1);

      gtk_text_buffer_delete_mark (buffer, mark);
    }
  else if (page == 3)
    {
      mark = gtk_text_buffer_create_mark (buffer, "mark", &iter, TRUE);

      gtk_text_buffer_insert_with_tags (buffer, &iter, "hypertext", -1, bold, NULL);
      gtk_text_buffer_insert (buffer, &iter, " /", -1);

      gtk_text_buffer_get_iter_at_mark (buffer, &start, mark);
      gtk_text_buffer_apply_tag (buffer, nobreaks, &start, &iter);
      gtk_text_buffer_insert (buffer, &iter, " ", -1);

      gtk_text_buffer_move_mark (buffer, mark, &iter);
      gtk_text_buffer_insert_with_tags (buffer, &iter, "ˈhaɪ pərˌtɛkst", -1, mono, NULL);
      gtk_text_buffer_insert (buffer, &iter, " /", -1);
      gtk_text_buffer_get_iter_at_mark (buffer, &start, mark);
      gtk_text_buffer_apply_tag (buffer, nobreaks, &start, &iter);
      gtk_text_buffer_insert (buffer, &iter, " ", -1);

      anchor = gtk_text_buffer_create_child_anchor (buffer, &iter);
      child = gtk_image_new_from_icon_name ("audio-volume-high-symbolic");
      gtk_widget_set_cursor_from_name (child, "pointer");
      controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
      g_signal_connect (controller, "pressed", G_CALLBACK (say_word), (gpointer)"hypertext");
      gtk_widget_add_controller (child, controller);
      gtk_text_view_add_child_at_anchor (text_view, child, anchor);

      gtk_text_buffer_insert (buffer, &iter, "\n"
          "Machine-readable text that is not sequential but is organized "
          "so that related items of information are connected.\n", -1);
      insert_link (buffer, &iter, "Go back", 1);

      gtk_text_buffer_delete_mark (buffer, mark);
    }
  gtk_text_buffer_end_irreversible_action (buffer);
}

/* Looks at all tags covering the position of iter in the text view,
 * and if one of them is a link, follow it by showing the page identified
 * by the data attached to it.
 */
static void
follow_if_link (GtkWidget   *text_view,
                GtkTextIter *iter)
{
  GSList *tags = NULL, *tagp = NULL;

  tags = gtk_text_iter_get_tags (iter);
  for (tagp = tags;  tagp != NULL;  tagp = tagp->next)
    {
      GtkTextTag *tag = tagp->data;
      int page = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (tag), "page"));

      if (page != 0)
        {
          show_page (GTK_TEXT_VIEW (text_view), page);
          break;
        }
    }

  if (tags)
    g_slist_free (tags);
}

/* Links can be activated by pressing Enter.
 */
static gboolean
key_pressed (GtkEventController *controller,
             guint               keyval,
             guint               keycode,
             GdkModifierType     modifiers,
             GtkWidget          *text_view)
{
  GtkTextIter iter;
  GtkTextBuffer *buffer;

  switch (keyval)
    {
      case GDK_KEY_Return:
      case GDK_KEY_KP_Enter:
        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
        gtk_text_buffer_get_iter_at_mark (buffer, &iter,
                                          gtk_text_buffer_get_insert (buffer));
        follow_if_link (text_view, &iter);
        break;

      default:
        break;
    }

  return GDK_EVENT_PROPAGATE;
}

static void set_cursor_if_appropriate (GtkTextView *text_view,
                                       int          x,
                                       int          y);

static void
released_cb (GtkGestureClick *gesture,
             guint            n_press,
             double           x,
             double           y,
             GtkWidget       *text_view)
{
  GtkTextIter start, end, iter;
  GtkTextBuffer *buffer;
  int tx, ty;

  if (gtk_gesture_single_get_button (GTK_GESTURE_SINGLE (gesture)) > 1)
    return;

  gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (text_view),
                                         GTK_TEXT_WINDOW_WIDGET,
                                         x, y, &tx, &ty);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));

  /* we shouldn't follow a link if the user has selected something */
  gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
  if (gtk_text_iter_get_offset (&start) != gtk_text_iter_get_offset (&end))
    return;

  if (gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (text_view), &iter, tx, ty))
    follow_if_link (text_view, &iter);
}

static void
motion_cb (GtkEventControllerMotion *controller,
           double                    x,
           double                    y,
           GtkTextView              *text_view)
{
  int tx, ty;

  gtk_text_view_window_to_buffer_coords (text_view,
                                         GTK_TEXT_WINDOW_WIDGET,
                                         x, y, &tx, &ty);

  set_cursor_if_appropriate (text_view, tx, ty);
}

static gboolean hovering_over_link = FALSE;

/* Looks at all tags covering the position (x, y) in the text view,
 * and if one of them is a link, change the cursor to the "hands" cursor
 * typically used by web browsers.
 */
static void
set_cursor_if_appropriate (GtkTextView *text_view,
                           int          x,
                           int          y)
{
  GSList *tags = NULL, *tagp = NULL;
  GtkTextIter iter;
  gboolean hovering = FALSE;

  if (gtk_text_view_get_iter_at_location (text_view, &iter, x, y))
    {
      tags = gtk_text_iter_get_tags (&iter);
      for (tagp = tags;  tagp != NULL;  tagp = tagp->next)
        {
          GtkTextTag *tag = tagp->data;
          int page = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (tag), "page"));

          if (page != 0)
            {
              hovering = TRUE;
              break;
            }
        }
    }

  if (hovering != hovering_over_link)
    {
      hovering_over_link = hovering;

      if (hovering_over_link)
        gtk_widget_set_cursor_from_name (GTK_WIDGET (text_view), "pointer");
      else
        gtk_widget_set_cursor_from_name (GTK_WIDGET (text_view), "text");
    }

  if (tags)
    g_slist_free (tags);
}

GtkWidget *
do_hypertext (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *view;
      GtkWidget *sw;
      GtkTextBuffer *buffer;
      GtkEventController *controller;

      window = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window), "Hypertext");
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_default_size (GTK_WINDOW (window), 330, 330);
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      view = gtk_text_view_new ();
      gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (view), GTK_WRAP_WORD);
      gtk_text_view_set_top_margin (GTK_TEXT_VIEW (view), 20);
      gtk_text_view_set_bottom_margin (GTK_TEXT_VIEW (view), 20);
      gtk_text_view_set_left_margin (GTK_TEXT_VIEW (view), 20);
      gtk_text_view_set_right_margin (GTK_TEXT_VIEW (view), 20);
      gtk_text_view_set_pixels_below_lines (GTK_TEXT_VIEW (view), 10);
      controller = gtk_event_controller_key_new ();
      g_signal_connect (controller, "key-pressed", G_CALLBACK (key_pressed), view);
      gtk_widget_add_controller (view, controller);

      controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
      g_signal_connect (controller, "released",
                        G_CALLBACK (released_cb), view);
      gtk_widget_add_controller (view, controller);

      controller = gtk_event_controller_motion_new ();
      g_signal_connect (controller, "motion",
                        G_CALLBACK (motion_cb), view);
      gtk_widget_add_controller (view, controller);

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
      gtk_text_buffer_set_enable_undo (buffer, TRUE);

      sw = gtk_scrolled_window_new ();
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_NEVER,
                                      GTK_POLICY_AUTOMATIC);
      gtk_window_set_child (GTK_WINDOW (window), sw);
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), view);

      show_page (GTK_TEXT_VIEW (view), 1);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
