/* gtktreemodelfilter.c
 * Copyright (C) 2000,2001  Red Hat, Inc., Jonathan Blandford <jrb@redhat.com>
 * Copyright (C) 2001-2003  Kristian Rietveld <kris@gtk.org>
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
#include "gtktreemodelfilter.h"
#include "gtktreednd.h"
#include "gtkprivate.h"
#include <string.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * GtkTreeModelFilter:
 *
 * A `GtkTreeModel` which hides parts of an underlying tree model
 *
 * A `GtkTreeModelFilter` is a tree model which wraps another tree model,
 * and can do the following things:
 *
 * - Filter specific rows, based on data from a “visible column”, a column
 *   storing booleans indicating whether the row should be filtered or not,
 *   or based on the return value of a “visible function”, which gets a
 *   model, iter and user_data and returns a boolean indicating whether the
 *   row should be filtered or not.
 *
 * - Modify the “appearance” of the model, using a modify function.
 *   This is extremely powerful and allows for just changing some
 *   values and also for creating a completely different model based
 *   on the given child model.
 *
 * - Set a different root node, also known as a “virtual root”. You can pass
 *   in a `GtkTreePath` indicating the root node for the filter at construction
 *   time.
 *
 * The basic API is similar to `GtkTreeModelSort`. For an example on its usage,
 * see the section on `GtkTreeModelSort`.
 *
 * When using `GtkTreeModelFilter`, it is important to realize that
 * `GtkTreeModelFilter` maintains an internal cache of all nodes which are
 * visible in its clients. The cache is likely to be a subtree of the tree
 * exposed by the child model. `GtkTreeModelFilter` will not cache the entire
 * child model when unnecessary to not compromise the caching mechanism
 * that is exposed by the reference counting scheme. If the child model
 * implements reference counting, unnecessary signals may not be emitted
 * because of reference counting rule 3, see the `GtkTreeModel`
 * documentation. (Note that e.g. `GtkTreeStore` does not implement
 * reference counting and will always emit all signals, even when
 * the receiving node is not visible).
 *
 * Because of this, limitations for possible visible functions do apply.
 * In general, visible functions should only use data or properties from
 * the node for which the visibility state must be determined, its siblings
 * or its parents. Usually, having a dependency on the state of any child
 * node is not possible, unless references are taken on these explicitly.
 * When no such reference exists, no signals may be received for these child
 * nodes (see reference counting rule number 3 in the `GtkTreeModel` section).
 *
 * Determining the visibility state of a given node based on the state
 * of its child nodes is a frequently occurring use case. Therefore,
 * `GtkTreeModelFilter` explicitly supports this. For example, when a node
 * does not have any children, you might not want the node to be visible.
 * As soon as the first row is added to the node’s child level (or the
 * last row removed), the node’s visibility should be updated.
 *
 * This introduces a dependency from the node on its child nodes. In order
 * to accommodate this, `GtkTreeModelFilter` must make sure the necessary
 * signals are received from the child model. This is achieved by building,
 * for all nodes which are exposed as visible nodes to `GtkTreeModelFilter`'s
 * clients, the child level (if any) and take a reference on the first node
 * in this level. Furthermore, for every row-inserted, row-changed or
 * row-deleted signal (also these which were not handled because the node
 * was not cached), `GtkTreeModelFilter` will check if the visibility state
 * of any parent node has changed.
 *
 * Beware, however, that this explicit support is limited to these two
 * cases. For example, if you want a node to be visible only if two nodes
 * in a child’s child level (2 levels deeper) are visible, you are on your
 * own. In this case, either rely on `GtkTreeStore` to emit all signals
 * because it does not implement reference counting, or for models that
 * do implement reference counting, obtain references on these child levels
 * yourself.
 *
 * Deprecated: 4.10: Use [class@Gtk.FilterListModel] instead.
 */

/* Notes on this implementation of GtkTreeModelFilter
 * ==================================================
 *
 * Warnings
 * --------
 *
 * In this code there is a potential for confusion as to whether an iter,
 * path or value refers to the GtkTreeModelFilter model, or to the child
 * model that has been set. As a convention, variables referencing the
 * child model will have a c_ prefix before them (ie. c_iter, c_value,
 * c_path). In case the c_ prefixed names are already in use, an f_
 * prefix is used. Conversion of iterators and paths between
 * GtkTreeModelFilter and the child model is done through the various
 * gtk_tree_model_filter_convert_* functions.
 *
 * Even though the GtkTreeModelSort and GtkTreeModelFilter have very
 * similar data structures, many assumptions made in the GtkTreeModelSort
 * code do *not* apply in the GtkTreeModelFilter case. Reference counting
 * in particular is more complicated in GtkTreeModelFilter, because
 * we explicitly support reliance on the state of a node’s children as
 * outlined in the public API documentation. Because of these differences,
 * you are strongly recommended to first read through these notes before
 * making any modification to the code.
 *
 * Iterator format
 * ---------------
 *
 * The iterator format of iterators handed out by GtkTreeModelFilter is
 * as follows:
 *
 *     iter->stamp = filter->priv->stamp
 *     iter->user_data = FilterLevel
 *     iter->user_data2 = FilterElt
 *
 * Internal data structure
 * -----------------------
 *
 * Using FilterLevel and FilterElt, GtkTreeModelFilter maintains a “cache”
 * of the mapping from GtkTreeModelFilter nodes to nodes in the child model.
 * This is to avoid re-creating a level each time (which involves computing
 * visibility for each node in that level) an operation is requested on
 * GtkTreeModelFilter, such as get iter, get path and get value.
 *
 * A FilterElt corresponds to a single node. The FilterElt can either be
 * visible or invisible in the model that is exposed to the clients of this
 * GtkTreeModelFilter. The visibility state is stored in the “visible_siter”
 * field, which is NULL when the node is not visible. The FilterLevel keeps
 * a reference to the parent FilterElt and its FilterLevel (if any). The
 * FilterElt can have a “children” pointer set, which points at a child
 * level (a sub level).
 *
 * In a FilterLevel, two separate GSequences are maintained. One contains
 * all nodes of this FilterLevel, regardless of the visibility state of
 * the node. Another contains only visible nodes. A visible FilterElt
 * is thus present in both the full and the visible GSequence. The
 * GSequence allows for fast access, addition and removal of nodes.
 *
 * It is important to recognize the two different mappings that play
 * a part in this code:
 *   I.  The mapping from the client to this model. The order in which
 *       nodes are stored in the *visible* GSequence is the order in
 *       which the nodes are exposed to clients of the GtkTreeModelFilter.
 *   II. The mapping from this model to its child model. Each FilterElt
 *       contains an “offset” field which is the offset of the
 *       corresponding node in the child model.
 *
 * Throughout the code, two kinds of paths relative to the GtkTreeModelFilter
 * (those generated from the sequence positions) are used. There are paths
 * which take non-visible nodes into account (generated from the full
 * sequences) and paths which don’t (generated from the visible sequences).
 * Paths which have been generated from the full sequences should only be
 * used internally and NEVER be passed along with a signal emisson.
 *
 * Reference counting
 * ------------------
 *
 * GtkTreeModelFilter forwards all reference and unreference operations
 * to the corresponding node in the child model. In addition,
 * GtkTreeModelFilter will also add references of its own. The full reference
 * count of each node (i.e. all forwarded references and these by the
 * filter model) is maintained internally in the “ref_count” fields in
 * FilterElt and FilterLevel. Because there is a need to determine whether
 * a node should be visible for the client, the reference count of only
 * the forwarded references is maintained as well, in the “ext_ref_count”
 * fields.
 *
 * In a few cases, GtkTreeModelFilter takes additional references on
 * nodes. The first case is that a reference is taken on the parent
 * (if any) of each level. This happens in gtk_tree_model_filter_build_level()
 * and the reference is released again in gtk_tree_model_filter_free_level().
 * This ensures that for all references which are taken by the filter
 * model, all parent nodes are referenced according to reference counting
 * rule 1 in the GtkTreeModel documentation.
 *
 * A second case is required to support visible functions which depend on
 * the state of a node’s children (see the public API documentation for
 * GtkTreeModelFilter above). We build the child level of each node that
 * could be visible in the client (i.e. the level has an ext_ref_count > 0;
 * not the elt, because the elt might be invisible and thus unreferenced
 * by the client). For each node that becomes visible, due to insertion or
 * changes in visibility state, it is checked whether node has children, if
 * so the child level is built.
 *
 * A reference is taken on the first node of each level so that the child
 * model will emit all signals for this level, due to reference counting
 * rule 3 in the GtkTreeModel documentation. If due to changes in the level,
 * another node becomes the first node (e.g. due to insertion or reordering),
 * this reference is transferred from the old to the new first node.
 *
 * When a level has an *external* reference count of zero (which means that
 * none of the nodes in the level is referenced by the clients), the level
 * has a “zero ref count” on all its parents. As soon as the level reaches
 * an *external* reference count of zero, the zero ref count value is
 * incremented by one for all parents of this level. Due to the additional
 * references taken by the filter model, it is important to base the
 * zero ref count on the external reference count instead of on the full
 * reference count of the node.
 *
 * The zero ref count value aids in determining which portions of the
 * cache are possibly unused and could be removed. If a FilterElt has
 * a zero ref count of one, then its child level is unused. However, the
 * child level can only be removed from the cache if the FilterElt's
 * parent level has an external ref count of zero. (Not the parent elt,
 * because an invisible parent elt with external ref count == 0 might still
 * become visible because of a state change in its child level!).  Otherwise,
 * monitoring this level is necessary to possibly update the visibility state
 * of the parent. This is an important difference from GtkTreeModelSort!
 *
 * Signals are only required for levels with an external ref count > 0.
 * This due to reference counting rule 3, see the GtkTreeModel
 * documentation. In the GtkTreeModelFilter we try hard to stick to this
 * rule and not emit redundant signals (though redundant emissions of
 * row-has-child-toggled could appear frequently; it does happen that
 * we simply forward the signal emitted by e.g. GtkTreeStore but also
 * emit our own copy).
 */


typedef struct _FilterElt FilterElt;
typedef struct _FilterLevel FilterLevel;

struct _FilterElt
{
  GtkTreeIter iter;
  FilterLevel *children;
  int offset;
  int ref_count;
  int ext_ref_count;
  int zero_ref_count;
  GSequenceIter *visible_siter; /* iter into visible_seq */
};

struct _FilterLevel
{
  GSequence *seq;
  GSequence *visible_seq;
  int ref_count;
  int ext_ref_count;

  FilterElt *parent_elt;
  FilterLevel *parent_level;
};


struct _GtkTreeModelFilterPrivate
{
  GtkTreeModel *child_model;
  gpointer root;
  GtkTreePath *virtual_root;

  int stamp;
  guint child_flags;
  int zero_ref_count;
  int visible_column;

  GtkTreeModelFilterVisibleFunc visible_func;
  gpointer visible_data;
  GDestroyNotify visible_destroy;

  GType *modify_types;
  GtkTreeModelFilterModifyFunc modify_func;
  gpointer modify_data;
  GDestroyNotify modify_destroy;
  int modify_n_columns;

  guint visible_method_set   : 1;
  guint modify_func_set      : 1;

  guint in_row_deleted       : 1;
  guint virtual_root_deleted : 1;

  /* signal ids */
  gulong changed_id;
  gulong inserted_id;
  gulong has_child_toggled_id;
  gulong deleted_id;
  gulong reordered_id;
};

/* properties */
enum
{
  PROP_0,
  PROP_CHILD_MODEL,
  PROP_VIRTUAL_ROOT
};

/* Set this to 0 to disable caching of child iterators.  This
 * allows for more stringent testing.  It is recommended to set this
 * to one when refactoring this code and running the unit tests to
 * catch more errors.
 */
#if 1
#  define GTK_TREE_MODEL_FILTER_CACHE_CHILD_ITERS(filter) \
        (((GtkTreeModelFilter *)filter)->priv->child_flags & GTK_TREE_MODEL_ITERS_PERSIST)
#else
#  define GTK_TREE_MODEL_FILTER_CACHE_CHILD_ITERS(filter) (FALSE)
#endif

/* Defining this constant enables more assertions, which will be
 * helpful when debugging the code.
 */
#undef MODEL_FILTER_DEBUG

#define FILTER_ELT(filter_elt) ((FilterElt *)filter_elt)
#define FILTER_LEVEL(filter_level) ((FilterLevel *)filter_level)
#define GET_ELT(siter) ((FilterElt*) (siter ? g_sequence_get (siter) : NULL))

/* general code (object/interface init, properties, etc) */
static void         gtk_tree_model_filter_tree_model_init                 (GtkTreeModelIface       *iface);
static void         gtk_tree_model_filter_drag_source_init                (GtkTreeDragSourceIface  *iface);
static void         gtk_tree_model_filter_finalize                        (GObject                 *object);
static void         gtk_tree_model_filter_set_property                    (GObject                 *object,
                                                                           guint                    prop_id,
                                                                           const GValue            *value,
                                                                           GParamSpec              *pspec);
static void         gtk_tree_model_filter_get_property                    (GObject                 *object,
                                                                           guint                    prop_id,
                                                                           GValue                 *value,
                                                                           GParamSpec             *pspec);

/* signal handlers */
static void         gtk_tree_model_filter_row_changed                     (GtkTreeModel           *c_model,
                                                                           GtkTreePath            *c_path,
                                                                           GtkTreeIter            *c_iter,
                                                                           gpointer                data);
static void         gtk_tree_model_filter_row_inserted                    (GtkTreeModel           *c_model,
                                                                           GtkTreePath            *c_path,
                                                                           GtkTreeIter            *c_iter,
                                                                           gpointer                data);
static void         gtk_tree_model_filter_row_has_child_toggled           (GtkTreeModel           *c_model,
                                                                           GtkTreePath            *c_path,
                                                                           GtkTreeIter            *c_iter,
                                                                           gpointer                data);
static void         gtk_tree_model_filter_row_deleted                     (GtkTreeModel           *c_model,
                                                                           GtkTreePath            *c_path,
                                                                           gpointer                data);
static void         gtk_tree_model_filter_rows_reordered                  (GtkTreeModel           *c_model,
                                                                           GtkTreePath            *c_path,
                                                                           GtkTreeIter            *c_iter,
                                                                           int                    *new_order,
                                                                           gpointer                data);

/* GtkTreeModel interface */
static GtkTreeModelFlags gtk_tree_model_filter_get_flags                       (GtkTreeModel           *model);
static int          gtk_tree_model_filter_get_n_columns                   (GtkTreeModel           *model);
static GType        gtk_tree_model_filter_get_column_type                 (GtkTreeModel           *model,
                                                                           int                     index);
static gboolean     gtk_tree_model_filter_get_iter_full                   (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter,
                                                                           GtkTreePath            *path);
static gboolean     gtk_tree_model_filter_get_iter                        (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter,
                                                                           GtkTreePath            *path);
static GtkTreePath *gtk_tree_model_filter_get_path                        (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter);
static void         gtk_tree_model_filter_get_value                       (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter,
                                                                           int                     column,
                                                                           GValue                 *value);
static gboolean     gtk_tree_model_filter_iter_next                       (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter);
static gboolean     gtk_tree_model_filter_iter_previous                   (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter);
static gboolean     gtk_tree_model_filter_iter_children                   (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter,
                                                                           GtkTreeIter            *parent);
static gboolean     gtk_tree_model_filter_iter_has_child                  (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter);
static int          gtk_tree_model_filter_iter_n_children                 (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter);
static gboolean     gtk_tree_model_filter_iter_nth_child                  (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter,
                                                                           GtkTreeIter            *parent,
                                                                           int                     n);
static gboolean     gtk_tree_model_filter_iter_parent                     (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter,
                                                                           GtkTreeIter            *child);
static void         gtk_tree_model_filter_ref_node                        (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter);
static void         gtk_tree_model_filter_unref_node                      (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter);

/* TreeDragSource interface */
static gboolean    gtk_tree_model_filter_row_draggable                    (GtkTreeDragSource      *drag_source,
                                                                           GtkTreePath            *path);
static GdkContentProvider *
                   gtk_tree_model_filter_drag_data_get                    (GtkTreeDragSource      *drag_source,
                                                                           GtkTreePath            *path);
static gboolean    gtk_tree_model_filter_drag_data_delete                 (GtkTreeDragSource      *drag_source,
                                                                           GtkTreePath            *path);

/* private functions */
static void        gtk_tree_model_filter_build_level                      (GtkTreeModelFilter     *filter,
                                                                           FilterLevel            *parent_level,
                                                                           FilterElt              *parent_elt,
                                                                           gboolean                emit_inserted);

static void        gtk_tree_model_filter_free_level                       (GtkTreeModelFilter     *filter,
                                                                           FilterLevel            *filter_level,
                                                                           gboolean                unref_self,
                                                                           gboolean                unref_parent,
                                                                           gboolean                unref_external);

static GtkTreePath *gtk_tree_model_filter_elt_get_path                    (FilterLevel            *level,
                                                                           FilterElt              *elt,
                                                                           GtkTreePath            *root);

static GtkTreePath *gtk_tree_model_filter_add_root                        (GtkTreePath            *src,
                                                                           GtkTreePath            *root);
static GtkTreePath *gtk_tree_model_filter_remove_root                     (GtkTreePath            *src,
                                                                           GtkTreePath            *root);

static void         gtk_tree_model_filter_increment_stamp                 (GtkTreeModelFilter     *filter);

static void         gtk_tree_model_filter_real_modify                     (GtkTreeModelFilter     *self,
                                                                           GtkTreeModel           *child_model,
                                                                           GtkTreeIter            *iter,
                                                                           GValue                 *value,
                                                                           int                     column);
static gboolean     gtk_tree_model_filter_real_visible                    (GtkTreeModelFilter     *filter,
                                                                           GtkTreeModel           *child_model,
                                                                           GtkTreeIter            *child_iter);
static gboolean     gtk_tree_model_filter_visible                         (GtkTreeModelFilter     *filter,
                                                                           GtkTreeIter            *child_iter);
static void         gtk_tree_model_filter_clear_cache_helper              (GtkTreeModelFilter     *filter,
                                                                           FilterLevel            *level);

static void         gtk_tree_model_filter_real_ref_node                   (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter,
                                                                           gboolean                external);
static void         gtk_tree_model_filter_real_unref_node                 (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter,
                                                                           gboolean                external,
                                                                           gboolean                propagate_unref);

static void         gtk_tree_model_filter_set_model                       (GtkTreeModelFilter     *filter,
                                                                           GtkTreeModel           *child_model);
static void         gtk_tree_model_filter_ref_path                        (GtkTreeModelFilter     *filter,
                                                                           GtkTreePath            *path);
static void         gtk_tree_model_filter_unref_path                      (GtkTreeModelFilter     *filter,
                                                                           GtkTreePath            *path,
                                                                           int                     depth);
static void         gtk_tree_model_filter_set_root                        (GtkTreeModelFilter     *filter,
                                                                           GtkTreePath            *root);

static GtkTreePath *gtk_real_tree_model_filter_convert_child_path_to_path (GtkTreeModelFilter     *filter,
                                                                           GtkTreePath            *child_path,
                                                                           gboolean                build_levels,
                                                                           gboolean                fetch_children);

static gboolean    gtk_tree_model_filter_elt_is_visible_in_target         (FilterLevel            *level,
                                                                           FilterElt              *elt);

static FilterElt   *gtk_tree_model_filter_insert_elt_in_level             (GtkTreeModelFilter     *filter,
                                                                           GtkTreeIter            *c_iter,
                                                                           FilterLevel            *level,
                                                                           int                     offset,
                                                                           int                    *index);
static FilterElt   *gtk_tree_model_filter_fetch_child                     (GtkTreeModelFilter     *filter,
                                                                           FilterLevel            *level,
                                                                           int                     offset,
                                                                           int                    *index);
static void         gtk_tree_model_filter_remove_elt_from_level           (GtkTreeModelFilter     *filter,
                                                                           FilterLevel            *level,
                                                                           FilterElt              *elt);
static void         gtk_tree_model_filter_update_children                 (GtkTreeModelFilter     *filter,
                                                                           FilterLevel            *level,
                                                                           FilterElt              *elt);
static void         gtk_tree_model_filter_emit_row_inserted_for_path      (GtkTreeModelFilter     *filter,
                                                                           GtkTreeModel           *c_model,
                                                                           GtkTreePath            *c_path,
                                                                           GtkTreeIter            *c_iter);


G_DEFINE_TYPE_WITH_CODE (GtkTreeModelFilter, gtk_tree_model_filter, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GtkTreeModelFilter)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
						gtk_tree_model_filter_tree_model_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
						gtk_tree_model_filter_drag_source_init))

static void
gtk_tree_model_filter_init (GtkTreeModelFilter *filter)
{
  filter->priv = gtk_tree_model_filter_get_instance_private (filter);
  filter->priv->visible_column = -1;
  filter->priv->zero_ref_count = 0;
  filter->priv->visible_method_set = FALSE;
  filter->priv->modify_func_set = FALSE;
  filter->priv->in_row_deleted = FALSE;
  filter->priv->virtual_root_deleted = FALSE;
}

static void
gtk_tree_model_filter_class_init (GtkTreeModelFilterClass *filter_class)
{
  GObjectClass *object_class;

  object_class = (GObjectClass *) filter_class;

  object_class->set_property = gtk_tree_model_filter_set_property;
  object_class->get_property = gtk_tree_model_filter_get_property;

  object_class->finalize = gtk_tree_model_filter_finalize;

  filter_class->visible = gtk_tree_model_filter_real_visible;
  filter_class->modify  = gtk_tree_model_filter_real_modify;

  g_object_class_install_property (object_class,
                                   PROP_CHILD_MODEL,
                                   g_param_spec_object ("child-model", NULL, NULL,
                                                        GTK_TYPE_TREE_MODEL,
                                                        GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class,
                                   PROP_VIRTUAL_ROOT,
                                   g_param_spec_boxed ("virtual-root", NULL, NULL,
                                                       GTK_TYPE_TREE_PATH,
                                                       GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
gtk_tree_model_filter_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = gtk_tree_model_filter_get_flags;
  iface->get_n_columns = gtk_tree_model_filter_get_n_columns;
  iface->get_column_type = gtk_tree_model_filter_get_column_type;
  iface->get_iter = gtk_tree_model_filter_get_iter;
  iface->get_path = gtk_tree_model_filter_get_path;
  iface->get_value = gtk_tree_model_filter_get_value;
  iface->iter_next = gtk_tree_model_filter_iter_next;
  iface->iter_previous = gtk_tree_model_filter_iter_previous;
  iface->iter_children = gtk_tree_model_filter_iter_children;
  iface->iter_has_child = gtk_tree_model_filter_iter_has_child;
  iface->iter_n_children = gtk_tree_model_filter_iter_n_children;
  iface->iter_nth_child = gtk_tree_model_filter_iter_nth_child;
  iface->iter_parent = gtk_tree_model_filter_iter_parent;
  iface->ref_node = gtk_tree_model_filter_ref_node;
  iface->unref_node = gtk_tree_model_filter_unref_node;
}

static void
gtk_tree_model_filter_drag_source_init (GtkTreeDragSourceIface *iface)
{
  iface->row_draggable = gtk_tree_model_filter_row_draggable;
  iface->drag_data_delete = gtk_tree_model_filter_drag_data_delete;
  iface->drag_data_get = gtk_tree_model_filter_drag_data_get;
}


static void
gtk_tree_model_filter_finalize (GObject *object)
{
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *) object;

  if (filter->priv->virtual_root && !filter->priv->virtual_root_deleted)
    {
      gtk_tree_model_filter_unref_path (filter, filter->priv->virtual_root,
                                        -1);
      filter->priv->virtual_root_deleted = TRUE;
    }

  gtk_tree_model_filter_set_model (filter, NULL);

  if (filter->priv->virtual_root)
    gtk_tree_path_free (filter->priv->virtual_root);

  if (filter->priv->root)
    gtk_tree_model_filter_free_level (filter, filter->priv->root, TRUE, TRUE, FALSE);

  g_free (filter->priv->modify_types);

  if (filter->priv->modify_destroy)
    filter->priv->modify_destroy (filter->priv->modify_data);

  if (filter->priv->visible_destroy)
    filter->priv->visible_destroy (filter->priv->visible_data);

  /* must chain up */
  G_OBJECT_CLASS (gtk_tree_model_filter_parent_class)->finalize (object);
}

static void
gtk_tree_model_filter_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (object);

  switch (prop_id)
    {
      case PROP_CHILD_MODEL:
        gtk_tree_model_filter_set_model (filter, g_value_get_object (value));
        break;
      case PROP_VIRTUAL_ROOT:
        gtk_tree_model_filter_set_root (filter, g_value_get_boxed (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gtk_tree_model_filter_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (object);

  switch (prop_id)
    {
      case PROP_CHILD_MODEL:
        g_value_set_object (value, filter->priv->child_model);
        break;
      case PROP_VIRTUAL_ROOT:
        g_value_set_boxed (value, filter->priv->virtual_root);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/* helpers */

static FilterElt *
filter_elt_new (void)
{
  return g_slice_new (FilterElt);
}

static void
filter_elt_free (gpointer elt)
{
  g_slice_free (FilterElt, elt);
}

static int
filter_elt_cmp (gconstpointer a,
                gconstpointer b,
                gpointer      user_data)
{
  const FilterElt *elt_a = a;
  const FilterElt *elt_b = b;

  if (elt_a->offset > elt_b->offset)
    return +1;
  else if (elt_a->offset < elt_b->offset)
    return -1;
  else
    return 0;
}

static FilterElt *
lookup_elt_with_offset (GSequence      *seq,
                        int             offset,
                        GSequenceIter **ret_siter)
{
  GSequenceIter *siter;
  FilterElt dummy;

  dummy.offset = offset;
  siter = g_sequence_lookup (seq, &dummy, filter_elt_cmp, NULL);

  if (ret_siter)
    *ret_siter = siter;

  return GET_ELT (siter);
}

static void
increase_offset_iter (gpointer data,
                      gpointer user_data)
{
  FilterElt *elt = data;
  int offset = GPOINTER_TO_INT (user_data);

  if (elt->offset >= offset)
    elt->offset++;
}

static void
decrease_offset_iter (gpointer data,
                      gpointer user_data)
{
  FilterElt *elt = data;
  int offset = GPOINTER_TO_INT (user_data);

  if (elt->offset > offset)
    elt->offset--;
}

static void
gtk_tree_model_filter_build_level (GtkTreeModelFilter *filter,
                                   FilterLevel        *parent_level,
                                   FilterElt          *parent_elt,
                                   gboolean            emit_inserted)
{
  GtkTreeIter iter;
  GtkTreeIter first_node;
  GtkTreeIter root;
  FilterLevel *new_level;
  FilterLevel *tmp_level;
  FilterElt *tmp_elt;
  GtkTreeIter f_iter;
  int length = 0;
  int i;
  gboolean empty = TRUE;

  g_assert (filter->priv->child_model != NULL);

  /* Avoid building a level that already exists */
#ifndef G_DISABLE_ASSERT
  if (parent_level)
    g_assert (parent_elt->children == NULL);
  else
    g_assert (filter->priv->root == NULL);
#endif

  if (filter->priv->in_row_deleted)
    return;

  if (!parent_level)
    {
      if (filter->priv->virtual_root)
        {
          if (gtk_tree_model_get_iter (filter->priv->child_model, &root, filter->priv->virtual_root) == FALSE)
            return;
          length = gtk_tree_model_iter_n_children (filter->priv->child_model, &root);

          if (gtk_tree_model_iter_children (filter->priv->child_model, &iter, &root) == FALSE)
            return;
        }
      else
        {
          if (!gtk_tree_model_get_iter_first (filter->priv->child_model, &iter))
            return;
          length = gtk_tree_model_iter_n_children (filter->priv->child_model, NULL);
        }
    }
  else
    {
      GtkTreeIter parent_iter;
      GtkTreeIter child_parent_iter;

      parent_iter.stamp = filter->priv->stamp;
      parent_iter.user_data = parent_level;
      parent_iter.user_data2 = parent_elt;

      gtk_tree_model_filter_convert_iter_to_child_iter (filter,
                                                        &child_parent_iter,
                                                        &parent_iter);
      if (gtk_tree_model_iter_children (filter->priv->child_model, &iter, &child_parent_iter) == FALSE)
        return;

      /* stamp may have changed */
      gtk_tree_model_filter_convert_iter_to_child_iter (filter,
                                                        &child_parent_iter,
                                                        &parent_iter);
      length = gtk_tree_model_iter_n_children (filter->priv->child_model, &child_parent_iter);

      /* Take a reference on the parent */
      gtk_tree_model_filter_real_ref_node (GTK_TREE_MODEL (filter),
                                           &parent_iter, FALSE);
    }

  g_return_if_fail (length > 0);

  new_level = g_new (FilterLevel, 1);
  new_level->seq = g_sequence_new (filter_elt_free);
  new_level->visible_seq = g_sequence_new (NULL);
  new_level->ref_count = 0;
  new_level->ext_ref_count = 0;
  new_level->parent_elt = parent_elt;
  new_level->parent_level = parent_level;

  if (parent_elt)
    parent_elt->children = new_level;
  else
    filter->priv->root = new_level;

  /* increase the count of zero ref_counts */
  tmp_level = parent_level;
  tmp_elt = parent_elt;

  while (tmp_level)
    {
      tmp_elt->zero_ref_count++;

      tmp_elt = tmp_level->parent_elt;
      tmp_level = tmp_level->parent_level;
    }
  if (new_level != filter->priv->root)
    filter->priv->zero_ref_count++;

  i = 0;

  first_node = iter;

  do
    {
      if (gtk_tree_model_filter_visible (filter, &iter))
        {
          FilterElt *filter_elt;

          filter_elt = filter_elt_new ();
          filter_elt->offset = i;
          filter_elt->zero_ref_count = 0;
          filter_elt->ref_count = 0;
          filter_elt->ext_ref_count = 0;
          filter_elt->children = NULL;
          filter_elt->visible_siter = NULL;

          if (GTK_TREE_MODEL_FILTER_CACHE_CHILD_ITERS (filter))
            filter_elt->iter = iter;

          g_sequence_append (new_level->seq, filter_elt);
          filter_elt->visible_siter = g_sequence_append (new_level->visible_seq, filter_elt);
          empty = FALSE;

          if (emit_inserted)
            {
              GtkTreePath *f_path;
              GtkTreeIter children;

              f_iter.stamp = filter->priv->stamp;
              f_iter.user_data = new_level;
              f_iter.user_data2 = filter_elt;

              f_path = gtk_tree_model_get_path (GTK_TREE_MODEL (filter),
                                                &f_iter);
              gtk_tree_model_row_inserted (GTK_TREE_MODEL (filter),
                                           f_path, &f_iter);
              gtk_tree_path_free (f_path);

              if (gtk_tree_model_iter_children (filter->priv->child_model,
                                                &children, &iter))
                gtk_tree_model_filter_update_children (filter,
                                                       new_level,
                                                       FILTER_ELT (f_iter.user_data2));
            }
        }
      i++;
    }
  while (gtk_tree_model_iter_next (filter->priv->child_model, &iter));

  /* The level does not contain any visible nodes.  However, changes in
   * this level might affect the parent node, which can either be visible
   * or invisible.  Therefore, this level can only be removed again,
   * if the parent level has an external reference count of zero.  That is,
   * if this level changes state, no signals are required in the parent
   * level.
   */
  if (empty &&
       (parent_level && parent_level->ext_ref_count == 0))
    {
      gtk_tree_model_filter_free_level (filter, new_level, FALSE, TRUE, FALSE);
      return;
    }

  /* If none of the nodes are visible, we will just pull in the
   * first node of the level.
   */
  if (empty)
    {
      FilterElt *filter_elt;

      filter_elt = filter_elt_new ();
      filter_elt->offset = 0;
      filter_elt->zero_ref_count = 0;
      filter_elt->ref_count = 0;
      filter_elt->ext_ref_count = 0;
      filter_elt->children = NULL;
      filter_elt->visible_siter = NULL;

      if (GTK_TREE_MODEL_FILTER_CACHE_CHILD_ITERS (filter))
        filter_elt->iter = first_node;

      g_sequence_append (new_level->seq, filter_elt);
    }

  /* Keep a reference on the first node of this level.  We need this
   * to make sure that we get all signals for this level.
   */
  f_iter.stamp = filter->priv->stamp;
  f_iter.user_data = new_level;
  f_iter.user_data2 = g_sequence_get (g_sequence_get_begin_iter (new_level->seq));

  gtk_tree_model_filter_real_ref_node (GTK_TREE_MODEL (filter), &f_iter, FALSE);
}

static void
gtk_tree_model_filter_free_level (GtkTreeModelFilter *filter,
                                  FilterLevel        *filter_level,
                                  gboolean            unref_self,
                                  gboolean            unref_parent,
                                  gboolean            unref_external)
{
  GSequenceIter *siter;
  GSequenceIter *end_siter;

  g_assert (filter_level);

  end_siter = g_sequence_get_end_iter (filter_level->seq);
  for (siter = g_sequence_get_begin_iter (filter_level->seq);
       siter != end_siter;
       siter = g_sequence_iter_next (siter))
    {
      FilterElt *elt = g_sequence_get (siter);

      if (elt->children)
        {
          /* If we recurse and unref_self == FALSE, then unref_parent
           * must also be FALSE (otherwise a still unref a node in this
           * level).
           */
          gtk_tree_model_filter_free_level (filter,
                                            FILTER_LEVEL (elt->children),
                                            unref_self,
                                            unref_self == FALSE ? FALSE : unref_parent,
                                            unref_external);
        }

      if (unref_external)
        {
          GtkTreeIter f_iter;

          f_iter.stamp = filter->priv->stamp;
          f_iter.user_data = filter_level;
          f_iter.user_data2 = elt;

          while (elt->ext_ref_count > 0)
            gtk_tree_model_filter_real_unref_node (GTK_TREE_MODEL (filter),
                                                   &f_iter,
                                                   TRUE, unref_self);
        }
    }

  /* Release the reference on the first item.
   */
  if (unref_self)
    {
      GtkTreeIter f_iter;

      f_iter.stamp = filter->priv->stamp;
      f_iter.user_data = filter_level;
      f_iter.user_data2 = g_sequence_get (g_sequence_get_begin_iter (filter_level->seq));

      gtk_tree_model_filter_real_unref_node (GTK_TREE_MODEL (filter),
                                             &f_iter, FALSE, TRUE);
    }

  if (filter_level->ext_ref_count == 0)
    {
      FilterLevel *parent_level = filter_level->parent_level;
      FilterElt *parent_elt = filter_level->parent_elt;

      while (parent_level)
        {
          parent_elt->zero_ref_count--;

          parent_elt = parent_level->parent_elt;
          parent_level = parent_level->parent_level;
        }

      if (filter_level != filter->priv->root)
        filter->priv->zero_ref_count--;
    }

#ifdef MODEL_FILTER_DEBUG
  if (filter_level == filter->priv->root)
    g_assert (filter->priv->zero_ref_count == 0);
#endif

  if (filter_level->parent_elt)
    {
      /* Release reference on parent */
      GtkTreeIter parent_iter;

      parent_iter.stamp = filter->priv->stamp;
      parent_iter.user_data = filter_level->parent_level;
      parent_iter.user_data2 = filter_level->parent_elt;

      gtk_tree_model_filter_real_unref_node (GTK_TREE_MODEL (filter),
                                             &parent_iter, FALSE, unref_parent);

      filter_level->parent_elt->children = NULL;
    }
  else
    filter->priv->root = NULL;

  g_sequence_free (filter_level->seq);
  g_sequence_free (filter_level->visible_seq);
  g_free (filter_level);
}

/* prune_level() is like free_level(), however instead of being fully
 * freed, the level is pruned to a level with only the first node used
 * for monitoring.  For now it is only being called from
 * gtk_tree_model_filter_remove_elt_from_level(), which is the reason
 * this function is lacking a “gboolean unref” argument.
 */
static void
gtk_tree_model_filter_prune_level (GtkTreeModelFilter *filter,
                                   FilterLevel        *level)
{
  GSequenceIter *siter;
  GSequenceIter *end_siter;
  FilterElt *elt;
  GtkTreeIter f_iter;

  /* This function is called when the parent of level became invisible.
   * All external ref counts of the children need to be dropped.
   * All children except the first one can be removed.
   */

  /* Any child levels can be freed */
  end_siter = g_sequence_get_end_iter (level->seq);
  for (siter = g_sequence_get_begin_iter (level->seq);
       siter != end_siter;
       siter = g_sequence_iter_next (siter))
    {
      elt = g_sequence_get (siter);

      if (elt->children)
        gtk_tree_model_filter_free_level (filter,
                                          FILTER_LEVEL (elt->children),
                                          TRUE, TRUE, TRUE);
    }

  /* For the first item, only drop the external references */
  elt = g_sequence_get (g_sequence_get_begin_iter (level->seq));

  f_iter.stamp = filter->priv->stamp;
  f_iter.user_data = level;
  f_iter.user_data2 = elt;

  while (elt->ext_ref_count > 0)
    gtk_tree_model_filter_real_unref_node (GTK_TREE_MODEL (filter),
                                           &f_iter, TRUE, TRUE);

  if (elt->visible_siter)
    {
      g_sequence_remove (elt->visible_siter);
      elt->visible_siter = NULL;
    }

  /* Remove the other elts */
  end_siter = g_sequence_get_end_iter (level->seq);
  siter = g_sequence_get_begin_iter (level->seq);
  siter = g_sequence_iter_next (siter);
  for (; siter != end_siter; siter = g_sequence_iter_next (siter))
    {
      elt = g_sequence_get (siter);

      f_iter.stamp = filter->priv->stamp;
      f_iter.user_data = level;
      f_iter.user_data2 = elt;

      while (elt->ext_ref_count > 0)
        gtk_tree_model_filter_real_unref_node (GTK_TREE_MODEL (filter),
                                               &f_iter, TRUE, TRUE);
      /* In this case, we do remove reference counts we've added ourselves,
       * since the node will be removed from the data structures.
       */
      while (elt->ref_count > 0)
        gtk_tree_model_filter_real_unref_node (GTK_TREE_MODEL (filter),
                                               &f_iter, FALSE, TRUE);

      if (elt->visible_siter)
        {
          g_sequence_remove (elt->visible_siter);
          elt->visible_siter = NULL;
        }
    }

  /* Remove [begin + 1, end] */
  siter = g_sequence_get_begin_iter (level->seq);
  siter = g_sequence_iter_next (siter);

  g_sequence_remove_range (siter, end_siter);

  /* The level must have reached an ext ref count of zero by now, though
   * we only assert on this in debugging mode.
   */
#ifdef MODEL_FILTER_DEBUG
  g_assert (level->ext_ref_count == 0);
#endif
}

static void
gtk_tree_model_filter_level_transfer_first_ref (GtkTreeModelFilter *filter,
                                                FilterLevel        *level,
                                                GSequenceIter      *from_iter,
                                                GSequenceIter      *to_iter)
{
  GtkTreeIter f_iter;

  f_iter.stamp = filter->priv->stamp;
  f_iter.user_data = level;
  f_iter.user_data2 = g_sequence_get (to_iter);

  gtk_tree_model_filter_real_ref_node (GTK_TREE_MODEL (filter),
                                       &f_iter, FALSE);

  f_iter.stamp = filter->priv->stamp;
  f_iter.user_data = level;
  f_iter.user_data2 = g_sequence_get (from_iter);

  gtk_tree_model_filter_real_unref_node (GTK_TREE_MODEL (filter),
                                         &f_iter, FALSE, TRUE);
}

static void
gtk_tree_model_filter_level_transfer_first_ref_with_index (GtkTreeModelFilter *filter,
                                                           FilterLevel        *level,
                                                           int                 from_index,
                                                           int                 to_index)
{
  gtk_tree_model_filter_level_transfer_first_ref (filter, level,
                                                  g_sequence_get_iter_at_pos (level->seq, from_index),
                                                  g_sequence_get_iter_at_pos (level->seq, to_index));
}

/* Creates paths suitable for accessing the child model. */
static GtkTreePath *
gtk_tree_model_filter_elt_get_path (FilterLevel *level,
                                    FilterElt   *elt,
                                    GtkTreePath *root)
{
  FilterLevel *walker = level;
  FilterElt *walker2 = elt;
  GtkTreePath *path;
  GtkTreePath *real_path;

  g_return_val_if_fail (level != NULL, NULL);
  g_return_val_if_fail (elt != NULL, NULL);

  path = gtk_tree_path_new ();

  while (walker)
    {
      gtk_tree_path_prepend_index (path, walker2->offset);

      walker2 = walker->parent_elt;
      walker = walker->parent_level;
    }

  if (root)
    {
      real_path = gtk_tree_model_filter_add_root (path, root);
      gtk_tree_path_free (path);
      return real_path;
    }

  return path;
}

static GtkTreePath *
gtk_tree_model_filter_add_root (GtkTreePath *src,
                                GtkTreePath *root)
{
  GtkTreePath *retval;
  int i;

  retval = gtk_tree_path_copy (root);

  for (i = 0; i < gtk_tree_path_get_depth (src); i++)
    gtk_tree_path_append_index (retval, gtk_tree_path_get_indices (src)[i]);

  return retval;
}

static GtkTreePath *
gtk_tree_model_filter_remove_root (GtkTreePath *src,
                                   GtkTreePath *root)
{
  GtkTreePath *retval;
  int i;
  int depth;
  int *indices;

  if (gtk_tree_path_get_depth (src) <= gtk_tree_path_get_depth (root))
    return NULL;

  depth = gtk_tree_path_get_depth (src);
  indices = gtk_tree_path_get_indices (src);

  for (i = 0; i < gtk_tree_path_get_depth (root); i++)
    if (indices[i] != gtk_tree_path_get_indices (root)[i])
      return NULL;

  retval = gtk_tree_path_new ();

  for (; i < depth; i++)
    gtk_tree_path_append_index (retval, indices[i]);

  return retval;
}

static void
gtk_tree_model_filter_increment_stamp (GtkTreeModelFilter *filter)
{
  do
    {
      filter->priv->stamp++;
    }
  while (filter->priv->stamp == 0);

  gtk_tree_model_filter_clear_cache (filter);
}

static gboolean
gtk_tree_model_filter_real_visible (GtkTreeModelFilter *filter,
                                    GtkTreeModel       *child_model,
                                    GtkTreeIter        *child_iter)
{
  if (filter->priv->visible_func)
    {
      return filter->priv->visible_func (child_model,
					 child_iter,
					 filter->priv->visible_data)
	? TRUE : FALSE;
    }
  else if (filter->priv->visible_column >= 0)
   {
     GValue val = G_VALUE_INIT;

     gtk_tree_model_get_value (child_model, child_iter,
                               filter->priv->visible_column, &val);

     if (g_value_get_boolean (&val))
       {
         g_value_unset (&val);
         return TRUE;
       }

     g_value_unset (&val);
     return FALSE;
   }

  /* no visible function set, so always visible */
  return TRUE;
}

static gboolean
gtk_tree_model_filter_visible (GtkTreeModelFilter *self,
                               GtkTreeIter        *child_iter)
{
  return GTK_TREE_MODEL_FILTER_GET_CLASS (self)->visible (self,
      self->priv->child_model, child_iter);
}

static void
gtk_tree_model_filter_clear_cache_helper_iter (gpointer data,
                                               gpointer user_data)
{
  GtkTreeModelFilter *filter = user_data;
  FilterElt *elt = data;

#ifdef MODEL_FILTER_DEBUG
  g_assert (elt->zero_ref_count >= 0);
#endif

  if (elt->zero_ref_count > 0)
    gtk_tree_model_filter_clear_cache_helper (filter, elt->children);
}

static void
gtk_tree_model_filter_clear_cache_helper (GtkTreeModelFilter *filter,
                                          FilterLevel        *level)
{
  g_assert (level);

  g_sequence_foreach (level->seq, gtk_tree_model_filter_clear_cache_helper_iter, filter);

  /* If the level's ext_ref_count is zero, it means the level is not visible
   * and can be removed.  But, since we support monitoring a child level
   * of a parent for changes (these might affect the parent), we will only
   * free the level if the parent level also has an external ref
   * count of zero.  In that case, changes concerning our parent are
   * not requested.
   *
   * The root level is always visible, so an exception holds for levels
   * with the root level as parent level: these have to remain cached.
   */
  if (level->ext_ref_count == 0 && level != filter->priv->root &&
      level->parent_level && level->parent_level != filter->priv->root &&
      level->parent_level->ext_ref_count == 0)
    {
      gtk_tree_model_filter_free_level (filter, level, TRUE, TRUE, FALSE);
      return;
    }
}

static gboolean
gtk_tree_model_filter_elt_is_visible_in_target (FilterLevel *level,
                                                FilterElt   *elt)
{
  if (!elt->visible_siter)
    return FALSE;

  if (!level->parent_elt)
    return TRUE;

  do
    {
      elt = level->parent_elt;
      level = level->parent_level;

      if (elt && !elt->visible_siter)
        return FALSE;
    }
  while (level);

  return TRUE;
}

/* If a change has occurred in path (inserted, changed or deleted),
 * then this function is used to check all its ancestors.  An ancestor
 * could have changed state as a result and this needs to be propagated
 * to the objects monitoring the filter model.
 */
static void
gtk_tree_model_filter_check_ancestors (GtkTreeModelFilter *filter,
                                       GtkTreePath        *path)
{
  int i = 0;
  int *indices = gtk_tree_path_get_indices (path);
  FilterElt *elt;
  FilterLevel *level;
  GtkTreeIter c_iter, tmp_iter, *root_iter;

  level = FILTER_LEVEL (filter->priv->root);

  if (!level)
    return;

  root_iter = NULL;
  if (filter->priv->virtual_root &&
      gtk_tree_model_get_iter (filter->priv->child_model, &tmp_iter,
                               filter->priv->virtual_root))
    root_iter = &tmp_iter;
  gtk_tree_model_iter_nth_child (filter->priv->child_model, &c_iter,
                                 root_iter,
                                 indices[i]);

  while (i < gtk_tree_path_get_depth (path) - 1)
    {
      gboolean requested_state;

      elt = lookup_elt_with_offset (level->seq,
                                    gtk_tree_path_get_indices (path)[i], NULL);

      requested_state = gtk_tree_model_filter_visible (filter, &c_iter);

      if (!elt)
        {
          int index;
          GtkTreePath *c_path;

          if (requested_state == FALSE)
            return;

          /* The elt does not exist in this level (so it is not
           * visible), but should now be visible.  We emit the
           * row-inserted and row-has-child-toggled signals.
           */
          elt = gtk_tree_model_filter_insert_elt_in_level (filter,
                                                           &c_iter,
                                                           level,
                                                           indices[i],
                                                           &index);

          /* insert_elt_in_level defaults to FALSE */
          elt->visible_siter = g_sequence_insert_sorted (level->visible_seq,
                                                         elt,
                                                         filter_elt_cmp, NULL);

          c_path = gtk_tree_model_get_path (filter->priv->child_model,
                                            &c_iter);

          gtk_tree_model_filter_emit_row_inserted_for_path (filter,
                                                            filter->priv->child_model,
                                                            c_path,
                                                            &c_iter);

          gtk_tree_path_free (c_path);

          /* We can immediately return, because this node was not visible
           * before and its children will be checked for in response to
           * the emitted row-has-child-toggled signal.
           */
          return;
        }
      else if (elt->visible_siter)
        {
          if (!requested_state)
            {
              /* A node has turned invisible.  Remove it from the level
               * and emit row-deleted.  Since this node is being
               * deleted. it makes no sense to look further up the
               * chain.
               */
              gtk_tree_model_filter_remove_elt_from_level (filter,
                                                           level, elt);
              return;
            }

          /* Otherwise continue up the chain */
        }
      else if (!elt->visible_siter)
        {
          if (requested_state)
            {
              /* A node is already in the cache, but invisible.  This
               * is usually a node on which a reference is kept by
               * the filter model, or a node fetched on the filter's
               * request, and thus not shown.  Therefore, we will
               * not emit row-inserted for this node.  Instead,
               * we signal to its parent that a change has occurred.
               *
               * Exception: root level, in this case, we must emit
               * row-inserted.
               */
              if (level->parent_level)
                {
                  GtkTreeIter f_iter;
                  GtkTreePath *f_path;

                  elt->visible_siter = g_sequence_insert_sorted (level->visible_seq, elt,
                                                                 filter_elt_cmp, NULL);

                  f_iter.stamp = filter->priv->stamp;
                  f_iter.user_data = level->parent_level;
                  f_iter.user_data2 = level->parent_elt;

                  f_path = gtk_tree_model_get_path (GTK_TREE_MODEL (filter),
                                                    &f_iter);
                  gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (filter),
                                                        f_path, &f_iter);
                  gtk_tree_path_free (f_path);
                }
              else
                {
                  GtkTreePath *c_path;

                  elt->visible_siter = g_sequence_insert_sorted (level->visible_seq, elt,
                                                                 filter_elt_cmp, NULL);

                  c_path = gtk_tree_model_get_path (filter->priv->child_model,
                                                    &c_iter);

                  gtk_tree_model_filter_emit_row_inserted_for_path (filter,
                                                                    filter->priv->child_model,
                                                                    c_path,
                                                                    &c_iter);

                  gtk_tree_path_free (c_path);
                }

              /* We can immediately return, because this node was not visible
               * before and the parent will check its children, including
               * this node, in response to the emitted row-has-child-toggled
               * signal.
               */
              return;
            }

          /* Not visible, so no need to continue. */
          return;
        }

      if (!elt->children)
        {
          /* If an elt does not have children, these are not visible.
           * Therefore, any signals emitted for these children will
           * be ignored, so we do not have to emit them.
           */
          return;
        }

      level = elt->children;
      i++;

      tmp_iter = c_iter;
      gtk_tree_model_iter_nth_child (filter->priv->child_model, &c_iter,
                                     &tmp_iter, indices[i]);
    }
}

static FilterElt *
gtk_tree_model_filter_insert_elt_in_level (GtkTreeModelFilter *filter,
                                           GtkTreeIter        *c_iter,
                                           FilterLevel        *level,
                                           int                 offset,
                                           int                *index)
{
  FilterElt *elt;
  GSequenceIter *siter;

  elt = filter_elt_new ();

  if (GTK_TREE_MODEL_FILTER_CACHE_CHILD_ITERS (filter))
    elt->iter = *c_iter;

  elt->offset = offset;
  elt->zero_ref_count = 0;
  elt->ref_count = 0;
  elt->ext_ref_count = 0;
  elt->children = NULL;

  /* Because we don't emit row_inserted, the node is invisible and thus
   * not inserted in visible_seq
   */
  elt->visible_siter = NULL;

  siter = g_sequence_insert_sorted (level->seq, elt, filter_elt_cmp, NULL);
  *index = g_sequence_iter_get_position (siter);

  /* If the insert location is zero, we need to move our reference
   * on the old first node to the new first node.
   */
  if (*index == 0)
    gtk_tree_model_filter_level_transfer_first_ref_with_index (filter, level,
                                                               1, 0);

  return elt;
}

static FilterElt *
gtk_tree_model_filter_fetch_child (GtkTreeModelFilter *filter,
                                   FilterLevel        *level,
                                   int                 offset,
                                   int                *index)
{
  int len;
  GtkTreePath *c_path = NULL;
  GtkTreeIter c_iter;
  GtkTreePath *c_parent_path = NULL;
  GtkTreeIter c_parent_iter;

  /* check if child exists and is visible */
  if (level->parent_elt)
    {
      c_parent_path =
        gtk_tree_model_filter_elt_get_path (level->parent_level,
                                            level->parent_elt,
                                            filter->priv->virtual_root);
      if (!c_parent_path)
        return NULL;
    }
  else
    {
      if (filter->priv->virtual_root)
        c_parent_path = gtk_tree_path_copy (filter->priv->virtual_root);
      else
        c_parent_path = NULL;
    }

  if (c_parent_path)
    {
      gtk_tree_model_get_iter (filter->priv->child_model,
                               &c_parent_iter,
                               c_parent_path);
      len = gtk_tree_model_iter_n_children (filter->priv->child_model,
                                            &c_parent_iter);

      c_path = gtk_tree_path_copy (c_parent_path);
      gtk_tree_path_free (c_parent_path);
    }
  else
    {
      len = gtk_tree_model_iter_n_children (filter->priv->child_model, NULL);
      c_path = gtk_tree_path_new ();
    }

  gtk_tree_path_append_index (c_path, offset);
  gtk_tree_model_get_iter (filter->priv->child_model, &c_iter, c_path);
  gtk_tree_path_free (c_path);

  if (offset >= len || !gtk_tree_model_filter_visible (filter, &c_iter))
    return NULL;

  return gtk_tree_model_filter_insert_elt_in_level (filter, &c_iter,
                                                    level, offset,
                                                    index);
}

/* Note that this function is never called from the row-deleted handler.
 * This means that this function is only used for removing elements
 * which are still present in the child model.  As a result, we must
 * take care to properly release the references the filter model has
 * on the child model nodes.
 */
static void
gtk_tree_model_filter_remove_elt_from_level (GtkTreeModelFilter *filter,
                                             FilterLevel        *level,
                                             FilterElt          *elt)
{
  FilterElt *parent;
  FilterLevel *parent_level;
  int length, orig_level_ext_ref_count;
  GtkTreeIter iter;
  GtkTreePath *path = NULL;

  gboolean emit_child_toggled = FALSE;

  /* We need to know about the level's ext ref count before removal
   * of this node.
   */
  orig_level_ext_ref_count = level->ext_ref_count;

  iter.stamp = filter->priv->stamp;
  iter.user_data = level;
  iter.user_data2 = elt;

  parent = level->parent_elt;
  parent_level = level->parent_level;

  if (!parent || orig_level_ext_ref_count > 0)
    path = gtk_tree_model_get_path (GTK_TREE_MODEL (filter), &iter);
  else
    /* If the level is not visible, the parent is potentially invisible
     * too.  Either way, as no signal will be emitted, there is no use
     * for a path.
     */
    path = NULL;

  length = g_sequence_get_length (level->seq);

  /* first register the node to be invisible */
  g_sequence_remove (elt->visible_siter);
  elt->visible_siter = NULL;

  /*
   * If level != root level and the number of visible nodes is 0 (ie. this
   * is the last node to be removed from the level), emit
   * row-has-child-toggled.
   */

  if (level != filter->priv->root
      && g_sequence_get_length (level->visible_seq) == 0
      && parent
      && parent->visible_siter)
    emit_child_toggled = TRUE;

  /* Distinguish:
   *   - length > 1: in this case, the node is removed from the level
   *                 and row-deleted is emitted.
   *   - length == 1: in this case, we need to decide whether to keep
   *                  the level or to free it.
   */
  if (length > 1)
    {
      GSequenceIter *siter;

      /* We emit row-deleted, and remove the node from the cache.
       * If it has any children, these will be removed here as well.
       */

      /* FIXME: I am not 100% sure it is always save to fully free the
       * level here.  Perhaps the state of the parent level, etc. has to
       * be checked to make the right decision, like is done below for
       * the case length == 1.
       */
      if (elt->children)
        gtk_tree_model_filter_free_level (filter, elt->children, TRUE, TRUE, TRUE);

      /* If the first node is being removed, transfer, the reference */
      if (elt == g_sequence_get (g_sequence_get_begin_iter (level->seq)))
        {
          gtk_tree_model_filter_level_transfer_first_ref_with_index (filter, level,
                                                                     0, 1);
        }

      while (elt->ext_ref_count > 0)
        gtk_tree_model_filter_real_unref_node (GTK_TREE_MODEL (filter),
                                               &iter, TRUE, TRUE);
      /* In this case, we do remove reference counts we've added ourselves,
       * since the node will be removed from the data structures.
       */
      while (elt->ref_count > 0)
        gtk_tree_model_filter_real_unref_node (GTK_TREE_MODEL (filter),
                                               &iter, FALSE, TRUE);

      /* remove the node */
      lookup_elt_with_offset (level->seq, elt->offset, &siter);
      g_sequence_remove (siter);

      gtk_tree_model_filter_increment_stamp (filter);

      /* Only if the node is in the root level (parent == NULL) or
       * the level is visible, a row-deleted signal is necessary.
       */
      if (!parent || orig_level_ext_ref_count > 0)
        gtk_tree_model_row_deleted (GTK_TREE_MODEL (filter), path);
    }
  else
    {
      /* There is only one node left in this level */
#ifdef MODEL_FILTER_DEBUG
      g_assert (length == 1);
#endif

      /* The row is signalled as deleted to the client.  We have to
       * drop the remaining external reference count here, the client
       * will not do it.
       *
       * We keep the reference counts we've obtained ourselves.
       */
      while (elt->ext_ref_count > 0)
        gtk_tree_model_filter_real_unref_node (GTK_TREE_MODEL (filter),
                                               &iter, TRUE, TRUE);

      /* This level is still required if:
       * - it is the root level
       * - its parent level is the root level
       * - its parent level has an external ref count > 0
       */
      if (! (level == filter->priv->root ||
             level->parent_level == filter->priv->root ||
             level->parent_level->ext_ref_count > 0))
        {
          /* Otherwise, the level can be removed */
          gtk_tree_model_filter_free_level (filter, level, TRUE, TRUE, TRUE);
        }
      else
        {
          /* Level is kept, but we turn our attention to a child level.
           *
           * If level is not the root level, it is a child level with
           * an ext ref count that is now 0.  That means that any child level
           * of elt can be removed.
           */
          if (level != filter->priv->root)
            {
#ifdef MODEL_FILTER_DEBUG
              g_assert (level->ext_ref_count == 0);
#endif
              if (elt->children)
                gtk_tree_model_filter_free_level (filter, elt->children,
                                                  TRUE, TRUE, TRUE);
            }
          else
            {
              /* In this case, we want to keep the level with the first
               * node pulled in to monitor for signals.
               */
              if (elt->children)
                gtk_tree_model_filter_prune_level (filter, elt->children);
            }
        }

      if (!parent || orig_level_ext_ref_count > 0)
        gtk_tree_model_row_deleted (GTK_TREE_MODEL (filter), path);
    }

  gtk_tree_path_free (path);

  if (emit_child_toggled && parent->ext_ref_count > 0)
    {
      GtkTreeIter piter;
      GtkTreePath *ppath;

      piter.stamp = filter->priv->stamp;
      piter.user_data = parent_level;
      piter.user_data2 = parent;

      ppath = gtk_tree_model_get_path (GTK_TREE_MODEL (filter), &piter);

      gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (filter),
                                            ppath, &piter);
      gtk_tree_path_free (ppath);
    }
}

/* This function is called after the given node has become visible.
 * When the node has children, we should build the level and
 * take a reference on the first child.
 */
static void
gtk_tree_model_filter_update_children (GtkTreeModelFilter *filter,
				       FilterLevel        *level,
				       FilterElt          *elt)
{
  GtkTreeIter c_iter;
  GtkTreeIter iter;

  if (!elt->visible_siter)
    return;

  iter.stamp = filter->priv->stamp;
  iter.user_data = level;
  iter.user_data2 = elt;

  gtk_tree_model_filter_convert_iter_to_child_iter (filter, &c_iter, &iter);

  if ((!level->parent_level || level->parent_level->ext_ref_count > 0) &&
      gtk_tree_model_iter_has_child (filter->priv->child_model, &c_iter))
    {
      if (!elt->children)
        gtk_tree_model_filter_build_level (filter, level, elt, FALSE);

      if (elt->ext_ref_count > 0 && elt->children &&
          g_sequence_get_length (elt->children->seq))
        {
          GtkTreePath *path;
          path = gtk_tree_model_get_path (GTK_TREE_MODEL (filter), &iter);
          gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (filter),
                                                path,
                                                &iter);
          if (path)
            gtk_tree_path_free (path);
        }
    }
}

/* Path is relative to the child model (this is on search on elt offset)
 * but with the virtual root already removed if necesssary.
 */
static gboolean
find_elt_with_offset (GtkTreeModelFilter *filter,
                      GtkTreePath        *path,
                      FilterLevel       **level_,
                      FilterElt         **elt_)
{
  int i = 0;
  FilterLevel *level;
  FilterLevel *parent_level = NULL;
  FilterElt *elt = NULL;

  level = FILTER_LEVEL (filter->priv->root);

  while (i < gtk_tree_path_get_depth (path))
    {
      if (!level)
        return FALSE;

      elt = lookup_elt_with_offset (level->seq,
                                    gtk_tree_path_get_indices (path)[i],
                                    NULL);

      if (!elt)
        return FALSE;

      parent_level = level;
      level = elt->children;
      i++;
    }

  if (level_)
    *level_ = parent_level;

  if (elt_)
    *elt_ = elt;

  return TRUE;
}

/* TreeModel signals */
static void
gtk_tree_model_filter_emit_row_inserted_for_path (GtkTreeModelFilter *filter,
                                                  GtkTreeModel       *c_model,
                                                  GtkTreePath        *c_path,
                                                  GtkTreeIter        *c_iter)
{
  FilterLevel *level;
  FilterElt *elt;
  GtkTreePath *path;
  GtkTreeIter iter, children;
  gboolean signals_emitted = FALSE;

  if (!filter->priv->root)
    {
      /* The root level has not been exposed to the view yet, so we
       * need to emit signals for any node that is being inserted.
       */
      gtk_tree_model_filter_build_level (filter, NULL, NULL, TRUE);

      /* Check if the root level was built.  Then child levels
       * that matter have also been built (due to update_children,
       * which triggers iter_n_children).
       */
      if (filter->priv->root &&
          g_sequence_get_length (FILTER_LEVEL (filter->priv->root)->visible_seq) > 0)
        signals_emitted = TRUE;
    }

  gtk_tree_model_filter_increment_stamp (filter);

  /* We need to disallow to build new levels, because we are then pulling
   * in a child in an invisible level.  We only want to find path if it
   * is in a visible level (and thus has a parent that is visible).
   */
  path = gtk_real_tree_model_filter_convert_child_path_to_path (filter,
                                                                c_path,
                                                                FALSE,
                                                                TRUE);

  if (!path)
    /* parent is probably being filtered out */
    return;

  gtk_tree_model_filter_get_iter_full (GTK_TREE_MODEL (filter), &iter, path);

  level = FILTER_LEVEL (iter.user_data);
  elt = FILTER_ELT (iter.user_data2);

  /* Make sure elt is visible.  elt can already be visible in case
   * it was pulled in above, so avoid inserted it into visible_seq twice.
   */
  if (!elt->visible_siter)
    {
      elt->visible_siter = g_sequence_insert_sorted (level->visible_seq,
                                                     elt, filter_elt_cmp,
                                                     NULL);
    }

  /* Check whether the node and all of its parents are visible */
  if (gtk_tree_model_filter_elt_is_visible_in_target (level, elt))
    {
      /* visibility changed -- reget path */
      gtk_tree_path_free (path);
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (filter), &iter);

      if (!signals_emitted &&
          (!level->parent_level || level->ext_ref_count > 0))
        gtk_tree_model_row_inserted (GTK_TREE_MODEL (filter), path, &iter);

      if (level->parent_level && level->parent_elt->ext_ref_count > 0 &&
          g_sequence_get_length (level->visible_seq) == 1)
        {
          /* We know that this is the first visible node in this level, so
           * we need to emit row-has-child-toggled on the parent.  This
           * does not apply to the root level.
           */

          gtk_tree_path_up (path);
          gtk_tree_model_get_iter (GTK_TREE_MODEL (filter), &iter, path);

          gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (filter),
                                                path,
                                                &iter);
        }

      if (!signals_emitted
          && gtk_tree_model_iter_children (c_model, &children, c_iter))
        gtk_tree_model_filter_update_children (filter, level, elt);
    }

  gtk_tree_path_free (path);
}

static void
gtk_tree_model_filter_row_changed (GtkTreeModel *c_model,
                                   GtkTreePath  *c_path,
                                   GtkTreeIter  *c_iter,
                                   gpointer      data)
{
  GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (data);
  GtkTreeIter iter;
  GtkTreeIter children;
  GtkTreeIter real_c_iter;
  GtkTreePath *path = NULL;
  GtkTreePath *real_path = NULL;

  FilterElt *elt;
  FilterLevel *level;

  gboolean requested_state;
  gboolean current_state;
  gboolean free_c_path = FALSE;

  g_return_if_fail (c_path != NULL || c_iter != NULL);

  if (!c_path)
    {
      c_path = gtk_tree_model_get_path (c_model, c_iter);
      free_c_path = TRUE;
    }

  if (filter->priv->virtual_root)
    real_path = gtk_tree_model_filter_remove_root (c_path,
                                                   filter->priv->virtual_root);
  else
    real_path = gtk_tree_path_copy (c_path);

  if (c_iter)
    real_c_iter = *c_iter;
  else
    gtk_tree_model_get_iter (c_model, &real_c_iter, c_path);

  /* is this node above the virtual root? */
  if (filter->priv->virtual_root &&
      (gtk_tree_path_get_depth (filter->priv->virtual_root)
          >= gtk_tree_path_get_depth (c_path)))
    goto done;

  /* what's the requested state? */
  requested_state = gtk_tree_model_filter_visible (filter, &real_c_iter);

  /* now, let's see whether the item is there */
  path = gtk_real_tree_model_filter_convert_child_path_to_path (filter,
                                                                c_path,
                                                                FALSE,
                                                                FALSE);

  if (path)
    {
      gtk_tree_model_filter_get_iter_full (GTK_TREE_MODEL (filter),
                                           &iter, path);
      current_state = FILTER_ELT (iter.user_data2)->visible_siter != NULL;
    }
  else
    current_state = FALSE;

  if (current_state == FALSE && requested_state == FALSE)
    /* no changes required */
    goto done;

  if (current_state == TRUE && requested_state == FALSE)
    {
      gtk_tree_model_filter_remove_elt_from_level (filter,
                                                   FILTER_LEVEL (iter.user_data),
                                                   FILTER_ELT (iter.user_data2));

      if (real_path)
        gtk_tree_model_filter_check_ancestors (filter, real_path);

      goto done;
    }

  if (current_state == TRUE && requested_state == TRUE)
    {
      level = FILTER_LEVEL (iter.user_data);
      elt = FILTER_ELT (iter.user_data2);

      if (gtk_tree_model_filter_elt_is_visible_in_target (level, elt))
        {
          /* propagate the signal; also get a path taking only visible
           * nodes into account.
           */
          gtk_tree_path_free (path);
          path = gtk_tree_model_get_path (GTK_TREE_MODEL (filter), &iter);

          if (level->ext_ref_count > 0)
            gtk_tree_model_row_changed (GTK_TREE_MODEL (filter), path, &iter);

          /* and update the children */
          if (gtk_tree_model_iter_children (c_model, &children, &real_c_iter))
            gtk_tree_model_filter_update_children (filter, level, elt);
        }

      if (real_path)
        gtk_tree_model_filter_check_ancestors (filter, real_path);

      goto done;
    }

  /* only current == FALSE and requested == TRUE is left,
   * pull in the child
   */
  g_return_if_fail (current_state == FALSE && requested_state == TRUE);

  if (real_path)
    gtk_tree_model_filter_check_ancestors (filter, real_path);

  gtk_tree_model_filter_emit_row_inserted_for_path (filter, c_model,
                                                    c_path, c_iter);

done:
  if (path)
    gtk_tree_path_free (path);

  if (real_path)
    gtk_tree_path_free (real_path);

  if (free_c_path)
    gtk_tree_path_free (c_path);
}

static void
gtk_tree_model_filter_row_inserted (GtkTreeModel *c_model,
                                    GtkTreePath  *c_path,
                                    GtkTreeIter  *c_iter,
                                    gpointer      data)
{
  GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (data);
  GtkTreePath *real_path = NULL;

  GtkTreeIter real_c_iter;

  FilterElt *elt = NULL;
  FilterLevel *level = NULL;
  FilterLevel *parent_level = NULL;
  GSequenceIter *siter;
  FilterElt dummy;

  int i = 0, offset;

  gboolean free_c_path = FALSE;
  gboolean emit_row_inserted = FALSE;

  g_return_if_fail (c_path != NULL || c_iter != NULL);

  if (!c_path)
    {
      c_path = gtk_tree_model_get_path (c_model, c_iter);
      free_c_path = TRUE;
    }

  if (c_iter)
    real_c_iter = *c_iter;
  else
    gtk_tree_model_get_iter (c_model, &real_c_iter, c_path);

  /* the row has already been inserted. so we need to fixup the
   * virtual root here first
   */
  if (filter->priv->virtual_root)
    {
      if (gtk_tree_path_get_depth (filter->priv->virtual_root) >=
          gtk_tree_path_get_depth (c_path))
        {
          int depth;
          int *v_indices, *c_indices;
          gboolean common_prefix = TRUE;

          depth = gtk_tree_path_get_depth (c_path) - 1;
          v_indices = gtk_tree_path_get_indices (filter->priv->virtual_root);
          c_indices = gtk_tree_path_get_indices (c_path);

          for (i = 0; i < depth; i++)
            if (v_indices[i] != c_indices[i])
              {
                common_prefix = FALSE;
                break;
              }

          if (common_prefix && v_indices[depth] >= c_indices[depth])
            (v_indices[depth])++;
        }
    }

  /* subtract virtual root if necessary */
  if (filter->priv->virtual_root)
    {
      real_path = gtk_tree_model_filter_remove_root (c_path,
                                                     filter->priv->virtual_root);
      /* not our child */
      if (!real_path)
        goto done;
    }
  else
    real_path = gtk_tree_path_copy (c_path);

  if (!filter->priv->root)
    {
      /* The root level has not been exposed to the view yet, so we
       * need to emit signals for any node that is being inserted.
       */
      gtk_tree_model_filter_build_level (filter, NULL, NULL, TRUE);

      /* Check if the root level was built.  Then child levels
       * that matter have also been built (due to update_children,
       * which triggers iter_n_children).
       */
      if (filter->priv->root)
        {
          emit_row_inserted = FALSE;
          goto done;
        }
    }

  if (gtk_tree_path_get_depth (real_path) - 1 >= 1)
    {
      gboolean found = FALSE;
      GtkTreePath *parent = gtk_tree_path_copy (real_path);
      gtk_tree_path_up (parent);

      found = find_elt_with_offset (filter, parent, &parent_level, &elt);

      gtk_tree_path_free (parent);

      if (!found)
        /* Parent is not in the cache and probably being filtered out */
        goto done;

      level = elt->children;
    }
  else
    level = FILTER_LEVEL (filter->priv->root);

  if (!level)
    {
      if (elt && elt->visible_siter)
        {
          /* The level in which the new node should be inserted does not
           * exist, but the parent, elt, does.  If elt is visible, emit
           * row-has-child-toggled.
           */
          GtkTreePath *tmppath;
          GtkTreeIter  tmpiter;

          tmpiter.stamp = filter->priv->stamp;
          tmpiter.user_data = parent_level;
          tmpiter.user_data2 = elt;

          tmppath = gtk_tree_model_get_path (GTK_TREE_MODEL (filter),
                                             &tmpiter);

          if (tmppath)
            {
              gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (filter),
                                                    tmppath, &tmpiter);
              gtk_tree_path_free (tmppath);
            }
        }
      goto done;
    }

  /* let's try to insert the value */
  offset = gtk_tree_path_get_indices (real_path)[gtk_tree_path_get_depth (real_path) - 1];

  /* update the offsets, yes if we didn't insert the node above, there will
   * be a gap here. This will be filled with the node (via fetch_child) when
   * it becomes visible
   */
  dummy.offset = offset;
  siter = g_sequence_search (level->seq, &dummy, filter_elt_cmp, NULL);
  siter = g_sequence_iter_prev (siter);
  g_sequence_foreach_range (siter, g_sequence_get_end_iter (level->seq),
                            increase_offset_iter, GINT_TO_POINTER (offset));

  /* only insert when visible */
  if (gtk_tree_model_filter_visible (filter, &real_c_iter))
    {
      FilterElt *felt;

      felt = gtk_tree_model_filter_insert_elt_in_level (filter,
                                                        &real_c_iter,
                                                        level, offset,
                                                        &i);

      /* insert_elt_in_level defaults to FALSE */
      felt->visible_siter = g_sequence_insert_sorted (level->visible_seq,
                                                      felt,
                                                      filter_elt_cmp, NULL);
      emit_row_inserted = TRUE;
    }

done:
  if (real_path)
    gtk_tree_model_filter_check_ancestors (filter, real_path);

  if (emit_row_inserted)
    gtk_tree_model_filter_emit_row_inserted_for_path (filter, c_model,
                                                      c_path, c_iter);

  if (real_path)
    gtk_tree_path_free (real_path);

  if (free_c_path)
    gtk_tree_path_free (c_path);
}

static void
gtk_tree_model_filter_row_has_child_toggled (GtkTreeModel *c_model,
                                             GtkTreePath  *c_path,
                                             GtkTreeIter  *c_iter,
                                             gpointer      data)
{
  GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (data);
  GtkTreePath *path;
  GtkTreeIter iter;
  FilterLevel *level;
  FilterElt *elt;
  gboolean requested_state;

  g_return_if_fail (c_path != NULL && c_iter != NULL);

  /* If we get row-has-child-toggled on the virtual root, and there is
   * no root level; try to build it now.
   */
  if (filter->priv->virtual_root && !filter->priv->root
      && !gtk_tree_path_compare (c_path, filter->priv->virtual_root))
    {
      gtk_tree_model_filter_build_level (filter, NULL, NULL, TRUE);
      return;
    }

  /* For all other levels, there is a chance that the visibility state
   * of the parent has changed now.
   */

  path = gtk_real_tree_model_filter_convert_child_path_to_path (filter,
                                                                c_path,
                                                                FALSE,
                                                                TRUE);
  if (!path)
    return;

  gtk_tree_model_filter_get_iter_full (GTK_TREE_MODEL (data), &iter, path);

  level = FILTER_LEVEL (iter.user_data);
  elt = FILTER_ELT (iter.user_data2);

  gtk_tree_path_free (path);

  requested_state = gtk_tree_model_filter_visible (filter, c_iter);

  if (!elt->visible_siter && !requested_state)
    {
      /* The parent node currently is not visible and will not become
       * visible, so we will not pass on the row-has-child-toggled event.
       */
      return;
    }
  else if (elt->visible_siter && !requested_state)
    {
      /* The node is no longer visible, so it has to be removed.
       * _remove_elt_from_level() takes care of emitting row-has-child-toggled
       * when required.
       */
      gtk_tree_model_filter_remove_elt_from_level (filter, level, elt);

      return;
    }
  else if (!elt->visible_siter && requested_state)
    {
      elt->visible_siter = g_sequence_insert_sorted (level->visible_seq,
                                                     elt, filter_elt_cmp,
                                                     NULL);

      /* Only insert if the parent is visible in the target */
      if (gtk_tree_model_filter_elt_is_visible_in_target (level, elt))
        {
          path = gtk_tree_model_get_path (GTK_TREE_MODEL (filter), &iter);
          gtk_tree_model_row_inserted (GTK_TREE_MODEL (filter), path, &iter);
          gtk_tree_path_free (path);

          /* We do not update children now, because that will happen
           * below.
           */
        }
    }
  /* For the remaining possibility, elt->visible && requested_state
   * no action is required.
   */

  /* If this node is referenced and has children, build the level so we
   * can monitor it for changes.
   */
  if (elt->ref_count > 1 && !elt->children &&
      gtk_tree_model_iter_has_child (c_model, c_iter))
    gtk_tree_model_filter_build_level (filter, level, elt, FALSE);

  /* get a path taking only visible nodes into account */
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (data), &iter);
  gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (data), path, &iter);
  gtk_tree_path_free (path);
}

static void
gtk_tree_model_filter_virtual_root_deleted (GtkTreeModelFilter *filter,
                                            GtkTreePath        *c_path)
{
  int i, nodes;
  GtkTreePath *path;
  FilterLevel *level = FILTER_LEVEL (filter->priv->root);

  /* The virtual root (or one of its ancestors) has been deleted.  This
   * means that all content for our model is now gone.  We deal with
   * this by removing everything in the filter model: we just iterate
   * over the root level and emit a row-deleted for each FilterElt.
   * (FIXME: Should we emit row-deleted for child nodes as well? This
   * has never been fully clear in TreeModel).
   */

  /* We unref the path of the virtual root, up to and not including the
   * deleted node which can no longer be unreffed.
   */
  gtk_tree_model_filter_unref_path (filter, filter->priv->virtual_root,
                                    gtk_tree_path_get_depth (c_path) - 1);
  filter->priv->virtual_root_deleted = TRUE;

  if (!level)
    return;

  nodes = g_sequence_get_length (level->visible_seq);

  /* We should not propagate the unref here.  An unref for any of these
   * nodes will fail, since the respective nodes in the child model are
   * no longer there.
   */
  gtk_tree_model_filter_free_level (filter, filter->priv->root, FALSE, TRUE, FALSE);

  gtk_tree_model_filter_increment_stamp (filter);

  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, 0);

  for (i = 0; i < nodes; i++)
    gtk_tree_model_row_deleted (GTK_TREE_MODEL (filter), path);

  gtk_tree_path_free (path);
}

static void
gtk_tree_model_filter_adjust_virtual_root (GtkTreeModelFilter *filter,
                                           GtkTreePath        *c_path)
{
  int i;
  int level;
  int *v_indices, *c_indices;
  gboolean common_prefix = TRUE;

  level = gtk_tree_path_get_depth (c_path) - 1;
  v_indices = gtk_tree_path_get_indices (filter->priv->virtual_root);
  c_indices = gtk_tree_path_get_indices (c_path);

  for (i = 0; i < level; i++)
    if (v_indices[i] != c_indices[i])
      {
        common_prefix = FALSE;
        break;
      }

  if (common_prefix && v_indices[level] > c_indices[level])
    (v_indices[level])--;
}

static void
gtk_tree_model_filter_row_deleted_invisible_node (GtkTreeModelFilter *filter,
                                                  GtkTreePath        *c_path)
{
  int offset;
  GtkTreePath *real_path;
  FilterLevel *level;
  FilterElt *elt;
  FilterElt dummy;
  GSequenceIter *siter;

  /* The node deleted in the child model is not visible in the
   * filter model.  We will not emit a signal, just fixup the offsets
   * of the other nodes.
   */

  if (!filter->priv->root)
    return;

  level = FILTER_LEVEL (filter->priv->root);

  /* subtract vroot if necessary */
  if (filter->priv->virtual_root)
    {
      real_path = gtk_tree_model_filter_remove_root (c_path,
                                                     filter->priv->virtual_root);
      /* we don't handle this */
      if (!real_path)
        return;
    }
  else
    real_path = gtk_tree_path_copy (c_path);

  if (gtk_tree_path_get_depth (real_path) - 1 >= 1)
    {
      gboolean found = FALSE;
      GtkTreePath *parent = gtk_tree_path_copy (real_path);
      gtk_tree_path_up (parent);

      found = find_elt_with_offset (filter, parent, &level, &elt);

      gtk_tree_path_free (parent);

      if (!found)
        {
          /* parent is filtered out, so no level */
          gtk_tree_path_free (real_path);
          return;
        }

      level = elt->children;
    }

  offset = gtk_tree_path_get_indices (real_path)[gtk_tree_path_get_depth (real_path) - 1];
  gtk_tree_path_free (real_path);

  if (!level)
    return;

  /* decrease offset of all nodes following the deleted node */
  dummy.offset = offset;
  siter = g_sequence_search (level->seq, &dummy, filter_elt_cmp, NULL);
  g_sequence_foreach_range (siter, g_sequence_get_end_iter (level->seq),
                            decrease_offset_iter, GINT_TO_POINTER (offset));
}

static void
gtk_tree_model_filter_row_deleted (GtkTreeModel *c_model,
                                   GtkTreePath  *c_path,
                                   gpointer      data)
{
  GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (data);
  GtkTreePath *path;
  GtkTreeIter iter;
  FilterElt *elt, *parent_elt = NULL;
  FilterLevel *level, *parent_level = NULL;
  GSequenceIter *siter;
  gboolean emit_child_toggled = FALSE;
  gboolean emit_row_deleted = FALSE;
  int offset;
  int orig_level_ext_ref_count;

  g_return_if_fail (c_path != NULL);

  /* special case the deletion of an ancestor of the virtual root */
  if (filter->priv->virtual_root &&
      (gtk_tree_path_is_ancestor (c_path, filter->priv->virtual_root) ||
       !gtk_tree_path_compare (c_path, filter->priv->virtual_root)))
    {
      gtk_tree_model_filter_virtual_root_deleted (filter, c_path);
      return;
    }

  /* adjust the virtual root for the deleted row */
  if (filter->priv->virtual_root &&
      gtk_tree_path_get_depth (filter->priv->virtual_root) >=
      gtk_tree_path_get_depth (c_path))
    gtk_tree_model_filter_adjust_virtual_root (filter, c_path);

  path = gtk_real_tree_model_filter_convert_child_path_to_path (filter,
                                                                c_path,
                                                                FALSE,
                                                                FALSE);

  if (!path)
    {
      gtk_tree_model_filter_row_deleted_invisible_node (filter, c_path);
      return;
    }

  /* a node was deleted, which was in our cache */
  gtk_tree_model_filter_get_iter_full (GTK_TREE_MODEL (data), &iter, path);

  level = FILTER_LEVEL (iter.user_data);
  elt = FILTER_ELT (iter.user_data2);
  offset = elt->offset;
  orig_level_ext_ref_count = level->ext_ref_count;

  if (elt->visible_siter)
    {
      /* get a path taking only visible nodes into account */
      gtk_tree_path_free (path);
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (filter), &iter);

      if (g_sequence_get_length (level->visible_seq) == 1)
        {
          emit_child_toggled = TRUE;
          parent_level = level->parent_level;
          parent_elt = level->parent_elt;
        }

      emit_row_deleted = TRUE;
    }

  /* Release the references on this node, without propagation because
   * the node does not exist anymore in the child model.  The filter
   * model's references on the node in case of level->parent or use
   * of a virtual root are automatically destroyed by the child model.
   */
  while (elt->ext_ref_count > 0)
    gtk_tree_model_filter_real_unref_node (GTK_TREE_MODEL (data), &iter,
                                           TRUE, FALSE);

  if (elt->children)
    /* If this last node has children, then the recursion in free_level
     * will release this reference.
     */
    while (elt->ref_count > 1)
      gtk_tree_model_filter_real_unref_node (GTK_TREE_MODEL (data), &iter,
                                             FALSE, FALSE);
  else
    while (elt->ref_count > 0)
      gtk_tree_model_filter_real_unref_node (GTK_TREE_MODEL (data), &iter,
                                             FALSE, FALSE);


  if (g_sequence_get_length (level->seq) == 1)
    {
      /* kill level */
      gtk_tree_model_filter_free_level (filter, level, FALSE, TRUE, FALSE);
    }
  else
    {
      GSequenceIter *tmp;
      gboolean is_first;

      lookup_elt_with_offset (level->seq, elt->offset, &siter);
      is_first = g_sequence_get_begin_iter (level->seq) == siter;

      if (elt->children)
        gtk_tree_model_filter_free_level (filter, elt->children,
                                          FALSE, FALSE, FALSE);

      /* remove the row */
      if (elt->visible_siter)
        g_sequence_remove (elt->visible_siter);
      tmp = g_sequence_iter_next (siter);
      g_sequence_remove (siter);
      g_sequence_foreach_range (tmp, g_sequence_get_end_iter (level->seq),
                                decrease_offset_iter, GINT_TO_POINTER (offset));

      /* Take a reference on the new first node.  The first node previously
       * keeping this reference has been removed above.
       */
      if (is_first)
        {
          GtkTreeIter f_iter;

          f_iter.stamp = filter->priv->stamp;
          f_iter.user_data = level;
          f_iter.user_data2 = g_sequence_get (g_sequence_get_begin_iter (level->seq));

          gtk_tree_model_filter_real_ref_node (GTK_TREE_MODEL (filter),
                                               &f_iter, FALSE);
        }
    }

  if (emit_row_deleted)
    {
      /* emit row_deleted */
      gtk_tree_model_filter_increment_stamp (filter);

      if (!parent_elt || orig_level_ext_ref_count > 0)
        gtk_tree_model_row_deleted (GTK_TREE_MODEL (data), path);
    }

  if (emit_child_toggled && parent_level)
    {
      GtkTreeIter iter2;
      GtkTreePath *path2;

      iter2.stamp = filter->priv->stamp;
      iter2.user_data = parent_level;
      iter2.user_data2 = parent_elt;

      /* We set in_row_deleted to TRUE to avoid a level build triggered
       * by row-has-child-toggled (parent model could call iter_has_child
       * for example).
       */
      filter->priv->in_row_deleted = TRUE;
      path2 = gtk_tree_model_get_path (GTK_TREE_MODEL (filter), &iter2);
      gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (filter),
                                            path2, &iter2);
      gtk_tree_path_free (path2);
      filter->priv->in_row_deleted = FALSE;
    }

  if (filter->priv->virtual_root)
    {
      GtkTreePath *real_path;

      real_path = gtk_tree_model_filter_remove_root (c_path,
                                                     filter->priv->virtual_root);
      if (real_path)
        {
          gtk_tree_model_filter_check_ancestors (filter, real_path);
          gtk_tree_path_free (real_path);
        }
    }
  else
    gtk_tree_model_filter_check_ancestors (filter, c_path);

  gtk_tree_path_free (path);
}

static void
gtk_tree_model_filter_rows_reordered (GtkTreeModel *c_model,
                                      GtkTreePath  *c_path,
                                      GtkTreeIter  *c_iter,
                                      int          *new_order,
                                      gpointer      data)
{
  FilterElt *elt;
  FilterLevel *level;
  GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (data);

  GtkTreePath *path;
  GtkTreeIter iter;

  GSequence *tmp_seq;
  GSequenceIter *tmp_end_iter;
  GSequenceIter *old_first_siter = NULL;
  int *tmp_array;
  int i, elt_count;
  int length;

  g_return_if_fail (new_order != NULL);

  if (c_path == NULL || gtk_tree_path_get_depth (c_path) == 0)
    {
      length = gtk_tree_model_iter_n_children (c_model, NULL);

      if (filter->priv->virtual_root)
        {
          int new_pos = -1;

          /* reorder root level of path */
          for (i = 0; i < length; i++)
            if (new_order[i] == gtk_tree_path_get_indices (filter->priv->virtual_root)[0])
              new_pos = i;

          if (new_pos < 0)
            return;

          gtk_tree_path_get_indices (filter->priv->virtual_root)[0] = new_pos;
          return;
        }

      path = gtk_tree_path_new ();
      level = FILTER_LEVEL (filter->priv->root);
    }
  else
    {
      GtkTreeIter child_iter;

      /* virtual root anchor reordering */
      if (filter->priv->virtual_root &&
	  gtk_tree_path_is_ancestor (c_path, filter->priv->virtual_root))
        {
          int new_pos = -1;
          int len;
          int depth;
          GtkTreeIter real_c_iter;

          depth = gtk_tree_path_get_depth (c_path);

          if (c_iter)
            real_c_iter = *c_iter;
          else
            gtk_tree_model_get_iter (c_model, &real_c_iter, c_path);

          len = gtk_tree_model_iter_n_children (c_model, &real_c_iter);

          for (i = 0; i < len; i++)
            if (new_order[i] == gtk_tree_path_get_indices (filter->priv->virtual_root)[depth])
              new_pos = i;

          if (new_pos < 0)
            return;

          gtk_tree_path_get_indices (filter->priv->virtual_root)[depth] = new_pos;
          return;
        }

      path = gtk_real_tree_model_filter_convert_child_path_to_path (filter,
                                                                    c_path,
                                                                    FALSE,
                                                                    FALSE);

      if (!path && filter->priv->virtual_root &&
          gtk_tree_path_compare (c_path, filter->priv->virtual_root))
        return;

      if (!path && !filter->priv->virtual_root)
        return;

      if (!path)
        {
          /* root level mode */
          if (!c_iter)
            gtk_tree_model_get_iter (c_model, c_iter, c_path);
          length = gtk_tree_model_iter_n_children (c_model, c_iter);
          path = gtk_tree_path_new ();
          level = FILTER_LEVEL (filter->priv->root);
        }
      else
        {
          gtk_tree_model_filter_get_iter_full (GTK_TREE_MODEL (data),
                                               &iter, path);

          elt = FILTER_ELT (iter.user_data2);

          if (!elt->children)
            {
              gtk_tree_path_free (path);
              return;
            }

          level = elt->children;

          gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (filter), &child_iter, &iter);
          length = gtk_tree_model_iter_n_children (c_model, &child_iter);
        }
    }

  if (!level || g_sequence_get_length (level->seq) < 1)
    {
      gtk_tree_path_free (path);
      return;
    }

  /* NOTE: we do not bail out here if level->seq->len < 2 like
   * GtkTreeModelSort does. This because we do some special tricky
   * reordering.
   */

  tmp_seq = g_sequence_new (filter_elt_free);
  tmp_end_iter = g_sequence_get_end_iter (tmp_seq);
  tmp_array = g_new (int, g_sequence_get_length (level->visible_seq));
  elt_count = 0;

  old_first_siter = g_sequence_get_iter_at_pos (level->seq, 0);

  for (i = 0; i < length; i++)
    {
      GSequenceIter *siter;

      elt = lookup_elt_with_offset (level->seq, new_order[i], &siter);
      if (elt == NULL)
        continue;

      /* Only for visible items an entry should be present in the order array
       * to be emitted.
       */
      if (elt->visible_siter)
        tmp_array[elt_count++] = g_sequence_iter_get_position (elt->visible_siter);

      /* Steal elt from level->seq and append it to tmp_seq */
      g_sequence_move (siter, tmp_end_iter);
      elt->offset = i;
    }

  g_warn_if_fail (g_sequence_get_length (level->seq) == 0);
  g_sequence_free (level->seq);
  level->seq = tmp_seq;
  g_sequence_sort (level->visible_seq, filter_elt_cmp, NULL);

  /* Transfer the reference from the old item at position 0 to the
   * new item at position 0, unless the old item at position 0 is also
   * at position 0 in the new sequence.
   */
  if (g_sequence_iter_get_position (old_first_siter) != 0)
    gtk_tree_model_filter_level_transfer_first_ref (filter,
                                                    level,
                                                    old_first_siter,
                                                    g_sequence_get_iter_at_pos (level->seq, 0));

  /* emit rows_reordered */
  if (g_sequence_get_length (level->visible_seq) > 0)
    {
      if (!gtk_tree_path_get_indices (path))
        gtk_tree_model_rows_reordered (GTK_TREE_MODEL (data), path, NULL,
                                       tmp_array);
      else
        {
          /* get a path taking only visible nodes into account */
          gtk_tree_path_free (path);
          path = gtk_tree_model_get_path (GTK_TREE_MODEL (data), &iter);

          gtk_tree_model_rows_reordered (GTK_TREE_MODEL (data), path, &iter,
                                         tmp_array);
        }
    }

  /* done */
  g_free (tmp_array);
  gtk_tree_path_free (path);
}

/* TreeModelIface implementation */
static GtkTreeModelFlags
gtk_tree_model_filter_get_flags (GtkTreeModel *model)
{
  GtkTreeModelFlags flags;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), 0);
  g_return_val_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->child_model != NULL, 0);

  flags = gtk_tree_model_get_flags (GTK_TREE_MODEL_FILTER (model)->priv->child_model);

  if ((flags & GTK_TREE_MODEL_LIST_ONLY) == GTK_TREE_MODEL_LIST_ONLY)
    return GTK_TREE_MODEL_LIST_ONLY;

  return 0;
}

static int
gtk_tree_model_filter_get_n_columns (GtkTreeModel *model)
{
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *)model;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), 0);
  g_return_val_if_fail (filter->priv->child_model != NULL, 0);

  if (filter->priv->child_model == NULL)
    return 0;

  /* so we can't set the modify func after this ... */
  filter->priv->modify_func_set = TRUE;

  if (filter->priv->modify_n_columns > 0)
    return filter->priv->modify_n_columns;

  return gtk_tree_model_get_n_columns (filter->priv->child_model);
}

static GType
gtk_tree_model_filter_get_column_type (GtkTreeModel *model,
                                       int           index)
{
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *)model;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), G_TYPE_INVALID);
  g_return_val_if_fail (filter->priv->child_model != NULL, G_TYPE_INVALID);

  /* so we can't set the modify func after this ... */
  filter->priv->modify_func_set = TRUE;

  if (filter->priv->modify_types)
    {
      g_return_val_if_fail (index < filter->priv->modify_n_columns, G_TYPE_INVALID);

      return filter->priv->modify_types[index];
    }

  return gtk_tree_model_get_column_type (filter->priv->child_model, index);
}

/* A special case of _get_iter; this function can also get iters which
 * are not visible.  These iters should ONLY be passed internally, never
 * pass those along with a signal emission.
 */
static gboolean
gtk_tree_model_filter_get_iter_full (GtkTreeModel *model,
                                     GtkTreeIter  *iter,
                                     GtkTreePath  *path)
{
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *)model;
  int *indices;
  FilterLevel *level;
  FilterElt *elt;
  int depth, i;
  GSequenceIter *siter;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), FALSE);
  g_return_val_if_fail (filter->priv->child_model != NULL, FALSE);

  indices = gtk_tree_path_get_indices (path);

  if (filter->priv->root == NULL)
    gtk_tree_model_filter_build_level (filter, NULL, NULL, FALSE);
  level = FILTER_LEVEL (filter->priv->root);

  depth = gtk_tree_path_get_depth (path);
  if (!depth)
    {
      iter->stamp = 0;
      return FALSE;
    }

  for (i = 0; i < depth - 1; i++)
    {
      if (!level || indices[i] >= g_sequence_get_length (level->seq))
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

      if (!elt->children)
        gtk_tree_model_filter_build_level (filter, level, elt, FALSE);
      level = elt->children;
    }

  if (!level || indices[i] >= g_sequence_get_length (level->seq))
    {
      iter->stamp = 0;
      return FALSE;
    }

  iter->stamp = filter->priv->stamp;
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

static gboolean
gtk_tree_model_filter_get_iter (GtkTreeModel *model,
                                GtkTreeIter  *iter,
                                GtkTreePath  *path)
{
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *)model;
  int *indices;
  FilterLevel *level;
  FilterElt *elt;
  GSequenceIter *siter;
  int depth, i;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), FALSE);
  g_return_val_if_fail (filter->priv->child_model != NULL, FALSE);

  indices = gtk_tree_path_get_indices (path);

  if (filter->priv->root == NULL)
    gtk_tree_model_filter_build_level (filter, NULL, NULL, FALSE);
  level = FILTER_LEVEL (filter->priv->root);

  depth = gtk_tree_path_get_depth (path);
  if (!depth)
    {
      iter->stamp = 0;
      return FALSE;
    }

  for (i = 0; i < depth - 1; i++)
    {
      if (!level || indices[i] >= g_sequence_get_length (level->visible_seq))
        {
          iter->stamp = 0;
          return FALSE;
        }

      siter = g_sequence_get_iter_at_pos (level->visible_seq, indices[i]);
      if (g_sequence_iter_is_end (siter))
        {
          iter->stamp = 0;
          return FALSE;
        }

      elt = GET_ELT (siter);
      g_assert (elt);
      if (!elt->children)
        gtk_tree_model_filter_build_level (filter, level, elt, FALSE);
      level = elt->children;
    }

  if (!level || indices[i] >= g_sequence_get_length (level->visible_seq))
    {
      iter->stamp = 0;
      return FALSE;
    }

  iter->stamp = filter->priv->stamp;
  iter->user_data = level;

  siter = g_sequence_get_iter_at_pos (level->visible_seq, indices[depth - 1]);
  if (g_sequence_iter_is_end (siter))
    {
      iter->stamp = 0;
      return FALSE;
    }
  iter->user_data2 = GET_ELT (siter);

  return TRUE;
}

static GtkTreePath *
gtk_tree_model_filter_get_path (GtkTreeModel *model,
                                GtkTreeIter  *iter)
{
  GtkTreePath *retval;
  FilterLevel *level;
  FilterElt *elt;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), NULL);
  g_return_val_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->child_model != NULL, NULL);
  g_return_val_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->stamp == iter->stamp, NULL);

  level = iter->user_data;
  elt = iter->user_data2;

  if (!elt->visible_siter)
    return NULL;

  retval = gtk_tree_path_new ();

  while (level)
    {
      int index;

      index = g_sequence_iter_get_position (elt->visible_siter);
      gtk_tree_path_prepend_index (retval, index);

      elt = level->parent_elt;
      level = level->parent_level;
    }

  return retval;
}

static void
gtk_tree_model_filter_real_modify (GtkTreeModelFilter *self,
                                   GtkTreeModel       *child_model,
                                   GtkTreeIter        *iter,
                                   GValue             *value,
                                   int                 column)
{
  if (self->priv->modify_func)
    {
      g_return_if_fail (column < self->priv->modify_n_columns);

      g_value_init (value, self->priv->modify_types[column]);
      self->priv->modify_func (GTK_TREE_MODEL (self),
                               iter, value, column,
                               self->priv->modify_data);
    }
  else
    {
      GtkTreeIter child_iter;

      gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (self),
                                                        &child_iter, iter);
      gtk_tree_model_get_value (child_model, &child_iter, column, value);
    }
}

static void
gtk_tree_model_filter_get_value (GtkTreeModel *model,
                                 GtkTreeIter  *iter,
                                 int           column,
                                 GValue       *value)
{
  GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (model);

  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (model));
  g_return_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->child_model != NULL);
  g_return_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->stamp == iter->stamp);

  GTK_TREE_MODEL_FILTER_GET_CLASS (model)->modify (filter,
      filter->priv->child_model, iter, value, column);
}

static gboolean
gtk_tree_model_filter_iter_next (GtkTreeModel *model,
                                 GtkTreeIter  *iter)
{
  FilterElt *elt;
  GSequenceIter *siter;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->child_model != NULL, FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->stamp == iter->stamp, FALSE);

  elt = iter->user_data2;

  siter = g_sequence_iter_next (elt->visible_siter);
  if (g_sequence_iter_is_end (siter))
    {
      iter->stamp = 0;
      return FALSE;
    }

  iter->user_data2 = GET_ELT (siter);

  return TRUE;
}

static gboolean
gtk_tree_model_filter_iter_previous (GtkTreeModel *model,
                                     GtkTreeIter  *iter)
{
  FilterElt *elt;
  GSequenceIter *siter;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->child_model != NULL, FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->stamp == iter->stamp, FALSE);

  elt = iter->user_data2;

  if (g_sequence_iter_is_begin (elt->visible_siter))
    {
      iter->stamp = 0;
      return FALSE;
    }
  siter = g_sequence_iter_prev (elt->visible_siter);

  iter->user_data2 = GET_ELT (siter);

  return TRUE;
}

static gboolean
gtk_tree_model_filter_iter_children (GtkTreeModel *model,
                                     GtkTreeIter  *iter,
                                     GtkTreeIter  *parent)
{
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *)model;
  FilterLevel *level;
  GSequenceIter *siter;

  iter->stamp = 0;
  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), FALSE);
  g_return_val_if_fail (filter->priv->child_model != NULL, FALSE);
  if (parent)
    g_return_val_if_fail (filter->priv->stamp == parent->stamp, FALSE);

  if (!parent)
    {
      if (!filter->priv->root)
        gtk_tree_model_filter_build_level (filter, NULL, NULL, FALSE);
      if (!filter->priv->root)
        return FALSE;

      level = filter->priv->root;
      siter = g_sequence_get_begin_iter (level->visible_seq);
      if (g_sequence_iter_is_end (siter))
        {
          iter->stamp = 0;
          return FALSE;
        }

      iter->stamp = filter->priv->stamp;
      iter->user_data = level;
      iter->user_data2 = GET_ELT (siter);

      return TRUE;
    }
  else
    {
      if (FILTER_ELT (parent->user_data2)->children == NULL)
        gtk_tree_model_filter_build_level (filter,
                                           FILTER_LEVEL (parent->user_data),
                                           FILTER_ELT (parent->user_data2),
                                           FALSE);
      if (FILTER_ELT (parent->user_data2)->children == NULL)
        return FALSE;

      level = FILTER_ELT (parent->user_data2)->children;
      siter = g_sequence_get_begin_iter (level->visible_seq);
      if (g_sequence_iter_is_end (siter))
        {
          iter->stamp = 0;
          return FALSE;
        }

      iter->stamp = filter->priv->stamp;
      iter->user_data = level;
      iter->user_data2 = GET_ELT (siter);

      return TRUE;
    }
}

static gboolean
gtk_tree_model_filter_iter_has_child (GtkTreeModel *model,
                                      GtkTreeIter  *iter)
{
  GtkTreeIter child_iter;
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *)model;
  FilterElt *elt;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), FALSE);
  g_return_val_if_fail (filter->priv->child_model != NULL, FALSE);
  g_return_val_if_fail (filter->priv->stamp == iter->stamp, FALSE);

  filter = GTK_TREE_MODEL_FILTER (model);

  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &child_iter, iter);
  elt = FILTER_ELT (iter->user_data2);

  if (!elt->visible_siter)
    return FALSE;

  /* we need to build the level to check if not all children are filtered
   * out
   */
  if (!elt->children
      && gtk_tree_model_iter_has_child (filter->priv->child_model, &child_iter))
    gtk_tree_model_filter_build_level (filter, FILTER_LEVEL (iter->user_data),
                                       elt, FALSE);

  if (elt->children && g_sequence_get_length (elt->children->visible_seq) > 0)
    return TRUE;

  return FALSE;
}

static int
gtk_tree_model_filter_iter_n_children (GtkTreeModel *model,
                                       GtkTreeIter  *iter)
{
  GtkTreeIter child_iter;
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *)model;
  FilterElt *elt;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), 0);
  g_return_val_if_fail (filter->priv->child_model != NULL, 0);
  if (iter)
    g_return_val_if_fail (filter->priv->stamp == iter->stamp, 0);

  if (!iter)
    {
      if (!filter->priv->root)
        gtk_tree_model_filter_build_level (filter, NULL, NULL, FALSE);

      if (filter->priv->root)
        return g_sequence_get_length (FILTER_LEVEL (filter->priv->root)->visible_seq);

      return 0;
    }

  elt = FILTER_ELT (iter->user_data2);

  if (!elt->visible_siter)
    return 0;

  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &child_iter, iter);

  if (!elt->children &&
      gtk_tree_model_iter_has_child (filter->priv->child_model, &child_iter))
    gtk_tree_model_filter_build_level (filter,
                                       FILTER_LEVEL (iter->user_data),
                                       elt, FALSE);

  if (elt->children)
    return g_sequence_get_length (elt->children->visible_seq);

  return 0;
}

static gboolean
gtk_tree_model_filter_iter_nth_child (GtkTreeModel *model,
                                      GtkTreeIter  *iter,
                                      GtkTreeIter  *parent,
                                      int           n)
{
  FilterLevel *level;
  GtkTreeIter children;
  GSequenceIter *siter;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), FALSE);
  if (parent)
    g_return_val_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->stamp == parent->stamp, FALSE);

  /* use this instead of has_child to force us to build the level, if needed */
  if (gtk_tree_model_filter_iter_children (model, &children, parent) == FALSE)
    {
      iter->stamp = 0;
      return FALSE;
    }

  level = children.user_data;
  siter = g_sequence_get_iter_at_pos (level->visible_seq, n);
  if (g_sequence_iter_is_end (siter))
    return FALSE;

  iter->stamp = GTK_TREE_MODEL_FILTER (model)->priv->stamp;
  iter->user_data = level;
  iter->user_data2 = GET_ELT (siter);

  return TRUE;
}

static gboolean
gtk_tree_model_filter_iter_parent (GtkTreeModel *model,
                                   GtkTreeIter  *iter,
                                   GtkTreeIter  *child)
{
  FilterLevel *level;

  iter->stamp = 0;
  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->child_model != NULL, FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->stamp == child->stamp, FALSE);

  level = child->user_data;

  if (level->parent_level)
    {
      iter->stamp = GTK_TREE_MODEL_FILTER (model)->priv->stamp;
      iter->user_data = level->parent_level;
      iter->user_data2 = level->parent_elt;

      return TRUE;
    }

  return FALSE;
}

static void
gtk_tree_model_filter_ref_node (GtkTreeModel *model,
                                GtkTreeIter  *iter)
{
  gtk_tree_model_filter_real_ref_node (model, iter, TRUE);
}

static void
gtk_tree_model_filter_real_ref_node (GtkTreeModel *model,
                                     GtkTreeIter  *iter,
                                     gboolean      external)
{
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *)model;
  GtkTreeIter child_iter;
  FilterLevel *level;
  FilterElt *elt;

  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (model));
  g_return_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->child_model != NULL);
  g_return_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->stamp == iter->stamp);

  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &child_iter, iter);

  gtk_tree_model_ref_node (filter->priv->child_model, &child_iter);

  level = iter->user_data;
  elt = iter->user_data2;

  elt->ref_count++;
  level->ref_count++;

  if (external)
    {
      elt->ext_ref_count++;
      level->ext_ref_count++;

      if (level->ext_ref_count == 1)
        {
          FilterLevel *parent_level = level->parent_level;
          FilterElt *parent_elt = level->parent_elt;

          /* we were at zero -- time to decrease the zero_ref_count val */
          while (parent_level)
            {
              parent_elt->zero_ref_count--;

              parent_elt = parent_level->parent_elt;
              parent_level = parent_level->parent_level;
            }

          if (filter->priv->root != level)
            filter->priv->zero_ref_count--;

#ifdef MODEL_FILTER_DEBUG
          g_assert (filter->priv->zero_ref_count >= 0);
          if (filter->priv->zero_ref_count > 0)
            g_assert (filter->priv->root != NULL);
#endif
        }
    }

#ifdef MODEL_FILTER_DEBUG
  g_assert (elt->ref_count >= elt->ext_ref_count);
  g_assert (elt->ref_count >= 0);
  g_assert (elt->ext_ref_count >= 0);
#endif
}

static void
gtk_tree_model_filter_unref_node (GtkTreeModel *model,
                                  GtkTreeIter  *iter)
{
  gtk_tree_model_filter_real_unref_node (model, iter, TRUE, TRUE);
}

static void
gtk_tree_model_filter_real_unref_node (GtkTreeModel *model,
                                       GtkTreeIter  *iter,
                                       gboolean      external,
                                       gboolean      propagate_unref)
{
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *)model;
  FilterLevel *level;
  FilterElt *elt;

  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (model));
  g_return_if_fail (filter->priv->child_model != NULL);
  g_return_if_fail (filter->priv->stamp == iter->stamp);

  if (propagate_unref)
    {
      GtkTreeIter child_iter;
      gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &child_iter, iter);
      gtk_tree_model_unref_node (filter->priv->child_model, &child_iter);
    }

  level = iter->user_data;
  elt = iter->user_data2;

  g_return_if_fail (elt->ref_count > 0);
#ifdef MODEL_FILTER_DEBUG
  g_assert (elt->ref_count >= elt->ext_ref_count);
  g_assert (elt->ref_count >= 0);
  g_assert (elt->ext_ref_count >= 0);
#endif

  elt->ref_count--;
  level->ref_count--;

  if (external)
    {
      elt->ext_ref_count--;
      level->ext_ref_count--;

      if (level->ext_ref_count == 0)
        {
          FilterLevel *parent_level = level->parent_level;
          FilterElt *parent_elt = level->parent_elt;

          /* we are at zero -- time to increase the zero_ref_count val */
          while (parent_level)
            {
              parent_elt->zero_ref_count++;

              parent_elt = parent_level->parent_elt;
              parent_level = parent_level->parent_level;
            }

          if (filter->priv->root != level)
            filter->priv->zero_ref_count++;

#ifdef MODEL_FILTER_DEBUG
          g_assert (filter->priv->zero_ref_count >= 0);
          if (filter->priv->zero_ref_count > 0)
            g_assert (filter->priv->root != NULL);
#endif
        }
    }

#ifdef MODEL_FILTER_DEBUG
  g_assert (elt->ref_count >= elt->ext_ref_count);
  g_assert (elt->ref_count >= 0);
  g_assert (elt->ext_ref_count >= 0);
#endif
}

/* TreeDragSource interface implementation */
static gboolean
gtk_tree_model_filter_row_draggable (GtkTreeDragSource *drag_source,
                                     GtkTreePath       *path)
{
  GtkTreeModelFilter *tree_model_filter = (GtkTreeModelFilter *)drag_source;
  GtkTreePath *child_path;
  gboolean draggable;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (drag_source), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  child_path = gtk_tree_model_filter_convert_path_to_child_path (tree_model_filter, path);
  draggable = gtk_tree_drag_source_row_draggable (GTK_TREE_DRAG_SOURCE (tree_model_filter->priv->child_model), child_path);
  gtk_tree_path_free (child_path);

  return draggable;
}

static GdkContentProvider *
gtk_tree_model_filter_drag_data_get (GtkTreeDragSource *drag_source,
                                     GtkTreePath       *path)
{
  GtkTreeModelFilter *tree_model_filter = (GtkTreeModelFilter *)drag_source;
  GtkTreePath *child_path;
  GdkContentProvider *gotten;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (drag_source), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  child_path = gtk_tree_model_filter_convert_path_to_child_path (tree_model_filter, path);
  gotten = gtk_tree_drag_source_drag_data_get (GTK_TREE_DRAG_SOURCE (tree_model_filter->priv->child_model), child_path);
  gtk_tree_path_free (child_path);

  return gotten;
}

static gboolean
gtk_tree_model_filter_drag_data_delete (GtkTreeDragSource *drag_source,
                                        GtkTreePath       *path)
{
  GtkTreeModelFilter *tree_model_filter = (GtkTreeModelFilter *)drag_source;
  GtkTreePath *child_path;
  gboolean deleted;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (drag_source), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  child_path = gtk_tree_model_filter_convert_path_to_child_path (tree_model_filter, path);
  deleted = gtk_tree_drag_source_drag_data_delete (GTK_TREE_DRAG_SOURCE (tree_model_filter->priv->child_model), child_path);
  gtk_tree_path_free (child_path);

  return deleted;
}

/* bits and pieces */
static void
gtk_tree_model_filter_set_model (GtkTreeModelFilter *filter,
                                 GtkTreeModel       *child_model)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (filter));

  if (filter->priv->child_model)
    {
      g_signal_handler_disconnect (filter->priv->child_model,
                                   filter->priv->changed_id);
      g_signal_handler_disconnect (filter->priv->child_model,
                                   filter->priv->inserted_id);
      g_signal_handler_disconnect (filter->priv->child_model,
                                   filter->priv->has_child_toggled_id);
      g_signal_handler_disconnect (filter->priv->child_model,
                                   filter->priv->deleted_id);
      g_signal_handler_disconnect (filter->priv->child_model,
                                   filter->priv->reordered_id);

      /* reset our state */
      if (filter->priv->root)
        gtk_tree_model_filter_free_level (filter, filter->priv->root,
                                          TRUE, TRUE, FALSE);

      filter->priv->root = NULL;
      g_object_unref (filter->priv->child_model);
      filter->priv->visible_column = -1;

      /* FIXME: do we need to destroy more here? */
    }

  filter->priv->child_model = child_model;

  if (child_model)
    {
      g_object_ref (filter->priv->child_model);
      filter->priv->changed_id =
        g_signal_connect (child_model, "row-changed",
                          G_CALLBACK (gtk_tree_model_filter_row_changed),
                          filter);
      filter->priv->inserted_id =
        g_signal_connect (child_model, "row-inserted",
                          G_CALLBACK (gtk_tree_model_filter_row_inserted),
                          filter);
      filter->priv->has_child_toggled_id =
        g_signal_connect (child_model, "row-has-child-toggled",
                          G_CALLBACK (gtk_tree_model_filter_row_has_child_toggled),
                          filter);
      filter->priv->deleted_id =
        g_signal_connect (child_model, "row-deleted",
                          G_CALLBACK (gtk_tree_model_filter_row_deleted),
                          filter);
      filter->priv->reordered_id =
        g_signal_connect (child_model, "rows-reordered",
                          G_CALLBACK (gtk_tree_model_filter_rows_reordered),
                          filter);

      filter->priv->child_flags = gtk_tree_model_get_flags (child_model);
      filter->priv->stamp = g_random_int ();
    }
}

static void
gtk_tree_model_filter_ref_path (GtkTreeModelFilter *filter,
                                GtkTreePath        *path)
{
  int len;
  GtkTreePath *p;

  len = gtk_tree_path_get_depth (path);
  p = gtk_tree_path_copy (path);
  while (len--)
    {
      GtkTreeIter iter;

      gtk_tree_model_get_iter (GTK_TREE_MODEL (filter->priv->child_model), &iter, p);
      gtk_tree_model_ref_node (GTK_TREE_MODEL (filter->priv->child_model), &iter);
      gtk_tree_path_up (p);
    }

  gtk_tree_path_free (p);
}

static void
gtk_tree_model_filter_unref_path (GtkTreeModelFilter *filter,
                                  GtkTreePath        *path,
                                  int                 depth)
{
  int len;
  GtkTreePath *p;

  if (depth != -1)
    len = depth;
  else
    len = gtk_tree_path_get_depth (path);

  p = gtk_tree_path_copy (path);
  while (len--)
    {
      GtkTreeIter iter;

      gtk_tree_model_get_iter (GTK_TREE_MODEL (filter->priv->child_model), &iter, p);
      gtk_tree_model_unref_node (GTK_TREE_MODEL (filter->priv->child_model), &iter);
      gtk_tree_path_up (p);
    }

  gtk_tree_path_free (p);
}

static void
gtk_tree_model_filter_set_root (GtkTreeModelFilter *filter,
                                GtkTreePath        *root)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (filter));

  if (root)
    {
      filter->priv->virtual_root = gtk_tree_path_copy (root);
      gtk_tree_model_filter_ref_path (filter, filter->priv->virtual_root);
      filter->priv->virtual_root_deleted = FALSE;
    }
  else
    filter->priv->virtual_root = NULL;
}

/* public API */

/**
 * gtk_tree_model_filter_new:
 * @child_model: A `GtkTreeModel`.
 * @root: (nullable): A `GtkTreePath`
 *
 * Creates a new `GtkTreeModel`, with @child_model as the child_model
 * and @root as the virtual root.
 *
 * Returns: (transfer full): A new `GtkTreeModel`.
 *
 * Deprecated: 4.10
 */
GtkTreeModel *
gtk_tree_model_filter_new (GtkTreeModel *child_model,
                           GtkTreePath  *root)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL (child_model), NULL);

  return g_object_new (GTK_TYPE_TREE_MODEL_FILTER,
                       "child-model", child_model,
                       "virtual-root", root,
                       NULL);
}

/**
 * gtk_tree_model_filter_get_model:
 * @filter: A `GtkTreeModelFilter`
 *
 * Returns a pointer to the child model of @filter.
 *
 * Returns: (transfer none): A pointer to a `GtkTreeModel`
 *
 * Deprecated: 4.10
 */
GtkTreeModel *
gtk_tree_model_filter_get_model (GtkTreeModelFilter *filter)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (filter), NULL);

  return filter->priv->child_model;
}

/**
 * gtk_tree_model_filter_set_visible_func:
 * @filter: A `GtkTreeModelFilter`
 * @func: A `GtkTreeModelFilterVisibleFunc`, the visible function
 * @data: (nullable): User data to pass to the visible function
 * @destroy: (nullable): Destroy notifier of @data
 *
 * Sets the visible function used when filtering the @filter to be @func.
 * The function should return %TRUE if the given row should be visible and
 * %FALSE otherwise.
 *
 * If the condition calculated by the function changes over time (e.g.
 * because it depends on some global parameters), you must call
 * gtk_tree_model_filter_refilter() to keep the visibility information
 * of the model up-to-date.
 *
 * Note that @func is called whenever a row is inserted, when it may still
 * be empty. The visible function should therefore take special care of empty
 * rows, like in the example below.
 *
 * |[<!-- language="C" -->
 * static gboolean
 * visible_func (GtkTreeModel *model,
 *               GtkTreeIter  *iter,
 *               gpointer      data)
 * {
 *   // Visible if row is non-empty and first column is “HI”
 *   char *str;
 *   gboolean visible = FALSE;
 *
 *   gtk_tree_model_get (model, iter, 0, &str, -1);
 *   if (str && strcmp (str, "HI") == 0)
 *     visible = TRUE;
 *   g_free (str);
 *
 *   return visible;
 * }
 * ]|
 *
 * Note that gtk_tree_model_filter_set_visible_func() or
 * gtk_tree_model_filter_set_visible_column() can only be called
 * once for a given filter model.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_model_filter_set_visible_func (GtkTreeModelFilter            *filter,
                                        GtkTreeModelFilterVisibleFunc  func,
                                        gpointer                       data,
                                        GDestroyNotify                 destroy)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (filter));
  g_return_if_fail (func != NULL);
  g_return_if_fail (filter->priv->visible_method_set == FALSE);

  filter->priv->visible_func = func;
  filter->priv->visible_data = data;
  filter->priv->visible_destroy = destroy;

  filter->priv->visible_method_set = TRUE;
}

/**
 * gtk_tree_model_filter_set_modify_func:
 * @filter: A `GtkTreeModelFilter`
 * @n_columns: The number of columns in the filter model.
 * @types: (array length=n_columns): The `GType`s of the columns.
 * @func: A `GtkTreeModelFilterModifyFunc`
 * @data: (nullable): User data to pass to the modify function
 * @destroy: (nullable): Destroy notifier of @data
 *
 * With the @n_columns and @types parameters, you give an array of column
 * types for this model (which will be exposed to the parent model/view).
 * The @func, @data and @destroy parameters are for specifying the modify
 * function. The modify function will get called for each
 * data access, the goal of the modify function is to return the data which
 * should be displayed at the location specified using the parameters of the
 * modify function.
 *
 * Note that gtk_tree_model_filter_set_modify_func()
 * can only be called once for a given filter model.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_model_filter_set_modify_func (GtkTreeModelFilter           *filter,
                                       int                           n_columns,
                                       GType                        *types,
                                       GtkTreeModelFilterModifyFunc  func,
                                       gpointer                      data,
                                       GDestroyNotify                destroy)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (filter));
  g_return_if_fail (func != NULL);
  g_return_if_fail (filter->priv->modify_func_set == FALSE);

  filter->priv->modify_n_columns = n_columns;
  filter->priv->modify_types = g_new0 (GType, n_columns);
  memcpy (filter->priv->modify_types, types, sizeof (GType) * n_columns);
  filter->priv->modify_func = func;
  filter->priv->modify_data = data;
  filter->priv->modify_destroy = destroy;

  filter->priv->modify_func_set = TRUE;
}

/**
 * gtk_tree_model_filter_set_visible_column:
 * @filter: A `GtkTreeModelFilter`
 * @column: A `int` which is the column containing the visible information
 *
 * Sets @column of the child_model to be the column where @filter should
 * look for visibility information. @columns should be a column of type
 * %G_TYPE_BOOLEAN, where %TRUE means that a row is visible, and %FALSE
 * if not.
 *
 * Note that gtk_tree_model_filter_set_visible_func() or
 * gtk_tree_model_filter_set_visible_column() can only be called
 * once for a given filter model.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_model_filter_set_visible_column (GtkTreeModelFilter *filter,
                                          int column)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (filter));
  g_return_if_fail (column >= 0);
  g_return_if_fail (filter->priv->visible_method_set == FALSE);

  filter->priv->visible_column = column;

  filter->priv->visible_method_set = TRUE;
}

/* conversion */

/**
 * gtk_tree_model_filter_convert_child_iter_to_iter:
 * @filter: A `GtkTreeModelFilter`
 * @filter_iter: (out): An uninitialized `GtkTreeIter`
 * @child_iter: A valid `GtkTreeIter` pointing to a row on the child model.
 *
 * Sets @filter_iter to point to the row in @filter that corresponds to the
 * row pointed at by @child_iter.  If @filter_iter was not set, %FALSE is
 * returned.
 *
 * Returns: %TRUE, if @filter_iter was set, i.e. if @child_iter is a
 * valid iterator pointing to a visible row in child model.
 *
 * Deprecated: 4.10
 */
gboolean
gtk_tree_model_filter_convert_child_iter_to_iter (GtkTreeModelFilter *filter,
                                                  GtkTreeIter        *filter_iter,
                                                  GtkTreeIter        *child_iter)
{
  gboolean ret;
  GtkTreePath *child_path, *path;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (filter), FALSE);
  g_return_val_if_fail (filter->priv->child_model != NULL, FALSE);
  g_return_val_if_fail (filter_iter != NULL, FALSE);
  g_return_val_if_fail (child_iter != NULL, FALSE);
  g_return_val_if_fail (filter_iter != child_iter, FALSE);

  filter_iter->stamp = 0;

  child_path = gtk_tree_model_get_path (filter->priv->child_model, child_iter);
  g_return_val_if_fail (child_path != NULL, FALSE);

  path = gtk_tree_model_filter_convert_child_path_to_path (filter,
                                                           child_path);
  gtk_tree_path_free (child_path);

  if (!path)
    return FALSE;

  ret = gtk_tree_model_get_iter (GTK_TREE_MODEL (filter), filter_iter, path);
  gtk_tree_path_free (path);

  return ret;
}

/**
 * gtk_tree_model_filter_convert_iter_to_child_iter:
 * @filter: A `GtkTreeModelFilter`
 * @child_iter: (out): An uninitialized `GtkTreeIter`
 * @filter_iter: A valid `GtkTreeIter` pointing to a row on @filter.
 *
 * Sets @child_iter to point to the row pointed to by @filter_iter.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_model_filter_convert_iter_to_child_iter (GtkTreeModelFilter *filter,
                                                  GtkTreeIter        *child_iter,
                                                  GtkTreeIter        *filter_iter)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (filter));
  g_return_if_fail (filter->priv->child_model != NULL);
  g_return_if_fail (child_iter != NULL);
  g_return_if_fail (filter_iter != NULL);
  g_return_if_fail (filter_iter->stamp == filter->priv->stamp);
  g_return_if_fail (filter_iter != child_iter);

  if (GTK_TREE_MODEL_FILTER_CACHE_CHILD_ITERS (filter))
    {
      *child_iter = FILTER_ELT (filter_iter->user_data2)->iter;
    }
  else
    {
      GtkTreePath *path;
      gboolean valid = FALSE;

      path = gtk_tree_model_filter_elt_get_path (filter_iter->user_data,
                                                 filter_iter->user_data2,
                                                 filter->priv->virtual_root);
      valid = gtk_tree_model_get_iter (filter->priv->child_model, child_iter,
                                       path);
      gtk_tree_path_free (path);

      g_return_if_fail (valid == TRUE);
    }
}

/* The path returned can only be used internally in the filter model. */
static GtkTreePath *
gtk_real_tree_model_filter_convert_child_path_to_path (GtkTreeModelFilter *filter,
                                                       GtkTreePath        *child_path,
                                                       gboolean            build_levels,
                                                       gboolean            fetch_children)
{
  int *child_indices;
  GtkTreePath *retval;
  GtkTreePath *real_path;
  FilterLevel *level;
  FilterElt *tmp;
  int i;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (filter), NULL);
  g_return_val_if_fail (filter->priv->child_model != NULL, NULL);
  g_return_val_if_fail (child_path != NULL, NULL);

  if (!filter->priv->virtual_root)
    real_path = gtk_tree_path_copy (child_path);
  else
    real_path = gtk_tree_model_filter_remove_root (child_path,
                                                   filter->priv->virtual_root);

  if (!real_path)
    return NULL;

  retval = gtk_tree_path_new ();
  child_indices = gtk_tree_path_get_indices (real_path);

  if (filter->priv->root == NULL && build_levels)
    gtk_tree_model_filter_build_level (filter, NULL, NULL, FALSE);
  level = FILTER_LEVEL (filter->priv->root);

  for (i = 0; i < gtk_tree_path_get_depth (real_path); i++)
    {
      GSequenceIter *siter;
      gboolean found_child = FALSE;

      if (!level)
        {
          gtk_tree_path_free (real_path);
          gtk_tree_path_free (retval);
          return NULL;
        }

      tmp = lookup_elt_with_offset (level->seq, child_indices[i], &siter);
      if (tmp)
        {
          gtk_tree_path_append_index (retval, g_sequence_iter_get_position (siter));
          if (!tmp->children && build_levels)
            gtk_tree_model_filter_build_level (filter, level, tmp, FALSE);
          level = tmp->children;
          found_child = TRUE;
        }

      if (!found_child && fetch_children)
        {
          int j;

          tmp = gtk_tree_model_filter_fetch_child (filter, level,
                                                   child_indices[i],
                                                   &j);

          /* didn't find the child, let's try to bring it back */
          if (!tmp || tmp->offset != child_indices[i])
            {
              /* not there */
              gtk_tree_path_free (real_path);
              gtk_tree_path_free (retval);
              return NULL;
            }

          gtk_tree_path_append_index (retval, j);
          if (!tmp->children && build_levels)
            gtk_tree_model_filter_build_level (filter, level, tmp, FALSE);
          level = tmp->children;
          found_child = TRUE;
        }
      else if (!found_child && !fetch_children)
        {
          /* no path */
          gtk_tree_path_free (real_path);
          gtk_tree_path_free (retval);
          return NULL;
        }
    }

  gtk_tree_path_free (real_path);
  return retval;
}

/**
 * gtk_tree_model_filter_convert_child_path_to_path:
 * @filter: A `GtkTreeModelFilter`
 * @child_path: A `GtkTreePath` to convert.
 *
 * Converts @child_path to a path relative to @filter. That is, @child_path
 * points to a path in the child model. The rerturned path will point to the
 * same row in the filtered model. If @child_path isn’t a valid path on the
 * child model or points to a row which is not visible in @filter, then %NULL
 * is returned.
 *
 * Returns: (nullable) (transfer full): A newly allocated `GtkTreePath`
 *
 * Deprecated: 4.10
 */
GtkTreePath *
gtk_tree_model_filter_convert_child_path_to_path (GtkTreeModelFilter *filter,
                                                  GtkTreePath        *child_path)
{
  GtkTreeIter iter;
  GtkTreePath *path;

  /* this function does the sanity checks */
  path = gtk_real_tree_model_filter_convert_child_path_to_path (filter,
                                                                child_path,
                                                                TRUE,
                                                                TRUE);

  if (!path)
      return NULL;

  /* get a new path which only takes visible nodes into account.
   * -- if this gives any performance issues, we can write a special
   *    version of convert_child_path_to_path immediately returning
   *    a visible-nodes-only path.
   */
  gtk_tree_model_filter_get_iter_full (GTK_TREE_MODEL (filter), &iter, path);

  gtk_tree_path_free (path);
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (filter), &iter);

  return path;
}

/**
 * gtk_tree_model_filter_convert_path_to_child_path:
 * @filter: A `GtkTreeModelFilter`
 * @filter_path: A `GtkTreePath` to convert.
 *
 * Converts @filter_path to a path on the child model of @filter. That is,
 * @filter_path points to a location in @filter. The returned path will
 * point to the same location in the model not being filtered. If @filter_path
 * does not point to a location in the child model, %NULL is returned.
 *
 * Returns: (nullable) (transfer full): A newly allocated `GtkTreePath`
 *
 * Deprecated: 4.10
 */
GtkTreePath *
gtk_tree_model_filter_convert_path_to_child_path (GtkTreeModelFilter *filter,
                                                  GtkTreePath        *filter_path)
{
  int *filter_indices;
  GtkTreePath *retval;
  FilterLevel *level;
  int i;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (filter), NULL);
  g_return_val_if_fail (filter->priv->child_model != NULL, NULL);
  g_return_val_if_fail (filter_path != NULL, NULL);

  /* convert path */
  retval = gtk_tree_path_new ();
  filter_indices = gtk_tree_path_get_indices (filter_path);
  if (!filter->priv->root)
    gtk_tree_model_filter_build_level (filter, NULL, NULL, FALSE);
  level = FILTER_LEVEL (filter->priv->root);

  for (i = 0; i < gtk_tree_path_get_depth (filter_path); i++)
    {
      FilterElt *elt;
      GSequenceIter *siter;

      if (!level)
        {
          gtk_tree_path_free (retval);
          return NULL;
        }

      siter = g_sequence_get_iter_at_pos (level->visible_seq, filter_indices[i]);
      if (g_sequence_iter_is_end (siter))
        {
          gtk_tree_path_free (retval);
          return NULL;
        }

      elt = GET_ELT (siter);
      g_assert (elt);
      if (elt->children == NULL)
        gtk_tree_model_filter_build_level (filter, level, elt, FALSE);

      gtk_tree_path_append_index (retval, elt->offset);
      level = elt->children;
    }

  /* apply vroot */

  if (filter->priv->virtual_root)
    {
      GtkTreePath *real_retval;

      real_retval = gtk_tree_model_filter_add_root (retval,
                                                    filter->priv->virtual_root);
      gtk_tree_path_free (retval);

      return real_retval;
    }

  return retval;
}

static gboolean
gtk_tree_model_filter_refilter_helper (GtkTreeModel *model,
                                       GtkTreePath  *path,
                                       GtkTreeIter  *iter,
                                       gpointer      data)
{
  /* evil, don't try this at home, but certainly speeds things up */
  gtk_tree_model_filter_row_changed (model, path, iter, data);

  return FALSE;
}

/**
 * gtk_tree_model_filter_refilter:
 * @filter: A `GtkTreeModelFilter`
 *
 * Emits ::row_changed for each row in the child model, which causes
 * the filter to re-evaluate whether a row is visible or not.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_model_filter_refilter (GtkTreeModelFilter *filter)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (filter));

  /* S L O W */
  gtk_tree_model_foreach (filter->priv->child_model,
                          gtk_tree_model_filter_refilter_helper,
                          filter);
}

/**
 * gtk_tree_model_filter_clear_cache:
 * @filter: A `GtkTreeModelFilter`
 *
 * This function should almost never be called. It clears the @filter
 * of any cached iterators that haven’t been reffed with
 * gtk_tree_model_ref_node(). This might be useful if the child model
 * being filtered is static (and doesn’t change often) and there has been
 * a lot of unreffed access to nodes. As a side effect of this function,
 * all unreffed iters will be invalid.
 *
 * Deprecated: 4.10
 */
void
gtk_tree_model_filter_clear_cache (GtkTreeModelFilter *filter)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (filter));

  if (filter->priv->zero_ref_count > 0)
    gtk_tree_model_filter_clear_cache_helper (filter,
                                              FILTER_LEVEL (filter->priv->root));
}
