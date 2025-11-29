/* gtkrbtree.c
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

#include "gtkrbtreeprivate.h"

#include "gtkdebug.h"

/* Define the following to print adds and removals to stdout.
 * The format of the printout will be suitable for addition as a new test to
 * testsuite/gtk/rbtree-crash.c
 * by just grepping the printouts from the relevant rbtree.
 *
 * This is meant to be a trivial way to add rbtree tests to the testsuite.
 */
#undef DUMP_MODIFICATION

typedef struct _GtkRbNode GtkRbNode;

struct _GtkRbTree
{
  guint ref_count;

  gsize element_size;
  gsize augment_size;
  GtkRbTreeAugmentFunc augment_func;
  GDestroyNotify clear_func;
  GDestroyNotify clear_augment_func;

  GtkRbNode *root;
};

struct _GtkRbNode
{
  guint red :1;
  guint dirty :1;

  GtkRbNode *left;
  GtkRbNode *right;
  /* The difference between tree and parent here is that we OR the tree with 1 and because
   * pointers are always multiples of 4, we can know if we've stored a parent or the tree here */
  union {
    gpointer parent_or_tree;
    GtkRbNode *parent;
    GtkRbTree *tree;
  };
};

#define NODE_FROM_POINTER(ptr) ((GtkRbNode *) (((guchar *) (ptr)) - sizeof (GtkRbNode)))
#define NODE_TO_POINTER(node) ((node) ? ((gpointer) (((guchar *) (node)) + sizeof (GtkRbNode))) : NULL)
#define NODE_TO_AUG_POINTER(tree, node) ((gpointer) ((node) ? (((guchar *) (node)) + sizeof (GtkRbNode) + (tree)->element_size) : NULL))

static inline gboolean
is_root (GtkRbNode *node)
{
  return GPOINTER_TO_SIZE (node->parent_or_tree) & 1 ? TRUE : FALSE;
}

static inline GtkRbNode *
parent (GtkRbNode *node)
{
  if (is_root (node))
    return NULL;
  else
    return node->parent;
}

static GtkRbTree *
tree (GtkRbNode *node)
{
  while (!is_root (node))
    node = parent (node);

  return GSIZE_TO_POINTER (GPOINTER_TO_SIZE (node->tree) & ~1);
}

static void
set_parent (GtkRbTree *tree,
            GtkRbNode *node,
            GtkRbNode *new_parent)
{

  if (new_parent != NULL)
    {
      node->parent = new_parent;
    }
  else
    {
      node->tree = GSIZE_TO_POINTER (GPOINTER_TO_SIZE (tree) | 1);
      tree->root = node;
    }
}

static inline gsize
gtk_rb_node_get_size (GtkRbTree *tree)
{
  return sizeof (GtkRbNode) + tree->element_size + tree->augment_size;
}

static GtkRbNode *
gtk_rb_node_new (GtkRbTree *tree)
{
  GtkRbNode *result;

  result = g_malloc0 (gtk_rb_node_get_size (tree));

  result->red = TRUE;
  result->dirty = TRUE;

  return result;
}

static void
gtk_rb_node_free (GtkRbTree *tree,
                  GtkRbNode *node)
{
  if (tree->clear_func)
    tree->clear_func (NODE_TO_POINTER (node));
  if (tree->clear_augment_func)
    tree->clear_augment_func (NODE_TO_AUG_POINTER (tree, node));

  g_free (node);
}

static void
gtk_rb_node_free_deep (GtkRbTree *tree,
                       GtkRbNode *node)
{
  GtkRbNode *right = node->right;

  if (node->left)
    gtk_rb_node_free_deep (tree, node->left);

  gtk_rb_node_free (tree, node);

  if (right)
    gtk_rb_node_free_deep (tree, right);
}

static void
gtk_rb_node_mark_dirty (GtkRbNode *node,
                        gboolean   mark_parent)
{
  if (node->dirty)
    return;
  
  node->dirty = TRUE;

  if (mark_parent && parent (node))
    gtk_rb_node_mark_dirty (parent (node), TRUE);
}

static void
gtk_rb_node_clean (GtkRbTree *tree,
                   GtkRbNode *node)
{
  if (!node->dirty)
    return;

  node->dirty = FALSE;
  if (tree->augment_func)
    tree->augment_func (tree,
                        NODE_TO_AUG_POINTER (tree, node),
                        NODE_TO_POINTER (node),
                        NODE_TO_POINTER (node->left),
                        NODE_TO_POINTER (node->right));
}

static GtkRbNode *
gtk_rb_node_get_first (GtkRbNode *node)
{
  while (node->left)
    node = node->left;

  return node;
}

static GtkRbNode *
gtk_rb_node_get_last (GtkRbNode *node)
{
  while (node->right)
    node = node->right;

  return node;
}

static GtkRbNode *
gtk_rb_node_get_previous (GtkRbNode *node)
{
  GtkRbNode *p;

  if (node->left)
    return gtk_rb_node_get_last (node->left);

  for (p = parent (node); p != NULL; p = parent (node))
    {
      if (p->right == node)
        return p;

      node = p;
    }

  return NULL;
}

static GtkRbNode *
gtk_rb_node_get_next (GtkRbNode *node)
{
  GtkRbNode *p;

  if (node->right)
    return gtk_rb_node_get_first (node->right);

  for (p = parent (node); p != NULL; p = parent (node))
    {
      if (p->left == node)
        return p;

      node = p;
    }

  return NULL;
}

#ifdef DUMP_MODIFICATION
static guint
position (GtkRbTree *tree,
          GtkRbNode *node)
{
  GtkRbNode *n;
  guint i;

  i = 0;
  for (n = gtk_rb_node_get_first (tree->root);
       n != node;
       n = gtk_rb_node_get_next (n))
    i++;

  return i;
}
#endif

static void
gtk_rb_node_rotate_left (GtkRbTree *tree,
                         GtkRbNode *node)
{
  GtkRbNode *right, *p;

  right = node->right;
  p = parent (node);

  node->right = right->left;
  if (right->left)
    set_parent (tree, right->left, node);

  set_parent (tree, right, p);
  if (p)
    {
      if (node == p->left)
	p->left = right;
      else
	p->right = right;
    }

  right->left = node;
  set_parent (tree, node, right);

  gtk_rb_node_mark_dirty (node, FALSE);
  gtk_rb_node_mark_dirty (right, FALSE);
}

static void
gtk_rb_node_rotate_right (GtkRbTree *tree,
                          GtkRbNode *node)
{
  GtkRbNode *left, *p;

  left = node->left;
  p = parent (node);

  node->left = left->right;
  if (left->right)
    set_parent (tree, left->right, node);

  set_parent (tree, left, p);
  if (p)
    {
      if (node == p->right)
	p->right = left;
      else
	p->left = left;
    }

  /* link node and left */
  left->right = node;
  set_parent (tree, node, left);

  gtk_rb_node_mark_dirty (node, FALSE);
  gtk_rb_node_mark_dirty (left, FALSE);
}

static gboolean
is_red (GtkRbNode *node_or_null)
{
  if (node_or_null == NULL)
    return FALSE;
  else
    return node_or_null->red;
}

static inline gboolean
is_black (GtkRbNode *node_or_null)
{
  return !is_red (node_or_null);
}

static void
set_black (GtkRbNode *node_or_null)
{
  if (node_or_null == NULL)
    return;

  node_or_null->red = FALSE;
}

static void
set_red (GtkRbNode *node_or_null)
{
  if (node_or_null == NULL)
    return;

  node_or_null->red = TRUE;
}

static void
gtk_rb_tree_insert_fixup (GtkRbTree *tree,
                          GtkRbNode *node)
{
  GtkRbNode *p;

  /* check Red-Black properties */
  for (p = parent (node);
       p && is_red (p);
       p = parent (node))
    {
      GtkRbNode *pp = parent (p);

      /* we have a violation */
      g_assert (pp);

      if (p == pp->left)
	{
	  GtkRbNode *uncle = pp->right;

	  if (is_red (uncle))
	    {
	      /* uncle is red */
	      set_black (p);
              set_black (uncle);
              set_red (pp);
	      node = pp;
	    }
	  else
	    {
	      /* uncle is black */
	      if (node == p->right)
		{
		  /* make node a left child */
		  gtk_rb_node_rotate_left (tree, p);
		  p = node;
                  node = p->left;
		}
	      /* recolor and rotate */
              set_black (p);
              set_red (pp);
	      gtk_rb_node_rotate_right (tree, pp);
	    }
	}
      else
	{
	  /* mirror image of above code */
	  GtkRbNode *uncle = pp->left;

	  if (is_red (uncle))
	    {
	      /* uncle is red */
              set_black (p);
              set_black (uncle);
              set_red (pp);
	      node = pp;
	    }
	  else
	    {
              /* uncle is black */
	      if (node == p->left)
		{
		  gtk_rb_node_rotate_right (tree, p);
		  p = node;
                  node = p->right;
		}
	      set_black (p);
	      set_red (pp);
	      gtk_rb_node_rotate_left (tree, pp);
	    }
	}
    }

  set_black (tree->root);
}

static void
gtk_rb_tree_remove_node_fixup (GtkRbTree *tree,
                               GtkRbNode *node,
                               GtkRbNode *p)
{
  while (node != tree->root && is_black (node))
    {
      if (node == p->left)
	{
	  GtkRbNode *w = p->right;

	  if (is_red (w))
	    {
	      set_black (w);
              set_red (p);
	      gtk_rb_node_rotate_left (tree, p);
	      w = p->right;
	    }
          g_assert (w);
	  if (is_black (w->left) && is_black (w->right))
	    {
	      set_red (w);
	      node = p;
	    }
	  else
	    {
	      if (is_black (w->right))
		{
		  set_black (w->left);
		  set_red (w);
		  gtk_rb_node_rotate_right (tree, w);
		  w = p->right;
		}
	      w->red = p->red;
	      set_black (p);
              set_black (w->right);
	      gtk_rb_node_rotate_left (tree, p);
	      node = tree->root;
	    }
	}
      else
	{
	  GtkRbNode *w = p->left;
	  if (is_red (w))
	    {
	      set_black (w);
	      set_red (p);
	      gtk_rb_node_rotate_right (tree, p);
	      w = p->left;
	    }
          g_assert (w);
	  if (is_black (w->right) && is_black (w->left))
	    {
	      set_red (w);
	      node = p;
	    }
	  else
	    {
	      if (is_black (w->left))
		{
		  set_black (w->right);
		  set_red (w);
		  gtk_rb_node_rotate_left (tree, w);
		  w = p->left;
		}
	      w->red = p->red;
	      set_black (p);
	      set_black (w->left);
	      gtk_rb_node_rotate_right (tree, p);
	      node = tree->root;
	    }
	}

      p = parent (node);
    }

  set_black (node);
}

GtkRbTree *
gtk_rb_tree_new_for_size (gsize                element_size,
                          gsize                augment_size,
                          GtkRbTreeAugmentFunc augment_func,
                          GDestroyNotify       clear_func,
                          GDestroyNotify       clear_augment_func)
{
  GtkRbTree *tree;

  tree = g_new0 (GtkRbTree, 1);
  tree->ref_count = 1;

  tree->element_size = element_size;
  tree->augment_size = augment_size;
  tree->augment_func = augment_func;
  tree->clear_func = clear_func;
  tree->clear_augment_func = clear_augment_func;

  return tree;
}

GtkRbTree *
gtk_rb_tree_ref (GtkRbTree *tree)
{
  tree->ref_count++;

  return tree;
}

void
gtk_rb_tree_unref (GtkRbTree *tree)
{
  tree->ref_count--;
  if (tree->ref_count > 0)
    return;

  if (tree->root)
    gtk_rb_node_free_deep (tree, tree->root);

  g_free (tree);
}

gpointer
gtk_rb_tree_get_first (GtkRbTree *tree)
{
  if (tree->root == NULL)
    return NULL;

  return NODE_TO_POINTER (gtk_rb_node_get_first (tree->root));
}

gpointer
gtk_rb_tree_get_last (GtkRbTree *tree)
{
  if (tree->root == NULL)
    return NULL;

  return NODE_TO_POINTER (gtk_rb_node_get_last (tree->root));
}

gpointer
gtk_rb_tree_node_get_previous (gpointer node)
{
  return NODE_TO_POINTER (gtk_rb_node_get_previous (NODE_FROM_POINTER (node)));
}

gpointer
gtk_rb_tree_node_get_next (gpointer node)
{
  return NODE_TO_POINTER (gtk_rb_node_get_next (NODE_FROM_POINTER (node)));
}

gpointer
gtk_rb_tree_get_root (GtkRbTree *tree)
{
  return NODE_TO_POINTER (tree->root);
}

gpointer
gtk_rb_tree_node_get_parent (gpointer node)
{
  return NODE_TO_POINTER (parent (NODE_FROM_POINTER (node)));
}

gpointer
gtk_rb_tree_node_get_left (gpointer node)
{
  return NODE_TO_POINTER (NODE_FROM_POINTER (node)->left);
}

gpointer
gtk_rb_tree_node_get_right (gpointer node)
{
  return NODE_TO_POINTER (NODE_FROM_POINTER (node)->right);
}

gpointer
gtk_rb_tree_get_augment (GtkRbTree *tree,
                         gpointer   node)
{
  GtkRbNode *rbnode = NODE_FROM_POINTER (node);

  gtk_rb_node_clean (tree, rbnode);

  return NODE_TO_AUG_POINTER (tree, rbnode);
}

GtkRbTree *
gtk_rb_tree_node_get_tree (gpointer node)
{
  return tree (NODE_FROM_POINTER (node));
}

void
gtk_rb_tree_node_mark_dirty (gpointer node)
{
  gtk_rb_node_mark_dirty (NODE_FROM_POINTER (node), TRUE);
}

gpointer
gtk_rb_tree_insert_before (GtkRbTree *tree,
                           gpointer   node)
{
  GtkRbNode *result;


  if (tree->root == NULL)
    {
#ifdef DUMP_MODIFICATION
      g_print ("add (tree, 0); /* 0x%p */\n", tree);
#endif /* DUMP_MODIFICATION */

      g_assert (node == NULL);

      result = gtk_rb_node_new (tree);
      tree->root = result;
    }
  else if (node == NULL)
    {
      return gtk_rb_tree_insert_after (tree, gtk_rb_tree_get_last (tree));
    }
  else
    {
      GtkRbNode *current = NODE_FROM_POINTER (node);

#ifdef DUMP_MODIFICATION
      g_print ("add (tree, %u); /* 0x%p */\n", position (tree, current), tree);
#endif /* DUMP_MODIFICATION */

      /* setup new node */
      result = gtk_rb_node_new (tree);

      if (current->left)
        {
          current = gtk_rb_node_get_last (current->left);
          current->right = result;
        }
      else
        {
          current->left = result;
        }
      set_parent (tree, result, current);
      gtk_rb_node_mark_dirty (current, TRUE);
    }

  gtk_rb_tree_insert_fixup (tree, result);

  return NODE_TO_POINTER (result);
}

gpointer
gtk_rb_tree_insert_after (GtkRbTree *tree,
                          gpointer   node)
{
  GtkRbNode *current, *result;

  if (node == NULL)
    return gtk_rb_tree_insert_before (tree, gtk_rb_tree_get_first (tree));

  current = NODE_FROM_POINTER (node);

#ifdef DUMP_MODIFICATION
  g_print ("add (tree, %u); /* 0x%p */\n", position (tree, current) + 1, tree);
#endif /* DUMP_MODIFICATION */

  /* setup new node */
  result = gtk_rb_node_new (tree);

  if (current->right)
    {
      current = gtk_rb_node_get_first (current->right);
      current->left = result;
    }
  else
    {
      current->right = result;
    }
  set_parent (tree, result, current);
  gtk_rb_node_mark_dirty (current, TRUE);

  gtk_rb_tree_insert_fixup (tree, result);

  return NODE_TO_POINTER (result);
}

void
gtk_rb_tree_remove (GtkRbTree *tree,
                    gpointer   node)
{
  GtkRbNode *x, *y, *p, *real_node;
  
  real_node = NODE_FROM_POINTER (node);

#ifdef DUMP_MODIFICATION
  g_print ("delete (tree, %u); /* 0x%p */\n", position (tree, real_node), tree);
#endif /* DUMP_MODIFICATION */

  y = real_node;
  if (y->left && y->right)
    {
      y = y->right;

      while (y->left)
	y = y->left;
    }

  /* x is y's only child, or nil */
  if (y->left)
    x = y->left;
  else
    x = y->right;

  /* remove y from the parent chain */
  p = parent (y);
  if (x != NULL)
    set_parent (tree, x, p);
  if (p)
    {
      if (y == p->left)
	p->left = x;
      else
	p->right = x;
      gtk_rb_node_mark_dirty (p, TRUE);
    }
  else
    {
      if (x == NULL)
        tree->root = NULL;
    }

  /* We need to clean up the validity of the tree.
   */
  if (is_black (y))
    gtk_rb_tree_remove_node_fixup (tree, x, p);

  if (y != real_node)
    {
      /* Move the node over */
      if (is_red (real_node) != is_red (y))
	y->red = !y->red;

      y->left = real_node->left;
      if (y->left)
        set_parent (tree, y->left, y);
      y->right = real_node->right;
      if (y->right)
        set_parent (tree, y->right, y);
      p = parent (real_node);
      set_parent (tree, y, p);
      if (p)
        {
          if (p->left == real_node)
            p->left = y;
          else
            p->right = y;
          gtk_rb_node_mark_dirty (p, TRUE);
        }
      gtk_rb_node_mark_dirty (y, TRUE);
    }

  gtk_rb_node_free (tree, real_node);
}

void
gtk_rb_tree_remove_all (GtkRbTree *tree)
{
#ifdef DUMP_MODIFICATION
      g_print ("delete_all (tree); /* 0x%p */\n", tree);
#endif /* DUMP_MODIFICATION */

  if (tree->root)
    gtk_rb_node_free_deep (tree, tree->root);

  tree->root = NULL;
}

