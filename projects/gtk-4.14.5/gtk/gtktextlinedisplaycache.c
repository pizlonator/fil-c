/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2019 Red Hat, Inc.
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

#include "config.h"

#include "gtktextbtreeprivate.h"
#include "gtktextbufferprivate.h"
#include "gtktextiterprivate.h"
#include "gtktextlinedisplaycacheprivate.h"
#include "gtkprivate.h"

#define DEFAULT_MRU_SIZE         250
#define BLOW_CACHE_TIMEOUT_SEC   20
#define DEBUG_LINE_DISPLAY_CACHE 0

struct _GtkTextLineDisplayCache
{
  GSequence   *sorted_by_line;
  GHashTable  *line_to_display;
  GtkTextLine *cursor_line;
  GQueue       mru;
  GSource     *evict_source;
  guint        mru_size;

#if DEBUG_LINE_DISPLAY_CACHE
  guint       log_source;
  int         hits;
  int         misses;
  int         inval;
  int         inval_cursors;
  int         inval_by_line;
  int         inval_by_range;
  int         inval_by_y_range;
#endif
};

static GQueue purge_in_idle;
static guint purge_in_idle_source;

#if DEBUG_LINE_DISPLAY_CACHE
# define STAT_ADD(val,n) ((val) += n)
# define STAT_INC(val)   STAT_ADD(val,1)
static gboolean
dump_stats (gpointer data)
{
  GtkTextLineDisplayCache *cache = data;
  g_printerr ("%p: size=%u hits=%d misses=%d inval_total=%d "
              "inval_cursors=%d inval_by_line=%d "
              "inval_by_range=%d inval_by_y_range=%d\n",
              cache, g_hash_table_size (cache->line_to_display),
              cache->hits, cache->misses,
              cache->inval, cache->inval_cursors,
              cache->inval_by_line, cache->inval_by_range,
              cache->inval_by_y_range);
  return G_SOURCE_CONTINUE;
}
#else
# define STAT_ADD(val,n)
# define STAT_INC(val)
#endif

GtkTextLineDisplayCache *
gtk_text_line_display_cache_new (void)
{
  GtkTextLineDisplayCache *ret;

  ret = g_new0 (GtkTextLineDisplayCache, 1);
  ret->sorted_by_line = g_sequence_new (NULL);
  ret->line_to_display = g_hash_table_new (NULL, NULL);
  ret->mru_size = DEFAULT_MRU_SIZE;

#if DEBUG_LINE_DISPLAY_CACHE
  ret->log_source = g_timeout_add_seconds (1, dump_stats, ret);
#endif

  return g_steal_pointer (&ret);
}

void
gtk_text_line_display_cache_free (GtkTextLineDisplayCache *cache)
{
#if DEBUG_LINE_DISPLAY_CACHE
  g_clear_handle_id (&cache->log_source, g_source_remove);
#endif

  gtk_text_line_display_cache_invalidate (cache);

  g_assert (g_sequence_get_begin_iter (cache->sorted_by_line) == g_sequence_get_end_iter (cache->sorted_by_line));

  g_clear_pointer (&cache->evict_source, g_source_destroy);
  g_clear_pointer (&cache->sorted_by_line, g_sequence_free);
  g_clear_pointer (&cache->line_to_display, g_hash_table_unref);
  g_free (cache);
}

static gboolean
gtk_text_line_display_cache_blow_cb (gpointer data)
{
  GtkTextLineDisplayCache *cache = data;

  g_assert (cache != NULL);

#if DEBUG_LINE_DISPLAY_CACHE
  g_printerr ("Evicting GtkTextLineDisplayCache\n");
#endif

  cache->evict_source = NULL;

  gtk_text_line_display_cache_invalidate (cache);

  return G_SOURCE_REMOVE;
}

void
gtk_text_line_display_cache_delay_eviction (GtkTextLineDisplayCache *cache)
{
  g_assert (cache != NULL);

  if (cache->evict_source != NULL)
    {
      gint64 deadline;

      deadline = g_get_monotonic_time () + (BLOW_CACHE_TIMEOUT_SEC * G_USEC_PER_SEC);
      g_source_set_ready_time (cache->evict_source, deadline);
    }
  else
    {
      guint tag;

      tag = g_timeout_add_seconds (BLOW_CACHE_TIMEOUT_SEC,
                                   gtk_text_line_display_cache_blow_cb,
                                   cache);
      cache->evict_source = g_main_context_find_source_by_id (NULL, tag);
      g_source_set_static_name (cache->evict_source, "[gtk+] gtk_text_line_display_cache_blow_cb");
    }
}

#if DEBUG_LINE_DISPLAY_CACHE
static void
check_disposition (GtkTextLineDisplayCache *cache,
                   GtkTextLayout           *layout)
{
  GSequenceIter *iter;
  int last = G_MAXUINT;

  g_assert (cache != NULL);
  g_assert (cache->sorted_by_line != NULL);
  g_assert (cache->line_to_display != NULL);

  for (iter = g_sequence_get_begin_iter (cache->sorted_by_line);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    {
      GtkTextLineDisplay *display = g_sequence_get (iter);
      GtkTextIter text_iter;
      guint line;

      gtk_text_layout_get_iter_at_line (layout, &text_iter, display->line, 0);
      line = gtk_text_iter_get_line (&text_iter);

      g_assert_cmpint (line, >, last);

      last = line;
    }
}
#endif

static void
gtk_text_line_display_cache_take_display (GtkTextLineDisplayCache *cache,
                                          GtkTextLineDisplay      *display,
                                          GtkTextLayout           *layout)
{
  g_assert (cache != NULL);
  g_assert (display != NULL);
  g_assert (display->line != NULL);
  g_assert (display->cache_iter == NULL);
  g_assert (display->mru_link.data == display);
  g_assert (display->mru_link.prev == NULL);
  g_assert (display->mru_link.next == NULL);
  g_assert (g_hash_table_lookup (cache->line_to_display, display->line) == NULL);

#if DEBUG_LINE_DISPLAY_CACHE
  check_disposition (cache, layout);
#endif

  display->cache_iter =
    g_sequence_insert_sorted (cache->sorted_by_line,
                              display,
                              (GCompareDataFunc) gtk_text_line_display_compare,
                              layout);
  g_hash_table_insert (cache->line_to_display, display->line, display);
  g_queue_push_head_link (&cache->mru, &display->mru_link);

  /* Cull the cache if we're at capacity */
  while (cache->mru.length > cache->mru_size)
    {
      display = g_queue_peek_tail (&cache->mru);

      gtk_text_line_display_cache_invalidate_display (cache, display, FALSE);
    }
}

static gboolean
purge_text_line_display_in_idle (gpointer data)
{
  GQueue q = purge_in_idle;

  purge_in_idle.head = NULL;
  purge_in_idle.tail = NULL;
  purge_in_idle.length = 0;

  while (q.head)
    {
      GtkTextLineDisplay *display = q.head->data;
      g_queue_unlink (&q, &display->mru_link);
      gtk_text_line_display_unref (display);
    }

  if (purge_in_idle.head == NULL)
    {
      purge_in_idle_source = 0;
      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;
}

/*
 * gtk_text_line_display_cache_invalidate_display:
 * @cache: a GtkTextLineDisplayCache
 * @display: a GtkTextLineDisplay
 * @cursors_only: if only the cursor positions should be invalidated
 *
 * If @cursors_only is TRUE, then only the cursors are invalidated. Otherwise,
 * @display is removed from the cache.
 *
 * Use this function when you already have access to a display as it reduces
 * some overhead.
 */
void
gtk_text_line_display_cache_invalidate_display (GtkTextLineDisplayCache *cache,
                                                GtkTextLineDisplay      *display,
                                                gboolean                 cursors_only)
{
  g_assert (cache != NULL);
  g_assert (display != NULL);
  g_assert (display->line != NULL);

  if (cursors_only)
    {
      g_clear_pointer (&display->cursors, g_array_unref);
      g_clear_pointer (&display->node, gsk_render_node_unref);
      display->cursors_invalid = TRUE;
      display->has_block_cursor = FALSE;
    }
  else
    {
      GSequenceIter *iter = g_steal_pointer (&display->cache_iter);

      if (cache->cursor_line == display->line)
        cache->cursor_line = NULL;

      g_hash_table_remove (cache->line_to_display, display->line);
      g_queue_unlink (&cache->mru, &display->mru_link);

      if (iter != NULL)
        {
          g_sequence_remove (iter);

          g_queue_push_head_link (&purge_in_idle, &display->mru_link);

          /* Purging a lot of GtkTextLineDisplay while processing a frame
           * can increase the chances that we miss our frame deadline. Instead
           * defer that work to right after the frame has completed. This can
           * help situations where we have large, zoomed out TextView like
           * those used in an overview map.
           */
          if G_UNLIKELY (purge_in_idle_source == 0)
            {
              GSource *source;

              purge_in_idle_source = g_idle_add_full (G_PRIORITY_LOW,
                                                      purge_text_line_display_in_idle,
                                                      NULL, NULL);
              source = g_main_context_find_source_by_id (NULL, purge_in_idle_source);
              g_source_set_static_name (source, "[gtk+ line-display-cache-gc]");
            }
        }
    }

  STAT_INC (cache->inval);
}

/*
 * gtk_text_line_display_cache_get:
 * @cache: a `GtkTextLineDisplayCache`
 * @layout: a `GtkTextLayout`
 * @line: a `GtkTextLine`
 * @size_only: if only line sizing is needed
 *
 * Gets a GtkTextLineDisplay for @line.
 *
 * If no cached display exists, a new display will be created.
 *
 * It's possible that calling this function will cause some existing
 * cached displays to be released and destroyed.
 *
 * Returns: (transfer full) (not nullable): a `GtkTextLineDisplay`
 */
GtkTextLineDisplay *
gtk_text_line_display_cache_get (GtkTextLineDisplayCache *cache,
                                 GtkTextLayout           *layout,
                                 GtkTextLine             *line,
                                 gboolean                 size_only)
{
  GtkTextLineDisplay *display;

  g_assert (cache != NULL);
  g_assert (layout != NULL);
  g_assert (line != NULL);

  display = g_hash_table_lookup (cache->line_to_display, line);

  if (display != NULL)
    {
      if (size_only || !display->size_only)
        {
          STAT_INC (cache->hits);

          if (!size_only && display->line == cache->cursor_line)
            gtk_text_layout_update_display_cursors (layout, display->line, display);

          if (!size_only && display->has_children)
            gtk_text_layout_update_children (layout, display);

          /* Move to front of MRU */
          g_queue_unlink (&cache->mru, &display->mru_link);
          g_queue_push_head_link (&cache->mru, &display->mru_link);

          return gtk_text_line_display_ref (display);
        }

      /* We need an updated display that includes more than just
       * sizing, so we need to drop this entry and force the layout
       * to create a new one.
       */
      gtk_text_line_display_cache_invalidate_display (cache, display, FALSE);
    }

  STAT_INC (cache->misses);

  g_assert (!g_hash_table_lookup (cache->line_to_display, line));

  display = gtk_text_layout_create_display (layout, line, size_only);

  g_assert (display != NULL);
  g_assert (display->line == line);

  if (!size_only)
    {
      if (line == cache->cursor_line)
        gtk_text_layout_update_display_cursors (layout, line, display);

      if (display->has_children)
        gtk_text_layout_update_children (layout, display);

      gtk_text_line_display_cache_take_display (cache,
                                                gtk_text_line_display_ref (display),
                                                layout);
    }

  return g_steal_pointer (&display);
}

void
gtk_text_line_display_cache_invalidate (GtkTextLineDisplayCache *cache)
{
  g_assert (cache != NULL);
  g_assert (cache->sorted_by_line != NULL);
  g_assert (cache->line_to_display != NULL);

  STAT_ADD (cache->inval, g_hash_table_size (cache->line_to_display));

  cache->cursor_line = NULL;

  while (cache->mru.head != NULL)
    {
      GtkTextLineDisplay *display = g_queue_peek_head (&cache->mru);

      gtk_text_line_display_cache_invalidate_display (cache, display, FALSE);
    }

  g_assert (g_hash_table_size (cache->line_to_display) == 0);
  g_assert (g_sequence_get_length (cache->sorted_by_line) == 0);
  g_assert (cache->mru.length == 0);
}

void
gtk_text_line_display_cache_invalidate_cursors (GtkTextLineDisplayCache *cache,
                                                GtkTextLine             *line)
{
  GtkTextLineDisplay *display;

  g_assert (cache != NULL);
  g_assert (line != NULL);

  STAT_INC (cache->inval_cursors);

  display = g_hash_table_lookup (cache->line_to_display, line);

  if (display != NULL)
    gtk_text_line_display_cache_invalidate_display (cache, display, TRUE);
}

/*
 * gtk_text_line_display_cache_invalidate_line:
 * @self: a GtkTextLineDisplayCache
 * @line: a GtkTextLine
 *
 * Removes a cached display for @line.
 *
 * Compare to gtk_text_line_display_cache_invalidate_cursors() which
 * only invalidates the cursors for this row.
 */
void
gtk_text_line_display_cache_invalidate_line (GtkTextLineDisplayCache *cache,
                                             GtkTextLine             *line)
{
  GtkTextLineDisplay *display;

  g_assert (cache != NULL);
  g_assert (line != NULL);

  display = g_hash_table_lookup (cache->line_to_display, line);

  if (display != NULL)
    gtk_text_line_display_cache_invalidate_display (cache, display, FALSE);

  STAT_INC (cache->inval_by_line);
}

static GSequenceIter *
find_iter_at_text_iter (GtkTextLineDisplayCache *cache,
                        GtkTextLayout           *layout,
                        const GtkTextIter       *iter)
{
  GSequenceIter *left;
  GSequenceIter *right;
  GSequenceIter *mid;
  GSequenceIter *end;
  GtkTextLine *target;
  guint target_lineno;

  g_assert (cache != NULL);
  g_assert (iter != NULL);

  if (g_sequence_is_empty (cache->sorted_by_line))
    return NULL;

  /* gtk_text_iter_get_line() will have cached value */
  target_lineno = gtk_text_iter_get_line (iter);
  target = _gtk_text_iter_get_text_line (iter);

  /* Get some iters so we can work with pointer compare */
  end = g_sequence_get_end_iter (cache->sorted_by_line);
  left = g_sequence_get_begin_iter (cache->sorted_by_line);
  right = g_sequence_iter_prev (end);

  /* We already checked for empty above */
  g_assert (!g_sequence_iter_is_end (left));
  g_assert (!g_sequence_iter_is_end (right));

  for (;;)
    {
      GtkTextLineDisplay *display;
      guint lineno;

      if (left == right)
        mid = left;
      else
        mid = g_sequence_range_get_midpoint (left, right);

      g_assert (mid != NULL);
      g_assert (!g_sequence_iter_is_end (mid));

      if (mid == end)
        break;

      display = g_sequence_get (mid);

      g_assert (display != NULL);
      g_assert (display->line != NULL);
      g_assert (display->cache_iter != NULL);

      if (target == display->line)
        return mid;

      if (right == left)
        break;

      lineno = _gtk_text_line_get_number (display->line);

      if (target_lineno < lineno)
        right = mid;
      else if (target_lineno > lineno)
        left = g_sequence_iter_next (mid);
      else
        g_assert_not_reached ();
    }

  return NULL;
}


/*
 * gtk_text_line_display_cache_invalidate_range:
 * @cache: a GtkTextLineDisplayCache
 * @begin: the starting text iter
 * @end: the ending text iter
 *
 * Removes all GtkTextLineDisplay that fall between or including
 * @begin and @end.
 */
void
gtk_text_line_display_cache_invalidate_range (GtkTextLineDisplayCache *cache,
                                              GtkTextLayout           *layout,
                                              const GtkTextIter       *begin,
                                              const GtkTextIter       *end,
                                              gboolean                 cursors_only)
{
  GSequenceIter *begin_iter;
  GSequenceIter *end_iter;
  GSequenceIter *iter;

  g_assert (cache != NULL);
  g_assert (layout != NULL);
  g_assert (begin != NULL);
  g_assert (end != NULL);

  STAT_INC (cache->inval_by_range);

  /* Short-circuit, is_empty() is O(1) */
  if (g_sequence_is_empty (cache->sorted_by_line))
    return;

  /* gtk_text_iter_order() preserving const */
  if (gtk_text_iter_compare (begin, end) > 0)
    {
      const GtkTextIter *tmp = end;
      end = begin;
      begin = tmp;
    }

  /* Common case, begin/end on same line. Just try to find the line by
   * line number and invalidate it alone.
   */
  if G_LIKELY (_gtk_text_iter_same_line (begin, end))
    {
      begin_iter = find_iter_at_text_iter (cache, layout, begin);

      if (begin_iter != NULL)
        {
          GtkTextLineDisplay *display = g_sequence_get (begin_iter);

          g_assert (display != NULL);
          g_assert (display->line != NULL);

          gtk_text_line_display_cache_invalidate_display (cache, display, cursors_only);
        }

      return;
    }

  /* Find GSequenceIter containing GtkTextLineDisplay that correspond
   * to each of the text positions.
   */
  begin_iter = find_iter_at_text_iter (cache, layout, begin);
  end_iter = find_iter_at_text_iter (cache, layout, end);

  /* Short-circuit if we found nothing */
  if (begin_iter == NULL && end_iter == NULL)
    return;

  /* If nothing matches the end, we need to walk to the end of our
   * cached displays. We know there is a non-zero number of items
   * in the sequence at this point, so we can iter_prev() safely.
   */
  if (end_iter == NULL)
    end_iter = g_sequence_iter_prev (g_sequence_get_end_iter (cache->sorted_by_line));

  /* If nothing matched the begin, we need to walk starting from
   * the first display we have cached.
   */
  if (begin_iter == NULL)
    begin_iter = g_sequence_get_begin_iter (cache->sorted_by_line);

  iter = begin_iter;

  for (;;)
    {
      GtkTextLineDisplay *display = g_sequence_get (iter);
      GSequenceIter *next = g_sequence_iter_next (iter);

      gtk_text_line_display_cache_invalidate_display (cache, display, cursors_only);

      if (iter == end_iter)
        break;

      iter = next;
    }
}

static GSequenceIter *
find_iter_at_at_y (GtkTextLineDisplayCache *cache,
                   GtkTextLayout           *layout,
                   int                      y)
{
  GtkTextBTree *btree;
  GSequenceIter *left;
  GSequenceIter *right;
  GSequenceIter *mid;
  GSequenceIter *end;

  g_assert (cache != NULL);
  g_assert (layout != NULL);

  if (g_sequence_is_empty (cache->sorted_by_line))
    return NULL;

  btree = _gtk_text_buffer_get_btree (layout->buffer);

  /* Get some iters so we can work with pointer compare */
  end = g_sequence_get_end_iter (cache->sorted_by_line);
  left = g_sequence_get_begin_iter (cache->sorted_by_line);
  right = g_sequence_iter_prev (end);

  /* We already checked for empty above */
  g_assert (!g_sequence_iter_is_end (left));
  g_assert (!g_sequence_iter_is_end (right));

  for (;;)
    {
      GtkTextLineDisplay *display;
      int cache_y;
      int cache_height;

      if (left == right)
        mid = left;
      else
        mid = g_sequence_range_get_midpoint (left, right);

      g_assert (mid != NULL);
      g_assert (!g_sequence_iter_is_end (mid));

      if (mid == end)
        break;

      display = g_sequence_get (mid);

      g_assert (display != NULL);
      g_assert (display->line != NULL);

      cache_y = _gtk_text_btree_find_line_top (btree, display->line, layout);
      cache_height = display->height;

      if (y >= cache_y && y <= (cache_y + cache_height))
        return mid;

      if (left == right)
        break;

      if (y < cache_y)
        right = mid;
      else if (y > (cache_y + cache_height))
        left = g_sequence_iter_next (mid);
      else
        g_assert_not_reached ();
    }

  return NULL;
}

/*
 * gtk_text_line_display_cache_invalidate_y_range:
 * @cache: a GtkTextLineDisplayCache
 * @y: the starting Y position
 * @old_height: the old height of the range
 * @new_height: the new height of the range
 * @cursors_only: if only cursors should be invalidated
 *
 * Remove all GtkTextLineDisplay that fall into the range starting
 * from the Y position to Y+Height.
 */
void
gtk_text_line_display_cache_invalidate_y_range (GtkTextLineDisplayCache *cache,
                                                GtkTextLayout           *layout,
                                                int                      y,
                                                int                      old_height,
                                                int                      new_height,
                                                gboolean                 cursors_only)
{
  GSequenceIter *iter;
  GtkTextBTree *btree;

  g_assert (cache != NULL);
  g_assert (layout != NULL);

  STAT_INC (cache->inval_by_y_range);

  /* A common pattern is to invalidate the whole buffer using y==0 and
   * old_height==new_height. So special case that instead of walking through
   * each display item one at a time.
   */
  if (y < 0 || (y == 0 && old_height == new_height))
    {
      gtk_text_line_display_cache_invalidate (cache);
      return;
    }

  btree = _gtk_text_buffer_get_btree (layout->buffer);
  iter = find_iter_at_at_y (cache, layout, y);

  if (iter == NULL)
    return;

  while (!g_sequence_iter_is_end (iter))
    {
      GtkTextLineDisplay *display;
      int cache_y;
      int cache_height;

      display = g_sequence_get (iter);
      iter = g_sequence_iter_next (iter);

      cache_y = _gtk_text_btree_find_line_top (btree, display->line, layout);
      cache_height = display->height;

      if (cache_y + cache_height >= y && cache_y <= y + old_height)
        {
          gtk_text_line_display_cache_invalidate_display (cache, display, cursors_only);

          y += cache_height;
          old_height -= cache_height;

          if (old_height > 0)
            continue;
        }

      break;
    }
}

void
gtk_text_line_display_cache_set_cursor_line (GtkTextLineDisplayCache *cache,
                                             GtkTextLine             *cursor_line)
{
  GtkTextLineDisplay *display;

  g_assert (cache != NULL);

  if (cursor_line == cache->cursor_line)
    return;

  display = g_hash_table_lookup (cache->line_to_display, cache->cursor_line);

  if (display != NULL)
    gtk_text_line_display_cache_invalidate_display (cache, display, FALSE);

  cache->cursor_line = cursor_line;

  display = g_hash_table_lookup (cache->line_to_display, cache->cursor_line);

  if (display != NULL)
    gtk_text_line_display_cache_invalidate_display (cache, display, FALSE);
}

void
gtk_text_line_display_cache_set_mru_size (GtkTextLineDisplayCache *cache,
                                          guint                    mru_size)
{
  GtkTextLineDisplay *display;

  g_assert (cache != NULL);

  if (mru_size == 0)
    mru_size = DEFAULT_MRU_SIZE;

  if (mru_size != cache->mru_size)
    {
      cache->mru_size = mru_size;

      while (cache->mru.length > cache->mru_size)
        {
          display = g_queue_peek_tail (&cache->mru);

          gtk_text_line_display_cache_invalidate_display (cache, display, FALSE);
        }
    }
}
