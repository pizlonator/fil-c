/* gtktreemodel.c
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
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <gobject/gvaluecollector.h>
#include "gtktreemodel.h"
#include "gtktreeview.h"
#include "gtktreeprivate.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * GtkTreeModel:
 *
 * The tree interface used by GtkTreeView
 *
 * The `GtkTreeModel` interface defines a generic tree interface for
 * use by the `GtkTreeView` widget. It is an abstract interface, and
 * is designed to be usable with any appropriate data structure. The
 * programmer just has to implement this interface on their own data
 * type for it to be viewable by a `GtkTreeView` widget.
 *
 * The model is represented as a hierarchical tree of strongly-typed,
 * columned data. In other words, the model can be seen as a tree where
 * every node has different values depending on which column is being
 * queried. The type of data found in a column is determined by using
 * the GType system (ie. %G_TYPE_INT, %GTK_TYPE_BUTTON, %G_TYPE_POINTER,
 * etc). The types are homogeneous per column across all nodes. It is
 * important to note that this interface only provides a way of examining
 * a model and observing changes. The implementation of each individual
 * model decides how and if changes are made.
 *
 * In order to make life simpler for programmers who do not need to
 * write their own specialized model, two generic models are provided
 * — the `GtkTreeStore` and the `GtkListStore`. To use these, the
 * developer simply pushes data into these models as necessary. These
 * models provide the data structure as well as all appropriate tree
 * interfaces. As a result, implementing drag and drop, sorting, and
 * storing data is trivial. For the vast majority of trees and lists,
 * these two models are sufficient.
 *
 * Models are accessed on a node/column level of granularity. One can
 * query for the value of a model at a certain node and a certain
 * column on that node. There are two structures used to reference a
 * particular node in a model. They are the [struct@Gtk.TreePath] and
 * the [struct@Gtk.TreeIter] (“iter” is short for iterator). Most of the
 * interface consists of operations on a [struct@Gtk.TreeIter].
 *
 * A path is essentially a potential node. It is a location on a model
 * that may or may not actually correspond to a node on a specific
 * model. A [struct@Gtk.TreePath] can be converted into either an
 * array of unsigned integers or a string. The string form is a list
 * of numbers separated by a colon. Each number refers to the offset
 * at that level. Thus, the path `0` refers to the root
 * node and the path `2:4` refers to the fifth child of
 * the third node.
 *
 * By contrast, a [struct@Gtk.TreeIter] is a reference to a specific node on
 * a specific model. It is a generic struct with an integer and three
 * generic pointers. These are filled in by the model in a model-specific
 * way. One can convert a path to an iterator by calling
 * gtk_tree_model_get_iter(). These iterators are the primary way
 * of accessing a model and are similar to the iterators used by
 * `GtkTextBuffer`. They are generally statically allocated on the
 * stack and only used for a short time. The model interface defines
 * a set of operations using them for navigating the model.
 *
 * It is expected that models fill in the iterator with private data.
 * For example, the `GtkListStore` model, which is internally a simple
 * linked list, stores a list node in one of the pointers. The
 * `GtkTreeModel`Sort stores an array and an offset in two of the
 * pointers. Additionally, there is an integer field. This field is
 * generally filled with a unique stamp per model. This stamp is for
 * catching errors resulting from using invalid iterators with a model.
 *
 * The lifecycle of an iterator can be a little confusing at first.
 * Iterators are expected to always be valid for as long as the model
 * is unchanged (and doesn’t emit a signal). The model is considered
 * to own all outstanding iterators and nothing needs to be done to
 * free them from the user’s point of view. Additionally, some models
 * guarantee that an iterator is valid for as long as the node it refers
 * to is valid (most notably the `GtkTreeStore` and `GtkListStore`).
 * Although generally uninteresting, as one always has to allow for
 * the case where iterators do not persist beyond a signal, some very
 * important performance enhancements were made in the sort model.
 * As a result, the %GTK_TREE_MODEL_ITERS_PERSIST flag was added to
 * indicate this behavior.
 *
 * To help show some common operation of a model, some examples are
 * provided. The first example shows three ways of getting the iter at
 * the location `3:2:5`. While the first method shown is
 * easier, the second is much more common, as you often get paths from
 * callbacks.
 *
 * ## Acquiring a `GtkTreeIter`
 *
 * ```c
 * // Three ways of getting the iter pointing to the location
 * GtkTreePath *path;
 * GtkTreeIter iter;
 * GtkTreeIter parent_iter;
 *
 * // get the iterator from a string
 * gtk_tree_model_get_iter_from_string (model,
 *                                      &iter,
 *                                      "3:2:5");
 *
 * // get the iterator from a path
 * path = gtk_tree_path_new_from_string ("3:2:5");
 * gtk_tree_model_get_iter (model, &iter, path);
 * gtk_tree_path_free (path);
 *
 * // walk the tree to find the iterator
 * gtk_tree_model_iter_nth_child (model, &iter,
 *                                NULL, 3);
 * parent_iter = iter;
 * gtk_tree_model_iter_nth_child (model, &iter,
 *                                &parent_iter, 2);
 * parent_iter = iter;
 * gtk_tree_model_iter_nth_child (model, &iter,
 *                                &parent_iter, 5);
 * ```
 *
 * This second example shows a quick way of iterating through a list
 * and getting a string and an integer from each row. The
 * populate_model() function used below is not
 * shown, as it is specific to the `GtkListStore`. For information on
 * how to write such a function, see the `GtkListStore` documentation.
 *
 * ## Reading data from a `GtkTreeModel`
 *
 * ```c
 * enum
 * {
 *   STRING_COLUMN,
 *   INT_COLUMN,
 *   N_COLUMNS
 * };
 *
 * ...
 *
 * GtkTreeModel *list_store;
 * GtkTreeIter iter;
 * gboolean valid;
 * int row_count = 0;
 *
 * // make a new list_store
 * list_store = gtk_list_store_new (N_COLUMNS,
 *                                  G_TYPE_STRING,
 *                                  G_TYPE_INT);
 *
 * // Fill the list store with data
 * populate_model (list_store);
 *
 * // Get the first iter in the list, check it is valid and walk
 * // through the list, reading each row.
 *
 * valid = gtk_tree_model_get_iter_first (list_store,
 *                                        &iter);
 * while (valid)
 *  {
 *    char *str_data;
 *    int    int_data;
 *
 *    // Make sure you terminate calls to gtk_tree_model_get() with a “-1” value
 *    gtk_tree_model_get (list_store, &iter,
 *                        STRING_COLUMN, &str_data,
 *                        INT_COLUMN, &int_data,
 *                        -1);
 *
 *    // Do something with the data
 *    g_print ("Row %d: (%s,%d)\n",
 *             row_count, str_data, int_data);
 *    g_free (str_data);
 *
 *    valid = gtk_tree_model_iter_next (list_store,
 *                                      &iter);
 *    row_count++;
 *  }
 * ```
 *
 * The `GtkTreeModel` interface contains two methods for reference
 * counting: gtk_tree_model_ref_node() and gtk_tree_model_unref_node().
 * These two methods are optional to implement. The reference counting
 * is meant as a way for views to let models know when nodes are being
 * displayed. `GtkTreeView` will take a reference on a node when it is
 * visible, which means the node is either in the toplevel or expanded.
 * Being displayed does not mean that the node is currently directly
 * visible to the user in the viewport. Based on this reference counting
 * scheme a caching model, for example, can decide whether or not to cache
 * a node based on the reference count. A file-system based model would
 * not want to keep the entire file hierarchy in memory, but just the
 * folders that are currently expanded in every current view.
 *
 * When working with reference counting, the following rules must be taken
 * into account:
 *
 * - Never take a reference on a node without owning a reference on its parent.
 *   This means that all parent nodes of a referenced node must be referenced
 *   as well.
 *
 * - Outstanding references on a deleted node are not released. This is not
 *   possible because the node has already been deleted by the time the
 *   row-deleted signal is received.
 *
 * - Models are not obligated to emit a signal on rows of which none of its
 *   siblings are referenced. To phrase this differently, signals are only
 *   required for levels in which nodes are referenced. For the root level
 *   however, signals must be emitted at all times (however the root level
 *   is always referenced when any view is attached).
 *
 * Deprecated: 4.10: Use [iface@Gio.ListModel] instead
 */

#define INITIALIZE_TREE_ITER(Iter) \
    G_STMT_START{ \
      (Iter)->stamp = 0; \
      (Iter)->user_data  = NULL; \
      (Iter)->user_data2 = NULL; \
      (Iter)->user_data3 = NULL; \
    }G_STMT_END

#define ROW_REF_DATA_STRING "gtk-tree-row-refs"

enum {
  ROW_CHANGED,
  ROW_INSERTED,
  ROW_HAS_CHILD_TOGGLED,
  ROW_DELETED,
  ROWS_REORDERED,
  LAST_SIGNAL
};

static guint tree_model_signals[LAST_SIGNAL] = { 0 };

/**
 * GtkTreePath:
 *
 * An opaque structure representing a path to a row in a model.
 *
 * Deprecated: 4.10
 */
struct _GtkTreePath
{
  int depth;    /* Number of elements */
  int alloc;    /* Number of allocated elements */
  int *indices;
};

typedef struct
{
  GSList *list;
} RowRefList;

static void      gtk_tree_model_base_init   (gpointer           g_class);

/* custom closures */
static void      row_inserted_marshal       (GClosure          *closure,
                                             GValue /* out */  *return_value,
                                             guint              n_param_value,
                                             const GValue      *param_values,
                                             gpointer           invocation_hint,
                                             gpointer           marshal_data);
static void      row_deleted_marshal        (GClosure          *closure,
                                             GValue /* out */  *return_value,
                                             guint              n_param_value,
                                             const GValue      *param_values,
                                             gpointer           invocation_hint,
                                             gpointer           marshal_data);
static void      rows_reordered_marshal     (GClosure          *closure,
                                             GValue /* out */  *return_value,
                                             guint              n_param_value,
                                             const GValue      *param_values,
                                             gpointer           invocation_hint,
                                             gpointer           marshal_data);

static void      gtk_tree_row_ref_inserted  (RowRefList        *refs,
                                             GtkTreePath       *path,
                                             GtkTreeIter       *iter);
static void      gtk_tree_row_ref_deleted   (RowRefList        *refs,
                                             GtkTreePath       *path);
static void      gtk_tree_row_ref_reordered (RowRefList        *refs,
                                             GtkTreePath       *path,
                                             GtkTreeIter       *iter,
                                             int               *new_order);

GType
gtk_tree_model_get_type (void)
{
  static GType tree_model_type = 0;

  if (! tree_model_type)
    {
      const GTypeInfo tree_model_info =
      {
        sizeof (GtkTreeModelIface), /* class_size */
        gtk_tree_model_base_init,   /* base_init */
        NULL,           /* base_finalize */
        NULL,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        0,
        0,              /* n_preallocs */
        NULL
      };

      tree_model_type =
        g_type_register_static (G_TYPE_INTERFACE, I_("GtkTreeModel"),
                                &tree_model_info, 0);

      g_type_interface_add_prerequisite (tree_model_type, G_TYPE_OBJECT);
    }

  return tree_model_type;
}

static void
gtk_tree_model_base_init (gpointer g_class)
{
  static gboolean initialized = FALSE;
  GClosure *closure;

  if (! initialized)
    {
      GType row_inserted_params[2];
      GType row_deleted_params[1];
      GType rows_reordered_params[3];

      row_inserted_params[0] = GTK_TYPE_TREE_PATH | G_SIGNAL_TYPE_STATIC_SCOPE;
      row_inserted_params[1] = GTK_TYPE_TREE_ITER;

      row_deleted_params[0] = GTK_TYPE_TREE_PATH | G_SIGNAL_TYPE_STATIC_SCOPE;

      rows_reordered_params[0] = GTK_TYPE_TREE_PATH | G_SIGNAL_TYPE_STATIC_SCOPE;
      rows_reordered_params[1] = GTK_TYPE_TREE_ITER;
      rows_reordered_params[2] = G_TYPE_POINTER;

      /**
       * GtkTreeModel::row-changed:
       * @tree_model: the `GtkTreeModel` on which the signal is emitted
       * @path: a `GtkTreePath` identifying the changed row
       * @iter: a valid `GtkTreeIter` pointing to the changed row
       *
       * This signal is emitted when a row in the model has changed.
       */
      tree_model_signals[ROW_CHANGED] =
        g_signal_new (I_("row-changed"),
                      GTK_TYPE_TREE_MODEL,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (GtkTreeModelIface, row_changed),
                      NULL, NULL,
                      _gtk_marshal_VOID__BOXED_BOXED,
                      G_TYPE_NONE, 2,
                      GTK_TYPE_TREE_PATH | G_SIGNAL_TYPE_STATIC_SCOPE,
                      GTK_TYPE_TREE_ITER);
      g_signal_set_va_marshaller (tree_model_signals[ROW_CHANGED],
                                  G_TYPE_FROM_CLASS (g_class),
                                  _gtk_marshal_VOID__BOXED_BOXEDv);

      /* We need to get notification about structure changes
       * to update row references., so instead of using the
       * standard g_signal_new() with an offset into our interface
       * structure, we use a customs closures for the class
       * closures (default handlers) that first update row references
       * and then calls the function from the interface structure.
       *
       * The reason we don't simply update the row references from
       * the wrapper functions (gtk_tree_model_row_inserted(), etc.)
       * is to keep proper ordering with respect to signal handlers
       * connected normally and after.
       */

      /**
       * GtkTreeModel::row-inserted:
       * @tree_model: the `GtkTreeModel` on which the signal is emitted
       * @path: a `GtkTreePath` identifying the new row
       * @iter: a valid `GtkTreeIter` pointing to the new row
       *
       * This signal is emitted when a new row has been inserted in
       * the model.
       *
       * Note that the row may still be empty at this point, since
       * it is a common pattern to first insert an empty row, and
       * then fill it with the desired values.
       */
      closure = g_closure_new_simple (sizeof (GClosure), NULL);
      g_closure_set_marshal (closure, row_inserted_marshal);
      tree_model_signals[ROW_INSERTED] =
        g_signal_newv (I_("row-inserted"),
                       GTK_TYPE_TREE_MODEL,
                       G_SIGNAL_RUN_FIRST,
                       closure,
                       NULL, NULL,
                       _gtk_marshal_VOID__BOXED_BOXED,
                       G_TYPE_NONE, 2,
                       row_inserted_params);
      g_signal_set_va_marshaller (tree_model_signals[ROW_INSERTED],
                                  G_TYPE_FROM_CLASS (g_class),
                                  _gtk_marshal_VOID__BOXED_BOXEDv);

      /**
       * GtkTreeModel::row-has-child-toggled:
       * @tree_model: the `GtkTreeModel` on which the signal is emitted
       * @path: a `GtkTreePath` identifying the row
       * @iter: a valid `GtkTreeIter` pointing to the row
       *
       * This signal is emitted when a row has gotten the first child
       * row or lost its last child row.
       */
      tree_model_signals[ROW_HAS_CHILD_TOGGLED] =
        g_signal_new (I_("row-has-child-toggled"),
                      GTK_TYPE_TREE_MODEL,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (GtkTreeModelIface, row_has_child_toggled),
                      NULL, NULL,
                      _gtk_marshal_VOID__BOXED_BOXED,
                      G_TYPE_NONE, 2,
                      GTK_TYPE_TREE_PATH | G_SIGNAL_TYPE_STATIC_SCOPE,
                      GTK_TYPE_TREE_ITER);
      g_signal_set_va_marshaller (tree_model_signals[ROW_HAS_CHILD_TOGGLED],
                                  G_TYPE_FROM_CLASS (g_class),
                                  _gtk_marshal_VOID__BOXED_BOXEDv);

      /**
       * GtkTreeModel::row-deleted:
       * @tree_model: the `GtkTreeModel` on which the signal is emitted
       * @path: a `GtkTreePath` identifying the row
       *
       * This signal is emitted when a row has been deleted.
       *
       * Note that no iterator is passed to the signal handler,
       * since the row is already deleted.
       *
       * This should be called by models after a row has been removed.
       * The location pointed to by @path should be the location that
       * the row previously was at. It may not be a valid location anymore.
       */
      closure = g_closure_new_simple (sizeof (GClosure), NULL);
      g_closure_set_marshal (closure, row_deleted_marshal);
      tree_model_signals[ROW_DELETED] =
        g_signal_newv (I_("row-deleted"),
                       GTK_TYPE_TREE_MODEL,
                       G_SIGNAL_RUN_FIRST,
                       closure,
                       NULL, NULL,
                       NULL,
                       G_TYPE_NONE, 1,
                       row_deleted_params);

      /**
       * GtkTreeModel::rows-reordered: (skip)
       * @tree_model: the `GtkTreeModel` on which the signal is emitted
       * @path: a `GtkTreePath` identifying the tree node whose children
       *     have been reordered
       * @iter: a valid `GtkTreeIter` pointing to the node whose children
       *     have been reordered, or %NULL if the depth of @path is 0
       * @new_order: an array of integers mapping the current position
       *     of each child to its old position before the re-ordering,
       *     i.e. @new_order`[newpos] = oldpos`
       *
       * This signal is emitted when the children of a node in the
       * `GtkTreeModel` have been reordered.
       *
       * Note that this signal is not emitted
       * when rows are reordered by DND, since this is implemented
       * by removing and then reinserting the row.
       */
      closure = g_closure_new_simple (sizeof (GClosure), NULL);
      g_closure_set_marshal (closure, rows_reordered_marshal);
      tree_model_signals[ROWS_REORDERED] =
        g_signal_newv (I_("rows-reordered"),
                       GTK_TYPE_TREE_MODEL,
                       G_SIGNAL_RUN_FIRST,
                       closure,
                       NULL, NULL,
                       _gtk_marshal_VOID__BOXED_BOXED_POINTER,
                       G_TYPE_NONE, 3,
                       rows_reordered_params);
      g_signal_set_va_marshaller (tree_model_signals[ROWS_REORDERED],
                                  G_TYPE_FROM_CLASS (g_class),
                                  _gtk_marshal_VOID__BOXED_BOXED_POINTERv);
      initialized = TRUE;
    }
}

static void
row_inserted_marshal (GClosure          *closure,
                      GValue /* out */  *return_value,
                      guint              n_param_values,
                      const GValue      *param_values,
                      gpointer           invocation_hint,
                      gpointer           marshal_data)
{
  GtkTreeModelIface *iface;

  void (* row_inserted_callback) (GtkTreeModel *tree_model,
                                  GtkTreePath *path,
                                  GtkTreeIter *iter) = NULL;

  GObject *model = g_value_get_object (param_values + 0);
  GtkTreePath *path = (GtkTreePath *)g_value_get_boxed (param_values + 1);
  GtkTreeIter *iter = (GtkTreeIter *)g_value_get_boxed (param_values + 2);

  /* first, we need to update internal row references */
  gtk_tree_row_ref_inserted ((RowRefList *)g_object_get_data (model, ROW_REF_DATA_STRING),
                             path, iter);

  /* fetch the interface ->row_inserted implementation */
  iface = GTK_TREE_MODEL_GET_IFACE (model);
  row_inserted_callback = G_STRUCT_MEMBER (gpointer, iface,
                              G_STRUCT_OFFSET (GtkTreeModelIface,
                                               row_inserted));

  /* Call that default signal handler, it if has been set */
  if (row_inserted_callback)
    row_inserted_callback (GTK_TREE_MODEL (model), path, iter);
}

static void
row_deleted_marshal (GClosure          *closure,
                     GValue /* out */  *return_value,
                     guint              n_param_values,
                     const GValue      *param_values,
                     gpointer           invocation_hint,
                     gpointer           marshal_data)
{
  GtkTreeModelIface *iface;
  void (* row_deleted_callback) (GtkTreeModel *tree_model,
                                 GtkTreePath  *path) = NULL;
  GObject *model = g_value_get_object (param_values + 0);
  GtkTreePath *path = (GtkTreePath *)g_value_get_boxed (param_values + 1);

  /* first, we need to update internal row references */
  gtk_tree_row_ref_deleted ((RowRefList *)g_object_get_data (model, ROW_REF_DATA_STRING),
                            path);

  /* fetch the interface ->row_deleted implementation */
  iface = GTK_TREE_MODEL_GET_IFACE (model);
  row_deleted_callback = G_STRUCT_MEMBER (gpointer, iface,
                              G_STRUCT_OFFSET (GtkTreeModelIface,
                                               row_deleted));

  /* Call that default signal handler, it if has been set */
  if (row_deleted_callback)
    row_deleted_callback (GTK_TREE_MODEL (model), path);
}

static void
rows_reordered_marshal (GClosure          *closure,
                        GValue /* out */  *return_value,
                        guint              n_param_values,
                        const GValue      *param_values,
                        gpointer           invocation_hint,
                        gpointer           marshal_data)
{
  GtkTreeModelIface *iface;
  void (* rows_reordered_callback) (GtkTreeModel *tree_model,
                                    GtkTreePath  *path,
                                    GtkTreeIter  *iter,
                                    int          *new_order);

  GObject *model = g_value_get_object (param_values + 0);
  GtkTreePath *path = (GtkTreePath *)g_value_get_boxed (param_values + 1);
  GtkTreeIter *iter = (GtkTreeIter *)g_value_get_boxed (param_values + 2);
  int *new_order = (int *)g_value_get_pointer (param_values + 3);

  /* first, we need to update internal row references */
  gtk_tree_row_ref_reordered ((RowRefList *)g_object_get_data (model, ROW_REF_DATA_STRING),
                              path, iter, new_order);

  /* fetch the interface ->rows_reordered implementation */
  iface = GTK_TREE_MODEL_GET_IFACE (model);
  rows_reordered_callback = G_STRUCT_MEMBER (gpointer, iface,
                              G_STRUCT_OFFSET (GtkTreeModelIface,
                                               rows_reordered));

  /* Call that default signal handler, it if has been set */
  if (rows_reordered_callback)
    rows_reordered_callback (GTK_TREE_MODEL (model), path, iter, new_order);
}

/**
 * gtk_tree_path_new:
 *
 * Creates a new `GtkTreePath`
 * This refers to a row.
 *
 * Returns: A newly created `GtkTreePath`.
 *
 * Deprecated: 4.10
 */
GtkTreePath *
gtk_tree_path_new (void)
{
  GtkTreePath *retval;
  retval = g_slice_new (GtkTreePath);
  retval->depth = 0;
  retval->alloc = 0;
  retval->indices = NULL;

  return retval;
}

/**
 * gtk_tree_path_new_from_string:
 * @path: The string representation of a path
 *
 * Creates a new `GtkTreePath` initialized to @path.
 *
 * @path is expected to be a colon separated list of numbers.
 * For example, the string “10:4:0” would create a path of depth
 * 3 pointing to the 11th child of the root node, the 5th
 * child of that 11th child, and the 1st child of that 5th child.
 * If an invalid path string is passed in, %NULL is returned.
 *
 * Returns: (nullable): A newly-created `GtkTreePath`
 *
 * Deprecated: 4.10
 */
GtkTreePath *
gtk_tree_path_new_from_string (const char *path)
{
  GtkTreePath *retval;
  const char *orig_path = path;
  char *ptr;
  int i;

  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (*path != '\000', NULL);

  retval = gtk_tree_path_new ();

  while (1)
    {
      i = strtol (path, &ptr, 10);
      if (i < 0)
        {
          g_warning (G_STRLOC ": Negative numbers in path %s passed to gtk_tree_path_new_from_string", orig_path);
          gtk_tree_path_free (retval);
          return NULL;
        }

      gtk_tree_path_append_index (retval, i);

      if (*ptr == '\000')
        break;
      if (ptr == path || *ptr != ':')
        {
          g_warning (G_STRLOC ": Invalid path %s passed to gtk_tree_path_new_from_string", orig_path);
          gtk_tree_path_free (retval);
          return NULL;
        }
      path = ptr + 1;
    }

  return retval;
}

/**
 * gtk_tree_path_new_from_indices:
 * @first_index: first integer
 * @...: list of integers terminated by -1
 *
 * Creates a new path with @first_index and @varargs as indices.
 *
 * Returns: A newly created `GtkTreePath`
 *
 * Deprecated: 4.10
 */
GtkTreePath *
gtk_tree_path_new_from_indices (int first_index,
                                ...)
{
  int arg;
  va_list args;
  GtkTreePath *path;

  path = gtk_tree_path_new ();

  va_start (args, first_index);
  arg = first_index;

  while (arg != -1)
    {
      gtk_tree_path_append_index (path, arg);
      arg = va_arg (args, int);
    }

  va_end (args);

  return path;
}

/**
 * gtk_tree_path_new_from_indicesv: (rename-to gtk_tree_path_new_from_indices)
 * @indices: (array length=length): array of indices
 * @length: length of @indices array
 *
 * Creates a new path with the given @indices array of @length.
 *
 * Returns: A newly created `GtkTreePath`
 *
 * Deprecated: 4.10
 */
GtkTreePath *
gtk_tree_path_new_from_indicesv (int *indices,
                                 gsize length)
{
  GtkTreePath *path;

  g_return_val_if_fail (indices != NULL && length != 0, NULL);

  path = gtk_tree_path_new ();
  path->alloc = length;
  path->depth = length;
  path->indices = g_new (int, length);
  memcpy (path->indices, indices, length * sizeof (int));

  return path;
}

/**
 * gtk_tree_path_to_string:
 * @path: a `GtkTreePath`
 *
 * Generates a string representation of the path.
 *
 * This string is a “:” separated list of numbers.
 * For example, “4:10:0:3” would be an acceptable
 * return value for this string. If the path has
 * depth 0, %NULL is returned.
 *
 * Returns: (nullable): A newly-allocated string
 *
 * Deprecated: 4.10
 */
char *
gtk_tree_path_to_string (GtkTreePath *path)
{
  char *retval, *ptr, *end;
  int i, n;

  g_return_val_if_fail (path != NULL, NULL);

  if (path->depth == 0)
    return NULL;

  n = path->depth * 12;
  ptr = retval = g_new0 (char, n);
  end = ptr + n;
  g_snprintf (retval, end - ptr, "%d", path->indices[0]);
  while (*ptr != '\000')
    ptr++;

  for (i = 1; i < path->depth; i++)
    {
      g_snprintf (ptr, end - ptr, ":%d", path->indices[i]);
      while (*ptr != '\000')
        ptr++;
    }

  return retval;
}

/**
 * gtk_tree_path_new_first:
 *
 * Creates a new `GtkTreePath`.
 *
 * The string representation of this path is “0”.
 *
 * Returns: A new `GtkTreePath`
 *
 * Deprecated: 4.10
 */
GtkTreePath *
gtk_tree_path_new_first (void)
{
  GtkTreePath *retval;

  retval = gtk_tree_path_new ();
  gtk_tree_path_append_index (retval, 0);

  return retval;
}

/**
 * gtk_tree_path_append_index:
 * @path: a `GtkTreePath`
 * @index_: the index
 *
 * Appends a new index to a path.
 *
 * As a result, the depth of the path is increased.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_path_append_index (GtkTreePath *path,
                            int          index_)
{
  g_return_if_fail (path != NULL);
  g_return_if_fail (index_ >= 0);

  if (path->depth == path->alloc)
    {
      path->alloc = MAX (path->alloc * 2, 1);
      path->indices = g_renew (int, path->indices, path->alloc);
    }

  path->depth += 1;
  path->indices[path->depth - 1] = index_;
}

/**
 * gtk_tree_path_prepend_index:
 * @path: a `GtkTreePath`
 * @index_: the index
 *
 * Prepends a new index to a path.
 *
 * As a result, the depth of the path is increased.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_path_prepend_index (GtkTreePath *path,
                             int        index)
{
  if (path->depth == path->alloc)
    {
      int *indices;
      path->alloc = MAX (path->alloc * 2, 1);
      indices = g_new (int, path->alloc);
      if (path->depth > 0)
        memcpy (indices + 1, path->indices, path->depth * sizeof (int));
      g_free (path->indices);
      path->indices = indices;
    }
  else if (path->depth > 0)
    memmove (path->indices + 1, path->indices, path->depth * sizeof (int));

  path->depth += 1;
  path->indices[0] = index;
}

/**
 * gtk_tree_path_get_depth:
 * @path: a `GtkTreePath`
 *
 * Returns the current depth of @path.
 *
 * Returns: The depth of @path
 *
 * Deprecated: 4.10
 */
int
gtk_tree_path_get_depth (GtkTreePath *path)
{
  g_return_val_if_fail (path != NULL, 0);

  return path->depth;
}

/**
 * gtk_tree_path_get_indices: (skip)
 * @path: a `GtkTreePath`
 *
 * Returns the current indices of @path.
 *
 * This is an array of integers, each representing a node in a tree.
 * This value should not be freed.
 *
 * The length of the array can be obtained with gtk_tree_path_get_depth().
 *
 * Returns: (nullable) (transfer none): The current indices
 *
 * Deprecated: 4.10
 */
int *
gtk_tree_path_get_indices (GtkTreePath *path)
{
  g_return_val_if_fail (path != NULL, NULL);

  return path->indices;
}

/**
 * gtk_tree_path_get_indices_with_depth: (rename-to gtk_tree_path_get_indices)
 * @path: a `GtkTreePath`
 * @depth: (out) (optional): return location for number of elements
 *   returned in the integer array
 *
 * Returns the current indices of @path.
 *
 * This is an array of integers, each representing a node in a tree.
 * It also returns the number of elements in the array.
 * The array should not be freed.
 *
 * Returns: (array length=depth) (transfer none) (nullable): The current
 *   indices
 *
 * Deprecated: 4.10
 */
int *
gtk_tree_path_get_indices_with_depth (GtkTreePath *path,
                                      int         *depth)
{
  g_return_val_if_fail (path != NULL, NULL);

  if (depth)
    *depth = path->depth;

  return path->indices;
}

/**
 * gtk_tree_path_free:
 * @path: (nullable): a `GtkTreePath`
 *
 * Frees @path. If @path is %NULL, it simply returns.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_path_free (GtkTreePath *path)
{
  if (!path)
    return;

  g_free (path->indices);
  g_slice_free (GtkTreePath, path);
}

/**
 * gtk_tree_path_copy:
 * @path: a `GtkTreePath`
 *
 * Creates a new `GtkTreePath` as a copy of @path.
 *
 * Returns: a new `GtkTreePath`
 *
 * Deprecated: 4.10
 */
GtkTreePath *
gtk_tree_path_copy (const GtkTreePath *path)
{
  GtkTreePath *retval;

  g_return_val_if_fail (path != NULL, NULL);

  retval = g_slice_new (GtkTreePath);
  retval->depth = path->depth;
  retval->alloc = retval->depth;
  retval->indices = g_new (int, path->alloc);
  if (path->depth > 0)
    memcpy (retval->indices, path->indices, path->depth * sizeof (int));
  return retval;
}

G_DEFINE_BOXED_TYPE (GtkTreePath, gtk_tree_path,
                     gtk_tree_path_copy,
                     gtk_tree_path_free)

/**
 * gtk_tree_path_compare:
 * @a: a `GtkTreePath`
 * @b: a `GtkTreePath` to compare with
 *
 * Compares two paths.
 *
 * If @a appears before @b in a tree, then -1 is returned.
 * If @b appears before @a, then 1 is returned.
 * If the two nodes are equal, then 0 is returned.
 *
 * Returns: the relative positions of @a and @b
 *
 * Deprecated: 4.10
 */
int
gtk_tree_path_compare (const GtkTreePath *a,
                       const GtkTreePath *b)
{
  int p = 0, q = 0;

  g_return_val_if_fail (a != NULL, 0);
  g_return_val_if_fail (b != NULL, 0);
  g_return_val_if_fail (a->depth > 0, 0);
  g_return_val_if_fail (b->depth > 0, 0);

  do
    {
      if (a->indices[p] == b->indices[q])
        continue;
      return (a->indices[p] < b->indices[q]?-1:1);
    }
  while (++p < a->depth && ++q < b->depth);
  if (a->depth == b->depth)
    return 0;
  return (a->depth < b->depth?-1:1);
}

/**
 * gtk_tree_path_is_ancestor:
 * @path: a `GtkTreePath`
 * @descendant: another `GtkTreePath`
 *
 * Returns %TRUE if @descendant is a descendant of @path.
 *
 * Returns: %TRUE if @descendant is contained inside @path
 *
 * Deprecated: 4.10
 */
gboolean
gtk_tree_path_is_ancestor (GtkTreePath *path,
                           GtkTreePath *descendant)
{
  int i;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (descendant != NULL, FALSE);

  /* can't be an ancestor if we're deeper */
  if (path->depth >= descendant->depth)
    return FALSE;

  i = 0;
  while (i < path->depth)
    {
      if (path->indices[i] != descendant->indices[i])
        return FALSE;
      ++i;
    }

  return TRUE;
}

/**
 * gtk_tree_path_is_descendant:
 * @path: a `GtkTreePath`
 * @ancestor: another `GtkTreePath`
 *
 * Returns %TRUE if @path is a descendant of @ancestor.
 *
 * Returns: %TRUE if @ancestor contains @path somewhere below it
 *
 * Deprecated: 4.10
 */
gboolean
gtk_tree_path_is_descendant (GtkTreePath *path,
                             GtkTreePath *ancestor)
{
  int i;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (ancestor != NULL, FALSE);

  /* can't be a descendant if we're shallower in the tree */
  if (path->depth <= ancestor->depth)
    return FALSE;

  i = 0;
  while (i < ancestor->depth)
    {
      if (path->indices[i] != ancestor->indices[i])
        return FALSE;
      ++i;
    }

  return TRUE;
}


/**
 * gtk_tree_path_next:
 * @path: a `GtkTreePath`
 *
 * Moves the @path to point to the next node at the current depth.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_path_next (GtkTreePath *path)
{
  g_return_if_fail (path != NULL);
  g_return_if_fail (path->depth > 0);

  path->indices[path->depth - 1] ++;
}

/**
 * gtk_tree_path_prev:
 * @path: a `GtkTreePath`
 *
 * Moves the @path to point to the previous node at the
 * current depth, if it exists.
 *
 * Returns: %TRUE if @path has a previous node, and
 *   the move was made
 *
 * Deprecated: 4.10
 */
gboolean
gtk_tree_path_prev (GtkTreePath *path)
{
  g_return_val_if_fail (path != NULL, FALSE);

  if (path->depth == 0)
    return FALSE;

  if (path->indices[path->depth - 1] == 0)
    return FALSE;

  path->indices[path->depth - 1] -= 1;

  return TRUE;
}

/**
 * gtk_tree_path_up:
 * @path: a `GtkTreePath`
 *
 * Moves the @path to point to its parent node, if it has a parent.
 *
 * Returns: %TRUE if @path has a parent, and the move was made
 *
 * Deprecated: 4.10
 */
gboolean
gtk_tree_path_up (GtkTreePath *path)
{
  g_return_val_if_fail (path != NULL, FALSE);

  if (path->depth == 0)
    return FALSE;

  path->depth--;

  return TRUE;
}

/**
 * gtk_tree_path_down:
 * @path: a `GtkTreePath`
 *
 * Moves @path to point to the first child of the current path.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_path_down (GtkTreePath *path)
{
  g_return_if_fail (path != NULL);

  gtk_tree_path_append_index (path, 0);
}

/**
 * gtk_tree_iter_copy:
 * @iter: a `GtkTreeIter`
 *
 * Creates a dynamically allocated tree iterator as a copy of @iter.
 *
 * This function is not intended for use in applications,
 * because you can just copy the structs by value
 * (`GtkTreeIter new_iter = iter;`).
 * You must free this iter with gtk_tree_iter_free().
 *
 * Returns: a newly-allocated copy of @iter
 *
 * Deprecated: 4.10
 */
GtkTreeIter *
gtk_tree_iter_copy (GtkTreeIter *iter)
{
  GtkTreeIter *retval;

  g_return_val_if_fail (iter != NULL, NULL);

  retval = g_slice_new (GtkTreeIter);
  *retval = *iter;

  return retval;
}

/**
 * gtk_tree_iter_free:
 * @iter: a dynamically allocated tree iterator
 *
 * Frees an iterator that has been allocated by gtk_tree_iter_copy().
 *
 * This function is mainly used for language bindings.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_iter_free (GtkTreeIter *iter)
{
  g_return_if_fail (iter != NULL);

  g_slice_free (GtkTreeIter, iter);
}

G_DEFINE_BOXED_TYPE (GtkTreeIter,  gtk_tree_iter,
                     gtk_tree_iter_copy,
                     gtk_tree_iter_free)

/**
 * gtk_tree_model_get_flags:
 * @tree_model: a `GtkTreeModel`
 *
 * Returns a set of flags supported by this interface.
 *
 * The flags are a bitwise combination of `GtkTreeModel`Flags.
 * The flags supported should not change during the lifetime
 * of the @tree_model.
 *
 * Returns: the flags supported by this interface
 *
 * Deprecated: 4.10
 */
GtkTreeModelFlags
gtk_tree_model_get_flags (GtkTreeModel *tree_model)
{
  GtkTreeModelIface *iface;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), 0);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  if (iface->get_flags)
    return (* iface->get_flags) (tree_model);

  return 0;
}

/**
 * gtk_tree_model_get_n_columns:
 * @tree_model: a `GtkTreeModel`
 *
 * Returns the number of columns supported by @tree_model.
 *
 * Returns: the number of columns
 *
 * Deprecated: 4.10
 */
int
gtk_tree_model_get_n_columns (GtkTreeModel *tree_model)
{
  GtkTreeModelIface *iface;
  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), 0);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  g_return_val_if_fail (iface->get_n_columns != NULL, 0);

  return (* iface->get_n_columns) (tree_model);
}

/**
 * gtk_tree_model_get_column_type:
 * @tree_model: a `GtkTreeModel`
 * @index_: the column index
 *
 * Returns the type of the column.
 *
 * Returns: the type of the column
 *
 * Deprecated: 4.10
 */
GType
gtk_tree_model_get_column_type (GtkTreeModel *tree_model,
                                int           index)
{
  GtkTreeModelIface *iface;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), G_TYPE_INVALID);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  g_return_val_if_fail (iface->get_column_type != NULL, G_TYPE_INVALID);
  g_return_val_if_fail (index >= 0, G_TYPE_INVALID);

  return (* iface->get_column_type) (tree_model, index);
}

/**
 * gtk_tree_model_get_iter:
 * @tree_model: a `GtkTreeModel`
 * @iter: (out): the uninitialized `GtkTreeIter`
 * @path: the `GtkTreePath`
 *
 * Sets @iter to a valid iterator pointing to @path.
 *
 * If @path does not exist, @iter is set to an invalid
 * iterator and %FALSE is returned.
 *
 * Returns: %TRUE, if @iter was set
 *
 * Deprecated: 4.10
 */
gboolean
gtk_tree_model_get_iter (GtkTreeModel *tree_model,
                         GtkTreeIter  *iter,
                         GtkTreePath  *path)
{
  GtkTreeModelIface *iface;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  g_return_val_if_fail (iface->get_iter != NULL, FALSE);
  g_return_val_if_fail (path->depth > 0, FALSE);

  INITIALIZE_TREE_ITER (iter);

  return (* iface->get_iter) (tree_model, iter, path);
}

/**
 * gtk_tree_model_get_iter_from_string:
 * @tree_model: a `GtkTreeModel`
 * @iter: (out): an uninitialized `GtkTreeIter`
 * @path_string: a string representation of a `GtkTreePath`
 *
 * Sets @iter to a valid iterator pointing to @path_string, if it
 * exists.
 *
 * Otherwise, @iter is left invalid and %FALSE is returned.
 *
 * Returns: %TRUE, if @iter was set
 *
 * Deprecated: 4.10
 */
gboolean
gtk_tree_model_get_iter_from_string (GtkTreeModel *tree_model,
                                     GtkTreeIter  *iter,
                                     const char   *path_string)
{
  gboolean retval;
  GtkTreePath *path;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (path_string != NULL, FALSE);

  path = gtk_tree_path_new_from_string (path_string);

  g_return_val_if_fail (path != NULL, FALSE);

  retval = gtk_tree_model_get_iter (tree_model, iter, path);
  gtk_tree_path_free (path);

  return retval;
}

/**
 * gtk_tree_model_get_string_from_iter:
 * @tree_model: a `GtkTreeModel`
 * @iter: a `GtkTreeIter`
 *
 * Generates a string representation of the iter.
 *
 * This string is a “:” separated list of numbers.
 * For example, “4:10:0:3” would be an acceptable
 * return value for this string.
 *
 * Returns: (nullable): a newly-allocated string
 *
 * Deprecated: 4.10
 */
char *
gtk_tree_model_get_string_from_iter (GtkTreeModel *tree_model,
                                     GtkTreeIter  *iter)
{
  GtkTreePath *path;
  char *ret;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), NULL);
  g_return_val_if_fail (iter != NULL, NULL);

  path = gtk_tree_model_get_path (tree_model, iter);

  g_return_val_if_fail (path != NULL, NULL);

  ret = gtk_tree_path_to_string (path);
  gtk_tree_path_free (path);

  return ret;
}

/**
 * gtk_tree_model_get_iter_first:
 * @tree_model: a `GtkTreeModel`
 * @iter: (out): the uninitialized `GtkTreeIter`
 *
 * Initializes @iter with the first iterator in the tree
 * (the one at the path "0").
 *
 * Returns %FALSE if the tree is empty, %TRUE otherwise.
 *
 * Returns: %TRUE, if @iter was set
 *
 * Deprecated: 4.10
 */
gboolean
gtk_tree_model_get_iter_first (GtkTreeModel *tree_model,
                               GtkTreeIter  *iter)
{
  GtkTreePath *path;
  gboolean retval;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  path = gtk_tree_path_new_first ();
  retval = gtk_tree_model_get_iter (tree_model, iter, path);
  gtk_tree_path_free (path);

  return retval;
}

/**
 * gtk_tree_model_get_path:
 * @tree_model: a `GtkTreeModel`
 * @iter: the `GtkTreeIter`
 *
 * Returns a newly-created `GtkTreePath` referenced by @iter.
 *
 * This path should be freed with gtk_tree_path_free().
 *
 * Returns: a newly-created `GtkTreePath`
 *
 * Deprecated: 4.10
 */
GtkTreePath *
gtk_tree_model_get_path (GtkTreeModel *tree_model,
                         GtkTreeIter  *iter)
{
  GtkTreeModelIface *iface;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), NULL);
  g_return_val_if_fail (iter != NULL, NULL);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  g_return_val_if_fail (iface->get_path != NULL, NULL);

  return (* iface->get_path) (tree_model, iter);
}

/**
 * gtk_tree_model_get_value:
 * @tree_model: a `GtkTreeModel`
 * @iter: the `GtkTreeIter`
 * @column: the column to lookup the value at
 * @value: (out) (transfer none): an empty `GValue` to set
 *
 * Initializes and sets @value to that at @column.
 *
 * When done with @value, g_value_unset() needs to be called
 * to free any allocated memory.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_model_get_value (GtkTreeModel *tree_model,
                          GtkTreeIter  *iter,
                          int           column,
                          GValue       *value)
{
  GtkTreeModelIface *iface;

  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (value != NULL);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  g_return_if_fail (iface->get_value != NULL);

  (* iface->get_value) (tree_model, iter, column, value);
}

/**
 * gtk_tree_model_iter_next:
 * @tree_model: a `GtkTreeModel`
 * @iter: (in): the `GtkTreeIter`
 *
 * Sets @iter to point to the node following it at the current level.
 *
 * If there is no next @iter, %FALSE is returned and @iter is set
 * to be invalid.
 *
 * Returns: %TRUE if @iter has been changed to the next node
 *
 * Deprecated: 4.10
 */
gboolean
gtk_tree_model_iter_next (GtkTreeModel  *tree_model,
                          GtkTreeIter   *iter)
{
  GtkTreeModelIface *iface;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  g_return_val_if_fail (iface->iter_next != NULL, FALSE);

  return (* iface->iter_next) (tree_model, iter);
}

static gboolean
gtk_tree_model_iter_previous_default (GtkTreeModel *tree_model,
                                      GtkTreeIter  *iter)
{
  gboolean retval;
  GtkTreePath *path;

  path = gtk_tree_model_get_path (tree_model, iter);
  if (path == NULL)
    return FALSE;

  retval = gtk_tree_path_prev (path) &&
           gtk_tree_model_get_iter (tree_model, iter, path);
  if (retval == FALSE)
    iter->stamp = 0;

  gtk_tree_path_free (path);

  return retval;
}

/**
 * gtk_tree_model_iter_previous:
 * @tree_model: a `GtkTreeModel`
 * @iter: (in): the `GtkTreeIter`
 *
 * Sets @iter to point to the previous node at the current level.
 *
 * If there is no previous @iter, %FALSE is returned and @iter is
 * set to be invalid.
 *
 * Returns: %TRUE if @iter has been changed to the previous node
 *
 * Deprecated: 4.10
 */
gboolean
gtk_tree_model_iter_previous (GtkTreeModel *tree_model,
                              GtkTreeIter  *iter)
{
  gboolean retval;
  GtkTreeModelIface *iface;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);

  if (iface->iter_previous)
    retval = (* iface->iter_previous) (tree_model, iter);
  else
    retval = gtk_tree_model_iter_previous_default (tree_model, iter);

  return retval;
}

/**
 * gtk_tree_model_iter_children:
 * @tree_model: a `GtkTreeModel`
 * @iter: (out): the new `GtkTreeIter` to be set to the child
 * @parent: (nullable): the `GtkTreeIter`
 *
 * Sets @iter to point to the first child of @parent.
 *
 * If @parent has no children, %FALSE is returned and @iter is
 * set to be invalid. @parent will remain a valid node after this
 * function has been called.
 *
 * If @parent is %NULL returns the first node, equivalent to
 * `gtk_tree_model_get_iter_first (tree_model, iter);`
 *
 * Returns: %TRUE, if @iter has been set to the first child
 *
 * Deprecated: 4.10
 */
gboolean
gtk_tree_model_iter_children (GtkTreeModel *tree_model,
                              GtkTreeIter  *iter,
                              GtkTreeIter  *parent)
{
  GtkTreeModelIface *iface;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  g_return_val_if_fail (iface->iter_children != NULL, FALSE);

  INITIALIZE_TREE_ITER (iter);

  return (* iface->iter_children) (tree_model, iter, parent);
}

/**
 * gtk_tree_model_iter_has_child:
 * @tree_model: a `GtkTreeModel`
 * @iter: the `GtkTreeIter` to test for children
 *
 * Returns %TRUE if @iter has children, %FALSE otherwise.
 *
 * Returns: %TRUE if @iter has children
 *
 * Deprecated: 4.10
 */
gboolean
gtk_tree_model_iter_has_child (GtkTreeModel *tree_model,
                               GtkTreeIter  *iter)
{
  GtkTreeModelIface *iface;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  g_return_val_if_fail (iface->iter_has_child != NULL, FALSE);

  return (* iface->iter_has_child) (tree_model, iter);
}

/**
 * gtk_tree_model_iter_n_children:
 * @tree_model: a `GtkTreeModel`
 * @iter: (nullable): the `GtkTreeIter`
 *
 * Returns the number of children that @iter has.
 *
 * As a special case, if @iter is %NULL, then the number
 * of toplevel nodes is returned.
 *
 * Returns: the number of children of @iter
 *
 * Deprecated: 4.10
 */
int
gtk_tree_model_iter_n_children (GtkTreeModel *tree_model,
                                GtkTreeIter  *iter)
{
  GtkTreeModelIface *iface;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), 0);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  g_return_val_if_fail (iface->iter_n_children != NULL, 0);

  return (* iface->iter_n_children) (tree_model, iter);
}

/**
 * gtk_tree_model_iter_nth_child:
 * @tree_model: a `GtkTreeModel`
 * @iter: (out): the `GtkTreeIter` to set to the nth child
 * @parent: (nullable): the `GtkTreeIter` to get the child from
 * @n: the index of the desired child
 *
 * Sets @iter to be the child of @parent, using the given index.
 *
 * The first index is 0. If @n is too big, or @parent has no children,
 * @iter is set to an invalid iterator and %FALSE is returned. @parent
 * will remain a valid node after this function has been called. As a
 * special case, if @parent is %NULL, then the @n-th root node
 * is set.
 *
 * Returns: %TRUE, if @parent has an @n-th child
 *
 * Deprecated: 4.10
 */
gboolean
gtk_tree_model_iter_nth_child (GtkTreeModel *tree_model,
                               GtkTreeIter  *iter,
                               GtkTreeIter  *parent,
                               int           n)
{
  GtkTreeModelIface *iface;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (n >= 0, FALSE);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  g_return_val_if_fail (iface->iter_nth_child != NULL, FALSE);

  INITIALIZE_TREE_ITER (iter);

  return (* iface->iter_nth_child) (tree_model, iter, parent, n);
}

/**
 * gtk_tree_model_iter_parent:
 * @tree_model: a `GtkTreeModel`
 * @iter: (out): the new `GtkTreeIter` to set to the parent
 * @child: the `GtkTreeIter`
 *
 * Sets @iter to be the parent of @child.
 *
 * If @child is at the toplevel, and doesn’t have a parent, then
 * @iter is set to an invalid iterator and %FALSE is returned.
 * @child will remain a valid node after this function has been
 * called.
 *
 * @iter will be initialized before the lookup is performed, so @child
 * and @iter cannot point to the same memory location.
 *
 * Returns: %TRUE, if @iter is set to the parent of @child
 *
 * Deprecated: 4.10
 */
gboolean
gtk_tree_model_iter_parent (GtkTreeModel *tree_model,
                            GtkTreeIter  *iter,
                            GtkTreeIter  *child)
{
  GtkTreeModelIface *iface;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (child != NULL, FALSE);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  g_return_val_if_fail (iface->iter_parent != NULL, FALSE);

  INITIALIZE_TREE_ITER (iter);

  return (* iface->iter_parent) (tree_model, iter, child);
}

/**
 * gtk_tree_model_ref_node:
 * @tree_model: a `GtkTreeModel`
 * @iter: the `GtkTreeIter`
 *
 * Lets the tree ref the node.
 *
 * This is an optional method for models to implement.
 * To be more specific, models may ignore this call as it exists
 * primarily for performance reasons.
 *
 * This function is primarily meant as a way for views to let
 * caching models know when nodes are being displayed (and hence,
 * whether or not to cache that node). Being displayed means a node
 * is in an expanded branch, regardless of whether the node is currently
 * visible in the viewport. For example, a file-system based model
 * would not want to keep the entire file-hierarchy in memory,
 * just the sections that are currently being displayed by
 * every current view.
 *
 * A model should be expected to be able to get an iter independent
 * of its reffed state.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_model_ref_node (GtkTreeModel *tree_model,
                         GtkTreeIter  *iter)
{
  GtkTreeModelIface *iface;

  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  if (iface->ref_node)
    (* iface->ref_node) (tree_model, iter);
}

/**
 * gtk_tree_model_unref_node:
 * @tree_model: a `GtkTreeModel`
 * @iter: the `GtkTreeIter`
 *
 * Lets the tree unref the node.
 *
 * This is an optional method for models to implement.
 * To be more specific, models may ignore this call as it exists
 * primarily for performance reasons. For more information on what
 * this means, see gtk_tree_model_ref_node().
 *
 * Please note that nodes that are deleted are not unreffed.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_model_unref_node (GtkTreeModel *tree_model,
                           GtkTreeIter  *iter)
{
  GtkTreeModelIface *iface;

  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (iter != NULL);

  iface = GTK_TREE_MODEL_GET_IFACE (tree_model);
  if (iface->unref_node)
    (* iface->unref_node) (tree_model, iter);
}

/**
 * gtk_tree_model_get:
 * @tree_model: a `GtkTreeModel`
 * @iter: a row in @tree_model
 * @...: pairs of column number and value return locations,
 *   terminated by -1
 *
 * Gets the value of one or more cells in the row referenced by @iter.
 *
 * The variable argument list should contain integer column numbers,
 * each column number followed by a place to store the value being
 * retrieved.  The list is terminated by a -1. For example, to get a
 * value from column 0 with type %G_TYPE_STRING, you would
 * write: `gtk_tree_model_get (model, iter, 0, &place_string_here, -1)`,
 * where `place_string_here` is a #gchararray
 * to be filled with the string.
 *
 * Returned values with type %G_TYPE_OBJECT have to be unreferenced,
 * values with type %G_TYPE_STRING or %G_TYPE_BOXED have to be freed.
 * Other values are passed by value.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_model_get (GtkTreeModel *tree_model,
                    GtkTreeIter  *iter,
                    ...)
{
  va_list var_args;

  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (iter != NULL);

  va_start (var_args, iter);
  gtk_tree_model_get_valist (tree_model, iter, var_args);
  va_end (var_args);
}

/**
 * gtk_tree_model_get_valist:
 * @tree_model: a `GtkTreeModel`
 * @iter: a row in @tree_model
 * @var_args: va_list of column/return location pairs
 *
 * Gets the value of one or more cells in the row referenced by @iter.
 *
 * See [method@Gtk.TreeModel.get], this version takes a va_list
 * for language bindings to use.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_model_get_valist (GtkTreeModel *tree_model,
                           GtkTreeIter  *iter,
                           va_list      var_args)
{
  int column;

  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (iter != NULL);

  column = va_arg (var_args, int);

  while (column != -1)
    {
      GValue value = G_VALUE_INIT;
      char *error = NULL;

      if (column >= gtk_tree_model_get_n_columns (tree_model))
        {
          g_warning ("%s: Invalid column number %d accessed (remember to end your list of columns with a -1)", G_STRLOC, column);
          break;
        }

      gtk_tree_model_get_value (GTK_TREE_MODEL (tree_model), iter, column, &value);

      G_VALUE_LCOPY (&value, var_args, 0, &error);
      if (error)
        {
          g_warning ("%s: %s", G_STRLOC, error);
          g_free (error);

          /* we purposely leak the value here, it might not be
           * in a sane state if an error condition occurred
           */
          break;
        }

      g_value_unset (&value);

      column = va_arg (var_args, int);
    }
}

/**
 * gtk_tree_model_row_changed:
 * @tree_model: a `GtkTreeModel`
 * @path: a `GtkTreePath` pointing to the changed row
 * @iter: a valid `GtkTreeIter` pointing to the changed row
 *
 * Emits the ::row-changed signal on @tree_model.
 *
 * See [signal@Gtk.TreeModel::row-changed].
 *
 * Deprecated: 4.10
 */
void
gtk_tree_model_row_changed (GtkTreeModel *tree_model,
                            GtkTreePath  *path,
                            GtkTreeIter  *iter)
{
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (path != NULL);
  g_return_if_fail (iter != NULL);

  g_signal_emit (tree_model, tree_model_signals[ROW_CHANGED], 0, path, iter);
}

/**
 * gtk_tree_model_row_inserted:
 * @tree_model: a `GtkTreeModel`
 * @path: a `GtkTreePath` pointing to the inserted row
 * @iter: a valid `GtkTreeIter` pointing to the inserted row
 *
 * Emits the ::row-inserted signal on @tree_model.
 *
 * See [signal@Gtk.TreeModel::row-inserted].
 *
 * Deprecated: 4.10
 */
void
gtk_tree_model_row_inserted (GtkTreeModel *tree_model,
                             GtkTreePath  *path,
                             GtkTreeIter  *iter)
{
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (path != NULL);
  g_return_if_fail (iter != NULL);

  g_signal_emit (tree_model, tree_model_signals[ROW_INSERTED], 0, path, iter);
}

/**
 * gtk_tree_model_row_has_child_toggled:
 * @tree_model: a `GtkTreeModel`
 * @path: a `GtkTreePath` pointing to the changed row
 * @iter: a valid `GtkTreeIter` pointing to the changed row
 *
 * Emits the ::row-has-child-toggled signal on @tree_model.
 *
 * See [signal@Gtk.TreeModel::row-has-child-toggled].
 *
 * This should be called by models after the child
 * state of a node changes.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_model_row_has_child_toggled (GtkTreeModel *tree_model,
                                      GtkTreePath  *path,
                                      GtkTreeIter  *iter)
{
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (path != NULL);
  g_return_if_fail (iter != NULL);

  g_signal_emit (tree_model, tree_model_signals[ROW_HAS_CHILD_TOGGLED], 0, path, iter);
}

/**
 * gtk_tree_model_row_deleted:
 * @tree_model: a `GtkTreeModel`
 * @path: a `GtkTreePath` pointing to the previous location of
 *   the deleted row
 *
 * Emits the ::row-deleted signal on @tree_model.
 *
 * See [signal@Gtk.TreeModel::row-deleted].
 *
 * This should be called by models after a row has been removed.
 * The location pointed to by @path should be the location that
 * the row previously was at. It may not be a valid location anymore.
 *
 * Nodes that are deleted are not unreffed, this means that any
 * outstanding references on the deleted node should not be released.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_model_row_deleted (GtkTreeModel *tree_model,
                            GtkTreePath  *path)
{
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (path != NULL);

  g_signal_emit (tree_model, tree_model_signals[ROW_DELETED], 0, path);
}

/**
 * gtk_tree_model_rows_reordered: (skip)
 * @tree_model: a `GtkTreeModel`
 * @path: a `GtkTreePath` pointing to the tree node whose children
 *   have been reordered
 * @iter: a valid `GtkTreeIter` pointing to the node whose children
 *   have been reordered, or %NULL if the depth of @path is 0
 * @new_order: an array of integers mapping the current position of
 *   each child to its old position before the re-ordering,
 *   i.e. @new_order`[newpos] = oldpos`
 *
 * Emits the ::rows-reordered signal on @tree_model.
 *
 * See [signal@Gtk.TreeModel::rows-reordered].
 *
 * This should be called by models when their rows have been
 * reordered.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_model_rows_reordered (GtkTreeModel *tree_model,
                               GtkTreePath  *path,
                               GtkTreeIter  *iter,
                               int          *new_order)
{
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (new_order != NULL);

  g_signal_emit (tree_model, tree_model_signals[ROWS_REORDERED], 0, path, iter, new_order);
}

/**
 * gtk_tree_model_rows_reordered_with_length: (rename-to gtk_tree_model_rows_reordered)
 * @tree_model: a `GtkTreeModel`
 * @path: a `GtkTreePath` pointing to the tree node whose children
 *   have been reordered
 * @iter: (nullable): a valid `GtkTreeIter` pointing to the node
 *   whose children have been reordered, or %NULL if the depth
 *   of @path is 0
 * @new_order: (array length=length): an array of integers
 *   mapping the current position of each child to its old
 *   position before the re-ordering,
 *   i.e. @new_order`[newpos] = oldpos`
 * @length: length of @new_order array
 *
 * Emits the ::rows-reordered signal on @tree_model.
 *
 * See [signal@Gtk.TreeModel::rows-reordered].
 *
 * This should be called by models when their rows have been
 * reordered.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_model_rows_reordered_with_length (GtkTreeModel *tree_model,
                                           GtkTreePath  *path,
                                           GtkTreeIter  *iter,
                                           int          *new_order,
                                           int           length)
{
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (new_order != NULL);
  g_return_if_fail (length == gtk_tree_model_iter_n_children (tree_model, iter));

  g_signal_emit (tree_model, tree_model_signals[ROWS_REORDERED], 0, path, iter, new_order);
}

static gboolean
gtk_tree_model_foreach_helper (GtkTreeModel            *model,
                               GtkTreeIter             *iter,
                               GtkTreePath             *path,
                               GtkTreeModelForeachFunc  func,
                               gpointer                 user_data)
{
  gboolean iters_persist;

  iters_persist = gtk_tree_model_get_flags (model) & GTK_TREE_MODEL_ITERS_PERSIST;

  do
    {
      GtkTreeIter child;

      if ((* func) (model, path, iter, user_data))
        return TRUE;

      if (!iters_persist)
        {
          if (!gtk_tree_model_get_iter (model, iter, path))
            return TRUE;
        }

      if (gtk_tree_model_iter_children (model, &child, iter))
        {
          gtk_tree_path_down (path);
          if (gtk_tree_model_foreach_helper (model, &child, path, func, user_data))
            return TRUE;
          gtk_tree_path_up (path);
        }

      gtk_tree_path_next (path);
    }
  while (gtk_tree_model_iter_next (model, iter));

  return FALSE;
}

/**
 * gtk_tree_model_foreach:
 * @model: a `GtkTreeModel`
 * @func: (scope call): a function to be called on each row
 * @user_data: (closure): user data to passed to @func
 *
 * Calls @func on each node in model in a depth-first fashion.
 *
 * If @func returns %TRUE, then the tree ceases to be walked,
 * and gtk_tree_model_foreach() returns.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_model_foreach (GtkTreeModel            *model,
                        GtkTreeModelForeachFunc  func,
                        gpointer                 user_data)
{
  GtkTreePath *path;
  GtkTreeIter iter;

  g_return_if_fail (GTK_IS_TREE_MODEL (model));
  g_return_if_fail (func != NULL);

  path = gtk_tree_path_new_first ();
  if (!gtk_tree_model_get_iter (model, &iter, path))
    {
      gtk_tree_path_free (path);
      return;
    }

  gtk_tree_model_foreach_helper (model, &iter, path, func, user_data);
  gtk_tree_path_free (path);
}


/*
 * GtkTreeRowReference
 */

static void gtk_tree_row_reference_unref_path (GtkTreePath  *path,
                                               GtkTreeModel *model,
                                               int           depth);


G_DEFINE_BOXED_TYPE (GtkTreeRowReference, gtk_tree_row_reference,
                     gtk_tree_row_reference_copy,
                     gtk_tree_row_reference_free)

struct _GtkTreeRowReference
{
  GObject *proxy;
  GtkTreeModel *model;
  GtkTreePath *path;
};


static void
release_row_references (gpointer data)
{
  RowRefList *refs = data;
  GSList *tmp_list = NULL;

  tmp_list = refs->list;
  while (tmp_list != NULL)
    {
      GtkTreeRowReference *reference = tmp_list->data;

      if (reference->proxy == (GObject *)reference->model)
        reference->model = NULL;
      reference->proxy = NULL;

      /* we don't free the reference, users are responsible for that. */

      tmp_list = tmp_list->next;
    }

  g_slist_free (refs->list);
  g_free (refs);
}

static void
gtk_tree_row_ref_inserted (RowRefList  *refs,
                           GtkTreePath *path,
                           GtkTreeIter *iter)
{
  GSList *tmp_list;

  if (refs == NULL)
    return;

  /* This function corrects the path stored in the reference to
   * account for an insertion. Note that it's called _after_ the
   * insertion with the path to the newly-inserted row. Which means
   * that the inserted path is in a different "coordinate system" than
   * the old path (e.g. if the inserted path was just before the old
   * path, then inserted path and old path will be the same, and old
   * path must be moved down one).
   */

  tmp_list = refs->list;

  while (tmp_list != NULL)
    {
      GtkTreeRowReference *reference = tmp_list->data;

      if (reference->path == NULL)
        goto done;

      if (reference->path->depth >= path->depth)
        {
          int i;
          gboolean ancestor = TRUE;

          for (i = 0; i < path->depth - 1; i ++)
            {
              if (path->indices[i] != reference->path->indices[i])
                {
                  ancestor = FALSE;
                  break;
                }
            }
          if (ancestor == FALSE)
            goto done;

          if (path->indices[path->depth-1] <= reference->path->indices[path->depth-1])
            reference->path->indices[path->depth-1] += 1;
        }
    done:
      tmp_list = tmp_list->next;
    }
}

static void
gtk_tree_row_ref_deleted (RowRefList  *refs,
                          GtkTreePath *path)
{
  GSList *tmp_list;

  if (refs == NULL)
    return;

  /* This function corrects the path stored in the reference to
   * account for a deletion. Note that it's called _after_ the
   * deletion with the old path of the just-deleted row. Which means
   * that the deleted path is the same now-defunct "coordinate system"
   * as the path saved in the reference, which is what we want to fix.
   */

  tmp_list = refs->list;

  while (tmp_list != NULL)
    {
      GtkTreeRowReference *reference = tmp_list->data;

      if (reference->path)
        {
          int i;

          if (path->depth > reference->path->depth)
            goto next;
          for (i = 0; i < path->depth - 1; i++)
            {
              if (path->indices[i] != reference->path->indices[i])
                goto next;
            }

          /* We know it affects us. */
          if (path->indices[i] == reference->path->indices[i])
            {
              if (reference->path->depth > path->depth)
                /* some parent was deleted, trying to unref any node
                 * between the deleted parent and the node the reference
                 * is pointing to is bad, as those nodes are already gone.
                 */
                gtk_tree_row_reference_unref_path (reference->path, reference->model, path->depth - 1);
              else
                gtk_tree_row_reference_unref_path (reference->path, reference->model, reference->path->depth - 1);
              gtk_tree_path_free (reference->path);
              reference->path = NULL;
            }
          else if (path->indices[i] < reference->path->indices[i])
            {
              reference->path->indices[path->depth-1]-=1;
            }
        }

next:
      tmp_list = tmp_list->next;
    }
}

static void
gtk_tree_row_ref_reordered (RowRefList  *refs,
                            GtkTreePath *path,
                            GtkTreeIter *iter,
                            int         *new_order)
{
  GSList *tmp_list;
  int length;

  if (refs == NULL)
    return;

  tmp_list = refs->list;

  while (tmp_list != NULL)
    {
      GtkTreeRowReference *reference = tmp_list->data;

      length = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (reference->model), iter);

      if (length < 2)
        return;

      if ((reference->path) &&
          (gtk_tree_path_is_ancestor (path, reference->path)))
        {
          int ref_depth = gtk_tree_path_get_depth (reference->path);
          int depth = gtk_tree_path_get_depth (path);

          if (ref_depth > depth)
            {
              int i;
              int *indices = gtk_tree_path_get_indices (reference->path);

              for (i = 0; i < length; i++)
                {
                  if (new_order[i] == indices[depth])
                    {
                      indices[depth] = i;
                      break;
                    }
                }
            }
        }

      tmp_list = tmp_list->next;
    }
}

/* We do this recursively so that we can unref children nodes
 * before their parent
 */
static void
gtk_tree_row_reference_unref_path_helper (GtkTreePath  *path,
                                          GtkTreeModel *model,
                                          GtkTreeIter  *parent_iter,
                                          int           depth,
                                          int           current_depth)
{
  GtkTreeIter iter;

  if (depth == current_depth)
    return;

  gtk_tree_model_iter_nth_child (model, &iter, parent_iter, path->indices[current_depth]);
  gtk_tree_row_reference_unref_path_helper (path, model, &iter, depth, current_depth + 1);
  gtk_tree_model_unref_node (model, &iter);
}

static void
gtk_tree_row_reference_unref_path (GtkTreePath  *path,
                                   GtkTreeModel *model,
                                   int           depth)
{
  GtkTreeIter iter;

  if (depth <= 0)
    return;

  gtk_tree_model_iter_nth_child (model, &iter, NULL, path->indices[0]);
  gtk_tree_row_reference_unref_path_helper (path, model, &iter, depth, 1);
  gtk_tree_model_unref_node (model, &iter);
}

/**
 * gtk_tree_row_reference_new:
 * @model: a `GtkTreeModel`
 * @path: a valid `GtkTreePath` to monitor
 *
 * Creates a row reference based on @path.
 *
 * This reference will keep pointing to the node pointed to
 * by @path, so long as it exists. Any changes that occur on @model are
 * propagated, and the path is updated appropriately. If
 * @path isn’t a valid path in @model, then %NULL is returned.
 *
 * Returns: (nullable): a newly allocated `GtkTreeRowReference`
 *
 * Deprecated: 4.10
 */
GtkTreeRowReference *
gtk_tree_row_reference_new (GtkTreeModel *model,
                            GtkTreePath  *path)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL (model), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  /* We use the model itself as the proxy object; and call
   * gtk_tree_row_reference_inserted(), etc, in the
   * class closure (default handler) marshalers for the signal.
   */
  return gtk_tree_row_reference_new_proxy (G_OBJECT (model), model, path);
}

/**
 * gtk_tree_row_reference_new_proxy:
 * @proxy: a proxy `GObject`
 * @model: a `GtkTreeModel`
 * @path: a valid `GtkTreePath` to monitor
 *
 * You do not need to use this function.
 *
 * Creates a row reference based on @path.
 *
 * This reference will keep pointing to the node pointed to
 * by @path, so long as it exists. If @path isn’t a valid
 * path in @model, then %NULL is returned. However, unlike
 * references created with gtk_tree_row_reference_new(), it
 * does not listen to the model for changes. The creator of
 * the row reference must do this explicitly using
 * gtk_tree_row_reference_inserted(), gtk_tree_row_reference_deleted(),
 * gtk_tree_row_reference_reordered().
 *
 * These functions must be called exactly once per proxy when the
 * corresponding signal on the model is emitted. This single call
 * updates all row references for that proxy. Since built-in GTK
 * objects like `GtkTreeView` already use this mechanism internally,
 * using them as the proxy object will produce unpredictable results.
 * Further more, passing the same object as @model and @proxy
 * doesn’t work for reasons of internal implementation.
 *
 * This type of row reference is primarily meant by structures that
 * need to carefully monitor exactly when a row reference updates
 * itself, and is not generally needed by most applications.
 *
 * Returns: (nullable): a newly allocated `GtkTreeRowReference`
 *
 * Deprecated: 4.10
 */
GtkTreeRowReference *
gtk_tree_row_reference_new_proxy (GObject      *proxy,
                                  GtkTreeModel *model,
                                  GtkTreePath  *path)
{
  GtkTreeRowReference *reference;
  RowRefList *refs;
  GtkTreeIter parent_iter;
  int i;

  g_return_val_if_fail (G_IS_OBJECT (proxy), NULL);
  g_return_val_if_fail (GTK_IS_TREE_MODEL (model), NULL);
  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (path->depth > 0, NULL);

  /* check that the path is valid */
  if (gtk_tree_model_get_iter (model, &parent_iter, path) == FALSE)
    return NULL;

  /* Now we want to ref every node */
  gtk_tree_model_iter_nth_child (model, &parent_iter, NULL, path->indices[0]);
  gtk_tree_model_ref_node (model, &parent_iter);

  for (i = 1; i < path->depth; i++)
    {
      GtkTreeIter iter;
      gtk_tree_model_iter_nth_child (model, &iter, &parent_iter, path->indices[i]);
      gtk_tree_model_ref_node (model, &iter);
      parent_iter = iter;
    }

  /* Make the row reference */
  reference = g_new (GtkTreeRowReference, 1);

  g_object_ref (proxy);
  g_object_ref (model);
  reference->proxy = proxy;
  reference->model = model;
  reference->path = gtk_tree_path_copy (path);

  refs = g_object_get_data (G_OBJECT (proxy), ROW_REF_DATA_STRING);

  if (refs == NULL)
    {
      refs = g_new (RowRefList, 1);
      refs->list = NULL;

      g_object_set_data_full (G_OBJECT (proxy),
                              I_(ROW_REF_DATA_STRING),
                              refs, release_row_references);
    }

  refs->list = g_slist_prepend (refs->list, reference);

  return reference;
}

/**
 * gtk_tree_row_reference_get_path:
 * @reference: a `GtkTreeRowReference`
 *
 * Returns a path that the row reference currently points to,
 * or %NULL if the path pointed to is no longer valid.
 *
 * Returns: (nullable) (transfer full): a current path
 *
 * Deprecated: 4.10
 */
GtkTreePath *
gtk_tree_row_reference_get_path (GtkTreeRowReference *reference)
{
  g_return_val_if_fail (reference != NULL, NULL);

  if (reference->proxy == NULL)
    return NULL;

  if (reference->path == NULL)
    return NULL;

  return gtk_tree_path_copy (reference->path);
}

/**
 * gtk_tree_row_reference_get_model:
 * @reference: a `GtkTreeRowReference`
 *
 * Returns the model that the row reference is monitoring.
 *
 * Returns: (transfer none): the model
 *
 * Deprecated: 4.10
 */
GtkTreeModel *
gtk_tree_row_reference_get_model (GtkTreeRowReference *reference)
{
  g_return_val_if_fail (reference != NULL, NULL);

  return reference->model;
}

/**
 * gtk_tree_row_reference_valid:
 * @reference: (nullable): a `GtkTreeRowReference`
 *
 * Returns %TRUE if the @reference is non-%NULL and refers to
 * a current valid path.
 *
 * Returns: %TRUE if @reference points to a valid path
 *
 * Deprecated: 4.10
 */
gboolean
gtk_tree_row_reference_valid (GtkTreeRowReference *reference)
{
  if (reference == NULL || reference->path == NULL)
    return FALSE;

  return TRUE;
}


/**
 * gtk_tree_row_reference_copy:
 * @reference: a `GtkTreeRowReference`
 *
 * Copies a `GtkTreeRowReference`.
 *
 * Returns: a copy of @reference
 *
 * Deprecated: 4.10
 */
GtkTreeRowReference *
gtk_tree_row_reference_copy (GtkTreeRowReference *reference)
{
  return gtk_tree_row_reference_new_proxy (reference->proxy,
                                           reference->model,
                                           reference->path);
}

/**
 * gtk_tree_row_reference_free:
 * @reference: (nullable): a `GtkTreeRowReference`
 *
 * Free’s @reference. @reference may be %NULL
 *
 * Deprecated: 4.10
 */
void
gtk_tree_row_reference_free (GtkTreeRowReference *reference)
{
  RowRefList *refs;

  if (reference == NULL)
    return;

  refs = g_object_get_data (G_OBJECT (reference->proxy), ROW_REF_DATA_STRING);

  if (refs == NULL)
    {
      g_warning (G_STRLOC": bad row reference, proxy has no outstanding row references");
      return;
    }

  refs->list = g_slist_remove (refs->list, reference);

  if (refs->list == NULL)
    {
      g_object_set_data (G_OBJECT (reference->proxy),
                         I_(ROW_REF_DATA_STRING),
                         NULL);
    }

  if (reference->path)
    {
      gtk_tree_row_reference_unref_path (reference->path, reference->model, reference->path->depth);
      gtk_tree_path_free (reference->path);
    }

  g_object_unref (reference->proxy);
  g_object_unref (reference->model);
  g_free (reference);
}

/**
 * gtk_tree_row_reference_inserted:
 * @proxy: a `GObject`
 * @path: the row position that was inserted
 *
 * Lets a set of row reference created by
 * gtk_tree_row_reference_new_proxy() know that the
 * model emitted the ::row-inserted signal.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_row_reference_inserted (GObject     *proxy,
                                 GtkTreePath *path)
{
  g_return_if_fail (G_IS_OBJECT (proxy));

  gtk_tree_row_ref_inserted ((RowRefList *)g_object_get_data (proxy, ROW_REF_DATA_STRING), path, NULL);
}

/**
 * gtk_tree_row_reference_deleted:
 * @proxy: a `GObject`
 * @path: the path position that was deleted
 *
 * Lets a set of row reference created by
 * gtk_tree_row_reference_new_proxy() know that the
 * model emitted the ::row-deleted signal.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_row_reference_deleted (GObject     *proxy,
                                GtkTreePath *path)
{
  g_return_if_fail (G_IS_OBJECT (proxy));

  gtk_tree_row_ref_deleted ((RowRefList *)g_object_get_data (proxy, ROW_REF_DATA_STRING), path);
}

/**
 * gtk_tree_row_reference_reordered: (skip)
 * @proxy: a `GObject`
 * @path: the parent path of the reordered signal
 * @iter: the iter pointing to the parent of the reordered
 * @new_order: (array): the new order of rows
 *
 * Lets a set of row reference created by
 * gtk_tree_row_reference_new_proxy() know that the
 * model emitted the ::rows-reordered signal.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_row_reference_reordered (GObject     *proxy,
                                  GtkTreePath *path,
                                  GtkTreeIter *iter,
                                  int         *new_order)
{
  g_return_if_fail (G_IS_OBJECT (proxy));

  gtk_tree_row_ref_reordered ((RowRefList *)g_object_get_data (proxy, ROW_REF_DATA_STRING), path, iter, new_order);
}
