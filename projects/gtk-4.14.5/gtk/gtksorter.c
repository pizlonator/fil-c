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

#include "gtksorterprivate.h"

#include <glib/gi18n-lib.h>
#include "gtktypebuiltins.h"
#include "gtkprivate.h"

/**
 * GtkSorter:
 *
 * `GtkSorter` is an object to describe sorting criteria.
 *
 * Its primary user is [class@Gtk.SortListModel]
 *
 * The model will use a sorter to determine the order in which
 * its items should appear by calling [method@Gtk.Sorter.compare]
 * for pairs of items.
 *
 * Sorters may change their sorting behavior through their lifetime.
 * In that case, they will emit the [signal@Gtk.Sorter::changed] signal
 * to notify that the sort order is no longer valid and should be updated
 * by calling gtk_sorter_compare() again.
 *
 * GTK provides various pre-made sorter implementations for common sorting
 * operations. [class@Gtk.ColumnView] has built-in support for sorting lists
 * via the [property@Gtk.ColumnViewColumn:sorter] property, where the user can
 * change the sorting by clicking on list headers.
 *
 * Of course, in particular for large lists, it is also possible to subclass
 * `GtkSorter` and provide one's own sorter.
 */

typedef struct _GtkSorterPrivate GtkSorterPrivate;
typedef struct _GtkDefaultSortKeys GtkDefaultSortKeys;

struct _GtkSorterPrivate
{
  GtkSortKeys *keys;
};

struct _GtkDefaultSortKeys
{
  GtkSortKeys keys;
  GtkSorter *sorter;
};

enum {
  CHANGED,
  LAST_SIGNAL
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkSorter, gtk_sorter, G_TYPE_OBJECT)

static guint signals[LAST_SIGNAL] = { 0 };

static GtkOrdering
gtk_sorter_default_compare (GtkSorter *self,
                            gpointer   item1,
                            gpointer   item2)
{
  g_critical ("Sorter of type '%s' does not implement GtkSorter::compare", G_OBJECT_TYPE_NAME (self));

  return GTK_ORDERING_EQUAL;
}

static GtkSorterOrder
gtk_sorter_default_get_order (GtkSorter *self)
{
  return GTK_SORTER_ORDER_PARTIAL;
}

static void
gtk_sorter_dispose (GObject *object)
{
  GtkSorter *self = GTK_SORTER (object);
  GtkSorterPrivate *priv = gtk_sorter_get_instance_private (self);

  g_clear_pointer (&priv->keys, gtk_sort_keys_unref);

  G_OBJECT_CLASS (gtk_sorter_parent_class)->dispose (object);
}

static void
gtk_sorter_class_init (GtkSorterClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = gtk_sorter_dispose;

  class->compare = gtk_sorter_default_compare;
  class->get_order = gtk_sorter_default_get_order;

  /**
   * GtkSorter::changed:
   * @self: The `GtkSorter`
   * @change: how the sorter changed
   *
   * Emitted whenever the sorter changed.
   *
   * Users of the sorter should then update the sort order
   * again via gtk_sorter_compare().
   *
   * [class@Gtk.SortListModel] handles this signal automatically.
   *
   * Depending on the @change parameter, it may be possible to update
   * the sort order without a full resorting. Refer to the
   * [enum@Gtk.SorterChange] documentation for details.
   */
  signals[CHANGED] =
    g_signal_new (I_("changed"),
                  G_TYPE_FROM_CLASS (class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__ENUM,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_SORTER_CHANGE);
  g_signal_set_va_marshaller (signals[CHANGED],
                              G_TYPE_FROM_CLASS (class),
                              g_cclosure_marshal_VOID__ENUMv);
}

static void
gtk_sorter_init (GtkSorter *self)
{
}

/**
 * gtk_sorter_compare:
 * @self: a `GtkSorter`
 * @item1: (type GObject) (transfer none): first item to compare
 * @item2: (type GObject) (transfer none): second item to compare
 *
 * Compares two given items according to the sort order implemented
 * by the sorter.
 *
 * Sorters implement a partial order:
 *
 * * It is reflexive, ie a = a
 * * It is antisymmetric, ie if a < b and b < a, then a = b
 * * It is transitive, ie given any 3 items with a ≤ b and b ≤ c,
 *   then a ≤ c
 *
 * The sorter may signal it conforms to additional constraints
 * via the return value of [method@Gtk.Sorter.get_order].
 *
 * Returns: %GTK_ORDERING_EQUAL if @item1 == @item2,
 *   %GTK_ORDERING_SMALLER if @item1 < @item2,
 *   %GTK_ORDERING_LARGER if @item1 > @item2
 */
GtkOrdering
gtk_sorter_compare (GtkSorter *self,
                    gpointer   item1,
                    gpointer   item2)
{
  GtkOrdering result;

  /* We turn this off because gtk_sorter_compare() is called so much that it's too expensive */
  /* g_return_val_if_fail (GTK_IS_SORTER (self), GTK_ORDERING_EQUAL); */
  g_return_val_if_fail (item1 && item2, GTK_ORDERING_EQUAL);

  if (item1 == item2)
    return GTK_ORDERING_EQUAL;

  result = GTK_SORTER_GET_CLASS (self)->compare (self, item1, item2);

#ifdef G_ENABLE_DEBUG
  if (result < -1 || result > 1)
    {
      g_critical ("A sorter of type \"%s\" returned %d, which is not a valid GtkOrdering result.\n"
                  "Did you forget to call gtk_ordering_from_cmpfunc()?",
                  G_OBJECT_TYPE_NAME (self), (int) result);
    }
#endif

  return result;
}

/**
 * gtk_sorter_get_order:
 * @self: a `GtkSorter`
 *
 * Gets the order that @self conforms to.
 *
 * See [enum@Gtk.SorterOrder] for details
 * of the possible return values.
 *
 * This function is intended to allow optimizations.
 *
 * Returns: The order
 */
GtkSorterOrder
gtk_sorter_get_order (GtkSorter *self)
{
  g_return_val_if_fail (GTK_IS_SORTER (self), GTK_SORTER_ORDER_PARTIAL);

  return GTK_SORTER_GET_CLASS (self)->get_order (self);
}

static int
gtk_default_sort_keys_compare (gconstpointer a,
                               gconstpointer b,
                               gpointer      data)
{
  GtkDefaultSortKeys *self = data;
  gpointer *key_a = (gpointer *) a;
  gpointer *key_b = (gpointer *) b;

  return gtk_sorter_compare (self->sorter, *key_a, *key_b);
}

static void
gtk_default_sort_keys_free (GtkSortKeys *keys)
{
  GtkDefaultSortKeys *self = (GtkDefaultSortKeys *) keys;

  g_object_unref (self->sorter);

  g_free (self);
}

static gboolean
gtk_default_sort_keys_is_compatible (GtkSortKeys *keys,
                                     GtkSortKeys *other)
{
  if (keys->klass != other->klass)
    return FALSE;

  return TRUE;
}

static void
gtk_default_sort_keys_init_key (GtkSortKeys *self,
                                gpointer     item,
                                gpointer     key_memory)
{
  gpointer *key = (gpointer *) key_memory;

  *key = g_object_ref (item);
}

static void
gtk_default_sort_keys_clear_key (GtkSortKeys *self,
                                 gpointer     key_memory)
{
  gpointer *key = (gpointer *) key_memory;

  g_object_unref (*key);
}

static const GtkSortKeysClass GTK_DEFAULT_SORT_KEYS_CLASS = 
{
  gtk_default_sort_keys_free,
  gtk_default_sort_keys_compare,
  gtk_default_sort_keys_is_compatible,
  gtk_default_sort_keys_init_key,
  gtk_default_sort_keys_clear_key,
};

/*<private>
 * gtk_sorter_get_keys:
 * @self: a `GtkSorter`
 *
 * Gets a `GtkSortKeys` that can be used as an alternative to
 * @self for faster sorting.
 *
 * The sort keys can change every time [signal@Gtk.Sorter::changed]
 * is emitted. When the keys change, you should redo all comparisons
 * with the new keys.
 *
 * When [method@Gtk.SortKeys.is_compatible] for the old and new keys
 * returns %TRUE, you can reuse keys you generated previously.
 *
 * Returns: (transfer full): the sort keys to sort with
 */
GtkSortKeys *
gtk_sorter_get_keys (GtkSorter *self)
{
  GtkSorterPrivate *priv = gtk_sorter_get_instance_private (self);
  GtkDefaultSortKeys *fallback;

  g_return_val_if_fail (GTK_IS_SORTER (self), NULL);

  if (priv->keys)
    return gtk_sort_keys_ref (priv->keys);

  fallback = gtk_sort_keys_new (GtkDefaultSortKeys, &GTK_DEFAULT_SORT_KEYS_CLASS, sizeof (gpointer), G_ALIGNOF (gpointer));
  fallback->sorter = g_object_ref (self);

  return (GtkSortKeys *) fallback;
}

/**
 * gtk_sorter_changed:
 * @self: a `GtkSorter`
 * @change: How the sorter changed
 *
 * Notifies all users of the sorter that it has changed.
 *
 * This emits the [signal@Gtk.Sorter::changed] signal. Users
 * of the sorter should then update the sort order via
 * [method@Gtk.Sorter.compare].
 *
 * Depending on the @change parameter, it may be possible to
 * update the sort order without a full resorting. Refer to
 * the [enum@Gtk.SorterChange] documentation for details.
 *
 * This function is intended for implementers of `GtkSorter`
 * subclasses and should not be called from other functions.
 */
void
gtk_sorter_changed (GtkSorter       *self,
                    GtkSorterChange  change)
{
  g_return_if_fail (GTK_IS_SORTER (self));

  g_signal_emit (self, signals[CHANGED], 0, change);
}

/*<private>
 * gtk_sorter_changed_with_keys:
 * @self: a `GtkSorter`
 * @change: How the sorter changed
 * @keys: (not nullable) (transfer full): New keys to use
 *
 * Updates the sorter's keys to @keys and then calls gtk_sorter_changed().
 *
 * If you do not want to update the keys, call that function instead.
 *
 * This function should also be called in your_sorter_init() to initialize
 * the keys to use with your sorter.
 */
void
gtk_sorter_changed_with_keys (GtkSorter       *self,
                              GtkSorterChange  change,
                              GtkSortKeys     *keys)
{
  GtkSorterPrivate *priv = gtk_sorter_get_instance_private (self);

  g_return_if_fail (GTK_IS_SORTER (self));
  g_return_if_fail (keys != NULL);

  g_clear_pointer (&priv->keys, gtk_sort_keys_unref);
  priv->keys = keys;

  gtk_sorter_changed (self, change);
}

/* See the comment in gtkenums.h as to why we need to play
 * games with the introspection scanner for static inline
 * functions
 */
#ifdef __GI_SCANNER__
/**
 * gtk_ordering_from_cmpfunc: (skip)
 * @cmpfunc_result: Result of a comparison function
 *
 * Converts the result of a `GCompareFunc` like strcmp() to a
 * `GtkOrdering` value.
 *
 * Returns: the corresponding `GtkOrdering`
 *
 * Since: 4.2
 **/
GtkOrdering
gtk_ordering_from_cmpfunc (int cmpfunc_result)
{
  return (GtkOrdering) ((cmpfunc_result > 0) - (cmpfunc_result < 0));
}
#endif
