/* gtktextchild.c - child pixmaps and widgets
 *
 * Copyright (c) 1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 2000      Red Hat, Inc.
 * Tk -> Gtk port by Havoc Pennington <hp@redhat.com>
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

#include "config.h"
#include "gtktextchild.h"
#include "gtktextbtreeprivate.h"
#include "gtktextlayoutprivate.h"
#include "gtkprivate.h"

typedef struct {
  char *replacement;
} GtkTextChildAnchorPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GtkTextChildAnchor, gtk_text_child_anchor, G_TYPE_OBJECT)

#define CHECK_IN_BUFFER(anchor)                                         \
  G_STMT_START {                                                        \
    if ((anchor)->segment == NULL)                                      \
      {                                                                 \
        g_warning ("%s: GtkTextChildAnchor hasn't been in a buffer yet",\
                   G_STRFUNC);                                          \
      }                                                                 \
  } G_STMT_END

#define CHECK_IN_BUFFER_RETURN(anchor, val)                             \
  G_STMT_START {                                                        \
    if ((anchor)->segment == NULL)                                      \
      {                                                                 \
        g_warning ("%s: GtkTextChildAnchor hasn't been in a buffer yet",\
                   G_STRFUNC);                                          \
        return (val);                                                   \
      }                                                                 \
  } G_STMT_END

#define PAINTABLE_SEG_SIZE ((unsigned) (G_STRUCT_OFFSET (GtkTextLineSegment, body) \
        + sizeof (GtkTextPaintable)))

#define WIDGET_SEG_SIZE ((unsigned) (G_STRUCT_OFFSET (GtkTextLineSegment, body) \
        + sizeof (GtkTextChildBody)))


static void
paintable_invalidate_size (GdkPaintable       *paintable,
                           GtkTextLineSegment *seg)
{
  if (seg->body.paintable.tree)
    {
      GtkTextIter start, end;

      _gtk_text_btree_get_iter_at_paintable (seg->body.paintable.tree, &start, seg);
      end = start;
      gtk_text_iter_forward_char (&end);

      _gtk_text_btree_invalidate_region (seg->body.paintable.tree, &start, &end, FALSE);
    }
}

static void
paintable_invalidate_contents (GdkPaintable       *paintable,
                               GtkTextLineSegment *seg)
{
  /* These do the same anyway */
  paintable_invalidate_size (paintable, seg);
}

static GtkTextLineSegment *
paintable_segment_cleanup_func (GtkTextLineSegment *seg,
                                GtkTextLine        *line)
{
  seg->body.paintable.line = line;

  return seg;
}

static int
paintable_segment_delete_func (GtkTextLineSegment *seg,
                               GtkTextLine        *line,
                               gboolean            tree_gone)
{
  GdkPaintable *paintable;
  guint flags;

  seg->body.paintable.tree = NULL;
  seg->body.paintable.line = NULL;

  paintable = seg->body.paintable.paintable;
  if (paintable)
    {
      flags = gdk_paintable_get_flags (paintable);
      if ((flags & GDK_PAINTABLE_STATIC_CONTENTS) == 0)
        g_signal_handlers_disconnect_by_func (paintable, G_CALLBACK (paintable_invalidate_contents), seg);

      if ((flags & GDK_PAINTABLE_STATIC_SIZE) == 0)
        g_signal_handlers_disconnect_by_func (paintable, G_CALLBACK (paintable_invalidate_size), seg);

      g_object_unref (paintable);
    }

  g_free (seg);

  return 0;
}

static void
paintable_segment_check_func (GtkTextLineSegment *seg,
                              GtkTextLine        *line)
{
  if (seg->next == NULL)
    g_error ("paintable segment is the last segment in a line");

  if (seg->byte_count != GTK_TEXT_UNKNOWN_CHAR_UTF8_LEN)
    g_error ("paintable segment has byte count of %d", seg->byte_count);

  if (seg->char_count != 1)
    g_error ("paintable segment has char count of %d", seg->char_count);
}

const GtkTextLineSegmentClass gtk_text_paintable_type = {
  "paintable",                          /* name */
  FALSE,                                /* leftGravity */
  NULL,                                 /* splitFunc */
  paintable_segment_delete_func,        /* deleteFunc */
  paintable_segment_cleanup_func,       /* cleanupFunc */
  NULL,                                 /* lineChangeFunc */
  paintable_segment_check_func          /* checkFunc */

};

GtkTextLineSegment *
_gtk_paintable_segment_new (GdkPaintable *paintable)
{
  /* gcc-11 issues a diagnostic here because the size allocated
     for SEG does not cover the entire size of a GtkTextLineSegment
     and gcc has no way to know that the union will only be used
     for limited types and the additional space is not needed.  */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
  GtkTextLineSegment *seg;
  guint flags;

  seg = g_malloc (PAINTABLE_SEG_SIZE);

  seg->type = &gtk_text_paintable_type;

  seg->next = NULL;

  /* We convert to the 0xFFFC "unknown character",
   * a 3-byte sequence in UTF-8.
   */
  seg->byte_count = GTK_TEXT_UNKNOWN_CHAR_UTF8_LEN;
  seg->char_count = 1;

  seg->body.paintable.paintable = paintable;
  seg->body.paintable.tree = NULL;
  seg->body.paintable.line = NULL;

  flags = gdk_paintable_get_flags (paintable);
  if ((flags & GDK_PAINTABLE_STATIC_CONTENTS) == 0)
    g_signal_connect (paintable,
                      "invalidate-contents",
                      G_CALLBACK (paintable_invalidate_contents),
                      seg);

  if ((flags & GDK_PAINTABLE_STATIC_SIZE) == 0)
    g_signal_connect (paintable,
                      "invalidate-size",
                      G_CALLBACK (paintable_invalidate_size),
                      seg);

  g_object_ref (paintable);

  return seg;
#pragma GCC diagnostic pop
}


static GtkTextLineSegment *
child_segment_cleanup_func (GtkTextLineSegment *seg,
                            GtkTextLine        *line)
{
  seg->body.child.line = line;

  return seg;
}

static int
child_segment_delete_func (GtkTextLineSegment *seg,
                           GtkTextLine       *line,
                           gboolean           tree_gone)
{
  GSList *tmp_list;
  GSList *copy;

  _gtk_text_btree_unregister_child_anchor (seg->body.child.obj);

  seg->body.child.tree = NULL;
  seg->body.child.line = NULL;

  /* avoid removing widgets while walking the list */
  copy = g_slist_copy (seg->body.child.widgets);
  tmp_list = copy;
  while (tmp_list != NULL)
    {
      GtkWidget *child = tmp_list->data;

      gtk_text_view_remove (GTK_TEXT_VIEW (gtk_widget_get_parent (child)), child);

      tmp_list = tmp_list->next;
    }

  /* On removal from the widget's parents (GtkTextView),
   * the widget should have been removed from the anchor.
   */
  g_assert (seg->body.child.widgets == NULL);

  g_slist_free (copy);

  _gtk_widget_segment_unref (seg);

  return 0;
}

static void
child_segment_check_func (GtkTextLineSegment *seg,
                          GtkTextLine        *line)
{
  if (seg->next == NULL)
    g_error ("child segment is the last segment in a line");

  if (seg->char_count != 1)
    g_error ("child segment has char count of %d", seg->char_count);
}

const GtkTextLineSegmentClass gtk_text_child_type = {
  "child-widget",                                        /* name */
  FALSE,                                                 /* leftGravity */
  NULL,                                                  /* splitFunc */
  child_segment_delete_func,                             /* deleteFunc */
  child_segment_cleanup_func,                            /* cleanupFunc */
  NULL,                                                  /* lineChangeFunc */
  child_segment_check_func                               /* checkFunc */
};

GtkTextLineSegment *
_gtk_widget_segment_new (GtkTextChildAnchor *anchor)
{
  /* gcc-11 issues a diagnostic here because the size allocated
     for SEG does not cover the entire size of a GtkTextLineSegment
     and gcc has no way to know that the union will only be used
     for limited types and the additional space is not needed.  */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
  GtkTextLineSegment *seg;
  GtkTextChildAnchorPrivate *priv = gtk_text_child_anchor_get_instance_private (anchor);

  seg = g_malloc (WIDGET_SEG_SIZE);

  seg->type = &gtk_text_child_type;

  seg->next = NULL;

  seg->byte_count = strlen (priv->replacement);
  seg->char_count = g_utf8_strlen (priv->replacement, seg->byte_count);

  seg->body.child.obj = anchor;
  seg->body.child.obj->segment = seg;
  seg->body.child.widgets = NULL;
  seg->body.child.tree = NULL;
  seg->body.child.line = NULL;

  g_object_ref (anchor);

  return seg;
#pragma GCC diagnostic pop
}

void
_gtk_widget_segment_add    (GtkTextLineSegment *widget_segment,
                            GtkWidget          *child)
{
  g_return_if_fail (widget_segment->type == &gtk_text_child_type);
  g_return_if_fail (widget_segment->body.child.tree != NULL);

  g_object_ref (child);

  widget_segment->body.child.widgets =
    g_slist_prepend (widget_segment->body.child.widgets,
                     child);
}

void
_gtk_widget_segment_remove (GtkTextLineSegment *widget_segment,
                            GtkWidget          *child)
{
  g_return_if_fail (widget_segment->type == &gtk_text_child_type);

  widget_segment->body.child.widgets =
    g_slist_remove (widget_segment->body.child.widgets,
                    child);

  g_object_unref (child);
}

void
_gtk_widget_segment_ref (GtkTextLineSegment *widget_segment)
{
  g_assert (widget_segment->type == &gtk_text_child_type);

  g_object_ref (widget_segment->body.child.obj);
}

void
_gtk_widget_segment_unref (GtkTextLineSegment *widget_segment)
{
  g_assert (widget_segment->type == &gtk_text_child_type);

  g_object_unref (widget_segment->body.child.obj);
}

GtkTextLayout*
_gtk_anchored_child_get_layout (GtkWidget *child)
{
  return g_object_get_data (G_OBJECT (child), "gtk-text-child-anchor-layout");
}

static void
_gtk_anchored_child_set_layout (GtkWidget     *child,
                                GtkTextLayout *layout)
{
  g_object_set_data (G_OBJECT (child),
                     I_("gtk-text-child-anchor-layout"),
                     layout);
}

static void gtk_text_child_anchor_finalize (GObject *obj);

static void
gtk_text_child_anchor_init (GtkTextChildAnchor *child_anchor)
{
  child_anchor->segment = NULL;
}

static void
gtk_text_child_anchor_class_init (GtkTextChildAnchorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_text_child_anchor_finalize;
}

/**
 * gtk_text_child_anchor_new:
 *
 * Creates a new `GtkTextChildAnchor`.
 *
 * Usually you would then insert it into a `GtkTextBuffer` with
 * [method@Gtk.TextBuffer.insert_child_anchor]. To perform the
 * creation and insertion in one step, use the convenience
 * function [method@Gtk.TextBuffer.create_child_anchor].
 *
 * Returns: a new `GtkTextChildAnchor`
 **/
GtkTextChildAnchor*
gtk_text_child_anchor_new (void)
{
  return gtk_text_child_anchor_new_with_replacement (_gtk_text_unknown_char_utf8);
}

/**
 * gtk_text_child_anchor_new_with_replacement:
 *
 * Creates a new `GtkTextChildAnchor` with the given replacement character.
 *
 * Usually you would then insert it into a `GtkTextBuffer` with
 * [method@Gtk.TextBuffer.insert_child_anchor].
 *
 * Returns: a new `GtkTextChildAnchor`
 *
 * Since: 4.6
 **/
GtkTextChildAnchor *
gtk_text_child_anchor_new_with_replacement (const char *replacement_character)
{
  GtkTextChildAnchor *anchor;
  GtkTextChildAnchorPrivate *priv;

  /* only a single character can be set as replacement */
  g_return_val_if_fail (g_utf8_strlen (replacement_character, -1) == 1, NULL);

  anchor = g_object_new (GTK_TYPE_TEXT_CHILD_ANCHOR, NULL);

  priv = gtk_text_child_anchor_get_instance_private (anchor);

  priv->replacement = g_strdup (replacement_character);

  return anchor;
}

static void
gtk_text_child_anchor_finalize (GObject *obj)
{
  GtkTextChildAnchor *anchor = GTK_TEXT_CHILD_ANCHOR (obj);
  GtkTextChildAnchorPrivate *priv = gtk_text_child_anchor_get_instance_private (anchor);
  GtkTextLineSegment *seg = anchor->segment;

  if (seg)
    {
      if (seg->body.child.tree != NULL)
        {
          g_warning ("Someone removed a reference to a GtkTextChildAnchor "
                     "they didn't own; the anchor is still in the text buffer "
                     "and the refcount is 0.");
          return;
        }

      g_slist_free_full (seg->body.child.widgets, g_object_unref);

      g_free (seg);
    }

  g_free (priv->replacement);

  G_OBJECT_CLASS (gtk_text_child_anchor_parent_class)->finalize (obj);
}

/**
 * gtk_text_child_anchor_get_widgets:
 * @anchor: a `GtkTextChildAnchor`
 * @out_len: (out): return location for the length of the array
 *
 * Gets a list of all widgets anchored at this child anchor.
 *
 * The order in which the widgets are returned is not defined.
 *
 * Returns: (array length=out_len) (transfer container): an
 *   array of widgets anchored at @anchor
 */
GtkWidget **
gtk_text_child_anchor_get_widgets (GtkTextChildAnchor *anchor,
                                   guint              *out_len)
{
  GtkTextLineSegment *seg = anchor->segment;
  GPtrArray *arr;
  GSList *iter;

  CHECK_IN_BUFFER_RETURN (anchor, NULL);

  g_return_val_if_fail (out_len != NULL, NULL);
  g_return_val_if_fail (seg->type == &gtk_text_child_type, NULL);

  iter = seg->body.child.widgets;

  if (!iter)
    {
      *out_len = 0;
      return NULL;
    }

  arr = g_ptr_array_new ();
  while (iter != NULL)
    {
      g_ptr_array_add (arr, iter->data);

      iter = iter->next;
    }

  /* Order is not relevant, so we don't need to reverse the list
   * again.
   */
  *out_len = arr->len;
  return (GtkWidget **)g_ptr_array_free (arr, FALSE);
}

/**
 * gtk_text_child_anchor_get_deleted:
 * @anchor: a `GtkTextChildAnchor`
 *
 * Determines whether a child anchor has been deleted from
 * the buffer.
 *
 * Keep in mind that the child anchor will be unreferenced
 * when removed from the buffer, so you need to hold your own
 * reference (with g_object_ref()) if you plan to use this
 * function — otherwise all deleted child anchors will also
 * be finalized.
 *
 * Returns: %TRUE if the child anchor has been deleted from its buffer
 */
gboolean
gtk_text_child_anchor_get_deleted (GtkTextChildAnchor *anchor)
{
  GtkTextLineSegment *seg = anchor->segment;

  CHECK_IN_BUFFER_RETURN (anchor, TRUE);

  g_return_val_if_fail (seg->type == &gtk_text_child_type, TRUE);

  return seg->body.child.tree == NULL;
}

void
gtk_text_child_anchor_register_child (GtkTextChildAnchor *anchor,
                                      GtkWidget          *child,
                                      GtkTextLayout      *layout)
{
  g_return_if_fail (GTK_IS_TEXT_CHILD_ANCHOR (anchor));
  g_return_if_fail (GTK_IS_WIDGET (child));

  CHECK_IN_BUFFER (anchor);

  _gtk_anchored_child_set_layout (child, layout);

  _gtk_widget_segment_add (anchor->segment, child);

  gtk_text_child_anchor_queue_resize (anchor, layout);
}

void
gtk_text_child_anchor_unregister_child (GtkTextChildAnchor *anchor,
                                        GtkWidget          *child)
{
  g_return_if_fail (GTK_IS_TEXT_CHILD_ANCHOR (anchor));
  g_return_if_fail (GTK_IS_WIDGET (child));

  CHECK_IN_BUFFER (anchor);

  if (_gtk_anchored_child_get_layout (child))
    {
      gtk_text_child_anchor_queue_resize (anchor,
                                          _gtk_anchored_child_get_layout (child));
    }

  _gtk_anchored_child_set_layout (child, NULL);

  _gtk_widget_segment_remove (anchor->segment, child);
}

void
gtk_text_child_anchor_queue_resize (GtkTextChildAnchor *anchor,
                                    GtkTextLayout      *layout)
{
  GtkTextIter start;
  GtkTextIter end;
  GtkTextLineSegment *seg;

  g_return_if_fail (GTK_IS_TEXT_CHILD_ANCHOR (anchor));
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));

  CHECK_IN_BUFFER (anchor);

  seg = anchor->segment;

  if (seg->body.child.tree == NULL)
    return;

  gtk_text_buffer_get_iter_at_child_anchor (layout->buffer,
                                            &start, anchor);
  end = start;
  gtk_text_iter_forward_char (&end);

  gtk_text_layout_invalidate (layout, &start, &end);
}

void
gtk_text_anchored_child_set_layout (GtkWidget     *child,
                                    GtkTextLayout *layout)
{
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (layout == NULL || GTK_IS_TEXT_LAYOUT (layout));

  _gtk_anchored_child_set_layout (child, layout);
}

const char *
gtk_text_child_anchor_get_replacement (GtkTextChildAnchor *anchor)
{
  GtkTextChildAnchorPrivate *priv = gtk_text_child_anchor_get_instance_private (anchor);

  return priv->replacement;
}
