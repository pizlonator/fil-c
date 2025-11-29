/* testtextbuffer.c -- Simplistic test suite
 * Copyright (C) 2000 Red Hat, Inc
 * Author: Havoc Pennington
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

#include "config.h"
#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>
#include "gtk/gtktexttypesprivate.h" /* Private header, for UNKNOWN_CHAR */
#include "gtk/gtktextbufferprivate.h" /* Private header */

static void
gtk_text_iter_spew (const GtkTextIter *iter, const char *desc)
{
  g_print (" %20s: line %d / char %d / line char %d / line byte %d\n",
           desc,
           gtk_text_iter_get_line (iter),
           gtk_text_iter_get_offset (iter),
           gtk_text_iter_get_line_offset (iter),
           gtk_text_iter_get_line_index (iter));
}

static void
check_get_set_text (GtkTextBuffer *buffer,
                    const char    *str)
{
  GtkTextIter start, end, iter;
  char *text;
  int n;

  gtk_text_buffer_set_text (buffer, str, -1);
  if (gtk_text_buffer_get_char_count (buffer) != g_utf8_strlen (str, -1))
    g_error ("Wrong number of chars (%d not %d)",
             gtk_text_buffer_get_char_count (buffer),
             (int) g_utf8_strlen (str, -1));
  gtk_text_buffer_get_bounds (buffer, &start, &end);
  text = gtk_text_buffer_get_text (buffer, &start, &end, TRUE);
  if (strcmp (text, str) != 0)
    g_error ("Got '%s' as buffer contents", text);
  g_free (text);

  /* line char counts */
  iter = start;
  n = 0;
  do
    {
      n += gtk_text_iter_get_chars_in_line (&iter);
    }
  while (gtk_text_iter_forward_line (&iter));

  if (n != gtk_text_buffer_get_char_count (buffer))
    g_error ("Sum of chars in lines is %d but buffer char count is %d",
             n, gtk_text_buffer_get_char_count (buffer));

  /* line byte counts */
  iter = start;
  n = 0;
  do
    {
      n += gtk_text_iter_get_bytes_in_line (&iter);
    }
  while (gtk_text_iter_forward_line (&iter));

  if (n != strlen (str))
    g_error ("Sum of chars in lines is %d but buffer byte count is %d",
             n, (int) strlen (str));

  gtk_text_buffer_set_text (buffer, "", -1);

  n = gtk_text_buffer_get_line_count (buffer);
  if (n != 1)
    g_error ("%d lines, expected 1", n);

  n = gtk_text_buffer_get_char_count (buffer);
  if (n != 0)
    g_error ("%d chars, expected 0", n);
}

static int
count_toggles_at_iter (GtkTextIter *iter,
                       GtkTextTag  *of_tag)
{
  GSList *tags;
  GSList *tmp;
  int count = 0;

  /* get toggle-ons and toggle-offs */
  tags = gtk_text_iter_get_toggled_tags (iter, TRUE);
  tags = g_slist_concat (tags,
                         gtk_text_iter_get_toggled_tags (iter, FALSE));

  tmp = tags;
  while (tmp != NULL)
    {
      if (of_tag == NULL)
        ++count;
      else if (of_tag == tmp->data)
        ++count;

      tmp = tmp->next;
    }

  g_slist_free (tags);

  return count;
}

static int
count_toggles_in_range_by_char (GtkTextBuffer     *buffer,
                                GtkTextTag        *of_tag,
                                const GtkTextIter *start,
                                const GtkTextIter *end)
{
  GtkTextIter iter;
  int count = 0;

  iter = *start;
  do
    {
      count += count_toggles_at_iter (&iter, of_tag);
      if (!gtk_text_iter_forward_char (&iter))
        {
          /* end iterator */
          count += count_toggles_at_iter (&iter, of_tag);
          break;
        }
    }
  while (gtk_text_iter_compare (&iter, end) <= 0);

  return count;
}

static int
count_toggles_in_buffer (GtkTextBuffer *buffer,
                         GtkTextTag    *of_tag)
{
  GtkTextIter start, end;

  gtk_text_buffer_get_bounds (buffer, &start, &end);

  return count_toggles_in_range_by_char (buffer, of_tag, &start, &end);
}

static void
check_specific_tag_in_range (GtkTextBuffer     *buffer,
                             const char        *tag_name,
                             const GtkTextIter *start,
                             const GtkTextIter *end)
{
  GtkTextIter iter;
  GtkTextTag *tag;
  gboolean state;
  int count;
  int buffer_count;
  int last_offset;

  if (gtk_text_iter_compare (start, end) > 0)
    {
      g_print ("  (inverted range for checking tags, skipping)\n");
      return;
    }

  tag = gtk_text_tag_table_lookup (gtk_text_buffer_get_tag_table (buffer),
                                   tag_name);

  buffer_count = count_toggles_in_range_by_char (buffer, tag, start, end);

  state = FALSE;
  count = 0;

  last_offset = -1;
  iter = *start;
  if (gtk_text_iter_toggles_tag (&iter, tag) ||
      gtk_text_iter_forward_to_tag_toggle (&iter, tag))
    {
      do
        {
          int this_offset;

          ++count;

          this_offset = gtk_text_iter_get_offset (&iter);

          if (this_offset <= last_offset)
            g_error ("forward_to_tag_toggle moved in wrong direction");

          last_offset = this_offset;

          if (gtk_text_iter_starts_tag (&iter, tag))
            {
              if (state)
                g_error ("Tag %p is already on, and was toggled on?", tag);
              state = TRUE;
            }
          else if (gtk_text_iter_ends_tag (&iter, tag))
            {
              if (!state)
                g_error ("Tag %p toggled off, but wasn't toggled on?", tag);
              state = FALSE;
            }
          else
            g_error ("forward_to_tag_toggle went to a location without a toggle");
        }
      while (gtk_text_iter_forward_to_tag_toggle (&iter, tag) &&
             gtk_text_iter_compare (&iter, end) <= 0);
    }

  if (count != buffer_count)
    g_error ("Counted %d tags iterating by char, %d iterating forward by tag toggle",
             buffer_count, count);

  state = FALSE;
  count = 0;

  iter = *end;
  last_offset = gtk_text_iter_get_offset (&iter);
  if (gtk_text_iter_toggles_tag (&iter, tag) ||
      gtk_text_iter_backward_to_tag_toggle (&iter, tag))
    {
      do
        {
          int this_offset;

          ++count;

          this_offset = gtk_text_iter_get_offset (&iter);

          if (this_offset >= last_offset)
            g_error ("backward_to_tag_toggle moved in wrong direction");

          last_offset = this_offset;

          if (gtk_text_iter_starts_tag (&iter, tag))
            {
              if (!state)
                g_error ("Tag %p wasn't on when we got to the on toggle going backward?", tag);
              state = FALSE;
            }
          else if (gtk_text_iter_ends_tag (&iter, tag))
            {
              if (state)
                g_error ("Tag %p off toggle, but we were already inside a tag?", tag);
              state = TRUE;
            }
          else
            g_error ("backward_to_tag_toggle went to a location without a toggle");
        }
      while (gtk_text_iter_backward_to_tag_toggle (&iter, tag) &&
             gtk_text_iter_compare (&iter, start) >= 0);
    }

  if (count != buffer_count)
    g_error ("Counted %d tags iterating by char, %d iterating backward by tag toggle\n",
             buffer_count, count);
}

static void
check_specific_tag (GtkTextBuffer *buffer,
                    const char    *tag_name)
{
  GtkTextIter start, end;

  gtk_text_buffer_get_bounds (buffer, &start, &end);
  check_specific_tag_in_range (buffer, tag_name, &start, &end);
  gtk_text_iter_forward_chars (&start, 2);
  gtk_text_iter_backward_chars (&end, 2);
  if (gtk_text_iter_compare (&start, &end) < 0)
    check_specific_tag_in_range (buffer, tag_name, &start, &end);
}

static void
run_tests (GtkTextBuffer *buffer)
{
  GtkTextIter iter;
  GtkTextIter start;
  GtkTextIter end;
  GtkTextIter mark;
  int i, j;
  int num_chars;
  GtkTextMark *bar_mark;
  GtkTextTag *tag;
  GHashTable *tag_states;
  int count;
  int buffer_count;

  gtk_text_buffer_get_bounds (buffer, &start, &end);

  /* Check that walking the tree via chars and via iterators produces
   * the same number of indexable locations.
   */
  num_chars = gtk_text_buffer_get_char_count (buffer);
  iter = start;
  bar_mark = gtk_text_buffer_create_mark (buffer, "bar", &iter, FALSE);
  i = 0;
  while (i < num_chars)
    {
      GtkTextIter current;
      GtkTextMark *foo_mark;

      gtk_text_buffer_get_iter_at_offset (buffer, &current, i);

      if (!gtk_text_iter_equal (&iter, &current))
        {
          g_error ("get_char_index didn't return current iter");
        }

      j = gtk_text_iter_get_offset (&iter);

      if (i != j)
        {
          g_error ("iter converted to %d not %d", j, i);
        }

      /* get/set mark */
      gtk_text_buffer_get_iter_at_mark (buffer, &mark, bar_mark);

      if (!gtk_text_iter_equal (&iter, &mark))
        {
          gtk_text_iter_spew (&iter, "iter");
          gtk_text_iter_spew (&mark, "mark");
          g_error ("Mark not moved to the right place.");
        }

      foo_mark = gtk_text_buffer_create_mark (buffer, "foo", &iter, FALSE);
      gtk_text_buffer_get_iter_at_mark (buffer, &mark, foo_mark);
      gtk_text_buffer_delete_mark (buffer, foo_mark);

      if (!gtk_text_iter_equal (&iter, &mark))
        {
          gtk_text_iter_spew (&iter, "iter");
          gtk_text_iter_spew (&mark, "mark");
          g_error ("Mark not created in the right place.");
        }

      if (gtk_text_iter_is_end (&iter))
        g_error ("iterators ran out before chars (offset %d of %d)",
                 i, num_chars);

      gtk_text_iter_forward_char (&iter);

      gtk_text_buffer_move_mark (buffer, bar_mark, &iter);

      ++i;
    }

  if (!gtk_text_iter_equal (&iter, &end))
    g_error ("Iterating over all chars didn't end with the end iter");

  /* Do the tree-walk backward
   */
  num_chars = gtk_text_buffer_get_char_count (buffer);
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, -1);

  gtk_text_buffer_move_mark (buffer, bar_mark, &iter);

  i = num_chars;

  if (!gtk_text_iter_equal (&iter, &end))
    g_error ("iter at char -1 is not equal to the end iterator");

  while (i >= 0)
    {
      GtkTextIter current;
      GtkTextMark *foo_mark;

      gtk_text_buffer_get_iter_at_offset (buffer, &current, i);

      if (!gtk_text_iter_equal (&iter, &current))
        {
          g_error ("get_char_index didn't return current iter while going backward");
        }
      j = gtk_text_iter_get_offset (&iter);

      if (i != j)
        {
          g_error ("going backward, iter converted to %d not %d", j, i);
        }

      /* get/set mark */
      gtk_text_buffer_get_iter_at_mark (buffer, &mark, bar_mark);

      if (!gtk_text_iter_equal (&iter, &mark))
        {
          gtk_text_iter_spew (&iter, "iter");
          gtk_text_iter_spew (&mark, "mark");
          g_error ("Mark not moved to the right place.");
        }

      foo_mark = gtk_text_buffer_create_mark (buffer, "foo", &iter, FALSE);
      gtk_text_buffer_get_iter_at_mark (buffer, &mark, foo_mark);
      gtk_text_buffer_delete_mark (buffer, foo_mark);

      if (!gtk_text_iter_equal (&iter, &mark))
        {
          gtk_text_iter_spew (&iter, "iter");
          gtk_text_iter_spew (&mark, "mark");
          g_error ("Mark not created in the right place.");
        }

      if (i > 0)
        {
          if (!gtk_text_iter_backward_char (&iter))
            g_error ("iterators ran out before char indexes");

          gtk_text_buffer_move_mark (buffer, bar_mark, &iter);
        }
      else
        {
          if (gtk_text_iter_backward_char (&iter))
            g_error ("went backward from 0?");
        }

      --i;
    }

  if (!gtk_text_iter_equal (&iter, &start))
    g_error ("Iterating backward over all chars didn't end with the start iter");

  /*
   * Check that get_line_count returns the same number of lines
   * as walking the tree by line
   */
  i = 1; /* include current (first) line */
  gtk_text_buffer_get_iter_at_line (buffer, &iter, 0);
  while (gtk_text_iter_forward_line (&iter))
    ++i;

  if (i != gtk_text_buffer_get_line_count (buffer))
    g_error ("Counted %d lines, buffer has %d", i,
             gtk_text_buffer_get_line_count (buffer));

  /*
   * Check that moving over tag toggles thinks about working.
   */

  buffer_count = count_toggles_in_buffer (buffer, NULL);

  tag_states = g_hash_table_new (NULL, NULL);
  count = 0;

  gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);
  if (gtk_text_iter_toggles_tag (&iter, NULL) ||
      gtk_text_iter_forward_to_tag_toggle (&iter, NULL))
    {
      do
        {
          GSList *tags;
          GSList *tmp;
          gboolean found_some = FALSE;

          /* get toggled-on tags */
          tags = gtk_text_iter_get_toggled_tags (&iter, TRUE);

          if (tags)
            found_some = TRUE;

          tmp = tags;
          while (tmp != NULL)
            {
              ++count;

              tag = tmp->data;

              if (g_hash_table_lookup (tag_states, tag))
                g_error ("Tag %p is already on, and was toggled on?", tag);

              g_hash_table_insert (tag_states, tag, GINT_TO_POINTER (TRUE));

              tmp = tmp->next;
            }

          g_slist_free (tags);

          /* get toggled-off tags */
          tags = gtk_text_iter_get_toggled_tags (&iter, FALSE);

          if (tags)
            found_some = TRUE;

          tmp = tags;
          while (tmp != NULL)
            {
              ++count;

              tag = tmp->data;

              if (!g_hash_table_lookup (tag_states, tag))
                g_error ("Tag %p is already off, and was toggled off?", tag);

              g_hash_table_remove (tag_states, tag);

              tmp = tmp->next;
            }

          g_slist_free (tags);

          if (!found_some)
            g_error ("No tags found going forward to tag toggle.");

        }
      while (gtk_text_iter_forward_to_tag_toggle (&iter, NULL));
    }

  g_hash_table_destroy (tag_states);

  if (count != buffer_count)
    g_error ("Counted %d tags iterating by char, %d iterating by tag toggle\n",
             buffer_count, count);

  /* Go backward; here TRUE in the hash means we saw
   * an off toggle last.
   */

  tag_states = g_hash_table_new (NULL, NULL);
  count = 0;

  gtk_text_buffer_get_end_iter (buffer, &iter);
  if (gtk_text_iter_toggles_tag (&iter, NULL) ||
      gtk_text_iter_backward_to_tag_toggle (&iter, NULL))
    {
      do
        {
          GSList *tags;
          GSList *tmp;
          gboolean found_some = FALSE;

          /* get toggled-off tags */
          tags = gtk_text_iter_get_toggled_tags (&iter, FALSE);

          if (tags)
            found_some = TRUE;

          tmp = tags;
          while (tmp != NULL)
            {
              ++count;

              tag = tmp->data;

              if (g_hash_table_lookup (tag_states, tag))
                g_error ("Tag %p has two off-toggles in a row?", tag);

              g_hash_table_insert (tag_states, tag, GINT_TO_POINTER (TRUE));

              tmp = tmp->next;
            }

          g_slist_free (tags);

          /* get toggled-on tags */
          tags = gtk_text_iter_get_toggled_tags (&iter, TRUE);

          if (tags)
            found_some = TRUE;

          tmp = tags;
          while (tmp != NULL)
            {
              ++count;

              tag = tmp->data;

              if (!g_hash_table_lookup (tag_states, tag))
                g_error ("Tag %p was toggled on, but saw no off-toggle?", tag);

              g_hash_table_remove (tag_states, tag);

              tmp = tmp->next;
            }

          g_slist_free (tags);

          if (!found_some)
            g_error ("No tags found going backward to tag toggle.");
        }
      while (gtk_text_iter_backward_to_tag_toggle (&iter, NULL));
    }

  g_hash_table_destroy (tag_states);

  if (count != buffer_count)
    g_error ("Counted %d tags iterating by char, %d iterating by tag toggle",
             buffer_count, count);

  check_specific_tag (buffer, "fg_red");
  check_specific_tag (buffer, "bg_green");
  check_specific_tag (buffer, "front_tag");
  check_specific_tag (buffer, "center_tag");
  check_specific_tag (buffer, "end_tag");
}


static const char  *book_closed_xpm[] = {
"16 16 6 1",
"       c None s None",
".      c black",
"X      c red",
"o      c yellow",
"O      c #808080",
"#      c white",
"                ",
"       ..       ",
"     ..XX.      ",
"   ..XXXXX.     ",
" ..XXXXXXXX.    ",
".ooXXXXXXXXX.   ",
"..ooXXXXXXXXX.  ",
".X.ooXXXXXXXXX. ",
".XX.ooXXXXXX..  ",
" .XX.ooXXX..#O  ",
"  .XX.oo..##OO. ",
"   .XX..##OO..  ",
"    .X.#OO..    ",
"     ..O..      ",
"      ..        ",
"                "};

static void
fill_buffer (GtkTextBuffer *buffer)
{
  GtkTextTag *tag;
  GdkRGBA color;
  GdkRGBA color2;
  GtkTextIter iter;
  GtkTextIter iter2;
  GdkPixbuf *pixbuf;
  GdkTexture *texture;
  int i;

  color.red = 0.0;
  color.green = 0.0;
  color.blue = 1.0;
  color.alpha = 1.0;

  color2.red = 1.0;
  color2.green = 0.0;
  color2.blue = 0.0;
  color2.alpha = 1.0;

  gtk_text_buffer_create_tag (buffer, "fg_blue",
                              "foreground_rgba", &color,
                              "background_rgba", &color2,
                              "font", "-*-courier-bold-r-*-*-30-*-*-*-*-*-*-*",
                              NULL);

  color.red = 1.0;
  color.green = 0.0;
  color.blue = 0.0;

  gtk_text_buffer_create_tag (buffer, "fg_red",
                              "rise", -4,
                              "foreground_rgba", &color,
                              NULL);

  color.red = 0.0;
  color.green = 1.0;
  color.blue = 0.0;

  gtk_text_buffer_create_tag (buffer, "bg_green",
                              "background_rgba", &color,
                              "font", "-*-courier-bold-r-*-*-10-*-*-*-*-*-*-*",
                              NULL);

  pixbuf = gdk_pixbuf_new_from_xpm_data (book_closed_xpm);
  texture = gdk_texture_new_for_pixbuf (pixbuf);

  g_assert_nonnull (texture);

  for (i = 0; i < 10; i++)
    {
      char *str;

      gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);

      gtk_text_buffer_insert_paintable (buffer, &iter, GDK_PAINTABLE (texture));

      gtk_text_buffer_get_iter_at_offset (buffer, &iter, 1);

      gtk_text_buffer_insert_paintable (buffer, &iter, GDK_PAINTABLE (texture));

      str = g_strdup_printf ("%d Hello World!\nwoo woo woo woo woo woo woo woo\n",
                            i);

      gtk_text_buffer_insert (buffer, &iter, str, -1);

      g_free (str);

      gtk_text_buffer_insert (buffer, &iter,
                              "(Hello World!)\nfoo foo Hello this is some text we are using to text word wrap. It has punctuation! gee; blah - hmm, great.\nnew line\n\n"
                              /* This is UTF8 stuff, Emacs doesn't
                                 really know how to display it */
                              "Spanish (Espa\303\261ol) \302\241Hola! / French (Fran\303\247ais) Bonjour, Salut / German (Deutsch S\303\274d) Gr\303\274\303\237 Gott (testing Latin-1 chars encoded in UTF8)\nThai (we can't display this, just making sure we don't crash)  (\340\270\240\340\270\262\340\270\251\340\270\262\340\271\204\340\270\227\340\270\242)  \340\270\252\340\270\247\340\270\261\340\270\252\340\270\224\340\270\265\340\270\204\340\270\243\340\270\261\340\270\232, \340\270\252\340\270\247\340\270\261\340\270\252\340\270\224\340\270\265\340\270\204\340\271\210\340\270\260\n",
                              -1);

      gtk_text_buffer_insert_paintable (buffer, &iter, GDK_PAINTABLE(texture));
      gtk_text_buffer_insert_paintable (buffer, &iter, GDK_PAINTABLE(texture));

      gtk_text_buffer_get_iter_at_offset (buffer, &iter, 4);

      gtk_text_buffer_insert_paintable (buffer, &iter, GDK_PAINTABLE(texture));

      gtk_text_buffer_get_iter_at_offset (buffer, &iter, 7);

      gtk_text_buffer_insert_paintable (buffer, &iter, GDK_PAINTABLE(texture));

      gtk_text_buffer_get_iter_at_offset (buffer, &iter, 8);

      gtk_text_buffer_insert_paintable (buffer, &iter, GDK_PAINTABLE(texture));

      gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, 0, 8);
      iter2 = iter;
      gtk_text_iter_forward_chars (&iter2, 10);

      gtk_text_buffer_apply_tag_by_name (buffer, "fg_blue", &iter, &iter2);

      gtk_text_iter_forward_chars (&iter, 7);
      gtk_text_iter_forward_chars (&iter2, 10);

      gtk_text_buffer_apply_tag_by_name (buffer, "bg_green", &iter, &iter2);

      gtk_text_iter_forward_chars (&iter, 12);
      gtk_text_iter_forward_chars (&iter2, 10);

      gtk_text_buffer_apply_tag_by_name (buffer, "bg_green", &iter, &iter2);

      gtk_text_iter_forward_chars (&iter, 10);
      gtk_text_iter_forward_chars (&iter2, 15);

      gtk_text_buffer_apply_tag_by_name (buffer, "fg_red", &iter, &iter2);
      gtk_text_buffer_apply_tag_by_name (buffer, "fg_blue", &iter, &iter2);

      gtk_text_iter_forward_chars (&iter, 20);
      gtk_text_iter_forward_chars (&iter2, 20);

      gtk_text_buffer_apply_tag_by_name (buffer, "fg_red", &iter, &iter2);
      gtk_text_buffer_apply_tag_by_name (buffer, "fg_blue", &iter, &iter2);

      gtk_text_iter_backward_chars (&iter, 25);
      gtk_text_iter_forward_chars (&iter2, 5);

      gtk_text_buffer_apply_tag_by_name (buffer, "fg_red", &iter, &iter2);
      gtk_text_buffer_apply_tag_by_name (buffer, "fg_blue", &iter, &iter2);

      gtk_text_iter_forward_chars (&iter, 15);
      gtk_text_iter_backward_chars (&iter2, 10);

      gtk_text_buffer_remove_tag_by_name (buffer, "fg_red", &iter, &iter2);
      gtk_text_buffer_remove_tag_by_name (buffer, "fg_blue", &iter, &iter2);
    }

  /* Put in tags that are just at the beginning, and just near the end,
   * and just near the middle.
   */
  tag = gtk_text_buffer_create_tag (buffer, "front_tag", NULL);
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, 3);
  gtk_text_buffer_get_iter_at_offset (buffer, &iter2, 300);

  gtk_text_buffer_apply_tag (buffer, tag, &iter, &iter2);

  tag = gtk_text_buffer_create_tag (buffer, "end_tag", NULL);
  gtk_text_buffer_get_end_iter (buffer, &iter2);
  gtk_text_iter_backward_chars (&iter2, 12);
  iter = iter2;
  gtk_text_iter_backward_chars (&iter, 157);

  gtk_text_buffer_apply_tag (buffer, tag, &iter, &iter2);

  tag = gtk_text_buffer_create_tag (buffer, "center_tag", NULL);
  gtk_text_buffer_get_iter_at_offset (buffer, &iter,
                                      gtk_text_buffer_get_char_count (buffer)/2);
  gtk_text_iter_backward_chars (&iter, 37);
  iter2 = iter;
  gtk_text_iter_forward_chars (&iter2, 57);

  gtk_text_buffer_apply_tag (buffer, tag, &iter, &iter2);

  g_object_unref (pixbuf);
  g_object_unref (texture);
}


/*
 * Line separator tests (initially to avoid regression on bugzilla #57428)
 */

static void
test_line_separation (const char* str,
                      gboolean    expect_next_line,
                      gboolean    expect_end_iter,
                      int         expected_line_count,
                      int         expected_line_break,
                      int         expected_next_line_start)
{
  GtkTextIter iter;
  GtkTextBuffer* buffer;
  gboolean on_next_line;
  gboolean on_end_iter;
  int new_pos;

  buffer = gtk_text_buffer_new (NULL);

  gtk_text_buffer_set_text (buffer, str, -1);
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, expected_line_break);

  g_assert_true (gtk_text_iter_ends_line (&iter) || gtk_text_iter_is_end (&iter));

  g_assert_cmpint (gtk_text_buffer_get_line_count (buffer), ==, expected_line_count);

  on_next_line = gtk_text_iter_forward_line (&iter);

  g_assert_cmpint (expect_next_line, ==, on_next_line);

  on_end_iter = gtk_text_iter_is_end (&iter);

  g_assert_true (on_end_iter == expect_end_iter);

  new_pos = gtk_text_iter_get_offset (&iter);

  if (on_next_line)
    g_assert_cmpint (expected_next_line_start, ==, new_pos);

  ++expected_line_break;
  while (expected_line_break < expected_next_line_start)
    {
      gtk_text_buffer_get_iter_at_offset (buffer, &iter, expected_line_break);

      g_assert_false (gtk_text_iter_ends_line (&iter));

      on_next_line = gtk_text_iter_forward_line (&iter);

      g_assert_cmpint (expect_next_line, ==, on_next_line);

      new_pos = gtk_text_iter_get_offset (&iter);

      if (on_next_line)
        g_assert_cmpint (expected_next_line_start, ==, new_pos);

      ++expected_line_break;
    }

  /* FIXME tests for backward line */

  g_object_unref (buffer);
}

/* there are cases where \r and \n should not be treated like \r\n,
 * originally bug #337022. */
static void
split_r_n_separators_test (void)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;

  buffer = gtk_text_buffer_new (NULL);

  gtk_text_buffer_set_text (buffer, "foo\ra\nbar\n", -1);

  /* delete 'a' so that we have

     1 foo\r
     2 \n
     3 bar\n

   * and both \r and \n are line separators */

  gtk_text_buffer_get_iter_at_offset (buffer, &iter, 5);
  gtk_text_buffer_backspace (buffer, &iter, TRUE, TRUE);

  g_assert_true (gtk_text_iter_ends_line (&iter));

  gtk_text_buffer_get_iter_at_offset (buffer, &iter, 3);
  g_assert_true (gtk_text_iter_ends_line (&iter));

  g_object_unref (buffer);
}

static void
test_line_separator (void)
{
  char *str;
  char buf[7] = { '\0', };

  /* Only one character has type G_UNICODE_PARAGRAPH_SEPARATOR in
   * Unicode 3.0; update this if that changes.
   */
#define PARAGRAPH_SEPARATOR 0x2029

  test_line_separation ("line", FALSE, TRUE, 1, 4, 4);
  test_line_separation ("line\r\n", FALSE, TRUE, 2, 4, 6);
  test_line_separation ("line\r", FALSE, TRUE, 2, 4, 5);
  test_line_separation ("line\n", FALSE, TRUE, 2, 4, 5);
  test_line_separation ("line\rqw", TRUE, FALSE, 2, 4, 5);
  test_line_separation ("line\nqw", TRUE, FALSE, 2, 4, 5);
  test_line_separation ("line\r\nqw", TRUE, FALSE, 2, 4, 6);

  g_unichar_to_utf8 (PARAGRAPH_SEPARATOR, buf);

  str = g_strdup_printf ("line%s", buf);
  test_line_separation (str, FALSE, TRUE, 2, 4, 5);
  g_free (str);
  str = g_strdup_printf ("line%sqw", buf);
  test_line_separation (str, TRUE, FALSE, 2, 4, 5);
  g_free (str);

  split_r_n_separators_test ();
}

static void
test_backspace (void)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  gboolean ret;

  buffer = gtk_text_buffer_new (NULL);

  gtk_text_buffer_set_text (buffer, "foo", -1);
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, 2);
  ret = gtk_text_buffer_backspace (buffer, &iter, TRUE, TRUE);
  g_assert_true (ret);
  g_assert_cmpint (1, ==, gtk_text_iter_get_offset (&iter));
  g_assert_cmpint (2, ==, gtk_text_buffer_get_char_count (buffer));

  gtk_text_buffer_set_text (buffer, "foo", -1);
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);
  ret = gtk_text_buffer_backspace (buffer, &iter, TRUE, TRUE);
  g_assert_true (!ret);
  g_assert_cmpint (0, ==, gtk_text_iter_get_offset (&iter));
  g_assert_cmpint (3, ==, gtk_text_buffer_get_char_count (buffer));

  /* test bug #544724 */
  gtk_text_buffer_set_text (buffer, "foo\r\n\r\nbar", -1);
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, 5);
  ret = gtk_text_buffer_backspace (buffer, &iter, TRUE, TRUE);
  g_assert_true (ret);
  g_assert_cmpint (0, ==, gtk_text_iter_get_line (&iter));
  g_assert_cmpint (8, ==, gtk_text_buffer_get_char_count (buffer));

  /* test empty last line */
  gtk_text_buffer_set_text (buffer, "", -1);
  gtk_text_buffer_get_end_iter (buffer, &iter);
  ret = gtk_text_buffer_backspace (buffer, &iter, TRUE, TRUE);
  g_assert_false (ret);
  g_assert_cmpint (0, ==, gtk_text_iter_get_offset (&iter));
  g_assert_cmpint (0, ==, gtk_text_buffer_get_char_count (buffer));

  gtk_text_buffer_set_text (buffer, "foo\n", -1);
  gtk_text_buffer_get_end_iter (buffer, &iter);
  ret = gtk_text_buffer_backspace (buffer, &iter, TRUE, TRUE);
  g_assert_true (ret);
  g_assert_cmpint (3, ==, gtk_text_iter_get_offset (&iter));
  g_assert_cmpint (3, ==, gtk_text_buffer_get_char_count (buffer));

  gtk_text_buffer_set_text (buffer, "foo\r\n", -1);
  gtk_text_buffer_get_end_iter (buffer, &iter);
  ret = gtk_text_buffer_backspace (buffer, &iter, TRUE, TRUE);
  g_assert_true (ret);
  g_assert_cmpint (3, ==, gtk_text_iter_get_offset (&iter));
  g_assert_cmpint (3, ==, gtk_text_buffer_get_char_count (buffer));

  g_object_unref (buffer);
}

static void
test_logical_motion (void)
{
  char *str;
  char buf1[7] = { '\0', };
  char buf2[7] = { '\0', };
  char buf3[7] = { '\0', };
  int expected[30];
  int expected_steps;
  int i;
  GtkTextBuffer *buffer;
  GtkTextIter iter;

  buffer = gtk_text_buffer_new (NULL);

#define LEADING_JAMO 0x1111
#define VOWEL_JAMO 0x1167
#define TRAILING_JAMO 0x11B9

  g_unichar_to_utf8 (LEADING_JAMO, buf1);
  g_unichar_to_utf8 (VOWEL_JAMO, buf2);
  g_unichar_to_utf8 (TRAILING_JAMO, buf3);

  /* Build the string "abc<leading><vowel><trailing>def\r\nxyz" */
  str = g_strconcat ("abc", buf1, buf2, buf3, "def\r\nxyz", NULL);
  gtk_text_buffer_set_text (buffer, str, -1);
  g_free (str);

  /* Check cursor positions */
  memset (expected, 0, sizeof (expected));
  expected[0] = 0;    /* before 'a' */
  expected[1] = 1;    /* before 'b' */
  expected[2] = 2;    /* before 'c' */
  expected[3] = 3;    /* before jamo */
  expected[4] = 6;    /* before 'd' */
  expected[5] = 7;    /* before 'e' */
  expected[6] = 8;    /* before 'f' */
  expected[7] = 9;    /* before '\r' */
  expected[8] = 11;   /* before 'x' */
  expected[9] = 12;   /* before 'y' */
  expected[10] = 13;  /* before 'z' */
  expected[11] = 14;  /* after 'z' (only matters going backward) */
  expected_steps = 11;

  gtk_text_buffer_get_start_iter (buffer, &iter);
  i = 0;
  do
    {
      int pos;

      pos = gtk_text_iter_get_offset (&iter);

      if (pos != expected[i])
        {
          g_error ("Cursor position %d, expected %d",
                   pos, expected[i]);
        }

      ++i;
    }
  while (gtk_text_iter_forward_cursor_position (&iter));

  if (!gtk_text_iter_is_end (&iter))
    g_error ("Expected to stop at the end iterator");

  if (!gtk_text_iter_is_cursor_position (&iter))
    g_error ("Should be a cursor position before the end iterator");

  if (i != expected_steps)
    g_error ("Expected %d steps, there were actually %d\n", expected_steps, i);

  i = expected_steps;
  do
    {
      int pos;

      pos = gtk_text_iter_get_offset (&iter);

      if (pos != expected[i])
        {
          g_error ("Moving backward, cursor position %d, expected %d",
                   pos, expected[i]);
        }

      /* g_print ("%d = %d\n", pos, expected[i]); */

      --i;
    }
  while (gtk_text_iter_backward_cursor_position (&iter));

  if (i != -1)
    g_error ("Expected %d steps, there were actually %d", expected_steps - i, i);

  if (!gtk_text_iter_is_start (&iter))
    g_error ("Expected to stop at the start iterator");


  /* Check sentence boundaries */

  gtk_text_buffer_set_text (buffer, "Hi.\nHi. \nHi! Hi. Hi? Hi.", -1);

  memset (expected, 0, sizeof (expected));

  expected[0] = 0;    /* before first Hi */
  expected[1] = 3;    /* After first . */
  expected[2] = 7;    /* After second . */
  expected[3] = 12;   /* After ! */
  expected[4] = 16;   /* After third . */
  expected[5] = 20;   /* After ? */

  expected_steps = 6;

  gtk_text_buffer_get_start_iter (buffer, &iter);
  i = 0;
  do
    {
      int pos;

      pos = gtk_text_iter_get_offset (&iter);

      if (pos != expected[i])
        {
          g_error ("Sentence position %d, expected %d",
                   pos, expected[i]);
        }

      if (i != 0 &&
          !gtk_text_iter_is_end (&iter) &&
          !gtk_text_iter_ends_sentence (&iter))
        g_error ("Iterator at %d should end a sentence", pos);

      ++i;
    }
  while (gtk_text_iter_forward_sentence_end (&iter));

  if (i != expected_steps)
    g_error ("Expected %d steps, there were actually %d", expected_steps, i);

  if (!gtk_text_iter_is_end (&iter))
    g_error ("Expected to stop at the end iterator");

  gtk_text_buffer_set_text (buffer, "Hi.\nHi. \nHi! Hi. Hi? Hi.", -1);

  memset (expected, 0, sizeof (expected));

  expected[0] = 24;
  expected[1] = 21;
  expected[2] = 17;
  expected[3] = 13;
  expected[4] = 9;
  expected[5] = 4;
  expected[6] = 0;

  expected_steps = 7;

  gtk_text_buffer_get_end_iter (buffer, &iter);
  i = 0;
  do
    {
      int pos;

      pos = gtk_text_iter_get_offset (&iter);

      if (pos != expected[i])
        {
          g_error ("Sentence position %d, expected %d",
                   pos, expected[i]);
        }

      if (pos != 0 &&
          !gtk_text_iter_is_end (&iter) &&
          !gtk_text_iter_starts_sentence (&iter))
        g_error ("Iterator at %d should start a sentence", pos);

      ++i;
    }
  while (gtk_text_iter_backward_sentence_start (&iter));

  if (i != expected_steps)
    g_error ("Expected %d steps, there were actually %d", expected_steps, i);

  if (gtk_text_iter_get_offset (&iter) != 0)
    g_error ("Expected to stop at the start iterator");

  g_object_unref (buffer);
}

static void
test_marks (void)
{
  GtkTextBuffer *buf1, *buf2;
  GtkTextMark *mark;
  GtkTextIter iter;

  buf1 = gtk_text_buffer_new (NULL);
  buf2 = gtk_text_buffer_new (NULL);

  gtk_text_buffer_get_start_iter (buf1, &iter);
  mark = gtk_text_buffer_create_mark (buf1, "foo", &iter, TRUE);
  g_object_ref (mark);
  gtk_text_mark_set_visible (mark, TRUE);
  gtk_text_buffer_delete_mark (buf1, mark);

  g_assert_true (gtk_text_mark_get_visible (mark));
  g_assert_true (gtk_text_mark_get_left_gravity (mark));
  g_assert_cmpstr ("foo", ==, gtk_text_mark_get_name (mark));
  g_assert_null (gtk_text_mark_get_buffer (mark));
  g_assert_true (gtk_text_mark_get_deleted (mark));
  g_assert_null (gtk_text_buffer_get_mark (buf1, "foo"));

  gtk_text_buffer_get_start_iter (buf2, &iter);
  gtk_text_buffer_add_mark (buf2, mark, &iter);
  gtk_text_buffer_insert (buf2, &iter, "ewfwefwefwe", -1);
  gtk_text_buffer_get_iter_at_mark (buf2, &iter, mark);

  g_assert_true (gtk_text_mark_get_visible (mark));
  g_assert_true (gtk_text_iter_is_start (&iter));
  g_assert_true (gtk_text_mark_get_left_gravity (mark));
  g_assert_cmpstr ("foo", ==, gtk_text_mark_get_name (mark));
  g_assert_true (gtk_text_mark_get_buffer (mark) == buf2);
  g_assert_false (gtk_text_mark_get_deleted (mark));
  g_assert_true (gtk_text_buffer_get_mark (buf2, "foo") == mark);

  gtk_text_buffer_delete_mark (buf2, mark);
  gtk_text_mark_set_visible (mark, FALSE);
  g_object_unref (mark);

  mark = gtk_text_mark_new ("blah", TRUE);
  gtk_text_buffer_get_start_iter (buf1, &iter);
  gtk_text_mark_set_visible (mark, TRUE);
  gtk_text_buffer_add_mark (buf1, mark, &iter);

  g_assert_true (gtk_text_mark_get_visible (mark));
  g_assert_true (gtk_text_mark_get_buffer (mark) == buf1);
  g_assert_false (gtk_text_mark_get_deleted (mark));
  g_assert_true (gtk_text_buffer_get_mark (buf1, "blah") == mark);
  g_assert_cmpstr ("blah", ==, gtk_text_mark_get_name (mark));

  gtk_text_mark_set_visible (mark, FALSE);
  gtk_text_buffer_delete_mark (buf1, mark);
  g_assert_false (gtk_text_mark_get_visible (mark));
  g_assert_null (gtk_text_buffer_get_mark (buf1, "blah"));
  g_assert_null (gtk_text_mark_get_buffer (mark));
  g_assert_true (gtk_text_mark_get_deleted (mark));

  gtk_text_buffer_get_start_iter (buf2, &iter);
  gtk_text_buffer_add_mark (buf2, mark, &iter);
  g_assert_true (gtk_text_mark_get_buffer (mark) == buf2);
  g_assert_false (gtk_text_mark_get_deleted (mark));
  g_assert_true (gtk_text_buffer_get_mark (buf2, "blah") == mark);
  g_assert_cmpstr ("blah", ==, gtk_text_mark_get_name (mark));

  g_object_unref (mark);
  g_object_unref (buf1);
  g_object_unref (buf2);
}

static void
test_utf8 (void)
{
  gunichar ch;

  /* Check UTF8 unknown char thing */
  g_assert_cmpint (GTK_TEXT_UNKNOWN_CHAR_UTF8_LEN, ==, 3);
  g_assert_cmpint (g_utf8_strlen (gtk_text_unknown_char_utf8_gtk_tests_only (), 3), ==, 1);
  ch = g_utf8_get_char (gtk_text_unknown_char_utf8_gtk_tests_only ());
  g_assert_true (ch == GTK_TEXT_UNKNOWN_CHAR);
}

static void
test_empty_buffer (void)
{
  GtkTextBuffer *buffer;
  int n;
  GtkTextIter start;

  buffer = gtk_text_buffer_new (NULL);

  /* Check that buffer starts with one empty line and zero chars */
  n = gtk_text_buffer_get_line_count (buffer);
  if (n != 1)
    g_error ("%d lines, expected 1", n);

  n = gtk_text_buffer_get_char_count (buffer);
  if (n != 0)
    g_error ("%d chars, expected 0", n);

  /* empty first line contains 0 chars */
  gtk_text_buffer_get_start_iter (buffer, &start);
  n = gtk_text_iter_get_chars_in_line (&start);
  if (n != 0)
    g_error ("%d chars in first line, expected 0", n);
  n = gtk_text_iter_get_bytes_in_line (&start);
  if (n != 0)
    g_error ("%d bytes in first line, expected 0", n);

  /* Run gruesome alien test suite on buffer */
  run_tests (buffer);

  g_object_unref (buffer);
}

static void
test_get_set(void)
{
  GtkTextBuffer *buffer;

  buffer = gtk_text_buffer_new (NULL);

  check_get_set_text (buffer, "Hello");
  check_get_set_text (buffer, "Hello\n");
  check_get_set_text (buffer, "Hello\r\n");
  check_get_set_text (buffer, "Hello\r");
  check_get_set_text (buffer, "Hello\nBar\nFoo");
  check_get_set_text (buffer, "Hello\nBar\nFoo\n");

  g_object_unref (buffer);
}

static void
test_fill_empty (void)
{
  GtkTextBuffer *buffer;
  int n;
  GtkTextIter start, end;

  buffer = gtk_text_buffer_new (NULL);

  /* Put stuff in the buffer */
  fill_buffer (buffer);

  /* Subject stuff-bloated buffer to further torment */
  run_tests (buffer);

  /* Delete all stuff from the buffer */
  gtk_text_buffer_get_bounds (buffer, &start, &end);
  gtk_text_buffer_delete (buffer, &start, &end);

  /* Check buffer for emptiness (note that a single
     empty line always remains in the buffer) */
  n = gtk_text_buffer_get_line_count (buffer);
  if (n != 1)
    g_error ("%d lines, expected 1", n);

  n = gtk_text_buffer_get_char_count (buffer);
  if (n != 0)
    g_error ("%d chars, expected 0", n);

  run_tests (buffer);

  g_object_unref (buffer);
}

static void
test_tag (void)
{
  GtkTextBuffer *buffer;
  GtkTextIter start, end;

  buffer = gtk_text_buffer_new (NULL);

  fill_buffer (buffer);

  gtk_text_buffer_set_text (buffer, "adcdef", -1);
  gtk_text_buffer_get_iter_at_offset (buffer, &start, 1);
  gtk_text_buffer_get_iter_at_offset (buffer, &end, 3);
  gtk_text_buffer_apply_tag_by_name (buffer, "fg_blue", &start, &end);

  run_tests (buffer);

  g_object_unref (buffer);
}

static void
check_buffer_contents (GtkTextBuffer *buffer,
                       const char    *contents)
{
  GtkTextIter start;
  GtkTextIter end;
  char *buffer_contents;

  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_get_end_iter (buffer, &end);
  buffer_contents = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
  g_assert_cmpstr (buffer_contents, ==, contents);
  g_free (buffer_contents);
}

static void
wait_for_changed (GtkTextBuffer *buffer)
{
  GMainLoop *loop;
  gulong id;

  loop = g_main_loop_new (NULL, FALSE);
  id = g_signal_connect_swapped (buffer, "changed", G_CALLBACK (g_main_loop_quit), loop);
  g_main_loop_run (loop);
  g_signal_handler_disconnect (buffer, id);
  g_main_loop_unref (loop);
}

static void
test_clipboard (void)
{
  GdkClipboard *clipboard;
  GtkTextBuffer *buffer;
  GtkTextIter start;
  GtkTextIter end;
  GtkTextTag *tag;

  clipboard = gdk_display_get_clipboard (gdk_display_get_default ());

  buffer = gtk_text_buffer_new (NULL);
  gtk_text_buffer_set_text (buffer, "abcdef", -1);

  /* Simple cut & paste */
  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_get_iter_at_offset (buffer, &end, 3);
  gtk_text_buffer_select_range (buffer, &start, &end);

  gtk_text_buffer_cut_clipboard (buffer, clipboard, TRUE);
  check_buffer_contents (buffer, "def");

  gtk_text_buffer_get_end_iter (buffer, &end);
  gtk_text_buffer_paste_clipboard (buffer, clipboard, &end, TRUE);
  wait_for_changed (buffer);

  check_buffer_contents (buffer, "defabc");

  /* Simple copy & paste */
  gtk_text_buffer_get_iter_at_offset (buffer, &start, 3);
  gtk_text_buffer_get_end_iter (buffer, &end);
  gtk_text_buffer_select_range (buffer, &start, &end);
  gtk_text_buffer_copy_clipboard (buffer, clipboard);

  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_paste_clipboard (buffer, clipboard, &start, TRUE);
  wait_for_changed (buffer);

  check_buffer_contents (buffer, "abcdefabc");

  /* Replace the selection when pasting */
  gtk_text_buffer_set_text (buffer, "abcdef", -1);

  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_get_iter_at_offset (buffer, &end, 3);
  gtk_text_buffer_select_range (buffer, &start, &end);
  gtk_text_buffer_copy_clipboard (buffer, clipboard);

  gtk_text_buffer_get_iter_at_offset (buffer, &start, 3);
  gtk_text_buffer_get_end_iter (buffer, &end);
  gtk_text_buffer_select_range (buffer, &start, &end);
  gtk_text_buffer_paste_clipboard (buffer, clipboard, NULL, TRUE);
  wait_for_changed (buffer);

  check_buffer_contents (buffer, "abcabc");

  /* Copy & paste text with tags.
   * See https://bugzilla.gnome.org/show_bug.cgi?id=339539
   */
  gtk_text_buffer_set_text (buffer, "abcdef", -1);

  tag = gtk_text_buffer_create_tag (buffer, NULL, NULL);

  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_get_iter_at_offset (buffer, &end, 4);
  gtk_text_buffer_apply_tag (buffer, tag, &start, &end);

  gtk_text_buffer_get_iter_at_offset (buffer, &start, 3);
  gtk_text_buffer_get_end_iter (buffer, &end);
  gtk_text_buffer_select_range (buffer, &start, &end);
  gtk_text_buffer_copy_clipboard (buffer, clipboard);
  gtk_text_buffer_paste_clipboard (buffer, clipboard, NULL, TRUE);
  wait_for_changed (buffer);

  check_buffer_contents (buffer, "abcdef");

  gtk_text_buffer_get_iter_at_offset (buffer, &start, 3);
  g_assert_true (gtk_text_iter_forward_to_tag_toggle (&start, tag));
  g_assert_cmpint (4, ==, gtk_text_iter_get_offset (&start));

  g_object_unref (buffer);
}

static void
test_get_iter (void)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  int offset;

  buffer = gtk_text_buffer_new (NULL);

  /* ß takes 2 bytes in UTF-8 */
  gtk_text_buffer_set_text (buffer, "ab\nßd\r\nef", -1);

  /* Test get_iter_at_line() */
  g_assert_true (gtk_text_buffer_get_iter_at_line (buffer, &iter, 0));
  g_assert_true (gtk_text_iter_is_start (&iter));

  g_assert_true (gtk_text_buffer_get_iter_at_line (buffer, &iter, 1));
  offset = gtk_text_iter_get_offset (&iter);
  g_assert_cmpint (offset, ==, 3);

  g_assert_true (gtk_text_buffer_get_iter_at_line (buffer, &iter, 2));
  offset = gtk_text_iter_get_offset (&iter);
  g_assert_cmpint (offset, ==, 7);

  g_assert_false (gtk_text_buffer_get_iter_at_line (buffer, &iter, 3));
  g_assert_true (gtk_text_iter_is_end (&iter));

  /* Test get_iter_at_line_offset() */
  g_assert_true (gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, 0, 0));
  g_assert_true (gtk_text_iter_is_start (&iter));

  g_assert_true (gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, 0, 1));
  offset = gtk_text_iter_get_offset (&iter);
  g_assert_cmpint (offset, ==, 1);

  g_assert_true (gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, 0, 2));
  offset = gtk_text_iter_get_offset (&iter);
  g_assert_cmpint (offset, ==, 2);

  g_assert_false (gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, 0, 3));
  offset = gtk_text_iter_get_offset (&iter);
  g_assert_cmpint (offset, ==, 2);

  g_assert_true (gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, 1, 1));
  offset = gtk_text_iter_get_offset (&iter);
  g_assert_cmpint (offset, ==, 4);

  g_assert_true (gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, 2, 1));
  offset = gtk_text_iter_get_offset (&iter);
  g_assert_cmpint (offset, ==, 8);

  g_assert_true (gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, 2, 2));
  g_assert_true (gtk_text_iter_is_end (&iter));

  g_assert_false (gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, 2, 3));
  g_assert_true (gtk_text_iter_is_end (&iter));

  g_assert_false (gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, 3, 1));
  g_assert_true (gtk_text_iter_is_end (&iter));

  /* Test get_iter_at_line_index() */
  g_assert_true (gtk_text_buffer_get_iter_at_line_index (buffer, &iter, 0, 0));
  g_assert_true (gtk_text_iter_is_start (&iter));

  g_assert_true (gtk_text_buffer_get_iter_at_line_index (buffer, &iter, 0, 1));
  offset = gtk_text_iter_get_offset (&iter);
  g_assert_cmpint (offset, ==, 1);

  g_assert_true (gtk_text_buffer_get_iter_at_line_index (buffer, &iter, 0, 2));
  offset = gtk_text_iter_get_offset (&iter);
  g_assert_cmpint (offset, ==, 2);

  g_assert_false (gtk_text_buffer_get_iter_at_line_index (buffer, &iter, 0, 3));
  offset = gtk_text_iter_get_offset (&iter);
  g_assert_cmpint (offset, ==, 2);

  g_assert_true (gtk_text_buffer_get_iter_at_line_index (buffer, &iter, 1, 0));
  offset = gtk_text_iter_get_offset (&iter);
  g_assert_cmpint (offset, ==, 3);

  g_assert_true (gtk_text_buffer_get_iter_at_line_index (buffer, &iter, 1, 2));
  offset = gtk_text_iter_get_offset (&iter);
  g_assert_cmpint (offset, ==, 4);

  g_assert_true (gtk_text_buffer_get_iter_at_line_index (buffer, &iter, 1, 3));
  offset = gtk_text_iter_get_offset (&iter);
  g_assert_cmpint (offset, ==, 5);

  g_assert_true (gtk_text_buffer_get_iter_at_line_index (buffer, &iter, 2, 2));
  g_assert_true (gtk_text_iter_is_end (&iter));

  g_assert_false (gtk_text_buffer_get_iter_at_line_index (buffer, &iter, 2, 3));
  g_assert_true (gtk_text_iter_is_end (&iter));

  g_assert_false (gtk_text_buffer_get_iter_at_line_index (buffer, &iter, 3, 1));
  g_assert_true (gtk_text_iter_is_end (&iter));

  /* Test get_iter_at_offset() */
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);
  g_assert_true (gtk_text_iter_is_start (&iter));

  gtk_text_buffer_get_iter_at_offset (buffer, &iter, 1);
  offset = gtk_text_iter_get_offset (&iter);
  g_assert_cmpint (offset, ==, 1);

  gtk_text_buffer_get_iter_at_offset (buffer, &iter, 8);
  offset = gtk_text_iter_get_offset (&iter);
  g_assert_cmpint (offset, ==, 8);
  g_assert_false (gtk_text_iter_is_end (&iter));

  gtk_text_buffer_get_iter_at_offset (buffer, &iter, 9);
  g_assert_true (gtk_text_iter_is_end (&iter));

  gtk_text_buffer_get_iter_at_offset (buffer, &iter, 100);
  g_assert_true (gtk_text_iter_is_end (&iter));

  gtk_text_buffer_get_iter_at_offset (buffer, &iter, -1);
  g_assert_true (gtk_text_iter_is_end (&iter));

  g_object_unref (buffer);
}

static void
test_iter_with_anchor (void)
{
  GtkTextView *text_view;
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextChildAnchor *anchor;
  GtkWidget *child;

  text_view = GTK_TEXT_VIEW (gtk_text_view_new ());
  buffer = gtk_text_view_get_buffer (text_view);

  gtk_text_buffer_set_text (buffer, "ab\ncd\r\nef", -1);
  g_assert_true (gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, 0, 1));
  anchor = gtk_text_buffer_create_child_anchor (buffer, &iter);
  child = gtk_label_new ("text");
  gtk_text_view_add_child_at_anchor (text_view, child, anchor);
  g_object_unref (child);

  gtk_text_buffer_get_iter_at_child_anchor (buffer, &iter, anchor);
  g_assert_cmpint (gtk_text_iter_get_char (&iter), ==, GTK_TEXT_UNKNOWN_CHAR);

  g_assert_true (gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, 1, 1));
  /* ß takes 2 bytes in UTF-8 */
  anchor = gtk_text_child_anchor_new_with_replacement ("ß");
  gtk_text_buffer_insert_child_anchor (buffer, &iter, anchor);
  child = gtk_label_new ("text");
  gtk_text_view_add_child_at_anchor (text_view, child, anchor);

  gtk_text_buffer_get_iter_at_child_anchor (buffer, &iter, anchor);
  g_assert_cmpint (gtk_text_iter_get_char (&iter), ==, 223);

  g_object_unref (child);
  g_object_ref_sink (text_view);
}

static void
test_get_text_with_anchor (void)
{
  GtkTextView *text_view;
  GtkTextBuffer *buffer;
  GtkTextIter iter, start, end;
  GtkTextChildAnchor *anchor;
  GtkWidget *child;

  text_view = GTK_TEXT_VIEW (gtk_text_view_new ());
  buffer = gtk_text_view_get_buffer (text_view);

  gtk_text_buffer_set_text (buffer, "ab\ncd\r\nef", -1);
  g_assert_true (gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, 0, 1));
  anchor = gtk_text_buffer_create_child_anchor (buffer, &iter);
  child = gtk_label_new ("text");
  gtk_text_view_add_child_at_anchor (text_view, child, anchor);
  g_object_unref (child);

  g_assert_true (gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, 1, 1));
  /* ß takes 2 bytes in UTF-8 */
  anchor = gtk_text_child_anchor_new_with_replacement ("ß");
  gtk_text_buffer_insert_child_anchor (buffer, &iter, anchor);
  child = gtk_label_new ("text");
  gtk_text_view_add_child_at_anchor (text_view, child, anchor);

  gtk_text_buffer_get_bounds (buffer, &start, &end);
  g_assert_cmpstr (gtk_text_buffer_get_text (buffer, &start, &end, FALSE), ==, "ab\ncßd\r\nef");

  g_object_unref (child);
  g_object_ref_sink (text_view);
}

/* Check that basic undo works */
static void
test_undo0 (void)
{
  GtkTextBuffer *buffer;
  const char *text;

  buffer = gtk_text_buffer_new (NULL);

  g_assert_true (gtk_text_buffer_get_enable_undo (buffer));
  g_assert_false (gtk_text_buffer_get_can_undo (buffer));

  gtk_text_buffer_set_text (buffer, "text before", -1);
  check_buffer_contents (buffer, "text before");
  g_assert_false (gtk_text_buffer_get_can_undo (buffer));

  text = "The quick brown fox jumps over the lazy dog.";
  gtk_text_buffer_insert_at_cursor (buffer, text, strlen (text));
  check_buffer_contents (buffer, "text before"
                                 "The quick brown fox jumps over the lazy dog.");
  g_assert_true (gtk_text_buffer_get_can_undo (buffer));

  text = "Θέλει αρετή και τόλμη η ελευθερία. (Ανδρέας Κάλβος)";
  gtk_text_buffer_insert_at_cursor (buffer, text, strlen (text));
  check_buffer_contents (buffer, "text before"
                                 "The quick brown fox jumps over the lazy dog."
                                 "Θέλει αρετή και τόλμη η ελευθερία. (Ανδρέας Κάλβος)");
  g_assert_true (gtk_text_buffer_get_can_undo (buffer));

  gtk_text_buffer_undo (buffer);

  check_buffer_contents (buffer, "text before"
                                 "The quick brown fox jumps over the lazy dog.");
  g_assert_true (gtk_text_buffer_get_can_undo (buffer));

  gtk_text_buffer_undo (buffer);

  check_buffer_contents (buffer, "text before");
  g_assert_false (gtk_text_buffer_get_can_undo (buffer));

  g_object_unref (buffer);
}

/* Check that bundling user actions works with history */
static void
test_undo1 (void)
{
  GtkTextBuffer *buffer;
  const char *text;

  buffer = gtk_text_buffer_new (NULL);

  g_assert_true (gtk_text_buffer_get_enable_undo (buffer));
  g_assert_false (gtk_text_buffer_get_can_undo (buffer));

  gtk_text_buffer_set_text (buffer, "text before", -1);
  check_buffer_contents (buffer, "text before");
  g_assert_false (gtk_text_buffer_get_can_undo (buffer));

  gtk_text_buffer_begin_user_action (buffer);

  text = "The quick brown fox jumps over the lazy dog.";

  gtk_text_buffer_insert_at_cursor (buffer, text, strlen (text));
  check_buffer_contents (buffer, "text before"
                                 "The quick brown fox jumps over the lazy dog.");

  text = "Θέλει αρετή και τόλμη η ελευθερία. (Ανδρέας Κάλβος)";

  gtk_text_buffer_insert_at_cursor (buffer, text, strlen (text));
  check_buffer_contents (buffer, "text before"
                                 "The quick brown fox jumps over the lazy dog."
                                 "Θέλει αρετή και τόλμη η ελευθερία. (Ανδρέας Κάλβος)");

  gtk_text_buffer_end_user_action (buffer);
  g_assert_true (gtk_text_buffer_get_can_undo (buffer));

  gtk_text_buffer_undo (buffer);
  check_buffer_contents (buffer, "text before");

  g_object_unref (buffer);
}

/* Check that irreversible actions work */
static void
test_undo2 (void)
{
  GtkTextBuffer *buffer;
  const char *text;

  buffer = gtk_text_buffer_new (NULL);

  g_assert_true (gtk_text_buffer_get_enable_undo (buffer));

  gtk_text_buffer_set_text (buffer, "text before", -1);
  check_buffer_contents (buffer, "text before");
  g_assert_false (gtk_text_buffer_get_can_undo (buffer));

  gtk_text_buffer_begin_irreversible_action (buffer);

  text = "The quick brown fox jumps over the lazy dog.";

  gtk_text_buffer_insert_at_cursor (buffer, text, strlen (text));
  check_buffer_contents (buffer, "text before"
                                 "The quick brown fox jumps over the lazy dog.");

  text = "Θέλει αρετή και τόλμη η ελευθερία. (Ανδρέας Κάλβος)";

  gtk_text_buffer_insert_at_cursor (buffer, text, strlen (text));
  check_buffer_contents (buffer, "text before"
                                 "The quick brown fox jumps over the lazy dog."
                                 "Θέλει αρετή και τόλμη η ελευθερία. (Ανδρέας Κάλβος)");

  gtk_text_buffer_end_irreversible_action (buffer);
  g_assert_false (gtk_text_buffer_get_can_undo (buffer));

  g_object_unref (buffer);
}

/* Simulate typing, check that words get batched together */
static void
test_undo3 (void)
{
  GtkTextBuffer *buffer;
  const char *str;
  GtkTextIter iter;

  buffer = gtk_text_buffer_new (NULL);

  str = "abc def. 122";
  for (int i = 0; str[i]; i++)
    gtk_text_buffer_insert_interactive_at_cursor (buffer, &str[i], 1, TRUE);

  gtk_text_buffer_get_end_iter (buffer, &iter);
  gtk_text_buffer_backspace (buffer, &iter, TRUE, TRUE);
  gtk_text_buffer_insert_interactive_at_cursor (buffer, "3", 1, TRUE);

  check_buffer_contents (buffer, "abc def. 123");
  g_assert_true (gtk_text_buffer_get_can_undo (buffer));

  gtk_text_buffer_undo (buffer);

  check_buffer_contents (buffer, "abc def. 12");
  g_assert_true (gtk_text_buffer_get_can_undo (buffer));

  gtk_text_buffer_undo (buffer);

  check_buffer_contents (buffer, "abc def. 122");
  g_assert_true (gtk_text_buffer_get_can_undo (buffer));

  gtk_text_buffer_undo (buffer);

  check_buffer_contents (buffer, "abc def.");
  g_assert_true (gtk_text_buffer_get_can_undo (buffer));

  gtk_text_buffer_undo (buffer);

  check_buffer_contents (buffer, "abc");
  g_assert_true (gtk_text_buffer_get_can_undo (buffer));

  gtk_text_buffer_undo (buffer);

  check_buffer_contents (buffer, "");
  g_assert_false (gtk_text_buffer_get_can_undo (buffer));

  g_object_unref (buffer);
}

static void
test_undo4 (void)
{
  GtkTextBuffer *buffer;
  int count;

  buffer = gtk_text_buffer_new (NULL);

  gtk_text_buffer_insert_interactive_at_cursor (buffer, "0\n", -1, TRUE);
  gtk_text_buffer_insert_interactive_at_cursor (buffer, "1\n", -1, TRUE);
  gtk_text_buffer_insert_interactive_at_cursor (buffer, "2\n", -1, TRUE);
  gtk_text_buffer_insert_interactive_at_cursor (buffer, "3\n", -1, TRUE);
  gtk_text_buffer_insert_interactive_at_cursor (buffer, "4\n", -1, TRUE);

  check_buffer_contents (buffer, "0\n1\n2\n3\n4\n");

  count = 0;
  while (gtk_text_buffer_get_can_undo (buffer))
    {
      count++;
      gtk_text_buffer_undo (buffer);
    }

  g_assert_cmpint (count, ==, 5);

  check_buffer_contents (buffer, "");

  count = 0;
  while (gtk_text_buffer_get_can_redo (buffer))
    {
      count++;
      gtk_text_buffer_redo (buffer);
    }

  g_assert_cmpint (count, ==, 5);

  check_buffer_contents (buffer, "0\n1\n2\n3\n4\n");

  g_object_unref (buffer);
}

static void
test_undo5 (void)
{
  GtkTextBuffer *buffer;
  int count;
  GtkTextIter start, end;

  buffer = gtk_text_buffer_new (NULL);

  gtk_text_buffer_set_text (buffer, "0\n1\n2\n3\n4\n", -1);

  check_buffer_contents (buffer, "0\n1\n2\n3\n4\n");

  count = 0;
  while (TRUE)
    {
      gtk_text_buffer_get_end_iter (buffer, &end);
      start = end;
      if (!gtk_text_iter_backward_line (&start))
        break;
      count++;
      gtk_text_buffer_begin_user_action (buffer);
      gtk_text_buffer_delete_interactive (buffer, &start, &end, TRUE);
      gtk_text_buffer_end_user_action (buffer);
    }

  g_assert_cmpint (count, ==, 5);

  check_buffer_contents (buffer, "");

  g_assert_true (gtk_text_buffer_get_can_undo (buffer));

  gtk_text_buffer_undo (buffer);

  check_buffer_contents (buffer, "0\n1\n2\n3\n4\n");

  g_object_unref (buffer);
}

static void
test_serialize_wrap_mode (void)
{
  GtkTextBuffer *buffer = gtk_text_buffer_new (NULL);
  struct val {
    GtkWrapMode mode;
    const char *name;
  } values[] = {
    { .mode = GTK_WRAP_NONE,      .name = "none" },
    { .mode = GTK_WRAP_CHAR,      .name = "char" },
    { .mode = GTK_WRAP_WORD,      .name = "word" },
    { .mode = GTK_WRAP_WORD_CHAR, .name = "word-char" },
  };
  gsize i;

  /* Four lines, each with a 2-byte run (digit plus newline) */
  gtk_text_buffer_set_text (buffer, "0\n1\n2\n3\n", -1);

  for (i = 0; i < G_N_ELEMENTS (values); i++)
    {
      GtkTextTag *tag = gtk_text_buffer_create_tag (buffer, NULL, "wrap-mode", values[i].mode, NULL);
      GtkTextIter start;
      GtkTextIter end;

      gtk_text_buffer_get_iter_at_offset (buffer, &start, i * 2);
      gtk_text_buffer_get_iter_at_offset (buffer, &end, (i + 1) * 2);
      gtk_text_buffer_apply_tag (buffer, tag, &start, &end);
    }

  /* Get the attributes for each line run as a GVariant */

  for (i = 0; i < G_N_ELEMENTS (values); i++)
    {
      /* Each line has a { wrap-mode: name } run */
      int run_start, run_end;

      GHashTable *attrs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

      gtk_text_buffer_add_run_attributes (buffer, 2 * i, attrs, &run_start, &run_end);

      g_assert_cmpint (run_start, ==, 2 * i);
      g_assert_cmpint (run_end, ==, 2 * (i + 1));

      const char *result = g_hash_table_lookup (attrs, "wrap-mode");
      g_assert_cmpstr (result, ==, values[i].name);
      g_hash_table_unref (attrs);
    }

  g_assert_finalize_object (buffer);
}

int
main (int argc, char** argv)
{
  /* First, we turn on btree debugging. */
  gtk_set_debug_flags (gtk_get_debug_flags () | GTK_DEBUG_TEXT);

  gtk_test_init (&argc, &argv);

  g_test_add_func ("/TextBuffer/UTF8 unknown char", test_utf8);
  g_test_add_func ("/TextBuffer/Line separator", test_line_separator);
  g_test_add_func ("/TextBuffer/Backspace", test_backspace);
  g_test_add_func ("/TextBuffer/Logical motion", test_logical_motion);
  g_test_add_func ("/TextBuffer/Marks", test_marks);
  g_test_add_func ("/TextBuffer/Empty buffer", test_empty_buffer);
  g_test_add_func ("/TextBuffer/Get and Set", test_get_set);
  g_test_add_func ("/TextBuffer/Fill and Empty", test_fill_empty);
  g_test_add_func ("/TextBuffer/Tag", test_tag);
  g_test_add_func ("/TextBuffer/Clipboard", test_clipboard);
  g_test_add_func ("/TextBuffer/Get iter", test_get_iter);
  g_test_add_func ("/TextBuffer/Iter with anchor", test_iter_with_anchor);
  g_test_add_func ("/TextBuffer/Get text with anchor", test_get_text_with_anchor);
  g_test_add_func ("/TextBuffer/Undo 0", test_undo0);
  g_test_add_func ("/TextBuffer/Undo 1", test_undo1);
  g_test_add_func ("/TextBuffer/Undo 2", test_undo2);
  g_test_add_func ("/TextBuffer/Undo 3", test_undo3);
  g_test_add_func ("/TextBuffer/Undo 4", test_undo4);
  g_test_add_func ("/TextBuffer/Undo 5", test_undo5);
  g_test_add_func ("/TextBuffer/Serialize wrap-mode", test_serialize_wrap_mode);

  return g_test_run();
}
