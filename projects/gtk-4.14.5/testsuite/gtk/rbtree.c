/* GtkTreeRBTree tests.
 *
 * Copyright (C) 2011, Red Hat, Inc.
 * Authors: Benjamin Otte <otte@gnome.org>
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

#include <locale.h>

#include "../../gtk/deprecated/gtktreerbtreeprivate.h"

/* gtk_tree_rbtree_test */

static guint
get_total_count (GtkTreeRBNode *node)
{
  guint child_total = 0;

  child_total += (guint) node->left->total_count;
  child_total += (guint) node->right->total_count;

  if (node->children)
    child_total += (guint) node->children->root->total_count;

  return child_total + 1;
}

static guint
count_total (GtkTreeRBTree *tree,
             GtkTreeRBNode *node)
{
  guint res;

  if (gtk_tree_rbtree_is_nil (node))
    return 0;

  res =
    count_total (tree, node->left) +
    count_total (tree, node->right) +
    (guint) 1 +
    (node->children ? count_total (node->children, node->children->root) : 0);

  if (res != node->total_count)
    g_print ("total count incorrect for node\n");

  if (get_total_count (node) != node->total_count)
    g_error ("Node has incorrect total count %u, should be %u", node->total_count, get_total_count (node));

  return res;
}

static int
_count_nodes (GtkTreeRBTree *tree,
              GtkTreeRBNode *node)
{
  int res;
  if (gtk_tree_rbtree_is_nil (node))
    return 0;

  g_assert_true (node->left);
  g_assert_true (node->right);

  res = (_count_nodes (tree, node->left) +
         _count_nodes (tree, node->right) + 1);

  if (res != node->count)
    g_print ("Tree failed\n");
  return res;
}

static void
gtk_tree_rbtree_test_height (GtkTreeRBTree *tree,
                             GtkTreeRBNode *node)
{
  int computed_offset = 0;

  /* This whole test is sort of a useless truism. */

  if (!gtk_tree_rbtree_is_nil (node->left))
    computed_offset += node->left->offset;

  if (!gtk_tree_rbtree_is_nil (node->right))
    computed_offset += node->right->offset;

  if (node->children && !gtk_tree_rbtree_is_nil (node->children->root))
    computed_offset += node->children->root->offset;

  if (GTK_TREE_RBNODE_GET_HEIGHT (node) + computed_offset != node->offset)
    g_error ("node has broken offset");

  if (!gtk_tree_rbtree_is_nil (node->left))
    gtk_tree_rbtree_test_height (tree, node->left);

  if (!gtk_tree_rbtree_is_nil (node->right))
    gtk_tree_rbtree_test_height (tree, node->right);

  if (node->children && !gtk_tree_rbtree_is_nil (node->children->root))
    gtk_tree_rbtree_test_height (node->children, node->children->root);
}

static void
gtk_tree_rbtree_test_dirty (GtkTreeRBTree *tree,
                            GtkTreeRBNode *node,
                            int            expected_dirtyness)
{
  g_assert_nonnull (node);

  if (expected_dirtyness)
    {
      g_assert_true (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_COLUMN_INVALID) ||
                GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_INVALID) ||
                GTK_TREE_RBNODE_FLAG_SET (node->left, GTK_TREE_RBNODE_DESCENDANTS_INVALID) ||
                GTK_TREE_RBNODE_FLAG_SET (node->right, GTK_TREE_RBNODE_DESCENDANTS_INVALID) ||
                (node->children && GTK_TREE_RBNODE_FLAG_SET (node->children->root, GTK_TREE_RBNODE_DESCENDANTS_INVALID)));
    }
  else
    {
      g_assert_true (!GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_COLUMN_INVALID) &&
                !GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_INVALID));
      if (!gtk_tree_rbtree_is_nil (node->left))
        g_assert_true (!GTK_TREE_RBNODE_FLAG_SET (node->left, GTK_TREE_RBNODE_DESCENDANTS_INVALID));
      if (!gtk_tree_rbtree_is_nil (node->right))
        g_assert_true (!GTK_TREE_RBNODE_FLAG_SET (node->right, GTK_TREE_RBNODE_DESCENDANTS_INVALID));
      if (node->children != NULL)
        g_assert_true (!GTK_TREE_RBNODE_FLAG_SET (node->children->root, GTK_TREE_RBNODE_DESCENDANTS_INVALID));
    }

  if (!gtk_tree_rbtree_is_nil (node->left))
    gtk_tree_rbtree_test_dirty (tree, node->left, GTK_TREE_RBNODE_FLAG_SET (node->left, GTK_TREE_RBNODE_DESCENDANTS_INVALID));
  if (!gtk_tree_rbtree_is_nil (node->right))
    gtk_tree_rbtree_test_dirty (tree, node->right, GTK_TREE_RBNODE_FLAG_SET (node->right, GTK_TREE_RBNODE_DESCENDANTS_INVALID));
  if (node->children != NULL && !gtk_tree_rbtree_is_nil (node->children->root))
    gtk_tree_rbtree_test_dirty (node->children, node->children->root, GTK_TREE_RBNODE_FLAG_SET (node->children->root, GTK_TREE_RBNODE_DESCENDANTS_INVALID));
}

static void gtk_tree_rbtree_test_structure (GtkTreeRBTree *tree);

static guint
gtk_tree_rbtree_test_structure_helper (GtkTreeRBTree *tree,
                                       GtkTreeRBNode *node)
{
  guint left_blacks, right_blacks;

  g_assert_false (gtk_tree_rbtree_is_nil (node));

  g_assert_nonnull (node->left);
  g_assert_nonnull (node->right);
  g_assert_nonnull (node->parent);

  if (!gtk_tree_rbtree_is_nil (node->left))
    {
      g_assert_true (node->left->parent == node);
      left_blacks = gtk_tree_rbtree_test_structure_helper (tree, node->left);
    }
  else
    left_blacks = 0;

  if (!gtk_tree_rbtree_is_nil (node->right))
    {
      g_assert_true (node->right->parent == node);
      right_blacks = gtk_tree_rbtree_test_structure_helper (tree, node->right);
    }
  else
    right_blacks = 0;

  if (node->children != NULL)
    {
      g_assert_true (node->children->parent_tree == tree);
      g_assert_true (node->children->parent_node == node);

      gtk_tree_rbtree_test_structure (node->children);
    }

  g_assert_true (left_blacks == right_blacks);

  return left_blacks + (GTK_TREE_RBNODE_GET_COLOR (node) == GTK_TREE_RBNODE_BLACK ? 1 : 0);
}

static void
gtk_tree_rbtree_test_structure (GtkTreeRBTree *tree)
{
  g_assert_nonnull (tree->root);
  if (gtk_tree_rbtree_is_nil (tree->root))
    return;

  g_assert_true (gtk_tree_rbtree_is_nil (tree->root->parent));
  gtk_tree_rbtree_test_structure_helper (tree, tree->root);
}

static void
gtk_tree_rbtree_test (GtkTreeRBTree *tree)
{
  GtkTreeRBTree *tmp_tree;

  if (tree == NULL)
    return;

  /* Test the entire tree */
  tmp_tree = tree;
  while (tmp_tree->parent_tree)
    tmp_tree = tmp_tree->parent_tree;

  if (gtk_tree_rbtree_is_nil (tmp_tree->root))
    return;

  gtk_tree_rbtree_test_structure (tmp_tree);

  g_assert_true ((_count_nodes (tmp_tree, tmp_tree->root->left) +
             _count_nodes (tmp_tree, tmp_tree->root->right) + 1) == tmp_tree->root->count);

  gtk_tree_rbtree_test_height (tmp_tree, tmp_tree->root);
  gtk_tree_rbtree_test_dirty (tmp_tree, tmp_tree->root, GTK_TREE_RBNODE_FLAG_SET (tmp_tree->root, GTK_TREE_RBNODE_DESCENDANTS_INVALID));
  g_assert_true (count_total (tmp_tree, tmp_tree->root) == tmp_tree->root->total_count);
}

/* gtk_rbtree_print() - unused, for debugging only */

static void
gtk_rbtree_print_node (GtkTreeRBTree *tree,
                       GtkTreeRBNode *node,
                       int            depth)
{
  int i;
  for (i = 0; i < depth; i++)
    g_print ("\t");

  g_print ("(%p - %s) (Offset %d) (Parity %d) (Validity %d%d%d)\n",
           node,
           (GTK_TREE_RBNODE_GET_COLOR (node) == GTK_TREE_RBNODE_BLACK) ? "BLACK" : " RED ",
           node->offset,
           node->total_count,
           (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_DESCENDANTS_INVALID)) ? 1 : 0,
           (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_INVALID)) ? 1 : 0,
           (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_COLUMN_INVALID)) ? 1 : 0);
  if (node->children != NULL)
    {
      g_print ("Looking at child.\n");
      gtk_rbtree_print_node (node->children, node->children->root, depth + 1);
      g_print ("Done looking at child.\n");
    }
  if (!gtk_tree_rbtree_is_nil (node->left))
    {
      gtk_rbtree_print_node (tree, node->left, depth + 1);
    }
  if (!gtk_tree_rbtree_is_nil (node->right))
    {
      gtk_rbtree_print_node (tree, node->right, depth + 1);
    }
}

/* not static so the debugger finds it. */
void gtk_rbtree_print (GtkTreeRBTree *tree);

void
gtk_rbtree_print (GtkTreeRBTree *tree)
{
  g_return_if_fail (tree != NULL);

  if (gtk_tree_rbtree_is_nil (tree->root))
    g_print ("Empty tree...\n");
  else
    gtk_rbtree_print_node (tree, tree->root, 0);
}

/* actual tests */

static guint
append_elements (GtkTreeRBTree *tree,
                 guint          depth,
                 guint          elements_per_depth,
                 gboolean       check,
                 guint          height)
{
  GtkTreeRBNode *node;
  guint i;

  g_assert_cmpint (depth, >, 0);

  node = NULL;
  depth--;

  for (i = 0; i < elements_per_depth; i++)
    {
      node = gtk_tree_rbtree_insert_after (tree, node, ++height, TRUE);
      if (depth)
        {
          node->children = gtk_tree_rbtree_new ();
          node->children->parent_tree = tree;
          node->children->parent_node = node;
          height = append_elements (node->children, depth, elements_per_depth, check, height);
        }
      if (check)
        gtk_tree_rbtree_test (tree);
    }

  return height;
}

static GtkTreeRBTree *
create_rbtree (guint    depth,
               guint    elements_per_depth,
               gboolean check)
{
  GtkTreeRBTree *tree;

  tree = gtk_tree_rbtree_new ();

  append_elements (tree, depth, elements_per_depth, check, 0);

  gtk_tree_rbtree_test (tree);

  return tree;
}

static void
test_create (void)
{
  GtkTreeRBTree *tree;

  tree = create_rbtree (5, 5, TRUE);

  gtk_tree_rbtree_free (tree);
}

static void
test_insert_after (void)
{
  guint i;
  GtkTreeRBTree *tree;
  GtkTreeRBNode *node;

  tree = gtk_tree_rbtree_new ();
  node = NULL;

  for (i = 1; i <= 100; i++)
    {
      node = gtk_tree_rbtree_insert_after (tree, node, i, TRUE);
      gtk_tree_rbtree_test (tree);
      g_assert_cmpint (tree->root->count, ==, i);
      g_assert_cmpint (tree->root->total_count, ==, i);
      g_assert_cmpint (tree->root->offset, ==, i * (i + 1) / 2);
    }

  gtk_tree_rbtree_free (tree);
}

static void
test_insert_before (void)
{
  guint i;
  GtkTreeRBTree *tree;
  GtkTreeRBNode *node;

  tree = gtk_tree_rbtree_new ();
  node = NULL;

  for (i = 1; i <= 100; i++)
    {
      node = gtk_tree_rbtree_insert_before (tree, node, i, TRUE);
      gtk_tree_rbtree_test (tree);
      g_assert_cmpint (tree->root->count, ==, i);
      g_assert_cmpint (tree->root->total_count, ==, i);
      g_assert_cmpint (tree->root->offset, ==, i * (i + 1) / 2);
    }

  gtk_tree_rbtree_free (tree);
}

static void
test_remove_node (void)
{
  GtkTreeRBTree *tree;

  tree = create_rbtree (3, 16, g_test_thorough ());

  while (tree->root->count > 1)
    {
      GtkTreeRBTree *find_tree;
      GtkTreeRBNode *find_node;
      guint i;

      i = g_test_rand_int_range (0, tree->root->total_count);
      if (!gtk_tree_rbtree_find_index (tree, i, &find_tree, &find_node))
        {
          /* We search an available index, so we mustn't fail. */
          g_assert_not_reached ();
        }

      gtk_tree_rbtree_test (find_tree);

      if (find_tree->root->count == 1)
        {
          gtk_tree_rbtree_remove (find_tree);
        }
      else
        gtk_tree_rbtree_remove_node (find_tree, find_node);
      gtk_tree_rbtree_test (tree);
    }

  gtk_tree_rbtree_free (tree);
}

static void
test_remove_root (void)
{
  GtkTreeRBTree *tree;
  GtkTreeRBNode *node;

  tree = gtk_tree_rbtree_new ();

  node = gtk_tree_rbtree_insert_after (tree, NULL, 1, TRUE);
  gtk_tree_rbtree_insert_after (tree, node, 2, TRUE);
  gtk_tree_rbtree_insert_before (tree, node, 3, TRUE);

  gtk_tree_rbtree_remove_node (tree, node);

  gtk_tree_rbtree_free (tree);
}

static int *
fisher_yates_shuffle (guint n_items)
{
  int *list;
  guint i, j;

  list = g_new (int, n_items);

  for (i = 0; i < n_items; i++)
    {
      j = g_test_rand_int_range (0, i + 1);
      list[i] = list[j];
      list[j] = i;
    }

  return list;
}

static GtkTreeRBTree *
create_unsorted_tree (int *order,
                      guint n)
{
  GtkTreeRBTree *tree;
  GtkTreeRBNode *node;
  guint i;

  tree = gtk_tree_rbtree_new ();
  node = NULL;

  for (i = 0; i < n; i++)
    {
      node = gtk_tree_rbtree_insert_after (tree, node, 0, TRUE);
    }

  for (i = 0; i < n; i++)
    {
      node = gtk_tree_rbtree_find_count (tree, order[i] + 1);
      gtk_tree_rbtree_node_set_height (tree, node, i);
    }

  gtk_tree_rbtree_test (tree);

  return tree;
}

static void
test_reorder (void)
{
  guint n = g_test_perf () ? 1000000 : 100;
  GtkTreeRBTree *tree;
  GtkTreeRBNode *node;
  int *reorder;
  guint i;
  double elapsed;

  reorder = fisher_yates_shuffle (n);
  tree = create_unsorted_tree (reorder, n);

  g_test_timer_start ();

  gtk_tree_rbtree_reorder (tree, reorder, n);

  elapsed = g_test_timer_elapsed ();
  if (g_test_perf ())
    g_test_minimized_result (elapsed, "reordering rbtree with %u items: %gsec", n, elapsed);

  gtk_tree_rbtree_test (tree);

  for (node = gtk_tree_rbtree_first (tree), i = 0;
       node != NULL;
       node = gtk_tree_rbtree_next (tree, node), i++)
    {
      g_assert_cmpint (GTK_TREE_RBNODE_GET_HEIGHT (node), ==, i);
    }
  g_assert_cmpint (i, ==, n);

  gtk_tree_rbtree_free (tree);

  g_free (reorder);
}

int
main (int   argc,
      char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  g_test_add_func ("/rbtree/create", test_create);
  g_test_add_func ("/rbtree/insert_after", test_insert_after);
  g_test_add_func ("/rbtree/insert_before", test_insert_before);
  g_test_add_func ("/rbtree/remove_node", test_remove_node);
  g_test_add_func ("/rbtree/remove_root", test_remove_root);
  g_test_add_func ("/rbtree/reorder", test_reorder);

  return g_test_run ();
}
