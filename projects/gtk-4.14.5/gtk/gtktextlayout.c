/* GTK - The GIMP Toolkit
 * gtktextlayout.c - calculate the layout of the text
 *
 * Copyright (c) 1992-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 2000 Red Hat, Inc.
 * Tk->Gtk port by Havoc Pennington
 * Pango support by Owen Taylor
 *
 * This file can be used under your choice of two licenses, the LGPL
 * and the original Tk license.
 *
 * LGPL:
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
 *
 * Original Tk license:
 *
 * This software is copyrighted by the Regents of the University of
 * California, Sun Microsystems, Inc., and other parties.  The
 * following terms apply to all files associated with the software
 * unless explicitly disclaimed in individual files.
 *
 * The authors hereby grant permission to use, copy, modify,
 * distribute, and license this software and its documentation for any
 * purpose, provided that existing copyright notices are retained in
 * all copies and that this notice is included verbatim in any
 * distributions. No written agreement, license, or royalty fee is
 * required for any of the authorized uses.  Modifications to this
 * software may be copyrighted by their authors and need not follow
 * the licensing terms described here, provided that the new terms are
 * clearly indicated on the first page of each file where they apply.
 *
 * IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY
 * PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
 * DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION,
 * OR ANY DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
 * NON-INFRINGEMENT.  THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS,
 * AND THE AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO PROVIDE
 * MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * GOVERNMENT USE: If you are acquiring this software on behalf of the
 * U.S. government, the Government shall have only "Restricted Rights"
 * in the software and related documentation as defined in the Federal
 * Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
 * are acquiring the software on behalf of the Department of Defense,
 * the software shall be classified as "Commercial Computer Software"
 * and the Government shall have only "Restricted Rights" as defined
 * in Clause 252.227-7013 (c) (1) of DFARs.  Notwithstanding the
 * foregoing, the authors grant the U.S. Government and others acting
 * in its behalf permission to use and distribute the software in
 * accordance with the terms specified in this license.
 *
 */
/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtktextlayoutprivate.h"

#include "gtkcsscolorvalueprivate.h"
#include "gtkmarshalers.h"
#include "gtktextbtreeprivate.h"
#include "gtktextbufferprivate.h"
#include "gtktextiterprivate.h"
#include "gtktextlinedisplaycacheprivate.h"
#include "gtktextutilprivate.h"
#include "gskpangoprivate.h"
#include "gtksnapshotprivate.h"
#include "gtkwidgetprivate.h"
#include "gtktextviewprivate.h"
#include "gtkprivate.h"
#include "gtkrenderlayoutprivate.h"

#include <stdlib.h>
#include <string.h>

#define GTK_TEXT_LAYOUT_GET_PRIVATE(o)  ((GtkTextLayoutPrivate *) gtk_text_layout_get_instance_private ((o)))

typedef struct _GtkTextLayoutPrivate GtkTextLayoutPrivate;

struct _GtkTextLayoutPrivate
{
  /* Cache the line that the cursor is positioned on, as the keyboard
     direction only influences the direction of the cursor line.
  */
  GtkTextLine *cursor_line;

  /* Cache for GtkTextLineDisplay to reduce overhead creating layouts */
  GtkTextLineDisplayCache *cache;
};

static void gtk_text_layout_invalidated     (GtkTextLayout     *layout);

static void gtk_text_layout_invalidate_cache       (GtkTextLayout     *layout,
						    GtkTextLine       *line,
						    gboolean           cursors_only);
static void gtk_text_layout_invalidate_cursor_line (GtkTextLayout     *layout,
						    gboolean           cursors_only);
static void gtk_text_layout_emit_changed           (GtkTextLayout     *layout,
						    int                y,
						    int                old_height,
						    int                new_height);

static void gtk_text_layout_invalidate_all (GtkTextLayout *layout);

static PangoAttribute *gtk_text_attr_appearance_new (const GtkTextAppearance *appearance);

static void gtk_text_layout_after_mark_set_handler     (GtkTextBuffer     *buffer,
                                                        const GtkTextIter *location,
                                                        GtkTextMark       *mark,
                                                        gpointer           data);
static void gtk_text_layout_after_buffer_insert_text   (GtkTextBuffer     *textbuffer,
                                                        GtkTextIter       *iter,
                                                        char              *str,
                                                        int                len,
                                                        gpointer           data);
static void gtk_text_layout_after_buffer_delete_range  (GtkTextBuffer     *textbuffer,
                                                        GtkTextIter       *start,
                                                        GtkTextIter       *end,
                                                        gpointer           data);
static void gtk_text_layout_before_mark_set_handler    (GtkTextBuffer     *buffer,
                                                        const GtkTextIter *location,
                                                        GtkTextMark       *mark,
                                                        gpointer           data);
static void gtk_text_layout_before_buffer_insert_text  (GtkTextBuffer     *textbuffer,
                                                        GtkTextIter       *iter,
                                                        char              *str,
                                                        int                len,
                                                        gpointer           data);
static void gtk_text_layout_before_buffer_delete_range (GtkTextBuffer     *textbuffer,
                                                        GtkTextIter       *start,
                                                        GtkTextIter       *end,
                                                        gpointer           data);


static void gtk_text_layout_update_cursor_line         (GtkTextLayout     *layout);

static void line_display_index_to_iter                 (GtkTextLayout     *layout,
                                                        GtkTextLineDisplay*display,
                                                        GtkTextIter       *iter,
                                                        int                index,
                                                        int                trailing);

static int line_display_iter_to_index                  (GtkTextLayout     *layout,
                                                        GtkTextLineDisplay*display,
                                                        const GtkTextIter *iter);

enum {
  INVALIDATED,
  CHANGED,
  ALLOCATE_CHILD,
  LAST_SIGNAL
};

enum {
  ARG_0,
  LAST_ARG
};

#define PIXEL_BOUND(d) (((d) + PANGO_SCALE - 1) / PANGO_SCALE)

static guint signals[LAST_SIGNAL] = { 0 };

PangoAttrType gtk_text_attr_appearance_type = 0;

G_DEFINE_TYPE_WITH_PRIVATE (GtkTextLayout, gtk_text_layout, G_TYPE_OBJECT)

static void
gtk_text_layout_dispose (GObject *object)
{
  GtkTextLayout *layout = GTK_TEXT_LAYOUT (object);
  GtkTextLayoutPrivate *priv = GTK_TEXT_LAYOUT_GET_PRIVATE (layout);

  g_clear_pointer (&priv->cache, gtk_text_line_display_cache_free);

  gtk_text_layout_set_buffer (layout, NULL);

  if (layout->default_style != NULL)
    {
      gtk_text_attributes_unref (layout->default_style);
      layout->default_style = NULL;
    }

  g_clear_object (&layout->ltr_context);
  g_clear_object (&layout->rtl_context);

  if (layout->preedit_attrs != NULL)
    {
      pango_attr_list_unref (layout->preedit_attrs);
      layout->preedit_attrs = NULL;
    }

  G_OBJECT_CLASS (gtk_text_layout_parent_class)->dispose (object);
}

static void
gtk_text_layout_finalize (GObject *object)
{
  GtkTextLayout *layout;

  layout = GTK_TEXT_LAYOUT (object);

  g_free (layout->preedit_string);

  G_OBJECT_CLASS (gtk_text_layout_parent_class)->finalize (object);
}

static void
gtk_text_layout_class_init (GtkTextLayoutClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_text_layout_dispose;
  object_class->finalize = gtk_text_layout_finalize;

  signals[INVALIDATED] =
    g_signal_new (I_("invalidated"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  signals[CHANGED] =
    g_signal_new (I_("changed"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  _gtk_marshal_VOID__INT_INT_INT,
                  G_TYPE_NONE,
                  3,
                  G_TYPE_INT,
                  G_TYPE_INT,
                  G_TYPE_INT);
  g_signal_set_va_marshaller (signals[CHANGED], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__INT_INT_INTv);

  signals[ALLOCATE_CHILD] =
    g_signal_new (I_("allocate-child"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT_INT_INT,
                  G_TYPE_NONE,
                  3,
                  G_TYPE_OBJECT,
                  G_TYPE_INT,
                  G_TYPE_INT);
  g_signal_set_va_marshaller (signals[ALLOCATE_CHILD], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__OBJECT_INT_INTv);
}

static void
gtk_text_layout_init (GtkTextLayout *text_layout)
{
  GtkTextLayoutPrivate *priv = GTK_TEXT_LAYOUT_GET_PRIVATE (text_layout);

  text_layout->cursor_visible = TRUE;
  priv->cache = gtk_text_line_display_cache_new ();
}

GtkTextLayout*
gtk_text_layout_new (void)
{
  return g_object_new (GTK_TYPE_TEXT_LAYOUT, NULL);
}

static void
free_style_cache (GtkTextLayout *text_layout)
{
  if (text_layout->one_style_cache)
    {
      gtk_text_attributes_unref (text_layout->one_style_cache);
      text_layout->one_style_cache = NULL;
    }
}

/*
 * gtk_text_layout_set_buffer:
 * @buffer: (nullable):
 */
void
gtk_text_layout_set_buffer (GtkTextLayout *layout,
                            GtkTextBuffer *buffer)
{
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (buffer == NULL || GTK_IS_TEXT_BUFFER (buffer));

  if (layout->buffer == buffer)
    return;

  free_style_cache (layout);

  if (layout->buffer)
    {
      _gtk_text_btree_remove_view (_gtk_text_buffer_get_btree (layout->buffer),
                                  layout);

      g_signal_handlers_disconnect_by_func (layout->buffer,
                                            G_CALLBACK (gtk_text_layout_after_mark_set_handler),
                                            layout);
      g_signal_handlers_disconnect_by_func (layout->buffer,
                                            G_CALLBACK (gtk_text_layout_after_buffer_insert_text),
                                            layout);
      g_signal_handlers_disconnect_by_func (layout->buffer,
                                            G_CALLBACK (gtk_text_layout_after_buffer_delete_range),
                                            layout);

      g_signal_handlers_disconnect_by_func (layout->buffer,
                                            G_CALLBACK (gtk_text_layout_before_mark_set_handler),
                                            layout);
      g_signal_handlers_disconnect_by_func (layout->buffer,
                                            G_CALLBACK (gtk_text_layout_before_buffer_insert_text),
                                            layout);
      g_signal_handlers_disconnect_by_func (layout->buffer,
                                            G_CALLBACK (gtk_text_layout_before_buffer_delete_range),
                                            layout);

      g_object_unref (layout->buffer);
      layout->buffer = NULL;
    }

  if (buffer)
    {
      layout->buffer = buffer;

      g_object_ref (buffer);

      _gtk_text_btree_add_view (_gtk_text_buffer_get_btree (buffer), layout);

      /* Bind to all signals that move the insert mark. */
      g_signal_connect_after (layout->buffer, "mark-set",
                              G_CALLBACK (gtk_text_layout_after_mark_set_handler), layout);
      g_signal_connect_after (layout->buffer, "insert-text",
                              G_CALLBACK (gtk_text_layout_after_buffer_insert_text), layout);
      g_signal_connect_after (layout->buffer, "delete-range",
                              G_CALLBACK (gtk_text_layout_after_buffer_delete_range), layout);

      g_signal_connect (layout->buffer, "mark-set",
                        G_CALLBACK (gtk_text_layout_before_mark_set_handler), layout);
      g_signal_connect (layout->buffer, "insert-text",
                        G_CALLBACK (gtk_text_layout_before_buffer_insert_text), layout);
      g_signal_connect (layout->buffer, "delete-range",
                        G_CALLBACK (gtk_text_layout_before_buffer_delete_range), layout);

      gtk_text_layout_update_cursor_line (layout);
    }
}

void
gtk_text_layout_default_style_changed (GtkTextLayout *layout)
{
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));

  DV (g_print ("invalidating all due to default style change (%s)\n", G_STRLOC));
  gtk_text_layout_invalidate_all (layout);
}

void
gtk_text_layout_set_default_style (GtkTextLayout     *layout,
                                   GtkTextAttributes *values)
{
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (values != NULL);

  if (values == layout->default_style)
    return;

  gtk_text_attributes_ref (values);

  if (layout->default_style)
    gtk_text_attributes_unref (layout->default_style);

  layout->default_style = values;

  gtk_text_layout_default_style_changed (layout);
}

void
gtk_text_layout_set_contexts (GtkTextLayout *layout,
                              PangoContext  *ltr_context,
                              PangoContext  *rtl_context)
{
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));

  if (layout->ltr_context != ltr_context)
    {
      if (layout->ltr_context)
	g_object_unref (layout->ltr_context);

      layout->ltr_context = ltr_context;
      g_object_ref (layout->ltr_context);
    }

  if (layout->rtl_context != rtl_context)
    {
      if (layout->rtl_context)
	g_object_unref (layout->rtl_context);

      layout->rtl_context = rtl_context;
      g_object_ref (layout->rtl_context);
    }

  DV (g_print ("invalidating all due to new pango contexts (%s)\n", G_STRLOC));
  gtk_text_layout_invalidate_all (layout);
}

/**
 * gtk_text_layout_set_overwrite_mode:
 * @layout: a `GtkTextLayout`
 * @overwrite: overwrite mode
 *
 * Sets overwrite mode
 */
void
gtk_text_layout_set_overwrite_mode (GtkTextLayout *layout,
				    gboolean       overwrite)
{
  overwrite = overwrite != 0;
  if (overwrite != layout->overwrite_mode)
    {
      layout->overwrite_mode = overwrite;
      gtk_text_layout_invalidate_cursor_line (layout, TRUE);
    }
}

/**
 * gtk_text_layout_set_cursor_direction:
 * @direction: the new direction(s) for which to draw cursors.
 *   %GTK_TEXT_DIR_NONE means draw cursors for both
 *   left-to-right insertion and right-to-left insertion.
 *   (The two cursors will be visually distinguished.)
 *
 * Sets which text directions (left-to-right and/or right-to-left) for
 * which cursors will be drawn for the insertion point. The visual
 * point at which new text is inserted depends on whether the new
 * text is right-to-left or left-to-right, so it may be desired to
 * make the drawn position of the cursor depend on the keyboard state.
 */
void
gtk_text_layout_set_cursor_direction (GtkTextLayout   *layout,
				      GtkTextDirection direction)
{
  if (direction != layout->cursor_direction)
    {
      layout->cursor_direction = direction;
      gtk_text_layout_invalidate_cursor_line (layout, TRUE);
    }
}

/**
 * gtk_text_layout_set_keyboard_direction:
 * @keyboard_dir: the current direction of the keyboard.
 *
 * Sets the keyboard direction; this is used as for the bidirectional
 * base direction for the line with the cursor if the line contains
 * only neutral characters.
 */
void
gtk_text_layout_set_keyboard_direction (GtkTextLayout   *layout,
					GtkTextDirection keyboard_dir)
{
  if (keyboard_dir != layout->keyboard_direction)
    {
      layout->keyboard_direction = keyboard_dir;
      gtk_text_layout_invalidate_cursor_line (layout, TRUE);
    }
}

/**
 * gtk_text_layout_get_buffer:
 * @layout: a `GtkTextLayout`
 *
 * Gets the text buffer used by the layout. See
 * gtk_text_layout_set_buffer().
 *
 * Returns: the text buffer used by the layout.
 */
GtkTextBuffer *
gtk_text_layout_get_buffer (GtkTextLayout *layout)
{
  g_return_val_if_fail (GTK_IS_TEXT_LAYOUT (layout), NULL);

  return layout->buffer;
}

void
gtk_text_layout_set_screen_width (GtkTextLayout *layout, int width)
{
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (width >= 0);
  g_return_if_fail (layout->wrap_loop_count == 0);

  if (layout->screen_width == width)
    return;

  layout->screen_width = width;

  DV (g_print ("invalidating all due to new screen width (%s)\n", G_STRLOC));
  gtk_text_layout_invalidate_all (layout);
}

/**
 * gtk_text_layout_set_cursor_visible:
 * @layout: a `GtkTextLayout`
 * @cursor_visible: If %FALSE, then the insertion cursor will not
 *   be shown, even if the text is editable.
 *
 * Sets whether the insertion cursor should be shown. Generally,
 * widgets using `GtkTextLayout` will hide the cursor when the
 * widget does not have the input focus.
 */
void
gtk_text_layout_set_cursor_visible (GtkTextLayout *layout,
                                    gboolean       cursor_visible)
{
  cursor_visible = (cursor_visible != FALSE);

  if (layout->cursor_visible != cursor_visible)
    {
      GtkTextIter iter;
      int y, height;

      layout->cursor_visible = cursor_visible;

      /* Now queue a redraw on the paragraph containing the cursor
       */
      gtk_text_buffer_get_iter_at_mark (layout->buffer, &iter,
                                        gtk_text_buffer_get_insert (layout->buffer));

      gtk_text_layout_get_line_yrange (layout, &iter, &y, &height);
      gtk_text_layout_emit_changed (layout, y, height, height);

      gtk_text_layout_invalidate_cache (layout, _gtk_text_iter_get_text_line (&iter), TRUE);
    }
}

/**
 * gtk_text_layout_get_cursor_visible:
 * @layout: a `GtkTextLayout`
 *
 * Returns whether the insertion cursor will be shown.
 *
 * Returns: if %FALSE, the insertion cursor will not be
 *   shown, even if the text is editable.
 */
gboolean
gtk_text_layout_get_cursor_visible (GtkTextLayout *layout)
{
  return layout->cursor_visible;
}

/**
 * gtk_text_layout_set_preedit_string:
 * @layout: a `PangoLayout`
 * @preedit_string: a string to display at the insertion point
 * @preedit_attrs: a `PangoAttrList` of attributes that apply to @preedit_string
 * @cursor_pos: position of cursor within preedit string in chars
 *
 * Set the preedit string and attributes. The preedit string is a
 * string showing text that is currently being edited and not
 * yet committed into the buffer.
 */
void
gtk_text_layout_set_preedit_string (GtkTextLayout *layout,
				    const char    *preedit_string,
				    PangoAttrList *preedit_attrs,
				    int            cursor_pos)
{
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (preedit_attrs != NULL || preedit_string == NULL);

  g_free (layout->preedit_string);

  if (layout->preedit_attrs)
    pango_attr_list_unref (layout->preedit_attrs);

  if (preedit_string)
    {
      layout->preedit_string = g_strdup (preedit_string);
      layout->preedit_len = strlen (layout->preedit_string);
      pango_attr_list_ref (preedit_attrs);
      layout->preedit_attrs = preedit_attrs;

      cursor_pos = CLAMP (cursor_pos, 0, g_utf8_strlen (layout->preedit_string, -1));
      layout->preedit_cursor = g_utf8_offset_to_pointer (layout->preedit_string, cursor_pos) - layout->preedit_string;
    }
  else
    {
      layout->preedit_string = NULL;
      layout->preedit_len = 0;
      layout->preedit_attrs = NULL;
      layout->preedit_cursor = 0;
    }

  gtk_text_layout_invalidate_cursor_line (layout, FALSE);
}

void
gtk_text_layout_get_size (GtkTextLayout *layout,
                          int *width,
                          int *height)
{
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));

  if (width)
    *width = layout->width;

  if (height)
    *height = layout->height;
}

static void
gtk_text_layout_invalidated (GtkTextLayout *layout)
{
  g_signal_emit (layout, signals[INVALIDATED], 0);
}

static void
gtk_text_layout_emit_changed (GtkTextLayout *layout,
			      int            y,
			      int            old_height,
			      int            new_height)
{
  g_signal_emit (layout, signals[CHANGED], 0, y, old_height, new_height);
}

void
gtk_text_layout_changed (GtkTextLayout *layout,
                         int            y,
                         int            old_height,
                         int            new_height)
{
  GtkTextLayoutPrivate *priv = GTK_TEXT_LAYOUT_GET_PRIVATE (layout);
  gtk_text_line_display_cache_invalidate_y_range (priv->cache, layout, y, old_height, new_height, FALSE);
  gtk_text_layout_emit_changed (layout, y, old_height, new_height);
}

void
gtk_text_layout_cursors_changed (GtkTextLayout *layout,
                                 int            y,
                                 int            old_height,
                                 int            new_height)
{
  GtkTextLayoutPrivate *priv = GTK_TEXT_LAYOUT_GET_PRIVATE (layout);
  gtk_text_line_display_cache_invalidate_y_range (priv->cache, layout, y, old_height, new_height, TRUE);
  gtk_text_layout_emit_changed (layout, y, old_height, new_height);
}

static void
invalidate_cached_style (GtkTextLayout *layout)
{
  free_style_cache (layout);
}

/* These should be called around a loop which wraps a CONTIGUOUS bunch
 * of display lines. If the lines aren’t contiguous you can’t call
 * these.
 */
void
gtk_text_layout_wrap_loop_start (GtkTextLayout *layout)
{
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (layout->one_style_cache == NULL);

  layout->wrap_loop_count += 1;
}

void
gtk_text_layout_wrap_loop_end (GtkTextLayout *layout)
{
  g_return_if_fail (layout->wrap_loop_count > 0);

  layout->wrap_loop_count -= 1;

  if (layout->wrap_loop_count == 0)
    {
      /* We cache a some stuff if we're iterating over some lines wrapping
       * them. This cleans it up.
       */
      /* Nuke our cached style */
      invalidate_cached_style (layout);
      g_assert (layout->one_style_cache == NULL);
    }
}

static void
gtk_text_layout_invalidate_all (GtkTextLayout *layout)
{
  GtkTextIter start;
  GtkTextIter end;

  if (layout->buffer == NULL)
    return;

  gtk_text_buffer_get_bounds (layout->buffer, &start, &end);

  gtk_text_layout_invalidate (layout, &start, &end);
}

static void
gtk_text_layout_invalidate_cache (GtkTextLayout *layout,
                                  GtkTextLine   *line,
				  gboolean       cursors_only)
{
  GtkTextLayoutPrivate *priv = GTK_TEXT_LAYOUT_GET_PRIVATE (layout);

  g_assert (GTK_IS_TEXT_LAYOUT (layout));

  if (priv->cache != NULL)
    {
      if (cursors_only)
        gtk_text_line_display_cache_invalidate_cursors (priv->cache, line);
      else
        gtk_text_line_display_cache_invalidate_line (priv->cache, line);
    }
}

/* Now invalidate the paragraph containing the cursor
 */
static void
gtk_text_layout_invalidate_cursor_line (GtkTextLayout *layout,
					gboolean       cursors_only)
{
  GtkTextLayoutPrivate *priv = GTK_TEXT_LAYOUT_GET_PRIVATE (layout);
  GtkTextLineData *line_data;

  if (priv->cursor_line == NULL)
    return;

  line_data = _gtk_text_line_get_data (priv->cursor_line, layout);

  if (line_data != NULL)
    {
      gtk_text_layout_invalidate_cache (layout, priv->cursor_line, cursors_only);

      if (!cursors_only)
        _gtk_text_line_invalidate_wrap (priv->cursor_line, line_data);

      gtk_text_layout_invalidated (layout);
    }
}

static void
gtk_text_layout_update_cursor_line (GtkTextLayout *layout)
{
  GtkTextLayoutPrivate *priv = GTK_TEXT_LAYOUT_GET_PRIVATE (layout);
  GtkTextIter iter;

  gtk_text_buffer_get_iter_at_mark (layout->buffer, &iter,
                                    gtk_text_buffer_get_insert (layout->buffer));

  priv->cursor_line = _gtk_text_iter_get_text_line (&iter);

  gtk_text_line_display_cache_set_cursor_line (priv->cache, priv->cursor_line);
}

void
gtk_text_layout_invalidate (GtkTextLayout     *layout,
			    const GtkTextIter *start,
			    const GtkTextIter *end)
{
  GtkTextLine *line;
  GtkTextLine *last_line;

  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (layout->wrap_loop_count == 0);

  /* Because we may be invalidating a mark, it's entirely possible
   * that gtk_text_iter_equal (start, end) in which case we
   * should still invalidate the line they are both on. i.e.
   * we always invalidate the line with "start" even
   * if there's an empty range.
   */

#if 0
  gtk_text_view_index_spew (start_index, "invalidate start");
  gtk_text_view_index_spew (end_index, "invalidate end");
#endif

  last_line = _gtk_text_iter_get_text_line (end);
  line = _gtk_text_iter_get_text_line (start);

  while (TRUE)
    {
      GtkTextLineData *line_data = _gtk_text_line_get_data (line, layout);

      gtk_text_layout_invalidate_cache (layout, line, FALSE);

      if (line_data)
        _gtk_text_line_invalidate_wrap (line, line_data);

      if (line == last_line)
        break;

      line = _gtk_text_line_next_excluding_last (line);
    }

  gtk_text_layout_invalidated (layout);
}

void
gtk_text_layout_invalidate_cursors (GtkTextLayout     *layout,
				    const GtkTextIter *start,
				    const GtkTextIter *end)
{
  GtkTextLayoutPrivate *priv = GTK_TEXT_LAYOUT_GET_PRIVATE (layout);
  gtk_text_line_display_cache_invalidate_range (priv->cache, layout, start, end, TRUE);
  gtk_text_layout_invalidated (layout);
}

void
gtk_text_layout_invalidate_selection (GtkTextLayout *layout)
{
  GtkTextIter selection_start, selection_end;

  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));

  if (layout->buffer &&
      gtk_text_buffer_get_selection_bounds (layout->buffer,
                                            &selection_start,
                                            &selection_end))
    gtk_text_layout_invalidate (layout, &selection_start, &selection_end);
}

void
gtk_text_layout_free_line_data (GtkTextLayout   *layout,
                                GtkTextLine     *line,
                                GtkTextLineData *line_data)
{
  gtk_text_layout_invalidate_cache (layout, line, FALSE);

  g_free (line_data);
}

/**
 * gtk_text_layout_is_valid:
 * @layout: a `GtkTextLayout`
 *
 * Check if there are any invalid regions in a `GtkTextLayout`’s buffer
 *
 * Returns: %TRUE if any invalid regions were found
 */
gboolean
gtk_text_layout_is_valid (GtkTextLayout *layout)
{
  g_return_val_if_fail (GTK_IS_TEXT_LAYOUT (layout), FALSE);

  return _gtk_text_btree_is_valid (_gtk_text_buffer_get_btree (layout->buffer),
                                  layout);
}

static void
update_layout_size (GtkTextLayout *layout)
{
  _gtk_text_btree_get_view_size (_gtk_text_buffer_get_btree (layout->buffer),
				layout,
				&layout->width, &layout->height);
}

/**
 * gtk_text_layout_validate_yrange:
 * @layout: a `GtkTextLayout`
 * @anchor: iter pointing into a line that will be used as the
 *   coordinate origin
 * @y0_: offset from the top of the line pointed to by @anchor at
 *   which to begin validation. (The offset here is in pixels
 *   after validation.)
 * @y1_: offset from the top of the line pointed to by @anchor at
 *   which to end validation. (The offset here is in pixels
 *   after validation.)
 *
 * Ensure that a region of a `GtkTextLayout` is valid. The ::changed
 * signal will be emitted if any lines are validated.
 */
void
gtk_text_layout_validate_yrange (GtkTextLayout *layout,
                                 GtkTextIter   *anchor,
                                 int            y0,
                                 int            y1)
{
  GtkTextLine *line;
  GtkTextLine *first_line = NULL;
  GtkTextLine *last_line = NULL;
  int seen;
  int delta_height = 0;
  int first_line_y = 0;        /* Quiet GCC */
  int last_line_y = 0;         /* Quiet GCC */

  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));

  if (y0 > 0)
    y0 = 0;
  if (y1 < 0)
    y1 = 0;

  /* Validate backwards from the anchor line to y0
   */
  line = _gtk_text_iter_get_text_line (anchor);
  line = _gtk_text_line_previous (line);
  seen = 0;
  while (line && seen < -y0)
    {
      GtkTextLineData *line_data = _gtk_text_line_get_data (line, layout);
      if (!line_data || !line_data->valid)
        {
          int old_height, new_height;
          int top_ink, bottom_ink;

	  old_height = line_data ? line_data->height : 0;
          top_ink = line_data ? line_data->top_ink : 0;
          bottom_ink = line_data ? line_data->bottom_ink : 0;

          _gtk_text_btree_validate_line (_gtk_text_buffer_get_btree (layout->buffer),
                                         line, layout);
          line_data = _gtk_text_line_get_data (line, layout);

	  new_height = line_data ? line_data->height : 0;
          if (line_data)
            {
              top_ink = MAX (top_ink, line_data->top_ink);
              bottom_ink = MAX (bottom_ink, line_data->bottom_ink);
            }

          delta_height += new_height - old_height;

          first_line = line;
          first_line_y = -seen - new_height - top_ink;
          if (!last_line)
            {
              last_line = line;
              last_line_y = -seen + bottom_ink;
            }
        }

      seen += line_data ? line_data->height : 0;
      line = _gtk_text_line_previous (line);
    }

  /* Validate forwards to y1 */
  line = _gtk_text_iter_get_text_line (anchor);
  seen = 0;
  while (line && seen < y1)
    {
      GtkTextLineData *line_data = _gtk_text_line_get_data (line, layout);
      if (!line_data || !line_data->valid)
        {
          int old_height, new_height;
          int top_ink, bottom_ink;

	  old_height = line_data ? line_data->height : 0;
          top_ink = line_data ? line_data->top_ink : 0;
          bottom_ink = line_data ? line_data->bottom_ink : 0;

          _gtk_text_btree_validate_line (_gtk_text_buffer_get_btree (layout->buffer),
                                         line, layout);
          line_data = _gtk_text_line_get_data (line, layout);
	  new_height = line_data ? line_data->height : 0;
          if (line_data)
            {
              top_ink = MAX (top_ink, line_data->top_ink);
              bottom_ink = MAX (bottom_ink, line_data->bottom_ink);
            }

          delta_height += new_height - old_height;

          if (!first_line)
            {
              first_line = line;
              first_line_y = seen - top_ink;
            }
          last_line = line;
          last_line_y = seen + new_height + bottom_ink;
        }

      seen += line_data ? line_data->height : 0;
      line = _gtk_text_line_next_excluding_last (line);
    }

  /* If we found and validated any invalid lines, update size and
   * emit the changed signal
   */
  if (first_line)
    {
      int line_top;

      update_layout_size (layout);

      line_top = _gtk_text_btree_find_line_top (_gtk_text_buffer_get_btree (layout->buffer),
                                                first_line, layout);

      gtk_text_layout_emit_changed (layout,
				    line_top,
				    last_line_y - first_line_y - delta_height,
				    last_line_y - first_line_y);
    }
}

/**
 * gtk_text_layout_validate:
 * @tree: a `GtkTextLayout`
 * @max_pixels: the maximum number of pixels to validate. (No more
 *   than one paragraph beyond this limit will be validated)
 *
 * Validate regions of a `GtkTextLayout`. The ::changed signal will
 * be emitted for each region validated.
 **/
void
gtk_text_layout_validate (GtkTextLayout *layout,
                          int            max_pixels)
{
  GtkTextBTree *btree;
  int y, old_height, new_height;

  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));

  btree = _gtk_text_buffer_get_btree (layout->buffer);
  while (max_pixels > 0 &&
         _gtk_text_btree_validate (btree,
                                   layout,  max_pixels,
                                   &y, &old_height, &new_height))
    {
      max_pixels -= new_height;

      update_layout_size (layout);
      gtk_text_layout_emit_changed (layout, y, old_height, new_height);
    }
}

GtkTextLineData *
gtk_text_layout_wrap (GtkTextLayout   *layout,
                      GtkTextLine     *line,
                      /* may be NULL */
                      GtkTextLineData *line_data)
{
  GtkTextLineDisplay *display;
  PangoRectangle ink_rect, logical_rect;

  g_return_val_if_fail (GTK_IS_TEXT_LAYOUT (layout), NULL);
  g_return_val_if_fail (line != NULL, NULL);

  if (line_data == NULL)
    {
      line_data = _gtk_text_line_data_new (layout, line);
      _gtk_text_line_add_data (line, line_data);
    }

  display = gtk_text_layout_get_line_display (layout, line, TRUE);
  line_data->width = display->width;
  line_data->height = display->height;
  line_data->valid = TRUE;
  pango_layout_get_pixel_extents (display->layout, &ink_rect, &logical_rect);
  line_data->top_ink = MAX (0, logical_rect.x - ink_rect.x);
  line_data->bottom_ink = MAX (0, logical_rect.x + logical_rect.width - ink_rect.x - ink_rect.width);
  gtk_text_line_display_unref (display);

  return line_data;
}

/*
 * Layout utility functions
 */

/* If you get the style with get_style () you need to call
   release_style () to free it. */
static GtkTextAttributes*
get_style (GtkTextLayout *layout,
	   GPtrArray     *tags)
{
  GtkTextAttributes *style;

  /* If we have the one-style cache, then it means
     that we haven't seen a toggle since we filled in the
     one-style cache.
  */
  if (layout->one_style_cache != NULL)
    {
      gtk_text_attributes_ref (layout->one_style_cache);
      return layout->one_style_cache;
    }

  g_assert (layout->one_style_cache == NULL);

  /* No tags, use default style */
  if (tags == NULL || tags->len == 0)
    {
      /* One ref for the return value, one ref for the
         layout->one_style_cache reference */
      gtk_text_attributes_ref (layout->default_style);
      gtk_text_attributes_ref (layout->default_style);
      layout->one_style_cache = layout->default_style;

      return layout->default_style;
    }

  style = gtk_text_attributes_new ();

  gtk_text_attributes_copy_values (layout->default_style,
                                   style);

  _gtk_text_attributes_fill_from_tags (style, tags);

  g_assert (style->refcount == 1);

  /* Leave this style as the last one seen */
  g_assert (layout->one_style_cache == NULL);
  gtk_text_attributes_ref (style); /* ref held by layout->one_style_cache */
  layout->one_style_cache = style;

  /* Returning yet another refcount */
  return style;
}

static void
release_style (GtkTextLayout *layout,
               GtkTextAttributes *style)
{
  g_return_if_fail (style != NULL);
  g_return_if_fail (style->refcount > 0);

  gtk_text_attributes_unref (style);
}

/*
 * Lines
 */

/* This function tries to optimize the case where a line
   is completely invisible */
static gboolean
totally_invisible_line (GtkTextLayout *layout,
                        GtkTextLine   *line,
                        GtkTextIter   *iter)
{
  GtkTextLineSegment *seg;

  /* Check if the first char is visible, if so we are partially visible.
   * Note that we have to check this since we don't know the current
   * invisible/noninvisible toggle state; this function can use the whole btree
   * to get it right.
   */
  gtk_text_layout_get_iter_at_line (layout, iter, line, 0);
  if (!_gtk_text_btree_char_is_invisible (iter))
    return FALSE;

  seg = line->segments;

  while (seg != NULL)
    {
      if (seg->byte_count <= 0 &&
          seg->type == &gtk_text_toggle_on_type)
        {
          invalidate_cached_style (layout);

          /* Bail out if an elision-unsetting tag begins */
          if (seg->body.toggle.info->tag->priv->invisible_set &&
              !seg->body.toggle.info->tag->priv->values->invisible)
            break;
        }
      else if (seg->type == &gtk_text_toggle_off_type)
        {
          invalidate_cached_style (layout);

          /* Bail out if an elision-setting tag ends */
          if (seg->body.toggle.info->tag->priv->invisible_set &&
              seg->body.toggle.info->tag->priv->values->invisible)
            break;
        }

      seg = seg->next;
    }

  if (seg != NULL)       /* didn't reach line end */
    return FALSE;

  return TRUE;
}

static void
set_para_values (GtkTextLayout      *layout,
                 PangoDirection      base_dir,
                 GtkTextAttributes  *style,
                 GtkTextLineDisplay *display)
{
  PangoAlignment pango_align = PANGO_ALIGN_LEFT;
  PangoWrapMode pango_wrap = PANGO_WRAP_WORD;
  int h_margin;
  int h_padding;

  switch (base_dir)
    {
    /* If no base direction was found, then use the style direction */
    case PANGO_DIRECTION_NEUTRAL :
      display->direction = style->direction;

      /* Override the base direction */
      if (display->direction == GTK_TEXT_DIR_RTL)
        base_dir = PANGO_DIRECTION_RTL;
      else
        base_dir = PANGO_DIRECTION_LTR;

      break;
    case PANGO_DIRECTION_RTL :
      display->direction = GTK_TEXT_DIR_RTL;
      break;
    case PANGO_DIRECTION_LTR:
    case PANGO_DIRECTION_TTB_LTR:
    case PANGO_DIRECTION_TTB_RTL:
    case PANGO_DIRECTION_WEAK_LTR:
    case PANGO_DIRECTION_WEAK_RTL:
    default:
      display->direction = GTK_TEXT_DIR_LTR;
      break;
    }

  if (display->direction == GTK_TEXT_DIR_RTL)
    display->layout = pango_layout_new (layout->rtl_context);
  else
    display->layout = pango_layout_new (layout->ltr_context);

  switch (style->justification)
    {
    case GTK_JUSTIFY_LEFT:
      pango_align = (base_dir == PANGO_DIRECTION_LTR) ? PANGO_ALIGN_LEFT : PANGO_ALIGN_RIGHT;
      break;
    case GTK_JUSTIFY_RIGHT:
      pango_align = (base_dir == PANGO_DIRECTION_LTR) ? PANGO_ALIGN_RIGHT : PANGO_ALIGN_LEFT;
      break;
    case GTK_JUSTIFY_CENTER:
      pango_align = PANGO_ALIGN_CENTER;
      break;
    case GTK_JUSTIFY_FILL:
      pango_align = (base_dir == PANGO_DIRECTION_LTR) ? PANGO_ALIGN_LEFT : PANGO_ALIGN_RIGHT;
      pango_layout_set_justify (display->layout, TRUE);
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  pango_layout_set_alignment (display->layout, pango_align);
  pango_layout_set_spacing (display->layout,
                            style->pixels_inside_wrap * PANGO_SCALE);

  if (style->tabs)
    pango_layout_set_tabs (display->layout, style->tabs);

  display->top_margin = style->pixels_above_lines;
  display->height = style->pixels_above_lines + style->pixels_below_lines;
  display->bottom_margin = style->pixels_below_lines;
  display->left_margin = style->left_margin;
  display->right_margin = style->right_margin;

  display->x_offset = display->left_margin;

  pango_layout_set_indent (display->layout,
                           style->indent * PANGO_SCALE);

  switch (style->wrap_mode)
    {
    case GTK_WRAP_CHAR:
      pango_wrap = PANGO_WRAP_CHAR;
      break;
    case GTK_WRAP_WORD:
      pango_wrap = PANGO_WRAP_WORD;
      break;

    case GTK_WRAP_WORD_CHAR:
      pango_wrap = PANGO_WRAP_WORD_CHAR;
      break;

    case GTK_WRAP_NONE:
    default:
      break;
    }

  h_margin = display->left_margin + display->right_margin;
  h_padding = layout->left_padding + layout->right_padding;

  if (style->wrap_mode != GTK_WRAP_NONE)
    {
      int layout_width = (layout->screen_width - h_margin - h_padding);
      pango_layout_set_width (display->layout, layout_width * PANGO_SCALE);
      pango_layout_set_wrap (display->layout, pango_wrap);
    }
  display->total_width = MAX (layout->screen_width, layout->width) - h_margin - h_padding;

  if (style->pg_bg_rgba)
    {
      display->pg_bg_rgba = *style->pg_bg_rgba;
      display->pg_bg_rgba_set = TRUE;
    }
  else
    {
      display->pg_bg_rgba_set = FALSE;
    }
}

static PangoAttribute *
gtk_text_attr_appearance_copy (const PangoAttribute *attr)
{
  const GtkTextAttrAppearance *appearance_attr = (const GtkTextAttrAppearance *)attr;

  return gtk_text_attr_appearance_new (&appearance_attr->appearance);
}

static void
gtk_text_attr_appearance_destroy (PangoAttribute *attr)
{
  GtkTextAttrAppearance *appearance_attr = (GtkTextAttrAppearance *)attr;

  if (appearance_attr->appearance.fg_rgba)
    gdk_rgba_free (appearance_attr->appearance.fg_rgba);

  if (appearance_attr->appearance.bg_rgba)
    gdk_rgba_free (appearance_attr->appearance.bg_rgba);

  if (appearance_attr->appearance.underline_rgba)
    gdk_rgba_free (appearance_attr->appearance.underline_rgba);

  if (appearance_attr->appearance.overline_rgba)
    gdk_rgba_free (appearance_attr->appearance.overline_rgba);

  if (appearance_attr->appearance.strikethrough_rgba)
    gdk_rgba_free (appearance_attr->appearance.strikethrough_rgba);

  g_free (appearance_attr);
}

static gboolean
rgba_equal (const GdkRGBA *rgba1, const GdkRGBA *rgba2)
{
  if (rgba1 && rgba2)
    return gdk_rgba_equal (rgba1, rgba2);

  if (rgba1 || rgba2)
    return FALSE;

  return TRUE;
}

static gboolean
underline_equal (const GtkTextAppearance *appearance1,
                 const GtkTextAppearance *appearance2)
{
  return (appearance1->underline == appearance2->underline) &&
          rgba_equal ((const GdkRGBA *)appearance1->underline_rgba,
                      (const GdkRGBA *)appearance2->underline_rgba);
}

static gboolean
overline_equal (const GtkTextAppearance *appearance1,
                const GtkTextAppearance *appearance2)
{
  return (appearance1->overline == appearance2->overline) &&
          rgba_equal ((const GdkRGBA *)appearance1->overline_rgba,
                      (const GdkRGBA *)appearance2->overline_rgba);
}

static gboolean
strikethrough_equal (const GtkTextAppearance *appearance1,
                     const GtkTextAppearance *appearance2)
{
  return (appearance1->strikethrough == appearance2->strikethrough) &&
          rgba_equal ((const GdkRGBA *)appearance1->strikethrough_rgba,
                      (const GdkRGBA *)appearance2->strikethrough_rgba);
}

static gboolean
gtk_text_attr_appearance_compare (const PangoAttribute *attr1,
                                  const PangoAttribute *attr2)
{
  const GtkTextAppearance *appearance1 = &((const GtkTextAttrAppearance *)attr1)->appearance;
  const GtkTextAppearance *appearance2 = &((const GtkTextAttrAppearance *)attr2)->appearance;

  return rgba_equal (appearance1->fg_rgba, appearance2->fg_rgba) &&
         rgba_equal (appearance1->bg_rgba, appearance2->bg_rgba) &&
         appearance1->draw_bg == appearance2->draw_bg &&
         strikethrough_equal (appearance1, appearance2) &&
         underline_equal (appearance1, appearance2) &&
         overline_equal (appearance1, appearance2);
}

/*
 * gtk_text_attr_appearance_new:
 * @desc:
 *
 * Create a new font description attribute. (This attribute
 * allows setting family, style, weight, variant, stretch,
 * and size simultaneously.)
 *
 * Returns:
 */
static PangoAttribute *
gtk_text_attr_appearance_new (const GtkTextAppearance *appearance)
{
  static PangoAttrClass klass = {
    0,
    gtk_text_attr_appearance_copy,
    gtk_text_attr_appearance_destroy,
    gtk_text_attr_appearance_compare
  };

  GtkTextAttrAppearance *result;

  if (!klass.type)
    klass.type = gtk_text_attr_appearance_type =
      pango_attr_type_register (I_("GtkTextAttrAppearance"));

  result = g_new (GtkTextAttrAppearance, 1);
  result->attr.klass = &klass;

  result->appearance = *appearance;

  if (appearance->fg_rgba)
    result->appearance.fg_rgba = gdk_rgba_copy (appearance->fg_rgba);

  if (appearance->bg_rgba)
    result->appearance.bg_rgba = gdk_rgba_copy (appearance->bg_rgba);

  if (appearance->underline_rgba)
    result->appearance.underline_rgba = gdk_rgba_copy (appearance->underline_rgba);

  if (appearance->overline_rgba)
    result->appearance.overline_rgba = gdk_rgba_copy (appearance->overline_rgba);

  if (appearance->strikethrough_rgba)
    result->appearance.strikethrough_rgba = gdk_rgba_copy (appearance->strikethrough_rgba);

  return (PangoAttribute *)result;
}

static void
add_generic_attrs (GtkTextLayout      *layout,
                   GtkTextAppearance  *appearance,
                   int                 byte_count,
                   PangoAttrList      *attrs,
                   int                 start,
                   gboolean            size_only,
                   gboolean            is_text)
{
  PangoAttribute *attr;

  if (appearance->underline != PANGO_UNDERLINE_NONE)
    {
      attr = pango_attr_underline_new (appearance->underline);

      attr->start_index = start;
      attr->end_index = start + byte_count;

      pango_attr_list_insert (attrs, attr);
    }

  if (appearance->underline_rgba)
    {
      attr = pango_attr_underline_color_new (appearance->underline_rgba->red * 65535,
                                             appearance->underline_rgba->green * 65535,
                                             appearance->underline_rgba->blue * 65535);

      attr->start_index = start;
      attr->end_index = start + byte_count;

      pango_attr_list_insert (attrs, attr);
    }

  if (appearance->overline != PANGO_OVERLINE_NONE)
    {
      attr = pango_attr_overline_new (appearance->overline);

      attr->start_index = start;
      attr->end_index = start + byte_count;

      pango_attr_list_insert (attrs, attr);
    }

  if (appearance->overline_rgba)
    {
      attr = pango_attr_overline_color_new (appearance->overline_rgba->red * 65535,
                                            appearance->overline_rgba->green * 65535,
                                            appearance->overline_rgba->blue * 65535);

      attr->start_index = start;
      attr->end_index = start + byte_count;

      pango_attr_list_insert (attrs, attr);
    }

  if (appearance->strikethrough)
    {
      attr = pango_attr_strikethrough_new (appearance->strikethrough);

      attr->start_index = start;
      attr->end_index = start + byte_count;

      pango_attr_list_insert (attrs, attr);
    }

  if (appearance->strikethrough_rgba)
    {
      attr = pango_attr_strikethrough_color_new (appearance->strikethrough_rgba->red * 65535,
                                                 appearance->strikethrough_rgba->green * 65535,
                                                 appearance->strikethrough_rgba->blue * 65535);

      attr->start_index = start;
      attr->end_index = start + byte_count;

      pango_attr_list_insert (attrs, attr);
    }

  if (appearance->rise != 0)
    {
      attr = pango_attr_rise_new (appearance->rise);

      attr->start_index = start;
      attr->end_index = start + byte_count;

      pango_attr_list_insert (attrs, attr);
    }

  if (!size_only)
    {
      attr = gtk_text_attr_appearance_new (appearance);

      attr->start_index = start;
      attr->end_index = start + byte_count;

      ((GtkTextAttrAppearance *)attr)->appearance.is_text = is_text;

      pango_attr_list_insert (attrs, attr);
    }
}

static void
add_text_attrs (GtkTextLayout      *layout,
                GtkTextAttributes  *style,
                int                 byte_count,
                PangoAttrList      *attrs,
                int                 start,
                gboolean            size_only,
                PangoAttribute    **last_font_attr,
                PangoAttribute    **last_scale_attr,
                PangoAttribute    **last_fallback_attr)
{
  PangoAttribute *attr;

  if (*last_font_attr &&
      pango_font_description_equal (style->font, ((PangoAttrFontDesc*)*last_font_attr)->desc) &&
      (*last_font_attr)->end_index >= start)
    {
      (*last_font_attr)->start_index = MIN ((*last_font_attr)->start_index, start);
      (*last_font_attr)->end_index = MAX ((*last_font_attr)->end_index, start + byte_count);
    }
  else
    {
      attr = pango_attr_font_desc_new (style->font);
      attr->start_index = start;
      attr->end_index = start + byte_count;

      pango_attr_list_insert (attrs, attr);
      *last_font_attr = attr;
    }

  if (*last_scale_attr &&
      style->font_scale == ((PangoAttrFloat*)*last_scale_attr)->value &&
      (*last_scale_attr)->end_index >= start)
    {
      (*last_scale_attr)->start_index = MIN ((*last_scale_attr)->start_index, start);
      (*last_scale_attr)->end_index = MAX ((*last_scale_attr)->end_index, start + byte_count);
    }
  else if (style->font_scale != 1.0)
    {
      attr = pango_attr_scale_new (style->font_scale);
      attr->start_index = start;
      attr->end_index = start + byte_count;

      pango_attr_list_insert (attrs, attr);
      *last_scale_attr = attr;
    }

  if (*last_fallback_attr &&
      (!style->no_fallback) == ((PangoAttrInt*)*last_fallback_attr)->value &&
      (*last_fallback_attr)->end_index >= start)
    {
      (*last_fallback_attr)->start_index = MIN ((*last_fallback_attr)->start_index, start);
      (*last_fallback_attr)->end_index = MAX ((*last_fallback_attr)->end_index, start + byte_count);
    }
  else if (style->no_fallback)
    {
      attr = pango_attr_fallback_new (!style->no_fallback);
      attr->start_index = start;
      attr->end_index = start + byte_count;

      pango_attr_list_insert (attrs, attr);
      *last_fallback_attr = attr;
    }

  if (style->letter_spacing != 0)
    {
      attr = pango_attr_letter_spacing_new (style->letter_spacing);
      attr->start_index = start;
      attr->end_index = start + byte_count;

      pango_attr_list_insert (attrs, attr);
    }

  if (style->line_height != 0.0)
    {
      if (style->line_height_is_absolute)
        attr = pango_attr_line_height_new_absolute (style->line_height * PANGO_SCALE);
      else
        attr = pango_attr_line_height_new (style->line_height);
      attr->start_index = start;
      attr->end_index = start + byte_count;

      pango_attr_list_insert (attrs, attr);
    }

  if (style->font_features)
    {
      attr = pango_attr_font_features_new (style->font_features);
      attr->start_index = start;
      attr->end_index = start + byte_count;

      pango_attr_list_insert (attrs, attr);
    }

  if (style->no_breaks)
    {
      attr = pango_attr_allow_breaks_new (FALSE);
      attr->start_index = start;
      attr->end_index = start + byte_count;

      pango_attr_list_insert (attrs, attr);
    }

  if (style->show_spaces != PANGO_SHOW_NONE)
    {
      attr = pango_attr_show_new (style->show_spaces);
      attr->start_index = start;
      attr->end_index = start + byte_count;

      pango_attr_list_insert (attrs, attr);
    }

  if (style->no_hyphens)
    {
      attr = pango_attr_insert_hyphens_new (FALSE);
      attr->start_index = start;
      attr->end_index = start + byte_count;

      pango_attr_list_insert (attrs, attr);
    }

  if (style->text_transform != PANGO_TEXT_TRANSFORM_NONE)
    {
      attr = pango_attr_text_transform_new (style->text_transform);
      attr->start_index = start;
      attr->end_index = start + byte_count;

      pango_attr_list_insert (attrs, attr);
    }

  if (style->word)
    {
      attr = pango_attr_word_new ();
      attr->start_index = start;
      attr->end_index = start + byte_count;

      pango_attr_list_insert (attrs, attr);
    }

  if (style->sentence)
    {
      attr = pango_attr_sentence_new ();
      attr->start_index = start;
      attr->end_index = start + byte_count;

      pango_attr_list_insert (attrs, attr);
    }
}

static void
add_paintable_attrs (GtkTextLayout      *layout,
                     GtkTextLineDisplay *display,
                     GtkTextAttributes  *style,
                     GtkTextLineSegment *seg,
                     PangoAttrList      *attrs,
                     int                 start)
{
  PangoAttribute *attr;
  PangoRectangle logical_rect;
  GtkTextPaintable *paintable = &seg->body.paintable;
  int width, height;

  width = gdk_paintable_get_intrinsic_width (paintable->paintable);
  height = gdk_paintable_get_intrinsic_height (paintable->paintable);

  /* Pick *some* default size */
  if (width == 0)
    width = 32;
  if (height == 0)
    {
      double aspect = gdk_paintable_get_intrinsic_aspect_ratio (paintable->paintable);
      if (aspect == 0)
        aspect = 1.0;
      height = width / aspect;
    }

  logical_rect.x = 0;
  logical_rect.y = -height * PANGO_SCALE;
  logical_rect.width = width * PANGO_SCALE;
  logical_rect.height = height * PANGO_SCALE;

  attr = pango_attr_shape_new_with_data (&logical_rect, &logical_rect,
					 paintable->paintable, NULL, NULL);
  attr->start_index = start;
  attr->end_index = start + seg->byte_count;
  pango_attr_list_insert (attrs, attr);
}

static void
add_child_attrs (GtkTextLayout      *layout,
                 GtkTextLineDisplay *display,
                 GtkTextAttributes  *style,
                 GtkTextLineSegment *seg,
                 PangoAttrList      *attrs,
                 int                 start)
{
  PangoAttribute *attr;
  PangoRectangle logical_rect;
  int width, height;
  GSList *tmp_list;
  GtkWidget *widget = NULL;

  width = 1;
  height = 1;

  tmp_list = seg->body.child.widgets;
  while (tmp_list != NULL)
    {
      GtkWidget *child = tmp_list->data;

      if (_gtk_anchored_child_get_layout (child) == layout)
        {
          /* Found it */
          GtkRequisition req;

          gtk_widget_get_preferred_size (child, &req, NULL);

          width = req.width;
          height = req.height;

	  widget = child;

          break;
        }

      tmp_list = tmp_list->next;
    }

  if (tmp_list == NULL)
    {
      /* If tmp_list == NULL then there is no widget at this anchor in
       * this display; not an error. We make up an arbitrary size
       * to use, just so the programmer can see the blank spot.
       * We also put a NULL in the shaped objects list, to keep
       * the correspondence between the list and the shaped chars in
       * the layout. A bad hack, yes.
       */

      width = 30;
      height = 20;

      widget = NULL;
    }

  logical_rect.x = 0;
  logical_rect.y = -height * PANGO_SCALE;
  logical_rect.width = width * PANGO_SCALE;
  logical_rect.height = height * PANGO_SCALE;

  attr = pango_attr_shape_new_with_data (&logical_rect, &logical_rect,
					 widget, NULL, NULL);
  attr->start_index = start;
  attr->end_index = start + seg->byte_count;
  pango_attr_list_insert (attrs, attr);
}

/*
 * get_block_cursor:
 * @layout: a `GtkTextLayout`
 * @display: a `GtkTextLineDisplay`
 * @insert_iter: iter pointing to the cursor location
 * @insert_index: cursor offset in the @display’s layout, it may
 * be different from @insert_iter’s offset in case when preedit
 * string is present.
 * @pos: location to store cursor position
 * @cursor_at_line_end: whether cursor is at the end of line
 *
 * Checks whether layout should display block cursor at given position.
 * For this layout must be in overwrite mode and text at @insert_iter
 * must be editable.
 */
static gboolean
get_block_cursor (GtkTextLayout      *layout,
		  GtkTextLineDisplay *display,
		  const GtkTextIter  *insert_iter,
		  int                 insert_index,
		  GdkRectangle       *pos,
		  gboolean           *cursor_at_line_end)
{
  PangoRectangle pango_pos;

  if (layout->overwrite_mode &&
      gtk_text_iter_editable (insert_iter, TRUE) &&
      _gtk_text_util_get_block_cursor_location (display->layout,
						insert_index,
						&pango_pos,
    					        cursor_at_line_end))
    {
      if (pos)
	{
	  pos->x = PANGO_PIXELS (pango_pos.x);
	  pos->y = PANGO_PIXELS (pango_pos.y);
	  pos->width = PANGO_PIXELS (pango_pos.width);
	  pos->height = PANGO_PIXELS (pango_pos.height);
	}

      return TRUE;
    }
  else
    return FALSE;
}

static void
add_cursor (GtkTextLayout      *layout,
            GtkTextLineDisplay *display,
            GtkTextLineSegment *seg,
            int                 start)
{
  CursorPosition cursor;
  GtkTextBTree *btree;

  btree = _gtk_text_buffer_get_btree (layout->buffer);

  cursor.pos = start;
  cursor.is_insert = _gtk_text_btree_mark_is_insert (btree, seg->body.mark.obj);
  cursor.is_selection_bound = _gtk_text_btree_mark_is_selection_bound (btree, seg->body.mark.obj);

  /* Hide insertion cursor when we have a selection or the layout
   * user has hidden the cursor.
   */
  if (cursor.is_insert &&
      (!layout->cursor_visible || gtk_text_buffer_get_selection_bounds (layout->buffer, NULL, NULL)))
    return;

  if (layout->overwrite_mode && cursor.is_insert)
    {
      GtkTextIter iter;
      gboolean cursor_at_line_end;

      _gtk_text_btree_get_iter_at_mark (btree, &iter, seg->body.mark.obj);

      if (get_block_cursor (layout, display, &iter, start,
			    &display->block_cursor,
			    &cursor_at_line_end))
	{
	  display->has_block_cursor = TRUE;
	  display->cursor_at_line_end = cursor_at_line_end;
	  return;
	}
    }

  if (!display->cursors)
    display->cursors = g_array_new (FALSE, FALSE, sizeof(CursorPosition));

  display->cursors = g_array_append_val (display->cursors, cursor);
}

static gboolean
is_shape (PangoLayoutRun *run)
{
  GSList *tmp_list = run->item->analysis.extra_attrs;

  while (tmp_list)
    {
      PangoAttribute *attr = tmp_list->data;

      if (attr->klass->type == PANGO_ATTR_SHAPE)
        return TRUE;

      tmp_list = tmp_list->next;
    }

  return FALSE;
}

static void
allocate_child_widgets (GtkTextLayout      *text_layout,
                        GtkTextLineDisplay *display)
{
  PangoLayout *layout = display->layout;
  PangoLayoutIter *run_iter;

  run_iter = pango_layout_get_iter (layout);
  do
    {
      PangoLayoutRun *run = pango_layout_iter_get_run_readonly (run_iter);

      if (run && is_shape (run))
        {
          int byte_index;
          GtkTextIter text_iter;
          GtkTextChildAnchor *anchor = NULL;
          GtkWidget **widgets = NULL;
          guint n_widgets = 0;
          guint i;

          /* The pango iterator iterates in visual order.
           * We use the byte index to find the child widget.
           */
          byte_index = pango_layout_iter_get_index (run_iter);
          line_display_index_to_iter (text_layout, display, &text_iter, byte_index, 0);
          anchor = gtk_text_iter_get_child_anchor (&text_iter);
          if (anchor)
            widgets = gtk_text_child_anchor_get_widgets (anchor, &n_widgets);

          for (i = 0; i < n_widgets; i++)
            {
              GtkWidget  *child = widgets[i];
              PangoRectangle extents;

              if (_gtk_anchored_child_get_layout (child) == text_layout)
                {

                  /* We emit "allocate_child" with the x,y of
                   * the widget with respect to the top of the line
                   * and the left side of the buffer
                   */
                  pango_layout_iter_get_run_extents (run_iter,
                                                     NULL,
                                                     &extents);

                  g_signal_emit (text_layout,
                                 signals[ALLOCATE_CHILD],
                                 0,
                                 child,
                                 PANGO_PIXELS (extents.x) + display->x_offset,
                                 PANGO_PIXELS (extents.y) + display->top_margin);
                }
            }

          g_free (widgets);
        }
    }
  while (pango_layout_iter_next_run (run_iter));

  pango_layout_iter_free (run_iter);
}

void
gtk_text_layout_update_children (GtkTextLayout      *text_layout,
                                 GtkTextLineDisplay *display)
{
  allocate_child_widgets (text_layout, display);
}

static void
convert_color (GdkRGBA        *result,
	       PangoAttrColor *attr)
{
  result->red = attr->color.red / 65535.;
  result->blue = attr->color.blue / 65535.;
  result->green = attr->color.green / 65535.;
  result->alpha = 1;
}

/* This function is used to convert the preedit string attributes, which are
 * standard PangoAttributes, into the custom attributes used by the text
 * widget and insert them into an attr list with a given offset.
 */
static void
add_preedit_attrs (GtkTextLayout     *layout,
		   GtkTextAttributes *style,
		   PangoAttrList     *attrs,
		   int                offset,
		   gboolean           size_only)
{
  PangoAttrIterator *iter = pango_attr_list_get_iterator (layout->preedit_attrs);

  do
    {
      GtkTextAppearance appearance = style->appearance;
      PangoFontDescription *font_desc = pango_font_description_copy_static (style->font);
      PangoAttribute *insert_attr;
      GSList *extra_attrs = NULL;
      GSList *tmp_list;
      PangoLanguage *language;
      int start, end;

      pango_attr_iterator_range (iter, &start, &end);

      if (end == G_MAXINT)
	end = layout->preedit_len;

      if (end == start)
	continue;

      pango_attr_iterator_get_font (iter, font_desc, &language, &extra_attrs);

      if (appearance.fg_rgba)
	appearance.fg_rgba = gdk_rgba_copy (appearance.fg_rgba);
      if (appearance.bg_rgba)
	appearance.bg_rgba = gdk_rgba_copy (appearance.bg_rgba);
      if (appearance.underline_rgba)
	appearance.underline_rgba = gdk_rgba_copy (appearance.underline_rgba);
      if (appearance.overline_rgba)
	appearance.overline_rgba = gdk_rgba_copy (appearance.overline_rgba);
      if (appearance.strikethrough_rgba)
	appearance.strikethrough_rgba = gdk_rgba_copy (appearance.strikethrough_rgba);

      tmp_list = extra_attrs;
      while (tmp_list)
	{
	  PangoAttribute *attr = tmp_list->data;
	  GdkRGBA rgba;

	  switch ((guint) attr->klass->type)
	    {
	    case PANGO_ATTR_FOREGROUND:
	      convert_color (&rgba, (PangoAttrColor *)attr);
	      if (appearance.fg_rgba)
		gdk_rgba_free (appearance.fg_rgba);
	      appearance.fg_rgba = gdk_rgba_copy (&rgba);
	      break;
	    case PANGO_ATTR_BACKGROUND:
	      convert_color (&rgba, (PangoAttrColor *)attr);
	      if (appearance.bg_rgba)
		gdk_rgba_free (appearance.bg_rgba);
	      appearance.bg_rgba = gdk_rgba_copy (&rgba);
	      appearance.draw_bg = TRUE;
	      break;
	    case PANGO_ATTR_UNDERLINE:
	      appearance.underline = ((PangoAttrInt *)attr)->value;
	      break;
            case PANGO_ATTR_UNDERLINE_COLOR:
              convert_color (&rgba, (PangoAttrColor*)attr);
	      if (appearance.underline_rgba)
		gdk_rgba_free (appearance.underline_rgba);
	      appearance.underline_rgba = gdk_rgba_copy (&rgba);
	      break;
	    case PANGO_ATTR_OVERLINE:
	      appearance.overline = ((PangoAttrInt *)attr)->value;
	      break;
            case PANGO_ATTR_OVERLINE_COLOR:
              convert_color (&rgba, (PangoAttrColor*)attr);
	      if (appearance.overline_rgba)
		gdk_rgba_free (appearance.overline_rgba);
	      appearance.overline_rgba = gdk_rgba_copy (&rgba);
	      break;
	    case PANGO_ATTR_STRIKETHROUGH:
	      appearance.strikethrough = ((PangoAttrInt *)attr)->value;
	      break;
            case PANGO_ATTR_STRIKETHROUGH_COLOR:
              convert_color (&rgba, (PangoAttrColor*)attr);
	      if (appearance.strikethrough_rgba)
		gdk_rgba_free (appearance.strikethrough_rgba);
	      appearance.strikethrough_rgba = gdk_rgba_copy (&rgba);
	      break;
            case PANGO_ATTR_RISE:
              appearance.rise = ((PangoAttrInt *)attr)->value;
              break;
	    default:
	      break;
	    }

	  pango_attribute_destroy (attr);
	  tmp_list = tmp_list->next;
	}

      g_slist_free (extra_attrs);

      insert_attr = pango_attr_font_desc_new (font_desc);
      insert_attr->start_index = start + offset;
      insert_attr->end_index = end + offset;

      pango_attr_list_insert (attrs, insert_attr);

      if (language)
	{
	  insert_attr = pango_attr_language_new (language);
	  insert_attr->start_index = start + offset;
	  insert_attr->end_index = end + offset;

	  pango_attr_list_insert (attrs, insert_attr);
	}

      add_generic_attrs (layout, &appearance, end - start,
                         attrs, start + offset,
                         size_only, TRUE);

      if (appearance.fg_rgba)
	gdk_rgba_free (appearance.fg_rgba);
      if (appearance.bg_rgba)
	gdk_rgba_free (appearance.bg_rgba);
      if (appearance.underline_rgba)
	gdk_rgba_free (appearance.underline_rgba);
      if (appearance.overline_rgba)
	gdk_rgba_free (appearance.overline_rgba);
      if (appearance.strikethrough_rgba)
	gdk_rgba_free (appearance.strikethrough_rgba);

      pango_font_description_free (font_desc);
    }
  while (pango_attr_iterator_next (iter));

  pango_attr_iterator_destroy (iter);
}

/* Iterate over the line and fill in display->cursors.
 * It’s a stripped copy of gtk_text_layout_get_line_display() */
void
gtk_text_layout_update_display_cursors (GtkTextLayout      *layout,
                                        GtkTextLine        *line,
                                        GtkTextLineDisplay *display)
{
  GtkTextLineSegment *seg;
  GtkTextIter iter;
  int layout_byte_offset, buffer_byte_offset;
  GSList *cursor_byte_offsets = NULL;
  GSList *cursor_segs = NULL;
  GSList *tmp_list1, *tmp_list2;

  if (!display->cursors_invalid)
    return;

  display->cursors_invalid = FALSE;

  /* Special-case optimization for completely
   * invisible lines; makes it faster to deal
   * with sequences of invisible lines.
   */
  if (totally_invisible_line (layout, line, &iter))
    return;

  /* Iterate over segments */
  layout_byte_offset = 0; /* position in the layout text (includes preedit, does not include invisible text) */
  buffer_byte_offset = 0; /* position in the buffer line */
  seg = _gtk_text_iter_get_any_segment (&iter);
  while (seg != NULL)
    {
      /* Displayable segments */
      if (seg->type == &gtk_text_char_type ||
          seg->type == &gtk_text_paintable_type ||
          seg->type == &gtk_text_child_type)
        {
          gtk_text_layout_get_iter_at_line (layout, &iter, line,
                                            buffer_byte_offset);

          if (!_gtk_text_btree_char_is_invisible (&iter))
            layout_byte_offset += seg->byte_count;

	  buffer_byte_offset += seg->byte_count;
        }

      /* Marks */
      else if (seg->type == &gtk_text_right_mark_type ||
               seg->type == &gtk_text_left_mark_type)
        {
	  int cursor_offset = 0;

	  /* At the insertion point, add the preedit string, if any */

	  if (_gtk_text_btree_mark_is_insert (_gtk_text_buffer_get_btree (layout->buffer),
					      seg->body.mark.obj))
	    {
	      display->insert_index = layout_byte_offset;

	      if (layout->preedit_len > 0)
		{
		  layout_byte_offset += layout->preedit_len;
                  /* DO NOT increment the buffer byte offset for preedit */
		  cursor_offset = layout->preedit_cursor - layout->preedit_len;
		}
	    }

          /* Display visible marks */

          if (seg->body.mark.visible)
            {
              cursor_byte_offsets = g_slist_prepend (cursor_byte_offsets,
                                                     GINT_TO_POINTER (layout_byte_offset + cursor_offset));
              cursor_segs = g_slist_prepend (cursor_segs, seg);
            }
        }

      /* Toggles */
      else if (seg->type == &gtk_text_toggle_on_type ||
               seg->type == &gtk_text_toggle_off_type)
        {
        }

      else
        g_error ("Unknown segment type: %s", seg->type->name);

      seg = seg->next;
    }

  tmp_list1 = cursor_byte_offsets;
  tmp_list2 = cursor_segs;
  while (tmp_list1)
    {
      add_cursor (layout, display, tmp_list2->data,
                  GPOINTER_TO_INT (tmp_list1->data));
      tmp_list1 = tmp_list1->next;
      tmp_list2 = tmp_list2->next;
    }
  g_slist_free (cursor_byte_offsets);
  g_slist_free (cursor_segs);
}

/* Add the tag to the array if it's not there already, and remove
 * it otherwise. It keeps the array sorted by tags priority. */
static GPtrArray *
tags_array_toggle_tag (GPtrArray  *array,
		       GtkTextTag *tag)
{
  int pos;
  GtkTextTag **tags;

  if (array == NULL)
    array = g_ptr_array_new ();

  tags = (GtkTextTag**) array->pdata;

  for (pos = 0; pos < array->len && tags[pos]->priv->priority < tag->priv->priority; pos++) ;

  if (pos < array->len && tags[pos] == tag)
    g_ptr_array_remove_index (array, pos);
  else
    {
      g_ptr_array_set_size (array, array->len + 1);
      if (pos < array->len - 1)
	memmove (array->pdata + pos + 1, array->pdata + pos,
		 (array->len - pos - 1) * sizeof (GtkTextTag*));
      array->pdata[pos] = tag;
    }

  return array;
}

GtkTextLineDisplay *
gtk_text_layout_create_display (GtkTextLayout *layout,
                                GtkTextLine   *line,
                                gboolean       size_only)
{
  GtkTextLayoutPrivate *priv = GTK_TEXT_LAYOUT_GET_PRIVATE (layout);
  GtkTextLineDisplay *display;
  GtkTextLineSegment *seg;
  GtkTextIter iter;
  GtkTextAttributes *style;
  char *text;
  int text_pixel_width;
  PangoAttrList *attrs;
  int text_allocated, layout_byte_offset;
  PangoRectangle extents;
  gboolean para_values_set = FALSE;
  GSList *cursor_byte_offsets = NULL;
  GSList *cursor_segs = NULL;
  GSList *tmp_list1, *tmp_list2;
  gboolean saw_widget = FALSE;
  PangoDirection base_dir;
  GPtrArray *tags;
  gboolean initial_toggle_segments;
  int h_margin;
  int h_padding;
  PangoAttribute *last_font_attr = NULL;
  PangoAttribute *last_scale_attr = NULL;
  PangoAttribute *last_fallback_attr = NULL;
  GtkTextBTree *btree;

  g_return_val_if_fail (line != NULL, NULL);

  display = g_rc_box_new0 (GtkTextLineDisplay);

  display->mru_link.data = display;
  display->size_only = !!size_only;
  display->line = line;
  display->insert_index = -1;

  /* Special-case optimization for completely
   * invisible lines; makes it faster to deal
   * with sequences of invisible lines.
   */
  if (totally_invisible_line (layout, line, &iter))
    {
      display->layout = pango_layout_new (layout->ltr_context);
      return g_steal_pointer (&display);
    }

  /* Find the bidi base direction */
  base_dir = line->dir_propagated_forward;
  if (base_dir == PANGO_DIRECTION_NEUTRAL)
    base_dir = line->dir_propagated_back;

  if (line == priv->cursor_line &&
      line->dir_strong == PANGO_DIRECTION_NEUTRAL)
    {
      base_dir = (layout->keyboard_direction == GTK_TEXT_DIR_LTR) ?
         PANGO_DIRECTION_LTR : PANGO_DIRECTION_RTL;
    }

  btree = _gtk_text_buffer_get_btree (layout->buffer);

  /* Allocate space for flat text for buffer
   */
  text_allocated = _gtk_text_line_byte_count (line);
  text = g_malloc (text_allocated);

  attrs = pango_attr_list_new ();

  /* Iterate over segments, creating display chunks for them, and updating the tags array. */
  layout_byte_offset = 0; /* current length of layout text (includes preedit, does not include invisible text) */
  seg = _gtk_text_iter_get_any_segment (&iter);
  tags = _gtk_text_btree_get_tags (&iter);
  initial_toggle_segments = TRUE;
  while (seg != NULL)
    {
      /* Displayable segments */
      if (seg->type == &gtk_text_char_type ||
          seg->type == &gtk_text_paintable_type ||
          seg->type == &gtk_text_child_type)
        {
          style = get_style (layout, tags);
          initial_toggle_segments = FALSE;

          /* We have to delay setting the paragraph values until we
           * hit the first paintable or text segment because toggles at
           * the beginning of the paragraph should affect the
           * paragraph-global values
           */
          if (!para_values_set)
            {
              set_para_values (layout, base_dir, style, display);
              para_values_set = TRUE;
            }

          /* First see if the chunk is invisible, and ignore it if so. Tk
           * looked at tabs, wrap mode, etc. before doing this, but
           * that made no sense to me, so I am just skipping the
           * invisible chunks
           */
          if (!style->invisible)
            {
              if (seg->type == &gtk_text_char_type)
                {
                  /* We don't want to split segments because of marks,
                   * so we scan forward for more segments only
                   * separated from us by marks. In theory, we should
                   * also merge segments with identical styles, even
                   * if there are toggles in-between
                   */

                  int bytes = 0;
                  GtkTextLineSegment *prev_seg = NULL;

                  while (seg)
                    {
                      if (seg->type == &gtk_text_char_type)
                        {
                          memcpy (text + layout_byte_offset, seg->body.chars, seg->byte_count);
                          layout_byte_offset += seg->byte_count;
                          bytes += seg->byte_count;
                        }
                      else if (seg->type == &gtk_text_right_mark_type ||
                               seg->type == &gtk_text_left_mark_type)
                        {
                          /* If we have preedit string, break out of this loop - we'll almost
                           * certainly have different attributes on the preedit string
                           */

                          if (layout->preedit_len > 0 &&
                              _gtk_text_btree_mark_is_insert (btree, seg->body.mark.obj))
                            break;

                          if (seg->body.mark.visible)
                            {
                              cursor_byte_offsets = g_slist_prepend (cursor_byte_offsets, GINT_TO_POINTER (layout_byte_offset));
                              cursor_segs = g_slist_prepend (cursor_segs, seg);
                              if (_gtk_text_btree_mark_is_insert (btree, seg->body.mark.obj))
                                display->insert_index = layout_byte_offset;
                            }
                        }
                      else
                        break;

                      prev_seg = seg;
                      seg = seg->next;
                    }

                  seg = prev_seg; /* Back up one */
                  add_generic_attrs (layout, &style->appearance,
                                     bytes,
                                     attrs, layout_byte_offset - bytes,
                                     size_only, TRUE);
                  add_text_attrs (layout, style, bytes, attrs,
                                  layout_byte_offset - bytes, size_only,
                                  &last_font_attr,
                                  &last_scale_attr,
                                  &last_fallback_attr);
                }
              else if (seg->type == &gtk_text_paintable_type)
                {
                  add_generic_attrs (layout,
                                     &style->appearance,
                                     seg->byte_count,
                                     attrs, layout_byte_offset,
                                     size_only, FALSE);
                  add_paintable_attrs (layout, display, style,
                                       seg, attrs, layout_byte_offset);
                  memcpy (text + layout_byte_offset, _gtk_text_unknown_char_utf8,
                          seg->byte_count);
                  layout_byte_offset += seg->byte_count;
                }
              else if (seg->type == &gtk_text_child_type)
                {
                  saw_widget = TRUE;

                  add_generic_attrs (layout, &style->appearance,
                                     seg->byte_count,
                                     attrs, layout_byte_offset,
                                     size_only, FALSE);
                  add_child_attrs (layout, display, style,
                                   seg, attrs, layout_byte_offset);
                  memcpy (text + layout_byte_offset, gtk_text_child_anchor_get_replacement (seg->body.child.obj),
                          seg->byte_count);
                  layout_byte_offset += seg->byte_count;
                }
              else
                {
                  /* We don't know this segment type */
                  g_assert_not_reached ();
                }

            } /* if (segment was visible) */

          release_style (layout, style);
        }

      /* Toggles */
      else if (seg->type == &gtk_text_toggle_on_type ||
               seg->type == &gtk_text_toggle_off_type)
        {
          /* Style may have changed, drop our
             current cached style */
          invalidate_cached_style (layout);
          /* Add the tag only after we have seen some non-toggle non-mark segment,
           * otherwise the tag is already accounted for by _gtk_text_btree_get_tags(). */
          if (!initial_toggle_segments)
            tags = tags_array_toggle_tag (tags, seg->body.toggle.info->tag);
        }

      /* Marks */
      else if (seg->type == &gtk_text_right_mark_type ||
               seg->type == &gtk_text_left_mark_type)
        {
          int cursor_offset = 0;

          /* At the insertion point, add the preedit string, if any */

          if (_gtk_text_btree_mark_is_insert (btree, seg->body.mark.obj))
            {
              display->insert_index = layout_byte_offset;

              if (layout->preedit_len > 0)
                {
                  text_allocated += layout->preedit_len;
                  text = g_realloc (text, text_allocated);

                  style = get_style (layout, tags);
                  add_preedit_attrs (layout, style, attrs, layout_byte_offset, size_only);
                  release_style (layout, style);

                  memcpy (text + layout_byte_offset, layout->preedit_string, layout->preedit_len);
                  layout_byte_offset += layout->preedit_len;
                  /* DO NOT increment the buffer byte offset for preedit */

                  cursor_offset = layout->preedit_cursor - layout->preedit_len;
                }
            }


          /* Display visible marks */

          if (seg->body.mark.visible)
            {
              cursor_byte_offsets = g_slist_prepend (cursor_byte_offsets,
                                                     GINT_TO_POINTER (layout_byte_offset + cursor_offset));
              cursor_segs = g_slist_prepend (cursor_segs, seg);
            }
        }

      else
        g_error ("Unknown segment type: %s", seg->type->name);

      seg = seg->next;
    }

  if (!para_values_set)
    {
      style = get_style (layout, tags);
      set_para_values (layout, base_dir, style, display);
      release_style (layout, style);
    }

  /* Pango doesn't want the trailing paragraph delimiters */

  {
    /* Only one character has type G_UNICODE_PARAGRAPH_SEPARATOR in
     * Unicode 3.0; update this if that changes.
     */
#define PARAGRAPH_SEPARATOR 0x2029
    gunichar ch = 0;

    if (layout_byte_offset > 0)
      {
        const char *prev = g_utf8_prev_char (text + layout_byte_offset);
        ch = g_utf8_get_char (prev);
        if (ch == PARAGRAPH_SEPARATOR || ch == '\r' || ch == '\n')
          layout_byte_offset = prev - text; /* chop off */

        if (ch == '\n' && layout_byte_offset > 0)
          {
            /* Possibly chop a CR as well */
            prev = g_utf8_prev_char (text + layout_byte_offset);
            if (*prev == '\r')
              --layout_byte_offset;
          }
      }
  }

  pango_layout_set_text (display->layout, text, layout_byte_offset);
  pango_layout_set_attributes (display->layout, attrs);

  tmp_list1 = cursor_byte_offsets;
  tmp_list2 = cursor_segs;
  while (tmp_list1)
    {
      add_cursor (layout, display, tmp_list2->data,
                  GPOINTER_TO_INT (tmp_list1->data));
      tmp_list1 = tmp_list1->next;
      tmp_list2 = tmp_list2->next;
    }
  g_slist_free (cursor_byte_offsets);
  g_slist_free (cursor_segs);

  pango_layout_get_extents (display->layout, NULL, &extents);

  text_pixel_width = PIXEL_BOUND (extents.width);

  h_margin = display->left_margin + display->right_margin;
  h_padding = layout->left_padding + layout->right_padding;

  display->width = text_pixel_width + h_margin + h_padding;
  display->height += PANGO_PIXELS (extents.height);

  /* If we aren't wrapping, we need to do the alignment of each
   * paragraph ourselves.
   */
  if (pango_layout_get_width (display->layout) < 0)
    {
      int excess = display->total_width - text_pixel_width;

      switch (pango_layout_get_alignment (display->layout))
        {
        case PANGO_ALIGN_LEFT:
        default:
          break;
        case PANGO_ALIGN_CENTER:
          display->x_offset += excess / 2;
          break;
        case PANGO_ALIGN_RIGHT:
          display->x_offset += excess;
          break;
        }
    }

  /* Free this if we aren't in a loop */
  if (layout->wrap_loop_count == 0)
    invalidate_cached_style (layout);

  g_free (text);
  pango_attr_list_unref (attrs);
  if (tags != NULL)
    g_ptr_array_free (tags, TRUE);

  display->has_children = saw_widget;

  if (saw_widget)
    allocate_child_widgets (layout, display);

  return g_steal_pointer (&display);
}

GtkTextLineDisplay *
gtk_text_layout_get_line_display (GtkTextLayout *layout,
                                  GtkTextLine   *line,
                                  gboolean       size_only)
{
  GtkTextLayoutPrivate *priv = GTK_TEXT_LAYOUT_GET_PRIVATE (layout);

  return gtk_text_line_display_cache_get (priv->cache, layout, line, size_only);
}

static void
gtk_text_line_display_finalize (GtkTextLineDisplay *display)
{
  g_assert (display != NULL);
  g_assert (display->cache_iter == NULL);
  g_assert (display->mru_link.prev == NULL);
  g_assert (display->mru_link.next == NULL);
  g_assert (display->mru_link.data == display);

  g_clear_object (&display->layout);
  g_clear_pointer (&display->cursors, g_array_unref);
  g_clear_pointer (&display->node, gsk_render_node_unref);
}

GtkTextLineDisplay *
gtk_text_line_display_ref (GtkTextLineDisplay *display)
{
  return g_rc_box_acquire (display);
}

void
gtk_text_line_display_unref (GtkTextLineDisplay *display)
{
  g_rc_box_release_full (display, (GDestroyNotify)gtk_text_line_display_finalize);
}

/* Functions to convert iter <=> index for the line of a GtkTextLineDisplay
 * taking into account the preedit string and invisible text if necessary.
 */
static int
line_display_iter_to_index (GtkTextLayout      *layout,
			    GtkTextLineDisplay *display,
			    const GtkTextIter  *iter)
{
  int index;

  g_return_val_if_fail (_gtk_text_iter_get_text_line (iter) == display->line, 0);

  index = gtk_text_iter_get_visible_line_index (iter);

  if (layout->preedit_len > 0 && display->insert_index >= 0)
    {
      if (index >= display->insert_index)
	index += layout->preedit_len;
    }

  return index;
}

static void
line_display_index_to_iter (GtkTextLayout      *layout,
			    GtkTextLineDisplay *display,
			    GtkTextIter        *iter,
			    int                 index,
			    int                 trailing)
{
  g_return_if_fail (!_gtk_text_line_is_last (display->line,
                                             _gtk_text_buffer_get_btree (layout->buffer)));

  if (layout->preedit_len > 0 && display->insert_index >= 0)
    {
      if (index >= display->insert_index + layout->preedit_len)
	index -= layout->preedit_len;
      else if (index > display->insert_index)
	{
	  index = display->insert_index;
	  trailing = 0;
	}
    }

  gtk_text_layout_get_iter_at_line (layout, iter, display->line, 0);

  gtk_text_iter_set_visible_line_index (iter, index);

  if (_gtk_text_iter_get_text_line (iter) != display->line)
    {
      /* Clamp to end of line - really this clamping should have been done
       * before here, maybe in Pango, this is a broken band-aid I think
       */
      gtk_text_layout_get_iter_at_line (layout, iter, display->line, 0);
      if (!gtk_text_iter_ends_line (iter))
        gtk_text_iter_forward_to_line_end (iter);
    }

  gtk_text_iter_forward_chars (iter, trailing);
}

static void
get_line_at_y (GtkTextLayout *layout,
               int            y,
               GtkTextLine  **line,
               int           *line_top)
{
  GtkTextBTree *btree = _gtk_text_buffer_get_btree (layout->buffer);

  if (y < 0)
    y = 0;
  if (y > layout->height)
    y = layout->height;

  *line = _gtk_text_btree_find_line_by_y (btree, layout, y, line_top);
  if (*line == NULL)
    {
      *line = _gtk_text_btree_get_end_iter_line (btree);

      if (line_top)
        *line_top = _gtk_text_btree_find_line_top (btree, *line, layout);
    }
}

/**
 * gtk_text_layout_get_line_at_y:
 * @layout: a `GtkLayout`
 * @target_iter: the iterator in which the result is stored
 * @y: the y position
 * @line_top: location to store the y coordinate of the
 *   top of the line. (Can by %NULL)
 *
 * Get the iter at the beginning of the line which is displayed
 * at the given y.
 */
void
gtk_text_layout_get_line_at_y (GtkTextLayout *layout,
                               GtkTextIter   *target_iter,
                               int            y,
                               int           *line_top)
{
  GtkTextLine *line;

  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (target_iter != NULL);

  get_line_at_y (layout, y, &line, line_top);
  gtk_text_layout_get_iter_at_line (layout, target_iter, line, 0);
}

gboolean
gtk_text_layout_get_iter_at_pixel (GtkTextLayout *layout,
                                   GtkTextIter   *target_iter,
                                   int            x,
                                   int            y)
{
  int trailing;
  gboolean inside;

  inside = gtk_text_layout_get_iter_at_position (layout, target_iter, &trailing, x, y);

  gtk_text_iter_forward_chars (target_iter, trailing);

  return inside;
}

gboolean
gtk_text_layout_get_iter_at_position (GtkTextLayout *layout,
                                      GtkTextIter   *target_iter,
                                      int           *trailing,
                                      int            x,
                                      int            y)
{
  GtkTextLine *line;
  int byte_index;
  int line_top;
  GtkTextLineDisplay *display;
  gboolean inside;

  g_return_val_if_fail (GTK_IS_TEXT_LAYOUT (layout), FALSE);
  g_return_val_if_fail (target_iter != NULL, FALSE);

  get_line_at_y (layout, y, &line, &line_top);

  display = gtk_text_layout_get_line_display (layout, line, FALSE);

  x -= display->x_offset;
  y -= line_top + display->top_margin;

  /* If we are below the layout, position the cursor at the last character
   * of the line.
   */
  if (y > display->height - display->top_margin - display->bottom_margin)
    {
      byte_index = _gtk_text_line_byte_count (line);
      if (trailing)
        *trailing = 0;

      inside = FALSE;
    }
  else
    {
       /* Ignore the "outside" return value from pango. Pango is doing
        * the right thing even if we are outside the layout in the
        * x-direction.
        */
      inside = pango_layout_xy_to_index (display->layout, x * PANGO_SCALE, y * PANGO_SCALE,
                                         &byte_index, trailing);
    }

  line_display_index_to_iter (layout, display, target_iter, byte_index, 0);

  gtk_text_line_display_unref (display);

  return inside;
}


/**
 * gtk_text_layout_get_cursor_locations:
 * @layout: a `GtkTextLayout`
 * @iter: a `GtkTextIter`
 * @strong_pos: (out) (optional): location to store the strong cursor position
 * @weak_pos: (out) (optional): location to store the weak cursor position
 *
 * Given an iterator within a text layout, determine the positions of the
 * strong and weak cursors if the insertion point is at that
 * iterator. The position of each cursor is stored as a zero-width
 * rectangle. The strong cursor location is the location where
 * characters of the directionality equal to the base direction of the
 * paragraph are inserted.  The weak cursor location is the location
 * where characters of the directionality opposite to the base
 * direction of the paragraph are inserted.
 **/
void
gtk_text_layout_get_cursor_locations (GtkTextLayout  *layout,
                                      GtkTextIter    *iter,
                                      GdkRectangle   *strong_pos,
                                      GdkRectangle   *weak_pos)
{
  GtkTextLine *line;
  GtkTextLineDisplay *display;
  int line_top;
  int index;
  GtkTextIter insert_iter;

  PangoRectangle pango_strong_pos;
  PangoRectangle pango_weak_pos;

  g_return_if_fail (layout != NULL);
  g_return_if_fail (iter != NULL);

  line = _gtk_text_iter_get_text_line (iter);
  display = gtk_text_layout_get_line_display (layout, line, FALSE);
  index = line_display_iter_to_index (layout, display, iter);

  line_top = _gtk_text_btree_find_line_top (_gtk_text_buffer_get_btree (layout->buffer),
                                           line, layout);

  gtk_text_buffer_get_iter_at_mark (layout->buffer, &insert_iter,
                                    gtk_text_buffer_get_insert (layout->buffer));

  if (gtk_text_iter_equal (iter, &insert_iter))
    index += layout->preedit_cursor - layout->preedit_len;

  pango_layout_get_cursor_pos (display->layout, index,
			       strong_pos ? &pango_strong_pos : NULL,
			       weak_pos ? &pango_weak_pos : NULL);

  if (strong_pos)
    {
      strong_pos->x = display->x_offset + pango_strong_pos.x / PANGO_SCALE;
      strong_pos->y = line_top + display->top_margin + pango_strong_pos.y / PANGO_SCALE;
      strong_pos->width = 0;
      strong_pos->height = pango_strong_pos.height / PANGO_SCALE;
    }

  if (weak_pos)
    {
      weak_pos->x = display->x_offset + pango_weak_pos.x / PANGO_SCALE;
      weak_pos->y = line_top + display->top_margin + pango_weak_pos.y / PANGO_SCALE;
      weak_pos->width = 0;
      weak_pos->height = pango_weak_pos.height / PANGO_SCALE;
    }

  gtk_text_line_display_unref (display);
}

/**
 * _gtk_text_layout_get_block_cursor:
 * @layout: a `GtkTextLayout`
 * @pos: a `GdkRectangle` to store block cursor position
 *
 * If layout is to display a block cursor, calculates its position
 * and returns %TRUE. Otherwise it returns %FALSE. In case when
 * cursor is visible, it simply returns the position stored in
 * the line display, otherwise it has to compute the position
 * (see get_block_cursor()).
 **/
gboolean
_gtk_text_layout_get_block_cursor (GtkTextLayout *layout,
				   GdkRectangle  *pos)
{
  GtkTextLine *line;
  GtkTextLineDisplay *display;
  GtkTextIter iter;
  GdkRectangle rect;
  gboolean block = FALSE;

  g_return_val_if_fail (layout != NULL, FALSE);

  gtk_text_buffer_get_iter_at_mark (layout->buffer, &iter,
                                    gtk_text_buffer_get_insert (layout->buffer));
  line = _gtk_text_iter_get_text_line (&iter);
  display = gtk_text_layout_get_line_display (layout, line, FALSE);

  if (display->has_block_cursor)
    {
      block = TRUE;
      rect = display->block_cursor;
    }
  else
    {
      int index = display->insert_index;

      if (index < 0)
        index = gtk_text_iter_get_line_index (&iter);

      if (get_block_cursor (layout, display, &iter, index, &rect, NULL))
	block = TRUE;
    }

  if (block && pos)
    {
      int line_top;

      line_top = _gtk_text_btree_find_line_top (_gtk_text_buffer_get_btree (layout->buffer),
						line, layout);

      *pos = rect;
      pos->x += display->x_offset;
      pos->y += line_top + display->top_margin;
    }

  gtk_text_line_display_unref (display);

  return block;
}

/**
 * gtk_text_layout_get_line_yrange:
 * @layout: a `GtkTextLayout`
 * @iter: a `GtkTextIter`
 * @y: (nullable): location to store the top of the paragraph in pixels
 * @height: (nullable): location to store the height of the paragraph in pixels
 *
 * Find the range of y coordinates for the paragraph containing
 * the given iter.
 **/
void
gtk_text_layout_get_line_yrange (GtkTextLayout     *layout,
                                 const GtkTextIter *iter,
                                 int               *y,
                                 int               *height)
{
  GtkTextLine *line;

  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (_gtk_text_iter_get_btree (iter) == _gtk_text_buffer_get_btree (layout->buffer));

  line = _gtk_text_iter_get_text_line (iter);

  if (y)
    *y = _gtk_text_btree_find_line_top (_gtk_text_buffer_get_btree (layout->buffer),
                                       line, layout);
  if (height)
    {
      GtkTextLineData *line_data = _gtk_text_line_get_data (line, layout);
      if (line_data)
        *height = line_data->height;
      else
        *height = 0;
    }
}

void
gtk_text_layout_get_iter_location (GtkTextLayout     *layout,
                                   const GtkTextIter *iter,
                                   GdkRectangle      *rect)
{
  PangoRectangle pango_rect;
  GtkTextLine *line;
  GtkTextBTree *tree;
  GtkTextLineDisplay *display;
  int byte_index;
  int x_offset;

  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (_gtk_text_iter_get_btree (iter) == _gtk_text_buffer_get_btree (layout->buffer));
  g_return_if_fail (rect != NULL);

  tree = _gtk_text_iter_get_btree (iter);
  line = _gtk_text_iter_get_text_line (iter);

  display = gtk_text_layout_get_line_display (layout, line, FALSE);

  rect->y = _gtk_text_btree_find_line_top (tree, line, layout);

  x_offset = display->x_offset * PANGO_SCALE;

  byte_index = gtk_text_iter_get_line_index (iter);

  pango_layout_index_to_pos (display->layout, byte_index, &pango_rect);

  rect->x = PANGO_PIXELS (x_offset + pango_rect.x);
  rect->y += PANGO_PIXELS (pango_rect.y) + display->top_margin;
  rect->width = PANGO_PIXELS (pango_rect.width);
  rect->height = PANGO_PIXELS (pango_rect.height);

  gtk_text_line_display_unref (display);
}

/* FFIXX */

/* Find the iter for the logical beginning of the first display line whose
 * top y is >= y. If none exists, move the iter to the logical beginning
 * of the last line in the buffer.
 */
static void
find_display_line_below (GtkTextLayout *layout,
                         GtkTextIter   *iter,
                         int            y)
{
  GtkTextBTree *btree;
  GtkTextLine *line, *next;
  GtkTextLine *found_line = NULL;
  int line_top;
  int found_byte = 0;

  btree = _gtk_text_buffer_get_btree (layout->buffer);

  line = _gtk_text_btree_find_line_by_y (btree, layout, y, &line_top);
  if (!line)
    {
      line = _gtk_text_btree_get_end_iter_line (btree);

      line_top = _gtk_text_btree_find_line_top (btree, line, layout);
    }

  while (line && !found_line)
    {
      GtkTextLineDisplay *display = gtk_text_layout_get_line_display (layout, line, FALSE);
      PangoLayoutIter *layout_iter;

      layout_iter = pango_layout_get_iter (display->layout);

      line_top += display->top_margin;

      do
        {
          int first_y, last_y;
          PangoLayoutLine *layout_line = pango_layout_iter_get_line_readonly (layout_iter);

          found_byte = pango_layout_line_get_start_index (layout_line);

          if (line_top >= y)
            {
              found_line = line;
              break;
            }

          pango_layout_iter_get_line_yrange (layout_iter, &first_y, &last_y);
          line_top += (last_y - first_y) / PANGO_SCALE;
        }
      while (pango_layout_iter_next_line (layout_iter));

      pango_layout_iter_free (layout_iter);

      line_top += display->bottom_margin;
      gtk_text_line_display_unref (display);

      next = _gtk_text_line_next_excluding_last (line);
      if (!next)
        found_line = line;

      line = next;
    }

  gtk_text_layout_get_iter_at_line (layout, iter, found_line, found_byte);
}

/* Find the iter for the logical beginning of the last display line whose
 * top y is >= y. If none exists, move the iter to the logical beginning
 * of the first line in the buffer.
 */
static void
find_display_line_above (GtkTextLayout *layout,
                         GtkTextIter   *iter,
                         int            y)
{
  GtkTextBTree *btree;
  GtkTextLine *line;
  GtkTextLine *found_line = NULL;
  int line_top;
  int found_byte = 0;

  btree = _gtk_text_buffer_get_btree (layout->buffer);
  line = _gtk_text_btree_find_line_by_y (btree, layout, y, &line_top);
  if (!line)
    {
      line = _gtk_text_btree_get_end_iter_line (btree);

      line_top = _gtk_text_btree_find_line_top (btree, line, layout);
    }

  while (line && !found_line)
    {
      GtkTextLineDisplay *display = gtk_text_layout_get_line_display (layout, line, FALSE);
      PangoRectangle logical_rect;
      PangoLayoutIter *layout_iter;
      int tmp_top;

      layout_iter = pango_layout_get_iter (display->layout);

      line_top -= display->top_margin + display->bottom_margin;
      pango_layout_iter_get_layout_extents (layout_iter, NULL, &logical_rect);
      line_top -= logical_rect.height / PANGO_SCALE;

      tmp_top = line_top + display->top_margin;

      do
        {
          int first_y, last_y;
          PangoLayoutLine *layout_line = pango_layout_iter_get_line_readonly (layout_iter);

          found_byte = pango_layout_line_get_start_index (layout_line);

          pango_layout_iter_get_line_yrange (layout_iter, &first_y, &last_y);

          tmp_top -= (last_y - first_y) / PANGO_SCALE;

          if (tmp_top < y)
            {
              found_line = line;
	      pango_layout_iter_free (layout_iter);
              goto done;
            }
        }
      while (pango_layout_iter_next_line (layout_iter));

      pango_layout_iter_free (layout_iter);

      gtk_text_line_display_unref (display);

      line = _gtk_text_line_previous (line);
    }

 done:

  if (found_line)
    gtk_text_layout_get_iter_at_line (layout, iter, found_line, found_byte);
  else
    gtk_text_buffer_get_iter_at_offset (layout->buffer, iter, 0);
}

/**
 * gtk_text_layout_clamp_iter_to_vrange:
 * @layout: a `GtkTextLayout`
 * @iter:   a `GtkTextIter`
 * @top:    the top of the range
 * @bottom: the bottom the range
 *
 * If the iterator is not fully in the range @top <= y < @bottom,
 * then, if possible, move it the minimum distance so that the
 * iterator in this range.
 *
 * Returns: %TRUE if the iterator was moved, otherwise %FALSE.
 **/
gboolean
gtk_text_layout_clamp_iter_to_vrange (GtkTextLayout *layout,
                                      GtkTextIter   *iter,
                                      int            top,
                                      int            bottom)
{
  GdkRectangle iter_rect;

  gtk_text_layout_get_iter_location (layout, iter, &iter_rect);

  /* If the iter is at least partially above the range, put the iter
   * at the first fully visible line after the range.
   */
  if (iter_rect.y < top)
    {
      find_display_line_below (layout, iter, top);

      return TRUE;
    }
  /* Otherwise, if the iter is at least partially below the screen, put the
   * iter on the last logical position of the last completely visible
   * line on screen
   */
  else if (iter_rect.y + iter_rect.height > bottom)
    {
      find_display_line_above (layout, iter, bottom);

      return TRUE;
    }
  else
    return FALSE;
}

/**
 * gtk_text_layout_move_iter_to_previous_line:
 * @layout: a `GtkLayout`
 * @iter:   a `GtkTextIter`
 *
 * Move the iterator to the beginning of the previous line. The lines
 * of a wrapped paragraph are treated as distinct for this operation.
 **/
gboolean
gtk_text_layout_move_iter_to_previous_line (GtkTextLayout *layout,
                                            GtkTextIter   *iter)
{
  GtkTextLine *line;
  GtkTextLineDisplay *display;
  int line_byte;
  GSList *tmp_list;
  PangoLayoutLine *layout_line;
  GtkTextIter orig;
  gboolean update_byte = FALSE;

  g_return_val_if_fail (GTK_IS_TEXT_LAYOUT (layout), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  orig = *iter;


  line = _gtk_text_iter_get_text_line (iter);
  display = gtk_text_layout_get_line_display (layout, line, FALSE);
  line_byte = line_display_iter_to_index (layout, display, iter);

  /* If display->height == 0 then the line is invisible, so don't
   * move onto it.
   */
  while (display->height == 0)
    {
      GtkTextLine *prev_line;

      prev_line = _gtk_text_line_previous (line);

      if (prev_line == NULL)
        {
          line_display_index_to_iter (layout, display, iter, 0, 0);
          goto out;
        }

      gtk_text_line_display_unref (display);

      line = prev_line;
      display = gtk_text_layout_get_line_display (layout, prev_line, FALSE);
      update_byte = TRUE;
    }

  tmp_list = pango_layout_get_lines_readonly (display->layout);
  layout_line = tmp_list->data;

  if (update_byte)
    {
      line_byte = pango_layout_line_get_start_index (layout_line) + pango_layout_line_get_length (layout_line);
    }

  if (line_byte < pango_layout_line_get_length (layout_line) || !tmp_list->next) /* first line of paragraph */
    {
      GtkTextLine *prev_line;

      prev_line = _gtk_text_line_previous (line);

      /* first line of the whole buffer, do not move the iter and return FALSE */
      if (prev_line == NULL)
        goto out;

      while (prev_line)
        {
          gtk_text_line_display_unref (display);

          display = gtk_text_layout_get_line_display (layout, prev_line, FALSE);

          if (display->height > 0)
            {
              tmp_list = g_slist_last (pango_layout_get_lines_readonly (display->layout));
              layout_line = tmp_list->data;

              line_display_index_to_iter (layout, display, iter,
                                          pango_layout_line_get_start_index (layout_line) + pango_layout_line_get_length (layout_line), 0);
              break;
            }

          prev_line = _gtk_text_line_previous (prev_line);
        }
    }
  else
    {
      int prev_offset = pango_layout_line_get_start_index (layout_line);

      tmp_list = tmp_list->next;
      while (tmp_list)
        {
          layout_line = tmp_list->data;

          if (line_byte < pango_layout_line_get_start_index (layout_line) + pango_layout_line_get_length (layout_line) ||
              !tmp_list->next)
            {
 	      line_display_index_to_iter (layout, display, iter, prev_offset, 0);
              break;
            }

          prev_offset = pango_layout_line_get_start_index (layout_line);
          tmp_list = tmp_list->next;
        }
    }

 out:

  gtk_text_line_display_unref (display);

  return
    !gtk_text_iter_equal (iter, &orig) &&
    !gtk_text_iter_is_end (iter);
}

/**
 * gtk_text_layout_move_iter_to_next_line:
 * @layout: a `GtkLayout`
 * @iter:   a `GtkTextIter`
 *
 * Move the iterator to the beginning of the next line. The
 * lines of a wrapped paragraph are treated as distinct for
 * this operation.
 **/
gboolean
gtk_text_layout_move_iter_to_next_line (GtkTextLayout *layout,
                                        GtkTextIter   *iter)
{
  GtkTextLine *line;
  GtkTextLineDisplay *display;
  int line_byte;
  GtkTextIter orig;
  gboolean found = FALSE;
  gboolean found_after = FALSE;
  gboolean first = TRUE;

  g_return_val_if_fail (GTK_IS_TEXT_LAYOUT (layout), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  orig = *iter;

  line = _gtk_text_iter_get_text_line (iter);

  while (line && !found_after)
    {
      GSList *tmp_list;

      display = gtk_text_layout_get_line_display (layout, line, FALSE);

      if (display->height == 0)
        goto next;

      if (first)
	{
	  line_byte = line_display_iter_to_index (layout, display, iter);
	  first = FALSE;
	}
      else
	line_byte = 0;

      tmp_list = pango_layout_get_lines_readonly (display->layout);
      while (tmp_list && !found_after)
        {
          PangoLayoutLine *layout_line = tmp_list->data;

          if (found)
            {
	      line_display_index_to_iter (layout, display, iter,
                                          pango_layout_line_get_start_index (layout_line), 0);
              found_after = TRUE;
            }
          else if (line_byte < pango_layout_line_get_start_index (layout_line) + pango_layout_line_get_length (layout_line) || !tmp_list->next)
            found = TRUE;

          tmp_list = tmp_list->next;
        }

    next:

      gtk_text_line_display_unref (display);

      line = _gtk_text_line_next_excluding_last (line);
    }

  if (!found_after)
    gtk_text_buffer_get_end_iter (layout->buffer, iter);

  return
    !gtk_text_iter_equal (iter, &orig) &&
    !gtk_text_iter_is_end (iter);
}

/**
 * gtk_text_layout_move_iter_to_line_end:
 * @layout: a `GtkTextLayout`
 * @direction: if negative, move to beginning of line, otherwise
               move to end of line.
 *
 * Move to the beginning or end of a display line.
 **/
gboolean
gtk_text_layout_move_iter_to_line_end (GtkTextLayout *layout,
                                       GtkTextIter   *iter,
                                       int            direction)
{
  GtkTextLine *line;
  GtkTextLineDisplay *display;
  int line_byte;
  GSList *tmp_list;
  GtkTextIter orig;

  g_return_val_if_fail (GTK_IS_TEXT_LAYOUT (layout), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  orig = *iter;

  line = _gtk_text_iter_get_text_line (iter);
  display = gtk_text_layout_get_line_display (layout, line, FALSE);
  line_byte = line_display_iter_to_index (layout, display, iter);

  tmp_list = pango_layout_get_lines_readonly (display->layout);
  while (tmp_list)
    {
      PangoLayoutLine *layout_line = tmp_list->data;

      if (line_byte < pango_layout_line_get_start_index (layout_line) + pango_layout_line_get_length (layout_line) || !tmp_list->next)
        {
 	  line_display_index_to_iter (layout, display, iter,
 				      direction < 0 ? pango_layout_line_get_start_index (layout_line) : pango_layout_line_get_start_index (layout_line) + pango_layout_line_get_length (layout_line),
 				      0);

          /* FIXME: As a bad hack, we move back one position when we
	   * are inside a paragraph to avoid going to next line on a
	   * forced break not at whitespace. Real fix is to keep track
	   * of whether marks are at leading or trailing edge?  */
          if (direction > 0 && pango_layout_line_get_length (layout_line) > 0 &&
	      !gtk_text_iter_ends_line (iter) &&
	      !_gtk_text_btree_char_is_invisible (iter))
            gtk_text_iter_backward_char (iter);
          break;
        }

      tmp_list = tmp_list->next;
    }

  gtk_text_line_display_unref (display);

  return
    !gtk_text_iter_equal (iter, &orig) &&
    !gtk_text_iter_is_end (iter);
}


/**
 * gtk_text_layout_iter_starts_line:
 * @layout: a `GtkTextLayout`
 * @iter: iterator to test
 *
 * Tests whether an iterator is at the start of a display line.
 **/
gboolean
gtk_text_layout_iter_starts_line (GtkTextLayout       *layout,
                                  const GtkTextIter   *iter)
{
  GtkTextLine *line;
  GtkTextLineDisplay *display;
  int line_byte;
  GSList *tmp_list;

  g_return_val_if_fail (GTK_IS_TEXT_LAYOUT (layout), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  line = _gtk_text_iter_get_text_line (iter);
  display = gtk_text_layout_get_line_display (layout, line, FALSE);
  line_byte = line_display_iter_to_index (layout, display, iter);

  tmp_list = pango_layout_get_lines_readonly (display->layout);
  while (tmp_list)
    {
      PangoLayoutLine *layout_line = tmp_list->data;

      if (line_byte < pango_layout_line_get_start_index (layout_line) + pango_layout_line_get_length (layout_line) ||
          !tmp_list->next)
        {
          /* We're located on this line or the para delimiters before
           * it
           */
          gtk_text_line_display_unref (display);

          if (line_byte == pango_layout_line_get_start_index (layout_line))
            return TRUE;
          else
            return FALSE;
        }

      tmp_list = tmp_list->next;
    }

  g_assert_not_reached ();
  return FALSE;
}

void
gtk_text_layout_get_iter_at_line (GtkTextLayout  *layout,
                                  GtkTextIter    *iter,
                                  GtkTextLine    *line,
                                  int             byte_offset)
{
  _gtk_text_btree_get_iter_at_line (_gtk_text_buffer_get_btree (layout->buffer),
                                    iter, line, byte_offset);
}

/**
 * gtk_text_layout_move_iter_to_x:
 * @layout: a `GtkTextLayout`
 * @iter: a `GtkTextIter`
 * @x: X coordinate
 *
 * Keeping the iterator on the same line of the layout, move it to the
 * specified X coordinate. The lines of a wrapped paragraph are
 * treated as distinct for this operation.
 **/
void
gtk_text_layout_move_iter_to_x (GtkTextLayout *layout,
                                GtkTextIter   *iter,
                                int            x)
{
  GtkTextLine *line;
  GtkTextLineDisplay *display;
  int line_byte;
  PangoLayoutIter *layout_iter;

  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (iter != NULL);

  line = _gtk_text_iter_get_text_line (iter);

  display = gtk_text_layout_get_line_display (layout, line, FALSE);
  line_byte = line_display_iter_to_index (layout, display, iter);

  layout_iter = pango_layout_get_iter (display->layout);

  do
    {
      PangoLayoutLine *layout_line = pango_layout_iter_get_line_readonly (layout_iter);

      if (line_byte < pango_layout_line_get_start_index (layout_line) + pango_layout_line_get_length (layout_line) ||
          pango_layout_iter_at_last_line (layout_iter))
        {
          PangoRectangle logical_rect;
          int byte_index, trailing;
          int x_offset = display->x_offset * PANGO_SCALE;

          pango_layout_iter_get_line_extents (layout_iter, NULL, &logical_rect);

          pango_layout_line_x_to_index (layout_line,
                                        x * PANGO_SCALE - x_offset - logical_rect.x,
                                        &byte_index, &trailing);

 	  line_display_index_to_iter (layout, display, iter, byte_index, trailing);

          break;
        }
    }
  while (pango_layout_iter_next_line (layout_iter));

  pango_layout_iter_free (layout_iter);

  gtk_text_line_display_unref (display);
}

/**
 * gtk_text_layout_move_iter_visually:
 * @layout: a `GtkTextLayout`
 * @iter: a `GtkTextIter`
 * @count: number of characters to move (negative moves left, positive moves right)
 *
 * Move the iterator a given number of characters visually, treating
 * it as the strong cursor position. If @count is positive, then the
 * new strong cursor position will be @count positions to the right of
 * the old cursor position. If @count is negative then the new strong
 * cursor position will be @count positions to the left of the old
 * cursor position.
 *
 * In the presence of bidirection text, the correspondence
 * between logical and visual order will depend on the direction
 * of the current run, and there may be jumps when the cursor
 * is moved off of the end of a run.
 **/

gboolean
gtk_text_layout_move_iter_visually (GtkTextLayout *layout,
                                    GtkTextIter   *iter,
                                    int            count)
{
  GtkTextLineDisplay *display = NULL;
  GtkTextIter orig;
  GtkTextIter lineiter;

  g_return_val_if_fail (layout != NULL, FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  orig = *iter;

  while (count != 0)
    {
      GtkTextLine *line = _gtk_text_iter_get_text_line (iter);
      int line_byte;
      int extra_back = 0;
      gboolean strong;

      int byte_count = _gtk_text_line_byte_count (line);

      int new_index;
      int new_trailing;

      if (!display)
	display = gtk_text_layout_get_line_display (layout, line, FALSE);

      if (layout->cursor_direction == GTK_TEXT_DIR_NONE)
	strong = TRUE;
      else
	strong = display->direction == layout->cursor_direction;

      line_byte = line_display_iter_to_index (layout, display, iter);

      if (count > 0)
        {
          pango_layout_move_cursor_visually (display->layout, strong, line_byte, 0, 1, &new_index, &new_trailing);
          count--;
        }
      else
        {
          pango_layout_move_cursor_visually (display->layout, strong, line_byte, 0, -1, &new_index, &new_trailing);
          count++;
        }

      /* We need to handle the preedit string specially. Well, we don't really need to
       * handle it specially, since hopefully calling gtk_im_context_reset() will
       * remove the preedit string; but if we start off in front of the preedit
       * string (logically) and end up in or on the back edge of the preedit string,
       * we should move the iter one place farther.
       */
      if (layout->preedit_len > 0 && display->insert_index >= 0)
	{
	  if (line_byte == display->insert_index + layout->preedit_len &&
	      new_index < display->insert_index + layout->preedit_len)
	    {
	      line_byte = display->insert_index;
	      extra_back = 1;
	    }
	}

      if (new_index < 0 || (new_index == 0 && extra_back))
        {
          do
            {
              line = _gtk_text_line_previous (line);
              if (!line)
                goto done;
            }
          while (totally_invisible_line (layout, line, &lineiter));

          gtk_text_line_display_unref (display);
 	  display = gtk_text_layout_get_line_display (layout, line, FALSE);
          gtk_text_iter_forward_to_line_end (&lineiter);
          new_index = gtk_text_iter_get_visible_line_index (&lineiter);
        }
      else if (new_index > byte_count)
        {
          do
            {
              line = _gtk_text_line_next_excluding_last (line);
              if (!line)
                goto done;
            }
          while (totally_invisible_line (layout, line, &lineiter));

          gtk_text_line_display_unref (display);
 	  display = gtk_text_layout_get_line_display (layout, line, FALSE);
          new_index = 0;
        }

       line_display_index_to_iter (layout, display, iter, new_index, new_trailing);
       if (extra_back)
	 gtk_text_iter_backward_char (iter);
    }

  g_clear_pointer (&display, gtk_text_line_display_unref);

 done:

  return
    !gtk_text_iter_equal (iter, &orig) &&
    !gtk_text_iter_is_end (iter);
}

void
gtk_text_layout_spew (GtkTextLayout *layout)
{
#if 0
  GtkTextDisplayLine *iter;
  guint wrapped = 0;
  guint paragraphs = 0;
  GtkTextLine *last_line = NULL;

  iter = layout->line_list;
  while (iter != NULL)
    {
      if (iter->line != last_line)
        {
          printf ("%5u  paragraph (%p)\n", paragraphs, iter->line);
          ++paragraphs;
          last_line = iter->line;
        }

      printf ("  %5u  y: %d len: %d start: %d bytes: %d\n",
              wrapped, iter->y, iter->length, iter->byte_offset,
              iter->byte_count);

      ++wrapped;
      iter = iter->next;
    }

  printf ("Layout %s recompute\n",
          layout->need_recompute ? "needs" : "doesn't need");

  printf ("Layout pars: %u lines: %u size: %d x %d Screen width: %d\n",
          paragraphs, wrapped, layout->width,
          layout->height, layout->screen_width);
#endif
}

static void
gtk_text_layout_before_mark_set_handler (GtkTextBuffer     *buffer,
                                         const GtkTextIter *location,
                                         GtkTextMark       *mark,
                                         gpointer           data)
{
  GtkTextLayout *layout = GTK_TEXT_LAYOUT (data);
  GtkTextLayoutPrivate *priv = GTK_TEXT_LAYOUT_GET_PRIVATE (layout);

  if (mark == gtk_text_buffer_get_insert (buffer))
    gtk_text_line_display_cache_set_cursor_line (priv->cache, NULL);
}

/* Catch all situations that move the insertion point.
 */
static void
gtk_text_layout_after_mark_set_handler (GtkTextBuffer     *buffer,
                                        const GtkTextIter *location,
                                        GtkTextMark       *mark,
                                        gpointer           data)
{
  GtkTextLayout *layout = GTK_TEXT_LAYOUT (data);
  GtkTextLayoutPrivate *priv = GTK_TEXT_LAYOUT_GET_PRIVATE (layout);

  if (mark == gtk_text_buffer_get_insert (buffer))
    {
      gtk_text_layout_update_cursor_line (layout);
      gtk_text_line_display_cache_set_cursor_line (priv->cache, priv->cursor_line);
    }
}

static void
gtk_text_layout_before_buffer_insert_text (GtkTextBuffer *textbuffer,
                                           GtkTextIter   *iter,
                                           char          *str,
                                           int            len,
                                           gpointer       data)
{
  GtkTextLayout *layout = GTK_TEXT_LAYOUT (data);
  GtkTextLayoutPrivate *priv = GTK_TEXT_LAYOUT_GET_PRIVATE (layout);
  GtkTextLine *line = _gtk_text_iter_get_text_line (iter);

  gtk_text_line_display_cache_invalidate_line (priv->cache, line);
}

static void
gtk_text_layout_after_buffer_insert_text (GtkTextBuffer *textbuffer,
                                          GtkTextIter   *iter,
                                          char          *str,
                                          int            len,
                                          gpointer       data)
{
  GtkTextLayout *layout = GTK_TEXT_LAYOUT (data);

  gtk_text_layout_update_cursor_line (layout);
}

static void
gtk_text_layout_before_buffer_delete_range (GtkTextBuffer *textbuffer,
                                            GtkTextIter   *start,
                                            GtkTextIter   *end,
                                            gpointer       data)
{
  GtkTextLayout *layout = GTK_TEXT_LAYOUT (data);
  GtkTextLayoutPrivate *priv = GTK_TEXT_LAYOUT_GET_PRIVATE (layout);

  gtk_text_line_display_cache_invalidate_range (priv->cache, layout, start, end, FALSE);
}

static void
gtk_text_layout_after_buffer_delete_range (GtkTextBuffer *textbuffer,
                                           GtkTextIter   *start,
                                           GtkTextIter   *end,
                                           gpointer       data)
{
  GtkTextLayout *layout = GTK_TEXT_LAYOUT (data);

  gtk_text_layout_update_cursor_line (layout);
}

static void
render_para (GskPangoRenderer   *crenderer,
             GtkTextLineDisplay *line_display,
             int                 selection_start_index,
             int                 selection_end_index,
             const GdkRGBA      *selection,
             gboolean            draw_selection_text,
             float               cursor_alpha)
{
  PangoLayout *layout = line_display->layout;
  int byte_offset = 0;
  PangoLayoutIter *iter;
  int screen_width;
  gboolean first = TRUE;

  iter = pango_layout_get_iter (layout);
  screen_width = line_display->total_width;

  do
    {
      PangoLayoutLine *line = pango_layout_iter_get_line_readonly (iter);
      int selection_y, selection_height;
      int first_y, last_y;
      PangoRectangle line_rect;
      int baseline;
      gboolean at_last_line;

      pango_layout_iter_get_line_extents (iter, NULL, &line_rect);
      baseline = pango_layout_iter_get_baseline (iter);
      pango_layout_iter_get_line_yrange (iter, &first_y, &last_y);

      /* Adjust for margins */

      line_rect.x += line_display->x_offset * PANGO_SCALE;
      line_rect.y += line_display->top_margin * PANGO_SCALE;
      baseline += line_display->top_margin * PANGO_SCALE;

      /* Selection is the height of the line, plus top/bottom
       * margin if we're the first/last line
       */
      selection_y = PANGO_PIXELS (first_y) + line_display->top_margin;
      selection_height = PANGO_PIXELS (last_y) - PANGO_PIXELS (first_y);

      if (first)
        {
          selection_y -= line_display->top_margin;
          selection_height += line_display->top_margin;
          first = FALSE;
        }

      at_last_line = pango_layout_iter_at_last_line (iter);
      if (at_last_line)
        selection_height += line_display->bottom_margin;

      /* Don't draw the text underneath if the whole line is selected. We can
       * only do it if the selection is opaque.
       */
      if (selection_start_index < byte_offset &&
          selection_end_index > pango_layout_line_get_length (line) + byte_offset &&
          selection->alpha >= 1)
        {
          gtk_snapshot_append_color (crenderer->snapshot,
                                     selection,
                                     &GRAPHENE_RECT_INIT (line_display->left_margin,
                                                          selection_y,
                                                          screen_width,
                                                          selection_height));

          if (draw_selection_text)
            {
              gsk_pango_renderer_set_state (crenderer, GSK_PANGO_RENDERER_SELECTED);
              pango_renderer_draw_layout_line (PANGO_RENDERER (crenderer),
                                               line,
                                               line_rect.x,
                                               baseline);
            }
        }
      else
        {
          if (line_display->pg_bg_rgba_set)
            gtk_snapshot_append_color (crenderer->snapshot,
                                       &line_display->pg_bg_rgba,
                                       &GRAPHENE_RECT_INIT (line_display->left_margin,
                                                            selection_y,
                                                            screen_width,
                                                            selection_height));

          gsk_pango_renderer_set_state (crenderer, GSK_PANGO_RENDERER_NORMAL);
          pango_renderer_draw_layout_line (PANGO_RENDERER (crenderer),
                                           line,
                                           line_rect.x,
                                           baseline);

          /* Check if some part of the line is selected; the newline
           * that is after pango_layout_line_get_length (line) for the last line of the
           * paragraph counts as part of the line for this
           */
          if ((selection_start_index < byte_offset + pango_layout_line_get_length (line) ||
               (selection_start_index == byte_offset + pango_layout_line_get_length (line) && pango_layout_iter_at_last_line (iter))) &&
              selection_end_index > byte_offset)
            {
              int *ranges = NULL;
              int n_ranges, i;

              pango_layout_line_get_x_ranges (line, selection_start_index, selection_end_index, &ranges, &n_ranges);

              gsk_pango_renderer_set_state (crenderer, GSK_PANGO_RENDERER_SELECTED);

              for (i = 0; i < n_ranges; i++)
                {
                  graphene_rect_t bounds;

                  bounds.origin.x = line_display->x_offset + PANGO_PIXELS (ranges[2*i]);
                  bounds.origin.y = selection_y;
                  bounds.size.width = PANGO_PIXELS (ranges[2*i + 1]) - PANGO_PIXELS (ranges[2*i]);
                  bounds.size.height = selection_height;

                  if (bounds.origin.x < PANGO_PIXELS (line_rect.x))
                    {
                      bounds.size.width -= PANGO_PIXELS (line_rect.x) - bounds.origin.x;
                      bounds.origin.x = PANGO_PIXELS (line_rect.x);
                    }

                  bounds.size.width = MIN (bounds.size.width,
                                           PANGO_PIXELS (line_rect.x) +
                                           PANGO_PIXELS (line_rect.width) -
                                           bounds.origin.x);

                  gtk_snapshot_append_color (crenderer->snapshot, selection, &bounds);

                  if (draw_selection_text)
                    {
                      gtk_snapshot_push_clip (crenderer->snapshot, &bounds);
                      pango_renderer_draw_layout_line (PANGO_RENDERER (crenderer),
                                                       line,
                                                       line_rect.x,
                                                       baseline);
                      gtk_snapshot_pop (crenderer->snapshot);
                    }
                }

              g_free (ranges);

              /* Paint in the ends of the line */
              if (line_rect.x > line_display->left_margin * PANGO_SCALE &&
                  ((line_display->direction == GTK_TEXT_DIR_LTR && selection_start_index < byte_offset) ||
                   (line_display->direction == GTK_TEXT_DIR_RTL && selection_end_index > byte_offset + pango_layout_line_get_length (line))))
                gtk_snapshot_append_color (crenderer->snapshot,
                                           selection,
                                           &GRAPHENE_RECT_INIT (line_display->left_margin,
                                                                selection_y,
                                                                PANGO_PIXELS (line_rect.x) - line_display->left_margin,
                                                                selection_height));

              if (line_rect.x + line_rect.width <
                  (screen_width + line_display->left_margin) * PANGO_SCALE &&
                  ((line_display->direction == GTK_TEXT_DIR_LTR && selection_end_index > byte_offset + pango_layout_line_get_length (line)) ||
                   (line_display->direction == GTK_TEXT_DIR_RTL && selection_start_index < byte_offset)))
                {
                  int nonlayout_width = line_display->left_margin
                                      + screen_width
                                      - PANGO_PIXELS (line_rect.x)
                                      - PANGO_PIXELS (line_rect.width);
                  gtk_snapshot_append_color (crenderer->snapshot,
                                             selection,
                                             &GRAPHENE_RECT_INIT (PANGO_PIXELS (line_rect.x) + PANGO_PIXELS (line_rect.width),
                                                                  selection_y,
                                                                  nonlayout_width,
                                                                  selection_height));
                }
            }
          else if (line_display->has_block_cursor &&
                   gtk_widget_has_focus (crenderer->widget) &&
                   cursor_alpha > 0 &&
                   byte_offset <= line_display->insert_index &&
                   (line_display->insert_index < byte_offset + pango_layout_line_get_length (line) ||
                    (at_last_line && line_display->insert_index == byte_offset + pango_layout_line_get_length (line))))
            {
              GtkCssStyle *style;
              GdkRGBA cursor_color;
              graphene_rect_t bounds = {
                .origin.x = line_display->x_offset + line_display->block_cursor.x,
                .origin.y = line_display->block_cursor.y + line_display->top_margin,
                .size.width = line_display->block_cursor.width,
                .size.height = line_display->block_cursor.height,
              };

              /* we draw text using base color on filled cursor rectangle
               * of cursor color (normally white on black)
               */
              style = gtk_css_node_get_style (gtk_widget_get_css_node (crenderer->widget));
              cursor_color = *gtk_css_color_value_get_rgba (style->font->caret_color ? style->font->caret_color : style->core->color);

              gtk_snapshot_push_opacity (crenderer->snapshot, cursor_alpha);
              gtk_snapshot_append_color (crenderer->snapshot, &cursor_color, &bounds);

              /* draw text under the cursor if any */
              if (!line_display->cursor_at_line_end)
                {
                  gsk_pango_renderer_set_state (crenderer, GSK_PANGO_RENDERER_CURSOR);
                  gtk_snapshot_push_clip (crenderer->snapshot, &bounds);
                  pango_renderer_draw_layout_line (PANGO_RENDERER (crenderer),
                                                   line,
                                                   line_rect.x,
                                                   baseline);
                  gtk_snapshot_pop (crenderer->snapshot);
                }
              gtk_snapshot_pop (crenderer->snapshot);
            }
        }

      byte_offset += pango_layout_line_get_length (line);
    }
  while (pango_layout_iter_next_line (iter));

  pango_layout_iter_free (iter);
}

static gboolean
snapshot_shape (PangoAttrShape         *attr,
                GdkSnapshot            *snapshot,
                double                  width,
                double                  height)
{
  if (GDK_IS_PAINTABLE (attr->data))
    {
      gdk_paintable_snapshot (GDK_PAINTABLE (attr->data), snapshot, width, height);
      return TRUE;
    }

  return FALSE;
}

void
gtk_text_layout_snapshot (GtkTextLayout      *layout,
                          GtkWidget          *widget,
                          GtkSnapshot        *snapshot,
                          const GdkRectangle *clip,
                          float               cursor_alpha)
{
  GtkTextLayoutPrivate *priv;
  GskPangoRenderer *crenderer;
  int offset_y;
  GtkTextIter selection_start, selection_end;
  int selection_start_line;
  int selection_end_line;
  gboolean have_selection;
  gboolean draw_selection_text;
  const GdkRGBA *selection;
  GdkRGBA color;
  GtkSnapshot *cursor_snapshot;
  GtkTextBTree *btree;
  GtkTextLine *first_line;
  GtkTextLine *last_line;
  GtkCssStyle *style;

  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (layout->default_style != NULL);
  g_return_if_fail (layout->buffer != NULL);
  g_return_if_fail (snapshot != NULL);

  priv = GTK_TEXT_LAYOUT_GET_PRIVATE (layout);

  if (clip->height <= 0)
    return;

  btree = _gtk_text_buffer_get_btree (layout->buffer);

  first_line = _gtk_text_btree_find_line_by_y (btree, layout, clip->y, &offset_y);
  if (first_line == NULL)
    return;

  last_line = _gtk_text_btree_find_line_by_y (btree, layout, clip->y + clip->height - 1, NULL);
  if (last_line == NULL)
    last_line = _gtk_text_btree_get_end_iter_line (btree);

  style = gtk_css_node_get_style (gtk_widget_get_css_node (widget));
  color = *gtk_css_color_value_get_rgba (style->core->color);

  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (0, offset_y));
  offset_y = 0;

  cursor_snapshot = NULL;

  crenderer = gsk_pango_renderer_acquire ();

  gsk_pango_renderer_set_shape_handler (crenderer, snapshot_shape);

  crenderer->widget = widget;
  crenderer->snapshot = snapshot;
  crenderer->fg_color = &color;

  have_selection = gtk_text_buffer_get_selection_bounds (layout->buffer,
                                                         &selection_start,
                                                         &selection_end);
  if (have_selection)
    {
      GtkCssNode *selection_node;
      GdkRGBA text_color;

      selection_start_line = gtk_text_iter_get_line (&selection_start);
      selection_end_line = gtk_text_iter_get_line (&selection_end);

      selection_node = gtk_text_view_get_selection_node ((GtkTextView*)widget);
      style = gtk_css_node_get_style (selection_node);

      selection = gtk_css_color_value_get_rgba (style->background->background_color);
      text_color = *gtk_css_color_value_get_rgba (style->core->color);

      draw_selection_text = text_color.alpha > 0;
    }
  else
    {
      selection_start_line = -1;
      selection_end_line = -1;
      selection = NULL;
      draw_selection_text = FALSE;
    }

  gtk_text_layout_wrap_loop_start (layout);

  for (GtkTextLine *line = first_line;
       line != NULL;
       line = _gtk_text_line_next_excluding_last (line))
    {
      GtkTextLineDisplay *line_display;
      int selection_start_index = -1;
      int selection_end_index = -1;

      line_display = gtk_text_layout_get_line_display (layout, line, FALSE);

      if (line_display->height > 0)
        {
          g_assert (line_display->layout != NULL);

          if (have_selection)
            {
              GtkTextIter line_start;
              int current_line;

              gtk_text_layout_get_iter_at_line (layout, &line_start, line, 0);
              current_line = gtk_text_iter_get_line (&line_start);

              if (selection_start_line < current_line)
                selection_start_index = -1;
              else if (selection_start_line == current_line)
                selection_start_index = gtk_text_iter_get_visible_line_index (&selection_start);
              else
                selection_start_index = -1;

              if (selection_end_line < current_line)
                {
                  selection_end_index = -1;
                  have_selection = FALSE;
                }
              else if (selection_end_line == current_line)
                selection_end_index = gtk_text_iter_get_visible_line_index (&selection_end);
              else if (selection_start_line <= current_line)
                {
                  GtkTextIter line_end = line_start;
                  int byte_count;

                  if (!gtk_text_iter_ends_line (&line_end))
                    gtk_text_iter_forward_to_line_end (&line_end);
                  byte_count = gtk_text_iter_get_visible_line_index (&line_end);
                  selection_end_index = byte_count + 1; /* + 1 to flag past-the-end */
                }
              else
                selection_end_index = -1;
            }

          if (line_display->node != NULL)
            {
              if (line_display->has_block_cursor && gtk_widget_has_focus (widget))
                g_clear_pointer (&line_display->node, gsk_render_node_unref);
            }

          if (line_display->node == NULL &&
              (pango_layout_get_character_count (line_display->layout) > 0 ||
               selection_start_index != -1 || selection_end_index != -1 ||
               line_display->has_block_cursor))
            {
              gtk_snapshot_push_collect (snapshot);
              render_para (crenderer, line_display,
                           selection_start_index, selection_end_index,
                           selection,
                           draw_selection_text,
                           cursor_alpha);
              line_display->node = gtk_snapshot_pop_collect (snapshot);
            }

          if (line_display->node != NULL)
            {
              gtk_snapshot_save (crenderer->snapshot);
              gtk_snapshot_translate (crenderer->snapshot,
                                      &GRAPHENE_POINT_INIT (0, offset_y));
              gtk_snapshot_append_node (crenderer->snapshot, line_display->node);
              gtk_snapshot_restore (crenderer->snapshot);
            }

          /* We paint the cursors last, because they overlap another chunk
           * and need to appear on top.
           */
          if (line_display->cursors != NULL)
            {
              GdkDisplay *display;
              GtkCssBoxes boxes;

              if (cursor_snapshot == NULL)
                cursor_snapshot = gtk_snapshot_new ();

              display = gtk_widget_get_display (widget);
              gtk_css_boxes_init (&boxes, widget);

              for (int i = 0; i < line_display->cursors->len; i++)
                {
                  PangoDirection dir;
                  CursorPosition cursor;

                  cursor = g_array_index (line_display->cursors, CursorPosition, i);
                  dir = (line_display->direction == GTK_TEXT_DIR_RTL) ? PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR;

                  if (cursor.is_insert || cursor.is_selection_bound)
                    gtk_snapshot_push_opacity (cursor_snapshot, cursor_alpha);

                  gtk_css_style_snapshot_caret (&boxes, display, cursor_snapshot,
                                                line_display->x_offset, offset_y + line_display->top_margin,
                                                line_display->layout, cursor.pos, dir);

                  if (cursor.is_insert || cursor.is_selection_bound)
                    gtk_snapshot_pop (cursor_snapshot);
                }
            }
        } /* line_display->height > 0 */

      offset_y += line_display->height;

      gtk_text_line_display_unref (line_display);

      if (line == last_line)
        break;
    }

  gtk_text_layout_wrap_loop_end (layout);

  if (cursor_snapshot)
    {
      GskRenderNode *cursors;

      cursors = gtk_snapshot_free_to_node (cursor_snapshot);
      if (cursors)
        {
          gtk_snapshot_append_node (crenderer->snapshot, cursors);
          gsk_render_node_unref (cursors);
        }
    }

  /* Only update eviction source once per snapshot */
  gtk_text_line_display_cache_delay_eviction (priv->cache);

  gsk_pango_renderer_release (crenderer);
}

int
gtk_text_line_display_compare (const GtkTextLineDisplay *display1,
                               const GtkTextLineDisplay *display2,
                               GtkTextLayout            *layout)
{
  int line1;
  int line2;

  line1 = _gtk_text_line_get_number (display1->line);
  line2 = _gtk_text_line_get_number (display2->line);

  if (line1 < line2)
    return -1;
  else if (line1 > line2)
    return 1;
  else
    return 0;
}

void
gtk_text_layout_set_mru_size (GtkTextLayout *layout,
                              guint          mru_size)
{
  GtkTextLayoutPrivate *priv = GTK_TEXT_LAYOUT_GET_PRIVATE (layout);

  gtk_text_line_display_cache_set_mru_size (priv->cache, mru_size);
}
