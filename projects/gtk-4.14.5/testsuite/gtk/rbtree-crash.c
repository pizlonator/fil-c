/* GtkRBTree tests.
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

#include <gtk/gtk.h>

#include "gtk/gtkrbtreeprivate.h"

typedef struct _Node Node;
typedef struct _Aug Aug;

struct _Node {
  guint unused;
};

struct _Aug {
  guint n_items;
};

static void
augment (GtkRbTree *tree,
         gpointer   _aug,
         gpointer   _node,
         gpointer   left,
         gpointer   right)
{
  Aug *aug = _aug;
  
  aug->n_items = 1;

  if (left)
    {
      Aug *left_aug = gtk_rb_tree_get_augment (tree, left);

      aug->n_items += left_aug->n_items;
    }

  if (right)
    {
      Aug *right_aug = gtk_rb_tree_get_augment (tree, right);

      aug->n_items += right_aug->n_items;
    }
}

static Node *
get (GtkRbTree *tree,
     guint      pos)
{
  Node *node, *tmp;

  node = gtk_rb_tree_get_root (tree);

  while (node)
    {
      tmp = gtk_rb_tree_node_get_left (node);
      if (tmp)
        {
          Aug *aug = gtk_rb_tree_get_augment (tree, tmp);
          if (pos < aug->n_items)
            {
              node = tmp;
              continue;
            }
          pos -= aug->n_items;
        }

      if (pos < 1)
        break;
      pos--;

      node = gtk_rb_tree_node_get_right (node);
    }

  return node;
}

static void
add (GtkRbTree *tree,
     guint      pos)
{
  Node *node = get (tree, pos);

  gtk_rb_tree_insert_before (tree, node);
}

static void
delete (GtkRbTree *tree,
        guint      pos)
{
  Node *node = get (tree, pos);

  gtk_rb_tree_remove (tree, node);
}

#if 0
static guint
print_node (GtkRbTree  *tree,
            Node       *node,
            guint       depth,
            const char *prefix,
            guint       n)
{
  Node *child;

  child = gtk_rb_tree_node_get_left (node);
  if (child)
    n = print_node (tree, child, depth + 1, "/", n);
  g_print ("%*s %u\n", 2 * depth, prefix, n);
  n++;
  child = gtk_rb_tree_node_get_right (node);
  if (child)
    n = print_node (tree, child, depth + 1, "\\", n);

  return n;
}

static void
print (GtkRbTree *tree)
{
  print_node (tree, gtk_rb_tree_get_root (tree), 0, "", 0);
}
#endif

static void
test_crash (void)
{
  GtkRbTree *tree;
  guint i;

  tree = gtk_rb_tree_new (Node, Aug, augment, NULL, NULL);

  for (i = 0; i < 300; i++)
    add (tree, i);
  //print (tree);
  delete (tree, 144);
  add (tree, 56);
  delete (tree, 113);
  delete (tree, 278);
  delete (tree, 45);
  delete (tree, 108);
  delete (tree, 41);
  add (tree, 56);
  add (tree, 200);
  delete (tree, 127);
  delete (tree, 222);
  add (tree, 80);
  add (tree, 143);
  add (tree, 216);
  delete (tree, 177);
  delete (tree, 193);
  add (tree, 190);
  delete (tree, 288);
  add (tree, 45);
  add (tree, 57);
  add (tree, 211);
  delete (tree, 103);
  add (tree, 152);
  delete (tree, 60);
  add (tree, 185);
  delete (tree, 167);
  add (tree, 92);
  delete (tree, 104);
  delete (tree, 110);
  delete (tree, 115);
  add (tree, 32);
  delete (tree, 44);
  add (tree, 159);
  add (tree, 271);
  delete (tree, 35);
  add (tree, 250);
  delete (tree, 36);
  add (tree, 284);
  delete (tree, 82);
  delete (tree, 248);
  add (tree, 22);
  delete (tree, 284);
  add (tree, 88);
  delete (tree, 182);
  add (tree, 70);
  add (tree, 55);
  delete (tree, 6);
  add (tree, 85);
  delete (tree, 36);
  delete (tree, 33);
  delete (tree, 108);
  add (tree, 229);
  delete (tree, 269);
  add (tree, 20);
  add (tree, 170);
  delete (tree, 154);
  add (tree, 26);
  add (tree, 211);
  delete (tree, 167);
  add (tree, 183);
  add (tree, 292);
  delete (tree, 2);
  add (tree, 5);
  delete (tree, 14);
  delete (tree, 91);
  add (tree, 172);
  add (tree, 99);
  delete (tree, 3);
  delete (tree, 74);
  delete (tree, 122);
  add (tree, 87);
  add (tree, 176);
  delete (tree, 294);
  add (tree, 169);
  delete (tree, 41);
  add (tree, 95);
  delete (tree, 185);
  add (tree, 218);
  delete (tree, 62);
  delete (tree, 175);
  add (tree, 196);
  delete (tree, 33);
  delete (tree, 46);
  add (tree, 30);
  add (tree, 72);
  delete (tree, 196);
  delete (tree, 291);
  add (tree, 198);
  delete (tree, 181);
  add (tree, 105);
  delete (tree, 75);
  add (tree, 30);
  add (tree, 261);
  delete (tree, 284);
  delete (tree, 214);
  delete (tree, 134);
  add (tree, 153);
  delete (tree, 46);
  add (tree, 154);
  add (tree, 266);
  delete (tree, 272);
  delete (tree, 150);
  add (tree, 131);
  delete (tree, 208);
  add (tree, 241);
  add (tree, 31);
  add (tree, 151);
  add (tree, 266);
  delete (tree, 285);
  add (tree, 178);
  add (tree, 159);
  add (tree, 203);
  delete (tree, 266);
  add (tree, 52);
  delete (tree, 104);
  add (tree, 243);
  delete (tree, 12);
  add (tree, 20);
  delete (tree, 68);
  //print (tree);
  delete (tree, 102);

  gtk_rb_tree_unref (tree);
}

static void
test_crash2 (void)
{
  GtkRbTree *tree;

  tree = gtk_rb_tree_new (Node, Aug, augment, NULL, NULL);

  add (tree, 0);
  add (tree, 0);
  add (tree, 1);

  gtk_rb_tree_unref (tree);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  g_test_add_func ("/rbtree/crash", test_crash);
  g_test_add_func ("/rbtree/crash2", test_crash2);

  return g_test_run ();
}
