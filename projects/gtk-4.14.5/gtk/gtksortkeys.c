/*
 * Copyright © 2020 Benjamin Otte
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
 */

#include "config.h"

#include "gtksortkeysprivate.h"

#include "gtkcssstyleprivate.h"
#include "gtkstyleproviderprivate.h"

GtkSortKeys *
gtk_sort_keys_alloc (const GtkSortKeysClass *klass,
                     gsize                   size,
                     gsize                   key_size,
                     gsize                   key_align)
{
  GtkSortKeys *self;

  g_return_val_if_fail (key_align > 0, NULL);

  self = g_malloc0 (size);

  self->klass = klass;
  self->ref_count = 1;

  self->key_size = key_size;
  self->key_align = key_align;

  return self;
}

GtkSortKeys *
gtk_sort_keys_ref (GtkSortKeys *self)
{
  self->ref_count += 1;

  return self;
}

void
gtk_sort_keys_unref (GtkSortKeys *self)
{
  self->ref_count -= 1;
  if (self->ref_count > 0)
    return;

  self->klass->free (self);
}

gsize
gtk_sort_keys_get_key_size (GtkSortKeys *self)
{
  return self->key_size;
}

gsize
gtk_sort_keys_get_key_align (GtkSortKeys *self)
{
  return self->key_align;
}

GCompareDataFunc
gtk_sort_keys_get_key_compare_func (GtkSortKeys *self)
{
  return self->klass->key_compare;
}

gboolean
gtk_sort_keys_is_compatible (GtkSortKeys *self,
                             GtkSortKeys *other)
{
  if (self == other)
    return TRUE;

  return self->klass->is_compatible (self, other);
}

gboolean
gtk_sort_keys_needs_clear_key (GtkSortKeys *self)
{
  return self->klass->clear_key != NULL;
}

static void
gtk_equal_sort_keys_free (GtkSortKeys *keys)
{
  g_free (keys);
}

static int
gtk_equal_sort_keys_compare (gconstpointer a,
                             gconstpointer b,
                             gpointer      unused)
{
  return GTK_ORDERING_EQUAL;
}

static gboolean
gtk_equal_sort_keys_is_compatible (GtkSortKeys *keys,
                                   GtkSortKeys *other)
{
  return keys->klass == other->klass;
}

static void
gtk_equal_sort_keys_init_key (GtkSortKeys *keys,
                               gpointer     item,
                               gpointer     key_memory)
{
}

static const GtkSortKeysClass GTK_EQUAL_SORT_KEYS_CLASS =
{
  gtk_equal_sort_keys_free,
  gtk_equal_sort_keys_compare,
  gtk_equal_sort_keys_is_compatible,
  gtk_equal_sort_keys_init_key,
  NULL
};

/*<private>
 * gtk_sort_keys_new_equal:
 *
 * Creates a new GtkSortKeys that compares every element as equal.
 * This is useful when sorters are in an invalid configuration.
 *
 * Returns: a new GtkSortKeys
 **/
GtkSortKeys *
gtk_sort_keys_new_equal (void)
{
  return gtk_sort_keys_new (GtkSortKeys,
                            &GTK_EQUAL_SORT_KEYS_CLASS,
                            0, 1);
}

