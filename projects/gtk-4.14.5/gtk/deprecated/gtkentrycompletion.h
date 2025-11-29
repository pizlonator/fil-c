/* gtkentrycompletion.h
 * Copyright (C) 2003  Kristian Rietveld  <kris@gtk.org>
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

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gdk/gdk.h>
#include <gtk/deprecated/gtktreemodel.h>
#include <gtk/deprecated/gtkliststore.h>
#include <gtk/deprecated/gtkcellarea.h>
#include <gtk/deprecated/gtktreeviewcolumn.h>
#include <gtk/deprecated/gtktreemodelfilter.h>

G_BEGIN_DECLS

#define GTK_TYPE_ENTRY_COMPLETION            (gtk_entry_completion_get_type ())
#define GTK_ENTRY_COMPLETION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_ENTRY_COMPLETION, GtkEntryCompletion))
#define GTK_IS_ENTRY_COMPLETION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_ENTRY_COMPLETION))

typedef struct _GtkEntryCompletion            GtkEntryCompletion;

/**
 * GtkEntryCompletionMatchFunc:
 * @completion: the `GtkEntryCompletion`
 * @key: the string to match, normalized and case-folded
 * @iter: a `GtkTreeIter` indicating the row to match
 * @user_data: user data given to gtk_entry_completion_set_match_func()
 *
 * A function which decides whether the row indicated by @iter matches
 * a given @key, and should be displayed as a possible completion for @key.
 *
 * Note that @key is normalized and case-folded (see g_utf8_normalize()
 * and g_utf8_casefold()). If this is not appropriate, match functions
 * have access to the unmodified key via
 * `gtk_editable_get_text (GTK_EDITABLE (gtk_entry_completion_get_entry ()))`.
 *
 * Returns: %TRUE if @iter should be displayed as a possible completion
 *   for @key
 */
typedef gboolean (* GtkEntryCompletionMatchFunc) (GtkEntryCompletion *completion,
                                                  const char         *key,
                                                  GtkTreeIter        *iter,
                                                  gpointer            user_data);


GDK_AVAILABLE_IN_ALL
GType               gtk_entry_completion_get_type               (void) G_GNUC_CONST;
GDK_DEPRECATED_IN_4_10
GtkEntryCompletion *gtk_entry_completion_new                    (void);
GDK_DEPRECATED_IN_4_10
GtkEntryCompletion *gtk_entry_completion_new_with_area          (GtkCellArea                 *area);

GDK_DEPRECATED_IN_4_10
GtkWidget          *gtk_entry_completion_get_entry              (GtkEntryCompletion          *completion);

GDK_DEPRECATED_IN_4_10
void                gtk_entry_completion_set_model              (GtkEntryCompletion          *completion,
                                                                 GtkTreeModel                *model);
GDK_DEPRECATED_IN_4_10
GtkTreeModel       *gtk_entry_completion_get_model              (GtkEntryCompletion          *completion);

GDK_DEPRECATED_IN_4_10
void                gtk_entry_completion_set_match_func         (GtkEntryCompletion          *completion,
                                                                 GtkEntryCompletionMatchFunc  func,
                                                                 gpointer                     func_data,
                                                                 GDestroyNotify               func_notify);
GDK_DEPRECATED_IN_4_10
void                gtk_entry_completion_set_minimum_key_length (GtkEntryCompletion          *completion,
                                                                 int                          length);
GDK_DEPRECATED_IN_4_10
int                 gtk_entry_completion_get_minimum_key_length (GtkEntryCompletion          *completion);
GDK_DEPRECATED_IN_4_10
char *             gtk_entry_completion_compute_prefix         (GtkEntryCompletion          *completion,
                                                                 const char                  *key);
GDK_DEPRECATED_IN_4_10
void                gtk_entry_completion_complete               (GtkEntryCompletion          *completion);
GDK_DEPRECATED_IN_4_10
void                gtk_entry_completion_insert_prefix          (GtkEntryCompletion          *completion);

GDK_DEPRECATED_IN_4_10
void                gtk_entry_completion_set_inline_completion  (GtkEntryCompletion          *completion,
                                                                 gboolean                     inline_completion);
GDK_DEPRECATED_IN_4_10
gboolean            gtk_entry_completion_get_inline_completion  (GtkEntryCompletion          *completion);
GDK_DEPRECATED_IN_4_10
void                gtk_entry_completion_set_inline_selection  (GtkEntryCompletion          *completion,
                                                                 gboolean                     inline_selection);
GDK_DEPRECATED_IN_4_10
gboolean            gtk_entry_completion_get_inline_selection  (GtkEntryCompletion          *completion);
GDK_DEPRECATED_IN_4_10
void                gtk_entry_completion_set_popup_completion   (GtkEntryCompletion          *completion,
                                                                 gboolean                     popup_completion);
GDK_DEPRECATED_IN_4_10
gboolean            gtk_entry_completion_get_popup_completion   (GtkEntryCompletion          *completion);
GDK_DEPRECATED_IN_4_10
void                gtk_entry_completion_set_popup_set_width    (GtkEntryCompletion          *completion,
                                                                 gboolean                     popup_set_width);
GDK_DEPRECATED_IN_4_10
gboolean            gtk_entry_completion_get_popup_set_width    (GtkEntryCompletion          *completion);
GDK_DEPRECATED_IN_4_10
void                gtk_entry_completion_set_popup_single_match (GtkEntryCompletion          *completion,
                                                                 gboolean                     popup_single_match);
GDK_DEPRECATED_IN_4_10
gboolean            gtk_entry_completion_get_popup_single_match (GtkEntryCompletion          *completion);

GDK_DEPRECATED_IN_4_10
const char          *gtk_entry_completion_get_completion_prefix (GtkEntryCompletion *completion);
/* convenience */
GDK_DEPRECATED_IN_4_10
void                gtk_entry_completion_set_text_column        (GtkEntryCompletion          *completion,
                                                                 int                          column);
GDK_DEPRECATED_IN_4_10
int                 gtk_entry_completion_get_text_column        (GtkEntryCompletion          *completion);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GtkEntryCompletion, g_object_unref)

G_END_DECLS

