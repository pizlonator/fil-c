/* simple.c
 * Copyright (C) 2017  Red Hat, Inc
 * Author: Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */
#include <gtk/gtk.h>

static GtkWidget *_margin;
static GtkWidget *_align;
static GtkWidget *_xalign;
static GtkWidget *_yalign;

static void
highlight_at_mark (GtkTextBuffer *buffer,
                   GtkTextMark   *mark,
                   gboolean       on)
{
  GtkTextIter iter, iter2;

  gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);
  iter2 = iter;
  gtk_text_iter_forward_line (&iter2);

  if (on)
    gtk_text_buffer_apply_tag_by_name (buffer, "hihi", &iter, &iter2);
  else
    gtk_text_buffer_remove_tag_by_name (buffer, "hihi", &iter, &iter2);
}

static void
go_forward_or_back (GtkButton   *button,
                    GtkTextView *tv,
                    gboolean     forward)
{
  GtkTextBuffer *buffer;
  GtkTextMark *mark;
  GtkTextIter iter;
  gboolean found;

  buffer = gtk_text_view_get_buffer (tv);
  mark = gtk_text_buffer_get_mark (buffer, "mimi");
  highlight_at_mark (buffer, mark, FALSE);

  gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);
  if (forward)
    found = gtk_text_iter_forward_search (&iter, "\n-----", 0, &iter, NULL, NULL);
  else
    found = gtk_text_iter_backward_search (&iter, "\n-----", 0, &iter, NULL, NULL);
  if (found)
    {
      double margin;
      gboolean use_align;
      double xalign, yalign;

      gtk_text_iter_forward_char (&iter);
      gtk_text_buffer_move_mark (buffer, mark, &iter);
      highlight_at_mark (buffer, mark, TRUE);

      margin = gtk_spin_button_get_value (GTK_SPIN_BUTTON (_margin));
      use_align = gtk_check_button_get_active (GTK_CHECK_BUTTON (_align));
      xalign = gtk_spin_button_get_value (GTK_SPIN_BUTTON (_xalign));
      yalign = gtk_spin_button_get_value (GTK_SPIN_BUTTON (_yalign));

      gtk_text_view_scroll_to_mark (tv, mark, margin, use_align, xalign, yalign);
    }
  else
    {
      if (forward)
        gtk_text_buffer_get_end_iter (buffer, &iter);
      else
        gtk_text_buffer_get_start_iter (buffer, &iter);

      gtk_text_buffer_move_mark (buffer, mark, &iter);

      gtk_widget_error_bell (GTK_WIDGET (button));
    }
}

static void
go_forward (GtkButton   *button,
            GtkTextView *tv)
{
  go_forward_or_back (button, tv, TRUE);
}

static void
go_back (GtkButton   *button,
         GtkTextView *tv)
{
  go_forward_or_back (button, tv, FALSE);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *box;
  GtkWidget *sw, *tv;
  GtkWidget *button, *box2;
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextTag *tag;
  GdkRGBA bg;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 600);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);

  tv = gtk_text_view_new ();
  gtk_text_view_set_left_margin (GTK_TEXT_VIEW (tv), 10);
  gtk_text_view_set_right_margin (GTK_TEXT_VIEW (tv), 10);
  gtk_text_view_set_top_margin (GTK_TEXT_VIEW (tv), 10);
  gtk_text_view_set_bottom_margin (GTK_TEXT_VIEW (tv), 10);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (tv));

  if (argc > 1)
    {
      char *contents;
      gsize size;

      if (g_file_get_contents (argv[1], &contents, &size, NULL))
        gtk_text_buffer_set_text (buffer, contents, size);
    }

  gtk_text_buffer_get_start_iter (buffer, &iter);
  gtk_text_buffer_create_mark (buffer, "mimi", &iter, TRUE);

  tag = gtk_text_tag_new ("hihi");
  bg.red = 0;
  bg.green = 0;
  bg.blue = 1;
  bg.alpha = 0.3;
  g_object_set (tag, "background-rgba", &bg, NULL);
  gtk_text_tag_table_add (gtk_text_buffer_get_tag_table (buffer), tag);

  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), tv);

  gtk_widget_set_hexpand (sw, TRUE);
  gtk_widget_set_vexpand (sw, TRUE);
  gtk_box_append (GTK_BOX (box), sw);

  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_box_append (GTK_BOX (box), box2);

  button = gtk_button_new_with_label ("Forward");
  g_signal_connect (button, "clicked", G_CALLBACK (go_forward), tv);
  gtk_box_append (GTK_BOX (box2), button);

  button = gtk_button_new_with_label ("Back");
  g_signal_connect (button, "clicked", G_CALLBACK (go_back), tv);
  gtk_box_append (GTK_BOX (box2), button);

  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_box_append (GTK_BOX (box), box2);
  gtk_box_append (GTK_BOX (box2), gtk_label_new ("Margin:"));
  _margin = gtk_spin_button_new_with_range (0, 0.5, 0.1);
  gtk_box_append (GTK_BOX (box2), _margin);

  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_box_append (GTK_BOX (box), box2);
  gtk_box_append (GTK_BOX (box2), gtk_label_new ("Align:"));
  _align = gtk_check_button_new ();
  gtk_box_append (GTK_BOX (box2), _align);
  _xalign = gtk_spin_button_new_with_range (0, 1, 0.1);
  gtk_box_append (GTK_BOX (box2), _xalign);
  _yalign = gtk_spin_button_new_with_range (0, 1, 0.1);
  gtk_box_append (GTK_BOX (box2), _yalign);

  gtk_window_set_child (GTK_WINDOW (window), box);

  gtk_window_present (GTK_WINDOW (window));

  while (1)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
