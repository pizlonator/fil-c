/* GtkTreeModel ref counting tests
 * Copyright (C) 2011  Kristian Rietveld  <kris@gtk.org>
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

#include "gtktreemodelrefcount.h"
#include "treemodel.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/* And the tests themselves */

static void
test_list_no_reference (void)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);

  assert_root_level_unreferenced (ref_model);

  g_object_unref (ref_model);
}

static void
test_list_reference_during_creation (void)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkWidget *tree_view;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);
  tree_view = gtk_tree_view_new_with_model (model);

  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);

  assert_root_level_referenced (ref_model, 1);

  g_object_unref (g_object_ref_sink (tree_view));

  assert_root_level_unreferenced (ref_model);

  g_object_unref (ref_model);
}

static void
test_list_reference_after_creation (void)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkWidget *tree_view;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  assert_root_level_unreferenced (ref_model);

  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);

  tree_view = gtk_tree_view_new_with_model (model);

  assert_root_level_referenced (ref_model, 1);

  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);

  assert_root_level_referenced (ref_model, 1);

  g_object_unref (g_object_ref_sink (tree_view));

  assert_root_level_unreferenced (ref_model);

  g_object_unref (ref_model);
}

static void
test_list_reference_reordered (void)
{
  GtkTreeIter iter1, iter2, iter3, iter4, iter5;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkWidget *tree_view;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  assert_root_level_unreferenced (ref_model);

  gtk_tree_store_append (GTK_TREE_STORE (model), &iter1, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter2, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter3, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter4, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter5, NULL);

  tree_view = gtk_tree_view_new_with_model (model);

  assert_root_level_referenced (ref_model, 1);

  gtk_tree_store_move_after (GTK_TREE_STORE (model),
                             &iter1, &iter5);

  assert_root_level_referenced (ref_model, 1);

  gtk_tree_store_move_after (GTK_TREE_STORE (model),
                             &iter3, &iter4);

  assert_root_level_referenced (ref_model, 1);

  g_object_unref (g_object_ref_sink (tree_view));

  assert_root_level_unreferenced (ref_model);

  g_object_unref (ref_model);
}


static void
test_tree_no_reference (void)
{
  GtkTreeIter iter, child;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);

  assert_entire_model_unreferenced (ref_model);

  g_object_unref (ref_model);
}

static void
test_tree_reference_during_creation (void)
{
  GtkTreeIter iter, child;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkWidget *tree_view;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);
  tree_view = gtk_tree_view_new_with_model (model);

  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);

  assert_root_level_referenced (ref_model, 1);
  assert_not_entire_model_referenced (ref_model, 1);
  assert_level_unreferenced (ref_model, &child);

  g_object_unref (g_object_ref_sink (tree_view));

  assert_entire_model_unreferenced (ref_model);

  g_object_unref (ref_model);
}

static void
test_tree_reference_after_creation (void)
{
  GtkTreeIter iter, child;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkWidget *tree_view;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);

  assert_entire_model_unreferenced (ref_model);

  tree_view = gtk_tree_view_new_with_model (model);

  assert_root_level_referenced (ref_model, 1);
  assert_not_entire_model_referenced (ref_model, 1);
  assert_level_unreferenced (ref_model, &child);

  g_object_unref (g_object_ref_sink (tree_view));

  assert_entire_model_unreferenced (ref_model);

  g_object_unref (ref_model);
}

static void
test_tree_reference_reordered (void)
{
  GtkTreeIter parent;
  GtkTreeIter iter1, iter2, iter3, iter4, iter5;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkWidget *tree_view;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  assert_root_level_unreferenced (ref_model);

  gtk_tree_store_append (GTK_TREE_STORE (model), &parent, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter1, &parent);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter2, &parent);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter3, &parent);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter4, &parent);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter5, &parent);

  tree_view = gtk_tree_view_new_with_model (model);
  gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

  assert_entire_model_referenced (ref_model, 1);

  gtk_tree_store_move_after (GTK_TREE_STORE (model),
                             &iter1, &iter5);

  assert_entire_model_referenced (ref_model, 1);

  gtk_tree_store_move_after (GTK_TREE_STORE (model),
                             &iter3, &iter4);

  assert_entire_model_referenced (ref_model, 1);

  g_object_unref (g_object_ref_sink (tree_view));

  assert_entire_model_unreferenced (ref_model);

  g_object_unref (ref_model);
}

static void
test_tree_reference_expand_all (void)
{
  GtkTreeIter iter, child;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkWidget *tree_view;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);

  assert_entire_model_unreferenced (ref_model);

  tree_view = gtk_tree_view_new_with_model (model);

  assert_root_level_referenced (ref_model, 1);
  assert_not_entire_model_referenced (ref_model, 1);
  assert_level_unreferenced (ref_model, &child);

  gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

  assert_entire_model_referenced (ref_model, 1);

  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);

  assert_root_level_referenced (ref_model, 1);
  assert_not_entire_model_referenced (ref_model, 1);
  assert_level_unreferenced (ref_model, &child);

  g_object_unref (g_object_ref_sink (tree_view));

  assert_entire_model_unreferenced (ref_model);

  g_object_unref (ref_model);
}

static void
test_tree_reference_collapse_all (void)
{
  GtkTreeIter iter, child;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkWidget *tree_view;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);

  assert_entire_model_unreferenced (ref_model);

  tree_view = gtk_tree_view_new_with_model (model);
  gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

  assert_entire_model_referenced (ref_model, 1);

  gtk_tree_view_collapse_all (GTK_TREE_VIEW (tree_view));

  assert_root_level_referenced (ref_model, 1);
  assert_not_entire_model_referenced (ref_model, 1);
  assert_level_unreferenced (ref_model, &child);

  g_object_unref (g_object_ref_sink (tree_view));

  assert_entire_model_unreferenced (ref_model);

  g_object_unref (ref_model);
}

static void
test_tree_reference_expand_collapse (void)
{
  GtkTreeIter parent1, parent2, child;
  GtkTreePath *path1, *path2;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkWidget *tree_view;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);
  tree_view = gtk_tree_view_new_with_model (model);

  gtk_tree_store_append (GTK_TREE_STORE (model), &parent1, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &parent1);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &parent1);
  gtk_tree_store_append (GTK_TREE_STORE (model), &parent2, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &parent2, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &parent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child, &parent2);

  path1 = gtk_tree_model_get_path (model, &parent1);
  path2 = gtk_tree_model_get_path (model, &parent2);

  assert_level_unreferenced (ref_model, &parent1);
  assert_level_unreferenced (ref_model, &parent2);

  gtk_tree_view_expand_row (GTK_TREE_VIEW (tree_view), path1, FALSE);

  assert_level_referenced (ref_model, 1, &parent1);
  assert_level_unreferenced (ref_model, &parent2);

  gtk_tree_view_collapse_row (GTK_TREE_VIEW (tree_view), path1);

  assert_level_unreferenced (ref_model, &parent1);
  assert_level_unreferenced (ref_model, &parent2);

  gtk_tree_view_expand_row (GTK_TREE_VIEW (tree_view), path2, FALSE);

  assert_level_unreferenced (ref_model, &parent1);
  assert_level_referenced (ref_model, 1, &parent2);

  gtk_tree_view_collapse_row (GTK_TREE_VIEW (tree_view), path2);

  assert_level_unreferenced (ref_model, &parent1);
  assert_level_unreferenced (ref_model, &parent2);

  gtk_tree_view_expand_row (GTK_TREE_VIEW (tree_view), path2, FALSE);

  assert_level_unreferenced (ref_model, &parent1);
  assert_level_referenced (ref_model, 1, &parent2);

  gtk_tree_view_expand_row (GTK_TREE_VIEW (tree_view), path1, FALSE);

  assert_level_referenced (ref_model, 1, &parent1);
  assert_level_referenced (ref_model, 1, &parent2);

  gtk_tree_path_free (path1);
  gtk_tree_path_free (path2);

  g_object_unref (g_object_ref_sink (tree_view));
  g_object_unref (ref_model);
}

static void
test_row_reference_list (void)
{
  GtkTreeIter iter0, iter1, iter2;
  GtkTreePath *path;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkTreeRowReference *row_ref;
  GtkWidget *tree_view;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  gtk_tree_store_append (GTK_TREE_STORE (model), &iter0, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter1, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter2, NULL);

  assert_root_level_unreferenced (ref_model);

  /* create and remove a row ref and check reference counts */
  path = gtk_tree_path_new_from_indices (1, -1);
  row_ref = gtk_tree_row_reference_new (model, path);

  assert_node_ref_count (ref_model, &iter0, 0);
  assert_node_ref_count (ref_model, &iter1, 1);
  assert_node_ref_count (ref_model, &iter2, 0);

  gtk_tree_row_reference_free (row_ref);

  assert_root_level_unreferenced (ref_model);

  /* the same, but then also with a tree view monitoring the model */
  tree_view = gtk_tree_view_new_with_model (model);

  assert_root_level_referenced (ref_model, 1);

  row_ref = gtk_tree_row_reference_new (model, path);

  assert_node_ref_count (ref_model, &iter0, 1);
  assert_node_ref_count (ref_model, &iter1, 2);
  assert_node_ref_count (ref_model, &iter2, 1);

  g_object_unref (g_object_ref_sink (tree_view));

  assert_node_ref_count (ref_model, &iter0, 0);
  assert_node_ref_count (ref_model, &iter1, 1);
  assert_node_ref_count (ref_model, &iter2, 0);

  gtk_tree_row_reference_free (row_ref);

  assert_root_level_unreferenced (ref_model);

  gtk_tree_path_free (path);

  g_object_unref (ref_model);
}

static void
test_row_reference_list_remove (void)
{
  GtkTreeIter iter0, iter1, iter2;
  GtkTreePath *path;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkTreeRowReference *row_ref;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  gtk_tree_store_append (GTK_TREE_STORE (model), &iter0, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter1, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter2, NULL);

  assert_root_level_unreferenced (ref_model);

  /* test creating the row reference and then removing the node */
  path = gtk_tree_path_new_from_indices (1, -1);
  row_ref = gtk_tree_row_reference_new (model, path);

  assert_node_ref_count (ref_model, &iter0, 0);
  assert_node_ref_count (ref_model, &iter1, 1);
  assert_node_ref_count (ref_model, &iter2, 0);

  gtk_tree_store_remove (GTK_TREE_STORE (model), &iter1);

  assert_root_level_unreferenced (ref_model);

  gtk_tree_row_reference_free (row_ref);

  assert_root_level_unreferenced (ref_model);

  /* test creating a row ref, removing another node and then removing
   * the row ref node.
   */
  row_ref = gtk_tree_row_reference_new (model, path);

  assert_node_ref_count (ref_model, &iter0, 0);
  assert_node_ref_count (ref_model, &iter2, 1);

  gtk_tree_store_remove (GTK_TREE_STORE (model), &iter0);

  assert_root_level_referenced (ref_model, 1);

  gtk_tree_store_remove (GTK_TREE_STORE (model), &iter2);

  g_assert_false (gtk_tree_model_get_iter_first (model, &iter0));

  gtk_tree_row_reference_free (row_ref);

  gtk_tree_path_free (path);

  g_object_unref (ref_model);
}

static void
test_row_reference_tree (void)
{
  GtkTreeIter iter0, iter1, iter2;
  GtkTreeIter child0, child1, child2;
  GtkTreeIter grandchild0, grandchild1, grandchild2;
  GtkTreePath *path;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkTreeRowReference *row_ref, *row_ref1;
  GtkWidget *tree_view;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  gtk_tree_store_append (GTK_TREE_STORE (model), &iter0, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child0, &iter0);
  gtk_tree_store_append (GTK_TREE_STORE (model), &grandchild0, &child0);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter1, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child1, &iter1);
  gtk_tree_store_append (GTK_TREE_STORE (model), &grandchild1, &child1);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter2, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child2, &iter2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &grandchild2, &child2);

  assert_entire_model_unreferenced (ref_model);

  /* create and remove a row ref and check reference counts */
  path = gtk_tree_path_new_from_indices (1, 0, 0, -1);
  row_ref = gtk_tree_row_reference_new (model, path);
  gtk_tree_path_free (path);

  assert_node_ref_count (ref_model, &iter0, 0);
  assert_node_ref_count (ref_model, &child0, 0);
  assert_node_ref_count (ref_model, &grandchild0, 0);
  assert_node_ref_count (ref_model, &iter1, 1);
  assert_node_ref_count (ref_model, &child1, 1);
  assert_node_ref_count (ref_model, &grandchild1, 1);
  assert_node_ref_count (ref_model, &iter2, 0);
  assert_node_ref_count (ref_model, &child2, 0);
  assert_node_ref_count (ref_model, &grandchild2, 0);

  gtk_tree_row_reference_free (row_ref);

  assert_entire_model_unreferenced (ref_model);

  /* again, with path 1:1 */
  path = gtk_tree_path_new_from_indices (1, 0, -1);
  row_ref = gtk_tree_row_reference_new (model, path);
  gtk_tree_path_free (path);

  assert_node_ref_count (ref_model, &iter0, 0);
  assert_node_ref_count (ref_model, &child0, 0);
  assert_node_ref_count (ref_model, &grandchild0, 0);
  assert_node_ref_count (ref_model, &iter1, 1);
  assert_node_ref_count (ref_model, &child1, 1);
  assert_node_ref_count (ref_model, &grandchild1, 0);
  assert_node_ref_count (ref_model, &iter2, 0);
  assert_node_ref_count (ref_model, &child2, 0);
  assert_node_ref_count (ref_model, &grandchild2, 0);

  gtk_tree_row_reference_free (row_ref);

  assert_entire_model_unreferenced (ref_model);

  /* both row refs existent at once and also with a tree view monitoring
   * the model
   */
  tree_view = gtk_tree_view_new_with_model (model);

  assert_root_level_referenced (ref_model, 1);

  path = gtk_tree_path_new_from_indices (1, 0, 0, -1);
  row_ref = gtk_tree_row_reference_new (model, path);
  gtk_tree_path_free (path);

  assert_node_ref_count (ref_model, &iter0, 1);
  assert_node_ref_count (ref_model, &child0, 0);
  assert_node_ref_count (ref_model, &grandchild0, 0);
  assert_node_ref_count (ref_model, &iter1, 2);
  assert_node_ref_count (ref_model, &child1, 1);
  assert_node_ref_count (ref_model, &grandchild1, 1);
  assert_node_ref_count (ref_model, &iter2, 1);
  assert_node_ref_count (ref_model, &child2, 0);
  assert_node_ref_count (ref_model, &grandchild2, 0);

  path = gtk_tree_path_new_from_indices (1, 0, -1);
  row_ref1 = gtk_tree_row_reference_new (model, path);
  gtk_tree_path_free (path);

  assert_node_ref_count (ref_model, &iter0, 1);
  assert_node_ref_count (ref_model, &child0, 0);
  assert_node_ref_count (ref_model, &grandchild0, 0);
  assert_node_ref_count (ref_model, &iter1, 3);
  assert_node_ref_count (ref_model, &child1, 2);
  assert_node_ref_count (ref_model, &grandchild1, 1);
  assert_node_ref_count (ref_model, &iter2, 1);
  assert_node_ref_count (ref_model, &child2, 0);
  assert_node_ref_count (ref_model, &grandchild2, 0);

  gtk_tree_row_reference_free (row_ref);

  assert_node_ref_count (ref_model, &iter0, 1);
  assert_node_ref_count (ref_model, &child0, 0);
  assert_node_ref_count (ref_model, &grandchild0, 0);
  assert_node_ref_count (ref_model, &iter1, 2);
  assert_node_ref_count (ref_model, &child1, 1);
  assert_node_ref_count (ref_model, &grandchild1, 0);
  assert_node_ref_count (ref_model, &iter2, 1);
  assert_node_ref_count (ref_model, &child2, 0);
  assert_node_ref_count (ref_model, &grandchild2, 0);

  g_object_unref (g_object_ref_sink (tree_view));

  assert_node_ref_count (ref_model, &iter0, 0);
  assert_node_ref_count (ref_model, &child0, 0);
  assert_node_ref_count (ref_model, &grandchild0, 0);
  assert_node_ref_count (ref_model, &iter1, 1);
  assert_node_ref_count (ref_model, &child1, 1);
  assert_node_ref_count (ref_model, &grandchild1, 0);
  assert_node_ref_count (ref_model, &iter2, 0);
  assert_node_ref_count (ref_model, &child2, 0);
  assert_node_ref_count (ref_model, &grandchild2, 0);

  gtk_tree_row_reference_free (row_ref1);

  assert_root_level_unreferenced (ref_model);

  g_object_unref (ref_model);
}

static void
test_row_reference_tree_remove (void)
{
  GtkTreeIter iter0, iter1, iter2;
  GtkTreeIter child0, child1, child2;
  GtkTreeIter grandchild0, grandchild1, grandchild2;
  GtkTreePath *path;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkTreeRowReference *row_ref, *row_ref1, *row_ref2;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  gtk_tree_store_append (GTK_TREE_STORE (model), &iter0, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child0, &iter0);
  gtk_tree_store_append (GTK_TREE_STORE (model), &grandchild0, &child0);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter1, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child1, &iter1);
  gtk_tree_store_append (GTK_TREE_STORE (model), &grandchild1, &child1);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter2, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child2, &iter2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &grandchild2, &child2);

  assert_entire_model_unreferenced (ref_model);

  path = gtk_tree_path_new_from_indices (1, 0, 0, -1);
  row_ref = gtk_tree_row_reference_new (model, path);
  gtk_tree_path_free (path);

  path = gtk_tree_path_new_from_indices (2, 0, -1);
  row_ref1 = gtk_tree_row_reference_new (model, path);
  gtk_tree_path_free (path);

  path = gtk_tree_path_new_from_indices (2, -1);
  row_ref2 = gtk_tree_row_reference_new (model, path);
  gtk_tree_path_free (path);

  assert_node_ref_count (ref_model, &iter0, 0);
  assert_node_ref_count (ref_model, &child0, 0);
  assert_node_ref_count (ref_model, &grandchild0, 0);
  assert_node_ref_count (ref_model, &iter1, 1);
  assert_node_ref_count (ref_model, &child1, 1);
  assert_node_ref_count (ref_model, &grandchild1, 1);
  assert_node_ref_count (ref_model, &iter2, 2);
  assert_node_ref_count (ref_model, &child2, 1);
  assert_node_ref_count (ref_model, &grandchild2, 0);

  gtk_tree_store_remove (GTK_TREE_STORE (model), &grandchild1);

  assert_node_ref_count (ref_model, &iter0, 0);
  assert_node_ref_count (ref_model, &child0, 0);
  assert_node_ref_count (ref_model, &grandchild0, 0);
  assert_node_ref_count (ref_model, &iter1, 0);
  assert_node_ref_count (ref_model, &child1, 0);
  assert_node_ref_count (ref_model, &iter2, 2);
  assert_node_ref_count (ref_model, &child2, 1);
  assert_node_ref_count (ref_model, &grandchild2, 0);

  gtk_tree_store_remove (GTK_TREE_STORE (model), &child2);

  assert_node_ref_count (ref_model, &iter0, 0);
  assert_node_ref_count (ref_model, &child0, 0);
  assert_node_ref_count (ref_model, &grandchild0, 0);
  assert_node_ref_count (ref_model, &iter1, 0);
  assert_node_ref_count (ref_model, &child1, 0);
  assert_node_ref_count (ref_model, &iter2, 1);

  gtk_tree_store_remove (GTK_TREE_STORE (model), &iter2);

  assert_entire_model_unreferenced (ref_model);

  gtk_tree_row_reference_free (row_ref);
  gtk_tree_row_reference_free (row_ref1);
  gtk_tree_row_reference_free (row_ref2);

  g_object_unref (ref_model);
}

static void
test_row_reference_tree_remove_ancestor (void)
{
  GtkTreeIter iter0, iter1, iter2;
  GtkTreeIter child0, child1, child2;
  GtkTreeIter grandchild0, grandchild1, grandchild2;
  GtkTreePath *path;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkTreeRowReference *row_ref, *row_ref1;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  gtk_tree_store_append (GTK_TREE_STORE (model), &iter0, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child0, &iter0);
  gtk_tree_store_append (GTK_TREE_STORE (model), &grandchild0, &child0);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter1, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child1, &iter1);
  gtk_tree_store_append (GTK_TREE_STORE (model), &grandchild1, &child1);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter2, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child2, &iter2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &grandchild2, &child2);

  assert_entire_model_unreferenced (ref_model);

  path = gtk_tree_path_new_from_indices (1, 0, 0, -1);
  row_ref = gtk_tree_row_reference_new (model, path);
  gtk_tree_path_free (path);

  path = gtk_tree_path_new_from_indices (2, 0, -1);
  row_ref1 = gtk_tree_row_reference_new (model, path);
  gtk_tree_path_free (path);

  assert_node_ref_count (ref_model, &iter0, 0);
  assert_node_ref_count (ref_model, &child0, 0);
  assert_node_ref_count (ref_model, &grandchild0, 0);
  assert_node_ref_count (ref_model, &iter1, 1);
  assert_node_ref_count (ref_model, &child1, 1);
  assert_node_ref_count (ref_model, &grandchild1, 1);
  assert_node_ref_count (ref_model, &iter2, 1);
  assert_node_ref_count (ref_model, &child2, 1);
  assert_node_ref_count (ref_model, &grandchild2, 0);

  gtk_tree_store_remove (GTK_TREE_STORE (model), &child1);

  assert_node_ref_count (ref_model, &iter0, 0);
  assert_node_ref_count (ref_model, &child0, 0);
  assert_node_ref_count (ref_model, &grandchild0, 0);
  assert_node_ref_count (ref_model, &iter1, 0);
  assert_node_ref_count (ref_model, &iter2, 1);
  assert_node_ref_count (ref_model, &child2, 1);
  assert_node_ref_count (ref_model, &grandchild2, 0);

  gtk_tree_store_remove (GTK_TREE_STORE (model), &iter2);

  assert_entire_model_unreferenced (ref_model);

  gtk_tree_row_reference_free (row_ref);
  gtk_tree_row_reference_free (row_ref1);

  g_object_unref (ref_model);
}

static void
test_row_reference_tree_expand (void)
{
  GtkTreeIter iter0, iter1, iter2;
  GtkTreeIter child0, child1, child2;
  GtkTreeIter grandchild0, grandchild1, grandchild2;
  GtkTreePath *path;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkTreeRowReference *row_ref, *row_ref1, *row_ref2;
  GtkWidget *tree_view;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);
  tree_view = gtk_tree_view_new_with_model (model);

  gtk_tree_store_append (GTK_TREE_STORE (model), &iter0, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child0, &iter0);
  gtk_tree_store_append (GTK_TREE_STORE (model), &grandchild0, &child0);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter1, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child1, &iter1);
  gtk_tree_store_append (GTK_TREE_STORE (model), &grandchild1, &child1);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter2, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &child2, &iter2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &grandchild2, &child2);

  assert_root_level_referenced (ref_model, 1);

  gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

  assert_entire_model_referenced (ref_model, 1);

  path = gtk_tree_path_new_from_indices (1, 0, 0, -1);
  row_ref = gtk_tree_row_reference_new (model, path);
  gtk_tree_path_free (path);

  path = gtk_tree_path_new_from_indices (2, 0, -1);
  row_ref1 = gtk_tree_row_reference_new (model, path);
  gtk_tree_path_free (path);

  path = gtk_tree_path_new_from_indices (2, -1);
  row_ref2 = gtk_tree_row_reference_new (model, path);
  gtk_tree_path_free (path);

  assert_node_ref_count (ref_model, &iter0, 1);
  assert_node_ref_count (ref_model, &child0, 1);
  assert_node_ref_count (ref_model, &grandchild0, 1);
  assert_node_ref_count (ref_model, &iter1, 2);
  assert_node_ref_count (ref_model, &child1, 2);
  assert_node_ref_count (ref_model, &grandchild1, 2);
  assert_node_ref_count (ref_model, &iter2, 3);
  assert_node_ref_count (ref_model, &child2, 2);
  assert_node_ref_count (ref_model, &grandchild2, 1);

  gtk_tree_store_remove (GTK_TREE_STORE (model), &grandchild1);

  assert_node_ref_count (ref_model, &iter0, 1);
  assert_node_ref_count (ref_model, &child0, 1);
  assert_node_ref_count (ref_model, &grandchild0, 1);
  assert_node_ref_count (ref_model, &iter1, 1);
  assert_node_ref_count (ref_model, &child1, 1);
  assert_node_ref_count (ref_model, &iter2, 3);
  assert_node_ref_count (ref_model, &child2, 2);
  assert_node_ref_count (ref_model, &grandchild2, 1);

  gtk_tree_store_remove (GTK_TREE_STORE (model), &child2);

  assert_node_ref_count (ref_model, &iter0, 1);
  assert_node_ref_count (ref_model, &child0, 1);
  assert_node_ref_count (ref_model, &grandchild0, 1);
  assert_node_ref_count (ref_model, &iter1, 1);
  assert_node_ref_count (ref_model, &child1, 1);
  assert_node_ref_count (ref_model, &iter2, 2);

  gtk_tree_view_collapse_all (GTK_TREE_VIEW (tree_view));

  assert_node_ref_count (ref_model, &iter0, 1);
  assert_node_ref_count (ref_model, &child0, 0);
  assert_node_ref_count (ref_model, &grandchild0, 0);
  assert_node_ref_count (ref_model, &iter1, 1);
  assert_node_ref_count (ref_model, &child1, 0);
  assert_node_ref_count (ref_model, &iter2, 2);

  gtk_tree_store_remove (GTK_TREE_STORE (model), &iter2);

  assert_node_ref_count (ref_model, &iter0, 1);
  assert_node_ref_count (ref_model, &child0, 0);
  assert_node_ref_count (ref_model, &grandchild0, 0);
  assert_node_ref_count (ref_model, &iter1, 1);
  assert_node_ref_count (ref_model, &child1, 0);

  gtk_tree_row_reference_free (row_ref);
  gtk_tree_row_reference_free (row_ref1);
  gtk_tree_row_reference_free (row_ref2);

  g_object_unref (g_object_ref_sink (tree_view));
  g_object_unref (ref_model);
}

void
register_model_ref_count_tests (void)
{
  /* lists (though based on GtkTreeStore) */
  g_test_add_func ("/TreeModel/ref-count/list/no-reference",
                   test_list_no_reference);
  g_test_add_func ("/TreeModel/ref-count/list/reference-during-creation",
                   test_list_reference_during_creation);
  g_test_add_func ("/TreeModel/ref-count/list/reference-after-creation",
                   test_list_reference_after_creation);
  g_test_add_func ("/TreeModel/ref-count/list/reference-reordered",
                   test_list_reference_reordered);

  /* trees */
  g_test_add_func ("/TreeModel/ref-count/tree/no-reference",
                   test_tree_no_reference);
  g_test_add_func ("/TreeModel/ref-count/tree/reference-during-creation",
                   test_tree_reference_during_creation);
  g_test_add_func ("/TreeModel/ref-count/tree/reference-after-creation",
                   test_tree_reference_after_creation);
  g_test_add_func ("/TreeModel/ref-count/tree/expand-all",
                   test_tree_reference_expand_all);
  g_test_add_func ("/TreeModel/ref-count/tree/collapse-all",
                   test_tree_reference_collapse_all);
  g_test_add_func ("/TreeModel/ref-count/tree/expand-collapse",
                   test_tree_reference_expand_collapse);
  g_test_add_func ("/TreeModel/ref-count/tree/reference-reordered",
                   test_tree_reference_reordered);

  /* row references */
  g_test_add_func ("/TreeModel/ref-count/row-reference/list",
                   test_row_reference_list);
  g_test_add_func ("/TreeModel/ref-count/row-reference/list-remove",
                   test_row_reference_list_remove);
  g_test_add_func ("/TreeModel/ref-count/row-reference/tree",
                   test_row_reference_tree);
  g_test_add_func ("/TreeModel/ref-count/row-reference/tree-remove",
                   test_row_reference_tree_remove);
  g_test_add_func ("/TreeModel/ref-count/row-reference/tree-remove-ancestor",
                   test_row_reference_tree_remove_ancestor);
  g_test_add_func ("/TreeModel/ref-count/row-reference/tree-expand",
                   test_row_reference_tree_expand);
}
