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

#pragma once

#include <gdk/gdk.h>
#include <gtk/gtkenums.h>
#include <gtk/gtksorter.h>

typedef struct _GtkSortKeys GtkSortKeys;
typedef struct _GtkSortKeysClass GtkSortKeysClass;

struct _GtkSortKeys
{
  const GtkSortKeysClass *klass;
  int ref_count;

  gsize key_size;
  gsize key_align; /* must be power of 2 */
};

struct _GtkSortKeysClass
{
  void                  (* free)                                (GtkSortKeys            *self);

  GCompareDataFunc key_compare;

  gboolean              (* is_compatible)                       (GtkSortKeys            *self,
                                                                 GtkSortKeys            *other);

  void                  (* init_key)                            (GtkSortKeys            *self,
                                                                 gpointer                item,
                                                                 gpointer                key_memory);
  void                  (* clear_key)                           (GtkSortKeys            *self,
                                                                 gpointer                key_memory);
};

GtkSortKeys *           gtk_sort_keys_alloc                     (const GtkSortKeysClass *klass,
                                                                 gsize                   size,
                                                                 gsize                   key_size,
                                                                 gsize                   key_align);
#define gtk_sort_keys_new(_name, _klass, _key_size, _key_align) \
    ((_name *) gtk_sort_keys_alloc ((_klass), sizeof (_name), (_key_size), (_key_align)))
GtkSortKeys *           gtk_sort_keys_ref                       (GtkSortKeys            *self);
void                    gtk_sort_keys_unref                     (GtkSortKeys            *self);

GtkSortKeys *           gtk_sort_keys_new_equal                 (void);

gsize                   gtk_sort_keys_get_key_size              (GtkSortKeys            *self);
gsize                   gtk_sort_keys_get_key_align             (GtkSortKeys            *self);
GCompareDataFunc        gtk_sort_keys_get_key_compare_func      (GtkSortKeys            *self);
gboolean                gtk_sort_keys_is_compatible             (GtkSortKeys            *self,
                                                                 GtkSortKeys            *other);
gboolean                gtk_sort_keys_needs_clear_key           (GtkSortKeys            *self);

#define GTK_SORT_KEYS_ALIGN(_size,_align) (((_size) + (_align) - 1) & ~((_align) - 1))
static inline int
gtk_sort_keys_compare (GtkSortKeys *self,
                       gconstpointer a,
                       gconstpointer b)
{
  return self->klass->key_compare (a, b, self);
}
                       
static inline void
gtk_sort_keys_init_key (GtkSortKeys *self,
                        gpointer       item,
                        gpointer       key_memory)
{
  self->klass->init_key (self, item, key_memory);
}

static inline void
gtk_sort_keys_clear_key (GtkSortKeys *self,
                         gpointer       key_memory)
{
  if (self->klass->clear_key)
    self->klass->clear_key (self, key_memory);
}


