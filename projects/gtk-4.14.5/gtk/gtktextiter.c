/* GTK - The GIMP Toolkit
 * gtktextiter.c Copyright (C) 2000 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"
#include "gtktextiter.h"
#include "gtktextbtreeprivate.h"
#include "gtktextbufferprivate.h"
#include "gtktextiterprivate.h"
#include "gtkdebug.h"

#include <string.h>


/**
 * GtkTextIter:
 *
 * An iterator for the contents of a `GtkTextBuffer`.
 *
 * You may wish to begin by reading the
 * [text widget conceptual overview](section-text-widget.html),
 * which gives an overview of all the objects and data types
 * related to the text widget and how they work together.
 */


#define FIX_OVERFLOWS(varname) if ((varname) == G_MININT) (varname) = G_MININT + 1

typedef struct _GtkTextRealIter GtkTextRealIter;

struct G_GNUC_MAY_ALIAS _GtkTextRealIter
{
  /* Always-valid information */
  GtkTextBTree *tree;
  GtkTextLine *line;
  /* At least one of these is always valid;
     if invalid, they are -1.

     If the line byte offset is valid, so is the segment byte offset;
     and ditto for char offsets. */
  int line_byte_offset;
  int line_char_offset;
  /* These two are valid if >= 0 */
  int cached_char_index;
  int cached_line_number;
  /* Stamps to detect the buffer changing under us */
  int chars_changed_stamp;
  int segments_changed_stamp;
  /* Valid if the segments_changed_stamp is up-to-date */
  GtkTextLineSegment *segment;     /* indexable segment we index */
  GtkTextLineSegment *any_segment; /* first segment in our location,
                                      maybe same as "segment" */
  /* One of these will always be valid if segments_changed_stamp is
     up-to-date. If invalid, they are -1.

     If the line byte offset is valid, so is the segment byte offset;
     and ditto for char offsets. */
  int segment_byte_offset;
  int segment_char_offset;

  /* padding */
  int pad1;
  gpointer pad2;
};

/* These "set" functions should not assume any fields
   other than the char stamp and the tree are valid.
*/
static void
iter_set_common (GtkTextRealIter *iter,
                 GtkTextLine *line)
{
  /* Update segments stamp */
  iter->segments_changed_stamp =
    _gtk_text_btree_get_segments_changed_stamp (iter->tree);

  iter->line = line;

  iter->line_byte_offset = -1;
  iter->line_char_offset = -1;
  iter->segment_byte_offset = -1;
  iter->segment_char_offset = -1;
  iter->cached_char_index = -1;
  iter->cached_line_number = -1;
}

static void
iter_set_from_byte_offset (GtkTextRealIter *iter,
                           GtkTextLine *line,
                           int byte_offset)
{
  iter_set_common (iter, line);

  if (!_gtk_text_line_byte_locate (iter->line,
                                   byte_offset,
                                   &iter->segment,
                                   &iter->any_segment,
                                   &iter->segment_byte_offset,
                                   &iter->line_byte_offset))
    g_error ("Byte index %d is off the end of the line",
             byte_offset);
}

static void
iter_set_from_char_offset (GtkTextRealIter *iter,
                           GtkTextLine *line,
                           int char_offset)
{
  iter_set_common (iter, line);

  if (!_gtk_text_line_char_locate (iter->line,
                                   char_offset,
                                   &iter->segment,
                                   &iter->any_segment,
                                   &iter->segment_char_offset,
                                   &iter->line_char_offset))
    g_error ("Char offset %d is off the end of the line",
             char_offset);
}

static void
iter_set_from_segment (GtkTextRealIter *iter,
                       GtkTextLine *line,
                       GtkTextLineSegment *segment)
{
  GtkTextLineSegment *seg;
  int byte_offset;

  /* This could theoretically be optimized by computing all the iter
     fields in this same loop, but I'm skipping it for now. */
  byte_offset = 0;
  seg = line->segments;
  while (seg != segment)
    {
      byte_offset += seg->byte_count;
      seg = seg->next;
    }

  iter_set_from_byte_offset (iter, line, byte_offset);
}

/* This function ensures that the segment-dependent information is
   truly computed lazily; often we don't need to do the full make_real
   work. This ensures the btree and line are valid, but doesn't
   update the segments. */
static GtkTextRealIter*
gtk_text_iter_make_surreal (const GtkTextIter *_iter)
{
  GtkTextRealIter *iter = (GtkTextRealIter*)_iter;

  if (iter->chars_changed_stamp !=
      _gtk_text_btree_get_chars_changed_stamp (iter->tree))
    {
      g_warning ("Invalid text buffer iterator: either the iterator "
                 "is uninitialized, or the characters/paintables/widgets "
                 "in the buffer have been modified since the iterator "
                 "was created.\nYou must use marks, character numbers, "
                 "or line numbers to preserve a position across buffer "
                 "modifications.\nYou can apply tags and insert marks "
                 "without invalidating your iterators,\n"
                 "but any mutation that affects 'indexable' buffer contents "
                 "(contents that can be referred to by character offset)\n"
                 "will invalidate all outstanding iterators");
      return NULL;
    }

  /* We don't update the segments information since we are becoming
     only surreal. However we do invalidate the segments information
     if appropriate, to be sure we segfault if we try to use it and we
     should have used make_real. */

  if (iter->segments_changed_stamp !=
      _gtk_text_btree_get_segments_changed_stamp (iter->tree))
    {
      iter->segment = NULL;
      iter->any_segment = NULL;
      /* set to segfault-causing values. */
      iter->segment_byte_offset = -10000;
      iter->segment_char_offset = -10000;
    }

  return iter;
}

static GtkTextRealIter*
gtk_text_iter_make_real (const GtkTextIter *_iter)
{
  GtkTextRealIter *iter;

  iter = gtk_text_iter_make_surreal (_iter);
  if (iter == NULL)
    return NULL;

  if (iter->segments_changed_stamp !=
      _gtk_text_btree_get_segments_changed_stamp (iter->tree))
    {
      if (iter->line_byte_offset >= 0)
        {
          iter_set_from_byte_offset (iter,
                                     iter->line,
                                     iter->line_byte_offset);
        }
      else
        {
          g_assert (iter->line_char_offset >= 0);

          iter_set_from_char_offset (iter,
                                     iter->line,
                                     iter->line_char_offset);
        }
    }

  g_assert (iter->segment != NULL);
  g_assert (iter->any_segment != NULL);
  g_assert (iter->segment->char_count > 0);

  return iter;
}

static GtkTextRealIter*
iter_init_common (GtkTextIter *_iter,
                  GtkTextBTree *tree)
{
  GtkTextRealIter *iter = (GtkTextRealIter*)_iter;

  g_assert (iter != NULL);
  g_assert (tree != NULL);

  memset (iter, 0, sizeof (GtkTextRealIter));

  iter->tree = tree;

  iter->chars_changed_stamp =
    _gtk_text_btree_get_chars_changed_stamp (iter->tree);

  return iter;
}

static GtkTextRealIter*
iter_init_from_segment (GtkTextIter *iter,
                        GtkTextBTree *tree,
                        GtkTextLine *line,
                        GtkTextLineSegment *segment)
{
  GtkTextRealIter *real;

  g_return_val_if_fail (line != NULL, NULL);

  real = iter_init_common (iter, tree);

  iter_set_from_segment (real, line, segment);

  return real;
}

static GtkTextRealIter*
iter_init_from_byte_offset (GtkTextIter *iter,
                            GtkTextBTree *tree,
                            GtkTextLine *line,
                            int line_byte_offset)
{
  GtkTextRealIter *real;

  g_return_val_if_fail (line != NULL, NULL);

  real = iter_init_common (iter, tree);

  iter_set_from_byte_offset (real, line, line_byte_offset);

  if (real->segment->type == &gtk_text_char_type &&
      (real->segment->body.chars[real->segment_byte_offset] & 0xc0) == 0x80)
    g_warning ("Incorrect line byte index %d falls in the middle of a UTF-8 "
               "character; this will crash the text buffer. "
               "Byte indexes must refer to the start of a character.",
               line_byte_offset);

  return real;
}

static GtkTextRealIter*
iter_init_from_char_offset (GtkTextIter *iter,
                            GtkTextBTree *tree,
                            GtkTextLine *line,
                            int line_char_offset)
{
  GtkTextRealIter *real;

  g_return_val_if_fail (line != NULL, NULL);

  real = iter_init_common (iter, tree);

  iter_set_from_char_offset (real, line, line_char_offset);

  return real;
}

static inline void
invalidate_char_index (GtkTextRealIter *iter)
{
  iter->cached_char_index = -1;
}

static inline void
adjust_char_index (GtkTextRealIter *iter, int count)
{
  if (iter->cached_char_index >= 0)
    iter->cached_char_index += count;
}

static inline void
adjust_line_number (GtkTextRealIter *iter, int count)
{
  if (iter->cached_line_number >= 0)
    iter->cached_line_number += count;
}

static inline void
ensure_char_offsets (GtkTextRealIter *iter)
{
  if (iter->line_char_offset < 0)
    {
      g_assert (iter->line_byte_offset >= 0);

      _gtk_text_line_byte_to_char_offsets (iter->line,
                                          iter->line_byte_offset,
                                          &iter->line_char_offset,
                                          &iter->segment_char_offset);
    }
}

static inline void
ensure_byte_offsets (GtkTextRealIter *iter)
{
  if (iter->line_byte_offset < 0)
    {
      g_assert (iter->line_char_offset >= 0);

      _gtk_text_line_char_to_byte_offsets (iter->line,
                                          iter->line_char_offset,
                                          &iter->line_byte_offset,
                                          &iter->segment_byte_offset);
    }
}

static inline gboolean
is_segment_start (GtkTextRealIter *real)
{
  return real->segment_byte_offset == 0 || real->segment_char_offset == 0;
}

static void
check_invariants (const GtkTextIter *iter)
{
  if (GTK_DEBUG_CHECK (TEXT))
    _gtk_text_iter_check (iter);
}

/**
 * gtk_text_iter_get_buffer:
 * @iter: an iterator
 *
 * Returns the `GtkTextBuffer` this iterator is associated with.
 *
 * Returns: (transfer none): the buffer
 **/
GtkTextBuffer*
gtk_text_iter_get_buffer (const GtkTextIter *iter)
{
  GtkTextRealIter *real;

  g_return_val_if_fail (iter != NULL, NULL);

  real = gtk_text_iter_make_surreal (iter);

  if (real == NULL)
    return NULL;

  check_invariants (iter);

  return _gtk_text_btree_get_buffer (real->tree);
}

/**
 * gtk_text_iter_copy:
 * @iter: an iterator
 *
 * Creates a dynamically-allocated copy of an iterator.
 *
 * This function is not useful in applications, because
 * iterators can be copied with a simple assignment
 * (`GtkTextIter i = j;`).
 *
 * The function is used by language bindings.
 *
 * Returns: a copy of the @iter, free with [method@Gtk.TextIter.free]
 */
GtkTextIter*
gtk_text_iter_copy (const GtkTextIter *iter)
{
  GtkTextIter *new_iter;

  g_return_val_if_fail (iter != NULL, NULL);

  new_iter = g_new (GtkTextIter, 1);

  *new_iter = *iter;

  return new_iter;
}

/**
 * gtk_text_iter_free:
 * @iter: a dynamically-allocated iterator
 *
 * Free an iterator allocated on the heap.
 *
 * This function is intended for use in language bindings,
 * and is not especially useful for applications, because
 * iterators can simply be allocated on the stack.
 */
void
gtk_text_iter_free (GtkTextIter *iter)
{
  g_return_if_fail (iter != NULL);

  g_free (iter);
}

/**
 * gtk_text_iter_assign:
 * @iter: a `GtkTextIter`
 * @other: another `GtkTextIter`
 *
 * Assigns the value of @other to @iter.
 *
 * This function is not useful in applications, because
 * iterators can be assigned with `GtkTextIter i = j;`.
 *
 * The function is used by language bindings.
 */
void
gtk_text_iter_assign (GtkTextIter       *iter,
                      const GtkTextIter *other)
{
  g_return_if_fail (iter != NULL);
  g_return_if_fail (other != NULL);

  *iter = *other;
}

G_DEFINE_BOXED_TYPE (GtkTextIter, gtk_text_iter,
                     gtk_text_iter_copy,
                     gtk_text_iter_free)

GtkTextLineSegment*
_gtk_text_iter_get_indexable_segment (const GtkTextIter *iter)
{
  GtkTextRealIter *real;

  g_return_val_if_fail (iter != NULL, NULL);

  real = gtk_text_iter_make_real (iter);

  if (real == NULL)
    return NULL;

  check_invariants (iter);

  g_assert (real->segment != NULL);

  return real->segment;
}

GtkTextLineSegment*
_gtk_text_iter_get_any_segment (const GtkTextIter *iter)
{
  GtkTextRealIter *real;

  g_return_val_if_fail (iter != NULL, NULL);

  real = gtk_text_iter_make_real (iter);

  if (real == NULL)
    return NULL;

  check_invariants (iter);

  g_assert (real->any_segment != NULL);

  return real->any_segment;
}

int
_gtk_text_iter_get_segment_byte (const GtkTextIter *iter)
{
  GtkTextRealIter *real;

  g_return_val_if_fail (iter != NULL, 0);

  real = gtk_text_iter_make_real (iter);

  if (real == NULL)
    return 0;

  ensure_byte_offsets (real);

  check_invariants (iter);

  return real->segment_byte_offset;
}

int
_gtk_text_iter_get_segment_char (const GtkTextIter *iter)
{
  GtkTextRealIter *real;

  g_return_val_if_fail (iter != NULL, 0);

  real = gtk_text_iter_make_real (iter);

  if (real == NULL)
    return 0;

  ensure_char_offsets (real);

  check_invariants (iter);

  return real->segment_char_offset;
}

/* This function does not require a still-valid
   iterator */
GtkTextLine*
_gtk_text_iter_get_text_line (const GtkTextIter *iter)
{
  const GtkTextRealIter *real;

  g_return_val_if_fail (iter != NULL, NULL);

  real = (const GtkTextRealIter*)iter;

  return real->line;
}

/* This function does not require a still-valid
   iterator */
GtkTextBTree*
_gtk_text_iter_get_btree (const GtkTextIter *iter)
{
  const GtkTextRealIter *real;

  g_return_val_if_fail (iter != NULL, NULL);

  real = (const GtkTextRealIter*)iter;

  return real->tree;
}

/*
 * Conversions
 */

/**
 * gtk_text_iter_get_offset:
 * @iter: an iterator
 *
 * Returns the character offset of an iterator.
 *
 * Each character in a `GtkTextBuffer` has an offset,
 * starting with 0 for the first character in the buffer.
 * Use [method@Gtk.TextBuffer.get_iter_at_offset] to convert
 * an offset back into an iterator.
 *
 * Returns: a character offset
 */
int
gtk_text_iter_get_offset (const GtkTextIter *iter)
{
  GtkTextRealIter *real;

  g_return_val_if_fail (iter != NULL, 0);

  real = gtk_text_iter_make_surreal (iter);

  if (real == NULL)
    return 0;

  check_invariants (iter);

  if (real->cached_char_index < 0)
    {
      ensure_char_offsets (real);

      real->cached_char_index =
        _gtk_text_line_char_index (real->line);
      real->cached_char_index += real->line_char_offset;
    }

  check_invariants (iter);

  return real->cached_char_index;
}

/**
 * gtk_text_iter_get_line:
 * @iter: an iterator
 *
 * Returns the line number containing the iterator.
 *
 * Lines in a `GtkTextBuffer` are numbered beginning
 * with 0 for the first line in the buffer.
 *
 * Returns: a line number
 */
int
gtk_text_iter_get_line (const GtkTextIter *iter)
{
  GtkTextRealIter *real;

  g_return_val_if_fail (iter != NULL, 0);

  real = gtk_text_iter_make_surreal (iter);

  if (real == NULL)
    return 0;

  if (real->cached_line_number < 0)
    real->cached_line_number =
      _gtk_text_line_get_number (real->line);

  check_invariants (iter);

  return real->cached_line_number;
}

/**
 * gtk_text_iter_get_line_offset:
 * @iter: an iterator
 *
 * Returns the character offset of the iterator,
 * counting from the start of a newline-terminated line.
 *
 * The first character on the line has offset 0.
 *
 * Returns: offset from start of line
 */
int
gtk_text_iter_get_line_offset (const GtkTextIter *iter)
{
  GtkTextRealIter *real;

  g_return_val_if_fail (iter != NULL, 0);

  real = gtk_text_iter_make_surreal (iter);

  if (real == NULL)
    return 0;

  ensure_char_offsets (real);

  check_invariants (iter);

  return real->line_char_offset;
}

/**
 * gtk_text_iter_get_line_index:
 * @iter: an iterator
 *
 * Returns the byte index of the iterator, counting
 * from the start of a newline-terminated line.
 *
 * Remember that `GtkTextBuffer` encodes text in
 * UTF-8, and that characters can require a variable
 * number of bytes to represent.
 *
 * Returns: distance from start of line, in bytes
 */
int
gtk_text_iter_get_line_index (const GtkTextIter *iter)
{
  GtkTextRealIter *real;

  g_return_val_if_fail (iter != NULL, 0);

  real = gtk_text_iter_make_surreal (iter);

  if (real == NULL)
    return 0;

  ensure_byte_offsets (real);

  check_invariants (iter);

  return real->line_byte_offset;
}

/**
 * gtk_text_iter_get_visible_line_offset:
 * @iter: a `GtkTextIter`
 *
 * Returns the offset in characters from the start of the
 * line to the given @iter, not counting characters that
 * are invisible due to tags with the “invisible” flag
 * toggled on.
 *
 * Returns: offset in visible characters from the start of the line
 */
int
gtk_text_iter_get_visible_line_offset (const GtkTextIter *iter)
{
  GtkTextRealIter *real;
  int vis_offset;
  GtkTextLineSegment *seg;
  GtkTextIter pos;

  g_return_val_if_fail (iter != NULL, 0);

  real = gtk_text_iter_make_real (iter);

  if (real == NULL)
    return 0;

  ensure_char_offsets (real);

  check_invariants (iter);

  vis_offset = real->line_char_offset;

  g_assert (vis_offset >= 0);

  _gtk_text_btree_get_iter_at_line (real->tree,
                                    &pos,
                                    real->line,
                                    0);

  seg = _gtk_text_iter_get_indexable_segment (&pos);

  while (seg != real->segment)
    {
      /* This is a pretty expensive call, making the
       * whole function pretty lame; we could keep track
       * of current invisibility state by looking at toggle
       * segments as we loop, and then call this function
       * only once per line, in order to speed up the loop
       * quite a lot.
       */
      if (_gtk_text_btree_char_is_invisible (&pos))
        vis_offset -= seg->char_count;

      _gtk_text_iter_forward_indexable_segment (&pos);

      seg = _gtk_text_iter_get_indexable_segment (&pos);
    }

  if (_gtk_text_btree_char_is_invisible (&pos))
    vis_offset -= real->segment_char_offset;

  return vis_offset;
}


/**
 * gtk_text_iter_get_visible_line_index:
 * @iter: a `GtkTextIter`
 *
 * Returns the number of bytes from the start of the
 * line to the given @iter, not counting bytes that
 * are invisible due to tags with the “invisible” flag
 * toggled on.
 *
 * Returns: byte index of @iter with respect to the start of the line
 */
int
gtk_text_iter_get_visible_line_index (const GtkTextIter *iter)
{
  GtkTextRealIter *real;
  int vis_offset;
  GtkTextLineSegment *seg;
  GtkTextIter pos;

  g_return_val_if_fail (iter != NULL, 0);

  real = gtk_text_iter_make_real (iter);

  if (real == NULL)
    return 0;

  ensure_byte_offsets (real);

  check_invariants (iter);

  vis_offset = real->line_byte_offset;

  g_assert (vis_offset >= 0);

  _gtk_text_btree_get_iter_at_line (real->tree,
                                    &pos,
                                    real->line,
                                    0);

  seg = _gtk_text_iter_get_indexable_segment (&pos);

  while (seg != real->segment)
    {
      /* This is a pretty expensive call, making the
       * whole function pretty lame; we could keep track
       * of current invisibility state by looking at toggle
       * segments as we loop, and then call this function
       * only once per line, in order to speed up the loop
       * quite a lot.
       */
      if (_gtk_text_btree_char_is_invisible (&pos))
        vis_offset -= seg->byte_count;

      _gtk_text_iter_forward_indexable_segment (&pos);

      seg = _gtk_text_iter_get_indexable_segment (&pos);
    }

  if (_gtk_text_btree_char_is_invisible (&pos))
    vis_offset -= real->segment_byte_offset;

  return vis_offset;
}

/*
 * Dereferencing
 */

/**
 * gtk_text_iter_get_char:
 * @iter: an iterator
 *
 * The Unicode character at this iterator is returned.
 *
 * Equivalent to operator* on a C++ iterator. If the element at
 * this iterator is a non-character element, such as an image
 * embedded in the buffer, the Unicode “unknown” character 0xFFFC
 * is returned. If invoked on the end iterator, zero is returned;
 * zero is not a valid Unicode character.
 *
 * So you can write a loop which ends when this function returns 0.
 *
 * Returns: a Unicode character, or 0 if @iter is not dereferenceable
 */
gunichar
gtk_text_iter_get_char (const GtkTextIter *iter)
{
  GtkTextRealIter *real;

  g_return_val_if_fail (iter != NULL, 0);

  real = gtk_text_iter_make_real (iter);

  if (real == NULL)
    return 0;

  check_invariants (iter);

  if (gtk_text_iter_is_end (iter))
    return 0;
  else if (real->segment->type == &gtk_text_char_type)
    {
      ensure_byte_offsets (real);

      return g_utf8_get_char (real->segment->body.chars +
                              real->segment_byte_offset);
    }
  else if (real->segment->type == &gtk_text_child_type)
    {
      return g_utf8_get_char (gtk_text_child_anchor_get_replacement (real->segment->body.child.obj));
    }
  else
    {
      /* Unicode "unknown character" 0xFFFC */
      return GTK_TEXT_UNKNOWN_CHAR;
    }
}

/**
 * gtk_text_iter_get_slice:
 * @start: iterator at start of a range
 * @end: iterator at end of a range
 *
 * Returns the text in the given range.
 *
 * A “slice” is an array of characters encoded in UTF-8 format,
 * including the Unicode “unknown” character 0xFFFC for iterable
 * non-character elements in the buffer, such as images.
 * Because images are encoded in the slice, byte and
 * character offsets in the returned array will correspond to byte
 * offsets in the text buffer. Note that 0xFFFC can occur in normal
 * text as well, so it is not a reliable indicator that a paintable or
 * widget is in the buffer.
 *
 * Returns: (transfer full): slice of text from the buffer
 */
char *
gtk_text_iter_get_slice       (const GtkTextIter *start,
                               const GtkTextIter *end)
{
  g_return_val_if_fail (start != NULL, NULL);
  g_return_val_if_fail (end != NULL, NULL);

  check_invariants (start);
  check_invariants (end);

  return _gtk_text_btree_get_text (start, end, TRUE, TRUE);
}

/**
 * gtk_text_iter_get_text:
 * @start: iterator at start of a range
 * @end: iterator at end of a range
 *
 * Returns text in the given range.
 *
 * If the range
 * contains non-text elements such as images, the character and byte
 * offsets in the returned string will not correspond to character and
 * byte offsets in the buffer. If you want offsets to correspond, see
 * [method@Gtk.TextIter.get_slice].
 *
 * Returns: (transfer full): array of characters from the buffer
 */
char *
gtk_text_iter_get_text       (const GtkTextIter *start,
                              const GtkTextIter *end)
{
  g_return_val_if_fail (start != NULL, NULL);
  g_return_val_if_fail (end != NULL, NULL);

  check_invariants (start);
  check_invariants (end);

  return _gtk_text_btree_get_text (start, end, TRUE, FALSE);
}

/**
 * gtk_text_iter_get_visible_slice:
 * @start: iterator at start of range
 * @end: iterator at end of range
 *
 * Returns visible text in the given range.
 *
 * Like [method@Gtk.TextIter.get_slice], but invisible text
 * is not included. Invisible text is usually invisible because
 * a `GtkTextTag` with the “invisible” attribute turned on has
 * been applied to it.
 *
 * Returns: (transfer full): slice of text from the buffer
 */
char *
gtk_text_iter_get_visible_slice (const GtkTextIter  *start,
                                 const GtkTextIter  *end)
{
  g_return_val_if_fail (start != NULL, NULL);
  g_return_val_if_fail (end != NULL, NULL);

  check_invariants (start);
  check_invariants (end);

  return _gtk_text_btree_get_text (start, end, FALSE, TRUE);
}

/**
 * gtk_text_iter_get_visible_text:
 * @start: iterator at start of range
 * @end: iterator at end of range
 *
 * Returns visible text in the given range.
 *
 * Like [method@Gtk.TextIter.get_text], but invisible text
 * is not included. Invisible text is usually invisible because
 * a `GtkTextTag` with the “invisible” attribute turned on has
 * been applied to it.
 *
 * Returns: (transfer full): string containing visible text in the
 * range
 */
char *
gtk_text_iter_get_visible_text (const GtkTextIter  *start,
                                const GtkTextIter  *end)
{
  g_return_val_if_fail (start != NULL, NULL);
  g_return_val_if_fail (end != NULL, NULL);

  check_invariants (start);
  check_invariants (end);

  return _gtk_text_btree_get_text (start, end, FALSE, FALSE);
}

/**
 * gtk_text_iter_get_paintable:
 * @iter: an iterator
 *
 * If the element at @iter is a paintable, the paintable is returned.
 *
 * Otherwise, %NULL is returned.
 *
 * Returns: (transfer none) (nullable): the paintable at @iter
 **/
GdkPaintable *
gtk_text_iter_get_paintable (const GtkTextIter *iter)
{
  GtkTextRealIter *real;

  g_return_val_if_fail (iter != NULL, NULL);

  real = gtk_text_iter_make_real (iter);

  if (real == NULL)
    return NULL;

  check_invariants (iter);

  if (real->segment->type != &gtk_text_paintable_type)
    return NULL;
  else
    return real->segment->body.paintable.paintable;
}

/**
 * gtk_text_iter_get_child_anchor:
 * @iter: an iterator
 *
 * If the location at @iter contains a child anchor, the
 * anchor is returned.
 *
 * Otherwise, %NULL is returned.
 *
 * Returns: (transfer none) (nullable): the anchor at @iter
 **/
GtkTextChildAnchor*
gtk_text_iter_get_child_anchor (const GtkTextIter *iter)
{
  GtkTextRealIter *real;

  g_return_val_if_fail (iter != NULL, NULL);

  real = gtk_text_iter_make_real (iter);

  if (real == NULL)
    return NULL;

  check_invariants (iter);

  if (real->segment->type != &gtk_text_child_type)
    return NULL;
  else
    return real->segment->body.child.obj;
}

/**
 * gtk_text_iter_get_marks:
 * @iter: an iterator
 *
 * Returns a list of all `GtkTextMark` at this location.
 *
 * Because marks are not iterable (they don’t take up any "space"
 * in the buffer, they are just marks in between iterable locations),
 * multiple marks can exist in the same place.
 *
 * The returned list is not in any meaningful order.
 *
 * Returns: (element-type GtkTextMark) (transfer container):
 *   list of `GtkTextMark`
 */
GSList*
gtk_text_iter_get_marks (const GtkTextIter *iter)
{
  GtkTextRealIter *real;
  GtkTextLineSegment *seg;
  GSList *retval;

  g_return_val_if_fail (iter != NULL, NULL);

  real = gtk_text_iter_make_real (iter);

  if (real == NULL)
    return NULL;

  check_invariants (iter);

  retval = NULL;
  seg = real->any_segment;
  while (seg != real->segment)
    {
      if (seg->type == &gtk_text_left_mark_type ||
          seg->type == &gtk_text_right_mark_type)
        retval = g_slist_prepend (retval, seg->body.mark.obj);

      seg = seg->next;
    }

  /* The returned list isn't guaranteed to be in any special order,
     and it isn't. */
  return retval;
}

/**
 * gtk_text_iter_get_toggled_tags:
 * @iter: an iterator
 * @toggled_on: %TRUE to get toggled-on tags
 *
 * Returns a list of `GtkTextTag` that are toggled on or off at this
 * point.
 *
 * If @toggled_on is %TRUE, the list contains tags that are
 * toggled on. If a tag is toggled on at @iter, then some non-empty
 * range of characters following @iter has that tag applied to it.  If
 * a tag is toggled off, then some non-empty range following @iter
 * does not have the tag applied to it.
 *
 * Returns: (element-type GtkTextTag) (transfer container): tags
 *   toggled at this point
 */
GSList*
gtk_text_iter_get_toggled_tags  (const GtkTextIter  *iter,
                                 gboolean            toggled_on)
{
  GtkTextRealIter *real;
  GtkTextLineSegment *seg;
  GSList *retval;

  g_return_val_if_fail (iter != NULL, NULL);

  real = gtk_text_iter_make_real (iter);

  if (real == NULL)
    return NULL;

  check_invariants (iter);

  retval = NULL;
  seg = real->any_segment;
  while (seg != real->segment)
    {
      if (toggled_on)
        {
          if (seg->type == &gtk_text_toggle_on_type)
            {
              retval = g_slist_prepend (retval, seg->body.toggle.info->tag);
            }
        }
      else
        {
          if (seg->type == &gtk_text_toggle_off_type)
            {
              retval = g_slist_prepend (retval, seg->body.toggle.info->tag);
            }
        }

      seg = seg->next;
    }

  /* The returned list isn't guaranteed to be in any special order,
     and it isn't. */
  return retval;
}

/**
 * gtk_text_iter_starts_tag:
 * @iter: an iterator
 * @tag: (nullable): a `GtkTextTag`
 *
 * Returns %TRUE if @tag is toggled on at exactly this point.
 *
 * If @tag is %NULL, returns %TRUE if any tag is toggled on at this point.
 *
 * Note that if this function returns %TRUE, it means that
 * @iter is at the beginning of the tagged range, and that the
 * character at @iter is inside the tagged range. In other
 * words, unlike [method@Gtk.TextIter.ends_tag], if
 * this function returns %TRUE, [method@Gtk.TextIter.has_tag]
 * will also return %TRUE for the same parameters.
 *
 * Returns: whether @iter is the start of a range tagged with @tag
 **/
gboolean
gtk_text_iter_starts_tag (const GtkTextIter *iter,
                          GtkTextTag        *tag)
{
  GtkTextRealIter *real;
  GtkTextLineSegment *seg;

  g_return_val_if_fail (iter != NULL, FALSE);

  real = gtk_text_iter_make_real (iter);

  if (real == NULL)
    return FALSE;

  check_invariants (iter);

  seg = real->any_segment;
  while (seg != real->segment)
    {
      if (seg->type == &gtk_text_toggle_on_type)
        {
          if (tag == NULL ||
              seg->body.toggle.info->tag == tag)
            return TRUE;
        }

      seg = seg->next;
    }

  return FALSE;
}

/**
 * gtk_text_iter_ends_tag:
 * @iter: an iterator
 * @tag: (nullable): a `GtkTextTag`
 *
 * Returns %TRUE if @tag is toggled off at exactly this point.
 *
 * If @tag is %NULL, returns %TRUE if any tag is toggled off at this point.
 *
 * Note that if this function returns %TRUE, it means that
 * @iter is at the end of the tagged range, but that the character
 * at @iter is outside the tagged range. In other words,
 * unlike [method@Gtk.TextIter.starts_tag], if this function
 * returns %TRUE, [method@Gtk.TextIter.has_tag] will return
 * %FALSE for the same parameters.
 *
 * Returns: whether @iter is the end of a range tagged with @tag
 */
gboolean
gtk_text_iter_ends_tag   (const GtkTextIter  *iter,
                          GtkTextTag         *tag)
{
  GtkTextRealIter *real;
  GtkTextLineSegment *seg;

  g_return_val_if_fail (iter != NULL, FALSE);

  real = gtk_text_iter_make_real (iter);

  if (real == NULL)
    return FALSE;

  check_invariants (iter);

  seg = real->any_segment;
  while (seg != real->segment)
    {
      if (seg->type == &gtk_text_toggle_off_type)
        {
          if (tag == NULL ||
              seg->body.toggle.info->tag == tag)
            return TRUE;
        }

      seg = seg->next;
    }

  return FALSE;
}

/**
 * gtk_text_iter_toggles_tag:
 * @iter: an iterator
 * @tag: (nullable): a `GtkTextTag`
 *
 * Gets whether a range with @tag applied to it begins
 * or ends at @iter.
 *
 * This is equivalent to (gtk_text_iter_starts_tag() ||
 * gtk_text_iter_ends_tag())
 *
 * Returns: whether @tag is toggled on or off at @iter
 */
gboolean
gtk_text_iter_toggles_tag (const GtkTextIter  *iter,
                           GtkTextTag         *tag)
{
  GtkTextRealIter *real;
  GtkTextLineSegment *seg;

  g_return_val_if_fail (iter != NULL, FALSE);

  real = gtk_text_iter_make_real (iter);

  if (real == NULL)
    return FALSE;

  check_invariants (iter);

  seg = real->any_segment;
  while (seg != real->segment)
    {
      if ( (seg->type == &gtk_text_toggle_off_type ||
            seg->type == &gtk_text_toggle_on_type) &&
           (tag == NULL ||
            seg->body.toggle.info->tag == tag) )
        return TRUE;

      seg = seg->next;
    }

  return FALSE;
}

/**
 * gtk_text_iter_has_tag:
 * @iter: an iterator
 * @tag: a `GtkTextTag`
 *
 * Returns %TRUE if @iter points to a character that is part
 * of a range tagged with @tag.
 *
 * See also [method@Gtk.TextIter.starts_tag] and
 * [method@Gtk.TextIter.ends_tag].
 *
 * Returns: whether @iter is tagged with @tag
 */
gboolean
gtk_text_iter_has_tag (const GtkTextIter   *iter,
                       GtkTextTag          *tag)
{
  GtkTextRealIter *real;

  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TEXT_TAG (tag), FALSE);

  real = gtk_text_iter_make_surreal (iter);

  if (real == NULL)
    return FALSE;

  check_invariants (iter);

  if (real->line_byte_offset >= 0)
    {
      return _gtk_text_line_byte_has_tag (real->line, real->tree,
                                          real->line_byte_offset, tag);
    }
  else
    {
      g_assert (real->line_char_offset >= 0);
      return _gtk_text_line_char_has_tag (real->line, real->tree,
                                          real->line_char_offset, tag);
    }
}

/**
 * gtk_text_iter_get_tags:
 * @iter: a `GtkTextIter`
 *
 * Returns a list of tags that apply to @iter, in ascending order of
 * priority.
 *
 * The highest-priority tags are last.
 *
 * The `GtkTextTag`s in the list don’t have a reference added,
 * but you have to free the list itself.
 *
 * Returns: (element-type GtkTextTag) (transfer container): list of
 *   `GtkTextTag`
 */
GSList*
gtk_text_iter_get_tags (const GtkTextIter *iter)
{
  GPtrArray *tags;
  GSList *retval;

  g_return_val_if_fail (iter != NULL, NULL);

  /* Get the tags at this spot */
  tags = _gtk_text_btree_get_tags (iter);

  /* No tags, use default style */
  if (tags == NULL || tags->len == 0)
    {
      if (tags)
        g_ptr_array_unref (tags);
      return NULL;
    }

  retval = NULL;

  for (int i = tags->len - 1; i >= 0; i--)
    retval = g_slist_prepend (retval, g_ptr_array_index (tags, i));

  g_ptr_array_unref (tags);

  return retval;
}

/**
 * gtk_text_iter_editable:
 * @iter: an iterator
 * @default_setting: %TRUE if text is editable by default
 *
 * Returns whether the character at @iter is within an editable region
 * of text.
 *
 * Non-editable text is “locked” and can’t be changed by the
 * user via `GtkTextView`. If no tags applied to this text affect
 * editability, @default_setting will be returned.
 *
 * You don’t want to use this function to decide whether text can be
 * inserted at @iter, because for insertion you don’t want to know
 * whether the char at @iter is inside an editable range, you want to
 * know whether a new character inserted at @iter would be inside an
 * editable range. Use [method@Gtk.TextIter.can_insert] to handle this
 * case.
 *
 * Returns: whether @iter is inside an editable range
 */
gboolean
gtk_text_iter_editable (const GtkTextIter *iter,
                        gboolean           default_setting)
{
  GtkTextAttributes *values;
  gboolean retval;

  g_return_val_if_fail (iter != NULL, FALSE);

  values = gtk_text_attributes_new ();

  values->editable = default_setting;

  gtk_text_iter_get_attributes (iter, values);

  retval = values->editable;

  gtk_text_attributes_unref (values);

  return retval;
}

/**
 * gtk_text_iter_can_insert:
 * @iter: an iterator
 * @default_editability: %TRUE if text is editable by default
 *
 * Considering the default editability of the buffer, and tags that
 * affect editability, determines whether text inserted at @iter would
 * be editable.
 *
 * If text inserted at @iter would be editable then the
 * user should be allowed to insert text at @iter.
 * [method@Gtk.TextBuffer.insert_interactive] uses this function
 * to decide whether insertions are allowed at a given position.
 *
 * Returns: whether text inserted at @iter would be editable
 */
gboolean
gtk_text_iter_can_insert (const GtkTextIter *iter,
                          gboolean           default_editability)
{
  g_return_val_if_fail (iter != NULL, FALSE);

  if (gtk_text_iter_editable (iter, default_editability))
    return TRUE;
  /* If at start/end of buffer, default editability is used */
  else if ((gtk_text_iter_is_start (iter) ||
            gtk_text_iter_is_end (iter)) &&
           default_editability)
    return TRUE;
  else
    {
      /* if iter isn't editable, and the char before iter is,
       * then iter is the first char in an editable region
       * and thus insertion at iter results in editable text.
       */
      GtkTextIter prev = *iter;
      gtk_text_iter_backward_char (&prev);
      return gtk_text_iter_editable (&prev, default_editability);
    }
}

gboolean
gtk_text_iter_get_attributes (const GtkTextIter  *iter,
                              GtkTextAttributes  *values)
{
  GPtrArray *tags;

  /* Get the tags at this spot */
  tags = _gtk_text_btree_get_tags (iter);

  /* No tags, use default style */
  if (tags == NULL || tags->len == 0)
    {
      if (tags)
        g_ptr_array_unref (tags);
      return FALSE;
    }

  _gtk_text_attributes_fill_from_tags (values, tags);

  g_ptr_array_unref (tags);

  return TRUE;
}

/**
 * gtk_text_iter_get_language:
 * @iter: an iterator
 *
 * Returns the language in effect at @iter.
 *
 * If no tags affecting language apply to @iter, the return
 * value is identical to that of [func@Gtk.get_default_language].
 *
 * Returns: (transfer full): language in effect at @iter
 */
PangoLanguage *
gtk_text_iter_get_language (const GtkTextIter *iter)
{
  GtkTextAttributes *values;
  PangoLanguage *retval;

  values = gtk_text_attributes_new ();

  gtk_text_iter_get_attributes (iter, values);

  retval = values->language;

  gtk_text_attributes_unref (values);

  return retval;
}

/**
 * gtk_text_iter_starts_line:
 * @iter: an iterator
 *
 * Returns %TRUE if @iter begins a paragraph.
 *
 * This is the case if [method@Gtk.TextIter.get_line_offset]
 * would return 0. However this function is potentially more
 * efficient than [method@Gtk.TextIter.get_line_offset], because
 * it doesn’t have to compute the offset, it just has to see
 * whether it’s 0.
 *
 * Returns: whether @iter begins a line
 */
gboolean
gtk_text_iter_starts_line (const GtkTextIter   *iter)
{
  GtkTextRealIter *real;

  g_return_val_if_fail (iter != NULL, FALSE);

  real = gtk_text_iter_make_surreal (iter);

  if (real == NULL)
    return FALSE;

  check_invariants (iter);

  if (real->line_byte_offset >= 0)
    {
      return (real->line_byte_offset == 0);
    }
  else
    {
      g_assert (real->line_char_offset >= 0);
      return (real->line_char_offset == 0);
    }
}

/**
 * gtk_text_iter_ends_line:
 * @iter: an iterator
 *
 * Returns %TRUE if @iter points to the start of the paragraph
 * delimiter characters for a line.
 *
 * Delimiters will be either a newline, a carriage return, a carriage
 * return followed by a newline, or a Unicode paragraph separator
 * character.
 *
 * Note that an iterator pointing to the \n of a \r\n pair will not be
 * counted as the end of a line, the line ends before the \r. The end
 * iterator is considered to be at the end of a line, even though there
 * are no paragraph delimiter chars there.
 *
 * Returns: whether @iter is at the end of a line
 */
gboolean
gtk_text_iter_ends_line (const GtkTextIter   *iter)
{
  gunichar wc;

  g_return_val_if_fail (iter != NULL, FALSE);

  check_invariants (iter);

  /* Only one character has type G_UNICODE_PARAGRAPH_SEPARATOR in
   * Unicode 3.0; update this if that changes.
   */
#define PARAGRAPH_SEPARATOR 0x2029

  wc = gtk_text_iter_get_char (iter);

  if (wc == '\r' || wc == PARAGRAPH_SEPARATOR || wc == 0) /* wc == 0 is end iterator */
    return TRUE;
  else if (wc == '\n')
    {
      GtkTextIter tmp = *iter;

      /* need to determine if a \r precedes the \n, in which case
       * we aren't the end of the line.
       * Note however that if \r and \n are on different lines, they
       * both are terminators. This for instance may happen after
       * deleting some text:

          1 some text\r    delete 'a'    1 some text\r
          2 a\n            --------->    2 \n
          3 ...                          3 ...

       */

      if (gtk_text_iter_get_line_offset (&tmp) == 0)
        return TRUE;

      if (!gtk_text_iter_backward_char (&tmp))
        return TRUE;

      return gtk_text_iter_get_char (&tmp) != '\r';
    }
  else
    return FALSE;
}

/**
 * gtk_text_iter_is_end:
 * @iter: an iterator
 *
 * Returns %TRUE if @iter is the end iterator.
 *
 * This means it is one past the last dereferenceable iterator
 * in the buffer. [method@Gtk.TextIter.is_end] is the most efficient
 * way to check whether an iterator is the end iterator.
 *
 * Returns: whether @iter is the end iterator
 */
gboolean
gtk_text_iter_is_end (const GtkTextIter *iter)
{
  GtkTextRealIter *real;

  g_return_val_if_fail (iter != NULL, FALSE);

  real = gtk_text_iter_make_surreal (iter);

  if (real == NULL)
    return FALSE;

  check_invariants (iter);

  if (!_gtk_text_line_contains_end_iter (real->line, real->tree))
    return FALSE;

  /* Now we need the segments validated */
  real = gtk_text_iter_make_real (iter);

  if (real == NULL)
    return FALSE;

  return _gtk_text_btree_is_end (real->tree, real->line,
                                 real->segment,
                                 real->segment_byte_offset,
                                 real->segment_char_offset);
}

/**
 * gtk_text_iter_is_start:
 * @iter: an iterator
 *
 * Returns %TRUE if @iter is the first iterator in the buffer.
 *
 * Returns: whether @iter is the first in the buffer
 */
gboolean
gtk_text_iter_is_start (const GtkTextIter *iter)
{
  return gtk_text_iter_get_offset (iter) == 0;
}

/**
 * gtk_text_iter_get_chars_in_line:
 * @iter: an iterator
 *
 * Returns the number of characters in the line containing @iter,
 * including the paragraph delimiters.
 *
 * Returns: number of characters in the line
 */
int
gtk_text_iter_get_chars_in_line (const GtkTextIter   *iter)
{
  GtkTextRealIter *real;
  int count;
  GtkTextLineSegment *seg;

  g_return_val_if_fail (iter != NULL, 0);

  real = gtk_text_iter_make_surreal (iter);

  if (real == NULL)
    return 0;

  check_invariants (iter);

  if (real->line_char_offset >= 0)
    {
      /* We can start at the segments we've already found. */
      count = real->line_char_offset - real->segment_char_offset;
      seg = _gtk_text_iter_get_indexable_segment (iter);
    }
  else
    {
      /* count whole line. */
      seg = real->line->segments;
      count = 0;
    }


  while (seg != NULL)
    {
      count += seg->char_count;

      seg = seg->next;
    }

  if (_gtk_text_line_contains_end_iter (real->line, real->tree))
    count -= 1; /* Dump the newline that was in the last segment of the end iter line */

  return count;
}

/**
 * gtk_text_iter_get_bytes_in_line:
 * @iter: an iterator
 *
 * Returns the number of bytes in the line containing @iter,
 * including the paragraph delimiters.
 *
 * Returns: number of bytes in the line
 */
int
gtk_text_iter_get_bytes_in_line (const GtkTextIter   *iter)
{
  GtkTextRealIter *real;
  int count;
  GtkTextLineSegment *seg;

  g_return_val_if_fail (iter != NULL, 0);

  real = gtk_text_iter_make_surreal (iter);

  if (real == NULL)
    return 0;

  check_invariants (iter);

  if (real->line_byte_offset >= 0)
    {
      /* We can start at the segments we've already found. */
      count = real->line_byte_offset - real->segment_byte_offset;
      seg = _gtk_text_iter_get_indexable_segment (iter);
    }
  else
    {
      /* count whole line. */
      seg = real->line->segments;
      count = 0;
    }

  while (seg != NULL)
    {
      count += seg->byte_count;

      seg = seg->next;
    }

  if (_gtk_text_line_contains_end_iter (real->line, real->tree))
    count -= 1; /* Dump the newline that was in the last segment of the end iter line */

  return count;
}

/*
 * Increments/decrements
 */

/* The return value of this indicates WHETHER WE MOVED.
 * The return value of public functions indicates
 * (MOVEMENT OCCURRED && NEW ITER IS DEREFERENCEABLE)
 *
 * This function will not change the iterator if
 * it’s already on the last (end iter) line, i.e. it
 * won’t move to the end of the last line.
 */
static gboolean
forward_line_leaving_caches_unmodified (GtkTextRealIter *real)
{
  if (!_gtk_text_line_contains_end_iter (real->line, real->tree))
    {
      GtkTextLine *new_line;

      new_line = _gtk_text_line_next (real->line);
      g_assert (new_line);
      g_assert (new_line != real->line);
      g_assert (!_gtk_text_line_is_last (new_line, real->tree));

      real->line = new_line;

      real->line_byte_offset = 0;
      real->line_char_offset = 0;

      real->segment_byte_offset = 0;
      real->segment_char_offset = 0;

      /* Find first segments in new line */
      real->any_segment = real->line->segments;
      real->segment = real->any_segment;
      while (real->segment->char_count == 0)
        real->segment = real->segment->next;

      return TRUE;
    }
  else
    {
      /* There is no way to move forward a line; we were already at
       * the line containing the end iterator.
       * However we may not be at the end iterator itself.
       */

      return FALSE;
    }
}

#if 0
/* The return value of this indicates WHETHER WE MOVED.
 * The return value of public functions indicates
 * (MOVEMENT OCCURRED && NEW ITER IS DEREFERENCEABLE)
 *
 * This function is currently unused, thus it is #if-0-ed. It is
 * left here, since it’s non-trivial code that might be useful in
 * the future.
 */
static gboolean
backward_line_leaving_caches_unmodified (GtkTextRealIter *real)
{
  GtkTextLine *new_line;

  new_line = _gtk_text_line_previous (real->line);

  g_assert (new_line != real->line);

  if (new_line != NULL)
    {
      real->line = new_line;

      real->line_byte_offset = 0;
      real->line_char_offset = 0;

      real->segment_byte_offset = 0;
      real->segment_char_offset = 0;

      /* Find first segments in new line */
      real->any_segment = real->line->segments;
      real->segment = real->any_segment;
      while (real->segment->char_count == 0)
        real->segment = real->segment->next;

      return TRUE;
    }
  else
    {
      /* There is no way to move backward; we were already
         at the first line. */

      /* We leave real->line as-is */

      /* Note that we didn't clamp to the start of the first line. */

      return FALSE;
    }
}
#endif

/* The return value indicates (MOVEMENT OCCURRED && NEW ITER IS
 * DEREFERENCEABLE)
 */
static gboolean
forward_char (GtkTextRealIter *real)
{
  GtkTextIter *iter = (GtkTextIter*)real;

  check_invariants ((GtkTextIter*)real);

  ensure_char_offsets (real);

  if ( (real->segment_char_offset + 1) == real->segment->char_count)
    {
      /* Need to move to the next segment; if no next segment,
         need to move to next line. */
      return _gtk_text_iter_forward_indexable_segment (iter);
    }
  else
    {
      /* Just moving within a segment. Keep byte count
         up-to-date, if it was already up-to-date. */

      g_assert (real->segment->type == &gtk_text_char_type);

      if (real->line_byte_offset >= 0)
        {
          int bytes;
          const char * start =
            real->segment->body.chars + real->segment_byte_offset;

          bytes = g_utf8_next_char (start) - start;

          real->line_byte_offset += bytes;
          real->segment_byte_offset += bytes;

          g_assert (real->segment_byte_offset < real->segment->byte_count);
        }

      real->line_char_offset += 1;
      real->segment_char_offset += 1;

      adjust_char_index (real, 1);

      g_assert (real->segment_char_offset < real->segment->char_count);

      /* We moved into the middle of a segment, so the any_segment
         must now be the segment we're in the middle of. */
      real->any_segment = real->segment;

      check_invariants ((GtkTextIter*)real);

      if (gtk_text_iter_is_end ((GtkTextIter*)real))
        return FALSE;
      else
        return TRUE;
    }
}

gboolean
_gtk_text_iter_forward_indexable_segment (GtkTextIter *iter)
{
  /* Need to move to the next segment; if no next segment,
     need to move to next line. */
  GtkTextLineSegment *seg;
  GtkTextLineSegment *any_seg;
  GtkTextRealIter *real;
  int chars_skipped;
  int bytes_skipped;

  g_return_val_if_fail (iter != NULL, FALSE);

  real = gtk_text_iter_make_real (iter);

  if (real == NULL)
    return FALSE;

  check_invariants (iter);

  if (real->line_char_offset >= 0)
    {
      chars_skipped = real->segment->char_count - real->segment_char_offset;
      g_assert (chars_skipped > 0);
    }
  else
    chars_skipped = 0;

  if (real->line_byte_offset >= 0)
    {
      bytes_skipped = real->segment->byte_count - real->segment_byte_offset;
      g_assert (bytes_skipped > 0);
    }
  else
    bytes_skipped = 0;

  /* Get first segment of any kind */
  any_seg = real->segment->next;
  /* skip non-indexable segments, if any */
  seg = any_seg;
  while (seg != NULL && seg->char_count == 0)
    seg = seg->next;

  if (seg != NULL)
    {
      real->any_segment = any_seg;
      real->segment = seg;

      if (real->line_byte_offset >= 0)
        {
          g_assert (bytes_skipped > 0);
          real->segment_byte_offset = 0;
          real->line_byte_offset += bytes_skipped;
        }

      if (real->line_char_offset >= 0)
        {
          g_assert (chars_skipped > 0);
          real->segment_char_offset = 0;
          real->line_char_offset += chars_skipped;
          adjust_char_index (real, chars_skipped);
        }

      check_invariants (iter);

      return !gtk_text_iter_is_end (iter);
    }
  else
    {
      /* End of the line */
      if (forward_line_leaving_caches_unmodified (real))
        {
          adjust_line_number (real, 1);
          if (real->line_char_offset >= 0)
            adjust_char_index (real, chars_skipped);

          g_assert (real->line_byte_offset == 0);
          g_assert (real->line_char_offset == 0);
          g_assert (real->segment_byte_offset == 0);
          g_assert (real->segment_char_offset == 0);
          g_assert (gtk_text_iter_starts_line (iter));

          check_invariants (iter);

          return !gtk_text_iter_is_end (iter);
        }
      else
        {
          /* End of buffer, but iter is still at start of last segment,
           * not at the end iterator. We put it on the end iterator.
           */

          check_invariants (iter);

          g_assert (!_gtk_text_line_is_last (real->line, real->tree));
          g_assert (_gtk_text_line_contains_end_iter (real->line, real->tree));

          gtk_text_iter_forward_to_line_end (iter);

          g_assert (gtk_text_iter_is_end (iter));

          return FALSE;
        }
    }
}

static gboolean
at_last_indexable_segment (GtkTextRealIter *real)
{
  GtkTextLineSegment *seg;

  /* Return TRUE if there are no indexable segments after
   * this iterator.
   */

  seg = real->segment->next;
  while (seg)
    {
      if (seg->char_count > 0)
        return FALSE;
      seg = seg->next;
    }
  return TRUE;
}

/* Goes back to the start of the next segment, even if
 * we’re not at the start of the current segment (always
 * ends up on a different segment if it returns TRUE)
 */
gboolean
_gtk_text_iter_backward_indexable_segment (GtkTextIter *iter)
{
  /* Move to the start of the previous segment; if no previous
   * segment, to the last segment in the previous line. This is
   * inherently a bit inefficient due to the singly-linked list and
   * tree nodes, but we can't afford the RAM for doubly-linked.
   */
  GtkTextRealIter *real;
  GtkTextLineSegment *seg;
  GtkTextLineSegment *any_seg;
  GtkTextLineSegment *prev_seg;
  GtkTextLineSegment *prev_any_seg;
  int bytes_skipped;
  int chars_skipped;

  g_return_val_if_fail (iter != NULL, FALSE);

  real = gtk_text_iter_make_real (iter);

  if (real == NULL)
    return FALSE;

  check_invariants (iter);

  /* Find first segments in line */
  any_seg = real->line->segments;
  seg = any_seg;
  while (seg->char_count == 0)
    seg = seg->next;

  if (seg == real->segment)
    {
      /* Could probably do this case faster by hand-coding the
       * iteration.
       */

      /* We were already at the start of a line;
       * go back to the previous line.
       */
      if (gtk_text_iter_backward_line (iter))
        {
          /* Go forward to last indexable segment in line. */
          while (!at_last_indexable_segment (real))
            _gtk_text_iter_forward_indexable_segment (iter);

          check_invariants (iter);

          return TRUE;
        }
      else
        return FALSE; /* We were at the start of the first line. */
    }

  /* We must be in the middle of a line; so find the indexable
   * segment just before our current segment.
   */
  g_assert (seg != real->segment);
  do
    {
      prev_seg = seg;
      prev_any_seg = any_seg;

      any_seg = seg->next;
      seg = any_seg;
      while (seg->char_count == 0)
        seg = seg->next;
    }
  while (seg != real->segment);

  g_assert (prev_seg != NULL);
  g_assert (prev_any_seg != NULL);
  g_assert (prev_seg->char_count > 0);

  /* We skipped the entire previous segment, plus any
   * chars we were into the current segment.
   */
  if (real->segment_byte_offset >= 0)
    bytes_skipped = prev_seg->byte_count + real->segment_byte_offset;
  else
    bytes_skipped = -1;

  if (real->segment_char_offset >= 0)
    chars_skipped = prev_seg->char_count + real->segment_char_offset;
  else
    chars_skipped = -1;

  real->segment = prev_seg;
  real->any_segment = prev_any_seg;
  real->segment_byte_offset = 0;
  real->segment_char_offset = 0;

  if (bytes_skipped >= 0)
    {
      if (real->line_byte_offset >= 0)
        {
          real->line_byte_offset -= bytes_skipped;
          g_assert (real->line_byte_offset >= 0);
        }
    }
  else
    real->line_byte_offset = -1;

  if (chars_skipped >= 0)
    {
      if (real->line_char_offset >= 0)
        {
          real->line_char_offset -= chars_skipped;
          g_assert (real->line_char_offset >= 0);
        }

      if (real->cached_char_index >= 0)
        {
          real->cached_char_index -= chars_skipped;
          g_assert (real->cached_char_index >= 0);
        }
    }
  else
    {
      real->line_char_offset = -1;
      real->cached_char_index = -1;
    }

  /* line number is unchanged. */

  check_invariants (iter);

  return TRUE;
}

/**
 * gtk_text_iter_forward_char:
 * @iter: an iterator
 *
 * Moves @iter forward by one character offset.
 *
 * Note that images embedded in the buffer occupy 1 character slot, so
 * this function may actually move onto an image instead of a character,
 * if you have images in your buffer. If @iter is the end iterator or
 * one character before it, @iter will now point at the end iterator,
 * and this function returns %FALSE for convenience when writing loops.
 *
 * Returns: whether @iter moved and is dereferenceable
 */
gboolean
gtk_text_iter_forward_char (GtkTextIter *iter)
{
  GtkTextRealIter *real;

  g_return_val_if_fail (iter != NULL, FALSE);

  real = gtk_text_iter_make_real (iter);

  if (real == NULL)
    return FALSE;
  else
    {
      check_invariants (iter);
      return forward_char (real);
    }
}

/**
 * gtk_text_iter_backward_char:
 * @iter: an iterator
 *
 * Moves backward by one character offset.
 *
 * Returns %TRUE if movement was possible; if @iter was the first
 * in the buffer (character offset 0), this function returns %FALSE
 * for convenience when writing loops.
 *
 * Returns: whether movement was possible
 */
gboolean
gtk_text_iter_backward_char (GtkTextIter *iter)
{
  g_return_val_if_fail (iter != NULL, FALSE);

  check_invariants (iter);

  return gtk_text_iter_backward_chars (iter, 1);
}

/*
  Definitely we should try to linear scan as often as possible for
  movement within a single line, because we can't use the BTree to
  speed within-line searches up; for movement between lines, we would
  like to avoid the linear scan probably.

  Instead of using this constant, it might be nice to cache the line
  length in the iterator and linear scan if motion is within a single
  line.

  I guess you'd have to profile the various approaches.
*/
#define MAX_LINEAR_SCAN 150


/**
 * gtk_text_iter_forward_chars:
 * @iter: an iterator
 * @count: number of characters to move, may be negative
 *
 * Moves @count characters if possible.
 *
 * If @count would move past the start or end of the buffer,
 * moves to the start or end of the buffer.
 *
 * The return value indicates whether the new position of
 * @iter is different from its original position, and dereferenceable
 * (the last iterator in the buffer is not dereferenceable). If @count
 * is 0, the function does nothing and returns %FALSE.
 *
 * Returns: whether @iter moved and is dereferenceable
 */
gboolean
gtk_text_iter_forward_chars (GtkTextIter *iter, int count)
{
  GtkTextRealIter *real;

  g_return_val_if_fail (iter != NULL, FALSE);

  FIX_OVERFLOWS (count);

  real = gtk_text_iter_make_real (iter);

  if (real == NULL)
    return FALSE;
  else if (count == 0)
    return FALSE;
  else if (count < 0)
    return gtk_text_iter_backward_chars (iter, 0 - count);
  else if (count < MAX_LINEAR_SCAN)
    {
      check_invariants (iter);

      while (count > 1)
        {
          if (!forward_char (real))
            return FALSE;
          --count;
        }

      return forward_char (real);
    }
  else
    {
      int current_char_index;
      int new_char_index;

      check_invariants (iter);

      current_char_index = gtk_text_iter_get_offset (iter);

      if (current_char_index == _gtk_text_btree_char_count (real->tree))
        return FALSE; /* can't move forward */

      new_char_index = current_char_index + count;
      gtk_text_iter_set_offset (iter, new_char_index);

      check_invariants (iter);

      /* Return FALSE if we're on the non-dereferenceable end
       * iterator.
       */
      if (gtk_text_iter_is_end (iter))
        return FALSE;
      else
        return TRUE;
    }
}

/**
 * gtk_text_iter_backward_chars:
 * @iter: an iterator
 * @count: number of characters to move
 *
 * Moves @count characters backward, if possible.
 *
 * If @count would move past the start or end of the buffer, moves
 * to the start or end of the buffer.
 *
 * The return value indicates whether the iterator moved
 * onto a dereferenceable position; if the iterator didn’t move, or
 * moved onto the end iterator, then %FALSE is returned. If @count is 0,
 * the function does nothing and returns %FALSE.
 *
 * Returns: whether @iter moved and is dereferenceable
 */
gboolean
gtk_text_iter_backward_chars (GtkTextIter *iter, int count)
{
  GtkTextRealIter *real;

  g_return_val_if_fail (iter != NULL, FALSE);

  FIX_OVERFLOWS (count);

  real = gtk_text_iter_make_real (iter);

  if (real == NULL)
    return FALSE;
  else if (count == 0)
    return FALSE;
  else if (count < 0)
    return gtk_text_iter_forward_chars (iter, 0 - count);

  ensure_char_offsets (real);
  check_invariants (iter);

  /* <, not <=, because if count == segment_char_offset
   * we're going to the front of the segment and the any_segment
   * might change
   */
  if (count < real->segment_char_offset)
    {
      /* Optimize the within-segment case */
      g_assert (real->segment->char_count > 0);
      g_assert (real->segment->type == &gtk_text_char_type);

      if (real->line_byte_offset >= 0)
        {
          const char *p;
          int new_byte_offset;

          /* if in the last fourth of the segment walk backwards */
          if (count < real->segment_char_offset / 4)
            p = g_utf8_offset_to_pointer (real->segment->body.chars + real->segment_byte_offset,
                                          -count);
          else
            p = g_utf8_offset_to_pointer (real->segment->body.chars,
                                          real->segment_char_offset - count);

          new_byte_offset = p - real->segment->body.chars;
          real->line_byte_offset -= (real->segment_byte_offset - new_byte_offset);
          real->segment_byte_offset = new_byte_offset;
        }

      real->segment_char_offset -= count;
      real->line_char_offset -= count;

      adjust_char_index (real, 0 - count);

      check_invariants (iter);

      return TRUE;
    }
  else
    {
      /* We need to go back into previous segments. For now,
       * just keep this really simple. FIXME
       * use backward_indexable_segment.
       */
      if (TRUE || count > MAX_LINEAR_SCAN)
        {
          int current_char_index;
          int new_char_index;

          current_char_index = gtk_text_iter_get_offset (iter);

          if (current_char_index == 0)
            return FALSE; /* can't move backward */

          new_char_index = current_char_index - count;
          if (new_char_index < 0)
            new_char_index = 0;

          gtk_text_iter_set_offset (iter, new_char_index);

          check_invariants (iter);

          return TRUE;
        }
      else
        {
          /* FIXME backward_indexable_segment here */

          return FALSE;
        }
    }
}

#if 0

/* These two can't be implemented efficiently (always have to use
 * a linear scan, since that’s the only way to find all the non-text
 * segments)
 */

/**
 * gtk_text_iter_forward_text_chars:
 * @iter: a `GtkTextIter`
 * @count: number of chars to move
 *
 * Moves forward by @count text characters.
 *
 * Paintables, widgets, etc. do not count as characters for this.
 *
 * Equivalent to moving through the results of gtk_text_iter_get_text(),
 * rather than gtk_text_iter_get_slice().
 *
 * Returns: whether @iter moved and is dereferenceable
 */
gboolean
gtk_text_iter_forward_text_chars  (GtkTextIter *iter,
                                   int          count)
{



}

/**
 * gtk_text_iter_backward_text_chars:
 * @iter: a `GtkTextIter`
 * @count: number of chars to move
 *
 * Moves backward by @count text characters (paintables, widgets,
 * etc. do not count as characters for this). Equivalent to moving
 * through the results of gtk_text_iter_get_text(), rather than
 * gtk_text_iter_get_slice().
 *
 * Returns: whether @iter moved and is dereferenceable
 **/
gboolean
gtk_text_iter_backward_text_chars (GtkTextIter *iter,
                                   int          count)
{


}
#endif

/**
 * gtk_text_iter_forward_line:
 * @iter: an iterator
 *
 * Moves @iter to the start of the next line.
 *
 * If the iter is already on the last line of the buffer,
 * moves the iter to the end of the current line. If after
 * the operation, the iter is at the end of the buffer and not
 * dereferenceable, returns %FALSE. Otherwise, returns %TRUE.
 *
 * Returns: whether @iter can be dereferenced
 */
gboolean
gtk_text_iter_forward_line (GtkTextIter *iter)
{
  GtkTextRealIter *real;

  g_return_val_if_fail (iter != NULL, FALSE);

  real = gtk_text_iter_make_real (iter);

  if (real == NULL)
    return FALSE;

  check_invariants (iter);

  if (forward_line_leaving_caches_unmodified (real))
    {
      invalidate_char_index (real);
      adjust_line_number (real, 1);

      check_invariants (iter);

      if (gtk_text_iter_is_end (iter))
        return FALSE;
      else
        return TRUE;
    }
  else
    {
      /* On the last line, move to end of it */

      if (!gtk_text_iter_is_end (iter))
        gtk_text_iter_forward_to_end (iter);

      check_invariants (iter);
      return FALSE;
    }
}

/**
 * gtk_text_iter_backward_line:
 * @iter: an iterator
 *
 * Moves @iter to the start of the previous line.
 *
 * Returns %TRUE if @iter could be moved; i.e. if @iter was at
 * character offset 0, this function returns %FALSE. Therefore,
 * if @iter was already on line 0, but not at the start of the line,
 * @iter is snapped to the start of the line and the function returns
 * %TRUE. (Note that this implies that
 * in a loop calling this function, the line number may not change on
 * every iteration, if your first iteration is on line 0.)
 *
 * Returns: whether @iter moved
 */
gboolean
gtk_text_iter_backward_line (GtkTextIter *iter)
{
  GtkTextLine *new_line;
  GtkTextRealIter *real;
  gboolean offset_will_change;
  int offset;

  g_return_val_if_fail (iter != NULL, FALSE);

  real = gtk_text_iter_make_real (iter);

  if (real == NULL)
    return FALSE;

  ensure_char_offsets (real);

  check_invariants (iter);

  new_line = _gtk_text_line_previous (real->line);

  offset_will_change = FALSE;
  if (real->line_char_offset > 0)
    offset_will_change = TRUE;

  if (new_line != NULL)
    {
      real->line = new_line;

      adjust_line_number (real, -1);
    }
  else
    {
      if (!offset_will_change)
        return FALSE;
    }

  invalidate_char_index (real);

  real->line_byte_offset = 0;
  real->line_char_offset = 0;

  real->segment_byte_offset = 0;
  real->segment_char_offset = 0;

  /* Find first segment in line */
  real->any_segment = real->line->segments;
  real->segment = _gtk_text_line_byte_to_segment (real->line,
                                                  0, &offset);

  g_assert (offset == 0);

  /* Note that if we are on the first line, we snap to the start of
   * the first line and return TRUE, so TRUE means the iterator
   * changed, not that the line changed; this is maybe a bit
   * weird. I'm not sure there's an obvious right thing to do though.
   */

  check_invariants (iter);

  return TRUE;
}


/**
 * gtk_text_iter_forward_lines:
 * @iter: a `GtkTextIter`
 * @count: number of lines to move forward
 *
 * Moves @count lines forward, if possible.
 *
 * If @count would move past the start or end of the buffer, moves to
 * the start or end of the buffer.
 *
 * The return value indicates whether the iterator moved
 * onto a dereferenceable position; if the iterator didn’t move, or
 * moved onto the end iterator, then %FALSE is returned. If @count is 0,
 * the function does nothing and returns %FALSE. If @count is negative,
 * moves backward by 0 - @count lines.
 *
 * Returns: whether @iter moved and is dereferenceable
 */
gboolean
gtk_text_iter_forward_lines (GtkTextIter *iter, int count)
{
  FIX_OVERFLOWS (count);

  if (count < 0)
    return gtk_text_iter_backward_lines (iter, 0 - count);
  else if (count == 0)
    return FALSE;
  else if (count == 1)
    {
      check_invariants (iter);
      return gtk_text_iter_forward_line (iter);
    }
  else
    {
      int old_line;

      if (gtk_text_iter_is_end (iter))
        return FALSE;

      old_line = gtk_text_iter_get_line (iter);

      gtk_text_iter_set_line (iter, old_line + count);

      if ((gtk_text_iter_get_line (iter) - old_line) < count)
        {
          /* count went past the last line, so move to end of last line */
          if (!gtk_text_iter_is_end (iter))
            gtk_text_iter_forward_to_end (iter);
        }

      return !gtk_text_iter_is_end (iter);
    }
}

/**
 * gtk_text_iter_backward_lines:
 * @iter: a `GtkTextIter`
 * @count: number of lines to move backward
 *
 * Moves @count lines backward, if possible.
 *
 * If @count would move past the start or end of the buffer, moves to
 * the start or end of the buffer.
 *
 * The return value indicates whether the iterator moved
 * onto a dereferenceable position; if the iterator didn’t move, or
 * moved onto the end iterator, then %FALSE is returned. If @count is 0,
 * the function does nothing and returns %FALSE. If @count is negative,
 * moves forward by 0 - @count lines.
 *
 * Returns: whether @iter moved and is dereferenceable
 */
gboolean
gtk_text_iter_backward_lines (GtkTextIter *iter, int count)
{
  FIX_OVERFLOWS (count);

  if (count < 0)
    return gtk_text_iter_forward_lines (iter, 0 - count);
  else if (count == 0)
    return FALSE;
  else if (count == 1)
    {
      return gtk_text_iter_backward_line (iter);
    }
  else
    {
      int old_line;

      old_line = gtk_text_iter_get_line (iter);

      gtk_text_iter_set_line (iter, MAX (old_line - count, 0));

      return (gtk_text_iter_get_line (iter) != old_line);
    }
}

/**
 * gtk_text_iter_forward_visible_line:
 * @iter: an iterator
 *
 * Moves @iter to the start of the next visible line.
 *
 * Returns %TRUE if there
 * was a next line to move to, and %FALSE if @iter was simply moved to
 * the end of the buffer and is now not dereferenceable, or if @iter was
 * already at the end of the buffer.
 *
 * Returns: whether @iter can be dereferenced
 */
gboolean
gtk_text_iter_forward_visible_line (GtkTextIter *iter)
{
  while (gtk_text_iter_forward_line (iter))
    {
      if (!_gtk_text_btree_char_is_invisible (iter))
        return TRUE;
      else
        {
          do
            {
              if (!gtk_text_iter_forward_char (iter))
                return FALSE;

              if (!_gtk_text_btree_char_is_invisible (iter))
                return TRUE;
            }
          while (!gtk_text_iter_ends_line (iter));
        }
    }

  return FALSE;
}

/**
 * gtk_text_iter_backward_visible_line:
 * @iter: an iterator
 *
 * Moves @iter to the start of the previous visible line.
 *
 * Returns %TRUE if
 * @iter could be moved; i.e. if @iter was at character offset 0, this
 * function returns %FALSE. Therefore if @iter was already on line 0,
 * but not at the start of the line, @iter is snapped to the start of
 * the line and the function returns %TRUE. (Note that this implies that
 * in a loop calling this function, the line number may not change on
 * every iteration, if your first iteration is on line 0.)
 *
 * Returns: whether @iter moved
 */
gboolean
gtk_text_iter_backward_visible_line (GtkTextIter *iter)
{
  while (gtk_text_iter_backward_line (iter))
    {
      if (!_gtk_text_btree_char_is_invisible (iter))
        return TRUE;
      else
        {
          do
            {
              if (!gtk_text_iter_backward_char (iter))
                return FALSE;

              if (!_gtk_text_btree_char_is_invisible (iter))
                return TRUE;
            }
          while (!gtk_text_iter_starts_line (iter));
        }
    }

  return FALSE;
}

/**
 * gtk_text_iter_forward_visible_lines:
 * @iter: a `GtkTextIter`
 * @count: number of lines to move forward
 *
 * Moves @count visible lines forward, if possible.
 *
 * If @count would move past the start or end of the buffer, moves to
 * the start or end of the buffer.
 *
 * The return value indicates whether the iterator moved
 * onto a dereferenceable position; if the iterator didn’t move, or
 * moved onto the end iterator, then %FALSE is returned. If @count is 0,
 * the function does nothing and returns %FALSE. If @count is negative,
 * moves backward by 0 - @count lines.
 *
 * Returns: whether @iter moved and is dereferenceable
 */
gboolean
gtk_text_iter_forward_visible_lines (GtkTextIter *iter,
                                     int          count)
{
  FIX_OVERFLOWS (count);

  if (count < 0)
    return gtk_text_iter_backward_visible_lines (iter, 0 - count);
  else if (count == 0)
    return FALSE;
  else if (count == 1)
    {
      check_invariants (iter);
      return gtk_text_iter_forward_visible_line (iter);
    }
  else
    {
      while (gtk_text_iter_forward_visible_line (iter) && count > 0)
        count--;
      return count == 0;
    }
}

/**
 * gtk_text_iter_backward_visible_lines:
 * @iter: a `GtkTextIter`
 * @count: number of lines to move backward
 *
 * Moves @count visible lines backward, if possible.
 *
 * If @count would move past the start or end of the buffer, moves to
 * the start or end of the buffer.
 *
 * The return value indicates whether the iterator moved
 * onto a dereferenceable position; if the iterator didn’t move, or
 * moved onto the end iterator, then %FALSE is returned. If @count is 0,
 * the function does nothing and returns %FALSE. If @count is negative,
 * moves forward by 0 - @count lines.
 *
 * Returns: whether @iter moved and is dereferenceable
 */
gboolean
gtk_text_iter_backward_visible_lines (GtkTextIter *iter,
                                      int          count)
{
  FIX_OVERFLOWS (count);

  if (count < 0)
    return gtk_text_iter_forward_visible_lines (iter, 0 - count);
  else if (count == 0)
    return FALSE;
  else if (count == 1)
    {
      return gtk_text_iter_backward_visible_line (iter);
    }
  else
    {
      while (gtk_text_iter_backward_visible_line (iter) && count > 0)
        count--;
      return count == 0;
    }
}

typedef gboolean (* FindLogAttrFunc) (const PangoLogAttr *attrs,
                                      int                 offset,
                                      int                 len,
                                      int                *found_offset,
                                      gboolean            already_moved_initially);

typedef gboolean (* TestLogAttrFunc) (const PangoLogAttr *attrs,
                                      int                 offset,
                                      int                 min_offset,
                                      int                 len);

/* Word funcs */

static gboolean
find_word_end_func (const PangoLogAttr *attrs,
                    int                 offset,
                    int                 len,
                    int                *found_offset,
                    gboolean            already_moved_initially)
{
  if (!already_moved_initially)
    ++offset;

  /* Find end of next word */
  while (offset <= len)
    {
      if (attrs[offset].is_word_end)
        {
          *found_offset = offset;
          return TRUE;
        }

      ++offset;
    }

  return FALSE;
}

static gboolean
is_word_end_func (const PangoLogAttr *attrs,
                  int                 offset,
                  int                 min_offset,
                  int                 len)
{
  return attrs[offset].is_word_end;
}

static gboolean
find_word_start_func (const PangoLogAttr *attrs,
                      int                 offset,
                      int                 len,
                      int                *found_offset,
                      gboolean            already_moved_initially)
{
  if (!already_moved_initially)
    --offset;

  /* Find start of prev word */
  while (offset >= 0)
    {
      if (attrs[offset].is_word_start)
        {
          *found_offset = offset;
          return TRUE;
        }

      --offset;
    }

  return FALSE;
}

static gboolean
is_word_start_func (const PangoLogAttr *attrs,
                    int                 offset,
                    int                 min_offset,
                    int                 len)
{
  return attrs[offset].is_word_start;
}

static gboolean
inside_word_func (const PangoLogAttr *attrs,
                  int                 offset,
                  int                 min_offset,
                  int                 len)
{
  /* Find next word start or end */
  while (offset >= min_offset &&
         !(attrs[offset].is_word_start || attrs[offset].is_word_end))
    --offset;

  if (offset >= 0)
    return attrs[offset].is_word_start;
  else
    return FALSE;
}

/* Sentence funcs */

static gboolean
find_sentence_end_func (const PangoLogAttr *attrs,
                        int                 offset,
                        int                 len,
                        int                *found_offset,
                        gboolean            already_moved_initially)
{
  if (!already_moved_initially)
    ++offset;

  /* Find end of next sentence */
  while (offset <= len)
    {
      if (attrs[offset].is_sentence_end)
        {
          *found_offset = offset;
          return TRUE;
        }

      ++offset;
    }

  return FALSE;
}

static gboolean
is_sentence_end_func (const PangoLogAttr *attrs,
                      int                 offset,
                      int                 min_offset,
                      int                 len)
{
  return attrs[offset].is_sentence_end;
}

static gboolean
find_sentence_start_func (const PangoLogAttr *attrs,
                          int                 offset,
                          int                 len,
                          int                *found_offset,
                          gboolean            already_moved_initially)
{
  if (!already_moved_initially)
    --offset;

  /* Find start of prev sentence */
  while (offset >= 0)
    {
      if (attrs[offset].is_sentence_start)
        {
          *found_offset = offset;
          return TRUE;
        }

      --offset;
    }

  return FALSE;
}

static gboolean
is_sentence_start_func (const PangoLogAttr *attrs,
                        int                 offset,
                        int                 min_offset,
                        int                 len)
{
  return attrs[offset].is_sentence_start;
}

static gboolean
inside_sentence_func (const PangoLogAttr *attrs,
                      int                 offset,
                      int                 min_offset,
                      int                 len)
{
  /* Find next sentence start or end */
  while (!(attrs[offset].is_sentence_start || attrs[offset].is_sentence_end))
    {
      --offset;
      if (offset < min_offset)
        return FALSE;
    }

  return attrs[offset].is_sentence_start;
}

static gboolean
test_log_attrs (const GtkTextIter *iter,
                TestLogAttrFunc    func)
{
  int char_len;
  const PangoLogAttr *attrs;
  int offset;

  g_return_val_if_fail (iter != NULL, FALSE);

  attrs = _gtk_text_buffer_get_line_log_attrs (gtk_text_iter_get_buffer (iter),
                                               iter, &char_len);

  offset = gtk_text_iter_get_line_offset (iter);

  g_assert (offset <= char_len);

  return (* func) (attrs, offset, 0, char_len);
}

static gboolean
find_line_log_attrs (const GtkTextIter *iter,
                     FindLogAttrFunc    func,
                     int               *found_offset,
                     gboolean           already_moved_initially)
{
  int char_len;
  const PangoLogAttr *attrs;
  int offset;

  g_return_val_if_fail (iter != NULL, FALSE);

  attrs = _gtk_text_buffer_get_line_log_attrs (gtk_text_iter_get_buffer (iter),
                                               iter, &char_len);

  offset = gtk_text_iter_get_line_offset (iter);

  return (* func) (attrs,
                   offset,
                   char_len,
                   found_offset,
                   already_moved_initially);
}

static gboolean
find_by_log_attrs (GtkTextIter     *arg_iter,
                   FindLogAttrFunc  func,
                   gboolean         forward)
{
  GtkTextIter iter;
  gboolean already_moved_initially = FALSE;

  g_return_val_if_fail (arg_iter != NULL, FALSE);

  iter = *arg_iter;

  while (TRUE)
    {
      int offset = 0;
      gboolean found;

      found = find_line_log_attrs (&iter, func, &offset, already_moved_initially);

      if (found)
        {
          gboolean moved;

          gtk_text_iter_set_line_offset (&iter, offset);

          moved = !gtk_text_iter_equal (&iter, arg_iter);

          *arg_iter = iter;
          return moved && !gtk_text_iter_is_end (arg_iter);
        }

      if (forward)
        {
          if (!gtk_text_iter_forward_line (&iter))
            return FALSE;

          already_moved_initially = TRUE;
        }
      else
        {
          /* Go to end of previous line. First go to the current line offset 0,
           * because backward_line() snaps to start of line 0 if iter is already
           * on line 0.
           */
          gtk_text_iter_set_line_offset (&iter, 0);

          if (gtk_text_iter_backward_line (&iter))
            {
              if (!gtk_text_iter_ends_line (&iter))
                gtk_text_iter_forward_to_line_end (&iter);

              already_moved_initially = TRUE;
            }
          else
            {
              return FALSE;
            }
        }
    }
}

static gboolean
find_visible_by_log_attrs (GtkTextIter     *iter,
                           FindLogAttrFunc  func,
                           gboolean         forward)
{
  GtkTextIter pos;

  g_return_val_if_fail (iter != NULL, FALSE);

  pos = *iter;

  while (TRUE)
    {
      GtkTextIter pos_before = pos;

      find_by_log_attrs (&pos, func, forward);

      if (gtk_text_iter_equal (&pos_before, &pos))
        break;

      if (!_gtk_text_btree_char_is_invisible (&pos))
	{
	  *iter = pos;
	  return !gtk_text_iter_is_end (iter);
	}
    }

  return FALSE;
}

typedef gboolean (* OneStepFunc) (GtkTextIter *iter);
typedef gboolean (* MultipleStepFunc) (GtkTextIter *iter, int count);

static gboolean
move_multiple_steps (GtkTextIter *iter,
		     int count,
		     OneStepFunc step_forward,
		     MultipleStepFunc n_steps_backward)
{
  g_return_val_if_fail (iter != NULL, FALSE);

  FIX_OVERFLOWS (count);

  if (count == 0)
    return FALSE;

  if (count < 0)
    return n_steps_backward (iter, -count);

  if (!step_forward (iter))
    return FALSE;
  --count;

  while (count > 0)
    {
      if (!step_forward (iter))
        break;
      --count;
    }

  return !gtk_text_iter_is_end (iter);
}


/**
 * gtk_text_iter_forward_word_end:
 * @iter: a `GtkTextIter`
 *
 * Moves forward to the next word end.
 *
 * If @iter is currently on a word end, moves forward to the
 * next one after that.
 *
 * Word breaks are determined by Pango and should be correct
 * for nearly any language.
 *
 * Returns: %TRUE if @iter moved and is not the end iterator
 */
gboolean
gtk_text_iter_forward_word_end (GtkTextIter *iter)
{
  return find_by_log_attrs (iter, find_word_end_func, TRUE);
}

/**
 * gtk_text_iter_backward_word_start:
 * @iter: a `GtkTextIter`
 *
 * Moves backward to the previous word start.
 *
 * If @iter is currently on a word start, moves backward to the
 * next one after that.
 *
 * Word breaks are determined by Pango and should be correct
 * for nearly any language
 *
 * Returns: %TRUE if @iter moved and is not the end iterator
 */
gboolean
gtk_text_iter_backward_word_start (GtkTextIter *iter)
{
  return find_by_log_attrs (iter, find_word_start_func, FALSE);
}

/* FIXME a loop around a truly slow function means
 * a truly spectacularly slow function.
 */

/**
 * gtk_text_iter_forward_word_ends:
 * @iter: a `GtkTextIter`
 * @count: number of times to move
 *
 * Calls [method@Gtk.TextIter.forward_word_end] up to @count times.
 *
 * Returns: %TRUE if @iter moved and is not the end iterator
 */
gboolean
gtk_text_iter_forward_word_ends (GtkTextIter      *iter,
                                 int               count)
{
  return move_multiple_steps (iter, count,
			      gtk_text_iter_forward_word_end,
			      gtk_text_iter_backward_word_starts);
}

/**
 * gtk_text_iter_backward_word_starts:
 * @iter: a `GtkTextIter`
 * @count: number of times to move
 *
 * Calls [method@Gtk.TextIter.backward_word_start] up to @count times.
 *
 * Returns: %TRUE if @iter moved and is not the end iterator
 */
gboolean
gtk_text_iter_backward_word_starts (GtkTextIter      *iter,
                                    int                count)
{
  return move_multiple_steps (iter, count,
			      gtk_text_iter_backward_word_start,
			      gtk_text_iter_forward_word_ends);
}

/**
 * gtk_text_iter_forward_visible_word_end:
 * @iter: a `GtkTextIter`
 *
 * Moves forward to the next visible word end.
 *
 * If @iter is currently on a word end, moves forward to the
 * next one after that.
 *
 * Word breaks are determined by Pango and should be correct
 * for nearly any language
 *
 * Returns: %TRUE if @iter moved and is not the end iterator
 */
gboolean
gtk_text_iter_forward_visible_word_end (GtkTextIter *iter)
{
  return find_visible_by_log_attrs (iter, find_word_end_func, TRUE);
}

/**
 * gtk_text_iter_backward_visible_word_start:
 * @iter: a `GtkTextIter`
 *
 * Moves backward to the previous visible word start.
 *
 * If @iter is currently on a word start, moves backward to the
 * next one after that.
 *
 * Word breaks are determined by Pango and should be correct
 * for nearly any language.
 *
 * Returns: %TRUE if @iter moved and is not the end iterator
 */
gboolean
gtk_text_iter_backward_visible_word_start (GtkTextIter      *iter)
{
  return find_visible_by_log_attrs (iter, find_word_start_func, FALSE);
}

/**
 * gtk_text_iter_forward_visible_word_ends:
 * @iter: a `GtkTextIter`
 * @count: number of times to move
 *
 * Calls [method@Gtk.TextIter.forward_visible_word_end] up to @count times.
 *
 * Returns: %TRUE if @iter moved and is not the end iterator
 */
gboolean
gtk_text_iter_forward_visible_word_ends (GtkTextIter *iter,
					 int          count)
{
  return move_multiple_steps (iter, count,
			      gtk_text_iter_forward_visible_word_end,
			      gtk_text_iter_backward_visible_word_starts);
}

/**
 * gtk_text_iter_backward_visible_word_starts:
 * @iter: a `GtkTextIter`
 * @count: number of times to move
 *
 * Calls [method@Gtk.TextIter.backward_visible_word_start] up to @count times.
 *
 * Returns: %TRUE if @iter moved and is not the end iterator
 */
gboolean
gtk_text_iter_backward_visible_word_starts (GtkTextIter *iter,
					    int          count)
{
  return move_multiple_steps (iter, count,
			      gtk_text_iter_backward_visible_word_start,
			      gtk_text_iter_forward_visible_word_ends);
}

/**
 * gtk_text_iter_starts_word:
 * @iter: a `GtkTextIter`
 *
 * Determines whether @iter begins a natural-language word.
 *
 * Word breaks are determined by Pango and should be correct
 * for nearly any language.
 *
 * Returns: %TRUE if @iter is at the start of a word
 */
gboolean
gtk_text_iter_starts_word (const GtkTextIter *iter)
{
  return test_log_attrs (iter, is_word_start_func);
}

/**
 * gtk_text_iter_ends_word:
 * @iter: a `GtkTextIter`
 *
 * Determines whether @iter ends a natural-language word.
 *
 * Word breaks are determined by Pango and should be correct
 * for nearly any language.
 *
 * Returns: %TRUE if @iter is at the end of a word
 */
gboolean
gtk_text_iter_ends_word (const GtkTextIter *iter)
{
  return test_log_attrs (iter, is_word_end_func);
}

/**
 * gtk_text_iter_inside_word:
 * @iter: a `GtkTextIter`
 *
 * Determines whether the character pointed by @iter is part of a
 * natural-language word (as opposed to say inside some whitespace).
 *
 * Word breaks are determined by Pango and should be correct
 * for nearly any language.
 *
 * Note that if [method@Gtk.TextIter.starts_word] returns %TRUE,
 * then this function returns %TRUE too, since @iter points to
 * the first character of the word.
 *
 * Returns: %TRUE if @iter is inside a word
 */
gboolean
gtk_text_iter_inside_word (const GtkTextIter *iter)
{
  return test_log_attrs (iter, inside_word_func);
}

/**
 * gtk_text_iter_starts_sentence:
 * @iter: a `GtkTextIter`
 *
 * Determines whether @iter begins a sentence.
 *
 * Sentence boundaries are determined by Pango and
 * should be correct for nearly any language.
 *
 * Returns: %TRUE if @iter is at the start of a sentence.
 */
gboolean
gtk_text_iter_starts_sentence (const GtkTextIter *iter)
{
  return test_log_attrs (iter, is_sentence_start_func);
}

/**
 * gtk_text_iter_ends_sentence:
 * @iter: a `GtkTextIter`
 *
 * Determines whether @iter ends a sentence.
 *
 * Sentence boundaries are determined by Pango and should
 * be correct for nearly any language.
 *
 * Returns: %TRUE if @iter is at the end of a sentence.
 */
gboolean
gtk_text_iter_ends_sentence (const GtkTextIter *iter)
{
  return test_log_attrs (iter, is_sentence_end_func);
}

/**
 * gtk_text_iter_inside_sentence:
 * @iter: a `GtkTextIter`
 *
 * Determines whether @iter is inside a sentence (as opposed to in
 * between two sentences, e.g. after a period and before the first
 * letter of the next sentence).
 *
 * Sentence boundaries are determined by Pango and should be correct
 * for nearly any language.
 *
 * Returns: %TRUE if @iter is inside a sentence.
 */
gboolean
gtk_text_iter_inside_sentence (const GtkTextIter *iter)
{
  return test_log_attrs (iter, inside_sentence_func);
}

/**
 * gtk_text_iter_forward_sentence_end:
 * @iter: a `GtkTextIter`
 *
 * Moves forward to the next sentence end.
 *
 * If @iter is at the end of a sentence, moves to the next
 * end of sentence.
 *
 * Sentence boundaries are determined by Pango and should
 * be correct for nearly any language.
 *
 * Returns: %TRUE if @iter moved and is not the end iterator
 */
gboolean
gtk_text_iter_forward_sentence_end (GtkTextIter *iter)
{
  return find_by_log_attrs (iter, find_sentence_end_func, TRUE);
}

/**
 * gtk_text_iter_backward_sentence_start:
 * @iter: a `GtkTextIter`
 *
 * Moves backward to the previous sentence start.
 *
 * If @iter is already at the start of a sentence, moves backward
 * to the next one.
 *
 * Sentence boundaries are determined by Pango and should
 * be correct for nearly any language.
 *
 * Returns: %TRUE if @iter moved and is not the end iterator
 */
gboolean
gtk_text_iter_backward_sentence_start (GtkTextIter *iter)
{
  return find_by_log_attrs (iter, find_sentence_start_func, FALSE);
}

/* FIXME a loop around a truly slow function means
 * a truly spectacularly slow function.
 */
/**
 * gtk_text_iter_forward_sentence_ends:
 * @iter: a `GtkTextIter`
 * @count: number of sentences to move
 *
 * Calls [method@Gtk.TextIter.forward_sentence_end] @count times.
 *
 * If @count is negative, moves backward instead of forward.
 *
 * Returns: %TRUE if @iter moved and is not the end iterator
 */
gboolean
gtk_text_iter_forward_sentence_ends (GtkTextIter      *iter,
                                     int               count)
{
  return move_multiple_steps (iter, count,
			      gtk_text_iter_forward_sentence_end,
			      gtk_text_iter_backward_sentence_starts);
}

/**
 * gtk_text_iter_backward_sentence_starts:
 * @iter: a `GtkTextIter`
 * @count: number of sentences to move
 *
 * Calls [method@Gtk.TextIter.backward_sentence_start] up to @count times.
 *
 * If @count is negative, moves forward instead of backward.
 *
 * Returns: %TRUE if @iter moved and is not the end iterator
 */
gboolean
gtk_text_iter_backward_sentence_starts (GtkTextIter *iter,
                                        int          count)
{
  return move_multiple_steps (iter, count,
			      gtk_text_iter_backward_sentence_start,
			      gtk_text_iter_forward_sentence_ends);
}

static gboolean
find_forward_cursor_pos_func (const PangoLogAttr *attrs,
                              int                 offset,
                              int                 len,
                              int                *found_offset,
                              gboolean            already_moved_initially)
{
  if (!already_moved_initially)
    ++offset;

  while (offset <= len)
    {
      if (attrs[offset].is_cursor_position)
        {
          *found_offset = offset;
          return TRUE;
        }

      ++offset;
    }

  return FALSE;
}

static gboolean
find_backward_cursor_pos_func (const PangoLogAttr *attrs,
                               int                 offset,
                               int                 len,
                               int                *found_offset,
                               gboolean            already_moved_initially)
{
  if (!already_moved_initially)
    --offset;

  while (offset >= 0)
    {
      if (attrs[offset].is_cursor_position)
        {
          *found_offset = offset;
          return TRUE;
        }

      --offset;
    }

  return FALSE;
}

static gboolean
is_cursor_pos_func (const PangoLogAttr *attrs,
                    int           offset,
                    int           min_offset,
                    int           len)
{
  return attrs[offset].is_cursor_position;
}

/**
 * gtk_text_iter_forward_cursor_position:
 * @iter: a `GtkTextIter`
 *
 * Moves @iter forward by a single cursor position.
 *
 * Cursor positions are (unsurprisingly) positions where the
 * cursor can appear. Perhaps surprisingly, there may not be
 * a cursor position between all characters. The most common
 * example for European languages would be a carriage return/newline
 * sequence.
 *
 * For some Unicode characters, the equivalent of say the letter “a”
 * with an accent mark will be represented as two characters, first
 * the letter then a "combining mark" that causes the accent to be
 * rendered; so the cursor can’t go between those two characters.
 *
 * See also the [struct@Pango.LogAttr] struct and the [func@Pango.break]
 * function.
 *
 * Returns: %TRUE if we moved and the new position is dereferenceable
 */
gboolean
gtk_text_iter_forward_cursor_position (GtkTextIter *iter)
{
  return find_by_log_attrs (iter, find_forward_cursor_pos_func, TRUE);
}

/**
 * gtk_text_iter_backward_cursor_position:
 * @iter: a `GtkTextIter`
 *
 * Like [method@Gtk.TextIter.forward_cursor_position], but moves backward.
 *
 * Returns: %TRUE if we moved
 */
gboolean
gtk_text_iter_backward_cursor_position (GtkTextIter *iter)
{
  return find_by_log_attrs (iter, find_backward_cursor_pos_func, FALSE);
}

/**
 * gtk_text_iter_forward_cursor_positions:
 * @iter: a `GtkTextIter`
 * @count: number of positions to move
 *
 * Moves up to @count cursor positions.
 *
 * See [method@Gtk.TextIter.forward_cursor_position] for details.
 *
 * Returns: %TRUE if we moved and the new position is dereferenceable
 */
gboolean
gtk_text_iter_forward_cursor_positions (GtkTextIter *iter,
                                        int          count)
{
  return move_multiple_steps (iter, count,
			      gtk_text_iter_forward_cursor_position,
			      gtk_text_iter_backward_cursor_positions);
}

/**
 * gtk_text_iter_backward_cursor_positions:
 * @iter: a `GtkTextIter`
 * @count: number of positions to move
 *
 * Moves up to @count cursor positions.
 *
 * See [method@Gtk.TextIter.forward_cursor_position] for details.
 *
 * Returns: %TRUE if we moved and the new position is dereferenceable
 */
gboolean
gtk_text_iter_backward_cursor_positions (GtkTextIter *iter,
                                         int          count)
{
  return move_multiple_steps (iter, count,
			      gtk_text_iter_backward_cursor_position,
			      gtk_text_iter_forward_cursor_positions);
}

/**
 * gtk_text_iter_forward_visible_cursor_position:
 * @iter: a `GtkTextIter`
 *
 * Moves @iter forward to the next visible cursor position.
 *
 * See [method@Gtk.TextIter.forward_cursor_position] for details.
 *
 * Returns: %TRUE if we moved and the new position is dereferenceable
 */
gboolean
gtk_text_iter_forward_visible_cursor_position (GtkTextIter *iter)
{
  return find_visible_by_log_attrs (iter, find_forward_cursor_pos_func, TRUE);
}

/**
 * gtk_text_iter_backward_visible_cursor_position:
 * @iter: a `GtkTextIter`
 *
 * Moves @iter backward to the previous visible cursor position.
 *
 * See [method@Gtk.TextIter.backward_cursor_position] for details.
 *
 * Returns: %TRUE if we moved and the new position is dereferenceable
 */
gboolean
gtk_text_iter_backward_visible_cursor_position (GtkTextIter *iter)
{
  return find_visible_by_log_attrs (iter, find_backward_cursor_pos_func, FALSE);
}

/**
 * gtk_text_iter_forward_visible_cursor_positions:
 * @iter: a `GtkTextIter`
 * @count: number of positions to move
 *
 * Moves up to @count visible cursor positions.
 *
 * See [method@Gtk.TextIter.forward_cursor_position] for details.
 *
 * Returns: %TRUE if we moved and the new position is dereferenceable
 */
gboolean
gtk_text_iter_forward_visible_cursor_positions (GtkTextIter *iter,
						int          count)
{
  return move_multiple_steps (iter, count,
			      gtk_text_iter_forward_visible_cursor_position,
			      gtk_text_iter_backward_visible_cursor_positions);
}

/**
 * gtk_text_iter_backward_visible_cursor_positions:
 * @iter: a `GtkTextIter`
 * @count: number of positions to move
 *
 * Moves up to @count visible cursor positions.
 *
 * See [method@Gtk.TextIter.backward_cursor_position] for details.
 *
 * Returns: %TRUE if we moved and the new position is dereferenceable
 */
gboolean
gtk_text_iter_backward_visible_cursor_positions (GtkTextIter *iter,
						 int          count)
{
  return move_multiple_steps (iter, count,
			      gtk_text_iter_backward_visible_cursor_position,
			      gtk_text_iter_forward_visible_cursor_positions);
}

/**
 * gtk_text_iter_is_cursor_position:
 * @iter: a `GtkTextIter`
 *
 * Determine if @iter is at a cursor position.
 *
 * See [method@Gtk.TextIter.forward_cursor_position] or
 * [struct@Pango.LogAttr] or [func@Pango.break] for details
 * on what a cursor position is.
 *
 * Returns: %TRUE if the cursor can be placed at @iter
 */
gboolean
gtk_text_iter_is_cursor_position (const GtkTextIter *iter)
{
  return test_log_attrs (iter, is_cursor_pos_func);
}

/**
 * gtk_text_iter_set_line_offset:
 * @iter: a `GtkTextIter`
 * @char_on_line: a character offset relative to the start of @iter’s current line
 *
 * Moves @iter within a line, to a new character (not byte) offset.
 *
 * The given character offset must be less than or equal to the number
 * of characters in the line; if equal, @iter moves to the start of the
 * next line. See [method@Gtk.TextIter.set_line_index] if you have a byte
 * index rather than a character offset.
 */
void
gtk_text_iter_set_line_offset (GtkTextIter *iter,
                               int          char_on_line)
{
  GtkTextRealIter *real;
  int chars_in_line;

  g_return_if_fail (iter != NULL);

  real = gtk_text_iter_make_surreal (iter);

  if (real == NULL)
    return;

  check_invariants (iter);

  chars_in_line = gtk_text_iter_get_chars_in_line (iter);

  g_return_if_fail (char_on_line <= chars_in_line);

  if (char_on_line < chars_in_line)
    iter_set_from_char_offset (real, real->line, char_on_line);
  else
    gtk_text_iter_forward_line (iter); /* set to start of next line */

  check_invariants (iter);
}

/**
 * gtk_text_iter_set_line_index:
 * @iter: a `GtkTextIter`
 * @byte_on_line: a byte index relative to the start of @iter’s current line
 *
 * Same as [method@Gtk.TextIter.set_line_offset], but works with a
 * byte index. The given byte index must be at
 * the start of a character, it can’t be in the middle of a UTF-8
 * encoded character.
 */
void
gtk_text_iter_set_line_index (GtkTextIter *iter,
                              int          byte_on_line)
{
  GtkTextRealIter *real;
  int bytes_in_line;

  g_return_if_fail (iter != NULL);

  real = gtk_text_iter_make_surreal (iter);

  if (real == NULL)
    return;

  check_invariants (iter);

  bytes_in_line = gtk_text_iter_get_bytes_in_line (iter);

  g_return_if_fail (byte_on_line <= bytes_in_line);

  if (byte_on_line < bytes_in_line)
    iter_set_from_byte_offset (real, real->line, byte_on_line);
  else
    gtk_text_iter_forward_line (iter);

  if (real->segment->type == &gtk_text_char_type &&
      (real->segment->body.chars[real->segment_byte_offset] & 0xc0) == 0x80)
    g_warning ("%s: Incorrect byte offset %d falls in the middle of a UTF-8 "
               "character; this will crash the text buffer. "
               "Byte indexes must refer to the start of a character.",
               G_STRLOC, byte_on_line);

  check_invariants (iter);
}


/**
 * gtk_text_iter_set_visible_line_offset:
 * @iter: a `GtkTextIter`
 * @char_on_line: a character offset
 *
 * Like [method@Gtk.TextIter.set_line_offset], but the offset is in visible
 * characters, i.e. text with a tag making it invisible is not
 * counted in the offset.
 */
void
gtk_text_iter_set_visible_line_offset (GtkTextIter *iter,
                                       int          char_on_line)
{
  int chars_seen = 0;
  GtkTextIter pos;

  g_return_if_fail (iter != NULL);

  gtk_text_iter_set_line_offset (iter, 0);

  pos = *iter;

  /* For now we use a ludicrously slow implementation */
  while (chars_seen < char_on_line)
    {
      if (!_gtk_text_btree_char_is_invisible (&pos))
        ++chars_seen;

      if (!gtk_text_iter_forward_char (&pos))
        break;

      if (chars_seen == char_on_line)
        break;
    }

  if (_gtk_text_iter_get_text_line (&pos) == _gtk_text_iter_get_text_line (iter))
    *iter = pos;
  else
    gtk_text_iter_forward_line (iter);
}

/**
 * gtk_text_iter_set_visible_line_index:
 * @iter: a `GtkTextIter`
 * @byte_on_line: a byte index
 *
 * Like [method@Gtk.TextIter.set_line_index], but the index is in visible
 * bytes, i.e. text with a tag making it invisible is not counted
 * in the index.
 */
void
gtk_text_iter_set_visible_line_index (GtkTextIter *iter,
                                      int          byte_on_line)
{
  GtkTextRealIter *real;
  int offset = 0;
  GtkTextIter pos;
  GtkTextLineSegment *seg;

  g_return_if_fail (iter != NULL);

  gtk_text_iter_set_line_offset (iter, 0);

  pos = *iter;

  real = gtk_text_iter_make_real (&pos);

  if (real == NULL)
    return;

  ensure_byte_offsets (real);

  check_invariants (&pos);

  seg = _gtk_text_iter_get_indexable_segment (&pos);

  while (seg != NULL && byte_on_line > 0)
    {
      if (!_gtk_text_btree_char_is_invisible (&pos))
        {
          if (byte_on_line < seg->byte_count)
            {
              iter_set_from_byte_offset (real, real->line, offset + byte_on_line);
              byte_on_line = 0;
              break;
            }
          else
            byte_on_line -= seg->byte_count;
        }

      offset += seg->byte_count;
      _gtk_text_iter_forward_indexable_segment (&pos);
      seg = _gtk_text_iter_get_indexable_segment (&pos);
    }

  if (byte_on_line == 0)
    *iter = pos;
  else
    gtk_text_iter_forward_line (iter);
}

/**
 * gtk_text_iter_set_line:
 * @iter: a `GtkTextIter`
 * @line_number: line number (counted from 0)
 *
 * Moves iterator @iter to the start of the line @line_number.
 *
 * If @line_number is negative or larger than or equal to the number of lines
 * in the buffer, moves @iter to the start of the last line in the buffer.
 */
void
gtk_text_iter_set_line (GtkTextIter *iter,
                        int          line_number)
{
  GtkTextLine *line;
  int real_line;
  GtkTextRealIter *real;

  g_return_if_fail (iter != NULL);

  real = gtk_text_iter_make_surreal (iter);

  if (real == NULL)
    return;

  check_invariants (iter);

  line = _gtk_text_btree_get_line_no_last (real->tree, line_number, &real_line);

  iter_set_from_char_offset (real, line, 0);

  /* We might as well cache this, since we know it. */
  real->cached_line_number = real_line;

  check_invariants (iter);
}

/**
 * gtk_text_iter_set_offset:
 * @iter: a `GtkTextIter`
 * @char_offset: a character number
 *
 * Sets @iter to point to @char_offset.
 *
 * @char_offset counts from the start
 * of the entire text buffer, starting with 0.
 */
void
gtk_text_iter_set_offset (GtkTextIter *iter,
                          int          char_offset)
{
  GtkTextLine *line;
  GtkTextRealIter *real;
  int line_start;
  int real_char_index;

  g_return_if_fail (iter != NULL);

  real = gtk_text_iter_make_surreal (iter);

  if (real == NULL)
    return;

  check_invariants (iter);

  if (real->cached_char_index >= 0 &&
      real->cached_char_index == char_offset)
    return;

  line = _gtk_text_btree_get_line_at_char (real->tree,
                                           char_offset,
                                           &line_start,
                                           &real_char_index);

  iter_set_from_char_offset (real, line, real_char_index - line_start);

  /* Go ahead and cache this since we have it. */
  real->cached_char_index = real_char_index;

  check_invariants (iter);
}

/**
 * gtk_text_iter_forward_to_end:
 * @iter: a `GtkTextIter`
 *
 * Moves @iter forward to the “end iterator”, which points
 * one past the last valid character in the buffer.
 *
 * [method@Gtk.TextIter.get_char] called on the end iterator
 * returns 0, which is convenient for writing loops.
 */
void
gtk_text_iter_forward_to_end  (GtkTextIter *iter)
{
  GtkTextBuffer *buffer;
  GtkTextRealIter *real;

  g_return_if_fail (iter != NULL);

  real = gtk_text_iter_make_surreal (iter);

  if (real == NULL)
    return;

  buffer = _gtk_text_btree_get_buffer (real->tree);

  gtk_text_buffer_get_end_iter (buffer, iter);
}

/* FIXME this and gtk_text_iter_forward_to_line_end() could be cleaned up
 * and made faster. Look at iter_ends_line() for inspiration, perhaps.
 * If all else fails we could cache the para delimiter pos in the iter.
 * I think forward_to_line_end() actually gets called fairly often.
 */
static int
find_paragraph_delimiter_for_line (GtkTextIter *iter)
{
  GtkTextIter end;
  end = *iter;

  if (_gtk_text_line_contains_end_iter (_gtk_text_iter_get_text_line (&end),
                                        _gtk_text_iter_get_btree (&end)))
    {
      gtk_text_iter_forward_to_end (&end);
    }
  else
    {
      /* if we aren't on the last line, go forward to start of next line, then scan
       * back for the delimiters on the previous line
       */
      gtk_text_iter_forward_line (&end);
      gtk_text_iter_backward_char (&end);
      while (!gtk_text_iter_ends_line (&end))
        gtk_text_iter_backward_char (&end);
    }

  return gtk_text_iter_get_line_offset (&end);
}

/**
 * gtk_text_iter_forward_to_line_end:
 * @iter: a `GtkTextIter`
 *
 * Moves the iterator to point to the paragraph delimiter characters.
 *
 * The possible characters are either a newline, a carriage return,
 * a carriage return/newline in sequence, or the Unicode paragraph
 * separator character.
 *
 * If the iterator is already at the paragraph delimiter
 * characters, moves to the paragraph delimiter characters for the
 * next line. If @iter is on the last line in the buffer, which does
 * not end in paragraph delimiters, moves to the end iterator (end of
 * the last line), and returns %FALSE.
 *
 * Returns: %TRUE if we moved and the new location is not the end iterator
 */
gboolean
gtk_text_iter_forward_to_line_end (GtkTextIter *iter)
{
  int current_offset;
  int new_offset;


  g_return_val_if_fail (iter != NULL, FALSE);

  current_offset = gtk_text_iter_get_line_offset (iter);
  new_offset = find_paragraph_delimiter_for_line (iter);

  if (current_offset < new_offset)
    {
      /* Move to end of this line. */
      gtk_text_iter_set_line_offset (iter, new_offset);
      return !gtk_text_iter_is_end (iter);
    }
  else
    {
      /* Move to end of next line. */
      if (gtk_text_iter_forward_line (iter))
        {
          /* We don't want to move past all
           * empty lines.
           */
          if (!gtk_text_iter_ends_line (iter))
            gtk_text_iter_forward_to_line_end (iter);
          return !gtk_text_iter_is_end (iter);
        }
      else
        return FALSE;
    }
}

/**
 * gtk_text_iter_forward_to_tag_toggle:
 * @iter: a `GtkTextIter`
 * @tag: (nullable): a `GtkTextTag`
 *
 * Moves forward to the next toggle (on or off) of the
 * @tag, or to the next toggle of any tag if
 * @tag is %NULL.
 *
 * If no matching tag toggles are found,
 * returns %FALSE, otherwise %TRUE. Does not return toggles
 * located at @iter, only toggles after @iter. Sets @iter to
 * the location of the toggle, or to the end of the buffer
 * if no toggle is found.
 *
 * Returns: whether we found a tag toggle after @iter
 */
gboolean
gtk_text_iter_forward_to_tag_toggle (GtkTextIter *iter,
                                     GtkTextTag  *tag)
{
  GtkTextLine *next_line;
  GtkTextLine *current_line;
  GtkTextRealIter *real;

  g_return_val_if_fail (iter != NULL, FALSE);

  real = gtk_text_iter_make_real (iter);

  if (real == NULL)
    return FALSE;

  check_invariants (iter);

  if (gtk_text_iter_is_end (iter))
    return FALSE;

  current_line = real->line;
  next_line = _gtk_text_line_next_could_contain_tag (current_line,
                                                     real->tree, tag);

  while (_gtk_text_iter_forward_indexable_segment (iter))
    {
      /* If we went forward to a line that couldn't contain a toggle
         for the tag, then skip forward to a line that could contain
         it. This potentially skips huge hunks of the tree, so we
         aren't a purely linear search. */
      if (real->line != current_line)
        {
          if (next_line == NULL)
            {
              /* End of search. Set to end of buffer. */
              _gtk_text_btree_get_end_iter (real->tree, iter);
              return FALSE;
            }

          if (real->line != next_line)
            iter_set_from_byte_offset (real, next_line, 0);

          current_line = real->line;
          next_line = _gtk_text_line_next_could_contain_tag (current_line,
                                                             real->tree,
                                                             tag);
        }

      if (gtk_text_iter_toggles_tag (iter, tag))
        {
          /* If there's a toggle here, it isn't indexable so
             any_segment can't be the indexable segment. */
          g_assert (real->any_segment != real->segment);
          return TRUE;
        }
    }

  /* Check end iterator for tags */
  if (gtk_text_iter_toggles_tag (iter, tag))
    {
      /* If there's a toggle here, it isn't indexable so
         any_segment can't be the indexable segment. */
      g_assert (real->any_segment != real->segment);
      return TRUE;
    }

  /* Reached end of buffer */
  return FALSE;
}

/**
 * gtk_text_iter_backward_to_tag_toggle:
 * @iter: a `GtkTextIter`
 * @tag: (nullable): a `GtkTextTag`
 *
 * Moves backward to the next toggle (on or off) of the
 * @tag, or to the next toggle of any tag if
 * @tag is %NULL.
 *
 * If no matching tag toggles are found,
 * returns %FALSE, otherwise %TRUE. Does not return toggles
 * located at @iter, only toggles before @iter. Sets @iter
 * to the location of the toggle, or the start of the buffer
 * if no toggle is found.
 *
 * Returns: whether we found a tag toggle before @iter
 */
gboolean
gtk_text_iter_backward_to_tag_toggle (GtkTextIter *iter,
                                      GtkTextTag  *tag)
{
  GtkTextLine *prev_line;
  GtkTextLine *current_line;
  GtkTextRealIter *real;

  g_return_val_if_fail (iter != NULL, FALSE);

  real = gtk_text_iter_make_real (iter);

  if (real == NULL)
    return FALSE;

  check_invariants (iter);

  current_line = real->line;
  prev_line = _gtk_text_line_previous_could_contain_tag (current_line,
                                                        real->tree, tag);


  /* If we're at segment start, go to the previous segment;
   * if mid-segment, snap to start of current segment.
   */
  if (is_segment_start (real))
    {
      if (!_gtk_text_iter_backward_indexable_segment (iter))
        return FALSE;
    }
  else
    {
      ensure_char_offsets (real);

      if (!gtk_text_iter_backward_chars (iter, real->segment_char_offset))
        return FALSE;
    }

  do
    {
      /* If we went backward to a line that couldn't contain a toggle
       * for the tag, then skip backward further to a line that
       * could contain it. This potentially skips huge hunks of the
       * tree, so we aren't a purely linear search.
       */
      if (real->line != current_line)
        {
          if (prev_line == NULL)
            {
              /* End of search. Set to start of buffer. */
              _gtk_text_btree_get_iter_at_char (real->tree, iter, 0);
              return FALSE;
            }

          if (real->line != prev_line)
            {
              /* Set to last segment in prev_line (could do this
               * more quickly)
               */
              iter_set_from_byte_offset (real, prev_line, 0);

              while (!at_last_indexable_segment (real))
                _gtk_text_iter_forward_indexable_segment (iter);
            }

          current_line = real->line;
          prev_line = _gtk_text_line_previous_could_contain_tag (current_line,
                                                                real->tree,
                                                                tag);
        }

      if (gtk_text_iter_toggles_tag (iter, tag))
        {
          /* If there's a toggle here, it isn't indexable so
           * any_segment can't be the indexable segment.
           */
          g_assert (real->any_segment != real->segment);
          return TRUE;
        }
    }
  while (_gtk_text_iter_backward_indexable_segment (iter));

  /* Reached front of buffer */
  return FALSE;
}

static gboolean
matches_pred (GtkTextIter *iter,
              GtkTextCharPredicate pred,
              gpointer user_data)
{
  int ch;

  ch = gtk_text_iter_get_char (iter);

  return (*pred) (ch, user_data);
}

/**
 * gtk_text_iter_forward_find_char:
 * @iter: a `GtkTextIter`
 * @pred: (scope call): a function to be called on each character
 * @user_data: (closure): user data for @pred
 * @limit: (nullable): search limit
 *
 * Advances @iter, calling @pred on each character.
 *
 * If @pred returns %TRUE, returns %TRUE and stops scanning.
 * If @pred never returns %TRUE, @iter is set to @limit if
 * @limit is non-%NULL, otherwise to the end iterator.
 *
 * Returns: whether a match was found
 */
gboolean
gtk_text_iter_forward_find_char (GtkTextIter         *iter,
                                 GtkTextCharPredicate pred,
                                 gpointer             user_data,
                                 const GtkTextIter   *limit)
{
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (pred != NULL, FALSE);

  if (limit &&
      gtk_text_iter_compare (iter, limit) >= 0)
    return FALSE;

  while ((limit == NULL ||
          !gtk_text_iter_equal (limit, iter)) &&
         gtk_text_iter_forward_char (iter))
    {
      if (matches_pred (iter, pred, user_data))
        return TRUE;
    }

  return FALSE;
}

/**
 * gtk_text_iter_backward_find_char:
 * @iter: a `GtkTextIter`
 * @pred: (scope call): function to be called on each character
 * @user_data: (closure): user data for @pred
 * @limit: (nullable): search limit
 *
 * Same as [method@Gtk.TextIter.forward_find_char],
 * but goes backward from @iter.
 *
 * Returns: whether a match was found
 */
gboolean
gtk_text_iter_backward_find_char (GtkTextIter         *iter,
                                  GtkTextCharPredicate pred,
                                  gpointer             user_data,
                                  const GtkTextIter   *limit)
{
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (pred != NULL, FALSE);

  if (limit &&
      gtk_text_iter_compare (iter, limit) <= 0)
    return FALSE;

  while ((limit == NULL ||
          !gtk_text_iter_equal (limit, iter)) &&
         gtk_text_iter_backward_char (iter))
    {
      if (matches_pred (iter, pred, user_data))
        return TRUE;
    }

  return FALSE;
}

static void
forward_chars_with_skipping (GtkTextIter *iter,
                             int          count,
                             gboolean     skip_invisible,
                             gboolean     skip_nontext,
                             gboolean     skip_decomp)
{
  int i;

  g_return_if_fail (count >= 0);

  i = count;

  while (i > 0)
    {
      gboolean ignored = FALSE;

      /* minimal workaround to avoid the infinite loop of bug #168247. */
      if (gtk_text_iter_is_end (iter))
        return;

      if (skip_nontext &&
          gtk_text_iter_get_char (iter) == GTK_TEXT_UNKNOWN_CHAR)
        ignored = TRUE;

      if (!ignored &&
          skip_invisible &&
          _gtk_text_btree_char_is_invisible (iter))
        ignored = TRUE;

      if (!ignored && skip_decomp)
        {
          /* being UTF8 correct sucks: this accounts for extra
             offsets coming from canonical decompositions of
             UTF8 characters (e.g. accented characters) which
             g_utf8_normalize() performs */
          char *normal;
          char *casefold;
          char buffer[6];
          int buffer_len;

          buffer_len = g_unichar_to_utf8 (gtk_text_iter_get_char (iter), buffer);
          casefold = g_utf8_casefold (buffer, buffer_len);
          normal = g_utf8_normalize (casefold, -1, G_NORMALIZE_NFD);
          i -= (g_utf8_strlen (normal, -1) - 1);
          g_free (normal);
          g_free (casefold);
        }

      gtk_text_iter_forward_char (iter);

      if (!ignored)
        --i;
    }
}

static const char *
pointer_from_offset_skipping_decomp (const char *str,
                                     int          offset)
{
  char *casefold, *normal;
  const char *p, *q;

  p = str;

  while (offset > 0)
    {
      q = g_utf8_next_char (p);
      casefold = g_utf8_casefold (p, q - p);
      normal = g_utf8_normalize (casefold, -1, G_NORMALIZE_NFD);
      offset -= g_utf8_strlen (normal, -1);
      g_free (casefold);
      g_free (normal);
      p = q;
    }

  return p;
}

static gboolean
exact_prefix_cmp (const char *string,
                  const char *prefix,
                  guint        prefix_len)
{
  GUnicodeType type;

  if (strncmp (string, prefix, prefix_len) != 0)
    return FALSE;
  if (string[prefix_len] == '\0')
    return TRUE;

  type = g_unichar_type (g_utf8_get_char (string + prefix_len));

  /* If string contains prefix, check that prefix is not followed
   * by a unicode mark symbol, e.g. that trailing 'a' in prefix
   * is not part of two-char a-with-hat symbol in string. */
  return type != G_UNICODE_SPACING_MARK &&
         type != G_UNICODE_ENCLOSING_MARK &&
         type != G_UNICODE_NON_SPACING_MARK;
}

static const char *
utf8_strcasestr (const char *haystack,
                 const char *needle)
{
  gsize needle_len;
  gsize haystack_len;
  const char *ret = NULL;
  char *p;
  char *casefold;
  char *caseless_haystack;
  int i;

  g_return_val_if_fail (haystack != NULL, NULL);
  g_return_val_if_fail (needle != NULL, NULL);

  casefold = g_utf8_casefold (haystack, -1);
  caseless_haystack = g_utf8_normalize (casefold, -1, G_NORMALIZE_NFD);
  g_free (casefold);

  needle_len = g_utf8_strlen (needle, -1);
  haystack_len = g_utf8_strlen (caseless_haystack, -1);

  if (needle_len == 0)
    {
      ret = (char *)haystack;
      goto finally;
    }

  if (haystack_len < needle_len)
    {
      ret = NULL;
      goto finally;
    }

  p = (char *)caseless_haystack;
  needle_len = strlen (needle);
  i = 0;

  while (*p)
    {
      if (exact_prefix_cmp (p, needle, needle_len))
        {
          ret = pointer_from_offset_skipping_decomp (haystack, i);
          goto finally;
        }

      p = g_utf8_next_char (p);
      i++;
    }

finally:
  g_free (caseless_haystack);

  return ret;
}

static const char *
utf8_strrcasestr (const char *haystack,
                  const char *needle)
{
  gsize needle_len;
  gsize haystack_len;
  const char *ret = NULL;
  char *p;
  char *casefold;
  char *caseless_haystack;
  int i;

  g_return_val_if_fail (haystack != NULL, NULL);
  g_return_val_if_fail (needle != NULL, NULL);

  casefold = g_utf8_casefold (haystack, -1);
  caseless_haystack = g_utf8_normalize (casefold, -1, G_NORMALIZE_NFD);
  g_free (casefold);

  needle_len = g_utf8_strlen (needle, -1);
  haystack_len = g_utf8_strlen (caseless_haystack, -1);

  if (needle_len == 0)
    {
      ret = (char *)haystack;
      goto finally;
    }

  if (haystack_len < needle_len)
    {
      ret = NULL;
      goto finally;
    }

  i = haystack_len - needle_len;
  p = g_utf8_offset_to_pointer (caseless_haystack, i);
  needle_len = strlen (needle);

  while (TRUE)
    {
      if (exact_prefix_cmp (p, needle, needle_len))
        {
          ret = pointer_from_offset_skipping_decomp (haystack, i);
          break;
        }

      if (p == caseless_haystack)
        break;

      p = g_utf8_prev_char (p);
      i--;
    }

finally:
  g_free (caseless_haystack);

  return ret;
}

/* normalizes caseless strings and returns true if @s2 matches
   the start of @s1 */
static gboolean
utf8_caselessnmatch (const char *s1,
                     const char *s2,
                     gssize       n1,
                     gssize       n2)
{
  char *casefold;
  char *normalized_s1;
  char *normalized_s2;
  int len_s1;
  int len_s2;
  gboolean ret = FALSE;

  g_return_val_if_fail (s1 != NULL, FALSE);
  g_return_val_if_fail (s2 != NULL, FALSE);
  g_return_val_if_fail (n1 > 0, FALSE);
  g_return_val_if_fail (n2 > 0, FALSE);

  casefold = g_utf8_casefold (s1, n1);
  normalized_s1 = g_utf8_normalize (casefold, -1, G_NORMALIZE_NFD);
  g_free (casefold);

  casefold = g_utf8_casefold (s2, n2);
  normalized_s2 = g_utf8_normalize (casefold, -1, G_NORMALIZE_NFD);
  g_free (casefold);

  len_s1 = strlen (normalized_s1);
  len_s2 = strlen (normalized_s2);

  if (len_s1 >= len_s2)
    ret = (strncmp (normalized_s1, normalized_s2, len_s2) == 0);

  g_free (normalized_s1);
  g_free (normalized_s2);

  return ret;
}

static gboolean
lines_match (const GtkTextIter *start,
             const char **lines,
             gboolean visible_only,
             gboolean slice,
             gboolean case_insensitive,
             GtkTextIter *match_start,
             GtkTextIter *match_end)
{
  GtkTextIter next;
  char *line_text;
  const char *found;
  int offset;

  if (*lines == NULL || **lines == '\0')
    {
      if (match_start)
        *match_start = *start;

      if (match_end)
        *match_end = *start;
      return TRUE;
    }

  next = *start;
  gtk_text_iter_forward_line (&next);

  /* No more text in buffer, but *lines is nonempty */
  if (gtk_text_iter_equal (start, &next))
    {
      return FALSE;
    }

  if (slice)
    {
      if (visible_only)
        line_text = gtk_text_iter_get_visible_slice (start, &next);
      else
        line_text = gtk_text_iter_get_slice (start, &next);
    }
  else
    {
      if (visible_only)
        line_text = gtk_text_iter_get_visible_text (start, &next);
      else
        line_text = gtk_text_iter_get_text (start, &next);
    }

  if (match_start) /* if this is the first line we're matching */
    {
      if (!case_insensitive)
        found = strstr (line_text, *lines);
      else
        found = utf8_strcasestr (line_text, *lines);
    }
  else
    {
      /* If it's not the first line, we have to match from the
       * start of the line.
       */
      if ((!case_insensitive &&
           (strncmp (line_text, *lines, strlen (*lines)) == 0)) ||
          (case_insensitive &&
           utf8_caselessnmatch (line_text, *lines, strlen (line_text),
                                strlen (*lines))))
        {
          found = line_text;
        }
      else
        found = NULL;
    }

  if (found == NULL)
    {
      g_free (line_text);
      return FALSE;
    }

  /* Get offset to start of search string */
  offset = g_utf8_strlen (line_text, found - line_text);

  next = *start;

  /* If match start needs to be returned, set it to the
   * start of the search string.
   */
  forward_chars_with_skipping (&next, offset,
                               visible_only, !slice, FALSE);
  if (match_start)
    *match_start = next;

  /* Go to end of search string */
  forward_chars_with_skipping (&next, g_utf8_strlen (*lines, -1),
                               visible_only, !slice, case_insensitive);

  g_free (line_text);

  ++lines;

  if (match_end)
    *match_end = next;

  /* pass NULL for match_start, since we don't need to find the
   * start again.
   */
  return lines_match (&next, lines, visible_only, slice, case_insensitive, NULL, match_end);
}

/* strsplit() that retains the delimiter as part of the string. */
static char **
strbreakup (const char *string,
            const char *delimiter,
            int         max_tokens,
            int        *num_strings,
            gboolean    case_insensitive)
{
  GSList *string_list = NULL, *slist;
  char **str_array, *s;
  char *casefold, *new_string;
  guint i, n = 1;

  g_return_val_if_fail (string != NULL, NULL);
  g_return_val_if_fail (delimiter != NULL, NULL);

  if (max_tokens < 1)
    max_tokens = G_MAXINT;

  s = strstr (string, delimiter);
  if (s)
    {
      guint delimiter_len = strlen (delimiter);

      do
        {
          guint len;

          len = s - string + delimiter_len;
          new_string = g_new (char, len + 1);
          strncpy (new_string, string, len);
          new_string[len] = 0;

          if (case_insensitive)
            {
              casefold = g_utf8_casefold (new_string, -1);
              g_free (new_string);
              new_string = g_utf8_normalize (casefold, -1, G_NORMALIZE_NFD);
              g_free (casefold);
            }

          string_list = g_slist_prepend (string_list, new_string);
          n++;
          string = s + delimiter_len;
          s = strstr (string, delimiter);
        }
      while (--max_tokens && s);
    }
  if (*string)
    {
      n++;

      if (case_insensitive)
        {
          casefold = g_utf8_casefold (string, -1);
          new_string = g_utf8_normalize (casefold, -1, G_NORMALIZE_NFD);
          g_free (casefold);
        }
      else
        new_string = g_strdup (string);

      string_list = g_slist_prepend (string_list, new_string);
    }

  str_array = g_new (char *, n);

  i = n - 1;

  str_array[i--] = NULL;
  for (slist = string_list; slist; slist = slist->next)
    str_array[i--] = slist->data;

  g_slist_free (string_list);

  if (num_strings != NULL)
    *num_strings = n - 1;

  return str_array;
}

/**
 * gtk_text_iter_forward_search:
 * @iter: start of search
 * @str: a search string
 * @flags: flags affecting how the search is done
 * @match_start: (out caller-allocates) (optional): return location for start of match
 * @match_end: (out caller-allocates) (optional): return location for end of match
 * @limit: (nullable): location of last possible @match_end, or %NULL for the end of the buffer
 *
 * Searches forward for @str.
 *
 * Any match is returned by setting @match_start to the first character
 * of the match and @match_end to the first character after the match.
 * The search will not continue past @limit. Note that a search is a
 * linear or O(n) operation, so you may wish to use @limit to avoid
 * locking up your UI on large buffers.
 *
 * @match_start will never be set to a `GtkTextIter` located before @iter,
 * even if there is a possible @match_end after or at @iter.
 *
 * Returns: whether a match was found
 */
gboolean
gtk_text_iter_forward_search (const GtkTextIter *iter,
                              const char        *str,
                              GtkTextSearchFlags flags,
                              GtkTextIter       *match_start,
                              GtkTextIter       *match_end,
                              const GtkTextIter *limit)
{
  char **lines = NULL;
  GtkTextIter match;
  gboolean retval = FALSE;
  GtkTextIter search;
  gboolean visible_only;
  gboolean slice;
  gboolean case_insensitive;

  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (str != NULL, FALSE);

  if (limit &&
      gtk_text_iter_compare (iter, limit) >= 0)
    return FALSE;

  if (*str == '\0')
    {
      /* If we can move one char, return the empty string there */
      match = *iter;

      if (gtk_text_iter_forward_char (&match))
        {
          if (limit &&
              gtk_text_iter_equal (&match, limit))
            return FALSE;

          if (match_start)
            *match_start = match;
          if (match_end)
            *match_end = match;
          return TRUE;
        }
      else
        return FALSE;
    }

  visible_only = (flags & GTK_TEXT_SEARCH_VISIBLE_ONLY) != 0;
  slice = (flags & GTK_TEXT_SEARCH_TEXT_ONLY) == 0;
  case_insensitive = (flags & GTK_TEXT_SEARCH_CASE_INSENSITIVE) != 0;

  /* locate all lines */

  lines = strbreakup (str, "\n", -1, NULL, case_insensitive);

  search = *iter;

  do
    {
      /* This loop has an inefficient worst-case, where
       * gtk_text_iter_get_text() is called repeatedly on
       * a single line.
       */
      GtkTextIter end;

      if (limit &&
          gtk_text_iter_compare (&search, limit) >= 0)
        break;

      if (lines_match (&search, (const char **)lines,
                       visible_only, slice, case_insensitive, &match, &end))
        {
          if (limit == NULL ||
              (limit &&
               gtk_text_iter_compare (&end, limit) <= 0))
            {
              retval = TRUE;

              if (match_start)
                *match_start = match;

              if (match_end)
                *match_end = end;
            }

          break;
        }
    }
  while (gtk_text_iter_forward_line (&search));

  g_strfreev ((char **)lines);

  return retval;
}

static gboolean
vectors_equal_ignoring_trailing (char     **vec1,
                                 char     **vec2,
                                 gboolean   case_insensitive)
{
  /* Ignores trailing chars in vec2's last line */

  char **i1, **i2;

  i1 = vec1;
  i2 = vec2;

  while (*i1 && *i2)
    {
      int len1;
      int len2;

      if (!case_insensitive)
        {
          if (strcmp (*i1, *i2) != 0)
            {
              if (*(i2 + 1) == NULL) /* if this is the last line */
                {
                  len1 = strlen (*i1);
                  len2 = strlen (*i2);

                  if (len2 >= len1 &&
                      strncmp (*i1, *i2, len1) == 0)
                    {
                      /* We matched ignoring the trailing stuff in vec2 */
                      return TRUE;
                    }
                  else
                    {
                      return FALSE;
                    }
                }
              else
                {
                  return FALSE;
                }
            }
        }
      else
        {
          len1 = strlen (*i1);
          len2 = strlen (*i2);

          if (!utf8_caselessnmatch (*i1, *i2, len1, len2))
            {
              if (*(i2 + 1) == NULL) /* if this is the last line */
                {
                  if (utf8_caselessnmatch (*i2, *i1, len2, len1))
                    {
                      /* We matched ignoring the trailing stuff in vec2 */
                      return TRUE;
                    }
                  else
                    {
                      return FALSE;
                    }
                }
              else
                {
                  return FALSE;
                }
            }
        }

      ++i1;
      ++i2;
    }

  if (*i1 || *i2)
    return FALSE;
  else
    return TRUE;
}

typedef struct _LinesWindow LinesWindow;

struct _LinesWindow
{
  int n_lines;
  char **lines;

  GtkTextIter first_line_start;
  GtkTextIter first_line_end;

  guint slice : 1;
  guint visible_only : 1;
};

static void
lines_window_init (LinesWindow       *win,
                   const GtkTextIter *start)
{
  int i;
  GtkTextIter line_start;
  GtkTextIter line_end;

  /* If we start on line 1, there are 2 lines to search (0 and 1), so
   * n_lines can be 2.
   */
  if (gtk_text_iter_is_start (start) ||
      gtk_text_iter_get_line (start) + 1 < win->n_lines)
    {
      /* Already at the end, or not enough lines to match */
      win->lines = g_new0 (char *, 1);
      *win->lines = NULL;
      return;
    }

  line_start = *start;
  line_end = *start;

  /* Move to start iter to start of line */
  gtk_text_iter_set_line_offset (&line_start, 0);

  if (gtk_text_iter_equal (&line_start, &line_end))
    {
      /* we were already at the start; so go back one line */
      gtk_text_iter_backward_line (&line_start);
    }

  win->first_line_start = line_start;
  win->first_line_end = line_end;

  win->lines = g_new0 (char *, win->n_lines + 1);

  i = win->n_lines - 1;
  while (i >= 0)
    {
      char *line_text;

      if (win->slice)
        {
          if (win->visible_only)
            line_text = gtk_text_iter_get_visible_slice (&line_start, &line_end);
          else
            line_text = gtk_text_iter_get_slice (&line_start, &line_end);
        }
      else
        {
          if (win->visible_only)
            line_text = gtk_text_iter_get_visible_text (&line_start, &line_end);
          else
            line_text = gtk_text_iter_get_text (&line_start, &line_end);
        }

      win->lines[i] = line_text;
      win->first_line_start = line_start;
      win->first_line_end = line_end;

      line_end = line_start;
      gtk_text_iter_backward_line (&line_start);

      --i;
    }
}

static gboolean
lines_window_back (LinesWindow *win)
{
  GtkTextIter new_start;
  char *line_text;

  new_start = win->first_line_start;

  if (!gtk_text_iter_backward_line (&new_start))
    return FALSE;
  else
    {
      win->first_line_start = new_start;
      win->first_line_end = new_start;

      gtk_text_iter_forward_line (&win->first_line_end);
    }

  if (win->slice)
    {
      if (win->visible_only)
        line_text = gtk_text_iter_get_visible_slice (&win->first_line_start,
                                                     &win->first_line_end);
      else
        line_text = gtk_text_iter_get_slice (&win->first_line_start,
                                             &win->first_line_end);
    }
  else
    {
      if (win->visible_only)
        line_text = gtk_text_iter_get_visible_text (&win->first_line_start,
                                                    &win->first_line_end);
      else
        line_text = gtk_text_iter_get_text (&win->first_line_start,
                                            &win->first_line_end);
    }

  /* Move lines to make room for first line. */
  memmove (win->lines + 1, win->lines, win->n_lines * sizeof (char *));

  *win->lines = line_text;

  /* Free old last line and NULL-terminate */
  g_free (win->lines[win->n_lines]);
  win->lines[win->n_lines] = NULL;

  return TRUE;
}

static void
lines_window_free (LinesWindow *win)
{
  g_strfreev (win->lines);
}

/**
 * gtk_text_iter_backward_search:
 * @iter: a `GtkTextIter` where the search begins
 * @str: search string
 * @flags: bitmask of flags affecting the search
 * @match_start: (out caller-allocates) (optional): return location for start of match
 * @match_end: (out caller-allocates) (optional): return location for end of match
 * @limit: (nullable): location of last possible @match_start, or %NULL for start of buffer
 *
 * Same as [method@Gtk.TextIter.forward_search], but moves backward.
 *
 * @match_end will never be set to a `GtkTextIter` located after @iter,
 * even if there is a possible @match_start before or at @iter.
 *
 * Returns: whether a match was found
 */
gboolean
gtk_text_iter_backward_search (const GtkTextIter *iter,
                               const char        *str,
                               GtkTextSearchFlags flags,
                               GtkTextIter       *match_start,
                               GtkTextIter       *match_end,
                               const GtkTextIter *limit)
{
  char **lines = NULL;
  char **l;
  int n_lines;
  LinesWindow win;
  gboolean retval = FALSE;
  gboolean visible_only;
  gboolean slice;
  gboolean case_insensitive;

  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (str != NULL, FALSE);

  if (limit &&
      gtk_text_iter_compare (limit, iter) > 0)
    return FALSE;

  if (*str == '\0')
    {
      /* If we can move one char, return the empty string there */
      GtkTextIter match = *iter;

      if (limit && gtk_text_iter_equal (limit, &match))
        return FALSE;

      if (gtk_text_iter_backward_char (&match))
        {
          if (match_start)
            *match_start = match;
          if (match_end)
            *match_end = match;
          return TRUE;
        }
      else
        return FALSE;
    }

  visible_only = (flags & GTK_TEXT_SEARCH_VISIBLE_ONLY) != 0;
  slice = (flags & GTK_TEXT_SEARCH_TEXT_ONLY) == 0;
  case_insensitive = (flags & GTK_TEXT_SEARCH_CASE_INSENSITIVE) != 0;

  /* locate all lines */

  lines = strbreakup (str, "\n", -1, &n_lines, case_insensitive);

  win.n_lines = n_lines;
  win.slice = slice;
  win.visible_only = visible_only;

  lines_window_init (&win, iter);

  if (*win.lines == NULL)
    goto out;

  do
    {
      const char *first_line_match;

      if (limit &&
          gtk_text_iter_compare (limit, &win.first_line_end) > 0)
        {
          /* We're now before the search limit, abort. */
          goto out;
        }

      /* If there are multiple lines, the first line will
       * end in '\n', so this will only match at the
       * end of the first line, which is correct.
       */
      if (!case_insensitive)
        first_line_match = g_strrstr (*win.lines, *lines);
      else
        first_line_match = utf8_strrcasestr (*win.lines, *lines);

      if (first_line_match &&
          vectors_equal_ignoring_trailing (lines + 1, win.lines + 1,
                                           case_insensitive))
        {
          /* Match! */
          int offset;
          GtkTextIter start_tmp;
          GtkTextIter end_tmp;

          /* Offset to start of search string */
          offset = g_utf8_strlen (*win.lines, first_line_match - *win.lines);

          start_tmp = win.first_line_start;
          forward_chars_with_skipping (&start_tmp, offset,
                                       visible_only, !slice, FALSE);

          if (limit &&
              gtk_text_iter_compare (limit, &start_tmp) > 0)
            goto out; /* match was bogus */

          if (match_start)
            *match_start = start_tmp;

          /* Go to end of search string */
          offset = 0;
          for (l = lines; *l != NULL; l++)
            offset += g_utf8_strlen (*l, -1);

          end_tmp = start_tmp;
          forward_chars_with_skipping (&end_tmp, offset,
                                       visible_only, !slice, case_insensitive);

          if (match_end)
            *match_end = end_tmp;

          retval = TRUE;
          goto out;
        }
    }
  while (lines_window_back (&win));

 out:
  lines_window_free (&win);
  g_strfreev (lines);

  return retval;
}

/*
 * Comparisons
 */

/**
 * gtk_text_iter_equal:
 * @lhs: a `GtkTextIter`
 * @rhs: another `GtkTextIter`
 *
 * Tests whether two iterators are equal, using the fastest possible
 * mechanism.
 *
 * This function is very fast; you can expect it to perform
 * better than e.g. getting the character offset for each
 * iterator and comparing the offsets yourself. Also, it’s a
 * bit faster than [method@Gtk.TextIter.compare].
 *
 * Returns: %TRUE if the iterators point to the same place in the buffer
 **/
gboolean
gtk_text_iter_equal (const GtkTextIter *lhs,
                     const GtkTextIter *rhs)
{
  GtkTextRealIter *real_lhs;
  GtkTextRealIter *real_rhs;

  real_lhs = (GtkTextRealIter*)lhs;
  real_rhs = (GtkTextRealIter*)rhs;

  check_invariants (lhs);
  check_invariants (rhs);

  if (real_lhs->line != real_rhs->line)
    return FALSE;
  else if (real_lhs->line_byte_offset >= 0 &&
           real_rhs->line_byte_offset >= 0)
    return real_lhs->line_byte_offset == real_rhs->line_byte_offset;
  else
    {
      /* the ensure_char_offsets() calls do nothing if the char offsets
         are already up-to-date. */
      ensure_char_offsets (real_lhs);
      ensure_char_offsets (real_rhs);
      return real_lhs->line_char_offset == real_rhs->line_char_offset;
    }
}

gboolean
_gtk_text_iter_same_line (const GtkTextIter *lhs,
                          const GtkTextIter *rhs)
{
  GtkTextRealIter *real_lhs;
  GtkTextRealIter *real_rhs;

  real_lhs = gtk_text_iter_make_surreal (lhs);
  real_rhs = gtk_text_iter_make_surreal (rhs);

  if (real_lhs == NULL || real_rhs == NULL)
    return FALSE;

  check_invariants (lhs);
  check_invariants (rhs);

  return real_lhs->line == real_rhs->line;
}

/**
 * gtk_text_iter_compare:
 * @lhs: a `GtkTextIter`
 * @rhs: another `GtkTextIter`
 *
 * A qsort()-style function that returns negative if @lhs is less than
 * @rhs, positive if @lhs is greater than @rhs, and 0 if they’re equal.
 *
 * Ordering is in character offset order, i.e. the first character
 * in the buffer is less than the second character in the buffer.
 *
 * Returns: -1 if @lhs is less than @rhs, 1 if @lhs is greater, 0 if they are equal
 */
int
gtk_text_iter_compare (const GtkTextIter *lhs,
                       const GtkTextIter *rhs)
{
  GtkTextRealIter *real_lhs;
  GtkTextRealIter *real_rhs;

  real_lhs = gtk_text_iter_make_surreal (lhs);
  real_rhs = gtk_text_iter_make_surreal (rhs);

  if (real_lhs == NULL ||
      real_rhs == NULL)
    return -1; /* why not */

  check_invariants (lhs);
  check_invariants (rhs);

  if (real_lhs->line == real_rhs->line)
    {
      int left_index, right_index;

      if (real_lhs->line_byte_offset >= 0 &&
          real_rhs->line_byte_offset >= 0)
        {
          left_index = real_lhs->line_byte_offset;
          right_index = real_rhs->line_byte_offset;
        }
      else
        {
          /* the ensure_char_offsets() calls do nothing if
             the offsets are already up-to-date. */
          ensure_char_offsets (real_lhs);
          ensure_char_offsets (real_rhs);
          left_index = real_lhs->line_char_offset;
          right_index = real_rhs->line_char_offset;
        }

      if (left_index < right_index)
        return -1;
      else if (left_index > right_index)
        return 1;
      else
        return 0;
    }
  else
    {
      int line1, line2;

      line1 = gtk_text_iter_get_line (lhs);
      line2 = gtk_text_iter_get_line (rhs);
      if (line1 < line2)
        return -1;
      else if (line1 > line2)
        return 1;
      else
        return 0;
    }
}

/**
 * gtk_text_iter_in_range:
 * @iter: a `GtkTextIter`
 * @start: start of range
 * @end: end of range
 *
 * Checks whether @iter falls in the range [@start, @end).
 *
 * @start and @end must be in ascending order.
 *
 * Returns: %TRUE if @iter is in the range
 */
gboolean
gtk_text_iter_in_range (const GtkTextIter *iter,
                        const GtkTextIter *start,
                        const GtkTextIter *end)
{
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (start != NULL, FALSE);
  g_return_val_if_fail (end != NULL, FALSE);
  g_return_val_if_fail (gtk_text_iter_compare (start, end) <= 0, FALSE);

  return gtk_text_iter_compare (iter, start) >= 0 &&
    gtk_text_iter_compare (iter, end) < 0;
}

/**
 * gtk_text_iter_order:
 * @first: a `GtkTextIter`
 * @second: another `GtkTextIter`
 *
 * Swaps the value of @first and @second if @second comes before
 * @first in the buffer.
 *
 * That is, ensures that @first and @second are in sequence.
 * Most text buffer functions that take a range call this
 * automatically on your behalf, so there’s no real reason to
 * call it yourself in those cases. There are some exceptions,
 * such as [method@Gtk.TextIter.in_range], that expect a
 * pre-sorted range.
 */
void
gtk_text_iter_order (GtkTextIter *first,
                     GtkTextIter *second)
{
  g_return_if_fail (first != NULL);
  g_return_if_fail (second != NULL);

  if (gtk_text_iter_compare (first, second) > 0)
    {
      GtkTextIter tmp;

      tmp = *first;
      *first = *second;
      *second = tmp;
    }
}

/*
 * Init iterators from the BTree
 */

void
_gtk_text_btree_get_iter_at_char (GtkTextBTree *tree,
                                  GtkTextIter *iter,
                                  int char_index)
{
  GtkTextRealIter *real = (GtkTextRealIter*)iter;
  int real_char_index;
  int line_start;
  GtkTextLine *line;

  g_return_if_fail (iter != NULL);
  g_return_if_fail (tree != NULL);

  line = _gtk_text_btree_get_line_at_char (tree, char_index,
                                           &line_start, &real_char_index);

  iter_init_from_char_offset (iter, tree, line, real_char_index - line_start);

  real->cached_char_index = real_char_index;

  check_invariants (iter);
}

void
_gtk_text_btree_get_iter_at_line_char (GtkTextBTree *tree,
                                       GtkTextIter  *iter,
                                       int           line_number,
                                       int           char_on_line)
{
  GtkTextRealIter *real = (GtkTextRealIter*)iter;
  GtkTextLine *line;
  int real_line;

  g_return_if_fail (iter != NULL);
  g_return_if_fail (tree != NULL);

  line = _gtk_text_btree_get_line_no_last (tree, line_number, &real_line);

  iter_init_from_char_offset (iter, tree, line, char_on_line);

  /* We might as well cache this, since we know it. */
  real->cached_line_number = real_line;

  check_invariants (iter);
}

void
_gtk_text_btree_get_iter_at_line_byte (GtkTextBTree   *tree,
                                       GtkTextIter    *iter,
                                       int             line_number,
                                       int             byte_index)
{
  GtkTextRealIter *real = (GtkTextRealIter*)iter;
  GtkTextLine *line;
  int real_line;

  g_return_if_fail (iter != NULL);
  g_return_if_fail (tree != NULL);

  line = _gtk_text_btree_get_line_no_last (tree, line_number, &real_line);

  iter_init_from_byte_offset (iter, tree, line, byte_index);

  /* We might as well cache this, since we know it. */
  real->cached_line_number = real_line;

  check_invariants (iter);
}

void
_gtk_text_btree_get_iter_at_line      (GtkTextBTree   *tree,
                                       GtkTextIter    *iter,
                                       GtkTextLine    *line,
                                       int             byte_offset)
{
  g_return_if_fail (iter != NULL);
  g_return_if_fail (tree != NULL);
  g_return_if_fail (line != NULL);

  iter_init_from_byte_offset (iter, tree, line, byte_offset);

  check_invariants (iter);
}

gboolean
_gtk_text_btree_get_iter_at_first_toggle (GtkTextBTree   *tree,
                                          GtkTextIter    *iter,
                                          GtkTextTag     *tag)
{
  GtkTextLine *line;

  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (tree != NULL, FALSE);

  line = _gtk_text_btree_first_could_contain_tag (tree, tag);

  if (line == NULL)
    {
      /* Set iter to last in tree */
      _gtk_text_btree_get_end_iter (tree, iter);
      check_invariants (iter);
      return FALSE;
    }
  else
    {
      iter_init_from_byte_offset (iter, tree, line, 0);

      if (!gtk_text_iter_toggles_tag (iter, tag))
        gtk_text_iter_forward_to_tag_toggle (iter, tag);

      check_invariants (iter);
      return TRUE;
    }
}

gboolean
_gtk_text_btree_get_iter_at_last_toggle  (GtkTextBTree   *tree,
                                          GtkTextIter    *iter,
                                          GtkTextTag     *tag)
{
  gboolean found;

  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (tree != NULL, FALSE);

  _gtk_text_btree_get_end_iter (tree, iter);

  if (gtk_text_iter_toggles_tag (iter, tag))
    found = TRUE;
  else
    found = gtk_text_iter_backward_to_tag_toggle (iter, tag);

  check_invariants (iter);

  return found;
}

gboolean
_gtk_text_btree_get_iter_at_mark_name (GtkTextBTree *tree,
                                       GtkTextIter *iter,
                                       const char *mark_name)
{
  GtkTextMark *mark;

  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (tree != NULL, FALSE);

  mark = _gtk_text_btree_get_mark_by_name (tree, mark_name);

  if (mark == NULL)
    return FALSE;
  else
    {
      _gtk_text_btree_get_iter_at_mark (tree, iter, mark);
      check_invariants (iter);
      return TRUE;
    }
}

void
_gtk_text_btree_get_iter_at_paintable (GtkTextBTree *tree,
                                       GtkTextIter *iter,
                                       GtkTextLineSegment *seg)
{
  g_return_if_fail (iter != NULL);
  g_return_if_fail (tree != NULL);

  iter_init_from_segment (iter, tree,
                          seg->body.paintable.line, seg);
  g_assert (seg->body.paintable.line == _gtk_text_iter_get_text_line (iter));
  check_invariants (iter);
}

void
_gtk_text_btree_get_iter_at_mark (GtkTextBTree *tree,
                                  GtkTextIter *iter,
                                  GtkTextMark *mark)
{
  GtkTextLineSegment *seg;

  g_return_if_fail (iter != NULL);
  g_return_if_fail (tree != NULL);
  g_return_if_fail (GTK_IS_TEXT_MARK (mark));

  seg = mark->segment;

  iter_init_from_segment (iter, tree,
                          seg->body.mark.line, seg);
  g_assert (seg->body.mark.line == _gtk_text_iter_get_text_line (iter));
  check_invariants (iter);
}

void
_gtk_text_btree_get_iter_at_child_anchor (GtkTextBTree       *tree,
                                          GtkTextIter        *iter,
                                          GtkTextChildAnchor *anchor)
{
  GtkTextLineSegment *seg;

  g_return_if_fail (iter != NULL);
  g_return_if_fail (tree != NULL);
  g_return_if_fail (GTK_IS_TEXT_CHILD_ANCHOR (anchor));

  seg = anchor->segment;

  g_assert (seg->body.child.line != NULL);

  iter_init_from_segment (iter, tree,
                          seg->body.child.line, seg);
  g_assert (seg->body.child.line == _gtk_text_iter_get_text_line (iter));
  check_invariants (iter);
}

void
_gtk_text_btree_get_end_iter         (GtkTextBTree   *tree,
                                      GtkTextIter    *iter)
{
  g_return_if_fail (iter != NULL);
  g_return_if_fail (tree != NULL);

  _gtk_text_btree_get_iter_at_char (tree,
                                   iter,
                                   _gtk_text_btree_char_count (tree));
  check_invariants (iter);
}

void
_gtk_text_iter_check (const GtkTextIter *iter)
{
  const GtkTextRealIter *real = (const GtkTextRealIter*)iter;
  int line_char_offset, line_byte_offset, seg_char_offset, seg_byte_offset;
  GtkTextLineSegment *byte_segment = NULL;
  GtkTextLineSegment *byte_any_segment = NULL;
  GtkTextLineSegment *char_segment = NULL;
  GtkTextLineSegment *char_any_segment = NULL;
  gboolean segments_updated;

  /* This function checks our class invariants for the Iter class. */

  g_assert (sizeof (GtkTextIter) == sizeof (GtkTextRealIter));

  if (real->chars_changed_stamp !=
      _gtk_text_btree_get_chars_changed_stamp (real->tree))
    g_error ("iterator check failed: invalid iterator");

  if (real->line_char_offset < 0 && real->line_byte_offset < 0)
    g_error ("iterator check failed: both char and byte offsets are invalid");

  segments_updated = (real->segments_changed_stamp ==
                      _gtk_text_btree_get_segments_changed_stamp (real->tree));

#if 0
  printf ("checking iter, segments %s updated, byte %d char %d\n",
          segments_updated ? "are" : "aren't",
          real->line_byte_offset,
          real->line_char_offset);
#endif

  if (segments_updated)
    {
      if (real->segment_char_offset < 0 && real->segment_byte_offset < 0)
        g_error ("iterator check failed: both char and byte segment offsets are invalid");

      if (real->segment->char_count == 0)
        g_error ("iterator check failed: segment is not indexable.");

      if (real->line_char_offset >= 0 && real->segment_char_offset < 0)
        g_error ("segment char offset is not properly up-to-date");

      if (real->line_byte_offset >= 0 && real->segment_byte_offset < 0)
        g_error ("segment byte offset is not properly up-to-date");

      if (real->segment_byte_offset >= 0 &&
          real->segment_byte_offset >= real->segment->byte_count)
        g_error ("segment byte offset is too large.");

      if (real->segment_char_offset >= 0 &&
          real->segment_char_offset >= real->segment->char_count)
        g_error ("segment char offset is too large.");
    }

  if (real->line_byte_offset >= 0)
    {
      _gtk_text_line_byte_locate (real->line, real->line_byte_offset,
                                  &byte_segment, &byte_any_segment,
                                  &seg_byte_offset, &line_byte_offset);

      if (line_byte_offset != real->line_byte_offset)
        g_error ("wrong byte offset was stored in iterator");

      if (segments_updated)
        {
          if (real->segment != byte_segment)
            g_error ("wrong segment was stored in iterator");

          if (real->any_segment != byte_any_segment)
            g_error ("wrong any_segment was stored in iterator");

          if (seg_byte_offset != real->segment_byte_offset)
            g_error ("wrong segment byte offset was stored in iterator");

          if (byte_segment->type == &gtk_text_char_type)
            {
              const char *p;
              p = byte_segment->body.chars + seg_byte_offset;

              if (!gtk_text_byte_begins_utf8_char (p))
                g_error ("broken iterator byte index pointed into the middle of a character");
            }
        }
    }

  if (real->line_char_offset >= 0)
    {
      _gtk_text_line_char_locate (real->line, real->line_char_offset,
                                  &char_segment, &char_any_segment,
                                  &seg_char_offset, &line_char_offset);

      if (line_char_offset != real->line_char_offset)
        g_error ("wrong char offset was stored in iterator");

      if (segments_updated)
        {
          if (real->segment != char_segment)
            g_error ("wrong segment was stored in iterator");

          if (real->any_segment != char_any_segment)
            g_error ("wrong any_segment was stored in iterator");

          if (seg_char_offset != real->segment_char_offset)
            g_error ("wrong segment char offset was stored in iterator");

          if (char_segment->type == &gtk_text_char_type)
            {
              const char *p;
              p = g_utf8_offset_to_pointer (char_segment->body.chars,
                                            seg_char_offset);

              /* hmm, not likely to happen eh */
              if (!gtk_text_byte_begins_utf8_char (p))
                g_error ("broken iterator char offset pointed into the middle of a character");
            }
        }
    }

  if (real->line_char_offset >= 0 && real->line_byte_offset >= 0)
    {
      if (byte_segment != char_segment)
        g_error ("char and byte offsets did not point to the same segment");

      if (byte_any_segment != char_any_segment)
        g_error ("char and byte offsets did not point to the same any segment");

      /* Make sure the segment offsets are equivalent, if it's a char
         segment. */
      if (char_segment->type == &gtk_text_char_type)
        {
          int byte_offset = 0;
          int char_offset = 0;
          while (char_offset < seg_char_offset)
            {
              const char * start = char_segment->body.chars + byte_offset;
              byte_offset += g_utf8_next_char (start) - start;
              char_offset += 1;
            }

          if (byte_offset != seg_byte_offset)
            g_error ("byte offset did not correspond to char offset");

          char_offset =
            g_utf8_strlen (char_segment->body.chars, seg_byte_offset);

          if (char_offset != seg_char_offset)
            g_error ("char offset did not correspond to byte offset");

          if (!gtk_text_byte_begins_utf8_char (char_segment->body.chars + seg_byte_offset))
            g_error ("byte index for iterator does not index the start of a character");
        }
    }

  if (real->cached_line_number >= 0)
    {
      int should_be;

      should_be = _gtk_text_line_get_number (real->line);
      if (real->cached_line_number != should_be)
        g_error ("wrong line number was cached");
    }

  if (real->cached_char_index >= 0)
    {
      if (real->line_char_offset >= 0) /* only way we can check it
                                          efficiently, not a real
                                          invariant. */
        {
          int char_index;

          char_index = _gtk_text_line_char_index (real->line);
          char_index += real->line_char_offset;

          if (real->cached_char_index != char_index)
            g_error ("wrong char index was cached");
        }
    }

  if (_gtk_text_line_is_last (real->line, real->tree))
    g_error ("Iterator was on last line (past the end iterator)");
}
