#include <gtk/gtk.h>
#include <locale.h>

#include "../gtk/gtktextprivate.h"
#include "../gtk/gtktextviewprivate.h"

static void
test_text_surrounding (void)
{
  GtkWidget *widget;
  GtkEventController *controller;
  GtkIMContext *context;
  gboolean ret;
  char *text;
  int cursor_pos, selection_bound;

  widget = gtk_text_new ();
  controller = gtk_text_get_key_controller (GTK_TEXT (widget));
  context = gtk_event_controller_key_get_im_context (GTK_EVENT_CONTROLLER_KEY (controller));

  gtk_editable_set_text (GTK_EDITABLE (widget), "abcd");
  gtk_editable_set_position (GTK_EDITABLE (widget), 2);

  ret = gtk_im_context_get_surrounding_with_selection (context,
                                                       &text,
                                                       &cursor_pos,
                                                       &selection_bound);

  g_assert_true (ret);
  g_assert_cmpstr (text, ==, "abcd");
  g_assert_cmpint (cursor_pos, ==, 2);
  g_assert_cmpint (selection_bound, ==, 2);

  g_free (text);

  ret = gtk_im_context_delete_surrounding (context, -1, 1);
  g_assert_true (ret);

  g_assert_cmpstr (gtk_editable_get_text (GTK_EDITABLE (widget)), ==, "acd");
  g_assert_cmpint (gtk_editable_get_position (GTK_EDITABLE (widget)), ==, 1);

  ret = gtk_im_context_delete_surrounding (context, 1, 1);
  g_assert_true (ret);

  g_assert_cmpstr (gtk_editable_get_text (GTK_EDITABLE (widget)), ==, "ac");
  g_assert_cmpint (gtk_editable_get_position (GTK_EDITABLE (widget)), ==, 1);

  gtk_editable_set_text (GTK_EDITABLE (widget), "abcd");
  gtk_editable_select_region (GTK_EDITABLE (widget), 4, 2);

  ret = gtk_im_context_get_surrounding_with_selection (context,
                                                       &text,
                                                       &cursor_pos,
                                                       &selection_bound);

  g_assert_true (ret);
  g_assert_cmpstr (text, ==, "abcd");
  g_assert_cmpint (cursor_pos, ==, 2);
  g_assert_cmpint (selection_bound, ==, 4);

  g_free (text);

  g_object_ref_sink (widget);
  g_object_unref (widget);
}

static void
test_textview_surrounding (void)
{
  GtkWidget *widget;
  GtkEventController *controller;
  GtkIMContext *context;
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter start, end;
  GtkTextIter bound, insert;
  gboolean ret;
  char *text;
  int anchor_pos, cursor_pos;

  widget = gtk_text_view_new ();
  controller = gtk_text_view_get_key_controller (GTK_TEXT_VIEW (widget));
  context = gtk_event_controller_key_get_im_context (GTK_EVENT_CONTROLLER_KEY (controller));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
  gtk_text_buffer_set_text (buffer, "abcd\nefgh\nijkl", -1);
  gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, 1, 2);
  gtk_text_buffer_place_cursor (buffer, &iter);

  ret = gtk_im_context_get_surrounding_with_selection (context,
                                                       &text,
                                                       &cursor_pos,
                                                       &anchor_pos);

  g_assert_true (ret);
  g_assert_cmpstr (text, ==, "abcd\nefgh\nijkl");
  g_assert_cmpint (cursor_pos, ==, 7);
  g_assert_cmpint (anchor_pos, ==, 7);

  g_free (text);

  ret = gtk_im_context_delete_surrounding (context, -1, 1);
  g_assert_true (ret);

  gtk_text_buffer_get_bounds (buffer, &start, &end);
  text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
  g_assert_cmpstr (text, ==, "abcd\negh\nijkl");
  g_free (text);
  gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
  g_assert_cmpint (gtk_text_iter_get_line (&start), ==, 1);
  g_assert_cmpint (gtk_text_iter_get_line_offset (&start), ==, 1);

  ret = gtk_im_context_delete_surrounding (context, 1, 1);
  g_assert_true (ret);

  gtk_text_buffer_get_bounds (buffer, &start, &end);
  text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
  g_assert_cmpstr (text, ==, "abcd\neg\nijkl");
  g_free (text);
  gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
  g_assert_cmpint (gtk_text_iter_get_line (&start), ==, 1);
  g_assert_cmpint (gtk_text_iter_get_line_offset (&start), ==, 1);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
  gtk_text_buffer_set_text (buffer, "ab cd\nef gh\nijkl", -1);
  gtk_text_buffer_get_iter_at_line_offset (buffer, &bound, 1, 4);
  gtk_text_buffer_get_iter_at_line_offset (buffer, &insert, 2, 2);
  gtk_text_buffer_select_range (buffer, &insert, &bound);

  ret = gtk_im_context_get_surrounding_with_selection (context,
                                                       &text,
                                                       &cursor_pos,
                                                       &anchor_pos);

  g_assert_true (ret);
  g_assert_cmpstr (text, ==, "cd\nef gh\nijkl");
  g_assert_cmpint (anchor_pos, ==, 7);
  g_assert_cmpint (cursor_pos, ==, 11);

  g_free (text);

  g_object_ref_sink (widget);
  g_object_unref (widget);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_object_set (gtk_settings_get_default (), "gtk-im-module", "gtk-im-context-simple", NULL);

  g_test_add_func ("/im-context/text-surrounding", test_text_surrounding);
  g_test_add_func ("/im-context/textview-surrounding", test_textview_surrounding);

  return g_test_run ();
}
