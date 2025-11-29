/* gtkatspitextbuffer.c - GtkTextBuffer-related utilities for AT-SPI
 *
 * Copyright (c) 2020 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.Free
 */

#include "config.h"
#include "gtkatspitextbufferprivate.h"
#include "gtkatspipangoprivate.h"
#include "gtktextbufferprivate.h"
#include "gtktextviewprivate.h"
#include "gtkpangoprivate.h"

char *
gtk_text_view_get_text_before (GtkTextView           *view,
                               int                    offset,
                               AtspiTextBoundaryType  boundary_type,
                               int                   *start_offset,
                               int                   *end_offset)
{
  GtkTextBuffer *buffer;
  GtkTextIter pos, start, end;

  buffer = gtk_text_view_get_buffer (view);
  gtk_text_buffer_get_iter_at_offset (buffer, &pos, offset);
  start = end = pos;

  switch (boundary_type)
    {
    case ATSPI_TEXT_BOUNDARY_CHAR:
      gtk_text_iter_backward_char (&start);
      break;

    case ATSPI_TEXT_BOUNDARY_WORD_START:
      if (!gtk_text_iter_starts_word (&start))
        gtk_text_iter_backward_word_start (&start);
      end = start;
      gtk_text_iter_backward_word_start (&start);
      break;

    case ATSPI_TEXT_BOUNDARY_WORD_END:
      if (gtk_text_iter_inside_word (&start) &&
          !gtk_text_iter_starts_word (&start))
        gtk_text_iter_backward_word_start (&start);
      while (!gtk_text_iter_ends_word (&start))
        {
          if (!gtk_text_iter_backward_char (&start))
            break;
        }
      end = start;
      gtk_text_iter_backward_word_start (&start);
      while (!gtk_text_iter_ends_word (&start))
        {
          if (!gtk_text_iter_backward_char (&start))
            break;
        }
      break;

    case ATSPI_TEXT_BOUNDARY_SENTENCE_START:
      if (!gtk_text_iter_starts_sentence (&start))
        gtk_text_iter_backward_sentence_start (&start);
      end = start;
      gtk_text_iter_backward_sentence_start (&start);
      break;

    case ATSPI_TEXT_BOUNDARY_SENTENCE_END:
      if (gtk_text_iter_inside_sentence (&start) &&
          !gtk_text_iter_starts_sentence (&start))
        gtk_text_iter_backward_sentence_start (&start);
      while (!gtk_text_iter_ends_sentence (&start))
        {
          if (!gtk_text_iter_backward_char (&start))
            break;
        }
      end = start;
      gtk_text_iter_backward_sentence_start (&start);
      while (!gtk_text_iter_ends_sentence (&start))
        {
          if (!gtk_text_iter_backward_char (&start))
            break;
        }
      break;

    case ATSPI_TEXT_BOUNDARY_LINE_START:
      gtk_text_view_backward_display_line_start (view, &start);
      end = start;
      gtk_text_view_backward_display_line (view, &start);
      gtk_text_view_backward_display_line_start (view, &start);
      break;

    case ATSPI_TEXT_BOUNDARY_LINE_END:
      gtk_text_view_backward_display_line_start (view, &start);
      if (!gtk_text_iter_is_start (&start))
        {
          gtk_text_view_backward_display_line (view, &start);
          end = start;
          gtk_text_view_forward_display_line_end (view, &end);
          if (!gtk_text_iter_is_start (&start))
            {
              if (gtk_text_view_backward_display_line (view, &start))
                gtk_text_view_forward_display_line_end (view, &start);
              else
                gtk_text_iter_set_offset (&start, 0);
            }
        }
      else
        end = start;
      break;

    default:
      g_assert_not_reached ();
    }

  *start_offset = gtk_text_iter_get_offset (&start);
  *end_offset = gtk_text_iter_get_offset (&end);

  return gtk_text_buffer_get_slice (buffer, &start, &end, FALSE);
}

char *
gtk_text_view_get_text_at (GtkTextView           *view,
                           int                    offset,
                           AtspiTextBoundaryType  boundary_type,
                           int                   *start_offset,
                           int                   *end_offset)
{
  GtkTextBuffer *buffer;
  GtkTextIter pos, start, end;

  buffer = gtk_text_view_get_buffer (view);
  gtk_text_buffer_get_iter_at_offset (buffer, &pos, offset);
  start = end = pos;

  switch (boundary_type)
    {
    case ATSPI_TEXT_BOUNDARY_CHAR:
      gtk_text_iter_forward_char (&end);
      break;

    case ATSPI_TEXT_BOUNDARY_WORD_START:
      if (!gtk_text_iter_starts_word (&start))
        gtk_text_iter_backward_word_start (&start);
      if (gtk_text_iter_inside_word (&end))
        gtk_text_iter_forward_word_end (&end);
      while (!gtk_text_iter_starts_word (&end))
        {
          if (!gtk_text_iter_forward_char (&end))
            break;
        }
      break;

    case ATSPI_TEXT_BOUNDARY_WORD_END:
      if (gtk_text_iter_inside_word (&start) &&
          !gtk_text_iter_starts_word (&start))
        gtk_text_iter_backward_word_start (&start);
      while (!gtk_text_iter_ends_word (&start))
        {
          if (!gtk_text_iter_backward_char (&start))
            break;
        }
      gtk_text_iter_forward_word_end (&end);
      break;

    case ATSPI_TEXT_BOUNDARY_SENTENCE_START:
      if (!gtk_text_iter_starts_sentence (&start))
        gtk_text_iter_backward_sentence_start (&start);
      if (gtk_text_iter_inside_sentence (&end))
        gtk_text_iter_forward_sentence_end (&end);
      while (!gtk_text_iter_starts_sentence (&end))
        {
          if (!gtk_text_iter_forward_char (&end))
            break;
        }
      break;

    case ATSPI_TEXT_BOUNDARY_SENTENCE_END:
      if (gtk_text_iter_inside_sentence (&start) &&
          !gtk_text_iter_starts_sentence (&start))
        gtk_text_iter_backward_sentence_start (&start);
      while (!gtk_text_iter_ends_sentence (&start))
        {
          if (!gtk_text_iter_backward_char (&start))
            break;
        }
      gtk_text_iter_forward_sentence_end (&end);
      break;

    case ATSPI_TEXT_BOUNDARY_LINE_START:
      gtk_text_view_backward_display_line_start (view, &start);
      gtk_text_view_forward_display_line (view, &end);
      break;

    case ATSPI_TEXT_BOUNDARY_LINE_END:
      gtk_text_view_backward_display_line_start (view, &start);
      if (!gtk_text_iter_is_start (&start))
        {
          gtk_text_view_backward_display_line (view, &start);
          gtk_text_view_forward_display_line_end (view, &start);
        }
      gtk_text_view_forward_display_line_end (view, &end);
      break;

    default:
      g_assert_not_reached ();
    }

  *start_offset = gtk_text_iter_get_offset (&start);
  *end_offset = gtk_text_iter_get_offset (&end);

  return gtk_text_buffer_get_slice (buffer, &start, &end, FALSE);
}

char *
gtk_text_view_get_text_after (GtkTextView           *view,
                              int                    offset,
                              AtspiTextBoundaryType  boundary_type,
                              int                   *start_offset,
                              int                   *end_offset)
{
  GtkTextBuffer *buffer;
  GtkTextIter pos, start, end;

  buffer = gtk_text_view_get_buffer (view);
  gtk_text_buffer_get_iter_at_offset (buffer, &pos, offset);
  start = end = pos;

  switch (boundary_type)
    {
    case ATSPI_TEXT_BOUNDARY_CHAR:
      gtk_text_iter_forward_char (&start);
      gtk_text_iter_forward_chars (&end, 2);
      break;

    case ATSPI_TEXT_BOUNDARY_WORD_START:
      if (gtk_text_iter_inside_word (&end))
        gtk_text_iter_forward_word_end (&end);
      while (!gtk_text_iter_starts_word (&end))
        {
          if (!gtk_text_iter_forward_char (&end))
            break;
        }
      start = end;
      if (!gtk_text_iter_is_end (&end))
        {
          gtk_text_iter_forward_word_end (&end);
          while (!gtk_text_iter_starts_word (&end))
            {
              if (!gtk_text_iter_forward_char (&end))
                break;
            }
        }
      break;

    case ATSPI_TEXT_BOUNDARY_WORD_END:
      gtk_text_iter_forward_word_end (&end);
      start = end;
      if (!gtk_text_iter_is_end (&end))
        gtk_text_iter_forward_word_end (&end);
      break;

    case ATSPI_TEXT_BOUNDARY_SENTENCE_START:
      if (gtk_text_iter_inside_sentence (&end))
        gtk_text_iter_forward_sentence_end (&end);
      while (!gtk_text_iter_starts_sentence (&end))
        {
          if (!gtk_text_iter_forward_char (&end))
            break;
        }
      start = end;
      if (!gtk_text_iter_is_end (&end))
        {
          gtk_text_iter_forward_sentence_end (&end);
          while (!gtk_text_iter_starts_sentence (&end))
            {
              if (!gtk_text_iter_forward_char (&end))
                break;
            }
        }
      break;

    case ATSPI_TEXT_BOUNDARY_SENTENCE_END:
      gtk_text_iter_forward_sentence_end (&end);
      start = end;
      if (!gtk_text_iter_is_end (&end))
        gtk_text_iter_forward_sentence_end (&end);
      break;

    case ATSPI_TEXT_BOUNDARY_LINE_START:
      gtk_text_view_forward_display_line (view, &end);
      start = end;
      gtk_text_view_forward_display_line (view, &end);
      break;

    case ATSPI_TEXT_BOUNDARY_LINE_END:
      gtk_text_view_forward_display_line_end (view, &end);
      start = end;
      gtk_text_view_forward_display_line (view, &end);
      gtk_text_view_forward_display_line_end (view, &end);
      break;

    default:
      g_assert_not_reached ();
    }

  *start_offset = gtk_text_iter_get_offset (&start);
  *end_offset = gtk_text_iter_get_offset (&end);

  return gtk_text_buffer_get_slice (buffer, &start, &end, FALSE);
}

char *
gtk_text_view_get_string_at (GtkTextView           *view,
                             int                    offset,
                             AtspiTextGranularity   granularity,
                             int                   *start_offset,
                             int                   *end_offset)
{
  GtkTextBuffer *buffer;
  GtkTextIter pos, start, end;

  buffer = gtk_text_view_get_buffer (view);
  gtk_text_buffer_get_iter_at_offset (buffer, &pos, offset);
  start = end = pos;

  if (granularity == ATSPI_TEXT_GRANULARITY_CHAR)
    {
      gtk_text_iter_forward_char (&end);
    }
  else if (granularity == ATSPI_TEXT_GRANULARITY_WORD)
    {
      if (!gtk_text_iter_starts_word (&start))
        gtk_text_iter_backward_word_start (&start);
      gtk_text_iter_forward_word_end (&end);
    }
  else if (granularity == ATSPI_TEXT_GRANULARITY_SENTENCE)
    {
      if (!gtk_text_iter_starts_sentence (&start))
        gtk_text_iter_backward_sentence_start (&start);
      gtk_text_iter_forward_sentence_end (&end);
    }
  else if (granularity == ATSPI_TEXT_GRANULARITY_LINE)
    {
      if (!gtk_text_view_starts_display_line (view, &start))
        gtk_text_view_backward_display_line (view, &start);
      gtk_text_view_forward_display_line_end (view, &end);
    }
  else if (granularity == ATSPI_TEXT_GRANULARITY_PARAGRAPH)
    {
      gtk_text_iter_set_line_offset (&start, 0);
      gtk_text_iter_forward_to_line_end (&end);
    }

  *start_offset = gtk_text_iter_get_offset (&start);
  *end_offset = gtk_text_iter_get_offset (&end);

  return gtk_text_buffer_get_slice (buffer, &start, &end, FALSE);
}
