/* gtktreemodelrefcount.c
 * Copyright (C) 2011  Kristian Rietveld <kris@gtk.org>
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
#include "gtktreemodelrefcount.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/* The purpose of this GtkTreeModel is to keep record of the reference count
 * of each node.  The reference count does not effect the functioning of
 * the model in any way.  Because this model is a subclass of GtkTreeStore,
 * the GtkTreeStore API should be used to add to and remove nodes from
 * this model.  We depend on the iter format of GtkTreeStore, which means
 * that this model needs to be revised in case the iter format of
 * GtkTreeStore is modified.  Currently, we make use of the fact that
 * the value stored in the user_data field is unique for each node.
 */

struct _GtkTreeModelRefCountPrivate
{
  GHashTable *node_hash;
};

typedef struct
{
  int ref_count;
}
NodeInfo;


static void      gtk_tree_model_ref_count_tree_model_init (GtkTreeModelIface *iface);
static void      gtk_tree_model_ref_count_finalize        (GObject           *object);

static NodeInfo *node_info_new                            (void);
static void      node_info_free                           (NodeInfo          *info);

/* GtkTreeModel interface */
static void      gtk_tree_model_ref_count_ref_node        (GtkTreeModel      *model,
                                                           GtkTreeIter       *iter);
static void      gtk_tree_model_ref_count_unref_node      (GtkTreeModel      *model,
                                                           GtkTreeIter       *iter);


G_DEFINE_TYPE_WITH_CODE (GtkTreeModelRefCount, gtk_tree_model_ref_count, GTK_TYPE_TREE_STORE,
                         G_ADD_PRIVATE (GtkTreeModelRefCount)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
                                                gtk_tree_model_ref_count_tree_model_init))

static void
row_removed (GtkTreeModelRefCount *ref_model,
             GtkTreePath *path)
{
  GHashTableIter iter;
  GtkTreeIter tree_iter;

  if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (ref_model), &tree_iter))
    {
      g_hash_table_remove_all (ref_model->priv->node_hash);
      return;
    }

  g_hash_table_iter_init (&iter, ref_model->priv->node_hash);

  while (g_hash_table_iter_next (&iter, &tree_iter.user_data, NULL))
    {
      if (!gtk_tree_store_iter_is_valid (GTK_TREE_STORE (ref_model), &tree_iter))
        g_hash_table_iter_remove (&iter);
    }
}

static void
gtk_tree_model_ref_count_init (GtkTreeModelRefCount *ref_model)
{
  ref_model->priv = gtk_tree_model_ref_count_get_instance_private (ref_model); 

  ref_model->priv->node_hash = g_hash_table_new_full (g_direct_hash,
                                                      g_direct_equal,
                                                      NULL,
                                                      (GDestroyNotify)node_info_free);

  g_signal_connect (ref_model, "row-deleted", G_CALLBACK (row_removed), NULL);
}

static void
gtk_tree_model_ref_count_class_init (GtkTreeModelRefCountClass *ref_model_class)
{
  G_OBJECT_CLASS (ref_model_class)->finalize = gtk_tree_model_ref_count_finalize;
}

static void
gtk_tree_model_ref_count_tree_model_init (GtkTreeModelIface *iface)
{
  iface->ref_node = gtk_tree_model_ref_count_ref_node;
  iface->unref_node = gtk_tree_model_ref_count_unref_node;
}

static void
gtk_tree_model_ref_count_finalize (GObject *object)
{
  GtkTreeModelRefCount *ref_model = GTK_TREE_MODEL_REF_COUNT (object);

  if (ref_model->priv->node_hash)
    {
      g_hash_table_destroy (ref_model->priv->node_hash);
      ref_model->priv->node_hash = NULL;
    }

  G_OBJECT_CLASS (gtk_tree_model_ref_count_parent_class)->finalize (object);
}


static NodeInfo *
node_info_new (void)
{
  NodeInfo *info = g_new (NodeInfo, 1);
  info->ref_count = 0;

  return info;
}

static void
node_info_free (NodeInfo *info)
{
  g_free (info);
}

static void
gtk_tree_model_ref_count_ref_node (GtkTreeModel *model,
                                   GtkTreeIter  *iter)
{
  NodeInfo *info;
  GtkTreeModelRefCount *ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  info = g_hash_table_lookup (ref_model->priv->node_hash, iter->user_data);
  if (!info)
    {
      info = node_info_new ();

      g_hash_table_insert (ref_model->priv->node_hash, iter->user_data, info);
    }

  info->ref_count++;
}

static void
gtk_tree_model_ref_count_unref_node (GtkTreeModel *model,
                                     GtkTreeIter  *iter)
{
  NodeInfo *info;
  GtkTreeModelRefCount *ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  info = g_hash_table_lookup (ref_model->priv->node_hash, iter->user_data);
  g_assert_nonnull (info);
  g_assert_cmpint (info->ref_count, >, 0);

  info->ref_count--;
}


GtkTreeModel *
gtk_tree_model_ref_count_new (void)
{
  GtkTreeModel *retval;

  retval = g_object_new (gtk_tree_model_ref_count_get_type (), NULL);

  return retval;
}

static void
dump_iter (GtkTreeModelRefCount *ref_model,
           GtkTreeIter          *iter)
{
  char *path_str;
  NodeInfo *info;
  GtkTreePath *path;

  path = gtk_tree_model_get_path (GTK_TREE_MODEL (ref_model), iter);
  path_str = gtk_tree_path_to_string (path);
  gtk_tree_path_free (path);

  info = g_hash_table_lookup (ref_model->priv->node_hash, iter->user_data);
  if (!info)
    g_print ("%-16s ref_count=0\n", path_str);
  else
    g_print ("%-16s ref_count=%d\n", path_str, info->ref_count);

  g_free (path_str);
}

static void
gtk_tree_model_ref_count_dump_recurse (GtkTreeModelRefCount *ref_model,
                                       GtkTreeIter          *iter)
{
  do
    {
      GtkTreeIter child;

      dump_iter (ref_model, iter);

      if (gtk_tree_model_iter_children (GTK_TREE_MODEL (ref_model),
                                        &child, iter))
        gtk_tree_model_ref_count_dump_recurse (ref_model, &child);
    }
  while (gtk_tree_model_iter_next (GTK_TREE_MODEL (ref_model), iter));
}

void
gtk_tree_model_ref_count_dump (GtkTreeModelRefCount *ref_model)
{
  GtkTreeIter iter;

  if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (ref_model), &iter))
    return;

  gtk_tree_model_ref_count_dump_recurse (ref_model, &iter);
}

static gboolean
check_iter (GtkTreeModelRefCount *ref_model,
            GtkTreeIter          *iter,
            int                   expected_ref_count,
            gboolean              may_assert)
{
  NodeInfo *info;

  if (may_assert)
    g_assert_true (gtk_tree_store_iter_is_valid (GTK_TREE_STORE (ref_model), iter));

  info = g_hash_table_lookup (ref_model->priv->node_hash, iter->user_data);
  if (!info)
    {
      if (expected_ref_count == 0)
        return TRUE;
      else
        {
          if (may_assert)
            g_error ("Expected ref count %d, but node has never been referenced.", expected_ref_count);
          return FALSE;
        }
    }

  if (may_assert)
    {
      if (expected_ref_count == 0)
        g_assert_cmpint (expected_ref_count, ==, info->ref_count);
      else
        g_assert_cmpint (expected_ref_count, <=, info->ref_count);
    }

  return expected_ref_count == info->ref_count;
}

gboolean
gtk_tree_model_ref_count_check_level (GtkTreeModelRefCount *ref_model,
                                      GtkTreeIter          *parent,
                                      int                   expected_ref_count,
                                      gboolean              recurse,
                                      gboolean              may_assert)
{
  GtkTreeIter iter;

  if (!gtk_tree_model_iter_children (GTK_TREE_MODEL (ref_model),
                                     &iter, parent))
    return TRUE;

  do
    {
      if (!check_iter (ref_model, &iter, expected_ref_count, may_assert))
        return FALSE;

      if (recurse &&
          gtk_tree_model_iter_has_child (GTK_TREE_MODEL (ref_model), &iter))
        {
          if (!gtk_tree_model_ref_count_check_level (ref_model, &iter,
                                                     expected_ref_count,
                                                     recurse, may_assert))
            return FALSE;
        }
    }
  while (gtk_tree_model_iter_next (GTK_TREE_MODEL (ref_model), &iter));

  return TRUE;
}

gboolean
gtk_tree_model_ref_count_check_node (GtkTreeModelRefCount *ref_model,
                                     GtkTreeIter          *iter,
                                     int                   expected_ref_count,
                                     gboolean              may_assert)
{
  return check_iter (ref_model, iter, expected_ref_count, may_assert);
}
