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

#include "gtktreelistrowsorter.h"

#include "gtktreelistmodel.h"

#include "gtksorterprivate.h"
#include "gtktypebuiltins.h"

/**
 * GtkTreeListRowSorter:
 *
 * `GtkTreeListRowSorter` is a special-purpose sorter that will apply a given
 * sorter to the levels in a tree.
 *
 * Here is an example for setting up a column view with a tree model and
 * a `GtkTreeListSorter`:
 *
 * ```c
 * column_sorter = gtk_column_view_get_sorter (view);
 * sorter = gtk_tree_list_row_sorter_new (g_object_ref (column_sorter));
 * sort_model = gtk_sort_list_model_new (tree_model, sorter);
 * selection = gtk_single_selection_new (sort_model);
 * gtk_column_view_set_model (view, G_LIST_MODEL (selection));
 * ```
 */

struct _GtkTreeListRowSorter
{
  GtkSorter parent_instance;

  GtkSorter *sorter;
};

enum {
  PROP_0,
  PROP_SORTER,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (GtkTreeListRowSorter, gtk_tree_list_row_sorter, GTK_TYPE_SORTER)

#define MAX_KEY_DEPTH (8)

/* our key is a gpointer[MAX_KEY_DEPTH] and contains:
 *
 * key[0] != NULL:
 * The depth of the item is <= MAX_KEY_DEPTH so we can put the keys
 * inline. This is the key for the ancestor at depth 0.
 *
 * key[0] == NULL && key[1] != NULL:
 * The depth of the item is > MAX_KEY_DEPTH so it had to be allocated.
 * key[1] contains this allocated and NULL-terminated array.
 *
 * key[0] == NULL && key[1] == NULL:
 * The item is not a TreeListRow. To break ties, we put the item in key[2] to
 * allow a direct compare.
 */
typedef struct _GtkTreeListRowSortKeys GtkTreeListRowSortKeys;
typedef struct _GtkTreeListRowCacheKey GtkTreeListRowCacheKey;
struct _GtkTreeListRowSortKeys
{
  GtkSortKeys keys;

  GtkSortKeys *sort_keys;
  GHashTable *cached_keys;
};

struct _GtkTreeListRowCacheKey
{
  GtkTreeListRow *row;
  guint ref_count;
};

static GtkTreeListRowCacheKey *
cache_key_from_key (GtkTreeListRowSortKeys *self,
                    gpointer                key)
{
  if (self->sort_keys == NULL)
    return key;

  return (GtkTreeListRowCacheKey *) ((char *) key + GTK_SORT_KEYS_ALIGN (gtk_sort_keys_get_key_size (self->sort_keys), G_ALIGNOF (GtkTreeListRowCacheKey)));
}

static void
gtk_tree_list_row_sort_keys_free (GtkSortKeys *keys)
{
  GtkTreeListRowSortKeys *self = (GtkTreeListRowSortKeys *) keys;

  g_assert (g_hash_table_size (self->cached_keys) == 0);
  g_hash_table_unref (self->cached_keys);
  if (self->sort_keys)
    gtk_sort_keys_unref (self->sort_keys);
  g_free (self);
}

static inline gboolean
unpack (gpointer  *key,
        gpointer **out_keys,
        gsize     *out_max_size)
{
  if (key[0])
    {
      *out_keys = key;
      *out_max_size = MAX_KEY_DEPTH;
      return TRUE;
    }
  else if (key[1])
    {
      *out_keys = (gpointer *) key[1];
      *out_max_size = G_MAXSIZE;
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static int
gtk_tree_list_row_sort_keys_compare (gconstpointer a,
                                     gconstpointer b,
                                     gpointer      data)
{
  GtkTreeListRowSortKeys *self = (GtkTreeListRowSortKeys *) data;
  gpointer *keysa = (gpointer *) a;
  gpointer *keysb = (gpointer *) b;
  gsize sizea, sizeb;
  gboolean resa, resb;
  gsize i;
  GtkOrdering result;

  resa = unpack (keysa, &keysa, &sizea);
  resb = unpack (keysb, &keysb, &sizeb);
  if (!resa)
    return resb ? GTK_ORDERING_LARGER : (keysa[2] < keysb[2] ? GTK_ORDERING_SMALLER : 
                                        (keysa[2] > keysb[2] ? GTK_ORDERING_LARGER : GTK_ORDERING_EQUAL));
  else if (!resb)
    return GTK_ORDERING_SMALLER;

  for (i = 0; i < MIN (sizea, sizeb); i++)
    {
      if (keysa[i] == keysb[i])
        {
          if (keysa[i] == NULL)
            return GTK_ORDERING_EQUAL;
          continue;
        }
      else if (keysa[i] == NULL)
        return GTK_ORDERING_SMALLER;
      else if (keysb[i] == NULL)
        return GTK_ORDERING_LARGER;

      if (self->sort_keys)
        result = gtk_sort_keys_compare (self->sort_keys, keysa[i], keysb[i]);
      else
        result = GTK_ORDERING_EQUAL;

      if (result == GTK_ORDERING_EQUAL)
        {
          /* We must break ties here because if a ever gets a child,
           * it would need to go right in between a and b. */
          GtkTreeListRowCacheKey *cachea = cache_key_from_key (self, keysa[i]);
          GtkTreeListRowCacheKey *cacheb = cache_key_from_key (self, keysb[i]);
          if (gtk_tree_list_row_get_position (cachea->row) < gtk_tree_list_row_get_position (cacheb->row))
            result = GTK_ORDERING_SMALLER;
          else
            result = GTK_ORDERING_LARGER;
        }
      return result;
    }

  if (sizea < sizeb)
    return GTK_ORDERING_SMALLER;
  else if (sizea > sizeb)
    return GTK_ORDERING_LARGER;
  else
    return GTK_ORDERING_EQUAL;
}

static gboolean
gtk_tree_list_row_sort_keys_is_compatible (GtkSortKeys *keys,
                                           GtkSortKeys *other)
{
  GtkTreeListRowSortKeys *self = (GtkTreeListRowSortKeys *) keys;
  GtkTreeListRowSortKeys *compare;

  /* FIXME https://gitlab.gnome.org/GNOME/gtk/-/issues/3228 */
  return FALSE;

  if (keys->klass != other->klass)
    return FALSE;

  compare = (GtkTreeListRowSortKeys *) other;

  if (self->sort_keys && compare->sort_keys)
    return gtk_sort_keys_is_compatible (self->sort_keys, compare->sort_keys);
  else
    return self->sort_keys == compare->sort_keys;
}

static gpointer
gtk_tree_list_row_sort_keys_ref_key (GtkTreeListRowSortKeys *self,
                                     GtkTreeListRow         *row)
{
  GtkTreeListRowCacheKey *cache_key;
  gpointer key;

  key = g_hash_table_lookup (self->cached_keys, row);
  if (key)
    {
      cache_key = cache_key_from_key (self, key);
      cache_key->ref_count++;
      return key;
    }

  if (self->sort_keys)
    key = g_malloc (GTK_SORT_KEYS_ALIGN (gtk_sort_keys_get_key_size (self->sort_keys), G_ALIGNOF (GtkTreeListRowCacheKey))
                    + sizeof (GtkTreeListRowCacheKey));
  else
    key = g_malloc (sizeof (GtkTreeListRowCacheKey));
  cache_key = cache_key_from_key (self, key);
  cache_key->row = g_object_ref (row);
  cache_key->ref_count = 1;
  if (self->sort_keys)
    {
      gpointer item = gtk_tree_list_row_get_item (row);
      gtk_sort_keys_init_key (self->sort_keys, item, key);
      g_object_unref (item);
    }

  g_hash_table_insert (self->cached_keys, row, key);
  return key;
}

static void
gtk_tree_list_row_sort_keys_unref_key (GtkTreeListRowSortKeys *self,
                                       gpointer                key)
{
  GtkTreeListRowCacheKey *cache_key = cache_key_from_key (self, key);

  cache_key->ref_count--;
  if (cache_key->ref_count > 0)
    return;

  if (self->sort_keys)
    gtk_sort_keys_clear_key (self->sort_keys, key);

  g_hash_table_remove (self->cached_keys, cache_key->row);
  g_object_unref (cache_key->row);
  g_free (key);
}

static void
gtk_tree_list_row_sort_keys_init_key (GtkSortKeys *keys,
                                      gpointer     item,
                                      gpointer     key_memory)
{
  GtkTreeListRowSortKeys *self = (GtkTreeListRowSortKeys *) keys;
  gpointer *key = (gpointer *) key_memory;
  GtkTreeListRow *row, *parent;
  guint i, depth;

  if (!GTK_IS_TREE_LIST_ROW (item))
    {
      key[0] = NULL;
      key[1] = NULL;
      key[2] = item;
      return;
    }

  row = GTK_TREE_LIST_ROW (item);
  depth = gtk_tree_list_row_get_depth (row) + 1;
  if (depth > MAX_KEY_DEPTH)
    {
      key[0] = NULL;
      key[1] = g_new (gpointer, depth + 1);
      key = key[1];
      key[depth] = NULL;
    }
  else if (depth < MAX_KEY_DEPTH)
    {
      key[depth] = NULL;
    }

  g_object_ref (row);
  for (i = depth; i-- > 0; )
    {
      key[i] = gtk_tree_list_row_sort_keys_ref_key (self, row);
      parent = gtk_tree_list_row_get_parent (row);
      g_object_unref (row);
      row = parent;
    }
  g_assert (row == NULL);
}

static void
gtk_tree_list_row_sort_keys_clear_key (GtkSortKeys *keys,
                                       gpointer     key_memory)
{
  GtkTreeListRowSortKeys *self = (GtkTreeListRowSortKeys *) keys;
  gpointer *key = (gpointer *) key_memory;
  gsize i, max;

  if (!unpack (key, &key, &max))
    return;

  for (i = 0; i < max && key[i] != NULL; i++)
    gtk_tree_list_row_sort_keys_unref_key (self, key[i]);
  
  if (key[0] == NULL)
    g_free (key[1]);
}

static const GtkSortKeysClass GTK_TREE_LIST_ROW_SORT_KEYS_CLASS =
{
  gtk_tree_list_row_sort_keys_free,
  gtk_tree_list_row_sort_keys_compare,
  gtk_tree_list_row_sort_keys_is_compatible,
  gtk_tree_list_row_sort_keys_init_key,
  gtk_tree_list_row_sort_keys_clear_key,
};

static GtkSortKeys *
gtk_tree_list_row_sort_keys_new (GtkTreeListRowSorter *self)
{
  GtkTreeListRowSortKeys *result;

  result = gtk_sort_keys_new (GtkTreeListRowSortKeys,
                              &GTK_TREE_LIST_ROW_SORT_KEYS_CLASS,
                              sizeof (gpointer[MAX_KEY_DEPTH]),
                              G_ALIGNOF (gpointer));

  if (self->sorter)
    result->sort_keys = gtk_sorter_get_keys (self->sorter);
  result->cached_keys = g_hash_table_new (NULL, NULL);

  return (GtkSortKeys *) result;
}

static GtkOrdering
gtk_tree_list_row_sorter_compare (GtkSorter *sorter,
                                  gpointer   item1,
                                  gpointer   item2)
{
  GtkTreeListRowSorter *self = GTK_TREE_LIST_ROW_SORTER (sorter);
  GtkTreeListRow *r1, *r2;
  GtkTreeListRow *p1, *p2;
  guint d1, d2;
  GtkOrdering result = GTK_ORDERING_EQUAL;

  /* break ties here so we really are a total order */
  if (!GTK_IS_TREE_LIST_ROW (item1))
    return GTK_IS_TREE_LIST_ROW (item2) ? GTK_ORDERING_LARGER : (item1 < item2 ? GTK_ORDERING_SMALLER : GTK_ORDERING_LARGER);
  else if (!GTK_IS_TREE_LIST_ROW (item2))
    return GTK_ORDERING_SMALLER;

  r1 = GTK_TREE_LIST_ROW (item1);
  r2 = GTK_TREE_LIST_ROW (item2);

  g_object_ref (r1);
  g_object_ref (r2);

  d1 = gtk_tree_list_row_get_depth (r1);
  d2 = gtk_tree_list_row_get_depth (r2);

  /* First, get to the same depth */
  while (d1 > d2)
    {
      p1 = gtk_tree_list_row_get_parent (r1);
      g_object_unref (r1);
      r1 = p1;
      d1--;
      result = GTK_ORDERING_LARGER;
    }
  while (d2 > d1)
    {
      p2 = gtk_tree_list_row_get_parent (r2);
      g_object_unref (r2);
      r2 = p2;
      d2--;
      result = GTK_ORDERING_SMALLER;
    }

  /* Now walk up until we find a common parent */
  if (r1 != r2)
    {
      while (TRUE)
        {
          p1 = gtk_tree_list_row_get_parent (r1);
          p2 = gtk_tree_list_row_get_parent (r2);
          if (p1 == p2)
            {
              gpointer obj1 = gtk_tree_list_row_get_item (r1);
              gpointer obj2 = gtk_tree_list_row_get_item (r2);

              if (self->sorter == NULL)
                result = GTK_ORDERING_EQUAL;
              else
                result = gtk_sorter_compare (self->sorter, obj1, obj2);

              /* We must break ties here because if r1 ever gets a child,
               * it would need to go right in between r1 and r2. */
              if (result == GTK_ORDERING_EQUAL)
                {
                  if (gtk_tree_list_row_get_position (r1) < gtk_tree_list_row_get_position (r2))
                    result = GTK_ORDERING_SMALLER;
                  else
                    result = GTK_ORDERING_LARGER;
                }

              g_object_unref (obj1);
              g_object_unref (obj2);

              break;
            }
          else
            {
              g_object_unref (r1);
              r1 = p1;
              g_object_unref (r2);
              r2 = p2;
            }
        }
    }

  g_object_unref (r1);
  g_object_unref (r2);

  return result;
}

static GtkSorterOrder
gtk_tree_list_row_sorter_get_order (GtkSorter *sorter)
{
  /* Must be a total order, because we need an exact position where new items go */
  return GTK_SORTER_ORDER_TOTAL;
}

static void
propagate_changed (GtkSorter *sorter,
                   GtkSorterChange change,
                   GtkTreeListRowSorter *self)
{
  gtk_sorter_changed_with_keys (GTK_SORTER (self),
                                change,
                                gtk_tree_list_row_sort_keys_new (self));
}

static void
gtk_tree_list_row_sorter_dispose (GObject *object)
{
  GtkTreeListRowSorter *self = GTK_TREE_LIST_ROW_SORTER (object);

  if (self->sorter)
    g_signal_handlers_disconnect_by_func (self->sorter, propagate_changed, self);
  g_clear_object (&self->sorter);

  G_OBJECT_CLASS (gtk_tree_list_row_sorter_parent_class)->dispose (object);
}

static void
gtk_tree_list_row_sorter_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GtkTreeListRowSorter *self = GTK_TREE_LIST_ROW_SORTER (object);

  switch (prop_id)
    {
    case PROP_SORTER:
      gtk_tree_list_row_sorter_set_sorter (self, GTK_SORTER (g_value_get_object (value)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tree_list_row_sorter_get_property (GObject     *object,
                                       guint        prop_id,
                                       GValue      *value,
                                       GParamSpec  *pspec)
{
  GtkTreeListRowSorter *self = GTK_TREE_LIST_ROW_SORTER (object);

  switch (prop_id)
    {
    case PROP_SORTER:
      g_value_set_object (value, self->sorter);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tree_list_row_sorter_class_init (GtkTreeListRowSorterClass *class)
{
  GtkSorterClass *sorter_class = GTK_SORTER_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  sorter_class->compare = gtk_tree_list_row_sorter_compare;
  sorter_class->get_order = gtk_tree_list_row_sorter_get_order;

  object_class->dispose = gtk_tree_list_row_sorter_dispose;
  object_class->set_property = gtk_tree_list_row_sorter_set_property;
  object_class->get_property = gtk_tree_list_row_sorter_get_property;

  /**
   * GtkTreeListRowSorter:sorter:
   *
   * The underlying sorter
   */
  properties[PROP_SORTER] =
      g_param_spec_object ("sorter", NULL, NULL,
                          GTK_TYPE_SORTER,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

static void
gtk_tree_list_row_sorter_init (GtkTreeListRowSorter *self)
{
  gtk_sorter_changed_with_keys (GTK_SORTER (self),
                                GTK_SORTER_CHANGE_DIFFERENT,
                                gtk_tree_list_row_sort_keys_new (self));
}

/**
 * gtk_tree_list_row_sorter_new:
 * @sorter: (nullable) (transfer full): a `GtkSorter`
 *
 * Create a special-purpose sorter that applies the sorting
 * of @sorter to the levels of a `GtkTreeListModel`.
 *
 * Note that this sorter relies on [property@Gtk.TreeListModel:passthrough]
 * being %FALSE as it can only sort [class@Gtk.TreeListRow]s.
 *
 * Returns: a new `GtkTreeListRowSorter`
 */
GtkTreeListRowSorter *
gtk_tree_list_row_sorter_new (GtkSorter *sorter)
{
  GtkTreeListRowSorter *result;

  g_return_val_if_fail (sorter == NULL || GTK_IS_SORTER (sorter), NULL);

  result = g_object_new (GTK_TYPE_TREE_LIST_ROW_SORTER,
                         "sorter", sorter,
                         NULL);

  g_clear_object (&sorter);

  return result;
}

/**
 * gtk_tree_list_row_sorter_set_sorter:
 * @self: a `GtkTreeListRowSorter`
 * @sorter: (nullable) (transfer none): The sorter to use
 *
 * Sets the sorter to use for items with the same parent.
 *
 * This sorter will be passed the [property@Gtk.TreeListRow:item] of
 * the tree list rows passed to @self.
 */
void
gtk_tree_list_row_sorter_set_sorter (GtkTreeListRowSorter *self,
                                     GtkSorter            *sorter)
{
  g_return_if_fail (GTK_IS_TREE_LIST_ROW_SORTER (self));
  g_return_if_fail (sorter == NULL || GTK_IS_SORTER (sorter));

  if (self->sorter == sorter)
    return;

  if (self->sorter)
    g_signal_handlers_disconnect_by_func (self->sorter, propagate_changed, self);
  g_set_object (&self->sorter, sorter);
  if (self->sorter)
    g_signal_connect (sorter, "changed", G_CALLBACK (propagate_changed), self);

  gtk_sorter_changed_with_keys (GTK_SORTER (self),
                                GTK_SORTER_CHANGE_DIFFERENT,
                                gtk_tree_list_row_sort_keys_new (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SORTER]);
}

/**
 * gtk_tree_list_row_sorter_get_sorter:
 * @self: a `GtkTreeListRowSorter`
 *
 * Returns the sorter used by @self.
 *
 * Returns: (transfer none) (nullable): the sorter used
 */
GtkSorter *
gtk_tree_list_row_sorter_get_sorter (GtkTreeListRowSorter *self)
{
  g_return_val_if_fail (GTK_IS_TREE_LIST_ROW_SORTER (self), NULL);

  return self->sorter;
}
