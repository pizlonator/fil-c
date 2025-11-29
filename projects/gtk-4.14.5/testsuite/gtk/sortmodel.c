/* Extensive GtkTreeModelSort tests.
 * Copyright (C) 2009,2011  Kristian Rietveld  <kris@gtk.org>
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

#include <gtk/gtk.h>

#include "treemodel.h"
#include "gtktreemodelrefcount.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static void
ref_count_single_level (void)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkTreeModel *sort_model;
  GtkWidget *tree_view;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);

  assert_root_level_unreferenced (ref_model);

  sort_model = gtk_tree_model_sort_new_with_model (model);
  tree_view = gtk_tree_view_new_with_model (sort_model);

  assert_entire_model_referenced (ref_model, 1);

  g_object_unref (g_object_ref_sink (tree_view));

  assert_entire_model_unreferenced (ref_model);

  g_object_unref (sort_model);
  g_object_unref (ref_model);
}

static void
ref_count_two_levels (void)
{
  GtkTreeIter parent1, parent2, iter;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkTreeModel *sort_model;
  GtkWidget *tree_view;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  gtk_tree_store_append (GTK_TREE_STORE (model), &parent1, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &parent2, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, &parent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, &parent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, &parent2);

  assert_entire_model_unreferenced (ref_model);

  sort_model = gtk_tree_model_sort_new_with_model (model);
  tree_view = gtk_tree_view_new_with_model (sort_model);

  assert_root_level_referenced (ref_model, 1);
  assert_node_ref_count (ref_model, &iter, 0);

  gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

  assert_node_ref_count (ref_model, &parent1, 1);
  assert_node_ref_count (ref_model, &parent2, 2);
  assert_node_ref_count (ref_model, &iter, 1);

  gtk_tree_view_collapse_all (GTK_TREE_VIEW (tree_view));

  assert_node_ref_count (ref_model, &parent1, 1);
  assert_node_ref_count (ref_model, &parent2, 2);
  assert_node_ref_count (ref_model, &iter, 0);

  gtk_tree_model_sort_clear_cache (GTK_TREE_MODEL_SORT (sort_model));

  assert_root_level_referenced (ref_model, 1);
  assert_node_ref_count (ref_model, &iter, 0);

  g_object_unref (g_object_ref_sink (tree_view));

  assert_entire_model_unreferenced (ref_model);

  g_object_unref (sort_model);
  g_object_unref (ref_model);
}

static void
ref_count_three_levels (void)
{
  GtkTreeIter grandparent1, grandparent2, parent1, parent2;
  GtkTreeIter iter_parent1, iter_parent2;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkTreeModel *sort_model;
  GtkTreePath *path;
  GtkWidget *tree_view;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  /* + grandparent1
   * + grandparent2
   *   + parent1
   *     + iter_parent1
   *   + parent2
   *     + iter_parent2
   *     + iter_parent2
   */

  gtk_tree_store_append (GTK_TREE_STORE (model), &grandparent1, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &grandparent2, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &parent1, &grandparent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter_parent1, &parent1);
  gtk_tree_store_append (GTK_TREE_STORE (model), &parent2, &grandparent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter_parent2, &parent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter_parent2, &parent2);

  assert_entire_model_unreferenced (ref_model);

  sort_model = gtk_tree_model_sort_new_with_model (model);
  tree_view = gtk_tree_view_new_with_model (sort_model);

  assert_root_level_referenced (ref_model, 1);
  assert_node_ref_count (ref_model, &parent1, 0);
  assert_node_ref_count (ref_model, &parent2, 0);
  assert_level_unreferenced (ref_model, &parent1);
  assert_level_unreferenced (ref_model, &parent2);

  path = gtk_tree_path_new_from_indices (1, -1);
  gtk_tree_view_expand_row (GTK_TREE_VIEW (tree_view), path, FALSE);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 2);
  assert_node_ref_count (ref_model, &parent1, 1);
  assert_node_ref_count (ref_model, &parent2, 1);
  assert_node_ref_count (ref_model, &iter_parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent2, 0);

  gtk_tree_view_expand_row (GTK_TREE_VIEW (tree_view), path, TRUE);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 2);
  assert_node_ref_count (ref_model, &parent1, 2);
  assert_node_ref_count (ref_model, &parent2, 2);
  assert_node_ref_count (ref_model, &iter_parent1, 1);
  assert_node_ref_count (ref_model, &iter_parent2, 1);

  gtk_tree_view_collapse_all (GTK_TREE_VIEW (tree_view));

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 2);
  assert_node_ref_count (ref_model, &parent1, 1);
  assert_node_ref_count (ref_model, &parent2, 1);
  assert_node_ref_count (ref_model, &iter_parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent2, 0);

  gtk_tree_model_sort_clear_cache (GTK_TREE_MODEL_SORT (sort_model));

  assert_root_level_referenced (ref_model, 1);
  assert_node_ref_count (ref_model, &parent1, 0);
  assert_node_ref_count (ref_model, &parent2, 0);

  gtk_tree_view_expand_row (GTK_TREE_VIEW (tree_view), path, FALSE);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 2);
  assert_node_ref_count (ref_model, &parent1, 1);
  assert_node_ref_count (ref_model, &parent2, 1);
  assert_node_ref_count (ref_model, &iter_parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent2, 0);

  gtk_tree_path_append_index (path, 1);
  gtk_tree_view_expand_row (GTK_TREE_VIEW (tree_view), path, FALSE);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 2);
  assert_node_ref_count (ref_model, &parent1, 1);
  assert_node_ref_count (ref_model, &parent2, 2);
  assert_node_ref_count (ref_model, &iter_parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent2, 1);

  gtk_tree_view_collapse_row (GTK_TREE_VIEW (tree_view), path);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 2);
  assert_node_ref_count (ref_model, &parent1, 1);
  assert_node_ref_count (ref_model, &parent2, 2);
  assert_node_ref_count (ref_model, &iter_parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent2, 0);

  gtk_tree_model_sort_clear_cache (GTK_TREE_MODEL_SORT (sort_model));

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 2);
  assert_node_ref_count (ref_model, &parent1, 1);
  assert_node_ref_count (ref_model, &parent2, 1);
  assert_node_ref_count (ref_model, &iter_parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent2, 0);

  gtk_tree_path_up (path);
  gtk_tree_view_collapse_row (GTK_TREE_VIEW (tree_view), path);
  gtk_tree_path_free (path);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 2);
  assert_node_ref_count (ref_model, &parent1, 0);
  assert_node_ref_count (ref_model, &parent2, 0);
  assert_node_ref_count (ref_model, &iter_parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent2, 0);

  gtk_tree_model_sort_clear_cache (GTK_TREE_MODEL_SORT (sort_model));

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 1);
  assert_node_ref_count (ref_model, &parent1, 0);
  assert_node_ref_count (ref_model, &parent2, 0);
  assert_node_ref_count (ref_model, &iter_parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent2, 0);

  g_object_unref (g_object_ref_sink (tree_view));

  assert_entire_model_unreferenced (ref_model);

  g_object_unref (sort_model);
  g_object_unref (ref_model);
}

static void
ref_count_delete_row (void)
{
  GtkTreeIter grandparent1, grandparent2, parent1, parent2;
  GtkTreeIter iter_parent1, iter_parent2;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkTreeModel *sort_model;
  GtkTreePath *path;
  GtkWidget *tree_view;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  /* + grandparent1
   * + grandparent2
   *   + parent1
   *     + iter_parent1
   *   + parent2
   *     + iter_parent2
   *     + iter_parent2
   */

  gtk_tree_store_append (GTK_TREE_STORE (model), &grandparent1, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &grandparent2, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &parent1, &grandparent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter_parent1, &parent1);
  gtk_tree_store_append (GTK_TREE_STORE (model), &parent2, &grandparent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter_parent2, &parent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter_parent2, &parent2);

  assert_entire_model_unreferenced (ref_model);

  sort_model = gtk_tree_model_sort_new_with_model (model);
  tree_view = gtk_tree_view_new_with_model (sort_model);

  assert_root_level_referenced (ref_model, 1);
  assert_node_ref_count (ref_model, &parent1, 0);
  assert_node_ref_count (ref_model, &parent2, 0);
  assert_level_unreferenced (ref_model, &parent1);
  assert_level_unreferenced (ref_model, &parent2);

  path = gtk_tree_path_new_from_indices (1, -1);
  gtk_tree_view_expand_row (GTK_TREE_VIEW (tree_view), path, TRUE);
  gtk_tree_path_free (path);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 2);
  assert_node_ref_count (ref_model, &parent1, 2);
  assert_node_ref_count (ref_model, &parent2, 2);
  assert_node_ref_count (ref_model, &iter_parent1, 1);
  assert_node_ref_count (ref_model, &iter_parent2, 1);

  gtk_tree_store_remove (GTK_TREE_STORE (model), &iter_parent2);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 2);
  assert_node_ref_count (ref_model, &parent1, 2);
  assert_level_referenced (ref_model, 1, &parent1);
  assert_node_ref_count (ref_model, &parent2, 2);
  assert_level_referenced (ref_model, 1, &parent2);

  gtk_tree_store_remove (GTK_TREE_STORE (model), &parent1);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 2);
  assert_node_ref_count (ref_model, &parent2, 2);
  assert_level_referenced (ref_model, 1, &parent2);

  gtk_tree_store_remove (GTK_TREE_STORE (model), &grandparent2);

  assert_node_ref_count (ref_model, &grandparent1, 1);

  gtk_tree_model_sort_clear_cache (GTK_TREE_MODEL_SORT (sort_model));

  assert_node_ref_count (ref_model, &grandparent1, 1);

  g_object_unref (g_object_ref_sink (tree_view));

  assert_entire_model_unreferenced (ref_model);

  g_object_unref (sort_model);
  g_object_unref (ref_model);
}

static void
ref_count_cleanup (void)
{
  GtkTreeIter grandparent1, grandparent2, parent1, parent2;
  GtkTreeIter iter_parent1, iter_parent2;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkTreeModel *sort_model;
  GtkWidget *tree_view;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  /* + grandparent1
   * + grandparent2
   *   + parent1
   *     + iter_parent1
   *   + parent2
   *     + iter_parent2
   *     + iter_parent2
   */

  gtk_tree_store_append (GTK_TREE_STORE (model), &grandparent1, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &grandparent2, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &parent1, &grandparent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter_parent1, &parent1);
  gtk_tree_store_append (GTK_TREE_STORE (model), &parent2, &grandparent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter_parent2, &parent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter_parent2, &parent2);

  sort_model = gtk_tree_model_sort_new_with_model (model);
  tree_view = gtk_tree_view_new_with_model (sort_model);

  gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

  g_object_unref (g_object_ref_sink (tree_view));

  assert_node_ref_count (ref_model, &grandparent1, 0);
  assert_node_ref_count (ref_model, &grandparent2, 1);
  assert_node_ref_count (ref_model, &parent1, 1);
  assert_node_ref_count (ref_model, &parent2, 1);
  assert_node_ref_count (ref_model, &iter_parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent2, 0);

  gtk_tree_model_sort_clear_cache (GTK_TREE_MODEL_SORT (sort_model));

  assert_entire_model_unreferenced (ref_model);

  g_object_unref (sort_model);
  g_object_unref (ref_model);
}

static void
ref_count_row_ref (void)
{
  GtkTreeIter grandparent1, grandparent2, parent1, parent2;
  GtkTreeIter iter_parent1, iter_parent2;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkTreeModel *sort_model;
  GtkWidget *tree_view;
  GtkTreePath *path;
  GtkTreeRowReference *row_ref;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  /* + grandparent1
   * + grandparent2
   *   + parent1
   *     + iter_parent1
   *   + parent2
   *     + iter_parent2
   *     + iter_parent2
   */

  gtk_tree_store_append (GTK_TREE_STORE (model), &grandparent1, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &grandparent2, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &parent1, &grandparent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter_parent1, &parent1);
  gtk_tree_store_append (GTK_TREE_STORE (model), &parent2, &grandparent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter_parent2, &parent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter_parent2, &parent2);

  sort_model = gtk_tree_model_sort_new_with_model (model);
  tree_view = gtk_tree_view_new_with_model (sort_model);

  path = gtk_tree_path_new_from_indices (1, 1, 1, -1);
  row_ref = gtk_tree_row_reference_new (sort_model, path);
  gtk_tree_path_free (path);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  /* Referenced because the node is visible, its child level is built
   * and referenced by the row ref.
   */
  assert_node_ref_count (ref_model, &grandparent2, 3);
  assert_node_ref_count (ref_model, &parent1, 0);
  /* Referenced by the row ref and because its child level is built. */
  assert_node_ref_count (ref_model, &parent2, 2);
  assert_node_ref_count (ref_model, &iter_parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent2, 1);

  gtk_tree_row_reference_free (row_ref);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 2);
  assert_node_ref_count (ref_model, &parent1, 0);
  assert_node_ref_count (ref_model, &parent2, 1);
  assert_node_ref_count (ref_model, &iter_parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent2, 0);

  path = gtk_tree_path_new_from_indices (1, 1, 1, -1);
  row_ref = gtk_tree_row_reference_new (sort_model, path);
  gtk_tree_path_free (path);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  /* Referenced because the node is visible, its child level is built
   * and referenced by the row ref.
   */
  assert_node_ref_count (ref_model, &grandparent2, 3);
  assert_node_ref_count (ref_model, &parent1, 0);
  /* Referenced by the row ref and because its child level is built. */
  assert_node_ref_count (ref_model, &parent2, 2);
  assert_node_ref_count (ref_model, &iter_parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent2, 1);

  gtk_tree_store_remove (GTK_TREE_STORE (model), &parent2);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 1);
  assert_node_ref_count (ref_model, &parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent1, 0);

  gtk_tree_row_reference_free (row_ref);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 1);
  assert_node_ref_count (ref_model, &parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent1, 0);

  g_object_unref (g_object_ref_sink (tree_view));
  g_object_unref (sort_model);

  assert_entire_model_unreferenced (ref_model);

  g_object_unref (ref_model);
}

static void
ref_count_reorder_single (void)
{
  GtkTreeIter iter1, iter2, iter3, iter4, iter5;
  GtkTreeIter siter1, siter2, siter3, siter4, siter5;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkTreeModel *sort_model;
  GtkWidget *tree_view;
  GType column_types[] = { G_TYPE_INT };

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  gtk_tree_store_set_column_types (GTK_TREE_STORE (model), 1,
                                   column_types);

  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &iter1, NULL, 0,
                                     0, 30, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &iter2, NULL, 1,
                                     0, 40, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &iter3, NULL, 2,
                                     0, 10, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &iter4, NULL, 3,
                                     0, 20, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &iter5, NULL, 4,
                                     0, 60, -1);

  assert_root_level_unreferenced (ref_model);

  sort_model = gtk_tree_model_sort_new_with_model (model);
  tree_view = gtk_tree_view_new_with_model (sort_model);

  assert_entire_model_referenced (ref_model, 1);

  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &siter1, &iter1);
  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &siter2, &iter2);
  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &siter3, &iter3);
  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &siter4, &iter4);
  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &siter5, &iter5);

  gtk_tree_model_ref_node (GTK_TREE_MODEL (sort_model), &siter1);
  gtk_tree_model_ref_node (GTK_TREE_MODEL (sort_model), &siter1);

  gtk_tree_model_ref_node (GTK_TREE_MODEL (sort_model), &siter3);
  gtk_tree_model_ref_node (GTK_TREE_MODEL (sort_model), &siter3);
  gtk_tree_model_ref_node (GTK_TREE_MODEL (sort_model), &siter3);

  gtk_tree_model_ref_node (GTK_TREE_MODEL (sort_model), &siter5);

  assert_node_ref_count (ref_model, &iter1, 3);
  assert_node_ref_count (ref_model, &iter2, 1);
  assert_node_ref_count (ref_model, &iter3, 4);
  assert_node_ref_count (ref_model, &iter4, 1);
  assert_node_ref_count (ref_model, &iter5, 2);

  /* Sort */
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model),
                                        0, GTK_SORT_ASCENDING);

  assert_node_ref_count (ref_model, &iter1, 3);
  assert_node_ref_count (ref_model, &iter2, 1);
  assert_node_ref_count (ref_model, &iter3, 4);
  assert_node_ref_count (ref_model, &iter4, 1);
  assert_node_ref_count (ref_model, &iter5, 2);

  /* Re-translate the iters after sorting */
  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &siter1, &iter1);
  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &siter2, &iter2);
  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &siter3, &iter3);
  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &siter4, &iter4);
  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &siter5, &iter5);

  gtk_tree_model_unref_node (GTK_TREE_MODEL (sort_model), &siter1);
  gtk_tree_model_unref_node (GTK_TREE_MODEL (sort_model), &siter1);

  gtk_tree_model_unref_node (GTK_TREE_MODEL (sort_model), &siter3);
  gtk_tree_model_unref_node (GTK_TREE_MODEL (sort_model), &siter3);
  gtk_tree_model_unref_node (GTK_TREE_MODEL (sort_model), &siter3);

  gtk_tree_model_unref_node (GTK_TREE_MODEL (sort_model), &siter5);

  assert_entire_model_referenced (ref_model, 1);

  g_object_unref (g_object_ref_sink (tree_view));
  g_object_unref (sort_model);

  assert_entire_model_unreferenced (ref_model);

  g_object_unref (ref_model);
}

static void
ref_count_reorder_two (void)
{
  GtkTreeIter iter1, iter2, iter3, iter4, iter5;
  GtkTreeIter citer1, citer2, citer3, citer4, citer5;
  GtkTreeIter siter1, siter2, siter3, siter4, siter5;
  GtkTreeIter sciter1, sciter2, sciter3, sciter4, sciter5;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkTreeModel *sort_model;
  GtkWidget *tree_view;
  GType column_types[] = { G_TYPE_INT };

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  gtk_tree_store_set_column_types (GTK_TREE_STORE (model), 1,
                                   column_types);

  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &iter1, NULL, 0,
                                     0, 30, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &iter2, NULL, 1,
                                     0, 40, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &iter3, NULL, 2,
                                     0, 10, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &iter4, NULL, 3,
                                     0, 20, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &iter5, NULL, 4,
                                     0, 60, -1);

  /* Child level */
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &citer1, &iter1, 0,
                                     0, 30, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &citer2, &iter1, 1,
                                     0, 40, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &citer3, &iter1, 2,
                                     0, 10, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &citer4, &iter1, 3,
                                     0, 20, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &citer5, &iter1, 4,
                                     0, 60, -1);

  assert_root_level_unreferenced (ref_model);

  sort_model = gtk_tree_model_sort_new_with_model (model);
  tree_view = gtk_tree_view_new_with_model (sort_model);
  gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

  assert_node_ref_count (ref_model, &iter1, 2);
  assert_node_ref_count (ref_model, &iter2, 1);
  assert_node_ref_count (ref_model, &iter3, 1);
  assert_node_ref_count (ref_model, &iter4, 1);
  assert_node_ref_count (ref_model, &iter5, 1);

  assert_level_referenced (ref_model, 1, &iter1);

  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &siter1, &iter1);
  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &siter2, &iter2);
  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &siter3, &iter3);
  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &siter4, &iter4);
  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &siter5, &iter5);

  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &sciter1, &citer1);
  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &sciter2, &citer2);
  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &sciter3, &citer3);
  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &sciter4, &citer4);
  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &sciter5, &citer5);

  gtk_tree_model_ref_node (GTK_TREE_MODEL (sort_model), &siter1);
  gtk_tree_model_ref_node (GTK_TREE_MODEL (sort_model), &siter1);

  gtk_tree_model_ref_node (GTK_TREE_MODEL (sort_model), &siter3);
  gtk_tree_model_ref_node (GTK_TREE_MODEL (sort_model), &siter3);
  gtk_tree_model_ref_node (GTK_TREE_MODEL (sort_model), &siter3);

  gtk_tree_model_ref_node (GTK_TREE_MODEL (sort_model), &siter5);

  assert_node_ref_count (ref_model, &iter1, 4);
  assert_node_ref_count (ref_model, &iter2, 1);
  assert_node_ref_count (ref_model, &iter3, 4);
  assert_node_ref_count (ref_model, &iter4, 1);
  assert_node_ref_count (ref_model, &iter5, 2);

  gtk_tree_model_ref_node (GTK_TREE_MODEL (sort_model), &sciter3);
  gtk_tree_model_ref_node (GTK_TREE_MODEL (sort_model), &sciter3);

  gtk_tree_model_ref_node (GTK_TREE_MODEL (sort_model), &sciter5);
  gtk_tree_model_ref_node (GTK_TREE_MODEL (sort_model), &sciter5);
  gtk_tree_model_ref_node (GTK_TREE_MODEL (sort_model), &sciter5);

  gtk_tree_model_ref_node (GTK_TREE_MODEL (sort_model), &sciter1);

  assert_node_ref_count (ref_model, &citer1, 2);
  assert_node_ref_count (ref_model, &citer2, 1);
  assert_node_ref_count (ref_model, &citer3, 3);
  assert_node_ref_count (ref_model, &citer4, 1);
  assert_node_ref_count (ref_model, &citer5, 4);

  /* Sort */
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model),
                                        0, GTK_SORT_ASCENDING);

  assert_node_ref_count (ref_model, &iter1, 4);
  assert_node_ref_count (ref_model, &iter2, 1);
  assert_node_ref_count (ref_model, &iter3, 4);
  assert_node_ref_count (ref_model, &iter4, 1);
  assert_node_ref_count (ref_model, &iter5, 2);

  assert_node_ref_count (ref_model, &citer1, 2);
  assert_node_ref_count (ref_model, &citer2, 1);
  assert_node_ref_count (ref_model, &citer3, 3);
  assert_node_ref_count (ref_model, &citer4, 1);
  assert_node_ref_count (ref_model, &citer5, 4);

  /* Re-translate the iters after sorting */
  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &siter1, &iter1);
  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &siter2, &iter2);
  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &siter3, &iter3);
  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &siter4, &iter4);
  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &siter5, &iter5);

  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &sciter1, &citer1);
  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &sciter2, &citer2);
  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &sciter3, &citer3);
  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &sciter4, &citer4);
  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &sciter5, &citer5);

  gtk_tree_model_unref_node (GTK_TREE_MODEL (sort_model), &siter1);
  gtk_tree_model_unref_node (GTK_TREE_MODEL (sort_model), &siter1);

  gtk_tree_model_unref_node (GTK_TREE_MODEL (sort_model), &siter3);
  gtk_tree_model_unref_node (GTK_TREE_MODEL (sort_model), &siter3);
  gtk_tree_model_unref_node (GTK_TREE_MODEL (sort_model), &siter3);

  gtk_tree_model_unref_node (GTK_TREE_MODEL (sort_model), &siter5);

  assert_node_ref_count (ref_model, &iter1, 2);
  assert_node_ref_count (ref_model, &iter2, 1);
  assert_node_ref_count (ref_model, &iter3, 1);
  assert_node_ref_count (ref_model, &iter4, 1);
  assert_node_ref_count (ref_model, &iter5, 1);

  gtk_tree_model_unref_node (GTK_TREE_MODEL (sort_model), &sciter3);
  gtk_tree_model_unref_node (GTK_TREE_MODEL (sort_model), &sciter3);

  gtk_tree_model_unref_node (GTK_TREE_MODEL (sort_model), &sciter5);
  gtk_tree_model_unref_node (GTK_TREE_MODEL (sort_model), &sciter5);
  gtk_tree_model_unref_node (GTK_TREE_MODEL (sort_model), &sciter5);

  gtk_tree_model_unref_node (GTK_TREE_MODEL (sort_model), &sciter1);

  assert_level_referenced (ref_model, 1, &iter1);

  g_object_unref (g_object_ref_sink (tree_view));
  g_object_unref (sort_model);

  assert_entire_model_unreferenced (ref_model);

  g_object_unref (ref_model);
}

static void
check_sort_order (GtkTreeModel *sort_model,
                  GtkSortType   sort_order,
                  const char   *parent_path)
{
  int prev_value;
  GtkTreeIter siter;

  if (!parent_path)
    gtk_tree_model_get_iter_first (sort_model, &siter);
  else
    {
      GtkTreePath *path;

      path = gtk_tree_path_new_from_string (parent_path);
      gtk_tree_path_append_index (path, 0);

      gtk_tree_model_get_iter (sort_model, &siter, path);

      gtk_tree_path_free (path);
    }

  if (sort_order == GTK_SORT_ASCENDING)
    prev_value = -1;
  else
    prev_value = INT_MAX;

  do
    {
      int value;

      gtk_tree_model_get (sort_model, &siter, 0, &value, -1);
      if (sort_order == GTK_SORT_ASCENDING)
        g_assert_cmpint (prev_value, <=, value);
      else
        g_assert_cmpint (prev_value, >=, value);

      prev_value = value;
    }
  while (gtk_tree_model_iter_next (sort_model, &siter));
}

static void
rows_reordered_single_level (void)
{
  GtkTreeIter iter1, iter2, iter3, iter4, iter5;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkTreeModel *sort_model;
  GtkWidget *tree_view;
  SignalMonitor *monitor;
  GtkTreePath *path;
  GType column_types[] = { G_TYPE_INT };
  int order[][5] =
    {
        { 2, 3, 0, 1, 4 },
        { 4, 3, 2, 1, 0 },
        { 2, 1, 4, 3, 0 }
    };

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  gtk_tree_store_set_column_types (GTK_TREE_STORE (model), 1,
                                   column_types);

  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &iter1, NULL, 0,
                                     0, 30, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &iter2, NULL, 1,
                                     0, 40, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &iter3, NULL, 2,
                                     0, 10, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &iter4, NULL, 3,
                                     0, 20, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &iter5, NULL, 4,
                                     0, 60, -1);

  sort_model = gtk_tree_model_sort_new_with_model (model);
  tree_view = gtk_tree_view_new_with_model (sort_model);

  monitor = signal_monitor_new (sort_model);

  /* Sort */
  path = gtk_tree_path_new ();
  signal_monitor_append_signal_reordered (monitor,
                                          ROWS_REORDERED,
                                          path, order[0], 5);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model),
                                        0, GTK_SORT_ASCENDING);
  signal_monitor_assert_is_empty (monitor);
  check_sort_order (sort_model, GTK_SORT_ASCENDING, NULL);

  signal_monitor_append_signal_reordered (monitor,
                                          ROWS_REORDERED,
                                          path, order[1], 5);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model),
                                        0, GTK_SORT_DESCENDING);
  signal_monitor_assert_is_empty (monitor);
  check_sort_order (sort_model, GTK_SORT_DESCENDING, NULL);

  signal_monitor_append_signal_reordered (monitor,
                                          ROWS_REORDERED,
                                          path, order[2], 5);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model),
                                        GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
                                        GTK_SORT_ASCENDING);
  signal_monitor_assert_is_empty (monitor);

  gtk_tree_path_free (path);
  signal_monitor_free (monitor);

  g_object_unref (g_object_ref_sink (tree_view));
  g_object_unref (sort_model);

  assert_entire_model_unreferenced (ref_model);

  g_object_unref (ref_model);
}

static void
rows_reordered_two_levels (void)
{
  GtkTreeIter iter1, iter2, iter3, iter4, iter5;
  GtkTreeIter citer1, citer2, citer3, citer4, citer5;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkTreeModel *sort_model;
  GtkWidget *tree_view;
  SignalMonitor *monitor;
  GtkTreePath *path, *child_path;
  GType column_types[] = { G_TYPE_INT };
  int order[][5] =
    {
        { 2, 3, 0, 1, 4 },
        { 4, 3, 2, 1, 0 },
        { 2, 1, 4, 3, 0 }
    };

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  gtk_tree_store_set_column_types (GTK_TREE_STORE (model), 1,
                                   column_types);

  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &iter1, NULL, 0,
                                     0, 30, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &iter2, NULL, 1,
                                     0, 40, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &iter3, NULL, 2,
                                     0, 10, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &iter4, NULL, 3,
                                     0, 20, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &iter5, NULL, 4,
                                     0, 60, -1);

  /* Child level */
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &citer1, &iter1, 0,
                                     0, 30, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &citer2, &iter1, 1,
                                     0, 40, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &citer3, &iter1, 2,
                                     0, 10, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &citer4, &iter1, 3,
                                     0, 20, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &citer5, &iter1, 4,
                                     0, 60, -1);

  sort_model = gtk_tree_model_sort_new_with_model (model);
  tree_view = gtk_tree_view_new_with_model (sort_model);
  gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

  monitor = signal_monitor_new (sort_model);

  /* Sort */
  path = gtk_tree_path_new ();
  child_path = gtk_tree_path_new_from_indices (2, -1);
  signal_monitor_append_signal_reordered (monitor,
                                          ROWS_REORDERED,
                                          path, order[0], 5);
  signal_monitor_append_signal_reordered (monitor,
                                          ROWS_REORDERED,
                                          child_path, order[0], 5);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model),
                                        0, GTK_SORT_ASCENDING);
  signal_monitor_assert_is_empty (monitor);
  check_sort_order (sort_model, GTK_SORT_ASCENDING, NULL);
  /* The parent node of the child level moved due to sorting */
  check_sort_order (sort_model, GTK_SORT_ASCENDING, "2");

  signal_monitor_append_signal_reordered (monitor,
                                          ROWS_REORDERED,
                                          path, order[1], 5);
  signal_monitor_append_signal_reordered (monitor,
                                          ROWS_REORDERED,
                                          child_path, order[1], 5);
  gtk_tree_path_free (child_path);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model),
                                        0, GTK_SORT_DESCENDING);
  signal_monitor_assert_is_empty (monitor);
  check_sort_order (sort_model, GTK_SORT_DESCENDING, NULL);
  /* The parent node of the child level moved due to sorting */
  check_sort_order (sort_model, GTK_SORT_DESCENDING, "2");

  child_path = gtk_tree_path_new_from_indices (0, -1);
  signal_monitor_append_signal_reordered (monitor,
                                          ROWS_REORDERED,
                                          path, order[2], 5);
  signal_monitor_append_signal_reordered (monitor,
                                          ROWS_REORDERED,
                                          child_path, order[2], 5);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model),
                                        GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
                                        GTK_SORT_ASCENDING);
  signal_monitor_assert_is_empty (monitor);

  gtk_tree_path_free (path);
  gtk_tree_path_free (child_path);
  signal_monitor_free (monitor);

  g_object_unref (g_object_ref_sink (tree_view));
  g_object_unref (sort_model);

  g_object_unref (ref_model);
}

static void
sorted_insert (void)
{
  GtkTreeIter iter1, iter2, iter3, iter4, iter5, new_iter;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkTreeModel *sort_model;
  GtkWidget *tree_view;
  SignalMonitor *monitor;
  GtkTreePath *path;
  GType column_types[] = { G_TYPE_INT };
  int order0[] = { 1, 2, 3, 0, 4, 5, 6 };

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  gtk_tree_store_set_column_types (GTK_TREE_STORE (model), 1,
                                   column_types);

  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &iter1, NULL, 0,
                                     0, 30, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &iter2, NULL, 1,
                                     0, 40, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &iter3, NULL, 2,
                                     0, 10, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &iter4, NULL, 3,
                                     0, 20, -1);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &iter5, NULL, 4,
                                     0, 60, -1);

  sort_model = gtk_tree_model_sort_new_with_model (model);
  tree_view = gtk_tree_view_new_with_model (sort_model);

  /* Sort */
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model),
                                        0, GTK_SORT_ASCENDING);
  check_sort_order (sort_model, GTK_SORT_ASCENDING, NULL);

  monitor = signal_monitor_new (sort_model);

  /* Insert a new item */
  signal_monitor_append_signal (monitor, ROW_INSERTED, "4");
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model), &new_iter, NULL,
                                     5, 0, 50, -1);
  signal_monitor_assert_is_empty (monitor);
  check_sort_order (sort_model, GTK_SORT_ASCENDING, NULL);

  /* Sort the tree sort and append a new item */
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
                                        0, GTK_SORT_ASCENDING);
  check_sort_order (model, GTK_SORT_ASCENDING, NULL);

  path = gtk_tree_path_new ();
  signal_monitor_append_signal (monitor, ROW_INSERTED, "0");
  signal_monitor_append_signal_reordered (monitor,
                                          ROWS_REORDERED,
                                          path, order0, 7);
  signal_monitor_append_signal (monitor, ROW_CHANGED, "3");
  gtk_tree_store_append (GTK_TREE_STORE (model), &new_iter, NULL);
  gtk_tree_store_set (GTK_TREE_STORE (model), &new_iter, 0, 35, -1);
  check_sort_order (model, GTK_SORT_ASCENDING, NULL);
  check_sort_order (sort_model, GTK_SORT_ASCENDING, NULL);

  gtk_tree_path_free (path);
  signal_monitor_free (monitor);

  g_object_unref (g_object_ref_sink (tree_view));
  g_object_unref (sort_model);

  g_object_unref (ref_model);
}


static void
specific_bug_300089 (void)
{
  /* Test case for GNOME Bugzilla bug 300089.  Written by
   * Matthias Clasen.
   */
  GtkTreeModel *sort_model, *child_model;
  GtkTreePath *path;
  GtkTreeIter iter, iter2, sort_iter;

  /*http://bugzilla.gnome.org/show_bug.cgi?id=300089 */

  child_model = GTK_TREE_MODEL (gtk_tree_store_new (1, G_TYPE_STRING));

  gtk_tree_store_append (GTK_TREE_STORE (child_model), &iter, NULL);
  gtk_tree_store_set (GTK_TREE_STORE (child_model), &iter, 0, "A", -1);
  gtk_tree_store_append (GTK_TREE_STORE (child_model), &iter, NULL);
  gtk_tree_store_set (GTK_TREE_STORE (child_model), &iter, 0, "B", -1);

  gtk_tree_store_append (GTK_TREE_STORE (child_model), &iter2, &iter);
  gtk_tree_store_set (GTK_TREE_STORE (child_model), &iter2, 0, "D", -1);
  gtk_tree_store_append (GTK_TREE_STORE (child_model), &iter2, &iter);
  gtk_tree_store_set (GTK_TREE_STORE (child_model), &iter2, 0, "E", -1);

  gtk_tree_store_append (GTK_TREE_STORE (child_model), &iter, NULL);
  gtk_tree_store_set (GTK_TREE_STORE (child_model), &iter, 0, "C", -1);


  sort_model = GTK_TREE_MODEL (gtk_tree_model_sort_new_with_model (child_model));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model),
                                        0, GTK_SORT_ASCENDING);

  path = gtk_tree_path_new_from_indices (1, 1, -1);

  /* make sure a level is constructed */
  gtk_tree_model_get_iter (sort_model, &sort_iter, path);

  /* change the "E" row in a way that causes it to change position */ 
  gtk_tree_model_get_iter (child_model, &iter, path);
  gtk_tree_store_set (GTK_TREE_STORE (child_model), &iter, 0, "A", -1);

  gtk_tree_path_free (path);
}

static void
specific_bug_364946 (void)
{
  /* This is a test case for GNOME Bugzilla bug 364946.  It was written
   * by Andreas Koehler.
   */
  GtkTreeStore *store;
  GtkTreeIter a, aa, aaa, aab, iter;
  GtkTreeModel *s_model;

  /*http://bugzilla.gnome.org/show_bug.cgi?id=364946 */

  store = gtk_tree_store_new (1, G_TYPE_STRING);

  gtk_tree_store_append (store, &a, NULL);
  gtk_tree_store_set (store, &a, 0, "0", -1);

  gtk_tree_store_append (store, &aa, &a);
  gtk_tree_store_set (store, &aa, 0, "0:0", -1);

  gtk_tree_store_append (store, &aaa, &aa);
  gtk_tree_store_set (store, &aaa, 0, "0:0:0", -1);

  gtk_tree_store_append (store, &aab, &aa);
  gtk_tree_store_set (store, &aab, 0, "0:0:1", -1);

  s_model = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (s_model), 0,
                                        GTK_SORT_ASCENDING);

  gtk_tree_model_get_iter_from_string (s_model, &iter, "0:0:0");

  gtk_tree_store_set (store, &aaa, 0, "0:0:0", -1);
  gtk_tree_store_remove (store, &aaa);
  gtk_tree_store_remove (store, &aab);

  gtk_tree_model_sort_clear_cache (GTK_TREE_MODEL_SORT (s_model));
}

static void
iter_test (GtkTreeModel *model)
{
  GtkTreeIter a, b;

  g_assert_true (gtk_tree_model_get_iter_first (model, &a));

  g_assert_true (gtk_tree_model_iter_next (model, &a));
  g_assert_true (gtk_tree_model_iter_next (model, &a));
  b = a;
  g_assert_false (gtk_tree_model_iter_next (model, &b));

  g_assert_true (gtk_tree_model_iter_previous (model, &a));
  g_assert_true (gtk_tree_model_iter_previous (model, &a));
  b = a;
  g_assert_false (gtk_tree_model_iter_previous (model, &b));
}

static void
specific_bug_674587 (void)
{
  GtkListStore *l;
  GtkTreeStore *t;
  GtkTreeModel *m;
  GtkTreeIter a;

  l = gtk_list_store_new (1, G_TYPE_STRING);

  gtk_list_store_append (l, &a);
  gtk_list_store_set (l, &a, 0, "0", -1);
  gtk_list_store_append (l, &a);
  gtk_list_store_set (l, &a, 0, "1", -1);
  gtk_list_store_append (l, &a);
  gtk_list_store_set (l, &a, 0, "2", -1);

  iter_test (GTK_TREE_MODEL (l));

  g_object_unref (l);

  t = gtk_tree_store_new (1, G_TYPE_STRING);

  gtk_tree_store_append (t, &a, NULL);
  gtk_tree_store_set (t, &a, 0, "0", -1);
  gtk_tree_store_append (t, &a, NULL);
  gtk_tree_store_set (t, &a, 0, "1", -1);
  gtk_tree_store_append (t, &a, NULL);
  gtk_tree_store_set (t, &a, 0, "2", -1);

  iter_test (GTK_TREE_MODEL (t));

  m = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (t));

  iter_test (m);

  g_object_unref (t);
  g_object_unref (m);
}

static void
row_deleted_cb (GtkTreeModel *tree_model,
    GtkTreePath  *path,
    guint *count)
{
  *count = *count + 1;
}

static void
specific_bug_698846 (void)
{
  GtkListStore *store;
  GtkTreeModel *sorted;
  GtkTreeIter iter;
  guint count = 0;

  /*http://bugzilla.gnome.org/show_bug.cgi?id=698846 */

  store = gtk_list_store_new (1, G_TYPE_STRING);
  sorted = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (store));

  gtk_list_store_insert_with_values (store, &iter, 0, 0, "a", -1);
  gtk_list_store_insert_with_values (store, &iter, 1, 0, "b", -1);

  g_signal_connect (sorted, "row-deleted", G_CALLBACK (row_deleted_cb), &count);

  gtk_list_store_clear (store);

  g_assert_cmpuint (count, ==, 2);
}

static int
sort_func (GtkTreeModel *model,
           GtkTreeIter  *a,
           GtkTreeIter  *b,
           gpointer      data)
{
  return 0;
}

static int column_changed;

static void
sort_column_changed (GtkTreeSortable *sortable)
{
  column_changed++;
}

static void
sort_column_change (void)
{
  GtkListStore *store;
  GtkTreeModel *sorted;
  int col;
  GtkSortType order;
  gboolean ret;

  /*http://bugzilla.gnome.org/show_bug.cgi?id=792459 */

  store = gtk_list_store_new (1, G_TYPE_STRING);
  sorted = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (store));

  column_changed = 0;
  g_signal_connect (sorted, "sort-column-changed", G_CALLBACK (sort_column_changed), NULL);

  g_assert_false (gtk_tree_sortable_has_default_sort_func (GTK_TREE_SORTABLE (sorted)));
  gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (sorted), sort_func, NULL, NULL);
  g_assert_true (gtk_tree_sortable_has_default_sort_func (GTK_TREE_SORTABLE (sorted)));

  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (sorted), 0, sort_func, NULL, NULL);

  ret = gtk_tree_sortable_get_sort_column_id (GTK_TREE_SORTABLE (sorted), &col, &order);
  g_assert_cmpint (column_changed, ==, 0);
  g_assert_false (ret);
  g_assert_cmpint (col, ==, GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID);
  g_assert_cmpint (order, ==, GTK_SORT_ASCENDING);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sorted),
                                        GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_DESCENDING);
  ret = gtk_tree_sortable_get_sort_column_id (GTK_TREE_SORTABLE (sorted), &col, &order);
  g_assert_cmpint (column_changed, ==, 1);
  g_assert_false (ret);
  g_assert_cmpint (col, ==, GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID);
  g_assert_cmpint (order, ==, GTK_SORT_DESCENDING);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sorted),
                                        GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_DESCENDING);
  ret = gtk_tree_sortable_get_sort_column_id (GTK_TREE_SORTABLE (sorted), &col, &order);
  g_assert_cmpint (column_changed, ==, 1);
  g_assert_false (ret);
  g_assert_cmpint (col, ==, GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID);
  g_assert_cmpint (order, ==, GTK_SORT_DESCENDING);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sorted),
                                        0, GTK_SORT_DESCENDING);
  ret = gtk_tree_sortable_get_sort_column_id (GTK_TREE_SORTABLE (sorted), &col, &order);
  g_assert_cmpint (column_changed, ==, 2);
  g_assert_true (ret);
  g_assert_cmpint (col, ==, 0);
  g_assert_cmpint (order, ==, GTK_SORT_DESCENDING);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sorted),
                                        GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING);
  ret = gtk_tree_sortable_get_sort_column_id (GTK_TREE_SORTABLE (sorted), &col, &order);
  g_assert_cmpint (column_changed, ==, 3);
  g_assert_false (ret);
  g_assert_cmpint (col, ==, GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID);
  g_assert_cmpint (order, ==, GTK_SORT_ASCENDING);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sorted),
                                        GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID, GTK_SORT_ASCENDING);
  ret = gtk_tree_sortable_get_sort_column_id (GTK_TREE_SORTABLE (sorted), &col, &order);
  g_assert_cmpint (column_changed, ==, 4);
  g_assert_false (ret);
  g_assert_cmpint (col, ==, GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID);
  g_assert_cmpint (order, ==, GTK_SORT_ASCENDING);
}

/* main */

void
register_sort_model_tests (void)
{
  g_test_add_func ("/TreeModelSort/ref-count/single-level",
                   ref_count_single_level);
  g_test_add_func ("/TreeModelSort/ref-count/two-levels",
                   ref_count_two_levels);
  g_test_add_func ("/TreeModelSort/ref-count/three-levels",
                   ref_count_three_levels);
  g_test_add_func ("/TreeModelSort/ref-count/delete-row",
                   ref_count_delete_row);
  g_test_add_func ("/TreeModelSort/ref-count/cleanup",
                   ref_count_cleanup);
  g_test_add_func ("/TreeModelSort/ref-count/row-ref",
                   ref_count_row_ref);
  g_test_add_func ("/TreeModelSort/ref-count/reorder/single-level",
                   ref_count_reorder_single);
  g_test_add_func ("/TreeModelSort/ref-count/reorder/two-levels",
                   ref_count_reorder_two);

  g_test_add_func ("/TreeModelSort/rows-reordered/single-level",
                   rows_reordered_single_level);
  g_test_add_func ("/TreeModelSort/rows-reordered/two-levels",
                   rows_reordered_two_levels);
  g_test_add_func ("/TreeModelSort/sorted-insert",
                   sorted_insert);

  g_test_add_func ("/TreeModelSort/specific/bug-300089",
                   specific_bug_300089);
  g_test_add_func ("/TreeModelSort/specific/bug-364946",
                   specific_bug_364946);
  g_test_add_func ("/TreeModelSort/specific/bug-674587",
                   specific_bug_674587);
  g_test_add_func ("/TreeModelSort/specific/bug-698846",
                   specific_bug_698846);
  g_test_add_func ("/TreeModelSort/specific/bug-792459",
                   sort_column_change);
}

