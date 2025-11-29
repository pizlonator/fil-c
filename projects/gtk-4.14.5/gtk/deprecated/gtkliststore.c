/* gtkliststore.c
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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <gobject/gvaluecollector.h>
#include "gtktreemodel.h"
#include "gtkliststore.h"
#include "gtktreedatalistprivate.h"
#include "gtktreednd.h"
#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * GtkListStore:
 *
 * A list-like data structure that can be used with the [class@Gtk.TreeView].
 *
 * The `GtkListStore` object is a list model for use with a `GtkTreeView`
 * widget.  It implements the `GtkTreeModel` interface, and consequentialy,
 * can use all of the methods available there.  It also implements the
 * `GtkTreeSortable` interface so it can be sorted by the view.
 * Finally, it also implements the tree
 * [drag](iface.TreeDragSource.html) and [drop](iface.TreeDragDest.html)
 * interfaces.
 *
 * The `GtkListStore` can accept most `GType`s as a column type, though
 * it can’t accept all custom types.  Internally, it will keep a copy of
 * data passed in (such as a string or a boxed pointer).  Columns that
 * accept `GObject`s are handled a little differently.  The
 * `GtkListStore` will keep a reference to the object instead of copying the
 * value.  As a result, if the object is modified, it is up to the
 * application writer to call [method@Gtk.TreeModel.row_changed] to emit the
 * [signal@Gtk.TreeModel::row_changed] signal. This most commonly affects lists
 * with [class@Gdk.Texture]s stored.
 *
 * An example for creating a simple list store:
 *
 * ```c
 * enum {
 *   COLUMN_STRING,
 *   COLUMN_INT,
 *   COLUMN_BOOLEAN,
 *   N_COLUMNS
 * };
 *
 * {
 *   GtkListStore *list_store;
 *   GtkTreePath *path;
 *   GtkTreeIter iter;
 *   int i;
 *
 *   list_store = gtk_list_store_new (N_COLUMNS,
 *                                    G_TYPE_STRING,
 *                                    G_TYPE_INT,
 *                                    G_TYPE_BOOLEAN);
 *
 *   for (i = 0; i < 10; i++)
 *     {
 *       char *some_data;
 *
 *       some_data = get_some_data (i);
 *
 *       // Add a new row to the model
 *       gtk_list_store_append (list_store, &iter);
 *       gtk_list_store_set (list_store, &iter,
 *                           COLUMN_STRING, some_data,
 *                           COLUMN_INT, i,
 *                           COLUMN_BOOLEAN,  FALSE,
 *                           -1);
 *
 *       // As the store will keep a copy of the string internally,
 *       // we free some_data.
 *       g_free (some_data);
 *     }
 *
 *   // Modify a particular row
 *   path = gtk_tree_path_new_from_string ("4");
 *   gtk_tree_model_get_iter (GTK_TREE_MODEL (list_store),
 *                            &iter,
 *                            path);
 *   gtk_tree_path_free (path);
 *   gtk_list_store_set (list_store, &iter,
 *                       COLUMN_BOOLEAN, TRUE,
 *                       -1);
 * }
 * ```
 *
 * `GtkListStore` is deprecated since GTK 4.10, and should not be used in newly
 * written code. You should use [class@Gio.ListStore] instead, and the various
 * list models provided by GTK.
 *
 * ## Performance Considerations
 *
 * Internally, the `GtkListStore` was originally implemented with a linked list
 * with a tail pointer.  As a result, it was fast at data insertion and deletion,
 * and not fast at random data access.  The `GtkListStore` sets the
 * `GTK_TREE_MODEL_ITERS_PERSIST` flag, which means that `GtkTreeIter`s can be
 * cached while the row exists.  Thus, if access to a particular row is needed
 * often and your code is expected to run on older versions of GTK, it is worth
 * keeping the iter around.
 *
 * ## Atomic Operations
 *
 * It is important to note that only the methods
 * gtk_list_store_insert_with_values() and gtk_list_store_insert_with_valuesv()
 * are atomic, in the sense that the row is being appended to the store and the
 * values filled in in a single operation with regard to `GtkTreeModel` signaling.
 * In contrast, using e.g. gtk_list_store_append() and then gtk_list_store_set()
 * will first create a row, which triggers the `GtkTreeModel::row-inserted` signal
 * on `GtkListStore`. The row, however, is still empty, and any signal handler
 * connecting to `GtkTreeModel::row-inserted` on this particular store should be prepared
 * for the situation that the row might be empty. This is especially important
 * if you are wrapping the `GtkListStore` inside a `GtkTreeModel`Filter and are
 * using a `GtkTreeModel`FilterVisibleFunc. Using any of the non-atomic operations
 * to append rows to the `GtkListStore` will cause the
 * `GtkTreeModel`FilterVisibleFunc to be visited with an empty row first; the
 * function must be prepared for that.
 *
 * ## GtkListStore as GtkBuildable
 *
 * The GtkListStore implementation of the [iface@Gtk.Buildable] interface allows
 * to specify the model columns with a `<columns>` element that may contain
 * multiple `<column>` elements, each specifying one model column. The “type”
 * attribute specifies the data type for the column.
 *
 * Additionally, it is possible to specify content for the list store
 * in the UI definition, with the `<data>` element. It can contain multiple
 * `<row>` elements, each specifying to content for one row of the list model.
 * Inside a `<row>`, the `<col>` elements specify the content for individual cells.
 *
 * Note that it is probably more common to define your models in the code,
 * and one might consider it a layering violation to specify the content of
 * a list store in a UI definition, data, not presentation, and common wisdom
 * is to separate the two, as far as possible.
 *
 * An example of a UI Definition fragment for a list store:
 *
 * ```xml
 * <object class="GtkListStore">
 *   <columns>
 *     <column type="gchararray"/>
 *     <column type="gchararray"/>
 *     <column type="gint"/>
 *   </columns>
 *   <data>
 *     <row>
 *       <col id="0">John</col>
 *       <col id="1">Doe</col>
 *       <col id="2">25</col>
 *     </row>
 *     <row>
 *       <col id="0">Johan</col>
 *       <col id="1">Dahlin</col>
 *       <col id="2">50</col>
 *     </row>
 *   </data>
 * </object>
 * ```
 *
 * Deprecated: 4.10: Use [class@Gio.ListStore] instead
 */


struct _GtkListStorePrivate
{
  GtkTreeIterCompareFunc default_sort_func;

  GDestroyNotify default_sort_destroy;
  GList *sort_list;
  GType *column_headers;

  int stamp;
  int n_columns;
  int sort_column_id;
  int length;

  GtkSortType order;

  guint columns_dirty : 1;

  gpointer default_sort_data;
  gpointer seq;         /* head of the list */
};

#define GTK_LIST_STORE_IS_SORTED(list) (((GtkListStore*)(list))->priv->sort_column_id != GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID)
static void         gtk_list_store_tree_model_init (GtkTreeModelIface *iface);
static void         gtk_list_store_drag_source_init(GtkTreeDragSourceIface *iface);
static void         gtk_list_store_drag_dest_init  (GtkTreeDragDestIface   *iface);
static void         gtk_list_store_sortable_init   (GtkTreeSortableIface   *iface);
static void         gtk_list_store_buildable_init  (GtkBuildableIface      *iface);
static void         gtk_list_store_finalize        (GObject           *object);
static GtkTreeModelFlags gtk_list_store_get_flags  (GtkTreeModel      *tree_model);
static int          gtk_list_store_get_n_columns   (GtkTreeModel      *tree_model);
static GType        gtk_list_store_get_column_type (GtkTreeModel      *tree_model,
						    int                index);
static gboolean     gtk_list_store_get_iter        (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    GtkTreePath       *path);
static GtkTreePath *gtk_list_store_get_path        (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static void         gtk_list_store_get_value       (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    int                column,
						    GValue            *value);
static gboolean     gtk_list_store_iter_next       (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static gboolean     gtk_list_store_iter_previous   (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static gboolean     gtk_list_store_iter_children   (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    GtkTreeIter       *parent);
static gboolean     gtk_list_store_iter_has_child  (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static int          gtk_list_store_iter_n_children (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static gboolean     gtk_list_store_iter_nth_child  (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    GtkTreeIter       *parent,
						    int                n);
static gboolean     gtk_list_store_iter_parent     (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    GtkTreeIter       *child);


static void gtk_list_store_set_n_columns   (GtkListStore *list_store,
					    int           n_columns);
static void gtk_list_store_set_column_type (GtkListStore *list_store,
					    int           column,
					    GType         type);

static void gtk_list_store_increment_stamp (GtkListStore *list_store);


/* Drag and Drop */
static gboolean real_gtk_list_store_row_draggable (GtkTreeDragSource *drag_source,
                                                   GtkTreePath       *path);
static gboolean gtk_list_store_drag_data_delete   (GtkTreeDragSource *drag_source,
                                                   GtkTreePath       *path);
static GdkContentProvider *
                gtk_list_store_drag_data_get      (GtkTreeDragSource *drag_source,
                                                   GtkTreePath       *path);
static gboolean gtk_list_store_drag_data_received (GtkTreeDragDest   *drag_dest,
                                                   GtkTreePath       *dest,
                                                   const GValue      *value);
static gboolean gtk_list_store_row_drop_possible  (GtkTreeDragDest   *drag_dest,
                                                   GtkTreePath       *dest_path,
						   const GValue      *value);


/* sortable */
static void     gtk_list_store_sort                  (GtkListStore           *list_store);
static void     gtk_list_store_sort_iter_changed     (GtkListStore           *list_store,
						      GtkTreeIter            *iter,
						      int                     column);
static gboolean gtk_list_store_get_sort_column_id    (GtkTreeSortable        *sortable,
						      int                    *sort_column_id,
						      GtkSortType            *order);
static void     gtk_list_store_set_sort_column_id    (GtkTreeSortable        *sortable,
						      int                     sort_column_id,
						      GtkSortType             order);
static void     gtk_list_store_set_sort_func         (GtkTreeSortable        *sortable,
						      int                     sort_column_id,
						      GtkTreeIterCompareFunc  func,
						      gpointer                data,
						      GDestroyNotify          destroy);
static void     gtk_list_store_set_default_sort_func (GtkTreeSortable        *sortable,
						      GtkTreeIterCompareFunc  func,
						      gpointer                data,
						      GDestroyNotify          destroy);
static gboolean gtk_list_store_has_default_sort_func (GtkTreeSortable        *sortable);


/* buildable */
static gboolean gtk_list_store_buildable_custom_tag_start (GtkBuildable       *buildable,
                                                           GtkBuilder         *builder,
                                                           GObject            *child,
                                                           const char         *tagname,
                                                           GtkBuildableParser *parser,
                                                           gpointer           *data);
static void     gtk_list_store_buildable_custom_tag_end   (GtkBuildable       *buildable,
                                                           GtkBuilder         *builder,
                                                           GObject            *child,
                                                           const char         *tagname,
                                                           gpointer            data);

G_DEFINE_TYPE_WITH_CODE (GtkListStore, gtk_list_store, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GtkListStore)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
						gtk_list_store_tree_model_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
						gtk_list_store_drag_source_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_DEST,
						gtk_list_store_drag_dest_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_SORTABLE,
						gtk_list_store_sortable_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
						gtk_list_store_buildable_init))


static void
gtk_list_store_class_init (GtkListStoreClass *class)
{
  GObjectClass *object_class;

  object_class = (GObjectClass*) class;

  object_class->finalize = gtk_list_store_finalize;
}

static void
gtk_list_store_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = gtk_list_store_get_flags;
  iface->get_n_columns = gtk_list_store_get_n_columns;
  iface->get_column_type = gtk_list_store_get_column_type;
  iface->get_iter = gtk_list_store_get_iter;
  iface->get_path = gtk_list_store_get_path;
  iface->get_value = gtk_list_store_get_value;
  iface->iter_next = gtk_list_store_iter_next;
  iface->iter_previous = gtk_list_store_iter_previous;
  iface->iter_children = gtk_list_store_iter_children;
  iface->iter_has_child = gtk_list_store_iter_has_child;
  iface->iter_n_children = gtk_list_store_iter_n_children;
  iface->iter_nth_child = gtk_list_store_iter_nth_child;
  iface->iter_parent = gtk_list_store_iter_parent;
}

static void
gtk_list_store_drag_source_init (GtkTreeDragSourceIface *iface)
{
  iface->row_draggable = real_gtk_list_store_row_draggable;
  iface->drag_data_delete = gtk_list_store_drag_data_delete;
  iface->drag_data_get = gtk_list_store_drag_data_get;
}

static void
gtk_list_store_drag_dest_init (GtkTreeDragDestIface *iface)
{
  iface->drag_data_received = gtk_list_store_drag_data_received;
  iface->row_drop_possible = gtk_list_store_row_drop_possible;
}

static void
gtk_list_store_sortable_init (GtkTreeSortableIface *iface)
{
  iface->get_sort_column_id = gtk_list_store_get_sort_column_id;
  iface->set_sort_column_id = gtk_list_store_set_sort_column_id;
  iface->set_sort_func = gtk_list_store_set_sort_func;
  iface->set_default_sort_func = gtk_list_store_set_default_sort_func;
  iface->has_default_sort_func = gtk_list_store_has_default_sort_func;
}

void
gtk_list_store_buildable_init (GtkBuildableIface *iface)
{
  iface->custom_tag_start = gtk_list_store_buildable_custom_tag_start;
  iface->custom_tag_end = gtk_list_store_buildable_custom_tag_end;
}

static void
gtk_list_store_init (GtkListStore *list_store)
{
  GtkListStorePrivate *priv;

  list_store->priv = gtk_list_store_get_instance_private (list_store);
  priv = list_store->priv;

  priv->seq = g_sequence_new (NULL);
  priv->sort_list = NULL;
  priv->stamp = g_random_int ();
  priv->sort_column_id = GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID;
  priv->columns_dirty = FALSE;
  priv->length = 0;
}

static gboolean
iter_is_valid (GtkTreeIter  *iter,
               GtkListStore *list_store)
{
  return iter != NULL &&
         iter->user_data != NULL &&
         list_store->priv->stamp == iter->stamp &&
         !g_sequence_iter_is_end (iter->user_data) &&
         g_sequence_iter_get_sequence (iter->user_data) == list_store->priv->seq;
}

/**
 * gtk_list_store_new:
 * @n_columns: number of columns in the list store
 * @...: all `GType` types for the columns, from first to last
 *
 * Creates a new list store.
 *
 * The list store will have @n_columns columns, with each column using
 * the given type passed to this function.
 *
 *
 * Note that only types derived from standard GObject fundamental types
 * are supported.
 *
 * As an example:
 *
 * ```c
 * gtk_list_store_new (3, G_TYPE_INT, G_TYPE_STRING, GDK_TYPE_TEXTURE);
 * ```
 *
 * will create a new `GtkListStore` with three columns, of type `int`,
 * `gchararray` and `GdkTexture`, respectively.
 *
 * Returns: a new `GtkListStore`
 *
 * Deprecated: 4.10: Use [class@Gio.ListStore] instead
 */
GtkListStore *
gtk_list_store_new (int n_columns,
		    ...)
{
  GtkListStore *retval;
  va_list args;
  int i;

  g_return_val_if_fail (n_columns > 0, NULL);

  retval = g_object_new (GTK_TYPE_LIST_STORE, NULL);
  gtk_list_store_set_n_columns (retval, n_columns);

  va_start (args, n_columns);

  for (i = 0; i < n_columns; i++)
    {
      GType type = va_arg (args, GType);
      if (! _gtk_tree_data_list_check_type (type))
        {
          g_warning ("%s: Invalid type %s", G_STRLOC, g_type_name (type));
          g_object_unref (retval);
          va_end (args);

          return NULL;
        }

      gtk_list_store_set_column_type (retval, i, type);
    }

  va_end (args);

  return retval;
}


/**
 * gtk_list_store_newv: (rename-to gtk_list_store_new)
 * @n_columns: number of columns in the list store
 * @types: (array length=n_columns): an array of `GType` types for the columns, from first to last
 *
 * Creates a new `GtkListStore`.
 *
 * This function is meant to be used by language bindings.
 *
 * Returns: (transfer full): a new `GtkListStore`
 *
 * Deprecated: 4.10: Use [class@Gio.ListStore] instead
 **/
GtkListStore *
gtk_list_store_newv (int    n_columns,
		     GType *types)
{
  GtkListStore *retval;
  int i;

  g_return_val_if_fail (n_columns > 0, NULL);

  retval = g_object_new (GTK_TYPE_LIST_STORE, NULL);
  gtk_list_store_set_n_columns (retval, n_columns);

  for (i = 0; i < n_columns; i++)
    {
      if (! _gtk_tree_data_list_check_type (types[i]))
	{
	  g_warning ("%s: Invalid type %s", G_STRLOC, g_type_name (types[i]));
	  g_object_unref (retval);
	  return NULL;
	}

      gtk_list_store_set_column_type (retval, i, types[i]);
    }

  return retval;
}

/**
 * gtk_list_store_set_column_types:
 * @list_store: A `GtkListStore`
 * @n_columns: Number of columns for the list store
 * @types: (array length=n_columns): An array length n of `GType`s
 *
 * Sets the types of the columns of a list store.
 *
 * This function is meant primarily for objects that inherit
 * from `GtkListStore`, and should only be used when constructing
 * a new instance.
 *
 * This function cannot be called after a row has been added, or
 * a method on the `GtkTreeModel` interface is called.
 *
 * Deprecated: 4.10: Use list models
 **/
void
gtk_list_store_set_column_types (GtkListStore *list_store,
				 int           n_columns,
				 GType        *types)
{
  GtkListStorePrivate *priv;
  int i;

  g_return_if_fail (GTK_IS_LIST_STORE (list_store));

  priv = list_store->priv;

  g_return_if_fail (priv->columns_dirty == 0);

  gtk_list_store_set_n_columns (list_store, n_columns);
  for (i = 0; i < n_columns; i++)
    {
      if (! _gtk_tree_data_list_check_type (types[i]))
	{
	  g_warning ("%s: Invalid type %s", G_STRLOC, g_type_name (types[i]));
	  continue;
	}
      gtk_list_store_set_column_type (list_store, i, types[i]);
    }
}

static void
gtk_list_store_set_n_columns (GtkListStore *list_store,
			      int           n_columns)
{
  GtkListStorePrivate *priv = list_store->priv;
  int i;

  if (priv->n_columns == n_columns)
    return;

  priv->column_headers = g_renew (GType, priv->column_headers, n_columns);
  for (i = priv->n_columns; i < n_columns; i++)
    priv->column_headers[i] = G_TYPE_INVALID;
  priv->n_columns = n_columns;

  if (priv->sort_list)
    _gtk_tree_data_list_header_free (priv->sort_list);
  priv->sort_list = _gtk_tree_data_list_header_new (n_columns, priv->column_headers);
}

static void
gtk_list_store_set_column_type (GtkListStore *list_store,
				int           column,
				GType         type)
{
  GtkListStorePrivate *priv = list_store->priv;

  if (!_gtk_tree_data_list_check_type (type))
    {
      g_warning ("%s: Invalid type %s", G_STRLOC, g_type_name (type));
      return;
    }

  priv->column_headers[column] = type;
}

static void
gtk_list_store_finalize (GObject *object)
{
  GtkListStore *list_store = GTK_LIST_STORE (object);
  GtkListStorePrivate *priv = list_store->priv;

  g_sequence_foreach (priv->seq,
		      (GFunc) _gtk_tree_data_list_free, priv->column_headers);

  g_sequence_free (priv->seq);

  _gtk_tree_data_list_header_free (priv->sort_list);
  g_free (priv->column_headers);

  if (priv->default_sort_destroy)
    {
      GDestroyNotify d = priv->default_sort_destroy;

      priv->default_sort_destroy = NULL;
      d (priv->default_sort_data);
      priv->default_sort_data = NULL;
    }

  G_OBJECT_CLASS (gtk_list_store_parent_class)->finalize (object);
}

/* Fulfill the GtkTreeModel requirements */
static GtkTreeModelFlags
gtk_list_store_get_flags (GtkTreeModel *tree_model)
{
  return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}

static int
gtk_list_store_get_n_columns (GtkTreeModel *tree_model)
{
  GtkListStore *list_store = GTK_LIST_STORE (tree_model);
  GtkListStorePrivate *priv = list_store->priv;

  priv->columns_dirty = TRUE;

  return priv->n_columns;
}

static GType
gtk_list_store_get_column_type (GtkTreeModel *tree_model,
				int           index)
{
  GtkListStore *list_store = GTK_LIST_STORE (tree_model);
  GtkListStorePrivate *priv = list_store->priv;

  g_return_val_if_fail (index < priv->n_columns, G_TYPE_INVALID);

  priv->columns_dirty = TRUE;

  return priv->column_headers[index];
}

static gboolean
gtk_list_store_get_iter (GtkTreeModel *tree_model,
			 GtkTreeIter  *iter,
			 GtkTreePath  *path)
{
  GtkListStore *list_store = GTK_LIST_STORE (tree_model);
  GtkListStorePrivate *priv = list_store->priv;
  GSequence *seq;
  int i;

  priv->columns_dirty = TRUE;

  seq = priv->seq;

  i = gtk_tree_path_get_indices (path)[0];

  if (i >= g_sequence_get_length (seq))
    {
      iter->stamp = 0;
      return FALSE;
    }

  iter->stamp = priv->stamp;
  iter->user_data = g_sequence_get_iter_at_pos (seq, i);

  return TRUE;
}

static GtkTreePath *
gtk_list_store_get_path (GtkTreeModel *tree_model,
			 GtkTreeIter  *iter)
{
  GtkListStore *list_store = GTK_LIST_STORE (tree_model);
  GtkListStorePrivate *priv = list_store->priv;
  GtkTreePath *path;

  g_return_val_if_fail (iter->stamp == priv->stamp, NULL);

  if (g_sequence_iter_is_end (iter->user_data))
    return NULL;

  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, g_sequence_iter_get_position (iter->user_data));

  return path;
}

static void
gtk_list_store_get_value (GtkTreeModel *tree_model,
			  GtkTreeIter  *iter,
			  int           column,
			  GValue       *value)
{
  GtkListStore *list_store = GTK_LIST_STORE (tree_model);
  GtkListStorePrivate *priv = list_store->priv;
  GtkTreeDataList *list;
  int tmp_column = column;

  g_return_if_fail (column < priv->n_columns);
  g_return_if_fail (iter_is_valid (iter, list_store));

  list = g_sequence_get (iter->user_data);

  while (tmp_column-- > 0 && list)
    list = list->next;

  if (list == NULL)
    g_value_init (value, priv->column_headers[column]);
  else
    _gtk_tree_data_list_node_to_value (list,
				       priv->column_headers[column],
				       value);
}

static gboolean
gtk_list_store_iter_next (GtkTreeModel  *tree_model,
			  GtkTreeIter   *iter)
{
  GtkListStore *list_store = GTK_LIST_STORE (tree_model);
  GtkListStorePrivate *priv = list_store->priv;
  gboolean retval;

  g_return_val_if_fail (priv->stamp == iter->stamp, FALSE);
  iter->user_data = g_sequence_iter_next (iter->user_data);

  retval = g_sequence_iter_is_end (iter->user_data);
  if (retval)
    iter->stamp = 0;

  return !retval;
}

static gboolean
gtk_list_store_iter_previous (GtkTreeModel *tree_model,
                              GtkTreeIter  *iter)
{
  GtkListStore *list_store = GTK_LIST_STORE (tree_model);
  GtkListStorePrivate *priv = list_store->priv;

  g_return_val_if_fail (priv->stamp == iter->stamp, FALSE);

  if (g_sequence_iter_is_begin (iter->user_data))
    {
      iter->stamp = 0;
      return FALSE;
    }

  iter->user_data = g_sequence_iter_prev (iter->user_data);

  return TRUE;
}

static gboolean
gtk_list_store_iter_children (GtkTreeModel *tree_model,
			      GtkTreeIter  *iter,
			      GtkTreeIter  *parent)
{
  GtkListStore *list_store = (GtkListStore *) tree_model;
  GtkListStorePrivate *priv = list_store->priv;

  /* this is a list, nodes have no children */
  if (parent)
    {
      iter->stamp = 0;
      return FALSE;
    }

  if (g_sequence_get_length (priv->seq) > 0)
    {
      iter->stamp = priv->stamp;
      iter->user_data = g_sequence_get_begin_iter (priv->seq);
      return TRUE;
    }
  else
    {
      iter->stamp = 0;
      return FALSE;
    }
}

static gboolean
gtk_list_store_iter_has_child (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter)
{
  return FALSE;
}

static int
gtk_list_store_iter_n_children (GtkTreeModel *tree_model,
				GtkTreeIter  *iter)
{
  GtkListStore *list_store = GTK_LIST_STORE (tree_model);
  GtkListStorePrivate *priv = list_store->priv;

  if (iter == NULL)
    return g_sequence_get_length (priv->seq);

  g_return_val_if_fail (priv->stamp == iter->stamp, -1);

  return 0;
}

static gboolean
gtk_list_store_iter_nth_child (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter,
			       GtkTreeIter  *parent,
			       int           n)
{
  GtkListStore *list_store = GTK_LIST_STORE (tree_model);
  GtkListStorePrivate *priv = list_store->priv;
  GSequenceIter *child;

  iter->stamp = 0;

  if (parent)
    return FALSE;

  child = g_sequence_get_iter_at_pos (priv->seq, n);

  if (g_sequence_iter_is_end (child))
    return FALSE;

  iter->stamp = priv->stamp;
  iter->user_data = child;

  return TRUE;
}

static gboolean
gtk_list_store_iter_parent (GtkTreeModel *tree_model,
			    GtkTreeIter  *iter,
			    GtkTreeIter  *child)
{
  iter->stamp = 0;
  return FALSE;
}

static gboolean
gtk_list_store_real_set_value (GtkListStore *list_store,
			       GtkTreeIter  *iter,
			       int           column,
			       GValue       *value,
			       gboolean      sort)
{
  GtkListStorePrivate *priv = list_store->priv;
  GtkTreeDataList *list;
  GtkTreeDataList *prev;
  int old_column = column;
  GValue real_value = G_VALUE_INIT;
  gboolean converted = FALSE;
  gboolean retval = FALSE;

  if (! g_type_is_a (G_VALUE_TYPE (value), priv->column_headers[column]))
    {
      if (! (g_value_type_transformable (G_VALUE_TYPE (value), priv->column_headers[column])))
	{
	  g_warning ("%s: Unable to convert from %s to %s",
		     G_STRLOC,
		     g_type_name (G_VALUE_TYPE (value)),
		     g_type_name (priv->column_headers[column]));
	  return retval;
	}

      g_value_init (&real_value, priv->column_headers[column]);
      if (!g_value_transform (value, &real_value))
	{
	  g_warning ("%s: Unable to make conversion from %s to %s",
		     G_STRLOC,
		     g_type_name (G_VALUE_TYPE (value)),
		     g_type_name (priv->column_headers[column]));
	  g_value_unset (&real_value);
	  return retval;
	}
      converted = TRUE;
    }

  prev = list = g_sequence_get (iter->user_data);

  while (list != NULL)
    {
      if (column == 0)
	{
	  if (converted)
	    _gtk_tree_data_list_value_to_node (list, &real_value);
	  else
	    _gtk_tree_data_list_value_to_node (list, value);
	  retval = TRUE;
	  if (converted)
	    g_value_unset (&real_value);
         if (sort && GTK_LIST_STORE_IS_SORTED (list_store))
            gtk_list_store_sort_iter_changed (list_store, iter, old_column);
	  return retval;
	}

      column--;
      prev = list;
      list = list->next;
    }

  if (g_sequence_get (iter->user_data) == NULL)
    {
      list = _gtk_tree_data_list_alloc();
      g_sequence_set (iter->user_data, list);
      list->next = NULL;
    }
  else
    {
      list = prev->next = _gtk_tree_data_list_alloc ();
      list->next = NULL;
    }

  while (column != 0)
    {
      list->next = _gtk_tree_data_list_alloc ();
      list = list->next;
      list->next = NULL;
      column --;
    }

  if (converted)
    _gtk_tree_data_list_value_to_node (list, &real_value);
  else
    _gtk_tree_data_list_value_to_node (list, value);

  retval = TRUE;
  if (converted)
    g_value_unset (&real_value);

  if (sort && GTK_LIST_STORE_IS_SORTED (list_store))
    gtk_list_store_sort_iter_changed (list_store, iter, old_column);

  return retval;
}


/**
 * gtk_list_store_set_value:
 * @list_store: A `GtkListStore`
 * @iter: A valid `GtkTreeIter` for the row being modified
 * @column: column number to modify
 * @value: new value for the cell
 *
 * Sets the data in the cell specified by @iter and @column.
 * The type of @value must be convertible to the type of the
 * column.
 *
 * Deprecated: 4.10: Use list models
 **/
void
gtk_list_store_set_value (GtkListStore *list_store,
			  GtkTreeIter  *iter,
			  int           column,
			  GValue       *value)
{
  GtkListStorePrivate *priv;

  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter_is_valid (iter, list_store));
  g_return_if_fail (G_IS_VALUE (value));
  priv = list_store->priv;
  g_return_if_fail (column >= 0 && column < priv->n_columns);

  if (gtk_list_store_real_set_value (list_store, iter, column, value, TRUE))
    {
      GtkTreePath *path;

      path = gtk_list_store_get_path (GTK_TREE_MODEL (list_store), iter);
      gtk_tree_model_row_changed (GTK_TREE_MODEL (list_store), path, iter);
      gtk_tree_path_free (path);
    }
}

static GtkTreeIterCompareFunc
gtk_list_store_get_compare_func (GtkListStore *list_store)
{
  GtkListStorePrivate *priv = list_store->priv;
  GtkTreeIterCompareFunc func = NULL;

  if (GTK_LIST_STORE_IS_SORTED (list_store))
    {
      if (priv->sort_column_id != -1)
	{
	  GtkTreeDataSortHeader *header;
	  header = _gtk_tree_data_list_get_header (priv->sort_list,
						   priv->sort_column_id);
	  g_return_val_if_fail (header != NULL, NULL);
	  g_return_val_if_fail (header->func != NULL, NULL);
	  func = header->func;
	}
      else
	{
	  func = priv->default_sort_func;
	}
    }

  return func;
}

static void
gtk_list_store_set_vector_internal (GtkListStore *list_store,
				    GtkTreeIter  *iter,
				    gboolean     *emit_signal,
				    gboolean     *maybe_need_sort,
				    int          *columns,
				    GValue       *values,
				    int           n_values)
{
  GtkListStorePrivate *priv = list_store->priv;
  int i;
  GtkTreeIterCompareFunc func = NULL;

  func = gtk_list_store_get_compare_func (list_store);
  if (func != _gtk_tree_data_list_compare_func)
    *maybe_need_sort = TRUE;

  for (i = 0; i < n_values; i++)
    {
      *emit_signal = gtk_list_store_real_set_value (list_store,
					       iter,
					       columns[i],
					       &values[i],
					       FALSE) || *emit_signal;

      if (func == _gtk_tree_data_list_compare_func &&
	  columns[i] == priv->sort_column_id)
	*maybe_need_sort = TRUE;
    }
}

static void
gtk_list_store_set_valist_internal (GtkListStore *list_store,
				    GtkTreeIter  *iter,
				    gboolean     *emit_signal,
				    gboolean     *maybe_need_sort,
				    va_list	  var_args)
{
  GtkListStorePrivate *priv = list_store->priv;
  int column;
  GtkTreeIterCompareFunc func = NULL;

  column = va_arg (var_args, int);

  func = gtk_list_store_get_compare_func (list_store);
  if (func != _gtk_tree_data_list_compare_func)
    *maybe_need_sort = TRUE;

  while (column != -1)
    {
      GValue value = G_VALUE_INIT;
      char *error = NULL;

      if (column < 0 || column >= priv->n_columns)
	{
	  g_warning ("%s: Invalid column number %d added to iter (remember to end your list of columns with a -1)", G_STRLOC, column);
	  break;
	}

      G_VALUE_COLLECT_INIT (&value, priv->column_headers[column],
                            var_args, 0, &error);
      if (error)
	{
	  g_warning ("%s: %s", G_STRLOC, error);
	  g_free (error);

 	  /* we purposely leak the value here, it might not be
	   * in a sane state if an error condition occurred
	   */
	  break;
	}

      /* FIXME: instead of calling this n times, refactor with above */
      *emit_signal = gtk_list_store_real_set_value (list_store,
						    iter,
						    column,
						    &value,
						    FALSE) || *emit_signal;

      if (func == _gtk_tree_data_list_compare_func &&
	  column == priv->sort_column_id)
	*maybe_need_sort = TRUE;

      g_value_unset (&value);

      column = va_arg (var_args, int);
    }
}

/**
 * gtk_list_store_set_valuesv: (rename-to gtk_list_store_set)
 * @list_store: A `GtkListStore`
 * @iter: A valid `GtkTreeIter` for the row being modified
 * @columns: (array length=n_values): an array of column numbers
 * @values: (array length=n_values): an array of GValues
 * @n_values: the length of the @columns and @values arrays
 *
 * A variant of gtk_list_store_set_valist() which
 * takes the columns and values as two arrays, instead of
 * varargs. This function is mainly intended for
 * language-bindings and in case the number of columns to
 * change is not known until run-time.
 *
 * Deprecated: 4.10: Use list models
 */
void
gtk_list_store_set_valuesv (GtkListStore *list_store,
			    GtkTreeIter  *iter,
			    int          *columns,
			    GValue       *values,
			    int           n_values)
{
  GtkListStorePrivate *priv;
  gboolean emit_signal = FALSE;
  gboolean maybe_need_sort = FALSE;

  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter_is_valid (iter, list_store));

  priv = list_store->priv;

  gtk_list_store_set_vector_internal (list_store, iter,
				      &emit_signal,
				      &maybe_need_sort,
				      columns, values, n_values);

  if (maybe_need_sort && GTK_LIST_STORE_IS_SORTED (list_store))
    gtk_list_store_sort_iter_changed (list_store, iter, priv->sort_column_id);

  if (emit_signal)
    {
      GtkTreePath *path;

      path = gtk_list_store_get_path (GTK_TREE_MODEL (list_store), iter);
      gtk_tree_model_row_changed (GTK_TREE_MODEL (list_store), path, iter);
      gtk_tree_path_free (path);
    }
}

/**
 * gtk_list_store_set_valist:
 * @list_store: A `GtkListStore`
 * @iter: A valid `GtkTreeIter` for the row being modified
 * @var_args: va_list of column/value pairs
 *
 * See gtk_list_store_set(); this version takes a va_list for use by language
 * bindings.
 *
 * Deprecated: 4.10: Use list models
 **/
void
gtk_list_store_set_valist (GtkListStore *list_store,
                           GtkTreeIter  *iter,
                           va_list	 var_args)
{
  GtkListStorePrivate *priv;
  gboolean emit_signal = FALSE;
  gboolean maybe_need_sort = FALSE;

  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter_is_valid (iter, list_store));

  priv = list_store->priv;

  gtk_list_store_set_valist_internal (list_store, iter,
				      &emit_signal,
				      &maybe_need_sort,
				      var_args);

  if (maybe_need_sort && GTK_LIST_STORE_IS_SORTED (list_store))
    gtk_list_store_sort_iter_changed (list_store, iter, priv->sort_column_id);

  if (emit_signal)
    {
      GtkTreePath *path;

      path = gtk_list_store_get_path (GTK_TREE_MODEL (list_store), iter);
      gtk_tree_model_row_changed (GTK_TREE_MODEL (list_store), path, iter);
      gtk_tree_path_free (path);
    }
}

/**
 * gtk_list_store_set:
 * @list_store: a `GtkListStore`
 * @iter: row iterator
 * @...: pairs of column number and value, terminated with -1
 *
 * Sets the value of one or more cells in the row referenced by @iter.
 * The variable argument list should contain integer column numbers,
 * each column number followed by the value to be set.
 * The list is terminated by a -1. For example, to set column 0 with type
 * %G_TYPE_STRING to “Foo”, you would write `gtk_list_store_set (store, iter,
 * 0, "Foo", -1)`.
 *
 * The value will be referenced by the store if it is a %G_TYPE_OBJECT, and it
 * will be copied if it is a %G_TYPE_STRING or %G_TYPE_BOXED.
 *
 * Deprecated: 4.10: Use list models
 */
void
gtk_list_store_set (GtkListStore *list_store,
		    GtkTreeIter  *iter,
		    ...)
{
  va_list var_args;

  va_start (var_args, iter);
  gtk_list_store_set_valist (list_store, iter, var_args);
  va_end (var_args);
}

/**
 * gtk_list_store_remove:
 * @list_store: A `GtkListStore`
 * @iter: A valid `GtkTreeIter`
 *
 * Removes the given row from the list store.  After being removed,
 * @iter is set to be the next valid row, or invalidated if it pointed
 * to the last row in @list_store.
 *
 * Returns: %TRUE if @iter is valid, %FALSE if not.
 *
 * Deprecated: 4.10: Use list models
 **/
gboolean
gtk_list_store_remove (GtkListStore *list_store,
		       GtkTreeIter  *iter)
{
  GtkListStorePrivate *priv;
  GtkTreePath *path;
  GSequenceIter *ptr, *next;

  g_return_val_if_fail (GTK_IS_LIST_STORE (list_store), FALSE);
  g_return_val_if_fail (iter_is_valid (iter, list_store), FALSE);

  priv = list_store->priv;

  path = gtk_list_store_get_path (GTK_TREE_MODEL (list_store), iter);

  ptr = iter->user_data;
  next = g_sequence_iter_next (ptr);

  _gtk_tree_data_list_free (g_sequence_get (ptr), priv->column_headers);
  g_sequence_remove (iter->user_data);

  priv->length--;

  gtk_tree_model_row_deleted (GTK_TREE_MODEL (list_store), path);
  gtk_tree_path_free (path);

  if (g_sequence_iter_is_end (next))
    {
      iter->stamp = 0;
      return FALSE;
    }
  else
    {
      iter->stamp = priv->stamp;
      iter->user_data = next;
      return TRUE;
    }
}

/**
 * gtk_list_store_insert:
 * @list_store: A `GtkListStore`
 * @iter: (out): An unset `GtkTreeIter` to set to the new row
 * @position: position to insert the new row, or -1 for last
 *
 * Creates a new row at @position.  @iter will be changed to point to this new
 * row.  If @position is -1 or is larger than the number of rows on the list,
 * then the new row will be appended to the list. The row will be empty after
 * this function is called.  To fill in values, you need to call
 * gtk_list_store_set() or gtk_list_store_set_value().
 *
 * Deprecated: 4.10: Use list models
 **/
void
gtk_list_store_insert (GtkListStore *list_store,
		       GtkTreeIter  *iter,
		       int           position)
{
  GtkListStorePrivate *priv;
  GtkTreePath *path;
  GSequence *seq;
  GSequenceIter *ptr;
  int length;

  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);

  priv = list_store->priv;

  priv->columns_dirty = TRUE;

  seq = priv->seq;

  length = g_sequence_get_length (seq);
  if (position > length || position < 0)
    position = length;

  ptr = g_sequence_get_iter_at_pos (seq, position);
  ptr = g_sequence_insert_before (ptr, NULL);

  iter->stamp = priv->stamp;
  iter->user_data = ptr;

  g_assert (iter_is_valid (iter, list_store));

  priv->length++;

  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, position);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (list_store), path, iter);
  gtk_tree_path_free (path);
}

/**
 * gtk_list_store_insert_before:
 * @list_store: A `GtkListStore`
 * @iter: (out): An unset `GtkTreeIter` to set to the new row
 * @sibling: (nullable): A valid `GtkTreeIter`
 *
 * Inserts a new row before @sibling. If @sibling is %NULL, then the row will
 * be appended to the end of the list. @iter will be changed to point to this
 * new row. The row will be empty after this function is called. To fill in
 * values, you need to call gtk_list_store_set() or gtk_list_store_set_value().
 *
 * Deprecated: 4.10: Use list models
 **/
void
gtk_list_store_insert_before (GtkListStore *list_store,
			      GtkTreeIter  *iter,
			      GtkTreeIter  *sibling)
{
  GtkListStorePrivate *priv;
  GSequenceIter *after;

  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);

  priv = list_store->priv;

  if (sibling)
    g_return_if_fail (iter_is_valid (sibling, list_store));

  if (!sibling)
    after = g_sequence_get_end_iter (priv->seq);
  else
    after = sibling->user_data;

  gtk_list_store_insert (list_store, iter, g_sequence_iter_get_position (after));
}

/**
 * gtk_list_store_insert_after:
 * @list_store: A `GtkListStore`
 * @iter: (out): An unset `GtkTreeIter` to set to the new row
 * @sibling: (nullable): A valid `GtkTreeIter`
 *
 * Inserts a new row after @sibling. If @sibling is %NULL, then the row will be
 * prepended to the beginning of the list. @iter will be changed to point to
 * this new row. The row will be empty after this function is called. To fill
 * in values, you need to call gtk_list_store_set() or gtk_list_store_set_value().
 *
 * Deprecated: 4.10: Use list models
 **/
void
gtk_list_store_insert_after (GtkListStore *list_store,
			     GtkTreeIter  *iter,
			     GtkTreeIter  *sibling)
{
  GtkListStorePrivate *priv;
  GSequenceIter *after;

  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);

  priv = list_store->priv;

  if (sibling)
    g_return_if_fail (iter_is_valid (sibling, list_store));

  if (!sibling)
    after = g_sequence_get_begin_iter (priv->seq);
  else
    after = g_sequence_iter_next (sibling->user_data);

  gtk_list_store_insert (list_store, iter, g_sequence_iter_get_position (after));
}

/**
 * gtk_list_store_prepend:
 * @list_store: A `GtkListStore`
 * @iter: (out): An unset `GtkTreeIter` to set to the prepend row
 *
 * Prepends a new row to @list_store. @iter will be changed to point to this new
 * row. The row will be empty after this function is called. To fill in
 * values, you need to call gtk_list_store_set() or gtk_list_store_set_value().
 *
 * Deprecated: 4.10: Use list models
 **/
void
gtk_list_store_prepend (GtkListStore *list_store,
			GtkTreeIter  *iter)
{
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);

  gtk_list_store_insert (list_store, iter, 0);
}

/**
 * gtk_list_store_append:
 * @list_store: A `GtkListStore`
 * @iter: (out): An unset `GtkTreeIter` to set to the appended row
 *
 * Appends a new row to @list_store.  @iter will be changed to point to this new
 * row.  The row will be empty after this function is called.  To fill in
 * values, you need to call gtk_list_store_set() or gtk_list_store_set_value().
 *
 * Deprecated: 4.10: Use list models
 **/
void
gtk_list_store_append (GtkListStore *list_store,
		       GtkTreeIter  *iter)
{
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);

  gtk_list_store_insert (list_store, iter, -1);
}

static void
gtk_list_store_increment_stamp (GtkListStore *list_store)
{
  GtkListStorePrivate *priv = list_store->priv;

  do
    {
      priv->stamp++;
    }
  while (priv->stamp == 0);
}

/**
 * gtk_list_store_clear:
 * @list_store: a `GtkListStore`.
 *
 * Removes all rows from the list store.
 *
 * Deprecated: 4.10: Use list models
 **/
void
gtk_list_store_clear (GtkListStore *list_store)
{
  GtkListStorePrivate *priv;
  GtkTreeIter iter;

  g_return_if_fail (GTK_IS_LIST_STORE (list_store));

  priv = list_store->priv;

  while (g_sequence_get_length (priv->seq) > 0)
    {
      iter.stamp = priv->stamp;
      iter.user_data = g_sequence_get_begin_iter (priv->seq);
      gtk_list_store_remove (list_store, &iter);
    }

  gtk_list_store_increment_stamp (list_store);
}

/**
 * gtk_list_store_iter_is_valid:
 * @list_store: a list store
 * @iter: the iterator to check
 *
 * Checks if the given iter is a valid iter for this `GtkListStore`.
 *
 * This function is slow. Only use it for debugging and/or testing
 * purposes.
 *
 * Returns: %TRUE if the iter is valid, %FALSE if the iter is invalid.
 *
 * Deprecated: 4.10: Use list models
 **/
gboolean
gtk_list_store_iter_is_valid (GtkListStore *list_store,
                              GtkTreeIter  *iter)
{
  GtkListStorePrivate *priv;
  GSequenceIter *seq_iter;

  g_return_val_if_fail (GTK_IS_LIST_STORE (list_store), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  /* can't use iter_is_valid() here, because iter might point
   * to random memory.
   *
   * We MUST NOT dereference it.
   */

  priv = list_store->priv;

  if (iter == NULL ||
      iter->user_data == NULL ||
      priv->stamp != iter->stamp)
    return FALSE;

  for (seq_iter = g_sequence_get_begin_iter (priv->seq);
       !g_sequence_iter_is_end (seq_iter);
       seq_iter = g_sequence_iter_next (seq_iter))
    {
      if (seq_iter == iter->user_data)
        return TRUE;
    }

  return FALSE;
}

static gboolean real_gtk_list_store_row_draggable (GtkTreeDragSource *drag_source,
                                                   GtkTreePath       *path)
{
  return TRUE;
}

static gboolean
gtk_list_store_drag_data_delete (GtkTreeDragSource *drag_source,
                                 GtkTreePath       *path)
{
  GtkTreeIter iter;

  if (gtk_list_store_get_iter (GTK_TREE_MODEL (drag_source),
                               &iter,
                               path))
    {
      gtk_list_store_remove (GTK_LIST_STORE (drag_source), &iter);
      return TRUE;
    }
  return FALSE;
}

static GdkContentProvider *
gtk_list_store_drag_data_get (GtkTreeDragSource *drag_source,
                              GtkTreePath       *path)
{
  /* Note that we don't need to handle the GTK_TREE_MODEL_ROW
   * target, because the default handler does it for us, but
   * we do anyway for the convenience of someone maybe overriding the
   * default handler.
   */
  return gtk_tree_create_row_drag_content (GTK_TREE_MODEL (drag_source), path);
}

static gboolean
gtk_list_store_drag_data_received (GtkTreeDragDest *drag_dest,
                                   GtkTreePath     *dest,
                                   const GValue    *value)
{
  GtkTreeModel *tree_model = GTK_TREE_MODEL (drag_dest);
  GtkListStore *list_store = GTK_LIST_STORE (tree_model);
  GtkListStorePrivate *priv = list_store->priv;
  GtkTreeModel *src_model = NULL;
  GtkTreePath *src_path = NULL;
  gboolean retval = FALSE;

  if (gtk_tree_get_row_drag_data (value,
				  &src_model,
				  &src_path) &&
      src_model == tree_model)
    {
      /* Copy the given row to a new position */
      GtkTreeIter src_iter;
      GtkTreeIter dest_iter;
      GtkTreePath *prev;

      if (!gtk_list_store_get_iter (src_model,
                                    &src_iter,
                                    src_path))
        {
          goto out;
        }

      /* Get the path to insert _after_ (dest is the path to insert _before_) */
      prev = gtk_tree_path_copy (dest);

      if (!gtk_tree_path_prev (prev))
        {
          /* dest was the first spot in the list; which means we are supposed
           * to prepend.
           */
          gtk_list_store_prepend (list_store, &dest_iter);

          retval = TRUE;
        }
      else
        {
          if (gtk_list_store_get_iter (tree_model, &dest_iter, prev))
            {
              GtkTreeIter tmp_iter = dest_iter;

              gtk_list_store_insert_after (list_store, &dest_iter, &tmp_iter);

              retval = TRUE;
            }
        }

      gtk_tree_path_free (prev);

      /* If we succeeded in creating dest_iter, copy data from src
       */
      if (retval)
        {
          GtkTreeDataList *dl = g_sequence_get (src_iter.user_data);
          GtkTreeDataList *copy_head = NULL;
          GtkTreeDataList *copy_prev = NULL;
          GtkTreeDataList *copy_iter = NULL;
	  GtkTreePath *path;
          int col;

          col = 0;
          while (dl)
            {
              copy_iter = _gtk_tree_data_list_node_copy (dl,
                                                         priv->column_headers[col]);

              if (copy_head == NULL)
                copy_head = copy_iter;

              if (copy_prev)
                copy_prev->next = copy_iter;

              copy_prev = copy_iter;

              dl = dl->next;
              ++col;
            }

	  dest_iter.stamp = priv->stamp;
          g_sequence_set (dest_iter.user_data, copy_head);

	  path = gtk_list_store_get_path (tree_model, &dest_iter);
	  gtk_tree_model_row_changed (tree_model, path, &dest_iter);
	  gtk_tree_path_free (path);
	}
    }
  else
    {
      /* FIXME maybe add some data targets eventually, or handle text
       * targets in the simple case.
       */
    }

 out:

  if (src_path)
    gtk_tree_path_free (src_path);

  return retval;
}

static gboolean
gtk_list_store_row_drop_possible (GtkTreeDragDest  *drag_dest,
                                  GtkTreePath      *dest_path,
                                  const GValue     *value)
{
  int *indices;
  GtkTreeModel *src_model = NULL;
  GtkTreePath *src_path = NULL;
  gboolean retval = FALSE;

  /* don't accept drops if the list has been sorted */
  if (GTK_LIST_STORE_IS_SORTED (drag_dest))
    return FALSE;

  if (!gtk_tree_get_row_drag_data (value,
				   &src_model,
				   &src_path))
    goto out;

  if (src_model != GTK_TREE_MODEL (drag_dest))
    goto out;

  if (gtk_tree_path_get_depth (dest_path) != 1)
    goto out;

  /* can drop before any existing node, or before one past any existing. */

  indices = gtk_tree_path_get_indices (dest_path);

  if (indices[0] <= g_sequence_get_length (GTK_LIST_STORE (drag_dest)->priv->seq))
    retval = TRUE;

 out:
  if (src_path)
    gtk_tree_path_free (src_path);

  return retval;
}

/* Sorting and reordering */

/* Reordering */
static int
gtk_list_store_reorder_func (GSequenceIter *a,
			     GSequenceIter *b,
			     gpointer       user_data)
{
  GHashTable *new_positions = user_data;
  int apos = GPOINTER_TO_INT (g_hash_table_lookup (new_positions, a));
  int bpos = GPOINTER_TO_INT (g_hash_table_lookup (new_positions, b));

  if (apos < bpos)
    return -1;
  if (apos > bpos)
    return 1;
  return 0;
}

/**
 * gtk_list_store_reorder:
 * @store: A `GtkListStore`.
 * @new_order: (array zero-terminated=1): an array of integers mapping the new
 *   position of each child to its old position before the re-ordering,
 *   i.e. @new_order`[newpos] = oldpos`. It must have
 *   exactly as many items as the list store’s length.
 *
 * Reorders @store to follow the order indicated by @new_order. Note that
 * this function only works with unsorted stores.
 *
 * Deprecated: 4.10: Use list models
 **/
void
gtk_list_store_reorder (GtkListStore *store,
			int          *new_order)
{
  GtkListStorePrivate *priv;
  int i;
  GtkTreePath *path;
  GHashTable *new_positions;
  GSequenceIter *ptr;
  int *order;

  g_return_if_fail (GTK_IS_LIST_STORE (store));
  g_return_if_fail (!GTK_LIST_STORE_IS_SORTED (store));
  g_return_if_fail (new_order != NULL);

  priv = store->priv;

  order = g_new (int, g_sequence_get_length (priv->seq));
  for (i = 0; i < g_sequence_get_length (priv->seq); i++)
    order[new_order[i]] = i;

  new_positions = g_hash_table_new (g_direct_hash, g_direct_equal);

  ptr = g_sequence_get_begin_iter (priv->seq);
  i = 0;
  while (!g_sequence_iter_is_end (ptr))
    {
      g_hash_table_insert (new_positions, ptr, GINT_TO_POINTER (order[i++]));

      ptr = g_sequence_iter_next (ptr);
    }
  g_free (order);

  g_sequence_sort_iter (priv->seq, gtk_list_store_reorder_func, new_positions);

  g_hash_table_destroy (new_positions);

  /* emit signal */
  path = gtk_tree_path_new ();
  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (store),
				 path, NULL, new_order);
  gtk_tree_path_free (path);
}

static GHashTable *
save_positions (GSequence *seq)
{
  GHashTable *positions = g_hash_table_new (g_direct_hash, g_direct_equal);
  GSequenceIter *ptr;

  ptr = g_sequence_get_begin_iter (seq);
  while (!g_sequence_iter_is_end (ptr))
    {
      g_hash_table_insert (positions, ptr,
			   GINT_TO_POINTER (g_sequence_iter_get_position (ptr)));
      ptr = g_sequence_iter_next (ptr);
    }

  return positions;
}

static int *
generate_order (GSequence *seq,
		GHashTable *old_positions)
{
  GSequenceIter *ptr;
  int *order = g_new (int, g_sequence_get_length (seq));
  int i;

  i = 0;
  ptr = g_sequence_get_begin_iter (seq);
  while (!g_sequence_iter_is_end (ptr))
    {
      int old_pos = GPOINTER_TO_INT (g_hash_table_lookup (old_positions, ptr));
      order[i++] = old_pos;
      ptr = g_sequence_iter_next (ptr);
    }

  g_hash_table_destroy (old_positions);

  return order;
}

/**
 * gtk_list_store_swap:
 * @store: A `GtkListStore`.
 * @a: A `GtkTreeIter`
 * @b: Another `GtkTreeIter`
 *
 * Swaps @a and @b in @store. Note that this function only works with
 * unsorted stores.
 *
 * Deprecated: 4.10: Use list models
 **/
void
gtk_list_store_swap (GtkListStore *store,
		     GtkTreeIter  *a,
		     GtkTreeIter  *b)
{
  GtkListStorePrivate *priv;
  GHashTable *old_positions;
  int *order;
  GtkTreePath *path;

  g_return_if_fail (GTK_IS_LIST_STORE (store));
  g_return_if_fail (!GTK_LIST_STORE_IS_SORTED (store));
  g_return_if_fail (iter_is_valid (a, store));
  g_return_if_fail (iter_is_valid (b, store));

  priv = store->priv;

  if (a->user_data == b->user_data)
    return;

  old_positions = save_positions (priv->seq);

  g_sequence_swap (a->user_data, b->user_data);

  order = generate_order (priv->seq, old_positions);
  path = gtk_tree_path_new ();

  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (store),
				 path, NULL, order);

  gtk_tree_path_free (path);
  g_free (order);
}

static void
gtk_list_store_move_to (GtkListStore *store,
			GtkTreeIter  *iter,
			int	      new_pos)
{
  GtkListStorePrivate *priv = store->priv;
  GHashTable *old_positions;
  GtkTreePath *path;
  int *order;

  old_positions = save_positions (priv->seq);

  g_sequence_move (iter->user_data, g_sequence_get_iter_at_pos (priv->seq, new_pos));

  order = generate_order (priv->seq, old_positions);

  path = gtk_tree_path_new ();
  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (store),
				 path, NULL, order);
  gtk_tree_path_free (path);
  g_free (order);
}

/**
 * gtk_list_store_move_before:
 * @store: A `GtkListStore`.
 * @iter: A `GtkTreeIter`
 * @position: (nullable): A `GtkTreeIter`
 *
 * Moves @iter in @store to the position before @position. Note that this
 * function only works with unsorted stores. If @position is %NULL, @iter
 * will be moved to the end of the list.
 *
 * Deprecated: 4.10: Use list models
 **/
void
gtk_list_store_move_before (GtkListStore *store,
                            GtkTreeIter  *iter,
			    GtkTreeIter  *position)
{
  int pos;

  g_return_if_fail (GTK_IS_LIST_STORE (store));
  g_return_if_fail (!GTK_LIST_STORE_IS_SORTED (store));
  g_return_if_fail (iter_is_valid (iter, store));
  if (position)
    g_return_if_fail (iter_is_valid (position, store));

  if (position)
    pos = g_sequence_iter_get_position (position->user_data);
  else
    pos = -1;

  gtk_list_store_move_to (store, iter, pos);
}

/**
 * gtk_list_store_move_after:
 * @store: A `GtkListStore`.
 * @iter: A `GtkTreeIter`
 * @position: (nullable): A `GtkTreeIter`
 *
 * Moves @iter in @store to the position after @position. Note that this
 * function only works with unsorted stores. If @position is %NULL, @iter
 * will be moved to the start of the list.
 *
 * Deprecated: 4.10: Use list models
 **/
void
gtk_list_store_move_after (GtkListStore *store,
			   GtkTreeIter  *iter,
			   GtkTreeIter  *position)
{
  int pos;

  g_return_if_fail (GTK_IS_LIST_STORE (store));
  g_return_if_fail (!GTK_LIST_STORE_IS_SORTED (store));
  g_return_if_fail (iter_is_valid (iter, store));
  if (position)
    g_return_if_fail (iter_is_valid (position, store));

  if (position)
    pos = g_sequence_iter_get_position (position->user_data) + 1;
  else
    pos = 0;

  gtk_list_store_move_to (store, iter, pos);
}

/* Sorting */
static int
gtk_list_store_compare_func (GSequenceIter *a,
			     GSequenceIter *b,
			     gpointer      user_data)
{
  GtkListStore *list_store = user_data;
  GtkListStorePrivate *priv = list_store->priv;
  GtkTreeIter iter_a;
  GtkTreeIter iter_b;
  int retval;
  GtkTreeIterCompareFunc func;
  gpointer data;

  if (priv->sort_column_id != -1)
    {
      GtkTreeDataSortHeader *header;

      header = _gtk_tree_data_list_get_header (priv->sort_list,
					       priv->sort_column_id);
      g_return_val_if_fail (header != NULL, 0);
      g_return_val_if_fail (header->func != NULL, 0);

      func = header->func;
      data = header->data;
    }
  else
    {
      g_return_val_if_fail (priv->default_sort_func != NULL, 0);
      func = priv->default_sort_func;
      data = priv->default_sort_data;
    }

  iter_a.stamp = priv->stamp;
  iter_a.user_data = (gpointer)a;
  iter_b.stamp = priv->stamp;
  iter_b.user_data = (gpointer)b;

  g_assert (iter_is_valid (&iter_a, list_store));
  g_assert (iter_is_valid (&iter_b, list_store));

  retval = (* func) (GTK_TREE_MODEL (list_store), &iter_a, &iter_b, data);

  if (priv->order == GTK_SORT_DESCENDING)
    {
      if (retval > 0)
        retval = -1;
      else if (retval < 0)
        retval = 1;
    }

  return retval;
}

static void
gtk_list_store_sort (GtkListStore *list_store)
{
  GtkListStorePrivate *priv = list_store->priv;
  int *new_order;
  GtkTreePath *path;
  GHashTable *old_positions;

  if (!GTK_LIST_STORE_IS_SORTED (list_store) ||
      g_sequence_get_length (priv->seq) <= 1)
    return;

  old_positions = save_positions (priv->seq);

  g_sequence_sort_iter (priv->seq, gtk_list_store_compare_func, list_store);

  /* Let the world know about our new order */
  new_order = generate_order (priv->seq, old_positions);

  path = gtk_tree_path_new ();
  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (list_store),
				 path, NULL, new_order);
  gtk_tree_path_free (path);
  g_free (new_order);
}

static gboolean
iter_is_sorted (GtkListStore *list_store,
                GtkTreeIter  *iter)
{
  GSequenceIter *cmp;

  if (!g_sequence_iter_is_begin (iter->user_data))
    {
      cmp = g_sequence_iter_prev (iter->user_data);
      if (gtk_list_store_compare_func (cmp, iter->user_data, list_store) > 0)
	return FALSE;
    }

  cmp = g_sequence_iter_next (iter->user_data);
  if (!g_sequence_iter_is_end (cmp))
    {
      if (gtk_list_store_compare_func (iter->user_data, cmp, list_store) > 0)
	return FALSE;
    }

  return TRUE;
}

static void
gtk_list_store_sort_iter_changed (GtkListStore *list_store,
				  GtkTreeIter  *iter,
				  int           column)

{
  GtkListStorePrivate *priv = list_store->priv;
  GtkTreePath *path;

  path = gtk_list_store_get_path (GTK_TREE_MODEL (list_store), iter);
  gtk_tree_model_row_changed (GTK_TREE_MODEL (list_store), path, iter);
  gtk_tree_path_free (path);

  if (!iter_is_sorted (list_store, iter))
    {
      GHashTable *old_positions;
      int *order;

      old_positions = save_positions (priv->seq);
      g_sequence_sort_changed_iter (iter->user_data,
				    gtk_list_store_compare_func,
				    list_store);
      order = generate_order (priv->seq, old_positions);
      path = gtk_tree_path_new ();
      gtk_tree_model_rows_reordered (GTK_TREE_MODEL (list_store),
                                     path, NULL, order);
      gtk_tree_path_free (path);
      g_free (order);
    }
}

static gboolean
gtk_list_store_get_sort_column_id (GtkTreeSortable  *sortable,
				   int              *sort_column_id,
				   GtkSortType      *order)
{
  GtkListStore *list_store = GTK_LIST_STORE (sortable);
  GtkListStorePrivate *priv = list_store->priv;

  if (sort_column_id)
    * sort_column_id = priv->sort_column_id;
  if (order)
    * order = priv->order;

  if (priv->sort_column_id == GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID ||
      priv->sort_column_id == GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID)
    return FALSE;

  return TRUE;
}

static void
gtk_list_store_set_sort_column_id (GtkTreeSortable  *sortable,
				   int               sort_column_id,
				   GtkSortType       order)
{
  GtkListStore *list_store = GTK_LIST_STORE (sortable);
  GtkListStorePrivate *priv = list_store->priv;

  if ((priv->sort_column_id == sort_column_id) &&
      (priv->order == order))
    return;

  if (sort_column_id != GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID)
    {
      if (sort_column_id != GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID)
	{
	  GtkTreeDataSortHeader *header = NULL;

	  header = _gtk_tree_data_list_get_header (priv->sort_list,
						   sort_column_id);

	  /* We want to make sure that we have a function */
	  g_return_if_fail (header != NULL);
	  g_return_if_fail (header->func != NULL);
	}
      else
	{
	  g_return_if_fail (priv->default_sort_func != NULL);
	}
    }


  priv->sort_column_id = sort_column_id;
  priv->order = order;

  gtk_tree_sortable_sort_column_changed (sortable);

  gtk_list_store_sort (list_store);
}

static void
gtk_list_store_set_sort_func (GtkTreeSortable        *sortable,
			      int                     sort_column_id,
			      GtkTreeIterCompareFunc  func,
			      gpointer                data,
			      GDestroyNotify          destroy)
{
  GtkListStore *list_store = GTK_LIST_STORE (sortable);
  GtkListStorePrivate *priv = list_store->priv;

  priv->sort_list = _gtk_tree_data_list_set_header (priv->sort_list,
							  sort_column_id,
							  func, data, destroy);

  if (priv->sort_column_id == sort_column_id)
    gtk_list_store_sort (list_store);
}

static void
gtk_list_store_set_default_sort_func (GtkTreeSortable        *sortable,
				      GtkTreeIterCompareFunc  func,
				      gpointer                data,
				      GDestroyNotify          destroy)
{
  GtkListStore *list_store = GTK_LIST_STORE (sortable);
  GtkListStorePrivate *priv = list_store->priv;

  if (priv->default_sort_destroy)
    {
      GDestroyNotify d = priv->default_sort_destroy;

      priv->default_sort_destroy = NULL;
      d (priv->default_sort_data);
    }

  priv->default_sort_func = func;
  priv->default_sort_data = data;
  priv->default_sort_destroy = destroy;

  if (priv->sort_column_id == GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID)
    gtk_list_store_sort (list_store);
}

static gboolean
gtk_list_store_has_default_sort_func (GtkTreeSortable *sortable)
{
  GtkListStore *list_store = GTK_LIST_STORE (sortable);
  GtkListStorePrivate *priv = list_store->priv;

  return (priv->default_sort_func != NULL);
}


/**
 * gtk_list_store_insert_with_values:
 * @list_store: A `GtkListStore`
 * @iter: (out) (optional): An unset `GtkTreeIter` to set to the new row
 * @position: position to insert the new row, or -1 to append after existing
 *    rows
 * @...: pairs of column number and value, terminated with -1
 *
 * Creates a new row at @position. @iter will be changed to point to this new
 * row. If @position is -1, or larger than the number of rows in the list, then
 * the new row will be appended to the list. The row will be filled with the
 * values given to this function.
 *
 * Calling
 * `gtk_list_store_insert_with_values (list_store, iter, position...)`
 * has the same effect as calling:
 *
 * |[<!-- language="C" -->
 * static void
 * insert_value (GtkListStore *list_store,
 *               GtkTreeIter  *iter,
 *               int           position)
 * {
 *   gtk_list_store_insert (list_store, iter, position);
 *   gtk_list_store_set (list_store,
 *                       iter
 *                       // ...
 *                       );
 * }
 * ]|
 *
 * with the difference that the former will only emit `GtkTreeModel`::row-inserted
 * once, while the latter will emit `GtkTreeModel`::row-inserted,
 * `GtkTreeModel`::row-changed and, if the list store is sorted,
 * `GtkTreeModel`::rows-reordered for every inserted value.
 *
 * Since emitting the `GtkTreeModel::rows-reordered` signal repeatedly can
 * affect the performance of the program, gtk_list_store_insert_with_values()
 * should generally be preferred when inserting rows in a sorted list store.
 *
 * Deprecated: 4.10: Use list models
 */
void
gtk_list_store_insert_with_values (GtkListStore *list_store,
				   GtkTreeIter  *iter,
				   int           position,
				   ...)
{
  GtkListStorePrivate *priv;
  GtkTreePath *path;
  GSequence *seq;
  GSequenceIter *ptr;
  GtkTreeIter tmp_iter;
  int length;
  gboolean changed = FALSE;
  gboolean maybe_need_sort = FALSE;
  va_list var_args;

  /* FIXME: refactor to reduce overlap with gtk_list_store_set() */
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));

  priv = list_store->priv;

  if (!iter)
    iter = &tmp_iter;

  priv->columns_dirty = TRUE;

  seq = priv->seq;

  length = g_sequence_get_length (seq);
  if (position > length || position < 0)
    position = length;

  ptr = g_sequence_get_iter_at_pos (seq, position);
  ptr = g_sequence_insert_before (ptr, NULL);

  iter->stamp = priv->stamp;
  iter->user_data = ptr;

  g_assert (iter_is_valid (iter, list_store));

  priv->length++;

  va_start (var_args, position);
  gtk_list_store_set_valist_internal (list_store, iter,
				      &changed, &maybe_need_sort,
				      var_args);
  va_end (var_args);

  /* Don't emit rows_reordered here */
  if (maybe_need_sort && GTK_LIST_STORE_IS_SORTED (list_store))
    g_sequence_sort_changed_iter (iter->user_data,
				  gtk_list_store_compare_func,
				  list_store);

  /* Just emit row_inserted */
  path = gtk_list_store_get_path (GTK_TREE_MODEL (list_store), iter);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (list_store), path, iter);
  gtk_tree_path_free (path);
}


/**
 * gtk_list_store_insert_with_valuesv: (rename-to gtk_list_store_insert_with_values)
 * @list_store: A `GtkListStore`
 * @iter: (out) (optional): An unset `GtkTreeIter` to set to the new row
 * @position: position to insert the new row, or -1 for last
 * @columns: (array length=n_values): an array of column numbers
 * @values: (array length=n_values): an array of GValues
 * @n_values: the length of the @columns and @values arrays
 *
 * A variant of gtk_list_store_insert_with_values() which
 * takes the columns and values as two arrays, instead of
 * varargs.
 *
 * This function is mainly intended for language-bindings.
 *
 * Deprecated: 4.10: Use list models
 */
void
gtk_list_store_insert_with_valuesv (GtkListStore *list_store,
				    GtkTreeIter  *iter,
				    int           position,
				    int          *columns,
				    GValue       *values,
				    int           n_values)
{
  GtkListStorePrivate *priv;
  GtkTreePath *path;
  GSequence *seq;
  GSequenceIter *ptr;
  GtkTreeIter tmp_iter;
  int length;
  gboolean changed = FALSE;
  gboolean maybe_need_sort = FALSE;

  /* FIXME refactor to reduce overlap with
   * gtk_list_store_insert_with_values()
   */
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));

  priv = list_store->priv;

  if (!iter)
    iter = &tmp_iter;

  priv->columns_dirty = TRUE;

  seq = priv->seq;

  length = g_sequence_get_length (seq);
  if (position > length || position < 0)
    position = length;

  ptr = g_sequence_get_iter_at_pos (seq, position);
  ptr = g_sequence_insert_before (ptr, NULL);

  iter->stamp = priv->stamp;
  iter->user_data = ptr;

  g_assert (iter_is_valid (iter, list_store));

  priv->length++;

  gtk_list_store_set_vector_internal (list_store, iter,
				      &changed, &maybe_need_sort,
				      columns, values, n_values);

  /* Don't emit rows_reordered here */
  if (maybe_need_sort && GTK_LIST_STORE_IS_SORTED (list_store))
    g_sequence_sort_changed_iter (iter->user_data,
				  gtk_list_store_compare_func,
				  list_store);

  /* Just emit row_inserted */
  path = gtk_list_store_get_path (GTK_TREE_MODEL (list_store), iter);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (list_store), path, iter);
  gtk_tree_path_free (path);
}

/* GtkBuildable custom tag implementation
 *
 * <columns>
 *   <column type="..."/>
 *   <column type="..."/>
 * </columns>
 */
typedef struct {
  gboolean translatable;
  char *context;
  int id;
} ColInfo;

typedef struct {
  GtkBuilder *builder;
  GObject *object;
  GSList *column_type_names;
  GType *column_types;
  GValue *values;
  int *colids;
  ColInfo **columns;
  int last_row;
  int n_columns;
  int row_column;
  gboolean is_data;
  const char *domain;
} SubParserData;

static void
list_store_start_element (GtkBuildableParseContext *context,
                          const char               *element_name,
                          const char              **names,
                          const char              **values,
                          gpointer                  user_data,
                          GError                  **error)
{
  SubParserData *data = (SubParserData*)user_data;

  if (strcmp (element_name, "col") == 0)
    {
      int id = -1;
      const char *id_str;
      const char *msg_context = NULL;
      gboolean translatable = FALSE;
      ColInfo *info;
      GValue val = G_VALUE_INIT;

      if (!_gtk_builder_check_parent (data->builder, context, "row", error))
        return;

      if (data->row_column >= data->n_columns)
        {
	  g_set_error (error,
                       GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_VALUE,
	  	       "Too many columns, maximum is %d", data->n_columns - 1);
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "id", &id_str,
                                        G_MARKUP_COLLECT_BOOLEAN|G_MARKUP_COLLECT_OPTIONAL, "translatable", &translatable,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "comments", NULL,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "context", &msg_context,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      if (!gtk_builder_value_from_string_type (data->builder, G_TYPE_INT, id_str, &val, error))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      id = g_value_get_int (&val);
      if (id < 0 || id >= data->n_columns)
        {
          g_set_error (error,
                       GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_VALUE,
                       "id value %d out of range", id);
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      info = g_slice_new0 (ColInfo);
      info->translatable = translatable;
      info->context = g_strdup (msg_context);
      info->id = id;

      data->colids[data->row_column] = id;
      data->columns[data->row_column] = info;
      data->row_column++;
      data->is_data = TRUE;
    }
  else if (strcmp (element_name, "row") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "data", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _gtk_builder_prefix_error (data->builder, context, error);
    }
  else if (strcmp (element_name, "columns") == 0 ||
           strcmp (element_name, "data") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "object", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _gtk_builder_prefix_error (data->builder, context, error);
    }
  else if (strcmp (element_name, "column") == 0)
    {
      const char *type;

      if (!_gtk_builder_check_parent (data->builder, context, "columns", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "type", &type,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      data->column_type_names = g_slist_prepend (data->column_type_names, g_strdup (type));
    }
  else
    {
      _gtk_builder_error_unhandled_tag (data->builder, context,
                                        "GtkListStore", element_name,
                                        error);
    }
}

static void
list_store_end_element (GtkBuildableParseContext  *context,
                        const char                *element_name,
                        gpointer                   user_data,
                        GError                   **error)
{
  SubParserData *data = (SubParserData*)user_data;

  g_assert (data->builder);

  if (strcmp (element_name, "row") == 0)
    {
      GtkTreeIter iter;
      int i;

      gtk_list_store_insert_with_valuesv (GTK_LIST_STORE (data->object),
                                          &iter,
                                          data->last_row,
                                          data->colids,
                                          data->values,
                                          data->row_column);
      for (i = 0; i < data->row_column; i++)
        {
          ColInfo *info = data->columns[i];
          g_free (info->context);
          g_slice_free (ColInfo, info);
          data->columns[i] = NULL;
          g_value_unset (&data->values[i]);
        }
      g_free (data->values);
      data->values = g_new0 (GValue, data->n_columns);
      data->last_row++;
      data->row_column = 0;
    }
  else if (strcmp (element_name, "columns") == 0)
    {
      GType *column_types;
      GSList *l;
      int i;
      GType type;

      data->column_type_names = g_slist_reverse (data->column_type_names);
      column_types = g_new0 (GType, g_slist_length (data->column_type_names));

      for (l = data->column_type_names, i = 0; l; l = l->next, i++)
        {
          type = gtk_builder_get_type_from_name (data->builder, l->data);
          if (type == G_TYPE_INVALID)
            {
              g_warning ("Unknown type %s specified in treemodel %s",
                         (const char *)l->data,
                         gtk_buildable_get_buildable_id (GTK_BUILDABLE (data->object)));
              continue;
            }
          column_types[i] = type;

          g_free (l->data);
        }

      gtk_list_store_set_column_types (GTK_LIST_STORE (data->object), i, column_types);

      g_free (column_types);
    }
  else if (strcmp (element_name, "col") == 0)
    {
      data->is_data = FALSE;
    }
}

static void
list_store_text (GtkBuildableParseContext  *context,
                 const char                *text,
                 gsize                      text_len,
                 gpointer                   user_data,
                 GError                   **error)
{
  SubParserData *data = (SubParserData*)user_data;
  int i;
  char *string;
  ColInfo *info;

  if (!data->is_data)
    return;

  i = data->row_column - 1;
  info = data->columns[i];

  string = g_strndup (text, text_len);
  if (info->translatable && text_len)
    {
      char *translated;

      /* FIXME: This will not use the domain set in the .ui file,
       * since the parser is not telling the builder about the domain.
       * However, it will work for gtk_builder_set_translation_domain() calls.
       */
      translated = g_strdup (_gtk_builder_parser_translate (data->domain,
                                                            info->context,
                                                            string));
      g_free (string);
      string = translated;
    }

  if (!gtk_builder_value_from_string_type (data->builder,
                                           data->column_types[info->id],
                                           string,
                                           &data->values[i],
                                           error))
    {
      _gtk_builder_prefix_error (data->builder, context, error);
    }
  g_free (string);
}

static const GtkBuildableParser list_store_parser =
  {
    list_store_start_element,
    list_store_end_element,
    list_store_text
  };

static gboolean
gtk_list_store_buildable_custom_tag_start (GtkBuildable       *buildable,
                                           GtkBuilder         *builder,
                                           GObject            *child,
                                           const char         *tagname,
                                           GtkBuildableParser *parser,
                                           gpointer           *parser_data)
{
  SubParserData *data;

  if (child)
    return FALSE;

  if (strcmp (tagname, "columns") == 0)
    {
      data = g_slice_new0 (SubParserData);
      data->builder = builder;
      data->object = G_OBJECT (buildable);
      data->column_type_names = NULL;

      *parser = list_store_parser;
      *parser_data = data;

      return TRUE;
    }
  else if (strcmp (tagname, "data") == 0)
    {
      int n_columns = gtk_list_store_get_n_columns (GTK_TREE_MODEL (buildable));
      if (n_columns == 0)
        g_error ("Cannot append data to an empty model");

      data = g_slice_new0 (SubParserData);
      data->builder = builder;
      data->object = G_OBJECT (buildable);
      data->values = g_new0 (GValue, n_columns);
      data->colids = g_new0 (int, n_columns);
      data->columns = g_new0 (ColInfo*, n_columns);
      data->column_types = GTK_LIST_STORE (buildable)->priv->column_headers;
      data->n_columns = n_columns;
      data->last_row = 0;
      data->domain = gtk_builder_get_translation_domain (builder);

      *parser = list_store_parser;
      *parser_data = data;

      return TRUE;
    }

  return FALSE;
}

static void
gtk_list_store_buildable_custom_tag_end (GtkBuildable *buildable,
                                         GtkBuilder   *builder,
                                         GObject      *child,
                                         const char   *tagname,
                                         gpointer      parser_data)
{
  SubParserData *data = (SubParserData*)parser_data;

  if (strcmp (tagname, "columns") == 0)
    {
      g_slist_free (data->column_type_names);
      g_slice_free (SubParserData, data);
    }
  else if (strcmp (tagname, "data") == 0)
    {
      int i;
      for (i = 0; i < data->n_columns; i++)
        {
          ColInfo *info = data->columns[i];
          if (info)
            {
              g_free (info->context);
              g_slice_free (ColInfo, info);
            }
        }
      g_free (data->colids);
      g_free (data->columns);
      g_free (data->values);
      g_slice_free (SubParserData, data);
    }
}
