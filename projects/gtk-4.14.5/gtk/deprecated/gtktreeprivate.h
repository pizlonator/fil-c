/* gtktreeprivate.h
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
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


#include <gtk/deprecated/gtktreeview.h>
#include <gtk/deprecated/gtktreeselection.h>
#include <gtk/deprecated/gtktreerbtreeprivate.h>

G_BEGIN_DECLS

#define TREE_VIEW_DRAG_WIDTH 6

typedef enum
{
  GTK_TREE_SELECT_MODE_TOGGLE = 1 << 0,
  GTK_TREE_SELECT_MODE_EXTEND = 1 << 1
}
GtkTreeSelectMode;

/* functions that shouldn't be exported */
void         _gtk_tree_selection_internal_select_node (GtkTreeSelection  *selection,
						       GtkTreeRBNode     *node,
						       GtkTreeRBTree     *tree,
						       GtkTreePath       *path,
                                                       GtkTreeSelectMode  mode,
						       gboolean           override_browse_mode);
void         _gtk_tree_selection_emit_changed         (GtkTreeSelection  *selection);
gboolean     _gtk_tree_view_find_node                 (GtkTreeView       *tree_view,
						       GtkTreePath       *path,
						       GtkTreeRBTree    **tree,
						       GtkTreeRBNode    **node);
gboolean     _gtk_tree_view_get_cursor_node           (GtkTreeView       *tree_view,
						       GtkTreeRBTree    **tree,
						       GtkTreeRBNode    **node);
GtkTreePath *_gtk_tree_path_new_from_rbtree           (GtkTreeRBTree     *tree,
						       GtkTreeRBNode     *node);

void         _gtk_tree_view_add_editable              (GtkTreeView       *tree_view,
                                                       GtkTreeViewColumn *column,
                                                       GtkTreePath       *path,
                                                       GtkCellEditable   *cell_editable,
                                                       GdkRectangle      *cell_area);
void         _gtk_tree_view_remove_editable           (GtkTreeView       *tree_view,
                                                       GtkTreeViewColumn *column,
                                                       GtkCellEditable   *cell_editable);

void       _gtk_tree_view_install_mark_rows_col_dirty (GtkTreeView *tree_view,
						       gboolean     install_handler);
void         _gtk_tree_view_column_autosize           (GtkTreeView       *tree_view,
						       GtkTreeViewColumn *column);

void         _gtk_tree_view_get_row_separator_func    (GtkTreeView                 *tree_view,
						       GtkTreeViewRowSeparatorFunc *func,
						       gpointer                    *data);
GtkTreePath *_gtk_tree_view_get_anchor_path           (GtkTreeView                 *tree_view);
void         _gtk_tree_view_set_anchor_path           (GtkTreeView                 *tree_view,
						       GtkTreePath                 *anchor_path);
GtkTreeRBTree *    _gtk_tree_view_get_rbtree          (GtkTreeView                 *tree_view);

GtkTreeViewColumn *_gtk_tree_view_get_focus_column    (GtkTreeView                 *tree_view);
void               _gtk_tree_view_set_focus_column    (GtkTreeView                 *tree_view,
						       GtkTreeViewColumn           *column);

GtkTreeSelection* _gtk_tree_selection_new                (void);
GtkTreeSelection* _gtk_tree_selection_new_with_tree_view (GtkTreeView      *tree_view);
void              _gtk_tree_selection_set_tree_view      (GtkTreeSelection *selection,
                                                          GtkTreeView      *tree_view);
gboolean          _gtk_tree_selection_row_is_selectable  (GtkTreeSelection *selection,
							  GtkTreeRBNode    *node,
							  GtkTreePath      *path);


void _gtk_tree_view_column_realize_button   (GtkTreeViewColumn *column);

void _gtk_tree_view_column_set_tree_view    (GtkTreeViewColumn *column,
					     GtkTreeView       *tree_view);
int _gtk_tree_view_column_request_width     (GtkTreeViewColumn *tree_column);
void _gtk_tree_view_column_allocate         (GtkTreeViewColumn *tree_column,
					     int                x_offset,
					     int                width,
					     int                height);
void _gtk_tree_view_column_unset_model      (GtkTreeViewColumn *column,
					     GtkTreeModel      *old_model);
void _gtk_tree_view_column_unset_tree_view  (GtkTreeViewColumn *column);
void _gtk_tree_view_column_start_drag       (GtkTreeView       *tree_view,
					     GtkTreeViewColumn *column,
                                             GdkDevice         *device);
gboolean _gtk_tree_view_column_cell_event   (GtkTreeViewColumn  *tree_column,
					     GdkEvent           *event,
					     const GdkRectangle *cell_area,
					     guint               flags);
gboolean          _gtk_tree_view_column_has_editable_cell(GtkTreeViewColumn  *column);
GtkCellRenderer  *_gtk_tree_view_column_get_edited_cell  (GtkTreeViewColumn  *column);
GtkCellRenderer  *_gtk_tree_view_column_get_cell_at_pos  (GtkTreeViewColumn  *column,
                                                          GdkRectangle       *cell_area,
                                                          GdkRectangle       *background_area,
                                                          int                 x,
                                                          int                 y);
gboolean          _gtk_tree_view_column_is_blank_at_pos  (GtkTreeViewColumn  *column,
                                                          GdkRectangle       *cell_area,
                                                          GdkRectangle       *background_area,
                                                          int                 x,
                                                          int                 y);

void		  gtk_tree_view_column_cell_snapshot     (GtkTreeViewColumn  *tree_column,
							  GtkSnapshot        *snapshot,
							  const GdkRectangle *background_area,
							  const GdkRectangle *cell_area,
							  guint               flags,
                                                          gboolean            draw_focus);
void		  _gtk_tree_view_column_cell_set_dirty	 (GtkTreeViewColumn  *tree_column,
							  gboolean            install_handler);
gboolean          _gtk_tree_view_column_cell_get_dirty   (GtkTreeViewColumn  *tree_column);

void              _gtk_tree_view_column_push_padding          (GtkTreeViewColumn  *column,
							       int                 padding);
int               _gtk_tree_view_column_get_requested_width   (GtkTreeViewColumn  *column);
int               _gtk_tree_view_column_get_drag_x            (GtkTreeViewColumn  *column);
GtkCellAreaContext *_gtk_tree_view_column_get_context         (GtkTreeViewColumn  *column);
gboolean         _gtk_tree_view_column_coords_in_resize_rect  (GtkTreeViewColumn *column,
                                                               double             x,
                                                               double             y);


G_END_DECLS



