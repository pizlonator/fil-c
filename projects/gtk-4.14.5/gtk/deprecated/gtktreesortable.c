/* gtktreesortable.c
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


#include "config.h"
#include "gtktreesortable.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"


G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * GtkTreeSortable:
 *
 * The interface for sortable models used by GtkTreeView
 *
 * `GtkTreeSortable` is an interface to be implemented by tree models which
 * support sorting. The `GtkTreeView` uses the methods provided by this interface
 * to sort the model.
 *
 * Deprecated: 4.10: There is no replacement for this interface. You should
 *   use [class@Gtk.SortListModel] to wrap your list model instead
 */


static void gtk_tree_sortable_base_init (gpointer g_class);

GType
gtk_tree_sortable_get_type (void)
{
  static GType tree_sortable_type = 0;

  if (! tree_sortable_type)
    {
      const GTypeInfo tree_sortable_info =
      {
	sizeof (GtkTreeSortableIface), /* class_size */
	gtk_tree_sortable_base_init,   /* base_init */
	NULL,		/* base_finalize */
	NULL,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	0,
	0,
	NULL
      };

      tree_sortable_type =
	g_type_register_static (G_TYPE_INTERFACE, I_("GtkTreeSortable"),
				&tree_sortable_info, 0);

      g_type_interface_add_prerequisite (tree_sortable_type, GTK_TYPE_TREE_MODEL);
    }

  return tree_sortable_type;
}

static void
gtk_tree_sortable_base_init (gpointer g_class)
{
  static gboolean initialized = FALSE;

  if (! initialized)
    {
      /**
       * GtkTreeSortable::sort-column-changed:
       * @sortable: the object on which the signal is emitted
       *
       * The ::sort-column-changed signal is emitted when the sort column
       * or sort order of @sortable is changed. The signal is emitted before
       * the contents of @sortable are resorted.
       */
      g_signal_new (I_("sort-column-changed"),
                    GTK_TYPE_TREE_SORTABLE,
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GtkTreeSortableIface, sort_column_changed),
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE, 0);
      initialized = TRUE;
    }
}

/**
 * gtk_tree_sortable_sort_column_changed:
 * @sortable: A `GtkTreeSortable`
 *
 * Emits a `GtkTreeSortable::sort-column-changed` signal on @sortable.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_sortable_sort_column_changed (GtkTreeSortable *sortable)
{
  g_return_if_fail (GTK_IS_TREE_SORTABLE (sortable));

  g_signal_emit_by_name (sortable, "sort-column-changed");
}

/**
 * gtk_tree_sortable_get_sort_column_id:
 * @sortable: A `GtkTreeSortable`
 * @sort_column_id: (out): The sort column id to be filled in
 * @order: (out): The `GtkSortType` to be filled in
 *
 * Fills in @sort_column_id and @order with the current sort column and the
 * order. It returns %TRUE unless the @sort_column_id is
 * %GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID or
 * %GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID.
 *
 * Returns: %TRUE if the sort column is not one of the special sort
 *   column ids.
 *
 * Deprecated: 4.10
 **/
gboolean
gtk_tree_sortable_get_sort_column_id (GtkTreeSortable  *sortable,
				      int              *sort_column_id,
				      GtkSortType      *order)
{
  GtkTreeSortableIface *iface;

  g_return_val_if_fail (GTK_IS_TREE_SORTABLE (sortable), FALSE);

  iface = GTK_TREE_SORTABLE_GET_IFACE (sortable);

  g_return_val_if_fail (iface != NULL, FALSE);
  g_return_val_if_fail (iface->get_sort_column_id != NULL, FALSE);

  return (* iface->get_sort_column_id) (sortable, sort_column_id, order);
}

/**
 * gtk_tree_sortable_set_sort_column_id:
 * @sortable: A `GtkTreeSortable`
 * @sort_column_id: the sort column id to set
 * @order: The sort order of the column
 *
 * Sets the current sort column to be @sort_column_id. The @sortable will
 * resort itself to reflect this change, after emitting a
 * `GtkTreeSortable::sort-column-changed` signal. @sort_column_id may either be
 * a regular column id, or one of the following special values:
 *
 * - %GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID: the default sort function
 *   will be used, if it is set
 *
 * - %GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID: no sorting will occur
 *
 * Deprecated: 4.10
 */
void
gtk_tree_sortable_set_sort_column_id (GtkTreeSortable  *sortable,
				      int               sort_column_id,
				      GtkSortType       order)
{
  GtkTreeSortableIface *iface;

  g_return_if_fail (GTK_IS_TREE_SORTABLE (sortable));

  iface = GTK_TREE_SORTABLE_GET_IFACE (sortable);

  g_return_if_fail (iface != NULL);
  g_return_if_fail (iface->set_sort_column_id != NULL);

  (* iface->set_sort_column_id) (sortable, sort_column_id, order);
}

/**
 * gtk_tree_sortable_set_sort_func:
 * @sortable: A `GtkTreeSortable`
 * @sort_column_id: the sort column id to set the function for
 * @sort_func: The comparison function
 * @user_data: (closure): User data to pass to @sort_func
 * @destroy: (nullable): Destroy notifier of @user_data
 *
 * Sets the comparison function used when sorting to be @sort_func. If the
 * current sort column id of @sortable is the same as @sort_column_id, then
 * the model will sort using this function.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_sortable_set_sort_func (GtkTreeSortable        *sortable,
				 int                     sort_column_id,
				 GtkTreeIterCompareFunc  sort_func,
				 gpointer                user_data,
				 GDestroyNotify          destroy)
{
  GtkTreeSortableIface *iface;

  g_return_if_fail (GTK_IS_TREE_SORTABLE (sortable));
  g_return_if_fail (sort_func != NULL);

  iface = GTK_TREE_SORTABLE_GET_IFACE (sortable);

  g_return_if_fail (iface != NULL);
  g_return_if_fail (iface->set_sort_func != NULL);
  g_return_if_fail (sort_column_id >= 0);

  (* iface->set_sort_func) (sortable, sort_column_id, sort_func, user_data, destroy);
}

/**
 * gtk_tree_sortable_set_default_sort_func:
 * @sortable: A `GtkTreeSortable`
 * @sort_func: The comparison function
 * @user_data: (closure): User data to pass to @sort_func
 * @destroy: (nullable): Destroy notifier of @user_data
 *
 * Sets the default comparison function used when sorting to be @sort_func.
 * If the current sort column id of @sortable is
 * %GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, then the model will sort using
 * this function.
 *
 * If @sort_func is %NULL, then there will be no default comparison function.
 * This means that once the model  has been sorted, it can’t go back to the
 * default state. In this case, when the current sort column id of @sortable
 * is %GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, the model will be unsorted.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_sortable_set_default_sort_func (GtkTreeSortable        *sortable,
					 GtkTreeIterCompareFunc  sort_func,
					 gpointer                user_data,
					 GDestroyNotify          destroy)
{
  GtkTreeSortableIface *iface;

  g_return_if_fail (GTK_IS_TREE_SORTABLE (sortable));

  iface = GTK_TREE_SORTABLE_GET_IFACE (sortable);

  g_return_if_fail (iface != NULL);
  g_return_if_fail (iface->set_default_sort_func != NULL);

  (* iface->set_default_sort_func) (sortable, sort_func, user_data, destroy);
}

/**
 * gtk_tree_sortable_has_default_sort_func:
 * @sortable: A `GtkTreeSortable`
 *
 * Returns %TRUE if the model has a default sort function. This is used
 * primarily by GtkTreeViewColumns in order to determine if a model can
 * go back to the default state, or not.
 *
 * Returns: %TRUE, if the model has a default sort function
 *
 * Deprecated: 4.10
 */
gboolean
gtk_tree_sortable_has_default_sort_func (GtkTreeSortable *sortable)
{
  GtkTreeSortableIface *iface;

  g_return_val_if_fail (GTK_IS_TREE_SORTABLE (sortable), FALSE);

  iface = GTK_TREE_SORTABLE_GET_IFACE (sortable);

  g_return_val_if_fail (iface != NULL, FALSE);
  g_return_val_if_fail (iface->has_default_sort_func != NULL, FALSE);

  return (* iface->has_default_sort_func) (sortable);
}
