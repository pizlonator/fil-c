/* gtkrbtreeprivate.h
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

/* A Red-Black Tree implementation used specifically by GtkTreeView.
 */
#pragma once

#include <glib.h>


G_BEGIN_DECLS


typedef enum
{
  GTK_TREE_RBNODE_BLACK = 1 << 0,
  GTK_TREE_RBNODE_RED = 1 << 1,
  GTK_TREE_RBNODE_IS_PARENT = 1 << 2,
  GTK_TREE_RBNODE_IS_SELECTED = 1 << 3,
  GTK_TREE_RBNODE_IS_PRELIT = 1 << 4,
  GTK_TREE_RBNODE_INVALID = 1 << 7,
  GTK_TREE_RBNODE_COLUMN_INVALID = 1 << 8,
  GTK_TREE_RBNODE_DESCENDANTS_INVALID = 1 << 9,
  GTK_TREE_RBNODE_NON_COLORS = GTK_TREE_RBNODE_IS_PARENT |
                            GTK_TREE_RBNODE_IS_SELECTED |
                            GTK_TREE_RBNODE_IS_PRELIT |
                          GTK_TREE_RBNODE_INVALID |
                          GTK_TREE_RBNODE_COLUMN_INVALID |
                          GTK_TREE_RBNODE_DESCENDANTS_INVALID
} GtkTreeRBNodeColor;

typedef struct _GtkTreeRBTree GtkTreeRBTree;
typedef struct _GtkTreeRBNode GtkTreeRBNode;
typedef struct _GtkTreeRBTreeView GtkTreeRBTreeView;

typedef void (*GtkTreeRBTreeTraverseFunc) (GtkTreeRBTree  *tree,
                                       GtkTreeRBNode  *node,
                                       gpointer  data);

struct _GtkTreeRBTree
{
  GtkTreeRBNode *root;
  GtkTreeRBTree *parent_tree;
  GtkTreeRBNode *parent_node;
};

struct _GtkTreeRBNode
{
  guint flags : 14;

  /* count is the number of nodes beneath us, plus 1 for ourselves.
   * i.e. node->left->count + node->right->count + 1
   */
  int count;

  GtkTreeRBNode *left;
  GtkTreeRBNode *right;
  GtkTreeRBNode *parent;

  /* count the number of total nodes beneath us, including nodes
   * of children trees.
   * i.e. node->left->count + node->right->count + node->children->root->count + 1
   */
  guint total_count;
  
  /* this is the total of sizes of
   * node->left, node->right, our own height, and the height
   * of all trees in ->children, iff children exists because
   * the thing is expanded.
   */
  int offset;

  /* Child trees */
  GtkTreeRBTree *children;
};


#define GTK_TREE_RBNODE_GET_COLOR(node)                (node?(((node->flags&GTK_TREE_RBNODE_RED)==GTK_TREE_RBNODE_RED)?GTK_TREE_RBNODE_RED:GTK_TREE_RBNODE_BLACK):GTK_TREE_RBNODE_BLACK)
#define GTK_TREE_RBNODE_SET_COLOR(node,color)         if((node->flags&color)!=color)node->flags=node->flags^(GTK_TREE_RBNODE_RED|GTK_TREE_RBNODE_BLACK)
#define GTK_TREE_RBNODE_GET_HEIGHT(node)                 (node->offset-(node->left->offset+node->right->offset+(node->children?node->children->root->offset:0)))
#define GTK_TREE_RBNODE_SET_FLAG(node, flag)           G_STMT_START{ (node->flags|=flag); }G_STMT_END
#define GTK_TREE_RBNODE_UNSET_FLAG(node, flag)         G_STMT_START{ (node->flags&=~(flag)); }G_STMT_END
#define GTK_TREE_RBNODE_FLAG_SET(node, flag)         (node?(((node->flags&flag)==flag)?TRUE:FALSE):FALSE)


GtkTreeRBTree * gtk_tree_rbtree_new                     (void);
void            gtk_tree_rbtree_free                    (GtkTreeRBTree                 *tree);
void            gtk_tree_rbtree_remove                  (GtkTreeRBTree                 *tree);
void            gtk_tree_rbtree_destroy                 (GtkTreeRBTree                 *tree);
GtkTreeRBNode * gtk_tree_rbtree_insert_before           (GtkTreeRBTree                 *tree,
                                                         GtkTreeRBNode                 *node,
                                                         int                            height,
                                                         gboolean                       valid);
GtkTreeRBNode * gtk_tree_rbtree_insert_after            (GtkTreeRBTree                 *tree,
                                                         GtkTreeRBNode                 *node,
                                                         int                            height,
                                                         gboolean                       valid);
void            gtk_tree_rbtree_remove_node             (GtkTreeRBTree                 *tree,
                                                         GtkTreeRBNode                 *node);
gboolean        gtk_tree_rbtree_is_nil                  (GtkTreeRBNode                 *node);
void            gtk_tree_rbtree_reorder                 (GtkTreeRBTree                 *tree,
                                                         int                           *new_order,
                                                         int                            length);
gboolean        gtk_tree_rbtree_contains                (GtkTreeRBTree                 *tree,
                                                         GtkTreeRBTree                 *potential_child);
GtkTreeRBNode * gtk_tree_rbtree_find_count              (GtkTreeRBTree                 *tree,
                                                         int                            count);
void            gtk_tree_rbtree_node_set_height         (GtkTreeRBTree                 *tree,
                                                         GtkTreeRBNode                 *node,
                                                         int                            height);
void            gtk_tree_rbtree_node_mark_invalid       (GtkTreeRBTree                 *tree,
                                                         GtkTreeRBNode                 *node);
void            gtk_tree_rbtree_node_mark_valid         (GtkTreeRBTree                 *tree,
                                                         GtkTreeRBNode                 *node);
void            gtk_tree_rbtree_column_invalid          (GtkTreeRBTree                 *tree);
void            gtk_tree_rbtree_mark_invalid            (GtkTreeRBTree                 *tree);
void            gtk_tree_rbtree_set_fixed_height        (GtkTreeRBTree                 *tree,
                                                         int                            height,
                                                         gboolean                       mark_valid);
int             gtk_tree_rbtree_node_find_offset        (GtkTreeRBTree                 *tree,
                                                         GtkTreeRBNode                 *node);
guint           gtk_tree_rbtree_node_get_index          (GtkTreeRBTree                 *tree,
                                                         GtkTreeRBNode                 *node);
gboolean        gtk_tree_rbtree_find_index              (GtkTreeRBTree                 *tree,
                                                         guint                          index,
                                                         GtkTreeRBTree                **new_tree,
                                                         GtkTreeRBNode                **new_node);
int             gtk_tree_rbtree_find_offset             (GtkTreeRBTree                 *tree,
                                                         int                            offset,
                                                         GtkTreeRBTree                **new_tree,
                                                         GtkTreeRBNode                **new_node);
void            gtk_tree_rbtree_traverse                (GtkTreeRBTree                 *tree,
                                                         GtkTreeRBNode                 *node,
                                                         GTraverseType                  order,
                                                         GtkTreeRBTreeTraverseFunc      func,
                                                         gpointer                       data);
GtkTreeRBNode * gtk_tree_rbtree_first                   (GtkTreeRBTree                 *tree);
GtkTreeRBNode * gtk_tree_rbtree_next                    (GtkTreeRBTree                 *tree,
                                                         GtkTreeRBNode                 *node);
GtkTreeRBNode * gtk_tree_rbtree_prev                    (GtkTreeRBTree                 *tree,
                                                         GtkTreeRBNode                 *node);
void            gtk_tree_rbtree_next_full               (GtkTreeRBTree                 *tree,
                                                         GtkTreeRBNode                 *node,
                                                         GtkTreeRBTree                **new_tree,
                                                         GtkTreeRBNode                **new_node);
void            gtk_tree_rbtree_prev_full               (GtkTreeRBTree                 *tree,
                                                         GtkTreeRBNode                 *node,
                                                         GtkTreeRBTree                **new_tree,
                                                         GtkTreeRBNode                **new_node);

int             gtk_tree_rbtree_get_depth               (GtkTreeRBTree                 *tree);


G_END_DECLS


