/*
 * Copyright © 2019 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkcustomfilter.h"

#include "gtktypebuiltins.h"

/**
 * GtkCustomFilter:
 *
 * `GtkCustomFilter` determines whether to include items with a callback.
 */
struct _GtkCustomFilter
{
  GtkFilter parent_instance;

  GtkCustomFilterFunc match_func;
  gpointer user_data;
  GDestroyNotify user_destroy;
};

G_DEFINE_TYPE (GtkCustomFilter, gtk_custom_filter, GTK_TYPE_FILTER)

static gboolean
gtk_custom_filter_match (GtkFilter *filter,
                         gpointer   item)
{
  GtkCustomFilter *self = GTK_CUSTOM_FILTER (filter);

  if (!self->match_func)
    return TRUE;

  return self->match_func (item, self->user_data);
}

static GtkFilterMatch
gtk_custom_filter_get_strictness (GtkFilter *filter)
{
  GtkCustomFilter *self = GTK_CUSTOM_FILTER (filter);

  if (!self->match_func)
    return GTK_FILTER_MATCH_ALL;

  return GTK_FILTER_MATCH_SOME;
}

static void
gtk_custom_filter_dispose (GObject *object)
{
  GtkCustomFilter *self = GTK_CUSTOM_FILTER (object);

  if (self->user_destroy)
    self->user_destroy (self->user_data);

  G_OBJECT_CLASS (gtk_custom_filter_parent_class)->dispose (object);
}

static void
gtk_custom_filter_class_init (GtkCustomFilterClass *class)
{
  GtkFilterClass *filter_class = GTK_FILTER_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  filter_class->match = gtk_custom_filter_match;
  filter_class->get_strictness = gtk_custom_filter_get_strictness;

  object_class->dispose = gtk_custom_filter_dispose;
}

static void
gtk_custom_filter_init (GtkCustomFilter *self)
{
}

/**
 * gtk_custom_filter_new:
 * @match_func: (nullable): function to filter items
 * @user_data: (nullable): user data to pass to @match_func
 * @user_destroy: destroy notify for @user_data
 *
 * Creates a new filter using the given @match_func to filter
 * items.
 *
 * If @match_func is %NULL, the filter matches all items.
 *
 * If the filter func changes its filtering behavior,
 * gtk_filter_changed() needs to be called.
 *
 * Returns: a new `GtkCustomFilter`
 **/
GtkCustomFilter *
gtk_custom_filter_new (GtkCustomFilterFunc match_func,
                       gpointer            user_data,
                       GDestroyNotify      user_destroy)
{
  GtkCustomFilter *result;

  result = g_object_new (GTK_TYPE_CUSTOM_FILTER, NULL);

  gtk_custom_filter_set_filter_func (result, match_func, user_data, user_destroy);

  return result;
}

/**
 * gtk_custom_filter_set_filter_func:
 * @self: a `GtkCustomFilter`
 * @match_func: (nullable): function to filter items
 * @user_data: (nullable): user data to pass to @match_func
 * @user_destroy: destroy notify for @user_data
 *
 * Sets the function used for filtering items.
 *
 * If @match_func is %NULL, the filter matches all items.
 *
 * If the filter func changes its filtering behavior,
 * gtk_filter_changed() needs to be called.
 *
 * If a previous function was set, its @user_destroy will be
 * called now.
 */
void
gtk_custom_filter_set_filter_func (GtkCustomFilter     *self,
                                   GtkCustomFilterFunc  match_func,
                                   gpointer             user_data,
                                   GDestroyNotify       user_destroy)
{
  g_return_if_fail (GTK_IS_CUSTOM_FILTER (self));
  g_return_if_fail (match_func || (user_data == NULL && !user_destroy));

  if (self->user_destroy)
    self->user_destroy (self->user_data);

  self->match_func = match_func;
  self->user_data = user_data;
  self->user_destroy = user_destroy;

  gtk_filter_changed (GTK_FILTER (self), GTK_FILTER_CHANGE_DIFFERENT);
}
