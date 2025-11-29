/* GTK - The GIMP Toolkit
 * gtktexttagprivate.h Copyright (C) 2000 Red Hat, Inc.
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

#pragma once

#include <gtk/gtk.h>
#include <gtk/gtktextattributesprivate.h>

typedef struct _GtkTextBTreeNode GtkTextBTreeNode;


struct _GtkTextTagPrivate
{
  GtkTextTagTable *table;

  char *name;           /* Name of this tag.  This field is actually
                         * a pointer to the key from the entry in
                         * tkxt->tagTable, so it needn't be freed
                         * explicitly. */
  /*
   * Information for displaying text with this tag.  The information
   * belows acts as an override on information specified by lower-priority
   * tags.  If no value is specified, then the next-lower-priority tag
   * on the text determines the value.  The text widget itself provides
   * defaults if no tag specifies an override.
   */

  GtkTextAttributes *values;

  int priority;  /* Priority of this tag within widget.  0
                         * means lowest priority.  Exactly one tag
                         * has each integer value between 0 and
                         * numTags-1. */
  /* Flags for whether a given value is set; if a value is unset, then
   * this tag does not affect it.
   */
  guint bg_color_set : 1;
  guint fg_color_set : 1;
  guint underline_rgba_set : 1;
  guint overline_rgba_set : 1;
  guint strikethrough_rgba_set : 1;
  guint scale_set : 1;
  guint justification_set : 1;
  guint left_margin_set : 1;
  guint indent_set : 1;
  guint rise_set : 1;
  guint strikethrough_set : 1;
  guint right_margin_set : 1;
  guint pixels_above_lines_set : 1;
  guint pixels_below_lines_set : 1;
  guint pixels_inside_wrap_set : 1;
  guint line_height_set : 1;
  guint tabs_set : 1;
  guint underline_set : 1;
  guint overline_set : 1;
  guint wrap_mode_set : 1;
  guint bg_full_height_set : 1;
  guint invisible_set : 1;
  guint editable_set : 1;
  guint language_set : 1;
  guint pg_bg_color_set : 1;
  guint fallback_set : 1;
  guint letter_spacing_set : 1;
  guint font_features_set : 1;
  guint allow_breaks_set : 1;
  guint show_spaces_set : 1;
  guint insert_hyphens_set : 1;
  guint text_transform_set : 1;
  guint word_set : 1;
  guint sentence_set : 1;

  /* Whether these margins accumulate or override */
  guint accumulative_margin : 1;
};


/* values should already have desired defaults; this function will override
 * the defaults with settings in the given tags, which should be sorted in
 * ascending order of priority
*/
void _gtk_text_attributes_fill_from_tags   (GtkTextAttributes   *values,
                                            GPtrArray           *tags);
void _gtk_text_tag_array_sort              (GPtrArray           *tags);

gboolean _gtk_text_tag_affects_size               (GtkTextTag *tag);
gboolean _gtk_text_tag_affects_nonsize_appearance (GtkTextTag *tag);

