/* GTK - The GIMP Toolkit
 * gtkfilesystemmodelprivate.h: GtkTreeModel wrapping a GtkFileSystem
 * Copyright (C) 2003, Red Hat, Inc.
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

#include <gio/gio.h>
#include <gtk/gtkfilefilter.h>
#include <gtk/deprecated/gtkfilechooser.h>

G_BEGIN_DECLS

#define GTK_TYPE_FILE_SYSTEM_MODEL (gtk_file_system_model_get_type ())
G_DECLARE_FINAL_TYPE (GtkFileSystemModel, gtk_file_system_model, GTK, FILE_SYSTEM_MODEL, GObject)

GtkFileSystemModel *_gtk_file_system_model_new              (void);
GtkFileSystemModel *_gtk_file_system_model_new_for_directory(GFile              *dir,
                                                             const char         *attributes);
GFile *             _gtk_file_system_model_get_directory    (GtkFileSystemModel *model);
GCancellable *      _gtk_file_system_model_get_cancellable  (GtkFileSystemModel *model);
GFileInfo *         _gtk_file_system_model_get_info_for_file(GtkFileSystemModel *model,
							     GFile              *file);

void                _gtk_file_system_model_add_and_query_file  (GtkFileSystemModel *model,
                                                                GFile              *file,
                                                                const char         *attributes);
void                _gtk_file_system_model_add_and_query_files (GtkFileSystemModel *model,
                                                                GList              *files,
                                                                const char         *attributes);
void                _gtk_file_system_model_update_files     (GtkFileSystemModel *model,
                                                             GList              *files,
                                                             GList              *infos);

void                _gtk_file_system_model_set_show_hidden  (GtkFileSystemModel *model,
							     gboolean            show_hidden);
void                _gtk_file_system_model_set_show_folders (GtkFileSystemModel *model,
							     gboolean            show_folders);
void                _gtk_file_system_model_set_show_files   (GtkFileSystemModel *model,
							     gboolean            show_files);
void                _gtk_file_system_model_set_filter_folders (GtkFileSystemModel *model,
							     gboolean            show_folders);

void                _gtk_file_system_model_set_filter       (GtkFileSystemModel *model,
                                                             GtkFileFilter      *filter);

void                _gtk_file_system_model_set_can_select_files (GtkFileSystemModel   *model,
                                                                 gboolean              can_select);

G_END_DECLS

