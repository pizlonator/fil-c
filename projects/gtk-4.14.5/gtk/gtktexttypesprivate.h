/* GTK - The GIMP Toolkit
 * gtktexttypes.h Copyright (C) 2000 Red Hat, Inc.
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
#include <gtk/gtktexttagprivate.h>

G_BEGIN_DECLS

/*
 * This is the PUBLIC representation of a text buffer.
 * GtkTextBTree is the PRIVATE internal representation of it.
 */

typedef struct _GtkTextBTree GtkTextBTree;

typedef struct _GtkTextCounter GtkTextCounter;
typedef struct _GtkTextLineSegment GtkTextLineSegment;
typedef struct _GtkTextLineSegmentClass GtkTextLineSegmentClass;
typedef struct _GtkTextToggleBody GtkTextToggleBody;
typedef struct _GtkTextMarkBody GtkTextMarkBody;

/*
 * Declarations for variables shared among the text-related files:
 */

/* In gtktextbtree.c */
extern G_GNUC_INTERNAL const GtkTextLineSegmentClass gtk_text_char_type;
extern G_GNUC_INTERNAL const GtkTextLineSegmentClass gtk_text_toggle_on_type;
extern G_GNUC_INTERNAL const GtkTextLineSegmentClass gtk_text_toggle_off_type;

/* In gtktextmark.c */
extern G_GNUC_INTERNAL const GtkTextLineSegmentClass gtk_text_left_mark_type;
extern G_GNUC_INTERNAL const GtkTextLineSegmentClass gtk_text_right_mark_type;

/* In gtktextchild.c */
extern G_GNUC_INTERNAL const GtkTextLineSegmentClass gtk_text_paintable_type;
extern G_GNUC_INTERNAL const GtkTextLineSegmentClass gtk_text_child_type;

/*
 * UTF 8 Stubs
 */

#define GTK_TEXT_UNKNOWN_CHAR 0xFFFC
#define GTK_TEXT_UNKNOWN_CHAR_UTF8_LEN 3
GDK_AVAILABLE_IN_ALL
const char *gtk_text_unknown_char_utf8_gtk_tests_only (void);
extern const char _gtk_text_unknown_char_utf8[GTK_TEXT_UNKNOWN_CHAR_UTF8_LEN+1];

GDK_AVAILABLE_IN_ALL
gboolean gtk_text_byte_begins_utf8_char (const char *byte);

G_END_DECLS


