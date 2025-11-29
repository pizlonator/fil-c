/* gtktreerbtree.c
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
#include "gtktreerbtreeprivate.h"
#include "gtkdebug.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static GtkTreeRBNode *gtk_tree_rbnode_new                (GtkTreeRBTree *tree,
                                                          int            height);
static void        gtk_tree_rbnode_free               (GtkTreeRBNode *node);
static void        gtk_tree_rbnode_rotate_left        (GtkTreeRBTree *tree,
                                                       GtkTreeRBNode *node);
static void        gtk_tree_rbnode_rotate_right       (GtkTreeRBTree *tree,
                                                       GtkTreeRBNode *node);
static void        gtk_tree_rbtree_insert_fixup       (GtkTreeRBTree *tree,
                                                       GtkTreeRBNode *node);
static void        gtk_tree_rbtree_remove_node_fixup  (GtkTreeRBTree *tree,
                                                       GtkTreeRBNode *node,
                                                       GtkTreeRBNode *parent);
static inline void fixup_validation              (GtkTreeRBTree *tree,
                                                  GtkTreeRBNode *node);
static inline void fixup_total_count             (GtkTreeRBTree *tree,
                                                  GtkTreeRBNode *node);
static void        gtk_tree_rbtree_test               (const char    *where,
                                                       GtkTreeRBTree *tree);
static void        gtk_tree_rbtree_debug_spew         (GtkTreeRBTree *tree,
                                                       GString       *s);

static const GtkTreeRBNode nil =
{
  /* .flags = */ GTK_TREE_RBNODE_BLACK,

  /* rest is NULL */
};

gboolean
gtk_tree_rbtree_is_nil (GtkTreeRBNode *node)
{
  return node == &nil;
}

static GtkTreeRBNode *
gtk_tree_rbnode_new (GtkTreeRBTree *tree,
                     int            height)
{
  GtkTreeRBNode *node = g_slice_new (GtkTreeRBNode);

  node->left = (GtkTreeRBNode *) &nil;
  node->right = (GtkTreeRBNode *) &nil;
  node->parent = (GtkTreeRBNode *) &nil;
  node->flags = GTK_TREE_RBNODE_RED;
  node->total_count = 1;
  node->count = 1;
  node->children = NULL;
  node->offset = height;
  return node;
}

static void
gtk_tree_rbnode_free (GtkTreeRBNode *node)
{
  if (GTK_DEBUG_CHECK (TREE))
    {
      node->left = (gpointer) 0xdeadbeef;
      node->right = (gpointer) 0xdeadbeef;
      node->parent = (gpointer) 0xdeadbeef;
      node->total_count = 56789;
      node->offset = 56789;
      node->count = 56789;
      node->flags = 0;
    }
  g_slice_free (GtkTreeRBNode, node);
}

static void
gtk_tree_rbnode_rotate_left (GtkTreeRBTree *tree,
                             GtkTreeRBNode *node)
{
  int node_height, right_height;
  GtkTreeRBNode *right;

  g_return_if_fail (!gtk_tree_rbtree_is_nil (node));
  g_return_if_fail (!gtk_tree_rbtree_is_nil (node->right));

  right = node->right;

  node_height = GTK_TREE_RBNODE_GET_HEIGHT (node);
  right_height = GTK_TREE_RBNODE_GET_HEIGHT (right);
  node->right = right->left;
  if (!gtk_tree_rbtree_is_nil (right->left))
    right->left->parent = node;

  right->parent = node->parent;
  if (!gtk_tree_rbtree_is_nil (node->parent))
    {
      if (node == node->parent->left)
        node->parent->left = right;
      else
        node->parent->right = right;
    }
  else
    {
      tree->root = right;
    }

  right->left = node;
  node->parent = right;

  node->count = 1 + node->left->count + node->right->count;
  right->count = 1 + right->left->count + right->right->count;

  node->offset = node_height + node->left->offset + node->right->offset +
                 (node->children ? node->children->root->offset : 0);
  right->offset = right_height + right->left->offset + right->right->offset +
                  (right->children ? right->children->root->offset : 0);

  fixup_validation (tree, node);
  fixup_validation (tree, right);
  fixup_total_count (tree, node);
  fixup_total_count (tree, right);
}

static void
gtk_tree_rbnode_rotate_right (GtkTreeRBTree *tree,
                              GtkTreeRBNode *node)
{
  int node_height, left_height;
  GtkTreeRBNode *left;

  g_return_if_fail (!gtk_tree_rbtree_is_nil (node));
  g_return_if_fail (!gtk_tree_rbtree_is_nil (node->left));

  left = node->left;

  node_height = GTK_TREE_RBNODE_GET_HEIGHT (node);
  left_height = GTK_TREE_RBNODE_GET_HEIGHT (left);

  node->left = left->right;
  if (!gtk_tree_rbtree_is_nil (left->right))
    left->right->parent = node;

  left->parent = node->parent;
  if (!gtk_tree_rbtree_is_nil (node->parent))
    {
      if (node == node->parent->right)
        node->parent->right = left;
      else
        node->parent->left = left;
    }
  else
    {
      tree->root = left;
    }

  /* link node and left */
  left->right = node;
  node->parent = left;

  node->count = 1 + node->left->count + node->right->count;
  left->count = 1 + left->left->count + left->right->count;

  node->offset = node_height + node->left->offset + node->right->offset +
                 (node->children ? node->children->root->offset : 0);
  left->offset = left_height + left->left->offset + left->right->offset +
                 (left->children ? left->children->root->offset : 0);

  fixup_validation (tree, node);
  fixup_validation (tree, left);
  fixup_total_count (tree, node);
  fixup_total_count (tree, left);
}

static void
gtk_tree_rbtree_insert_fixup (GtkTreeRBTree *tree,
                              GtkTreeRBNode *node)
{
  /* check Red-Black properties */
  while (node != tree->root && GTK_TREE_RBNODE_GET_COLOR (node->parent) == GTK_TREE_RBNODE_RED)
    {
      /* we have a violation */
      if (node->parent == node->parent->parent->left)
        {
          GtkTreeRBNode *y = node->parent->parent->right;
          if (GTK_TREE_RBNODE_GET_COLOR (y) == GTK_TREE_RBNODE_RED)
            {
              /* uncle is GTK_TREE_RBNODE_RED */
              GTK_TREE_RBNODE_SET_COLOR (node->parent, GTK_TREE_RBNODE_BLACK);
              GTK_TREE_RBNODE_SET_COLOR (y, GTK_TREE_RBNODE_BLACK);
              GTK_TREE_RBNODE_SET_COLOR (node->parent->parent, GTK_TREE_RBNODE_RED);
              node = node->parent->parent;
            }
          else
            {
              /* uncle is GTK_TREE_RBNODE_BLACK */
              if (node == node->parent->right)
                {
                  /* make node a left child */
                  node = node->parent;
                  gtk_tree_rbnode_rotate_left (tree, node);
                }

              /* recolor and rotate */
              GTK_TREE_RBNODE_SET_COLOR (node->parent, GTK_TREE_RBNODE_BLACK);
              GTK_TREE_RBNODE_SET_COLOR (node->parent->parent, GTK_TREE_RBNODE_RED);
              gtk_tree_rbnode_rotate_right (tree, node->parent->parent);
            }
        }
      else
        {
          /* mirror image of above code */
          GtkTreeRBNode *y = node->parent->parent->left;
          if (GTK_TREE_RBNODE_GET_COLOR (y) == GTK_TREE_RBNODE_RED)
            {
              /* uncle is GTK_TREE_RBNODE_RED */
              GTK_TREE_RBNODE_SET_COLOR (node->parent, GTK_TREE_RBNODE_BLACK);
              GTK_TREE_RBNODE_SET_COLOR (y, GTK_TREE_RBNODE_BLACK);
              GTK_TREE_RBNODE_SET_COLOR (node->parent->parent, GTK_TREE_RBNODE_RED);
              node = node->parent->parent;
            }
          else
            {
              /* uncle is GTK_TREE_RBNODE_BLACK */
              if (node == node->parent->left)
                {
                  node = node->parent;
                  gtk_tree_rbnode_rotate_right (tree, node);
                }
              GTK_TREE_RBNODE_SET_COLOR (node->parent, GTK_TREE_RBNODE_BLACK);
              GTK_TREE_RBNODE_SET_COLOR (node->parent->parent, GTK_TREE_RBNODE_RED);
              gtk_tree_rbnode_rotate_left (tree, node->parent->parent);
            }
        }
    }
  GTK_TREE_RBNODE_SET_COLOR (tree->root, GTK_TREE_RBNODE_BLACK);
}

static void
gtk_tree_rbtree_remove_node_fixup (GtkTreeRBTree *tree,
                                   GtkTreeRBNode *node,
                                   GtkTreeRBNode *parent)
{
  while (node != tree->root && GTK_TREE_RBNODE_GET_COLOR (node) == GTK_TREE_RBNODE_BLACK)
    {
      if (node == parent->left)
        {
          GtkTreeRBNode *w = parent->right;
          if (GTK_TREE_RBNODE_GET_COLOR (w) == GTK_TREE_RBNODE_RED)
            {
              GTK_TREE_RBNODE_SET_COLOR (w, GTK_TREE_RBNODE_BLACK);
              GTK_TREE_RBNODE_SET_COLOR (parent, GTK_TREE_RBNODE_RED);
              gtk_tree_rbnode_rotate_left (tree, parent);
              w = parent->right;
            }
          g_assert (w);
          if (GTK_TREE_RBNODE_GET_COLOR (w->left) == GTK_TREE_RBNODE_BLACK && GTK_TREE_RBNODE_GET_COLOR (w->right) == GTK_TREE_RBNODE_BLACK)
            {
              GTK_TREE_RBNODE_SET_COLOR (w, GTK_TREE_RBNODE_RED);
              node = parent;
            }
          else
            {
              if (GTK_TREE_RBNODE_GET_COLOR (w->right) == GTK_TREE_RBNODE_BLACK)
                {
                  GTK_TREE_RBNODE_SET_COLOR (w->left, GTK_TREE_RBNODE_BLACK);
                  GTK_TREE_RBNODE_SET_COLOR (w, GTK_TREE_RBNODE_RED);
                  gtk_tree_rbnode_rotate_right (tree, w);
                  w = parent->right;
                }
              GTK_TREE_RBNODE_SET_COLOR (w, GTK_TREE_RBNODE_GET_COLOR (parent));
              GTK_TREE_RBNODE_SET_COLOR (parent, GTK_TREE_RBNODE_BLACK);
              GTK_TREE_RBNODE_SET_COLOR (w->right, GTK_TREE_RBNODE_BLACK);
              gtk_tree_rbnode_rotate_left (tree, parent);
              node = tree->root;
            }
        }
      else
        {
          GtkTreeRBNode *w = parent->left;
          if (GTK_TREE_RBNODE_GET_COLOR (w) == GTK_TREE_RBNODE_RED)
            {
              GTK_TREE_RBNODE_SET_COLOR (w, GTK_TREE_RBNODE_BLACK);
              GTK_TREE_RBNODE_SET_COLOR (parent, GTK_TREE_RBNODE_RED);
              gtk_tree_rbnode_rotate_right (tree, parent);
              w = parent->left;
            }
          g_assert (w);
          if (GTK_TREE_RBNODE_GET_COLOR (w->right) == GTK_TREE_RBNODE_BLACK && GTK_TREE_RBNODE_GET_COLOR (w->left) == GTK_TREE_RBNODE_BLACK)
            {
              GTK_TREE_RBNODE_SET_COLOR (w, GTK_TREE_RBNODE_RED);
              node = parent;
            }
          else
            {
              if (GTK_TREE_RBNODE_GET_COLOR (w->left) == GTK_TREE_RBNODE_BLACK)
                {
                  GTK_TREE_RBNODE_SET_COLOR (w->right, GTK_TREE_RBNODE_BLACK);
                  GTK_TREE_RBNODE_SET_COLOR (w, GTK_TREE_RBNODE_RED);
                  gtk_tree_rbnode_rotate_left (tree, w);
                  w = parent->left;
                }
              GTK_TREE_RBNODE_SET_COLOR (w, GTK_TREE_RBNODE_GET_COLOR (parent));
              GTK_TREE_RBNODE_SET_COLOR (parent, GTK_TREE_RBNODE_BLACK);
              GTK_TREE_RBNODE_SET_COLOR (w->left, GTK_TREE_RBNODE_BLACK);
              gtk_tree_rbnode_rotate_right (tree, parent);
              node = tree->root;
            }
        }

      parent = node->parent;
    }
  GTK_TREE_RBNODE_SET_COLOR (node, GTK_TREE_RBNODE_BLACK);
}

GtkTreeRBTree *
gtk_tree_rbtree_new (void)
{
  GtkTreeRBTree *retval;

  retval = g_new (GtkTreeRBTree, 1);
  retval->parent_tree = NULL;
  retval->parent_node = NULL;

  retval->root = (GtkTreeRBNode *) &nil;

  return retval;
}

static void
gtk_tree_rbtree_free_helper (GtkTreeRBTree *tree,
                             GtkTreeRBNode *node,
                             gpointer       data)
{
  if (node->children)
    gtk_tree_rbtree_free (node->children);

  gtk_tree_rbnode_free (node);
}

void
gtk_tree_rbtree_free (GtkTreeRBTree *tree)
{
  gtk_tree_rbtree_traverse (tree,
                            tree->root,
                            G_POST_ORDER,
                            gtk_tree_rbtree_free_helper,
                            NULL);

  if (tree->parent_node &&
      tree->parent_node->children == tree)
    tree->parent_node->children = NULL;
  g_free (tree);
}

static void
gtk_rbnode_adjust (GtkTreeRBTree *tree,
                   GtkTreeRBNode *node,
                   int            count_diff,
                   int            total_count_diff,
                   int            offset_diff)
{
  while (tree && node && !gtk_tree_rbtree_is_nil (node))
    {
      fixup_validation (tree, node);
      node->offset += offset_diff;
      node->count += count_diff;
      node->total_count += total_count_diff;

      node = node->parent;
      if (gtk_tree_rbtree_is_nil (node))
        {
          node = tree->parent_node;
          tree = tree->parent_tree;
          count_diff = 0;
        }
    }
}

void
gtk_tree_rbtree_remove (GtkTreeRBTree *tree)
{
  GtkTreeRBTree *tmp_tree;

  if (GTK_DEBUG_CHECK (TREE))
    gtk_tree_rbtree_test (G_STRLOC, tree);

  /* ugly hack to make fixup_validation work in the first iteration of the
   * loop below */
  GTK_TREE_RBNODE_UNSET_FLAG (tree->root, GTK_TREE_RBNODE_DESCENDANTS_INVALID);

  gtk_rbnode_adjust (tree->parent_tree,
                     tree->parent_node,
                     0,
                     -(int) tree->root->total_count,
                     -tree->root->offset);

  tmp_tree = tree->parent_tree;

  gtk_tree_rbtree_free (tree);

  if (GTK_DEBUG_CHECK (TREE))
    gtk_tree_rbtree_test (G_STRLOC, tmp_tree);
}


GtkTreeRBNode *
gtk_tree_rbtree_insert_after (GtkTreeRBTree *tree,
                              GtkTreeRBNode *current,
                              int            height,
                              gboolean       valid)
{
  GtkTreeRBNode *node;
  gboolean right = TRUE;

  if (GTK_DEBUG_CHECK (TREE))
    {
      GString *s;

      s = g_string_new ("");
      g_string_append_printf (s, "gtk_tree_rbtree_insert_after: %p\n", current);
      gtk_tree_rbtree_debug_spew (tree, s);
      g_message ("%s", s->str);
      g_string_free (s, TRUE);
      gtk_tree_rbtree_test (G_STRLOC, tree);
    }

  if (current != NULL && !gtk_tree_rbtree_is_nil (current->right))
    {
      current = current->right;
      while (!gtk_tree_rbtree_is_nil (current->left))
        current = current->left;
      right = FALSE;
    }
  /* setup new node */
  node = gtk_tree_rbnode_new (tree, height);

  /* insert node in tree */
  if (current)
    {
      node->parent = current;
      if (right)
        current->right = node;
      else
        current->left = node;
      gtk_rbnode_adjust (tree, node->parent,
                         1, 1, height);
    }
  else
    {
      g_assert (gtk_tree_rbtree_is_nil (tree->root));
      tree->root = node;
      gtk_rbnode_adjust (tree->parent_tree, tree->parent_node,
                         0, 1, height);
    }

  if (valid)
    gtk_tree_rbtree_node_mark_valid (tree, node);
  else
    gtk_tree_rbtree_node_mark_invalid (tree, node);

  gtk_tree_rbtree_insert_fixup (tree, node);

  if (GTK_DEBUG_CHECK (TREE))
    {
      GString *s;

      s = g_string_new ("gtk_tree_rbtree_insert_after finished...\n");
      gtk_tree_rbtree_debug_spew (tree, s);
      g_message ("%s", s->str);
      g_string_free (s, TRUE);
      gtk_tree_rbtree_test (G_STRLOC, tree);
    }

  return node;
}

GtkTreeRBNode *
gtk_tree_rbtree_insert_before (GtkTreeRBTree *tree,
                               GtkTreeRBNode *current,
                               int            height,
                               gboolean       valid)
{
  GtkTreeRBNode *node;
  gboolean left = TRUE;

  if (GTK_DEBUG_CHECK (TREE))
    {
      GString *s;

      s = g_string_new ("");
      g_string_append_printf (s, "gtk_tree_rbtree_insert_before: %p\n", current);
      gtk_tree_rbtree_debug_spew (tree, s);
      g_message ("%s", s->str);
      g_string_free (s, TRUE);
      gtk_tree_rbtree_test (G_STRLOC, tree);
    }

  if (current != NULL && !gtk_tree_rbtree_is_nil (current->left))
    {
      current = current->left;
      while (!gtk_tree_rbtree_is_nil (current->right))
        current = current->right;
      left = FALSE;
    }

  /* setup new node */
  node = gtk_tree_rbnode_new (tree, height);

  /* insert node in tree */
  if (current)
    {
      node->parent = current;
      if (left)
        current->left = node;
      else
        current->right = node;
      gtk_rbnode_adjust (tree, node->parent,
                         1, 1, height);
    }
  else
    {
      g_assert (gtk_tree_rbtree_is_nil (tree->root));
      tree->root = node;
      gtk_rbnode_adjust (tree->parent_tree, tree->parent_node,
                         0, 1, height);
    }

  if (valid)
    gtk_tree_rbtree_node_mark_valid (tree, node);
  else
    gtk_tree_rbtree_node_mark_invalid (tree, node);

  gtk_tree_rbtree_insert_fixup (tree, node);

  if (GTK_DEBUG_CHECK (TREE))
    {
      GString *s;

      s = g_string_new ("gtk_tree_rbtree_insert_before finished...\n");
      gtk_tree_rbtree_debug_spew (tree, s);
      g_message ("%s", s->str);
      g_string_free (s, TRUE);
      gtk_tree_rbtree_test (G_STRLOC, tree);
    }

  return node;
}

GtkTreeRBNode *
gtk_tree_rbtree_find_count (GtkTreeRBTree *tree,
                            int            count)
{
  GtkTreeRBNode *node;

  node = tree->root;
  while (!gtk_tree_rbtree_is_nil (node) && (node->left->count + 1 != count))
    {
      if (node->left->count >= count)
        node = node->left;
      else
        {
          count -= (node->left->count + 1);
          node = node->right;
        }
    }
  if (gtk_tree_rbtree_is_nil (node))
    return NULL;
  return node;
}

void
gtk_tree_rbtree_node_set_height (GtkTreeRBTree *tree,
                                 GtkTreeRBNode *node,
                                 int            height)
{
  int diff = height - GTK_TREE_RBNODE_GET_HEIGHT (node);

  if (diff == 0)
    return;

  gtk_rbnode_adjust (tree, node, 0, 0, diff);

  if (GTK_DEBUG_CHECK (TREE))
    gtk_tree_rbtree_test (G_STRLOC, tree);
}

void
gtk_tree_rbtree_node_mark_invalid (GtkTreeRBTree *tree,
                                   GtkTreeRBNode *node)
{
  if (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_INVALID))
    return;

  GTK_TREE_RBNODE_SET_FLAG (node, GTK_TREE_RBNODE_INVALID);
  do
    {
      if (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_DESCENDANTS_INVALID))
        return;
      GTK_TREE_RBNODE_SET_FLAG (node, GTK_TREE_RBNODE_DESCENDANTS_INVALID);
      node = node->parent;
      if (gtk_tree_rbtree_is_nil (node))
        {
          node = tree->parent_node;
          tree = tree->parent_tree;
        }
    }
  while (node);
}

#if 0
/* Draconian version. */
void
gtk_tree_rbtree_node_mark_invalid (GtkTreeRBTree *tree,
                                   GtkTreeRBNode *node)
{
  GTK_TREE_RBNODE_SET_FLAG (node, GTK_TREE_RBNODE_INVALID);
  do
    {
      fixup_validation (tree, node);
      node = node->parent;
      if (gtk_tree_rbtree_is_nil (node))
        {
          node = tree->parent_node;
          tree = tree->parent_tree;
        }
    }
  while (node);
}
#endif

void
gtk_tree_rbtree_node_mark_valid (GtkTreeRBTree *tree,
                                 GtkTreeRBNode *node)
{
  if ((!GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_INVALID)) &&
      (!GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_COLUMN_INVALID)))
    return;

  GTK_TREE_RBNODE_UNSET_FLAG (node, GTK_TREE_RBNODE_INVALID);
  GTK_TREE_RBNODE_UNSET_FLAG (node, GTK_TREE_RBNODE_COLUMN_INVALID);

  do
    {
      if ((GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_INVALID)) ||
          (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_COLUMN_INVALID)) ||
          (node->children && GTK_TREE_RBNODE_FLAG_SET (node->children->root, GTK_TREE_RBNODE_DESCENDANTS_INVALID)) ||
          (GTK_TREE_RBNODE_FLAG_SET (node->left, GTK_TREE_RBNODE_DESCENDANTS_INVALID)) ||
          (GTK_TREE_RBNODE_FLAG_SET (node->right, GTK_TREE_RBNODE_DESCENDANTS_INVALID)))
        return;

      GTK_TREE_RBNODE_UNSET_FLAG (node, GTK_TREE_RBNODE_DESCENDANTS_INVALID);
      node = node->parent;
      if (gtk_tree_rbtree_is_nil (node))
        {
          node = tree->parent_node;
          tree = tree->parent_tree;
        }
    }
  while (node);
}

#if 0
/* Draconian version */
void
gtk_tree_rbtree_node_mark_valid (GtkTreeRBTree *tree,
                                 GtkTreeRBNode *node)
{
  GTK_TREE_RBNODE_UNSET_FLAG (node, GTK_TREE_RBNODE_INVALID);
  GTK_TREE_RBNODE_UNSET_FLAG (node, GTK_TREE_RBNODE_COLUMN_INVALID);

  do
    {
      fixup_validation (tree, node);
      node = node->parent;
      if (gtk_tree_rbtree_is_nil (node))
        {
          node = tree->parent_node;
          tree = tree->parent_tree;
        }
    }
  while (node);
}
#endif
/* Assume tree is the root node as it doesn't set DESCENDANTS_INVALID above.
 */
void
gtk_tree_rbtree_column_invalid (GtkTreeRBTree *tree)
{
  GtkTreeRBNode *node;

  if (tree == NULL)
    return;

  for (node = gtk_tree_rbtree_first (tree);
       node != NULL;
       node = gtk_tree_rbtree_next (tree, node))
    {
      if (!(GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_INVALID)))
        GTK_TREE_RBNODE_SET_FLAG (node, GTK_TREE_RBNODE_COLUMN_INVALID);
      GTK_TREE_RBNODE_SET_FLAG (node, GTK_TREE_RBNODE_DESCENDANTS_INVALID);

      if (node->children)
        gtk_tree_rbtree_column_invalid (node->children);
    }
}

void
gtk_tree_rbtree_mark_invalid (GtkTreeRBTree *tree)
{
  GtkTreeRBNode *node;

  if (tree == NULL)
    return;

  for (node = gtk_tree_rbtree_first (tree);
       node != NULL;
       node = gtk_tree_rbtree_next (tree, node))
    {
      GTK_TREE_RBNODE_SET_FLAG (node, GTK_TREE_RBNODE_INVALID);
      GTK_TREE_RBNODE_SET_FLAG (node, GTK_TREE_RBNODE_DESCENDANTS_INVALID);

      if (node->children)
        gtk_tree_rbtree_mark_invalid (node->children);
    }
}

void
gtk_tree_rbtree_set_fixed_height (GtkTreeRBTree *tree,
                                  int            height,
                                  gboolean       mark_valid)
{
  GtkTreeRBNode *node;

  if (tree == NULL)
    return;

  for (node = gtk_tree_rbtree_first (tree);
       node != NULL;
       node = gtk_tree_rbtree_next (tree, node))
    {
      if (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_INVALID))
        {
          gtk_tree_rbtree_node_set_height (tree, node, height);
          if (mark_valid)
            gtk_tree_rbtree_node_mark_valid (tree, node);
        }

      if (node->children)
        gtk_tree_rbtree_set_fixed_height (node->children, height, mark_valid);
    }
}

static void
reorder_prepare (GtkTreeRBTree *tree,
                 GtkTreeRBNode *node,
                 gpointer       data)
{
  node->offset -= node->left->offset + node->right->offset;
  GTK_TREE_RBNODE_UNSET_FLAG (node, GTK_TREE_RBNODE_DESCENDANTS_INVALID);
}

static void
reorder_fixup (GtkTreeRBTree *tree,
               GtkTreeRBNode *node,
               gpointer       data)
{
  node->offset += node->left->offset + node->right->offset;
  node->count = 1 + node->left->count + node->right->count;
  fixup_validation (tree, node);
  fixup_total_count (tree, node);
}

static void
reorder_copy_node (GtkTreeRBTree *tree,
                   GtkTreeRBNode *to,
                   GtkTreeRBNode *from)
{
  to->flags = (to->flags & GTK_TREE_RBNODE_NON_COLORS) | GTK_TREE_RBNODE_GET_COLOR (from);

  to->left = from->left;
  if (!gtk_tree_rbtree_is_nil (to->left))
    to->left->parent = to;

  to->right = from->right;
  if (!gtk_tree_rbtree_is_nil (to->right))
    to->right->parent = to;

  to->parent = from->parent;
  if (gtk_tree_rbtree_is_nil (to->parent))
    tree->root = to;
  else if (to->parent->left == from)
    to->parent->left = to;
  else if (to->parent->right == from)
    to->parent->right = to;
}

/* It basically pulls everything out of the tree, rearranges it, and puts it
 * back together.  Our strategy is to keep the old RBTree intact, and just
 * rearrange the contents.  When that is done, we go through and update the
 * heights.  There is probably a more elegant way to write this function.  If
 * anyone wants to spend the time writing it, patches will be accepted.
 */
void
gtk_tree_rbtree_reorder (GtkTreeRBTree *tree,
                         int           *new_order,
                         int            length)
{
  GtkTreeRBNode **nodes;
  GtkTreeRBNode *node;
  int i, j;

  g_return_if_fail (tree != NULL);
  g_return_if_fail (length > 0);
  g_return_if_fail (tree->root->count == length);

  nodes = g_new (GtkTreeRBNode *, length);

  gtk_tree_rbtree_traverse (tree, tree->root, G_PRE_ORDER, reorder_prepare, NULL);

  for (node = gtk_tree_rbtree_first (tree), i = 0;
       node;
       node = gtk_tree_rbtree_next (tree, node), i++)
    {
      nodes[i] = node;
    }

  for (i = 0; i < length; i++)
    {
      GtkTreeRBNode tmp = { 0, };
      GSList *l, *cycle = NULL;

      tmp.offset = -1;

      /* already swapped */
      if (nodes[i] == NULL)
        continue;
      /* no need to swap */
      if (new_order[i] == i)
        continue;

      /* make a list out of the pending nodes */
      for (j = i; new_order[j] != i; j = new_order[j])
        {
          cycle = g_slist_prepend (cycle, nodes[j]);
          nodes[j] = NULL;
        }

      node = nodes[j];
      reorder_copy_node (tree, &tmp, node);
      for (l = cycle; l; l = l->next)
        {
          reorder_copy_node (tree, node, l->data);
          node = l->data;
        }

      reorder_copy_node (tree, node, &tmp);
      nodes[j] = NULL;
      g_slist_free (cycle);
    }

  gtk_tree_rbtree_traverse (tree, tree->root, G_POST_ORDER, reorder_fixup, NULL);

  g_free (nodes);
}

/**
 * gtk_tree_rbtree_contains:
 * @tree: a tree
 * @potential_child: a potential child of @tree
 *
 * Checks if @potential_child is a child (direct or via intermediate
 * trees) of @tree.
 *
 * Returns: %TRUE if @potential_child is a child of @tree.
 **/
gboolean
gtk_tree_rbtree_contains (GtkTreeRBTree *tree,
                          GtkTreeRBTree *potential_child)
{
  g_return_val_if_fail (tree != NULL, FALSE);
  g_return_val_if_fail (potential_child != NULL, FALSE);

  do
    {
      potential_child = potential_child->parent_tree;
      if (potential_child == tree)
        return TRUE;
    }
  while (potential_child != NULL);

  return FALSE;
}

int
gtk_tree_rbtree_node_find_offset (GtkTreeRBTree *tree,
                                  GtkTreeRBNode *node)
{
  GtkTreeRBNode *last;
  int retval;

  g_assert (node);
  g_assert (node->left);

  retval = node->left->offset;

  while (tree && node && !gtk_tree_rbtree_is_nil (node))
    {
      last = node;
      node = node->parent;

      /* Add left branch, plus children, iff we came from the right */
      if (node->right == last)
        retval += node->offset - node->right->offset;

      if (gtk_tree_rbtree_is_nil (node))
        {
          node = tree->parent_node;
          tree = tree->parent_tree;

          /* Add the parent node, plus the left branch. */
          if (node)
            retval += node->left->offset + GTK_TREE_RBNODE_GET_HEIGHT (node);
        }
    }
  return retval;
}

guint
gtk_tree_rbtree_node_get_index (GtkTreeRBTree *tree,
                                GtkTreeRBNode *node)
{
  GtkTreeRBNode *last;
  guint retval;

  g_assert (node);
  g_assert (node->left);

  retval = node->left->total_count;

  while (tree && node && !gtk_tree_rbtree_is_nil (node))
    {
      last = node;
      node = node->parent;

      /* Add left branch, plus children, iff we came from the right */
      if (node->right == last)
        retval += node->total_count - node->right->total_count;

      if (gtk_tree_rbtree_is_nil (node))
        {
          node = tree->parent_node;
          tree = tree->parent_tree;

          /* Add the parent node, plus the left branch. */
          if (node)
            retval += node->left->total_count + 1; /* 1 == GTK_TREE_RBNODE_GET_PARITY() */
        }
    }

  return retval;
}

static int
gtk_rbtree_real_find_offset (GtkTreeRBTree  *tree,
                             int             height,
                             GtkTreeRBTree **new_tree,
                             GtkTreeRBNode **new_node)
{
  GtkTreeRBNode *tmp_node;

  g_assert (tree);

  if (height < 0)
    {
      *new_tree = NULL;
      *new_node = NULL;

      return 0;
    }


  tmp_node = tree->root;
  while (!gtk_tree_rbtree_is_nil (tmp_node) &&
         (tmp_node->left->offset > height ||
          (tmp_node->offset - tmp_node->right->offset) < height))
    {
      if (tmp_node->left->offset > height)
        tmp_node = tmp_node->left;
      else
        {
          height -= (tmp_node->offset - tmp_node->right->offset);
          tmp_node = tmp_node->right;
        }
    }
  if (gtk_tree_rbtree_is_nil (tmp_node))
    {
      *new_tree = NULL;
      *new_node = NULL;
      return 0;
    }
  if (tmp_node->children)
    {
      if ((tmp_node->offset -
           tmp_node->right->offset -
           tmp_node->children->root->offset) > height)
        {
          *new_tree = tree;
          *new_node = tmp_node;
          return (height - tmp_node->left->offset);
        }
      return gtk_rbtree_real_find_offset (tmp_node->children,
                                          height - tmp_node->left->offset -
                                          (tmp_node->offset -
                                           tmp_node->left->offset -
                                           tmp_node->right->offset -
                                           tmp_node->children->root->offset),
                                          new_tree,
                                          new_node);
    }
  *new_tree = tree;
  *new_node = tmp_node;
  return (height - tmp_node->left->offset);
}

int
gtk_tree_rbtree_find_offset (GtkTreeRBTree  *tree,
                             int             height,
                             GtkTreeRBTree **new_tree,
                             GtkTreeRBNode **new_node)
{
  g_assert (tree);

  if ((height < 0) ||
      (height >= tree->root->offset))
    {
      *new_tree = NULL;
      *new_node = NULL;

      return 0;
    }
  return gtk_rbtree_real_find_offset (tree, height, new_tree, new_node);
}

gboolean
gtk_tree_rbtree_find_index (GtkTreeRBTree  *tree,
                            guint           index,
                            GtkTreeRBTree **new_tree,
                            GtkTreeRBNode **new_node)
{
  GtkTreeRBNode *tmp_node;

  g_assert (tree);

  tmp_node = tree->root;
  while (!gtk_tree_rbtree_is_nil (tmp_node))
    {
      if (tmp_node->left->total_count > index)
        {
          tmp_node = tmp_node->left;
        }
      else if (tmp_node->total_count - tmp_node->right->total_count <= index)
        {
          index -= tmp_node->total_count - tmp_node->right->total_count;
          tmp_node = tmp_node->right;
        }
      else
        {
          index -= tmp_node->left->total_count;
          break;
        }
    }
  if (gtk_tree_rbtree_is_nil (tmp_node))
    {
      *new_tree = NULL;
      *new_node = NULL;
      return FALSE;
    }

  if (index > 0)
    {
      g_assert (tmp_node->children);

      return gtk_tree_rbtree_find_index (tmp_node->children,
                                         index - 1,
                                         new_tree,
                                         new_node);
    }

  *new_tree = tree;
  *new_node = tmp_node;
  return TRUE;
}

void
gtk_tree_rbtree_remove_node (GtkTreeRBTree *tree,
                             GtkTreeRBNode *node)
{
  GtkTreeRBNode *x, *y;
  int y_height;
  guint y_total_count;

  g_return_if_fail (tree != NULL);
  g_return_if_fail (node != NULL);


  if (GTK_DEBUG_CHECK (TREE))
    {
      GString *s;

      s = g_string_new ("");
      g_string_append_printf (s, "gtk_tree_rbtree_remove_node: %p\n", node);
      gtk_tree_rbtree_debug_spew (tree, s);
      g_message ("%s", s->str);
      g_string_free (s, TRUE);
      gtk_tree_rbtree_test (G_STRLOC, tree);
    }

  /* make sure we're deleting a node that's actually in the tree */
  for (x = node; !gtk_tree_rbtree_is_nil (x->parent); x = x->parent)
    ;
  g_return_if_fail (x == tree->root);

  if (GTK_DEBUG_CHECK (TREE))
    gtk_tree_rbtree_test (G_STRLOC, tree);

  if (gtk_tree_rbtree_is_nil (node->left) ||
      gtk_tree_rbtree_is_nil (node->right))
    {
      y = node;
    }
  else
    {
      y = node->right;

      while (!gtk_tree_rbtree_is_nil (y->left))
        y = y->left;
    }

  y_height = GTK_TREE_RBNODE_GET_HEIGHT (y)
             + (y->children ? y->children->root->offset : 0);
  y_total_count = 1 + (y->children ? y->children->root->total_count : 0);

  /* x is y's only child, or nil */
  if (!gtk_tree_rbtree_is_nil (y->left))
    x = y->left;
  else
    x = y->right;

  /* remove y from the parent chain */
  if (!gtk_tree_rbtree_is_nil (x))
    x->parent = y->parent;
  if (!gtk_tree_rbtree_is_nil (y->parent))
    {
      if (y == y->parent->left)
        y->parent->left = x;
      else
        y->parent->right = x;
    }
  else
    {
      tree->root = x;
    }

  /* We need to clean up the validity of the tree.
   */
  gtk_rbnode_adjust (tree, y, -1, -y_total_count, -y_height);

  if (GTK_TREE_RBNODE_GET_COLOR (y) == GTK_TREE_RBNODE_BLACK)
    gtk_tree_rbtree_remove_node_fixup (tree, x, y->parent);

  if (y != node)
    {
      int node_height, node_total_count;

      /* We want to see how much we remove from the aggregate values.
       * This is all the children we remove plus the node's values.
       */
      node_height = GTK_TREE_RBNODE_GET_HEIGHT (node)
                    + (node->children ? node->children->root->offset : 0);
      node_total_count = 1
                         + (node->children ? node->children->root->total_count : 0);

      /* Move the node over */
      if (GTK_TREE_RBNODE_GET_COLOR (node) != GTK_TREE_RBNODE_GET_COLOR (y))
        y->flags ^= (GTK_TREE_RBNODE_BLACK | GTK_TREE_RBNODE_RED);

      y->left = node->left;
      if (!gtk_tree_rbtree_is_nil (y->left))
        y->left->parent = y;
      y->right = node->right;
      if (!gtk_tree_rbtree_is_nil (y->right))
        y->right->parent = y;
      y->parent = node->parent;
      if (!gtk_tree_rbtree_is_nil (y->parent))
        {
          if (y->parent->left == node)
            y->parent->left = y;
          else
            y->parent->right = y;
        }
      else
        {
          tree->root = y;
        }
      y->count = node->count;
      y->total_count = node->total_count;
      y->offset = node->offset;

      gtk_rbnode_adjust (tree, y,
                         0,
                         y_total_count - node_total_count,
                         y_height - node_height);
    }

  gtk_tree_rbnode_free (node);

  if (GTK_DEBUG_CHECK (TREE))
    {
      GString *s;

      s = g_string_new ("gtk_tree_rbtree_remove_node finished...\n");
      gtk_tree_rbtree_debug_spew (tree, s);
      g_message ("%s", s->str);
      g_string_free (s, TRUE);
      gtk_tree_rbtree_test (G_STRLOC, tree);
    }
}

GtkTreeRBNode *
gtk_tree_rbtree_first (GtkTreeRBTree *tree)
{
  GtkTreeRBNode *node;

  node = tree->root;

  if (gtk_tree_rbtree_is_nil (node))
    return NULL;

  while (!gtk_tree_rbtree_is_nil (node->left))
    node = node->left;

  return node;
}

GtkTreeRBNode *
gtk_tree_rbtree_next (GtkTreeRBTree *tree,
                      GtkTreeRBNode *node)
{
  g_return_val_if_fail (tree != NULL, NULL);
  g_return_val_if_fail (node != NULL, NULL);

  /* Case 1: the node's below us. */
  if (!gtk_tree_rbtree_is_nil (node->right))
    {
      node = node->right;
      while (!gtk_tree_rbtree_is_nil (node->left))
        node = node->left;
      return node;
    }

  /* Case 2: it's an ancestor */
  while (!gtk_tree_rbtree_is_nil (node->parent))
    {
      if (node->parent->right == node)
        node = node->parent;
      else
        return (node->parent);
    }

  /* Case 3: There is no next node */
  return NULL;
}

GtkTreeRBNode *
gtk_tree_rbtree_prev (GtkTreeRBTree *tree,
                      GtkTreeRBNode *node)
{
  g_return_val_if_fail (tree != NULL, NULL);
  g_return_val_if_fail (node != NULL, NULL);

  /* Case 1: the node's below us. */
  if (!gtk_tree_rbtree_is_nil (node->left))
    {
      node = node->left;
      while (!gtk_tree_rbtree_is_nil (node->right))
        node = node->right;
      return node;
    }

  /* Case 2: it's an ancestor */
  while (!gtk_tree_rbtree_is_nil (node->parent))
    {
      if (node->parent->left == node)
        node = node->parent;
      else
        return (node->parent);
    }

  /* Case 3: There is no next node */
  return NULL;
}

void
gtk_tree_rbtree_next_full (GtkTreeRBTree  *tree,
                           GtkTreeRBNode  *node,
                           GtkTreeRBTree **new_tree,
                           GtkTreeRBNode **new_node)
{
  g_return_if_fail (tree != NULL);
  g_return_if_fail (node != NULL);
  g_return_if_fail (new_tree != NULL);
  g_return_if_fail (new_node != NULL);

  if (node->children)
    {
      *new_tree = node->children;
      *new_node = (*new_tree)->root;
      while (!gtk_tree_rbtree_is_nil ((*new_node)->left))
        *new_node = (*new_node)->left;
      return;
    }

  *new_tree = tree;
  *new_node = gtk_tree_rbtree_next (tree, node);

  while ((*new_node == NULL) &&
         (*new_tree != NULL))
    {
      *new_node = (*new_tree)->parent_node;
      *new_tree = (*new_tree)->parent_tree;
      if (*new_tree)
        *new_node = gtk_tree_rbtree_next (*new_tree, *new_node);
    }
}

void
gtk_tree_rbtree_prev_full (GtkTreeRBTree  *tree,
                           GtkTreeRBNode  *node,
                           GtkTreeRBTree **new_tree,
                           GtkTreeRBNode **new_node)
{
  g_return_if_fail (tree != NULL);
  g_return_if_fail (node != NULL);
  g_return_if_fail (new_tree != NULL);
  g_return_if_fail (new_node != NULL);

  *new_tree = tree;
  *new_node = gtk_tree_rbtree_prev (tree, node);

  if (*new_node == NULL)
    {
      *new_node = (*new_tree)->parent_node;
      *new_tree = (*new_tree)->parent_tree;
    }
  else
    {
      while ((*new_node)->children)
        {
          *new_tree = (*new_node)->children;
          *new_node = (*new_tree)->root;
          while (!gtk_tree_rbtree_is_nil ((*new_node)->right))
            *new_node = (*new_node)->right;
        }
    }
}

int
gtk_tree_rbtree_get_depth (GtkTreeRBTree *tree)
{
  GtkTreeRBTree *tmp_tree;
  int depth = 0;

  tmp_tree = tree->parent_tree;
  while (tmp_tree)
    {
      ++depth;
      tmp_tree = tmp_tree->parent_tree;
    }

  return depth;
}

static void
gtk_tree_rbtree_traverse_pre_order (GtkTreeRBTree            *tree,
                                    GtkTreeRBNode            *node,
                                    GtkTreeRBTreeTraverseFunc func,
                                    gpointer                  data)
{
  if (gtk_tree_rbtree_is_nil (node))
    return;

  (*func)(tree, node, data);
  gtk_tree_rbtree_traverse_pre_order (tree, node->left, func, data);
  gtk_tree_rbtree_traverse_pre_order (tree, node->right, func, data);
}

static void
gtk_tree_rbtree_traverse_post_order (GtkTreeRBTree            *tree,
                                     GtkTreeRBNode            *node,
                                     GtkTreeRBTreeTraverseFunc func,
                                     gpointer                  data)
{
  if (gtk_tree_rbtree_is_nil (node))
    return;

  gtk_tree_rbtree_traverse_post_order (tree, node->left, func, data);
  gtk_tree_rbtree_traverse_post_order (tree, node->right, func, data);
  (*func)(tree, node, data);
}

void
gtk_tree_rbtree_traverse (GtkTreeRBTree            *tree,
                          GtkTreeRBNode            *node,
                          GTraverseType             order,
                          GtkTreeRBTreeTraverseFunc func,
                          gpointer                  data)
{
  g_return_if_fail (tree != NULL);
  g_return_if_fail (node != NULL);
  g_return_if_fail (func != NULL);
  g_return_if_fail (order <= G_LEVEL_ORDER);

  switch (order)
    {
    case G_PRE_ORDER:
      gtk_tree_rbtree_traverse_pre_order (tree, node, func, data);
      break;

    case G_POST_ORDER:
      gtk_tree_rbtree_traverse_post_order (tree, node, func, data);
      break;

    case G_IN_ORDER:
    case G_LEVEL_ORDER:
    default:
      g_warning ("unsupported traversal order.");
      break;
    }
}

static inline
void fixup_validation (GtkTreeRBTree *tree,
                       GtkTreeRBNode *node)
{
  if (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_INVALID) ||
      GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_COLUMN_INVALID) ||
      GTK_TREE_RBNODE_FLAG_SET (node->left, GTK_TREE_RBNODE_DESCENDANTS_INVALID) ||
      GTK_TREE_RBNODE_FLAG_SET (node->right, GTK_TREE_RBNODE_DESCENDANTS_INVALID) ||
      (node->children != NULL && GTK_TREE_RBNODE_FLAG_SET (node->children->root, GTK_TREE_RBNODE_DESCENDANTS_INVALID)))
    {
      GTK_TREE_RBNODE_SET_FLAG (node, GTK_TREE_RBNODE_DESCENDANTS_INVALID);
    }
  else
    {
      GTK_TREE_RBNODE_UNSET_FLAG (node, GTK_TREE_RBNODE_DESCENDANTS_INVALID);
    }
}

static inline
void fixup_total_count (GtkTreeRBTree *tree,
                        GtkTreeRBNode *node)
{
  node->total_count = 1 +
                      (node->children != NULL ? node->children->root->total_count : 0) +
                      node->left->total_count + node->right->total_count;
}

#ifndef G_DISABLE_ASSERT
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
    g_error ("total count incorrect for node");

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

  g_assert (node->left);
  g_assert (node->right);

  res = (_count_nodes (tree, node->left) +
         _count_nodes (tree, node->right) + 1);

  if (res != node->count)
    g_error ("Tree failed");
  return res;
}
#endif /* G_DISABLE_ASSERT */

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
  g_assert (node);

  if (expected_dirtyness)
    {
      g_assert (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_COLUMN_INVALID) ||
                GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_INVALID) ||
                GTK_TREE_RBNODE_FLAG_SET (node->left, GTK_TREE_RBNODE_DESCENDANTS_INVALID) ||
                GTK_TREE_RBNODE_FLAG_SET (node->right, GTK_TREE_RBNODE_DESCENDANTS_INVALID) ||
                (node->children && GTK_TREE_RBNODE_FLAG_SET (node->children->root, GTK_TREE_RBNODE_DESCENDANTS_INVALID)));
    }
  else
    {
      g_assert (!GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_COLUMN_INVALID) &&
                !GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_INVALID));
      if (!gtk_tree_rbtree_is_nil (node->left))
        g_assert (!GTK_TREE_RBNODE_FLAG_SET (node->left, GTK_TREE_RBNODE_DESCENDANTS_INVALID));
      if (!gtk_tree_rbtree_is_nil (node->right))
        g_assert (!GTK_TREE_RBNODE_FLAG_SET (node->right, GTK_TREE_RBNODE_DESCENDANTS_INVALID));
      if (node->children != NULL)
        g_assert (!GTK_TREE_RBNODE_FLAG_SET (node->children->root, GTK_TREE_RBNODE_DESCENDANTS_INVALID));
    }

  if (!gtk_tree_rbtree_is_nil (node->left))
    gtk_tree_rbtree_test_dirty (tree, node->left, GTK_TREE_RBNODE_FLAG_SET (node->left, GTK_TREE_RBNODE_DESCENDANTS_INVALID));
  if (!gtk_tree_rbtree_is_nil (node->right))
    gtk_tree_rbtree_test_dirty (tree, node->right, GTK_TREE_RBNODE_FLAG_SET (node->right, GTK_TREE_RBNODE_DESCENDANTS_INVALID));
  if (node->children != NULL && !gtk_tree_rbtree_is_nil (node->children->root))
    gtk_tree_rbtree_test_dirty (node->children, node->children->root, GTK_TREE_RBNODE_FLAG_SET (node->children->root, GTK_TREE_RBNODE_DESCENDANTS_INVALID));
}

static void gtk_tree_rbtree_test_structure (GtkTreeRBTree *tree);

static void
gtk_tree_rbtree_test_structure_helper (GtkTreeRBTree *tree,
                                       GtkTreeRBNode *node)
{
  g_assert (!gtk_tree_rbtree_is_nil (node));

  g_assert (node->left != NULL);
  g_assert (node->right != NULL);
  g_assert (node->parent != NULL);

  if (!gtk_tree_rbtree_is_nil (node->left))
    {
      g_assert (node->left->parent == node);
      gtk_tree_rbtree_test_structure_helper (tree, node->left);
    }
  if (!gtk_tree_rbtree_is_nil (node->right))
    {
      g_assert (node->right->parent == node);
      gtk_tree_rbtree_test_structure_helper (tree, node->right);
    }

  if (node->children != NULL)
    {
      g_assert (node->children->parent_tree == tree);
      g_assert (node->children->parent_node == node);

      gtk_tree_rbtree_test_structure (node->children);
    }
}
static void
gtk_tree_rbtree_test_structure (GtkTreeRBTree *tree)
{
  g_assert (tree->root);
  if (gtk_tree_rbtree_is_nil (tree->root))
    return;

  g_assert (gtk_tree_rbtree_is_nil (tree->root->parent));
  gtk_tree_rbtree_test_structure_helper (tree, tree->root);
}

static void
gtk_tree_rbtree_test (const char    *where,
                      GtkTreeRBTree *tree)
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

  g_assert ((_count_nodes (tmp_tree, tmp_tree->root->left) +
             _count_nodes (tmp_tree, tmp_tree->root->right) + 1) == tmp_tree->root->count);


  gtk_tree_rbtree_test_height (tmp_tree, tmp_tree->root);
  gtk_tree_rbtree_test_dirty (tmp_tree, tmp_tree->root, GTK_TREE_RBNODE_FLAG_SET (tmp_tree->root, GTK_TREE_RBNODE_DESCENDANTS_INVALID));
  g_assert (count_total (tmp_tree, tmp_tree->root) == tmp_tree->root->total_count);
}

static void
gtk_tree_rbtree_debug_spew_helper (GtkTreeRBTree *tree,
                                   GtkTreeRBNode *node,
                                   GString       *s,
                                   int            depth)
{
  int i;
  for (i = 0; i < depth; i++)
    g_string_append (s, "\t");

  g_string_append_printf (s, "(%p - %s) (Offset %d) (Parity %d) (Validity %d%d%d)\n",
                          node,
                          (GTK_TREE_RBNODE_GET_COLOR (node) == GTK_TREE_RBNODE_BLACK) ? "BLACK" : " RED ",
                          node->offset,
                          node->total_count,
                          (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_DESCENDANTS_INVALID)) ? 1 : 0,
                          (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_INVALID)) ? 1 : 0,
                          (GTK_TREE_RBNODE_FLAG_SET (node, GTK_TREE_RBNODE_COLUMN_INVALID)) ? 1 : 0);
  if (node->children != NULL)
    {
      g_string_append (s, "Looking at child.\n");
      gtk_tree_rbtree_debug_spew (node->children, s);
      g_string_append (s, "Done looking at child.\n");
    }
  if (!gtk_tree_rbtree_is_nil (node->left))
    {
      gtk_tree_rbtree_debug_spew_helper (tree, node->left, s, depth + 1);
    }
  if (!gtk_tree_rbtree_is_nil (node->right))
    {
      gtk_tree_rbtree_debug_spew_helper (tree, node->right, s, depth + 1);
    }
}

static void
gtk_tree_rbtree_debug_spew (GtkTreeRBTree *tree,
                            GString       *s)
{
  g_return_if_fail (tree != NULL);

  if (gtk_tree_rbtree_is_nil (tree->root))
    g_string_append (s, "Empty tree...");
  else
    gtk_tree_rbtree_debug_spew_helper (tree, tree->root, s, 0);
}
