/* gtktreednd.c
 * Copyright (C) 2001  Red Hat, Inc.
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

#include "config.h"
#include <string.h>
#include "gtktreednd.h"

#include "gtkprivate.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * SECTION:gtktreednd
 * @Short_description: Interfaces for drag-and-drop support in GtkTreeView
 * @Title: GtkTreeView drag-and-drop
 *
 * GTK supports Drag-and-Drop in tree views with a high-level and a low-level
 * API.
 *
 * The low-level API consists of the GTK DND API, augmented by some treeview
 * utility functions: gtk_tree_view_set_drag_dest_row(),
 * gtk_tree_view_get_drag_dest_row(), gtk_tree_view_get_dest_row_at_pos(),
 * gtk_tree_view_create_row_drag_icon(), gtk_tree_set_row_drag_data() and
 * gtk_tree_get_row_drag_data(). This API leaves a lot of flexibility, but
 * nothing is done automatically, and implementing advanced features like
 * hover-to-open-rows or autoscrolling on top of this API is a lot of work.
 *
 * On the other hand, if you write to the high-level API, then all the
 * bookkeeping of rows is done for you, as well as things like hover-to-open
 * and auto-scroll, but your models have to implement the
 * `GtkTreeDragSource` and `GtkTreeDragDest` interfaces.
 */

/**
 * GtkTreeDragDest:
 *
 * Interface for Drag-and-Drop destinations in `GtkTreeView`.
 *
 * Deprecated: 4.10: List views use widgets to display their contents.
 *   You can use [class@Gtk.DropTarget] to implement a drop destination
 */

/**
 * GtkTreeDragSource:
 *
 * Interface for Drag-and-Drop destinations in `GtkTreeView`.
 *
 * Deprecated: 4.10: List views use widgets to display their contents.
 *   You can use [class@Gtk.DragSource] to implement a drag source
 */

GType
gtk_tree_drag_source_get_type (void)
{
  static GType our_type = 0;

  if (!our_type)
    {
      const GTypeInfo our_info =
      {
        sizeof (GtkTreeDragSourceIface), /* class_size */
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	NULL,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	0,
	0,              /* n_preallocs */
	NULL
      };

      our_type = g_type_register_static (G_TYPE_INTERFACE,
					 I_("GtkTreeDragSource"),
					 &our_info, 0);
    }

  return our_type;
}


GType
gtk_tree_drag_dest_get_type (void)
{
  static GType our_type = 0;

  if (!our_type)
    {
      const GTypeInfo our_info =
      {
        sizeof (GtkTreeDragDestIface), /* class_size */
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	NULL,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	0,
	0,              /* n_preallocs */
	NULL
      };

      our_type = g_type_register_static (G_TYPE_INTERFACE, I_("GtkTreeDragDest"), &our_info, 0);
    }

  return our_type;
}

/**
 * gtk_tree_drag_source_row_draggable:
 * @drag_source: a `GtkTreeDragSource`
 * @path: row on which user is initiating a drag
 *
 * Asks the `GtkTreeDragSource` whether a particular row can be used as
 * the source of a DND operation. If the source doesn’t implement
 * this interface, the row is assumed draggable.
 *
 * Returns: %TRUE if the row can be dragged
 *
 * Deprecated: 4.10: Use list models instead
 **/
gboolean
gtk_tree_drag_source_row_draggable (GtkTreeDragSource *drag_source,
                                    GtkTreePath       *path)
{
  GtkTreeDragSourceIface *iface = GTK_TREE_DRAG_SOURCE_GET_IFACE (drag_source);

  g_return_val_if_fail (path != NULL, FALSE);

  if (iface->row_draggable)
    return (* iface->row_draggable) (drag_source, path);
  else
    return TRUE;
    /* Returning TRUE if row_draggable is not implemented is a fallback.
       Interface implementations such as GtkTreeStore and GtkListStore really should
       implement row_draggable. */
}


/**
 * gtk_tree_drag_source_drag_data_delete:
 * @drag_source: a `GtkTreeDragSource`
 * @path: row that was being dragged
 *
 * Asks the `GtkTreeDragSource` to delete the row at @path, because
 * it was moved somewhere else via drag-and-drop. Returns %FALSE
 * if the deletion fails because @path no longer exists, or for
 * some model-specific reason. Should robustly handle a @path no
 * longer found in the model!
 *
 * Returns: %TRUE if the row was successfully deleted
 *
 * Deprecated: 4.10: Use list models instead
 **/
gboolean
gtk_tree_drag_source_drag_data_delete (GtkTreeDragSource *drag_source,
                                       GtkTreePath       *path)
{
  GtkTreeDragSourceIface *iface = GTK_TREE_DRAG_SOURCE_GET_IFACE (drag_source);

  g_return_val_if_fail (iface->drag_data_delete != NULL, FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  return (* iface->drag_data_delete) (drag_source, path);
}

/**
 * gtk_tree_drag_source_drag_data_get:
 * @drag_source: a `GtkTreeDragSource`
 * @path: row that was dragged
 *
 * Asks the `GtkTreeDragSource` to return a `GdkContentProvider` representing
 * the row at @path. Should robustly handle a @path no
 * longer found in the model!
 *
 * Returns: (nullable) (transfer full): a `GdkContentProvider` for the
 *    given @path
 *
 * Deprecated: 4.10: Use list models instead
 **/
GdkContentProvider *
gtk_tree_drag_source_drag_data_get (GtkTreeDragSource *drag_source,
                                    GtkTreePath       *path)
{
  GtkTreeDragSourceIface *iface = GTK_TREE_DRAG_SOURCE_GET_IFACE (drag_source);

  g_return_val_if_fail (iface->drag_data_get != NULL, FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  return (* iface->drag_data_get) (drag_source, path);
}

/**
 * gtk_tree_drag_dest_drag_data_received:
 * @drag_dest: a `GtkTreeDragDest`
 * @dest: row to drop in front of
 * @value: data to drop
 *
 * Asks the `GtkTreeDragDest` to insert a row before the path @dest,
 * deriving the contents of the row from @value. If @dest is
 * outside the tree so that inserting before it is impossible, %FALSE
 * will be returned. Also, %FALSE may be returned if the new row is
 * not created for some model-specific reason.  Should robustly handle
 * a @dest no longer found in the model!
 *
 * Returns: whether a new row was created before position @dest
 *
 * Deprecated: 4.10: Use list models instead
 **/
gboolean
gtk_tree_drag_dest_drag_data_received (GtkTreeDragDest  *drag_dest,
                                       GtkTreePath      *dest,
                                       const GValue     *value)
{
  GtkTreeDragDestIface *iface = GTK_TREE_DRAG_DEST_GET_IFACE (drag_dest);

  g_return_val_if_fail (iface->drag_data_received != NULL, FALSE);
  g_return_val_if_fail (dest != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  return (* iface->drag_data_received) (drag_dest, dest, value);
}


/**
 * gtk_tree_drag_dest_row_drop_possible:
 * @drag_dest: a `GtkTreeDragDest`
 * @dest_path: destination row
 * @value: the data being dropped
 *
 * Determines whether a drop is possible before the given @dest_path,
 * at the same depth as @dest_path. i.e., can we drop the data in
 * @value at that location. @dest_path does not have to
 * exist; the return value will almost certainly be %FALSE if the
 * parent of @dest_path doesn’t exist, though.
 *
 * Returns: %TRUE if a drop is possible before @dest_path
 *
 * Deprecated: 4.10: Use list models instead
 **/
gboolean
gtk_tree_drag_dest_row_drop_possible (GtkTreeDragDest   *drag_dest,
                                      GtkTreePath       *dest_path,
				      const GValue      *value)
{
  GtkTreeDragDestIface *iface = GTK_TREE_DRAG_DEST_GET_IFACE (drag_dest);

  g_return_val_if_fail (iface->row_drop_possible != NULL, FALSE);
  g_return_val_if_fail (dest_path != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  return (* iface->row_drop_possible) (drag_dest, dest_path, value);
}

typedef struct _GtkTreeRowData GtkTreeRowData;

struct _GtkTreeRowData
{
  GtkTreeModel *model;
  char path[4];
};

static GtkTreeRowData *
gtk_tree_row_data_copy (GtkTreeRowData *src)
{
  return g_memdup2 (src, sizeof (GtkTreeRowData) + strlen (src->path) + 1 -
    (sizeof (GtkTreeRowData) - G_STRUCT_OFFSET (GtkTreeRowData, path)));
}

G_DEFINE_BOXED_TYPE (GtkTreeRowData, gtk_tree_row_data,
                     gtk_tree_row_data_copy,
                     g_free)

/**
 * gtk_tree_create_row_drag_content:
 * @tree_model: a `GtkTreeModel`
 * @path: a row in @tree_model
 *
 * Creates a content provider for dragging @path from @tree_model.
 *
 * Returns: (transfer full): a new `GdkContentProvider`
 *
 * Deprecated: 4.10: Use list models instead
 */
GdkContentProvider *
gtk_tree_create_row_drag_content (GtkTreeModel *tree_model,
			          GtkTreePath  *path)
{
  GdkContentProvider *content;
  GtkTreeRowData *trd;
  char *path_str;
  int len;
  int struct_size;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  path_str = gtk_tree_path_to_string (path);

  len = strlen (path_str);

  /* the old allocate-end-of-struct-to-hold-string trick */
  struct_size = sizeof (GtkTreeRowData) + len + 1 -
    (sizeof (GtkTreeRowData) - G_STRUCT_OFFSET (GtkTreeRowData, path));

  trd = g_malloc (struct_size);

  strcpy (trd->path, path_str);

  g_free (path_str);

  trd->model = tree_model;

  content = gdk_content_provider_new_typed (GTK_TYPE_TREE_ROW_DATA, trd);

  g_free (trd);

  return content;
}

/**
 * gtk_tree_get_row_drag_data:
 * @value: a `GValue`
 * @tree_model: (nullable) (optional) (transfer none) (out): a `GtkTreeModel`
 * @path: (nullable) (optional) (out): row in @tree_model
 *
 * Obtains a @tree_model and @path from value of target type
 * %GTK_TYPE_TREE_ROW_DATA.
 *
 * The returned path must be freed with gtk_tree_path_free().
 *
 * Returns: %TRUE if @selection_data had target type %GTK_TYPE_TREE_ROW_DATA
 *  is otherwise valid
 *
 * Deprecated: 4.10: Use list models instead
 **/
gboolean
gtk_tree_get_row_drag_data (const GValue  *value,
			    GtkTreeModel **tree_model,
			    GtkTreePath  **path)
{
  GtkTreeRowData *trd;

  g_return_val_if_fail (value != NULL, FALSE);

  if (tree_model)
    *tree_model = NULL;

  if (path)
    *path = NULL;

  if (!G_VALUE_HOLDS (value, GTK_TYPE_TREE_ROW_DATA))
    return FALSE;

  trd = g_value_get_boxed (value);
  if (trd == NULL)
    return FALSE;

  if (tree_model)
    *tree_model = trd->model;

  if (path)
    *path = gtk_tree_path_new_from_string (trd->path);

  return TRUE;
}
