/* Text View/Automatic Scrolling
 * #Keywords: GtkTextView, GtkScrolledWindow
 *
 * This example demonstrates how to use the gravity of
 * GtkTextMarks to keep a text view scrolled to the bottom
 * while appending text.
 */

#include <gtk/gtk.h>

/* Scroll to the end of the buffer.
 */
static gboolean
scroll_to_end (GtkTextView *textview)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextMark *mark;
  char *spaces;
  char *text;
  static int count;

  buffer = gtk_text_view_get_buffer (textview);

  /* Get "end" mark. It's located at the end of buffer because
   * of right gravity
   */
  mark = gtk_text_buffer_get_mark (buffer, "end");
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);

  /* and insert some text at its position, the iter will be
   * revalidated after insertion to point to the end of inserted text
   */
  spaces = g_strnfill (count++, ' ');
  gtk_text_buffer_insert (buffer, &iter, "\n", -1);
  gtk_text_buffer_insert (buffer, &iter, spaces, -1);
  text = g_strdup_printf ("Scroll to end scroll to end scroll "
                          "to end scroll to end %d", count);
  gtk_text_buffer_insert (buffer, &iter, text, -1);
  g_free (spaces);
  g_free (text);

  /* Now scroll the end mark onscreen.
   */
  gtk_text_view_scroll_mark_onscreen (textview, mark);

  /* Emulate typewriter behavior, shift to the left if we
   * are far enough to the right.
   */
  if (count > 150)
    count = 0;

  return G_SOURCE_CONTINUE;
}

/* Scroll to the bottom of the buffer.
 */
static gboolean
scroll_to_bottom (GtkTextView *textview)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextMark *mark;
  char *spaces;
  char *text;
  static int count;

  buffer = gtk_text_view_get_buffer (textview);

  /* Get end iterator */
  gtk_text_buffer_get_end_iter (buffer, &iter);

  /* and insert some text at it, the iter will be revalidated
   * after insertion to point to the end of inserted text
   */
  spaces = g_strnfill (count++, ' ');
  gtk_text_buffer_insert (buffer, &iter, "\n", -1);
  gtk_text_buffer_insert (buffer, &iter, spaces, -1);
  text = g_strdup_printf ("Scroll to bottom scroll to bottom scroll "
                          "to bottom scroll to bottom %d", count);
  gtk_text_buffer_insert (buffer, &iter, text, -1);
  g_free (spaces);
  g_free (text);

  /* Move the iterator to the beginning of line, so we don't scroll
   * in horizontal direction
   */
  gtk_text_iter_set_line_offset (&iter, 0);

  /* and place the mark at iter. the mark will stay there after we
   * insert some text at the end because it has left gravity.
   */
  mark = gtk_text_buffer_get_mark (buffer, "scroll");
  gtk_text_buffer_move_mark (buffer, mark, &iter);

  /* Scroll the mark onscreen.
   */
  gtk_text_view_scroll_mark_onscreen (textview, mark);

  /* Shift text back if we got enough to the right.
   */
  if (count > 40)
    count = 0;

  return G_SOURCE_CONTINUE;
}

static guint
setup_scroll (GtkTextView *textview,
              gboolean     to_end)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;

  buffer = gtk_text_view_get_buffer (textview);
  gtk_text_buffer_get_end_iter (buffer, &iter);

  if (to_end)
    {
      /* If we want to scroll to the end, including horizontal scrolling,
       * then we just create a mark with right gravity at the end of the
       * buffer. It will stay at the end unless explicitly moved with
       * gtk_text_buffer_move_mark.
       */
      gtk_text_buffer_create_mark (buffer, "end", &iter, FALSE);

      /* Add scrolling timeout. */
      return g_timeout_add (50, (GSourceFunc) scroll_to_end, textview);
    }
  else
    {
      /* If we want to scroll to the bottom, but not scroll horizontally,
       * then an end mark won't do the job. Just create a mark so we can
       * use it with gtk_text_view_scroll_mark_onscreen, we'll position it
       * explicitly when needed. Use left gravity so the mark stays where
       * we put it after inserting new text.
       */
      gtk_text_buffer_create_mark (buffer, "scroll", &iter, TRUE);

      /* Add scrolling timeout. */
      return g_timeout_add (100, (GSourceFunc) scroll_to_bottom, textview);
    }
}

static void
remove_timeout (GtkWidget *window,
                gpointer   timeout)
{
  g_source_remove (GPOINTER_TO_UINT (timeout));
}

static void
create_text_view (GtkWidget *hbox,
                  gboolean   to_end)
{
  GtkWidget *swindow;
  GtkWidget *textview;
  guint timeout;

  swindow = gtk_scrolled_window_new ();
  gtk_box_append (GTK_BOX (hbox), swindow);
  textview = gtk_text_view_new ();
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (swindow), textview);

  timeout = setup_scroll (GTK_TEXT_VIEW (textview), to_end);

  /* Remove the timeout in destroy handler, so we don't try to
   * scroll destroyed widget.
   */
  g_signal_connect (textview, "destroy",
                    G_CALLBACK (remove_timeout),
                    GUINT_TO_POINTER (timeout));
}

GtkWidget *
do_textscroll (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *hbox;

      window = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window), "Automatic Scrolling");
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);
      gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
      gtk_box_set_homogeneous (GTK_BOX (hbox), TRUE);
      gtk_window_set_child (GTK_WINDOW (window), hbox);

      create_text_view (hbox, TRUE);
      create_text_view (hbox, FALSE);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
