/* Copyright (C) 2019 Red Hat, Inc.
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

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define GTK_TYPE_TEXT_HISTORY (gtk_text_history_get_type())

typedef struct _GtkTextHistoryFuncs GtkTextHistoryFuncs;

G_DECLARE_FINAL_TYPE (GtkTextHistory, gtk_text_history, GTK, TEXT_HISTORY, GObject)

struct _GtkTextHistoryFuncs
{
  void (*change_state) (gpointer     funcs_data,
                        gboolean     is_modified,
                        gboolean     can_undo,
                        gboolean     can_redo);
  void (*insert)       (gpointer     funcs_data,
                        guint        begin,
                        guint        end,
                        const char  *text,
                        guint        len);
  void (*delete)       (gpointer     funcs_data,
                        guint        begin,
                        guint        end,
                        const char  *expected_text,
                        guint        len);
  void (*select)       (gpointer     funcs_data,
                        int          selection_insert,
                        int          selection_bound);
};

GtkTextHistory *gtk_text_history_new                       (const GtkTextHistoryFuncs *funcs,
                                                            gpointer                   funcs_data);
void            gtk_text_history_begin_user_action         (GtkTextHistory            *self);
void            gtk_text_history_end_user_action           (GtkTextHistory            *self);
void            gtk_text_history_begin_irreversible_action (GtkTextHistory            *self);
void            gtk_text_history_end_irreversible_action   (GtkTextHistory            *self);
gboolean        gtk_text_history_get_can_undo              (GtkTextHistory            *self);
gboolean        gtk_text_history_get_can_redo              (GtkTextHistory            *self);
void            gtk_text_history_undo                      (GtkTextHistory            *self);
void            gtk_text_history_redo                      (GtkTextHistory            *self);
guint           gtk_text_history_get_max_undo_levels       (GtkTextHistory            *self);
void            gtk_text_history_set_max_undo_levels       (GtkTextHistory            *self,
                                                            guint                      max_undo_levels);
void            gtk_text_history_modified_changed          (GtkTextHistory            *self,
                                                            gboolean                   modified);
void            gtk_text_history_selection_changed         (GtkTextHistory            *self,
                                                            int                        selection_insert,
                                                            int                        selection_bound);
void            gtk_text_history_text_inserted             (GtkTextHistory            *self,
                                                            guint                      position,
                                                            const char                *text,
                                                            int                        len);
void            gtk_text_history_text_deleted              (GtkTextHistory            *self,
                                                            guint                      begin,
                                                            guint                      end,
                                                            const char                *text,
                                                            int                        len);
gboolean        gtk_text_history_get_enabled               (GtkTextHistory            *self);
void            gtk_text_history_set_enabled               (GtkTextHistory            *self,
                                                            gboolean                   enabled);

G_END_DECLS

