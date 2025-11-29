/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * Modified by the GTK+ Team and others 1997-2001.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtktextview.h"
#include "gtktextutilprivate.h"

#include "gtkcsscolorvalueprivate.h"
#include "gtktextbuffer.h"
#include "gtktextlayoutprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkrenderbackgroundprivate.h"

#define DRAG_ICON_MAX_WIDTH 250
#define DRAG_ICON_MAX_HEIGHT 250
#define DRAG_ICON_MAX_LINES 7
#define ELLIPSIS_CHARACTER "\xe2\x80\xa6"

static void
append_n_lines (GString *str, const char *text, GSList *lines, int n_lines)
{
  PangoLayoutLine *line;
  int i;

  for (i = 0; i < n_lines; i++)
    {
      line = lines->data;
      g_string_append_len (str,
                           &text[pango_layout_line_get_start_index (line)],
                           pango_layout_line_get_length (line));
      lines = lines->next;
    }
}

static void
limit_layout_lines (PangoLayout *layout)
{
  const char *text;
  GString     *str;
  GSList      *lines, *elem;
  int          n_lines;

  n_lines = pango_layout_get_line_count (layout);

  if (n_lines >= DRAG_ICON_MAX_LINES)
    {
      text  = pango_layout_get_text (layout);
      str   = g_string_new (NULL);
      lines = pango_layout_get_lines_readonly (layout);

      /* get first lines */
      elem = lines;
      append_n_lines (str, text, elem,
                      DRAG_ICON_MAX_LINES / 2);

      g_string_append (str, "\n" ELLIPSIS_CHARACTER "\n");

      /* get last lines */
      elem = g_slist_nth (lines, n_lines - DRAG_ICON_MAX_LINES / 2);
      append_n_lines (str, text, elem,
                      DRAG_ICON_MAX_LINES / 2);

      pango_layout_set_text (layout, str->str, -1);
      g_string_free (str, TRUE);
    }
}

/**
 * gtk_text_util_create_drag_icon:
 * @widget: `GtkWidget` to extract the pango context
 * @text: a #char to render the icon
 * @len: length of @text, or -1 for NUL-terminated text
 *
 * Creates a drag and drop icon from @text.
 *
 * Returns: (transfer full): a `GdkPaintable` to use as DND icon
 */
GdkPaintable *
gtk_text_util_create_drag_icon (GtkWidget *widget,
                                char      *text,
                                gssize     len)
{
  GtkCssStyle *style;
  GtkSnapshot *snapshot;
  PangoContext *context;
  PangoLayout *layout;
  GdkPaintable *paintable;
  int layout_width;
  int layout_height;
  const GdkRGBA *color;
  GdkDisplay *display;

  g_return_val_if_fail (widget != NULL, NULL);
  g_return_val_if_fail (text != NULL, NULL);

  context = gtk_widget_get_pango_context (widget);
  layout  = pango_layout_new (context);

  pango_layout_set_text (layout, text, len);
  pango_layout_set_wrap (layout, PANGO_WRAP_WORD_CHAR);
  pango_layout_get_size (layout, &layout_width, NULL);

  layout_width = MIN (layout_width, DRAG_ICON_MAX_WIDTH * PANGO_SCALE);
  pango_layout_set_width (layout, layout_width);

  limit_layout_lines (layout);

  snapshot = gtk_snapshot_new ();

  style = gtk_css_node_get_style (gtk_widget_get_css_node (widget));
  color = gtk_css_color_value_get_rgba (style->core->color);

  display = gtk_widget_get_display (widget);

  if (!gdk_display_is_rgba (display) ||
      !gdk_display_is_composited (display))
    {
      GtkWidget *bg_widget;
      GtkCssBoxes boxes;

      if (GTK_IS_TEXT (widget))
        bg_widget = gtk_widget_get_parent (widget);
      else
        bg_widget = widget;

      pango_layout_get_size (layout, &layout_width, &layout_height);

      style = gtk_css_node_get_style (gtk_widget_get_css_node (bg_widget));
      gtk_css_boxes_init_border_box (&boxes, style,
                                     0, 0,
                                     layout_width / PANGO_SCALE,
                                     layout_height / PANGO_SCALE);
      gtk_css_style_snapshot_background (&boxes, snapshot);
    }

  gtk_snapshot_append_layout (snapshot, layout, color);

  paintable = gtk_snapshot_free_to_paintable (snapshot, NULL);
  g_object_unref (layout);

  return paintable;
}

static void
set_attributes_from_style (GtkWidget         *widget,
                           GtkTextAttributes *values)
{
  GtkCssStyle *style;
  const GdkRGBA black = { 0, };

  if (!values->appearance.bg_rgba)
    values->appearance.bg_rgba = gdk_rgba_copy (&black);
  if (!values->appearance.fg_rgba)
    values->appearance.fg_rgba = gdk_rgba_copy (&black);

  style = gtk_css_node_get_style (gtk_widget_get_css_node (widget));

  *values->appearance.bg_rgba = *gtk_css_color_value_get_rgba (style->background->background_color);
  *values->appearance.fg_rgba = *gtk_css_color_value_get_rgba (style->core->color);

  if (values->font)
    pango_font_description_free (values->font);

  values->font = gtk_css_style_get_pango_font (style);
}

static int
get_border_window_size (GtkTextView       *text_view,
                        GtkTextWindowType  window_type)
{
  GtkWidget *gutter;

  gutter = gtk_text_view_get_gutter (text_view, window_type);
  if (gutter != NULL)
    return gtk_widget_get_width (gutter);

  return 0;
}

GdkPaintable *
gtk_text_util_create_rich_drag_icon (GtkWidget     *widget,
                                     GtkTextBuffer *buffer,
                                     GtkTextIter   *start,
                                     GtkTextIter   *end)
{
  GdkPaintable      *paintable;
  GtkSnapshot       *snapshot;
  int                layout_width, layout_height;
  GtkTextBuffer     *new_buffer;
  GtkTextLayout     *layout;
  GtkTextAttributes *style;
  PangoContext      *ltr_context, *rtl_context;
  GtkTextIter        iter;
  GdkDisplay        *display;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);
  g_return_val_if_fail (start != NULL, NULL);
  g_return_val_if_fail (end != NULL, NULL);

  new_buffer = gtk_text_buffer_new (gtk_text_buffer_get_tag_table (buffer));
  gtk_text_buffer_get_start_iter (new_buffer, &iter);

  gtk_text_buffer_insert_range (new_buffer, &iter, start, end);

  gtk_text_buffer_get_start_iter (new_buffer, &iter);

  layout = gtk_text_layout_new ();

  ltr_context = gtk_widget_create_pango_context (widget);
  pango_context_set_base_dir (ltr_context, PANGO_DIRECTION_LTR);
  rtl_context = gtk_widget_create_pango_context (widget);
  pango_context_set_base_dir (rtl_context, PANGO_DIRECTION_RTL);

  gtk_text_layout_set_contexts (layout, ltr_context, rtl_context);

  g_object_unref (ltr_context);
  g_object_unref (rtl_context);

  style = gtk_text_attributes_new ();

  layout_width = gtk_widget_get_width (widget);

  set_attributes_from_style (widget, style);

  if (GTK_IS_TEXT_VIEW (widget))
    {
      layout_width = layout_width
        - get_border_window_size (GTK_TEXT_VIEW (widget), GTK_TEXT_WINDOW_LEFT)
        - get_border_window_size (GTK_TEXT_VIEW (widget), GTK_TEXT_WINDOW_RIGHT);
    }

  style->direction = gtk_widget_get_direction (widget);
  style->wrap_mode = GTK_WRAP_WORD_CHAR;

  gtk_text_layout_set_default_style (layout, style);
  gtk_text_attributes_unref (style);

  gtk_text_layout_set_buffer (layout, new_buffer);
  gtk_text_layout_set_cursor_visible (layout, FALSE);
  gtk_text_layout_set_screen_width (layout, layout_width);

  gtk_text_layout_validate (layout, DRAG_ICON_MAX_HEIGHT);
  gtk_text_layout_get_size (layout, &layout_width, &layout_height);

  layout_width = MIN (layout_width, DRAG_ICON_MAX_WIDTH);
  layout_height = MIN (layout_height, DRAG_ICON_MAX_HEIGHT);

  snapshot = gtk_snapshot_new ();

  display = gtk_widget_get_display (widget);

  if (!gdk_display_is_rgba (display) ||
      !gdk_display_is_composited (display))
    {
      GtkCssBoxes boxes;
      GtkCssStyle *css_style;

      css_style = gtk_css_node_get_style (gtk_widget_get_css_node (widget));
      gtk_css_boxes_init_border_box (&boxes, css_style,
                                     0, 0, layout_width, layout_height);
      gtk_css_style_snapshot_background (&boxes, snapshot);
    }

  gtk_text_layout_snapshot (layout, widget, snapshot, &(GdkRectangle) { 0, 0, layout_width, layout_height }, 1.0);

  g_object_unref (layout);
  g_object_unref (new_buffer);

  paintable = gtk_snapshot_free_to_paintable (snapshot, &(graphene_size_t) { layout_width, layout_height });

  return paintable;
}

static int
layout_get_char_width (PangoLayout *layout)
{
  int width;
  PangoFontMetrics *metrics;
  const PangoFontDescription *font_desc;
  PangoContext *context = pango_layout_get_context (layout);

  font_desc = pango_layout_get_font_description (layout);
  if (!font_desc)
    font_desc = pango_context_get_font_description (context);

  metrics = pango_context_get_metrics (context, font_desc, NULL);
  width = pango_font_metrics_get_approximate_char_width (metrics);
  pango_font_metrics_unref (metrics);

  return width;
}

/*
 * _gtk_text_util_get_block_cursor_location
 * @layout: a `PangoLayout`
 * @index: index at which cursor is located
 * @pos: cursor location
 * @at_line_end: whether cursor is drawn at line end, not over some
 * character
 *
 * Returns: whether cursor should actually be drawn as a rectangle.
 *   It may not be the case if character at index is invisible.
 */
gboolean
_gtk_text_util_get_block_cursor_location (PangoLayout    *layout,
					  int             index,
					  PangoRectangle *pos,
					  gboolean       *at_line_end)
{
  PangoRectangle strong_pos, weak_pos;
  PangoLayoutLine *layout_line;
  gboolean rtl;
  int line_no;
  const char *text;

  g_return_val_if_fail (layout != NULL, FALSE);
  g_return_val_if_fail (index >= 0, FALSE);
  g_return_val_if_fail (pos != NULL, FALSE);

  pango_layout_index_to_pos (layout, index, pos);

  if (pos->width != 0)
    {
      /* cursor is at some visible character, good */
      if (at_line_end)
	*at_line_end = FALSE;
      if (pos->width < 0)
	{
	  pos->x += pos->width;
	  pos->width = -pos->width;
	}
      return TRUE;
    }

  pango_layout_index_to_line_x (layout, index, FALSE, &line_no, NULL);
  layout_line = pango_layout_get_line_readonly (layout, line_no);
  g_return_val_if_fail (layout_line != NULL, FALSE);

  text = pango_layout_get_text (layout);

  if (index < pango_layout_line_get_start_index (layout_line) + pango_layout_line_get_length (layout_line))
    {
      /* this may be a zero-width character in the middle of the line,
       * or it could be a character where line is wrapped, we do want
       * block cursor in latter case */
      if (g_utf8_next_char (text + index) - text !=
	  pango_layout_line_get_start_index (layout_line) + pango_layout_line_get_length (layout_line))
	{
	  /* zero-width character in the middle of the line, do not
	   * bother with block cursor */
	  return FALSE;
	}
    }

  /* Cursor is at the line end. It may be an empty line, or it could
   * be on the left or on the right depending on text direction, or it
   * even could be in the middle of visual layout in bidi text. */

  pango_layout_get_cursor_pos (layout, index, &strong_pos, &weak_pos);

  if (strong_pos.x != weak_pos.x)
    {
      /* do not show block cursor in this case, since the character typed
       * in may or may not appear at the cursor position */
      return FALSE;
    }

  /* In case when index points to the end of line, pos->x is always most right
   * pixel of the layout line, so we need to correct it for RTL text. */
  if (pango_layout_line_get_length (layout_line))
    {
      if (pango_layout_line_get_resolved_direction (layout_line) == PANGO_DIRECTION_RTL)
	{
	  PangoLayoutIter *iter;
	  PangoRectangle line_rect;
	  int i;
	  int left, right;
	  const char *p;

	  p = g_utf8_prev_char (text + index);

	  pango_layout_line_index_to_x (layout_line, p - text, FALSE, &left);
	  pango_layout_line_index_to_x (layout_line, p - text, TRUE, &right);
	  pos->x = MIN (left, right);

	  iter = pango_layout_get_iter (layout);
	  for (i = 0; i < line_no; i++)
	    pango_layout_iter_next_line (iter);
	  pango_layout_iter_get_line_extents (iter, NULL, &line_rect);
	  pango_layout_iter_free (iter);

          rtl = TRUE;
	  pos->x += line_rect.x;
	}
      else
	rtl = FALSE;
    }
  else
    {
      PangoContext *context = pango_layout_get_context (layout);
      rtl = pango_context_get_base_dir (context) == PANGO_DIRECTION_RTL;
    }

  pos->width = layout_get_char_width (layout);

  if (rtl)
    pos->x -= pos->width - 1;

  if (at_line_end)
    *at_line_end = TRUE;

  return pos->width != 0;
}
