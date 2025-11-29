/*
 * Copyright © 2019 Matthias Clasen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtkcustomsorter.h"

#include "gtktypebuiltins.h"

/**
 * GtkCustomSorter:
 *
 * `GtkCustomSorter` is a `GtkSorter` implementation that sorts via a callback
 * function.
 */
struct _GtkCustomSorter
{
  GtkSorter parent_instance;

  GCompareDataFunc sort_func;
  gpointer user_data;
  GDestroyNotify user_destroy;
};

G_DEFINE_TYPE (GtkCustomSorter, gtk_custom_sorter, GTK_TYPE_SORTER)

static GtkOrdering
gtk_custom_sorter_compare (GtkSorter *sorter,
                           gpointer   item1,
                           gpointer   item2)
{
  GtkCustomSorter *self = GTK_CUSTOM_SORTER (sorter);

  if (!self->sort_func)
    return GTK_ORDERING_EQUAL;

  return gtk_ordering_from_cmpfunc (self->sort_func (item1, item2, self->user_data));
}

static GtkSorterOrder
gtk_custom_sorter_get_order (GtkSorter *sorter)
{
  GtkCustomSorter *self = GTK_CUSTOM_SORTER (sorter);

  if (!self->sort_func)
    return GTK_SORTER_ORDER_NONE;

  return GTK_SORTER_ORDER_PARTIAL;
}

static void
gtk_custom_sorter_dispose (GObject *object)
{
  GtkCustomSorter *self = GTK_CUSTOM_SORTER (object);

  if (self->user_destroy)
    self->user_destroy (self->user_data);

  self->sort_func = NULL;
  self->user_destroy = NULL;
  self->user_data = NULL;

  G_OBJECT_CLASS (gtk_custom_sorter_parent_class)->dispose (object);
}

static void
gtk_custom_sorter_class_init (GtkCustomSorterClass *class)
{
  GtkSorterClass *sorter_class = GTK_SORTER_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  sorter_class->compare = gtk_custom_sorter_compare;
  sorter_class->get_order = gtk_custom_sorter_get_order;

  object_class->dispose = gtk_custom_sorter_dispose;
}

static void
gtk_custom_sorter_init (GtkCustomSorter *self)
{
}

/**
 * gtk_custom_sorter_new:
 * @sort_func: (nullable): the `GCompareDataFunc` to use for sorting
 * @user_data: (nullable): user data to pass to @sort_func
 * @user_destroy: (nullable): destroy notify for @user_data
 *
 * Creates a new `GtkSorter` that works by calling
 * @sort_func to compare items.
 *
 * If @sort_func is %NULL, all items are considered equal.
 *
 * Returns: a new `GtkCustomSorter`
 */
GtkCustomSorter *
gtk_custom_sorter_new (GCompareDataFunc sort_func,
                       gpointer         user_data,
                       GDestroyNotify   user_destroy)
{
  GtkCustomSorter *sorter;

  sorter = g_object_new (GTK_TYPE_CUSTOM_SORTER, NULL);

  gtk_custom_sorter_set_sort_func (sorter, sort_func, user_data, user_destroy);

  return sorter;
}

/**
 * gtk_custom_sorter_set_sort_func:
 * @self: a `GtkCustomSorter`
 * @sort_func: (nullable): function to sort items
 * @user_data: (nullable): user data to pass to @match_func
 * @user_destroy: destroy notify for @user_data
 *
 * Sets (or unsets) the function used for sorting items.
 *
 * If @sort_func is %NULL, all items are considered equal.
 *
 * If the sort func changes its sorting behavior,
 * gtk_sorter_changed() needs to be called.
 *
 * If a previous function was set, its @user_destroy will be
 * called now.
 */
void
gtk_custom_sorter_set_sort_func (GtkCustomSorter  *self,
                                 GCompareDataFunc  sort_func,
                                 gpointer          user_data,
                                 GDestroyNotify    user_destroy)
{
  g_return_if_fail (GTK_IS_CUSTOM_SORTER (self));
  g_return_if_fail (sort_func || (user_data == NULL && !user_destroy));

  if (self->user_destroy)
    self->user_destroy (self->user_data);

  self->sort_func = sort_func;
  self->user_data = user_data;
  self->user_destroy = user_destroy;

  gtk_sorter_changed (GTK_SORTER (self), GTK_SORTER_CHANGE_DIFFERENT);
}
