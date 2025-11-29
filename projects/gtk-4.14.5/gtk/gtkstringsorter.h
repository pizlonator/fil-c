/*
 * Copyright © 2019 Matthias Clasen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkexpression.h>
#include <gtk/gtksorter.h>

G_BEGIN_DECLS

#define GTK_TYPE_STRING_SORTER             (gtk_string_sorter_get_type ())
GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkStringSorter, gtk_string_sorter, GTK, STRING_SORTER, GtkSorter)

GDK_AVAILABLE_IN_ALL
GtkStringSorter *       gtk_string_sorter_new                   (GtkExpression          *expression);

GDK_AVAILABLE_IN_ALL
GtkExpression *         gtk_string_sorter_get_expression        (GtkStringSorter        *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_string_sorter_set_expression        (GtkStringSorter        *self,
                                                                 GtkExpression          *expression);
GDK_AVAILABLE_IN_ALL
gboolean                gtk_string_sorter_get_ignore_case       (GtkStringSorter        *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_string_sorter_set_ignore_case       (GtkStringSorter        *self,
                                                                 gboolean                ignore_case);

/**
 * GtkCollation:
 * @GTK_COLLATION_NONE: Don't do any collation
 * @GTK_COLLATION_UNICODE: Use [func@GLib.utf8_collate_key]
 * @GTK_COLLATION_FILENAME: Use [func@GLib.utf8_collate_key_for_filename]
 *
 * Describes how a [class@Gtk.StringSorter] turns strings into sort keys to
 * compare them.
 *
 * Note that the result of sorting will in general depend on the current locale
 * unless the mode is @GTK_COLLATION_NONE.
 *
 * Since: 4.10
 */
typedef enum
{
  GTK_COLLATION_NONE,
  GTK_COLLATION_UNICODE,
  GTK_COLLATION_FILENAME
} GtkCollation;

GDK_AVAILABLE_IN_4_10
void                    gtk_string_sorter_set_collation         (GtkStringSorter        *self,
                                                                 GtkCollation            collation);

GDK_AVAILABLE_IN_4_10
GtkCollation            gtk_string_sorter_get_collation         (GtkStringSorter        *self);

G_END_DECLS

