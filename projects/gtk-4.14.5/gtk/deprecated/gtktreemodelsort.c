/* gtktreemodelsort.c
 * Copyright (C) 2000,2001  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
 * Copyright (C) 2001,2002  Kristian Rietveld <kris@gtk.org>
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

#include "gtktreemodelsort.h"
#include "gtktreesortable.h"
#include "gtktreestore.h"
#include "gtktreedatalistprivate.h"
#include "gtkprivate.h"
#include "gtktreednd.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * GtkTreeModelSort:
 *
 * A GtkTreeModel which makes an underlying tree model sortable
 *
 * The `GtkTreeModelSort` is a model which implements the `GtkTreeSortable`
 * interface.  It does not hold any data itself, but rather is created with
 * a child model and proxies its data.  It has identical column types to
 * this child model, and the changes in the child are propagated.  The
 * primary purpose of this model is to provide a way to sort a different
 * model without modifying it. Note that the sort function used by
 * `GtkTreeModelSort` is not guaranteed to be stable.
 *
 * The use of this is best demonstrated through an example.  In the
 * following sample code we create two `GtkTreeView` widgets each with a
 * view of the same data.  As the model is wrapped here by a
 * `GtkTreeModelSort`, the two `GtkTreeView`s can each sort their
 * view of the data without affecting the other.  By contrast, if we
 * simply put the same model in each widget, then sorting the first would
 * sort the second.
 *
 * ## Using a `GtkTreeModelSort`
 *
 * |[<!-- language="C" -->
 * {
 *   GtkTreeView *tree_view1;
 *   GtkTreeView *tree_view2;
 *   GtkTreeModel *sort_model1;
 *   GtkTreeModel *sort_model2;
 *   GtkTreeModel *child_model;
 *
 *   // get the child model
 *   child_model = get_my_model ();
 *
 *   // Create the first tree
 *   sort_model1 = gtk_tree_model_sort_new_with_model (child_model);
 *   tree_view1 = gtk_tree_view_new_with_model (sort_model1);
 *
 *   // Create the second tree
 *   sort_model2 = gtk_tree_model_sort_new_with_model (child_model);
 *   tree_view2 = gtk_tree_view_new_with_model (sort_model2);
 *
 *   // Now we can sort the two models independently
 *   gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model1),
 *                                         COLUMN_1, GTK_SORT_ASCENDING);
 *   gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model2),
 *                                         COLUMN_1, GTK_SORT_DESCENDING);
 * }
 * ]|
 *
 * To demonstrate how to access the underlying child model from the sort
 * model, the next example will be a callback for the `GtkTreeSelection`
 * `GtkTreeSelection::changed` signal.  In this callback, we get a string
 * from COLUMN_1 of the model.  We then modify the string, find the same
 * selected row on the child model, and change the row there.
 *
 * ## Accessing the child model of in a selection changed callback
 *
 * |[<!-- language="C" -->
 * void
 * selection_changed (GtkTreeSelection *selection, gpointer data)
 * {
 *   GtkTreeModel *sort_model = NULL;
 *   GtkTreeModel *child_model;
 *   GtkTreeIter sort_iter;
 *   GtkTreeIter child_iter;
 *   char *some_data = NULL;
 *   char *modified_data;
 *
 *   // Get the current selected row and the model.
 *   if (! gtk_tree_selection_get_selected (selection,
 *                                          &sort_model,
 *                                          &sort_iter))
 *     return;
 *
 *   // Look up the current value on the selected row and get
 *   // a new value to change it to.
 *   gtk_tree_model_get (GTK_TREE_MODEL (sort_model), &sort_iter,
 *                       COLUMN_1, &some_data,
 *                       -1);
 *
 *   modified_data = change_the_data (some_data);
 *   g_free (some_data);
 *
 *   // Get an iterator on the child model, instead of the sort model.
 *   gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (sort_model),
 *                                                   &child_iter,
 *                                                   &sort_iter);
 *
 *   // Get the child model and change the value of the row. In this
 *   // example, the child model is a GtkListStore. It could be any other
 *   // type of model, though.
 *   child_model = gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (sort_model));
 *   gtk_list_store_set (GTK_LIST_STORE (child_model), &child_iter,
 *                       COLUMN_1, &modified_data,
 *                       -1);
 *   g_free (modified_data);
 * }
 * ]|
 *
 * Deprecated: 4.10: Use [class@Gtk.SortListModel] instead
 */


/* Notes on this implementation of GtkTreeModelSort
 * ================================================
 *
 * Warnings
 * --------
 *
 * In this code there is a potential for confusion as to whether an iter,
 * path or value refers to the GtkTreeModelSort model, or to the child model
 * that has been set. As a convention, variables referencing the child model
 * will have an s_ prefix before them (ie. s_iter, s_value, s_path);
 * Conversion of iterators and paths between GtkTreeModelSort and the child
 * model is done through the various gtk_tree_model_sort_convert_* functions.
 *
 * Iterator format
 * ---------------
 *
 * The iterator format of iterators handed out by GtkTreeModelSort is as
 * follows:
 *
 *    iter->stamp = tree_model_sort->stamp
 *    iter->user_data = SortLevel
 *    iter->user_data2 = SortElt
 *
 * Internal data structure
 * -----------------------
 *
 * Using SortLevel and SortElt, GtkTreeModelSort maintains a “cache” of
 * the mapping from GtkTreeModelSort nodes to nodes in the child model.
 * This is to avoid sorting a level each time an operation is requested
 * on GtkTreeModelSort, such as get iter, get path, get value.
 *
 * A SortElt corresponds to a single node. A node and its siblings are
 * stored in a SortLevel. The SortLevel keeps a reference to the parent
 * SortElt and its SortLevel (if any). The SortElt can have a "children"
 * pointer set, which points at a child level (a sub level).
 *
 * In a SortLevel, nodes are stored in a GSequence. The GSequence
 * allows for fast additions and removals, and supports sorting
 * the level of SortElt nodes.
 *
 * It is important to recognize the two different mappings that play
 * a part in this code:
 *   I.  The mapping from the client to this model. The order in which
 *       nodes are stored in the GSequence is the order in which the
 *       nodes are exposed to clients of the GtkTreeModelSort.
 *   II. The mapping from this model to its child model. Each SortElt
 *       contains an “offset” field which is the offset of the
 *       corresponding node in the child model.
 *
 * Reference counting
 * ------------------
 *
 * GtkTreeModelSort forwards all reference and unreference operations
 * to the corresponding node in the child model. The reference count
 * of each node is also maintained internally, in the “ref_count”
 * fields in SortElt and SortLevel. For each ref and unref operation on
 * a SortElt, the “ref_count” of the SortLevel is updated accordingly.
 * In addition, if a SortLevel has a parent, a reference is taken on
 * this parent. This happens in gtk_tree_model_sort_build_level() and
 * the reference is released again in gtk_tree_model_sort_free_level().
 * This ensures that when GtkTreeModelSort takes a reference on a node
 * (for example during sorting), all parent nodes are referenced
 * according to reference counting rule 1, see the GtkTreeModel
 * documentation.
 *
 * When a level has a reference count of zero, which means that
 * none of the nodes in the level is referenced, the level has
 * a “zero ref count” on all its parents. As soon as the level
 * reaches a reference count of zero, the zero ref count value is
 * incremented by one on all parents of this level. Similarly, as
 * soon as the reference count of a level changes from zero, the
 * zero ref count value is decremented by one on all parents.
 *
 * The zero ref count value is used to clear unused portions of
 * the cache. If a SortElt has a zero ref count of one, then
 * its child level is unused and can be removed from the cache.
 * If the zero ref count value is higher than one, then the
 * child level contains sublevels which are unused as well.
 * gtk_tree_model_sort_clear_cache() uses this to not recurse
 * into levels which have a zero ref count of zero.
 */

typedef struct _SortElt SortElt;
typedef struct _SortLevel SortLevel;
typedef struct _SortData SortData;

struct _SortElt
{
  GtkTreeIter    iter;
  SortLevel     *children;
  int            offset;
  int            ref_count;
  int            zero_ref_count;
  int            old_index; /* used while sorting */
  GSequenceIter *siter; /* iter into seq */
};

struct _SortLevel
{
  GSequence *seq;
  int        ref_count;
  SortElt   *parent_elt;
  SortLevel *parent_level;
};

struct _SortData
{
  GtkTreeModelSort *tree_model_sort;
  GtkTreeIterCompareFunc sort_func;
  gpointer sort_data;

  GtkTreePath *parent_path;
  int *parent_path_indices;
  int parent_path_depth;
};

/* Properties */
enum {
  PROP_0,
  /* Construct args */
  PROP_MODEL
};


struct _GtkTreeModelSortPrivate
{
  gpointer root;
  int stamp;
  guint child_flags;
  GtkTreeModel *child_model;
  int zero_ref_count;

  /* sort information */
  GList *sort_list;
  int sort_column_id;
  GtkSortType order;

  /* default sort */
  GtkTreeIterCompareFunc default_sort_func;
  gpointer default_sort_data;
  GDestroyNotify default_sort_destroy;

  /* signal ids */
  gulong changed_id;
  gulong inserted_id;
  gulong has_child_toggled_id;
  gulong deleted_id;
  gulong reordered_id;
};

/* Set this to 0 to disable caching of child iterators.  This
 * allows for more stringent testing.  It is recommended to set this
 * to one when refactoring this code and running the unit tests to
 * catch more errors.
 */
#if 1
#  define GTK_TREE_MODEL_SORT_CACHE_CHILD_ITERS(tree_model_sort) \
	(((GtkTreeModelSort *)tree_model_sort)->priv->child_flags&GTK_TREE_MODEL_ITERS_PERSIST)
#else
#  define GTK_TREE_MODEL_SORT_CACHE_CHILD_ITERS(tree_model_sort) (FALSE)
#endif

#define SORT_ELT(sort_elt) ((SortElt *)sort_elt)
#define SORT_LEVEL(sort_level) ((SortLevel *)sort_level)
#define GET_ELT(siter) ((SortElt *) (siter ? g_sequence_get (siter) : NULL))


#define GET_CHILD_ITER(tree_model_sort,ch_iter,so_iter) gtk_tree_model_sort_convert_iter_to_child_iter((GtkTreeModelSort*)(tree_model_sort), (ch_iter), (so_iter));

#define NO_SORT_FUNC ((GtkTreeIterCompareFunc) 0x1)

#define VALID_ITER(iter, tree_model_sort) ((iter) != NULL && (iter)->user_data != NULL && (iter)->user_data2 != NULL && (tree_model_sort)->priv->stamp == (iter)->stamp)

/* general (object/interface init, etc) */
static void gtk_tree_model_sort_tree_model_init       (GtkTreeModelIface     *iface);
static void gtk_tree_model_sort_tree_sortable_init    (GtkTreeSortableIface  *iface);
static void gtk_tree_model_sort_drag_source_init      (GtkTreeDragSourceIface*iface);
static void gtk_tree_model_sort_finalize              (GObject               *object);
static void gtk_tree_model_sort_set_property          (GObject               *object,
						       guint                  prop_id,
						       const GValue          *value,
						       GParamSpec            *pspec);
static void gtk_tree_model_sort_get_property          (GObject               *object,
						       guint                  prop_id,
						       GValue                *value,
						       GParamSpec            *pspec);

/* our signal handlers */
static void gtk_tree_model_sort_row_changed           (GtkTreeModel          *model,
						       GtkTreePath           *start_path,
						       GtkTreeIter           *start_iter,
						       gpointer               data);
static void gtk_tree_model_sort_row_inserted          (GtkTreeModel          *model,
						       GtkTreePath           *path,
						       GtkTreeIter           *iter,
						       gpointer               data);
static void gtk_tree_model_sort_row_has_child_toggled (GtkTreeModel          *model,
						       GtkTreePath           *path,
						       GtkTreeIter           *iter,
						       gpointer               data);
static void gtk_tree_model_sort_row_deleted           (GtkTreeModel          *model,
						       GtkTreePath           *path,
						       gpointer               data);
static void gtk_tree_model_sort_rows_reordered        (GtkTreeModel          *s_model,
						       GtkTreePath           *s_path,
						       GtkTreeIter           *s_iter,
						       int                   *new_order,
						       gpointer               data);

/* TreeModel interface */
static GtkTreeModelFlags gtk_tree_model_sort_get_flags     (GtkTreeModel          *tree_model);
static int          gtk_tree_model_sort_get_n_columns      (GtkTreeModel          *tree_model);
static GType        gtk_tree_model_sort_get_column_type    (GtkTreeModel          *tree_model,
                                                            int                    index);
static gboolean     gtk_tree_model_sort_get_iter           (GtkTreeModel          *tree_model,
                                                            GtkTreeIter           *iter,
                                                            GtkTreePath           *path);
static GtkTreePath *gtk_tree_model_sort_get_path           (GtkTreeModel          *tree_model,
                                                            GtkTreeIter           *iter);
static void         gtk_tree_model_sort_get_value          (GtkTreeModel          *tree_model,
                                                            GtkTreeIter           *iter,
                                                            int                    column,
                                                            GValue                *value);
static gboolean     gtk_tree_model_sort_iter_next          (GtkTreeModel          *tree_model,
                                                            GtkTreeIter           *iter);
static gboolean     gtk_tree_model_sort_iter_previous      (GtkTreeModel          *tree_model,
                                                            GtkTreeIter           *iter);
static gboolean     gtk_tree_model_sort_iter_children      (GtkTreeModel          *tree_model,
                                                            GtkTreeIter           *iter,
                                                            GtkTreeIter           *parent);
static gboolean     gtk_tree_model_sort_iter_has_child     (GtkTreeModel          *tree_model,
                                                            GtkTreeIter           *iter);
static int          gtk_tree_model_sort_iter_n_children    (GtkTreeModel          *tree_model,
                                                            GtkTreeIter           *iter);
static gboolean     gtk_tree_model_sort_iter_nth_child     (GtkTreeModel          *tree_model,
                                                            GtkTreeIter           *iter,
                                                            GtkTreeIter           *parent,
                                                            int                    n);
static gboolean     gtk_tree_model_sort_iter_parent        (GtkTreeModel          *tree_model,
                                                            GtkTreeIter           *iter,
                                                            GtkTreeIter           *child);
static void         gtk_tree_model_sort_ref_node           (GtkTreeModel          *tree_model,
                                                            GtkTreeIter           *iter);
static void         gtk_tree_model_sort_real_unref_node    (GtkTreeModel          *tree_model,
                                                            GtkTreeIter           *iter,
							    gboolean               propagate_unref);
static void         gtk_tree_model_sort_unref_node         (GtkTreeModel          *tree_model,
                                                            GtkTreeIter           *iter);

/* TreeDragSource interface */
static gboolean     gtk_tree_model_sort_row_draggable         (GtkTreeDragSource      *drag_source,
                                                               GtkTreePath            *path);
static GdkContentProvider *
                    gtk_tree_model_sort_drag_data_get         (GtkTreeDragSource      *drag_source,
                                                               GtkTreePath            *path);
static gboolean     gtk_tree_model_sort_drag_data_delete      (GtkTreeDragSource      *drag_source,
                                                               GtkTreePath            *path);

/* TreeSortable interface */
static gboolean     gtk_tree_model_sort_get_sort_column_id    (GtkTreeSortable        *sortable,
							       int                    *sort_column_id,
							       GtkSortType            *order);
static void         gtk_tree_model_sort_set_sort_column_id    (GtkTreeSortable        *sortable,
							       int                     sort_column_id,
							       GtkSortType        order);
static void         gtk_tree_model_sort_set_sort_func         (GtkTreeSortable        *sortable,
							       int                     sort_column_id,
							       GtkTreeIterCompareFunc  func,
							       gpointer                data,
							       GDestroyNotify          destroy);
static void         gtk_tree_model_sort_set_default_sort_func (GtkTreeSortable        *sortable,
							       GtkTreeIterCompareFunc  func,
							       gpointer                data,
							       GDestroyNotify          destroy);
static gboolean     gtk_tree_model_sort_has_default_sort_func (GtkTreeSortable     *sortable);

/* Private functions (sort funcs, level handling and other utils) */
static void         gtk_tree_model_sort_build_level       (GtkTreeModelSort *tree_model_sort,
							   SortLevel        *parent_level,
                                                           SortElt          *parent_elt);
static void         gtk_tree_model_sort_free_level        (GtkTreeModelSort *tree_model_sort,
							   SortLevel        *sort_level,
                                                           gboolean          unref);
static void         gtk_tree_model_sort_increment_stamp   (GtkTreeModelSort *tree_model_sort);
static void         gtk_tree_model_sort_sort_level        (GtkTreeModelSort *tree_model_sort,
							   SortLevel        *level,
							   gboolean          recurse,
							   gboolean          emit_reordered);
static void         gtk_tree_model_sort_sort              (GtkTreeModelSort *tree_model_sort);
static gboolean     gtk_tree_model_sort_insert_value      (GtkTreeModelSort *tree_model_sort,
							   SortLevel        *level,
							   GtkTreePath      *s_path,
							   GtkTreeIter      *s_iter);
static GtkTreePath *gtk_tree_model_sort_elt_get_path      (SortLevel        *level,
							   SortElt          *elt);
static void         gtk_tree_model_sort_set_model         (GtkTreeModelSort *tree_model_sort,
							   GtkTreeModel     *child_model);
static GtkTreePath *gtk_real_tree_model_sort_convert_child_path_to_path (GtkTreeModelSort *tree_model_sort,
									 GtkTreePath      *child_path,
									 gboolean          build_levels);

static int          gtk_tree_model_sort_compare_func        (gconstpointer     a,
                                                             gconstpointer     b,
                                                             gpointer          user_data);
static int          gtk_tree_model_sort_offset_compare_func (gconstpointer     a,
                                                             gconstpointer     b,
                                                             gpointer          user_data);
static void         gtk_tree_model_sort_clear_cache_helper  (GtkTreeModelSort *tree_model_sort,
                                                             SortLevel        *level);


G_DEFINE_TYPE_WITH_CODE (GtkTreeModelSort, gtk_tree_model_sort, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GtkTreeModelSort)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
						gtk_tree_model_sort_tree_model_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_SORTABLE,
						gtk_tree_model_sort_tree_sortable_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
						gtk_tree_model_sort_drag_source_init))

static void
gtk_tree_model_sort_init (GtkTreeModelSort *tree_model_sort)
{
  GtkTreeModelSortPrivate *priv;

  tree_model_sort->priv = priv =
    gtk_tree_model_sort_get_instance_private (tree_model_sort);
  priv->sort_column_id = GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID;
  priv->stamp = 0;
  priv->zero_ref_count = 0;
  priv->root = NULL;
  priv->sort_list = NULL;
}

static void
gtk_tree_model_sort_class_init (GtkTreeModelSortClass *class)
{
  GObjectClass *object_class;

  object_class = (GObjectClass *) class;

  object_class->set_property = gtk_tree_model_sort_set_property;
  object_class->get_property = gtk_tree_model_sort_get_property;

  object_class->finalize = gtk_tree_model_sort_finalize;

  /* Properties */
  g_object_class_install_property (object_class,
                                   PROP_MODEL,
                                   g_param_spec_object ("model", NULL, NULL,
							GTK_TYPE_TREE_MODEL,
							GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
gtk_tree_model_sort_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = gtk_tree_model_sort_get_flags;
  iface->get_n_columns = gtk_tree_model_sort_get_n_columns;
  iface->get_column_type = gtk_tree_model_sort_get_column_type;
  iface->get_iter = gtk_tree_model_sort_get_iter;
  iface->get_path = gtk_tree_model_sort_get_path;
  iface->get_value = gtk_tree_model_sort_get_value;
  iface->iter_next = gtk_tree_model_sort_iter_next;
  iface->iter_previous = gtk_tree_model_sort_iter_previous;
  iface->iter_children = gtk_tree_model_sort_iter_children;
  iface->iter_has_child = gtk_tree_model_sort_iter_has_child;
  iface->iter_n_children = gtk_tree_model_sort_iter_n_children;
  iface->iter_nth_child = gtk_tree_model_sort_iter_nth_child;
  iface->iter_parent = gtk_tree_model_sort_iter_parent;
  iface->ref_node = gtk_tree_model_sort_ref_node;
  iface->unref_node = gtk_tree_model_sort_unref_node;
}

static void
gtk_tree_model_sort_tree_sortable_init (GtkTreeSortableIface *iface)
{
  iface->get_sort_column_id = gtk_tree_model_sort_get_sort_column_id;
  iface->set_sort_column_id = gtk_tree_model_sort_set_sort_column_id;
  iface->set_sort_func = gtk_tree_model_sort_set_sort_func;
  iface->set_default_sort_func = gtk_tree_model_sort_set_default_sort_func;
  iface->has_default_sort_func = gtk_tree_model_sort_has_default_sort_func;
}

static void
gtk_tree_model_sort_drag_source_init (GtkTreeDragSourceIface *iface)
{
  iface->row_draggable = gtk_tree_model_sort_row_draggable;
  iface->drag_data_delete = gtk_tree_model_sort_drag_data_delete;
  iface->drag_data_get = gtk_tree_model_sort_drag_data_get;
}

/**
 * gtk_tree_model_sort_new_with_model: (constructor)
 * @child_model: A `GtkTreeModel`
 *
 * Creates a new `GtkTreeModelSort`, with @child_model as the child model.
 *
 * Returns: (transfer full) (type Gtk.TreeModelSort): A new `GtkTreeModelSort`.
 */
GtkTreeModel *
gtk_tree_model_sort_new_with_model (GtkTreeModel *child_model)
{
  GtkTreeModel *retval;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (child_model), NULL);

  retval = g_object_new (gtk_tree_model_sort_get_type (), NULL);

  gtk_tree_model_sort_set_model (GTK_TREE_MODEL_SORT (retval), child_model);

  return retval;
}

/* GObject callbacks */
static void
gtk_tree_model_sort_finalize (GObject *object)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) object;
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;

  gtk_tree_model_sort_set_model (tree_model_sort, NULL);

  if (priv->root)
    gtk_tree_model_sort_free_level (tree_model_sort, priv->root, TRUE);

  if (priv->sort_list)
    {
      _gtk_tree_data_list_header_free (priv->sort_list);
      priv->sort_list = NULL;
    }

  if (priv->default_sort_destroy)
    {
      priv->default_sort_destroy (priv->default_sort_data);
      priv->default_sort_destroy = NULL;
      priv->default_sort_data = NULL;
    }


  /* must chain up */
  G_OBJECT_CLASS (gtk_tree_model_sort_parent_class)->finalize (object);
}

static void
gtk_tree_model_sort_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
  GtkTreeModelSort *tree_model_sort = GTK_TREE_MODEL_SORT (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      gtk_tree_model_sort_set_model (tree_model_sort, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tree_model_sort_get_property (GObject    *object,
				  guint       prop_id,
				  GValue     *value,
				  GParamSpec *pspec)
{
  GtkTreeModelSort *tree_model_sort = GTK_TREE_MODEL_SORT (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, gtk_tree_model_sort_get_model (tree_model_sort));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* helpers */
static SortElt *
sort_elt_new (void)
{
  return g_slice_new (SortElt);
}

static void
sort_elt_free (gpointer elt)
{
  g_slice_free (SortElt, elt);
}

static void
increase_offset_iter (gpointer data,
                      gpointer user_data)
{
  SortElt *elt = data;
  int offset = GPOINTER_TO_INT (user_data);

  if (elt->offset >= offset)
    elt->offset++;
}

static void
decrease_offset_iter (gpointer data,
                      gpointer user_data)
{
  SortElt *elt = data;
  int offset = GPOINTER_TO_INT (user_data);

  if (elt->offset > offset)
    elt->offset--;
}

static void
fill_sort_data (SortData         *data,
                GtkTreeModelSort *tree_model_sort,
                SortLevel        *level)
{
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;

  data->tree_model_sort = tree_model_sort;

  if (priv->sort_column_id != GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID)
    {
      GtkTreeDataSortHeader *header;

      header = _gtk_tree_data_list_get_header (priv->sort_list,
					       priv->sort_column_id);

      g_return_if_fail (header != NULL);
      g_return_if_fail (header->func != NULL);

      data->sort_func = header->func;
      data->sort_data = header->data;
    }
  else
    {
      /* absolutely SHOULD NOT happen: */
      g_return_if_fail (priv->default_sort_func != NULL);

      data->sort_func = priv->default_sort_func;
      data->sort_data = priv->default_sort_data;
    }

  if (level->parent_elt)
    {
      data->parent_path = gtk_tree_model_sort_elt_get_path (level->parent_level,
                                                            level->parent_elt);
      gtk_tree_path_append_index (data->parent_path, 0);
    }
  else
    {
      data->parent_path = gtk_tree_path_new_first ();
    }
  data->parent_path_depth = gtk_tree_path_get_depth (data->parent_path);
  data->parent_path_indices = gtk_tree_path_get_indices (data->parent_path);
}

static void
free_sort_data (SortData *data)
{
  gtk_tree_path_free (data->parent_path);
}

static SortElt *
lookup_elt_with_offset (GtkTreeModelSort *tree_model_sort,
                        SortLevel        *level,
                        int               offset,
                        GSequenceIter   **ret_siter)
{
  GSequenceIter *siter, *end_siter;

  /* FIXME: We have to do a search like this, because the sequence is not
   * (always) sorted on offset order.  Perhaps we should introduce a
   * second sequence which is sorted on offset order.
   */
  end_siter = g_sequence_get_end_iter (level->seq);
  for (siter = g_sequence_get_begin_iter (level->seq);
       siter != end_siter;
       siter = g_sequence_iter_next (siter))
    {
      SortElt *elt = g_sequence_get (siter);

      if (elt->offset == offset)
        break;
    }

  if (ret_siter)
    *ret_siter = siter;

  return GET_ELT (siter);
}


static void
gtk_tree_model_sort_row_changed (GtkTreeModel *s_model,
				 GtkTreePath  *start_s_path,
				 GtkTreeIter  *start_s_iter,
				 gpointer      data)
{
  GtkTreeModelSort *tree_model_sort = GTK_TREE_MODEL_SORT (data);
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;
  GtkTreePath *path = NULL;
  GtkTreeIter iter;
  GtkTreeIter tmpiter;

  SortElt *elt;
  SortLevel *level;
  SortData sort_data;

  gboolean free_s_path = FALSE;

  int index = 0, old_index;

  g_return_if_fail (start_s_path != NULL || start_s_iter != NULL);

  if (!start_s_path)
    {
      free_s_path = TRUE;
      start_s_path = gtk_tree_model_get_path (s_model, start_s_iter);
    }

  path = gtk_real_tree_model_sort_convert_child_path_to_path (tree_model_sort,
							      start_s_path,
							      FALSE);
  if (!path)
    {
      if (free_s_path)
	gtk_tree_path_free (start_s_path);
      return;
    }

  gtk_tree_model_get_iter (GTK_TREE_MODEL (data), &iter, path);
  gtk_tree_model_sort_ref_node (GTK_TREE_MODEL (data), &iter);

  level = iter.user_data;
  elt = iter.user_data2;

  if (g_sequence_get_length (level->seq) < 2 ||
      (priv->sort_column_id == GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID &&
       priv->default_sort_func == NO_SORT_FUNC))
    {
      if (free_s_path)
	gtk_tree_path_free (start_s_path);

      gtk_tree_model_row_changed (GTK_TREE_MODEL (data), path, &iter);
      gtk_tree_model_sort_unref_node (GTK_TREE_MODEL (data), &iter);

      gtk_tree_path_free (path);

      return;
    }

  if (GTK_TREE_MODEL_SORT_CACHE_CHILD_ITERS (tree_model_sort))
    tmpiter = elt->iter;
  else
    gtk_tree_model_get_iter (priv->child_model,
                             &tmpiter, start_s_path);

  old_index = g_sequence_iter_get_position (elt->siter);

  fill_sort_data (&sort_data, tree_model_sort, level);
  g_sequence_sort_changed (elt->siter,
                           gtk_tree_model_sort_compare_func,
                           &sort_data);
  free_sort_data (&sort_data);

  index = g_sequence_iter_get_position (elt->siter);

  /* Prepare the path for signal emission */
  gtk_tree_path_up (path);
  gtk_tree_path_append_index (path, index);

  gtk_tree_model_sort_increment_stamp (tree_model_sort);

  /* if the item moved, then emit rows_reordered */
  if (old_index != index)
    {
      int *new_order;
      int j;

      GtkTreePath *tmppath;

      new_order = g_new (int, g_sequence_get_length (level->seq));

      for (j = 0; j < g_sequence_get_length (level->seq); j++)
        {
	  if (index > old_index)
	    {
	      if (j == index)
		new_order[j] = old_index;
	      else if (j >= old_index && j < index)
		new_order[j] = j + 1;
	      else
		new_order[j] = j;
	    }
	  else if (index < old_index)
	    {
	      if (j == index)
		new_order[j] = old_index;
	      else if (j > index && j <= old_index)
		new_order[j] = j - 1;
	      else
		new_order[j] = j;
	    }
	  /* else? shouldn't really happen */
	}

      if (level->parent_elt)
        {
	  iter.stamp = priv->stamp;
	  iter.user_data = level->parent_level;
	  iter.user_data2 = level->parent_elt;

	  tmppath = gtk_tree_model_get_path (GTK_TREE_MODEL (tree_model_sort), &iter);

	  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (tree_model_sort),
	                                 tmppath, &iter, new_order);
	}
      else
        {
	  /* toplevel */
	  tmppath = gtk_tree_path_new ();

          gtk_tree_model_rows_reordered (GTK_TREE_MODEL (tree_model_sort), tmppath,
	                                 NULL, new_order);
	}

      gtk_tree_path_free (tmppath);
      g_free (new_order);
    }

  /* emit row_changed signal (at new location) */
  gtk_tree_model_get_iter (GTK_TREE_MODEL (data), &iter, path);
  gtk_tree_model_row_changed (GTK_TREE_MODEL (data), path, &iter);
  gtk_tree_model_sort_unref_node (GTK_TREE_MODEL (data), &iter);

  gtk_tree_path_free (path);
  if (free_s_path)
    gtk_tree_path_free (start_s_path);
}

static void
gtk_tree_model_sort_row_inserted (GtkTreeModel          *s_model,
				  GtkTreePath           *s_path,
				  GtkTreeIter           *s_iter,
				  gpointer               data)
{
  GtkTreeModelSort *tree_model_sort = GTK_TREE_MODEL_SORT (data);
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;
  GtkTreePath *path;
  GtkTreeIter iter;
  GtkTreeIter real_s_iter;

  int i = 0;

  gboolean free_s_path = FALSE;

  SortElt *elt;
  SortLevel *level;
  SortLevel *parent_level = NULL;

  parent_level = level = SORT_LEVEL (priv->root);

  g_return_if_fail (s_path != NULL || s_iter != NULL);

  if (!s_path)
    {
      s_path = gtk_tree_model_get_path (s_model, s_iter);
      free_s_path = TRUE;
    }

  if (!s_iter)
    gtk_tree_model_get_iter (s_model, &real_s_iter, s_path);
  else
    real_s_iter = *s_iter;

  if (!priv->root)
    {
      gtk_tree_model_sort_build_level (tree_model_sort, NULL, NULL);

      /* the build level already put the inserted iter in the level,
	 so no need to handle this signal anymore */

      goto done_and_submit;
    }

  /* find the parent level */
  while (i < gtk_tree_path_get_depth (s_path) - 1)
    {
      if (!level)
	{
	  /* level not yet build, we won't cover this signal */
	  goto done;
	}

      if (g_sequence_get_length (level->seq) < gtk_tree_path_get_indices (s_path)[i])
	{
	  g_warning ("%s: A node was inserted with a parent that's not in the tree.\n"
		     "This possibly means that a GtkTreeModel inserted a child node\n"
		     "before the parent was inserted.",
		     G_STRLOC);
	  goto done;
	}

      elt = lookup_elt_with_offset (tree_model_sort, level,
                                    gtk_tree_path_get_indices (s_path)[i],
                                    NULL);

      g_return_if_fail (elt != NULL);

      if (!elt->children)
	{
	  /* not covering this signal */
	  goto done;
	}

      level = elt->children;
      parent_level = level;
      i++;
    }

  if (!parent_level)
    goto done;

  if (level->ref_count == 0 && level != priv->root)
    {
      gtk_tree_model_sort_free_level (tree_model_sort, level, TRUE);
      goto done;
    }

  if (!gtk_tree_model_sort_insert_value (tree_model_sort,
					 parent_level,
					 s_path,
					 &real_s_iter))
    goto done;

 done_and_submit:
  path = gtk_real_tree_model_sort_convert_child_path_to_path (tree_model_sort,
							      s_path,
							      FALSE);

  if (!path)
    return;

  gtk_tree_model_sort_increment_stamp (tree_model_sort);

  gtk_tree_model_get_iter (GTK_TREE_MODEL (data), &iter, path);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (data), path, &iter);
  gtk_tree_path_free (path);

 done:
  if (free_s_path)
    gtk_tree_path_free (s_path);

  return;
}

static void
gtk_tree_model_sort_row_has_child_toggled (GtkTreeModel *s_model,
					   GtkTreePath  *s_path,
					   GtkTreeIter  *s_iter,
					   gpointer      data)
{
  GtkTreeModelSort *tree_model_sort = GTK_TREE_MODEL_SORT (data);
  GtkTreePath *path;
  GtkTreeIter iter;

  g_return_if_fail (s_path != NULL && s_iter != NULL);

  path = gtk_real_tree_model_sort_convert_child_path_to_path (tree_model_sort, s_path, FALSE);
  if (path == NULL)
    return;

  gtk_tree_model_get_iter (GTK_TREE_MODEL (data), &iter, path);
  gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (data), path, &iter);

  gtk_tree_path_free (path);
}

static void
gtk_tree_model_sort_row_deleted (GtkTreeModel *s_model,
				 GtkTreePath  *s_path,
				 gpointer      data)
{
  GtkTreeModelSort *tree_model_sort = GTK_TREE_MODEL_SORT (data);
  GtkTreePath *path = NULL;
  SortElt *elt;
  SortLevel *level;
  GtkTreeIter iter;
  int offset;

  g_return_if_fail (s_path != NULL);

  path = gtk_real_tree_model_sort_convert_child_path_to_path (tree_model_sort, s_path, FALSE);
  if (path == NULL)
    return;

  gtk_tree_model_get_iter (GTK_TREE_MODEL (data), &iter, path);

  level = SORT_LEVEL (iter.user_data);
  elt = SORT_ELT (iter.user_data2);
  offset = elt->offset;

  gtk_tree_model_get_iter (GTK_TREE_MODEL (data), &iter, path);

  while (elt->ref_count > 0)
    gtk_tree_model_sort_real_unref_node (GTK_TREE_MODEL (data), &iter, FALSE);

  /* If this node has children, we free the level (recursively) here
   * and specify that unref may not be used, because parent and its
   * children have been removed by now.
   */
  if (elt->children)
    gtk_tree_model_sort_free_level (tree_model_sort,
                                    elt->children, FALSE);

  if (level->ref_count == 0 && g_sequence_get_length (level->seq) == 1)
    {
      gtk_tree_model_sort_increment_stamp (tree_model_sort);
      gtk_tree_model_row_deleted (GTK_TREE_MODEL (data), path);
      gtk_tree_path_free (path);

      if (level == tree_model_sort->priv->root)
	{
	  gtk_tree_model_sort_free_level (tree_model_sort,
					  tree_model_sort->priv->root,
                                          TRUE);
	  tree_model_sort->priv->root = NULL;
	}
      return;
    }

  g_sequence_remove (elt->siter);
  elt = NULL;

  /* The sequence is not ordered on offset, so we traverse the entire
   * sequence.
   */
  g_sequence_foreach (level->seq, decrease_offset_iter,
                      GINT_TO_POINTER (offset));

  gtk_tree_model_sort_increment_stamp (tree_model_sort);
  gtk_tree_model_row_deleted (GTK_TREE_MODEL (data), path);

  gtk_tree_path_free (path);
}

static void
gtk_tree_model_sort_rows_reordered (GtkTreeModel *s_model,
				    GtkTreePath  *s_path,
				    GtkTreeIter  *s_iter,
				    int          *new_order,
				    gpointer      data)
{
  SortLevel *level;
  GtkTreeIter iter;
  GtkTreePath *path;
  int *tmp_array;
  int i, length;
  GSequenceIter *siter, *end_siter;
  GtkTreeModelSort *tree_model_sort = GTK_TREE_MODEL_SORT (data);
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;

  g_return_if_fail (new_order != NULL);

  if (s_path == NULL || gtk_tree_path_get_depth (s_path) == 0)
    {
      if (priv->root == NULL)
	return;
      path = gtk_tree_path_new ();
      level = SORT_LEVEL (priv->root);
    }
  else
    {
      SortElt *elt;

      path = gtk_real_tree_model_sort_convert_child_path_to_path (tree_model_sort, s_path, FALSE);
      if (path == NULL)
	return;
      gtk_tree_model_get_iter (GTK_TREE_MODEL (data), &iter, path);

      elt = SORT_ELT (iter.user_data2);

      if (!elt->children)
	{
	  gtk_tree_path_free (path);
	  return;
	}

      level = elt->children;
    }

  length = g_sequence_get_length (level->seq);
  if (length < 2)
    {
      gtk_tree_path_free (path);
      return;
    }

  tmp_array = g_new (int, length);

  /* FIXME: I need to think about whether this can be done in a more
   * efficient way?
   */
  i = 0;
  end_siter = g_sequence_get_end_iter (level->seq);
  for (siter = g_sequence_get_begin_iter (level->seq);
       siter != end_siter;
       siter = g_sequence_iter_next (siter))
    {
      int j;
      SortElt *elt = g_sequence_get (siter);

      for (j = 0; j < length; j++)
	{
	  if (elt->offset == new_order[j])
	    tmp_array[i] = j;
	}

      i++;
    }

  /* This loop cannot be merged with the above loop nest, because that
   * would introduce duplicate offsets.
   */
  i = 0;
  end_siter = g_sequence_get_end_iter (level->seq);
  for (siter = g_sequence_get_begin_iter (level->seq);
       siter != end_siter;
       siter = g_sequence_iter_next (siter))
    {
      SortElt *elt = g_sequence_get (siter);

      elt->offset = tmp_array[i];
      i++;
    }
  g_free (tmp_array);

  if (priv->sort_column_id == GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID &&
      priv->default_sort_func == NO_SORT_FUNC)
    {
      gtk_tree_model_sort_sort_level (tree_model_sort, level,
				      FALSE, FALSE);
      gtk_tree_model_sort_increment_stamp (tree_model_sort);

      if (gtk_tree_path_get_depth (path))
	{
	  gtk_tree_model_get_iter (GTK_TREE_MODEL (tree_model_sort),
				   &iter,
				   path);
	  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (tree_model_sort),
					 path, &iter, new_order);
	}
      else
	{
	  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (tree_model_sort),
					 path, NULL, new_order);
	}
    }

  gtk_tree_path_free (path);
}

/* Fulfill our model requirements */
static GtkTreeModelFlags
gtk_tree_model_sort_get_flags (GtkTreeModel *tree_model)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) tree_model;
  GtkTreeModelFlags flags;

  g_return_val_if_fail (tree_model_sort->priv->child_model != NULL, 0);

  flags = gtk_tree_model_get_flags (tree_model_sort->priv->child_model);

  if ((flags & GTK_TREE_MODEL_LIST_ONLY) == GTK_TREE_MODEL_LIST_ONLY)
    return GTK_TREE_MODEL_LIST_ONLY;

  return 0;
}

static int
gtk_tree_model_sort_get_n_columns (GtkTreeModel *tree_model)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) tree_model;

  if (tree_model_sort->priv->child_model == 0)
    return 0;

  return gtk_tree_model_get_n_columns (tree_model_sort->priv->child_model);
}

static GType
gtk_tree_model_sort_get_column_type (GtkTreeModel *tree_model,
                                     int           index)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) tree_model;

  g_return_val_if_fail (tree_model_sort->priv->child_model != NULL, G_TYPE_INVALID);

  return gtk_tree_model_get_column_type (tree_model_sort->priv->child_model, index);
}

static gboolean
gtk_tree_model_sort_get_iter (GtkTreeModel *tree_model,
			      GtkTreeIter  *iter,
			      GtkTreePath  *path)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) tree_model;
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;
  int *indices;
  SortElt *elt;
  SortLevel *level;
  int depth, i;
  GSequenceIter *siter;

  g_return_val_if_fail (priv->child_model != NULL, FALSE);

  indices = gtk_tree_path_get_indices (path);

  if (priv->root == NULL)
    gtk_tree_model_sort_build_level (tree_model_sort, NULL, NULL);
  level = SORT_LEVEL (priv->root);

  depth = gtk_tree_path_get_depth (path);
  if (depth == 0)
    {
      iter->stamp = 0;
      return FALSE;
    }

  for (i = 0; i < depth - 1; i++)
    {
      if ((level == NULL) ||
	  (indices[i] >= g_sequence_get_length (level->seq)))
        {
          iter->stamp = 0;
          return FALSE;
        }

      siter = g_sequence_get_iter_at_pos (level->seq, indices[i]);
      if (g_sequence_iter_is_end (siter))
        {
          iter->stamp = 0;
          return FALSE;
        }

      elt = GET_ELT (siter);
      g_assert (elt);
      if (elt->children == NULL)
	gtk_tree_model_sort_build_level (tree_model_sort, level, elt);

      level = elt->children;
    }

  if (!level || indices[i] >= g_sequence_get_length (level->seq))
    {
      iter->stamp = 0;
      return FALSE;
    }

  iter->stamp = priv->stamp;
  iter->user_data = level;

  siter = g_sequence_get_iter_at_pos (level->seq, indices[depth - 1]);
  if (g_sequence_iter_is_end (siter))
    {
      iter->stamp = 0;
      return FALSE;
    }
  iter->user_data2 = GET_ELT (siter);

  return TRUE;
}

static GtkTreePath *
gtk_tree_model_sort_get_path (GtkTreeModel *tree_model,
			      GtkTreeIter  *iter)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) tree_model;
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;
  GtkTreePath *retval;
  SortLevel *level;
  SortElt *elt;

  g_return_val_if_fail (priv->child_model != NULL, NULL);
  g_return_val_if_fail (priv->stamp == iter->stamp, NULL);

  retval = gtk_tree_path_new ();

  level = SORT_LEVEL (iter->user_data);
  elt = SORT_ELT (iter->user_data2);

  while (level)
    {
      int index;

      index = g_sequence_iter_get_position (elt->siter);
      gtk_tree_path_prepend_index (retval, index);

      elt = level->parent_elt;
      level = level->parent_level;
    }

  return retval;
}

static void
gtk_tree_model_sort_get_value (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter,
			       int           column,
			       GValue       *value)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) tree_model;
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;
  GtkTreeIter child_iter;

  g_return_if_fail (priv->child_model != NULL);
  g_return_if_fail (VALID_ITER (iter, tree_model_sort));

  GET_CHILD_ITER (tree_model_sort, &child_iter, iter);
  gtk_tree_model_get_value (priv->child_model,
			    &child_iter, column, value);
}

static gboolean
gtk_tree_model_sort_iter_next (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) tree_model;
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;
  SortElt *elt;
  GSequenceIter *siter;

  g_return_val_if_fail (priv->child_model != NULL, FALSE);
  g_return_val_if_fail (priv->stamp == iter->stamp, FALSE);

  elt = iter->user_data2;

  siter = g_sequence_iter_next (elt->siter);
  if (g_sequence_iter_is_end (siter))
    {
      iter->stamp = 0;
      return FALSE;
    }
  iter->user_data2 = GET_ELT (siter);

  return TRUE;
}

static gboolean
gtk_tree_model_sort_iter_previous (GtkTreeModel *tree_model,
                                   GtkTreeIter  *iter)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) tree_model;
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;
  SortElt *elt;
  GSequenceIter *siter;

  g_return_val_if_fail (priv->child_model != NULL, FALSE);
  g_return_val_if_fail (priv->stamp == iter->stamp, FALSE);

  elt = iter->user_data2;

  if (g_sequence_iter_is_begin (elt->siter))
    {
      iter->stamp = 0;
      return FALSE;
    }

  siter = g_sequence_iter_prev (elt->siter);
  iter->user_data2 = GET_ELT (siter);

  return TRUE;
}

static gboolean
gtk_tree_model_sort_iter_children (GtkTreeModel *tree_model,
				   GtkTreeIter  *iter,
				   GtkTreeIter  *parent)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) tree_model;
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;
  SortLevel *level;

  iter->stamp = 0;
  g_return_val_if_fail (priv->child_model != NULL, FALSE);
  if (parent)
    g_return_val_if_fail (VALID_ITER (parent, tree_model_sort), FALSE);

  if (parent == NULL)
    {
      if (priv->root == NULL)
	gtk_tree_model_sort_build_level (tree_model_sort, NULL, NULL);
      if (priv->root == NULL)
	return FALSE;

      level = priv->root;
      iter->stamp = priv->stamp;
      iter->user_data = level;
      iter->user_data2 = g_sequence_get (g_sequence_get_begin_iter (level->seq));
    }
  else
    {
      SortElt *elt;

      level = SORT_LEVEL (parent->user_data);
      elt = SORT_ELT (parent->user_data2);

      if (elt->children == NULL)
        gtk_tree_model_sort_build_level (tree_model_sort, level, elt);

      if (elt->children == NULL)
	return FALSE;

      iter->stamp = priv->stamp;
      iter->user_data = elt->children;
      iter->user_data2 = g_sequence_get (g_sequence_get_begin_iter (elt->children->seq));
    }

  return TRUE;
}

static gboolean
gtk_tree_model_sort_iter_has_child (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) tree_model;
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;
  GtkTreeIter child_iter;

  g_return_val_if_fail (priv->child_model != NULL, FALSE);
  g_return_val_if_fail (VALID_ITER (iter, tree_model_sort), FALSE);

  GET_CHILD_ITER (tree_model_sort, &child_iter, iter);

  return gtk_tree_model_iter_has_child (priv->child_model, &child_iter);
}

static int
gtk_tree_model_sort_iter_n_children (GtkTreeModel *tree_model,
				     GtkTreeIter  *iter)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) tree_model;
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;
  GtkTreeIter child_iter;

  g_return_val_if_fail (priv->child_model != NULL, 0);
  if (iter)
    g_return_val_if_fail (VALID_ITER (iter, tree_model_sort), 0);

  if (iter == NULL)
    return gtk_tree_model_iter_n_children (priv->child_model, NULL);

  GET_CHILD_ITER (tree_model_sort, &child_iter, iter);

  return gtk_tree_model_iter_n_children (priv->child_model, &child_iter);
}

static gboolean
gtk_tree_model_sort_iter_nth_child (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter,
				    GtkTreeIter  *parent,
				    int           n)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) tree_model;
  SortLevel *level;
  /* We have this for the iter == parent case */
  GtkTreeIter children;

  if (parent)
    g_return_val_if_fail (VALID_ITER (parent, tree_model_sort), FALSE);

  /* Use this instead of has_child to force us to build the level, if needed */
  if (gtk_tree_model_sort_iter_children (tree_model, &children, parent) == FALSE)
    {
      iter->stamp = 0;
      return FALSE;
    }

  level = children.user_data;
  if (n >= g_sequence_get_length (level->seq))
    {
      iter->stamp = 0;
      return FALSE;
    }

  iter->stamp = tree_model_sort->priv->stamp;
  iter->user_data = level;
  iter->user_data2 = g_sequence_get (g_sequence_get_iter_at_pos (level->seq, n));

  return TRUE;
}

static gboolean
gtk_tree_model_sort_iter_parent (GtkTreeModel *tree_model,
				 GtkTreeIter  *iter,
				 GtkTreeIter  *child)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) tree_model;
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;
  SortLevel *level;

  iter->stamp = 0;
  g_return_val_if_fail (priv->child_model != NULL, FALSE);
  g_return_val_if_fail (VALID_ITER (child, tree_model_sort), FALSE);

  level = child->user_data;

  if (level->parent_level)
    {
      iter->stamp = priv->stamp;
      iter->user_data = level->parent_level;
      iter->user_data2 = level->parent_elt;

      return TRUE;
    }
  return FALSE;
}

static void
gtk_tree_model_sort_ref_node (GtkTreeModel *tree_model,
			      GtkTreeIter  *iter)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) tree_model;
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;
  GtkTreeIter child_iter;
  SortLevel *level;
  SortElt *elt;

  g_return_if_fail (priv->child_model != NULL);
  g_return_if_fail (VALID_ITER (iter, tree_model_sort));

  GET_CHILD_ITER (tree_model_sort, &child_iter, iter);

  /* Reference the node in the child model */
  gtk_tree_model_ref_node (priv->child_model, &child_iter);

  /* Increase the reference count of this element and its level */
  level = iter->user_data;
  elt = iter->user_data2;

  elt->ref_count++;
  level->ref_count++;

  if (level->ref_count == 1)
    {
      SortLevel *parent_level = level->parent_level;
      SortElt *parent_elt = level->parent_elt;

      /* We were at zero -- time to decrement the zero_ref_count val */
      while (parent_level)
        {
          parent_elt->zero_ref_count--;

          parent_elt = parent_level->parent_elt;
	  parent_level = parent_level->parent_level;
	}

      if (priv->root != level)
	priv->zero_ref_count--;
    }
}

static void
gtk_tree_model_sort_real_unref_node (GtkTreeModel *tree_model,
				     GtkTreeIter  *iter,
				     gboolean      propagate_unref)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) tree_model;
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;
  SortLevel *level;
  SortElt *elt;

  g_return_if_fail (priv->child_model != NULL);
  g_return_if_fail (VALID_ITER (iter, tree_model_sort));

  if (propagate_unref)
    {
      GtkTreeIter child_iter;

      GET_CHILD_ITER (tree_model_sort, &child_iter, iter);
      gtk_tree_model_unref_node (priv->child_model, &child_iter);
    }

  level = iter->user_data;
  elt = iter->user_data2;

  g_return_if_fail (elt->ref_count > 0);

  elt->ref_count--;
  level->ref_count--;

  if (level->ref_count == 0)
    {
      SortLevel *parent_level = level->parent_level;
      SortElt *parent_elt = level->parent_elt;

      /* We are at zero -- time to increment the zero_ref_count val */
      while (parent_level)
	{
          parent_elt->zero_ref_count++;

          parent_elt = parent_level->parent_elt;
	  parent_level = parent_level->parent_level;
	}

      if (priv->root != level)
	priv->zero_ref_count++;
    }
}

static void
gtk_tree_model_sort_unref_node (GtkTreeModel *tree_model,
				GtkTreeIter  *iter)
{
  gtk_tree_model_sort_real_unref_node (tree_model, iter, TRUE);
}

/* Sortable interface */
static gboolean
gtk_tree_model_sort_get_sort_column_id (GtkTreeSortable *sortable,
					int             *sort_column_id,
					GtkSortType     *order)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *)sortable;
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;

  if (sort_column_id)
    *sort_column_id = priv->sort_column_id;
  if (order)
    *order = priv->order;

  if (priv->sort_column_id == GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID ||
      priv->sort_column_id == GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID)
    return FALSE;

  return TRUE;
}

static void
gtk_tree_model_sort_set_sort_column_id (GtkTreeSortable *sortable,
					int              sort_column_id,
					GtkSortType      order)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *)sortable;
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;

  if (priv->sort_column_id == sort_column_id && priv->order == order)
    return;

  if (sort_column_id != GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID)
    {
      if (sort_column_id != GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID)
        {
          GtkTreeDataSortHeader *header = NULL;

          header = _gtk_tree_data_list_get_header (priv->sort_list,
	  				           sort_column_id);

          /* we want to make sure that we have a function */
          g_return_if_fail (header != NULL);
          g_return_if_fail (header->func != NULL);
        }
      else
        g_return_if_fail (priv->default_sort_func != NULL);
    }

  priv->sort_column_id = sort_column_id;
  priv->order = order;

  gtk_tree_sortable_sort_column_changed (sortable);

  gtk_tree_model_sort_sort (tree_model_sort);
}

static void
gtk_tree_model_sort_set_sort_func (GtkTreeSortable        *sortable,
				   int                     sort_column_id,
				   GtkTreeIterCompareFunc  func,
				   gpointer                data,
				   GDestroyNotify          destroy)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *) sortable;
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;

  priv->sort_list = _gtk_tree_data_list_set_header (priv->sort_list,
						    sort_column_id,
						    func, data, destroy);

  if (priv->sort_column_id == sort_column_id)
    gtk_tree_model_sort_sort (tree_model_sort);
}

static void
gtk_tree_model_sort_set_default_sort_func (GtkTreeSortable        *sortable,
					   GtkTreeIterCompareFunc  func,
					   gpointer                data,
					   GDestroyNotify          destroy)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *)sortable;
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;

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
    gtk_tree_model_sort_sort (tree_model_sort);
}

static gboolean
gtk_tree_model_sort_has_default_sort_func (GtkTreeSortable *sortable)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *)sortable;

  return (tree_model_sort->priv->default_sort_func != NO_SORT_FUNC);
}

/* DragSource interface */
static gboolean
gtk_tree_model_sort_row_draggable (GtkTreeDragSource *drag_source,
                                   GtkTreePath       *path)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *)drag_source;
  GtkTreePath *child_path;
  gboolean draggable;

  child_path = gtk_tree_model_sort_convert_path_to_child_path (tree_model_sort,
                                                               path);
  draggable = gtk_tree_drag_source_row_draggable (GTK_TREE_DRAG_SOURCE (tree_model_sort->priv->child_model), child_path);
  gtk_tree_path_free (child_path);

  return draggable;
}

static GdkContentProvider *
gtk_tree_model_sort_drag_data_get (GtkTreeDragSource *drag_source,
                                   GtkTreePath       *path)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *)drag_source;
  GtkTreePath *child_path;
  GdkContentProvider *gotten;

  child_path = gtk_tree_model_sort_convert_path_to_child_path (tree_model_sort, path);
  gotten = gtk_tree_drag_source_drag_data_get (GTK_TREE_DRAG_SOURCE (tree_model_sort->priv->child_model), child_path);
  gtk_tree_path_free (child_path);

  return gotten;
}

static gboolean
gtk_tree_model_sort_drag_data_delete (GtkTreeDragSource *drag_source,
                                      GtkTreePath       *path)
{
  GtkTreeModelSort *tree_model_sort = (GtkTreeModelSort *)drag_source;
  GtkTreePath *child_path;
  gboolean deleted;

  child_path = gtk_tree_model_sort_convert_path_to_child_path (tree_model_sort, path);
  deleted = gtk_tree_drag_source_drag_data_delete (GTK_TREE_DRAG_SOURCE (tree_model_sort->priv->child_model), child_path);
  gtk_tree_path_free (child_path);

  return deleted;
}

/* sorting code - private */
static int
gtk_tree_model_sort_compare_func (gconstpointer a,
				  gconstpointer b,
				  gpointer      user_data)
{
  SortData *data = (SortData *)user_data;
  GtkTreeModelSort *tree_model_sort = data->tree_model_sort;
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;
  const SortElt *sa = a;
  const SortElt *sb = b;

  GtkTreeIter iter_a, iter_b;
  int retval;

  if (GTK_TREE_MODEL_SORT_CACHE_CHILD_ITERS (tree_model_sort))
    {
      iter_a = sa->iter;
      iter_b = sb->iter;
    }
  else
    {
      data->parent_path_indices [data->parent_path_depth-1] = sa->offset;
      gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->child_model), &iter_a, data->parent_path);
      data->parent_path_indices [data->parent_path_depth-1] = sb->offset;
      gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->child_model), &iter_b, data->parent_path);
    }

  retval = (* data->sort_func) (GTK_TREE_MODEL (priv->child_model),
				&iter_a, &iter_b,
				data->sort_data);

  if (priv->order == GTK_SORT_DESCENDING)
    {
      if (retval > 0)
	retval = -1;
      else if (retval < 0)
	retval = 1;
    }

  return retval;
}

static int
gtk_tree_model_sort_offset_compare_func (gconstpointer a,
					 gconstpointer b,
					 gpointer      user_data)
{
  int retval;

  const SortElt *sa = (SortElt *)a;
  const SortElt *sb = (SortElt *)b;

  SortData *data = (SortData *)user_data;

  if (sa->offset < sb->offset)
    retval = -1;
  else if (sa->offset > sb->offset)
    retval = 1;
  else
    retval = 0;

  if (data->tree_model_sort->priv->order == GTK_SORT_DESCENDING)
    {
      if (retval > 0)
	retval = -1;
      else if (retval < 0)
	retval = 1;
    }

  return retval;
}

static void
gtk_tree_model_sort_sort_level (GtkTreeModelSort *tree_model_sort,
				SortLevel        *level,
				gboolean          recurse,
				gboolean          emit_reordered)
{
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;
  int i;
  GSequenceIter *begin_siter, *end_siter, *siter;
  SortElt *begin_elt;
  int *new_order;

  GtkTreeIter iter;
  GtkTreePath *path;

  SortData data;

  g_return_if_fail (level != NULL);

  begin_siter = g_sequence_get_begin_iter (level->seq);
  begin_elt = g_sequence_get (begin_siter);

  if (g_sequence_get_length (level->seq) < 1 && !begin_elt->children)
    return;

  iter.stamp = priv->stamp;
  iter.user_data = level;
  iter.user_data2 = begin_elt;

  gtk_tree_model_sort_ref_node (GTK_TREE_MODEL (tree_model_sort), &iter);

  i = 0;
  end_siter = g_sequence_get_end_iter (level->seq);
  for (siter = g_sequence_get_begin_iter (level->seq);
       siter != end_siter;
       siter = g_sequence_iter_next (siter))
    {
      SortElt *elt = g_sequence_get (siter);

      elt->old_index = i;
      i++;
    }

  fill_sort_data (&data, tree_model_sort, level);

  if (data.sort_func == NO_SORT_FUNC)
    g_sequence_sort (level->seq, gtk_tree_model_sort_offset_compare_func,
                     &data);
  else
    g_sequence_sort (level->seq, gtk_tree_model_sort_compare_func, &data);

  free_sort_data (&data);

  new_order = g_new (int, g_sequence_get_length (level->seq));

  i = 0;
  end_siter = g_sequence_get_end_iter (level->seq);
  for (siter = g_sequence_get_begin_iter (level->seq);
       siter != end_siter;
       siter = g_sequence_iter_next (siter))
    {
      SortElt *elt = g_sequence_get (siter);

      new_order[i++] = elt->old_index;
    }

  if (emit_reordered)
    {
      gtk_tree_model_sort_increment_stamp (tree_model_sort);
      if (level->parent_elt)
	{
	  iter.stamp = priv->stamp;
	  iter.user_data = level->parent_level;
	  iter.user_data2 = level->parent_elt;

	  path = gtk_tree_model_get_path (GTK_TREE_MODEL (tree_model_sort),
					  &iter);

	  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (tree_model_sort), path,
					 &iter, new_order);
	}
      else
	{
	  /* toplevel list */
	  path = gtk_tree_path_new ();
	  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (tree_model_sort), path,
					 NULL, new_order);
	}

      gtk_tree_path_free (path);
    }

  /* recurse, if possible */
  if (recurse)
    {
      end_siter = g_sequence_get_end_iter (level->seq);
      for (siter = g_sequence_get_begin_iter (level->seq);
           siter != end_siter;
           siter = g_sequence_iter_next (siter))
	{
	  SortElt *elt = g_sequence_get (siter);

	  if (elt->children)
	    gtk_tree_model_sort_sort_level (tree_model_sort,
					    elt->children,
					    TRUE, emit_reordered);
	}
    }

  g_free (new_order);

  /* get the iter we referenced at the beginning of this function and
   * unref it again
   */
  iter.stamp = priv->stamp;
  iter.user_data = level;
  iter.user_data2 = begin_elt;

  gtk_tree_model_sort_unref_node (GTK_TREE_MODEL (tree_model_sort), &iter);
}

static void
gtk_tree_model_sort_sort (GtkTreeModelSort *tree_model_sort)
{
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;

  if (priv->sort_column_id == GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID)
    return;

  if (!priv->root)
    return;

  if (priv->sort_column_id != GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID)
    {
      GtkTreeDataSortHeader *header = NULL;

      header = _gtk_tree_data_list_get_header (priv->sort_list,
					       priv->sort_column_id);

      /* we want to make sure that we have a function */
      g_return_if_fail (header != NULL);
      g_return_if_fail (header->func != NULL);
    }
  else
    g_return_if_fail (priv->default_sort_func != NULL);

  gtk_tree_model_sort_sort_level (tree_model_sort, priv->root,
				  TRUE, TRUE);
}

/* signal helpers */
static gboolean
gtk_tree_model_sort_insert_value (GtkTreeModelSort *tree_model_sort,
				  SortLevel        *level,
				  GtkTreePath      *s_path,
				  GtkTreeIter      *s_iter)
{
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;
  SortElt *elt;
  SortData data;
  int offset;

  elt = sort_elt_new ();

  offset = gtk_tree_path_get_indices (s_path)[gtk_tree_path_get_depth (s_path) - 1];

  if (GTK_TREE_MODEL_SORT_CACHE_CHILD_ITERS (tree_model_sort))
    elt->iter = *s_iter;
  elt->offset = offset;
  elt->zero_ref_count = 0;
  elt->ref_count = 0;
  elt->children = NULL;

  /* update all larger offsets */
  g_sequence_foreach (level->seq, increase_offset_iter, GINT_TO_POINTER (offset));

  fill_sort_data (&data, tree_model_sort, level);

  if (priv->sort_column_id == GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID &&
      priv->default_sort_func == NO_SORT_FUNC)
    {
      elt->siter = g_sequence_insert_sorted (level->seq, elt,
                                             gtk_tree_model_sort_offset_compare_func,
                                             &data);
    }
  else
    {
      elt->siter = g_sequence_insert_sorted (level->seq, elt,
                                             gtk_tree_model_sort_compare_func,
                                             &data);
    }

  free_sort_data (&data);

  return TRUE;
}

/* sort elt stuff */
static GtkTreePath *
gtk_tree_model_sort_elt_get_path (SortLevel *level,
				  SortElt *elt)
{
  SortLevel *walker = level;
  SortElt *walker2 = elt;
  GtkTreePath *path;

  g_return_val_if_fail (level != NULL, NULL);
  g_return_val_if_fail (elt != NULL, NULL);

  path = gtk_tree_path_new ();

  while (walker)
    {
      gtk_tree_path_prepend_index (path, walker2->offset);

      if (!walker->parent_level)
	break;

      walker2 = walker->parent_elt;
      walker = walker->parent_level;
    }

  return path;
}

/**
 * gtk_tree_model_sort_set_model:
 * @tree_model_sort: The `GtkTreeModelSort`.
 * @child_model: (nullable): A `GtkTreeModel`
 *
 * Sets the model of @tree_model_sort to be @model.  If @model is %NULL,
 * then the old model is unset.  The sort function is unset as a result
 * of this call. The model will be in an unsorted state until a sort
 * function is set.
 *
 * Deprecated: 4.10
 **/
static void
gtk_tree_model_sort_set_model (GtkTreeModelSort *tree_model_sort,
                               GtkTreeModel     *child_model)
{
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;

  if (child_model)
    g_object_ref (child_model);

  if (priv->child_model)
    {
      g_signal_handler_disconnect (priv->child_model,
                                   priv->changed_id);
      g_signal_handler_disconnect (priv->child_model,
                                   priv->inserted_id);
      g_signal_handler_disconnect (priv->child_model,
                                   priv->has_child_toggled_id);
      g_signal_handler_disconnect (priv->child_model,
                                   priv->deleted_id);
      g_signal_handler_disconnect (priv->child_model,
				   priv->reordered_id);

      /* reset our state */
      if (priv->root)
	gtk_tree_model_sort_free_level (tree_model_sort, priv->root, TRUE);
      priv->root = NULL;
      _gtk_tree_data_list_header_free (priv->sort_list);
      priv->sort_list = NULL;
      g_object_unref (priv->child_model);
    }

  priv->child_model = child_model;

  if (child_model)
    {
      GType *types;
      int i, n_columns;

      priv->changed_id =
        g_signal_connect (child_model, "row-changed",
                          G_CALLBACK (gtk_tree_model_sort_row_changed),
                          tree_model_sort);
      priv->inserted_id =
        g_signal_connect (child_model, "row-inserted",
                          G_CALLBACK (gtk_tree_model_sort_row_inserted),
                          tree_model_sort);
      priv->has_child_toggled_id =
        g_signal_connect (child_model, "row-has-child-toggled",
                          G_CALLBACK (gtk_tree_model_sort_row_has_child_toggled),
                          tree_model_sort);
      priv->deleted_id =
        g_signal_connect (child_model, "row-deleted",
                          G_CALLBACK (gtk_tree_model_sort_row_deleted),
                          tree_model_sort);
      priv->reordered_id =
	g_signal_connect (child_model, "rows-reordered",
			  G_CALLBACK (gtk_tree_model_sort_rows_reordered),
			  tree_model_sort);

      priv->child_flags = gtk_tree_model_get_flags (child_model);
      n_columns = gtk_tree_model_get_n_columns (child_model);

      types = g_new (GType, n_columns);
      for (i = 0; i < n_columns; i++)
        types[i] = gtk_tree_model_get_column_type (child_model, i);

      priv->sort_list = _gtk_tree_data_list_header_new (n_columns, types);
      g_free (types);

      priv->default_sort_func = NO_SORT_FUNC;
      priv->stamp = g_random_int ();
    }
}

/**
 * gtk_tree_model_sort_get_model:
 * @tree_model: a `GtkTreeModelSort`
 *
 * Returns the model the `GtkTreeModelSort` is sorting.
 *
 * Returns: (transfer none): the "child model" being sorted
 **/
GtkTreeModel *
gtk_tree_model_sort_get_model (GtkTreeModelSort *tree_model)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model), NULL);

  return tree_model->priv->child_model;
}


static GtkTreePath *
gtk_real_tree_model_sort_convert_child_path_to_path (GtkTreeModelSort *tree_model_sort,
						     GtkTreePath      *child_path,
						     gboolean          build_levels)
{
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;
  int *child_indices;
  GtkTreePath *retval;
  SortLevel *level;
  int i;

  g_return_val_if_fail (priv->child_model != NULL, NULL);
  g_return_val_if_fail (child_path != NULL, NULL);

  retval = gtk_tree_path_new ();
  child_indices = gtk_tree_path_get_indices (child_path);

  if (priv->root == NULL && build_levels)
    gtk_tree_model_sort_build_level (tree_model_sort, NULL, NULL);
  level = SORT_LEVEL (priv->root);

  for (i = 0; i < gtk_tree_path_get_depth (child_path); i++)
    {
      SortElt *tmp;
      GSequenceIter *siter;
      gboolean found_child = FALSE;

      if (!level)
	{
	  gtk_tree_path_free (retval);
	  return NULL;
	}

      if (child_indices[i] >= g_sequence_get_length (level->seq))
	{
	  gtk_tree_path_free (retval);
	  return NULL;
	}
      tmp = lookup_elt_with_offset (tree_model_sort, level,
                                    child_indices[i], &siter);
      if (tmp)
        {
          gtk_tree_path_append_index (retval, g_sequence_iter_get_position (siter));
          if (tmp->children == NULL && build_levels)
            gtk_tree_model_sort_build_level (tree_model_sort, level, tmp);

          level = tmp->children;
          found_child = TRUE;
        }

      if (! found_child)
	{
	  gtk_tree_path_free (retval);
	  return NULL;
	}
    }

  return retval;
}


/**
 * gtk_tree_model_sort_convert_child_path_to_path:
 * @tree_model_sort: A `GtkTreeModelSort`
 * @child_path: A `GtkTreePath` to convert
 *
 * Converts @child_path to a path relative to @tree_model_sort.  That is,
 * @child_path points to a path in the child model.  The returned path will
 * point to the same row in the sorted model.  If @child_path isn’t a valid
 * path on the child model, then %NULL is returned.
 *
 * Returns: (nullable) (transfer full): A newly allocated `GtkTreePath`
 *
 * Deprecated: 4.10
 **/
GtkTreePath *
gtk_tree_model_sort_convert_child_path_to_path (GtkTreeModelSort *tree_model_sort,
						GtkTreePath      *child_path)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model_sort), NULL);
  g_return_val_if_fail (tree_model_sort->priv->child_model != NULL, NULL);
  g_return_val_if_fail (child_path != NULL, NULL);

  return gtk_real_tree_model_sort_convert_child_path_to_path (tree_model_sort, child_path, TRUE);
}

/**
 * gtk_tree_model_sort_convert_child_iter_to_iter:
 * @tree_model_sort: A `GtkTreeModelSort`
 * @sort_iter: (out): An uninitialized `GtkTreeIter`
 * @child_iter: A valid `GtkTreeIter` pointing to a row on the child model
 *
 * Sets @sort_iter to point to the row in @tree_model_sort that corresponds to
 * the row pointed at by @child_iter.  If @sort_iter was not set, %FALSE
 * is returned.  Note: a boolean is only returned since 2.14.
 *
 * Returns: %TRUE, if @sort_iter was set, i.e. if @sort_iter is a
 * valid iterator pointer to a visible row in the child model.
 *
 * Deprecated: 4.10
 **/
gboolean
gtk_tree_model_sort_convert_child_iter_to_iter (GtkTreeModelSort *tree_model_sort,
						GtkTreeIter      *sort_iter,
						GtkTreeIter      *child_iter)
{
  gboolean ret;
  GtkTreePath *child_path, *path;
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model_sort), FALSE);
  g_return_val_if_fail (priv->child_model != NULL, FALSE);
  g_return_val_if_fail (sort_iter != NULL, FALSE);
  g_return_val_if_fail (child_iter != NULL, FALSE);
  g_return_val_if_fail (sort_iter != child_iter, FALSE);

  sort_iter->stamp = 0;

  child_path = gtk_tree_model_get_path (priv->child_model, child_iter);
  g_return_val_if_fail (child_path != NULL, FALSE);

  path = gtk_tree_model_sort_convert_child_path_to_path (tree_model_sort, child_path);
  gtk_tree_path_free (child_path);

  if (!path)
    {
      g_warning ("%s: The conversion of the child path to a GtkTreeModel sort path failed", G_STRLOC);
      return FALSE;
    }

  ret = gtk_tree_model_get_iter (GTK_TREE_MODEL (tree_model_sort),
                                 sort_iter, path);
  gtk_tree_path_free (path);

  return ret;
}

/**
 * gtk_tree_model_sort_convert_path_to_child_path:
 * @tree_model_sort: A `GtkTreeModelSort`
 * @sorted_path: A `GtkTreePath` to convert
 *
 * Converts @sorted_path to a path on the child model of @tree_model_sort.
 * That is, @sorted_path points to a location in @tree_model_sort.  The
 * returned path will point to the same location in the model not being
 * sorted.  If @sorted_path does not point to a location in the child model,
 * %NULL is returned.
 *
 * Returns: (nullable) (transfer full): A newly allocated `GtkTreePath`
 *
 * Deprecated: 4.10
 **/
GtkTreePath *
gtk_tree_model_sort_convert_path_to_child_path (GtkTreeModelSort *tree_model_sort,
						GtkTreePath      *sorted_path)
{
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;
  int *sorted_indices;
  GtkTreePath *retval;
  SortLevel *level;
  int i;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model_sort), NULL);
  g_return_val_if_fail (priv->child_model != NULL, NULL);
  g_return_val_if_fail (sorted_path != NULL, NULL);

  retval = gtk_tree_path_new ();
  sorted_indices = gtk_tree_path_get_indices (sorted_path);
  if (priv->root == NULL)
    gtk_tree_model_sort_build_level (tree_model_sort, NULL, NULL);
  level = SORT_LEVEL (priv->root);

  for (i = 0; i < gtk_tree_path_get_depth (sorted_path); i++)
    {
      SortElt *elt = NULL;
      GSequenceIter *siter;

      if ((level == NULL) ||
	  (g_sequence_get_length (level->seq) <= sorted_indices[i]))
	{
	  gtk_tree_path_free (retval);
	  return NULL;
	}

      siter = g_sequence_get_iter_at_pos (level->seq, sorted_indices[i]);
      if (g_sequence_iter_is_end (siter))
        {
          gtk_tree_path_free (retval);
          return NULL;
        }

      elt = GET_ELT (siter);
      g_assert (elt);
      if (elt->children == NULL)
	gtk_tree_model_sort_build_level (tree_model_sort, level, elt);

      if (level == NULL)
        {
	  gtk_tree_path_free (retval);
	  break;
	}

      gtk_tree_path_append_index (retval, elt->offset);
      level = elt->children;
    }

  return retval;
}

/**
 * gtk_tree_model_sort_convert_iter_to_child_iter:
 * @tree_model_sort: A `GtkTreeModelSort`
 * @child_iter: (out): An uninitialized `GtkTreeIter`
 * @sorted_iter: A valid `GtkTreeIter` pointing to a row on @tree_model_sort.
 *
 * Sets @child_iter to point to the row pointed to by @sorted_iter.
 *
 * Deprecated: 4.10
 **/
void
gtk_tree_model_sort_convert_iter_to_child_iter (GtkTreeModelSort *tree_model_sort,
						GtkTreeIter      *child_iter,
						GtkTreeIter      *sorted_iter)
{
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;
  g_return_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model_sort));
  g_return_if_fail (priv->child_model != NULL);
  g_return_if_fail (child_iter != NULL);
  g_return_if_fail (VALID_ITER (sorted_iter, tree_model_sort));
  g_return_if_fail (sorted_iter != child_iter);

  if (GTK_TREE_MODEL_SORT_CACHE_CHILD_ITERS (tree_model_sort))
    {
      *child_iter = SORT_ELT (sorted_iter->user_data2)->iter;
    }
  else
    {
      GtkTreePath *path;
      gboolean valid = FALSE;

      path = gtk_tree_model_sort_elt_get_path (sorted_iter->user_data,
					       sorted_iter->user_data2);
      valid = gtk_tree_model_get_iter (priv->child_model, child_iter, path);
      gtk_tree_path_free (path);

      g_return_if_fail (valid == TRUE);
    }
}

static void
gtk_tree_model_sort_build_level (GtkTreeModelSort *tree_model_sort,
				 SortLevel        *parent_level,
                                 SortElt          *parent_elt)
{
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;
  GtkTreeIter iter;
  SortLevel *new_level;
  int length = 0;
  int i;

  g_assert (priv->child_model != NULL);

  if (parent_level == NULL)
    {
      if (gtk_tree_model_get_iter_first (priv->child_model, &iter) == FALSE)
	return;
      length = gtk_tree_model_iter_n_children (priv->child_model, NULL);
    }
  else
    {
      GtkTreeIter parent_iter;
      GtkTreeIter child_parent_iter;

      parent_iter.stamp = priv->stamp;
      parent_iter.user_data = parent_level;
      parent_iter.user_data2 = parent_elt;

      gtk_tree_model_sort_convert_iter_to_child_iter (tree_model_sort,
						      &child_parent_iter,
						      &parent_iter);
      if (gtk_tree_model_iter_children (priv->child_model,
					&iter,
					&child_parent_iter) == FALSE)
	return;

      /* stamp may have changed */
      gtk_tree_model_sort_convert_iter_to_child_iter (tree_model_sort,
						      &child_parent_iter,
						      &parent_iter);

      length = gtk_tree_model_iter_n_children (priv->child_model, &child_parent_iter);

      gtk_tree_model_sort_ref_node (GTK_TREE_MODEL (tree_model_sort),
                                    &parent_iter);
    }

  g_return_if_fail (length > 0);

  new_level = g_new (SortLevel, 1);
  new_level->seq = g_sequence_new (sort_elt_free);
  new_level->ref_count = 0;
  new_level->parent_level = parent_level;
  new_level->parent_elt = parent_elt;

  if (parent_elt)
    parent_elt->children = new_level;
  else
    priv->root = new_level;

  /* increase the count of zero ref_counts.*/
  while (parent_level)
    {
      parent_elt->zero_ref_count++;

      parent_elt = parent_level->parent_elt;
      parent_level = parent_level->parent_level;
    }

  if (new_level != priv->root)
    priv->zero_ref_count++;

  for (i = 0; i < length; i++)
    {
      SortElt *sort_elt;

      sort_elt = sort_elt_new ();
      sort_elt->offset = i;
      sort_elt->zero_ref_count = 0;
      sort_elt->ref_count = 0;
      sort_elt->children = NULL;

      if (GTK_TREE_MODEL_SORT_CACHE_CHILD_ITERS (tree_model_sort))
	{
	  sort_elt->iter = iter;
	  if (gtk_tree_model_iter_next (priv->child_model, &iter) == FALSE &&
	      i < length - 1)
	    {
	      if (parent_level)
	        {
	          GtkTreePath *level;
		  char *str;

		  level = gtk_tree_model_sort_elt_get_path (parent_level,
							    parent_elt);
		  str = gtk_tree_path_to_string (level);
		  gtk_tree_path_free (level);

		  g_warning ("%s: There is a discrepancy between the sort model "
			     "and the child model.  The child model is "
			     "advertising a wrong length for level %s:.",
			     G_STRLOC, str);
		  g_free (str);
		}
	      else
	        {
		  g_warning ("%s: There is a discrepancy between the sort model "
			     "and the child model.  The child model is "
			     "advertising a wrong length for the root level.",
			     G_STRLOC);
		}

	      return;
	    }
	}

      sort_elt->siter = g_sequence_append (new_level->seq, sort_elt);
    }

  /* sort level */
  gtk_tree_model_sort_sort_level (tree_model_sort, new_level,
				  FALSE, FALSE);
}

static void
gtk_tree_model_sort_free_level (GtkTreeModelSort *tree_model_sort,
				SortLevel        *sort_level,
                                gboolean          unref)
{
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;
  GSequenceIter *siter;
  GSequenceIter *end_siter;

  g_assert (sort_level);

  end_siter = g_sequence_get_end_iter (sort_level->seq);
  for (siter = g_sequence_get_begin_iter (sort_level->seq);
       siter != end_siter;
       siter = g_sequence_iter_next (siter))
    {
      SortElt *elt = g_sequence_get (siter);

      if (elt->children)
        gtk_tree_model_sort_free_level (tree_model_sort,
                                        elt->children, unref);
    }

  if (sort_level->ref_count == 0)
    {
      SortLevel *parent_level = sort_level->parent_level;
      SortElt *parent_elt = sort_level->parent_elt;

      while (parent_level)
        {
          parent_elt->zero_ref_count--;

          parent_elt = parent_level->parent_elt;
	  parent_level = parent_level->parent_level;
	}

      if (sort_level != priv->root)
	priv->zero_ref_count--;
    }

  if (sort_level->parent_elt)
    {
      if (unref)
        {
          GtkTreeIter parent_iter;

          parent_iter.stamp = tree_model_sort->priv->stamp;
          parent_iter.user_data = sort_level->parent_level;
          parent_iter.user_data2 = sort_level->parent_elt;

          gtk_tree_model_sort_unref_node (GTK_TREE_MODEL (tree_model_sort),
                                          &parent_iter);
        }

      sort_level->parent_elt->children = NULL;
    }
  else
    priv->root = NULL;

  g_sequence_free (sort_level->seq);
  sort_level->seq = NULL;

  g_free (sort_level);
  sort_level = NULL;
}

static void
gtk_tree_model_sort_increment_stamp (GtkTreeModelSort *tree_model_sort)
{
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;

  do
    {
      priv->stamp++;
    }
  while (priv->stamp == 0);

  gtk_tree_model_sort_clear_cache (tree_model_sort);
}

static void
gtk_tree_model_sort_clear_cache_helper_iter (gpointer data,
                                             gpointer user_data)
{
  GtkTreeModelSort *tree_model_sort = user_data;
  SortElt *elt = data;

  if (elt->zero_ref_count > 0)
    gtk_tree_model_sort_clear_cache_helper (tree_model_sort, elt->children);
}

static void
gtk_tree_model_sort_clear_cache_helper (GtkTreeModelSort *tree_model_sort,
					SortLevel        *level)
{
  g_assert (level != NULL);

  g_sequence_foreach (level->seq, gtk_tree_model_sort_clear_cache_helper_iter,
                      tree_model_sort);

  if (level->ref_count == 0 && level != tree_model_sort->priv->root)
    gtk_tree_model_sort_free_level (tree_model_sort, level, TRUE);
}

/**
 * gtk_tree_model_sort_reset_default_sort_func:
 * @tree_model_sort: A `GtkTreeModelSort`
 *
 * This resets the default sort function to be in the “unsorted” state.  That
 * is, it is in the same order as the child model. It will re-sort the model
 * to be in the same order as the child model only if the `GtkTreeModelSort`
 * is in “unsorted” state.
 *
 * Deprecated: 4.10
 **/
void
gtk_tree_model_sort_reset_default_sort_func (GtkTreeModelSort *tree_model_sort)
{
  GtkTreeModelSortPrivate *priv = tree_model_sort->priv;

  g_return_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model_sort));

  if (priv->default_sort_destroy)
    {
      GDestroyNotify d = priv->default_sort_destroy;

      priv->default_sort_destroy = NULL;
      d (priv->default_sort_data);
    }

  priv->default_sort_func = NO_SORT_FUNC;
  priv->default_sort_data = NULL;
  priv->default_sort_destroy = NULL;

  if (priv->sort_column_id == GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID)
    gtk_tree_model_sort_sort (tree_model_sort);
  priv->sort_column_id = GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID;
}

/**
 * gtk_tree_model_sort_clear_cache:
 * @tree_model_sort: A `GtkTreeModelSort`
 *
 * This function should almost never be called.  It clears the @tree_model_sort
 * of any cached iterators that haven’t been reffed with
 * gtk_tree_model_ref_node().  This might be useful if the child model being
 * sorted is static (and doesn’t change often) and there has been a lot of
 * unreffed access to nodes.  As a side effect of this function, all unreffed
 * iters will be invalid.
 *
 * Deprecated: 4.10
 **/
void
gtk_tree_model_sort_clear_cache (GtkTreeModelSort *tree_model_sort)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model_sort));

  if (tree_model_sort->priv->zero_ref_count > 0)
    gtk_tree_model_sort_clear_cache_helper (tree_model_sort, (SortLevel *)tree_model_sort->priv->root);
}

static gboolean
gtk_tree_model_sort_iter_is_valid_helper (GtkTreeIter *iter,
					  SortLevel   *level)
{
  GSequenceIter *siter;
  GSequenceIter *end_siter;

  end_siter = g_sequence_get_end_iter (level->seq);
  for (siter = g_sequence_get_begin_iter (level->seq);
       siter != end_siter; siter = g_sequence_iter_next (siter))
    {
      SortElt *elt = g_sequence_get (siter);

      if (iter->user_data == level && iter->user_data2 == elt)
	return TRUE;

      if (elt->children)
	if (gtk_tree_model_sort_iter_is_valid_helper (iter, elt->children))
	  return TRUE;
    }

  return FALSE;
}

/**
 * gtk_tree_model_sort_iter_is_valid:
 * @tree_model_sort: A `GtkTreeModelSort`.
 * @iter: A `GtkTreeIter`
 *
 * > This function is slow. Only use it for debugging and/or testing
 * > purposes.
 *
 * Checks if the given iter is a valid iter for this `GtkTreeModelSort`.
 *
 * Returns: %TRUE if the iter is valid, %FALSE if the iter is invalid.
 *
 * Deprecated: 4.10
 **/
gboolean
gtk_tree_model_sort_iter_is_valid (GtkTreeModelSort *tree_model_sort,
                                   GtkTreeIter      *iter)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL_SORT (tree_model_sort), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  if (!VALID_ITER (iter, tree_model_sort))
    return FALSE;

  return gtk_tree_model_sort_iter_is_valid_helper (iter,
						   tree_model_sort->priv->root);
}
