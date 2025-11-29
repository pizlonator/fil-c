/* gtkrbtree.h
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


typedef struct _GtkRbTree GtkRbTree;

typedef void            (* GtkRbTreeAugmentFunc)        (GtkRbTree               *tree,
                                                         gpointer                 node_augment,
                                                         gpointer                 node,
                                                         gpointer                 left,
                                                         gpointer                 right);

GtkRbTree *          gtk_rb_tree_new_for_size           (gsize                    element_size,
                                                         gsize                    augment_size,
                                                         GtkRbTreeAugmentFunc     augment_func,
                                                         GDestroyNotify           clear_func,
                                                         GDestroyNotify           clear_augment_func);
#define gtk_rb_tree_new(type, augment_type, augment_func, clear_func, clear_augment_func) \
  gtk_rb_tree_new_for_size (sizeof (type), sizeof (augment_type), (augment_func), (clear_func), (clear_augment_func))

GtkRbTree *          gtk_rb_tree_ref                    (GtkRbTree               *tree);
void                 gtk_rb_tree_unref                  (GtkRbTree               *tree);

gpointer             gtk_rb_tree_get_root               (GtkRbTree               *tree);
gpointer             gtk_rb_tree_get_first              (GtkRbTree               *tree);
gpointer             gtk_rb_tree_get_last               (GtkRbTree               *tree);

gpointer             gtk_rb_tree_node_get_previous      (gpointer                 node);
gpointer             gtk_rb_tree_node_get_next          (gpointer                 node);
gpointer             gtk_rb_tree_node_get_parent        (gpointer                 node);
gpointer             gtk_rb_tree_node_get_left          (gpointer                 node);
gpointer             gtk_rb_tree_node_get_right         (gpointer                 node);
GtkRbTree *          gtk_rb_tree_node_get_tree          (gpointer                 node);
void                 gtk_rb_tree_node_mark_dirty        (gpointer                 node);

gpointer             gtk_rb_tree_get_augment            (GtkRbTree               *tree,
                                                         gpointer                 node);

gpointer             gtk_rb_tree_insert_before          (GtkRbTree               *tree,
                                                         gpointer                 node);
gpointer             gtk_rb_tree_insert_after           (GtkRbTree               *tree,
                                                         gpointer                 node);
void                 gtk_rb_tree_remove                 (GtkRbTree               *tree,
                                                         gpointer                 node);
void                 gtk_rb_tree_remove_all             (GtkRbTree               *tree);


G_END_DECLS


