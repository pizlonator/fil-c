/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gdataset.c: Generic dataset mechanism, similar to GtkObject data.
 * Copyright (C) 1998 Tim Janik
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
 * MT safe ; except for g_data*_foreach()
 */

#include "config.h"

#include <string.h>

#include "gdataset.h"
#include "gbitlock.h"

#include "gslice.h"
#include "gdatasetprivate.h"
#include "gutilsprivate.h"
#include "ghash.h"
#include "gquark.h"
#include "gstrfuncs.h"
#include "gtestutils.h"
#include "gthread.h"
#include "glib_trace.h"
#include "galloca.h"

/**
 * GData:
 *
 * An opaque data structure that represents a keyed data list.
 *
 * See also: [Keyed data lists](datalist-and-dataset.html).
 **/

/**
 * GDestroyNotify:
 * @data: the data element.
 *
 * Specifies the type of function which is called when a data element
 * is destroyed. It is passed the pointer to the data element and
 * should free any memory and resources allocated for it.
 **/

#define G_DATALIST_FLAGS_MASK_INTERNAL 0x7

#define G_DATALIST_CLEAN_POINTER(ptr) \
  ((GData *) ((gpointer) (((guintptr) (ptr)) & ~((guintptr) G_DATALIST_FLAGS_MASK_INTERNAL))))

/* datalist pointer accesses have to be carried out atomically */
#define G_DATALIST_GET_POINTER(datalist) \
  G_DATALIST_CLEAN_POINTER (g_atomic_pointer_get (datalist))

#define G_DATALIST_SET_POINTER(datalist, pointer)       G_STMT_START {                           \
  gpointer _oldv = g_atomic_pointer_get (datalist);                                              \
  gpointer _newv;                                                                                \
  do {                                                                                           \
    _newv = (gpointer) (((guintptr) _oldv & ((guintptr) G_DATALIST_FLAGS_MASK_INTERNAL)) | (guintptr) pointer); \
  } while (!g_atomic_pointer_compare_and_exchange_full ((void**) datalist, _oldv,                \
                                                        _newv, &_oldv));                         \
} G_STMT_END

/* --- structures --- */
typedef struct {
  GQuark          key;
  gpointer        data;
  GDestroyNotify  destroy;
} GDataElt;

typedef struct _GDataset GDataset;
struct _GData
{
  guint32  len;     /* Number of elements */
  guint32  alloc;   /* Number of allocated elements */
  GDataElt data[1]; /* Flexible array */
};

struct _GDataset
{
  gconstpointer location;
  GData        *datalist;
};


/* --- prototypes --- */
static inline GDataset*	g_dataset_lookup		(gconstpointer	  dataset_location);
static void		g_dataset_destroy_internal	(GDataset	 *dataset);
static inline gpointer	g_data_set_internal		(GData     	**datalist,
							 GQuark   	  key_id,
							 gpointer         data,
							 GDestroyNotify   destroy_func,
							 GDataset	 *dataset);
static void		g_data_initialize		(void);

/* Locking model:
 * Each standalone GDataList is protected by a bitlock in the datalist pointer,
 * which protects that modification of the non-flags part of the datalist pointer
 * and the contents of the datalist.
 *
 * For GDataSet we have a global lock g_dataset_global that protects
 * the global dataset hash and cache, and additionally it protects the
 * datalist such that we can avoid to use the bit lock in a few places
 * where it is easy.
 */

/* --- variables --- */
G_LOCK_DEFINE_STATIC (g_dataset_global);
static GHashTable   *g_dataset_location_ht = NULL;
static GDataset     *g_dataset_cached = NULL; /* should this be
						 thread specific? */

/* --- functions --- */

#define DATALIST_LOCK_BIT 2

G_ALWAYS_INLINE static inline GData *
g_datalist_lock_and_get (GData **datalist)
{
  gpointer ptr;

  g_pointer_bit_lock_and_get ((void **) datalist, DATALIST_LOCK_BIT, &ptr);
  return G_DATALIST_CLEAN_POINTER (ptr);
}

static void
g_datalist_unlock (GData **datalist)
{
  g_pointer_bit_unlock ((void **)datalist, DATALIST_LOCK_BIT);
}

static void
g_datalist_unlock_and_set (GData **datalist, gpointer ptr)
{
  g_pointer_bit_unlock_and_set ((void **) datalist, DATALIST_LOCK_BIT, ptr, G_DATALIST_FLAGS_MASK_INTERNAL);
}

static gboolean
datalist_append (GData **data, GQuark key_id, gpointer new_data, GDestroyNotify destroy_func)
{
  gboolean reallocated;
  GData *d;

  d = *data;
  if (!d)
    {
      d = g_malloc (G_STRUCT_OFFSET (GData, data) + 2u * sizeof (GDataElt));
      d->len = 0;
      d->alloc = 2u;
      *data = d;
      reallocated = TRUE;
    }
  else if (d->len == d->alloc)
    {
      d->alloc = d->alloc * 2u;
#if G_ENABLE_DEBUG
      /* d->alloc is always a power of two. It thus overflows the first time
       * when going to (G_MAXUINT32+1), or when requesting 2^31+1 elements.
       *
       * This is not handled, and we just crash. That's because we track the GData
       * in a linear list, which horribly degrades long before we add 2 billion entries.
       * Don't ever try to do that. */
      g_assert (d->alloc > d->len);
#endif
      d = g_realloc (d, G_STRUCT_OFFSET (GData, data) + d->alloc * sizeof (GDataElt));
      *data = d;
      reallocated = TRUE;
    }
  else
    reallocated = FALSE;

  d->data[d->len] = (GDataElt){
    .key = key_id,
    .data = new_data,
    .destroy = destroy_func,
  };
  d->len++;

  return reallocated;
}

static void
datalist_remove (GData *data, guint32 idx)
{
#if G_ENABLE_DEBUG
  g_assert (idx < data->len);
#endif

  /* g_data_remove_internal() relies on the fact, that this function removes
   * the entry similar to g_array_remove_index_fast(). That is, the entries up
   * to @idx are left unchanged, and the last entry is moved to position @idx.
   * */

  data->len--;

  if (idx != data->len)
    data->data[idx] = data->data[data->len];
}

static gboolean
datalist_shrink (GData **data, GData **d_to_free)
{
  guint32 alloc_by_4;
  guint32 v;
  GData *d;

  d = *data;

  alloc_by_4 = d->alloc / 4u;

  if (G_LIKELY (d->len > alloc_by_4))
    {
      /* No shrinking */
      return FALSE;
    }

  if (d->len == 0)
    {
      /* The list became empty. We drop the allocated memory altogether. */

      /* The caller will free the buffer after releasing the lock, to minimize
       * the time we hold the lock. Transfer it out. */
      *d_to_free = d;
      *data = NULL;
      return TRUE;
    }

  /* If the buffer is filled not more than 25%. Shrink to double the current length. */

  v = d->len;
  if (v != alloc_by_4)
    {
      /* d->alloc is a power of two. Usually, we remove one element at a
       * time, then we will just reach reach a quarter of that.
       *
       * However, with g_datalist_id_remove_multiple(), len can be smaller
       * at once. In that case, find first the next power of two. */
      v = g_nearest_pow (v);
    }
  v *= 2u;

#if G_ENABLE_DEBUG
  g_assert (v > d->len);
  g_assert (v <= d->alloc / 2u);
#endif

  d->alloc = v;
  d = g_realloc (d, G_STRUCT_OFFSET (GData, data) + (v * sizeof (GDataElt)));
  *d_to_free = NULL;
  *data = d;
  return TRUE;
}

static GDataElt *
datalist_find (GData *data, GQuark key_id, guint32 *out_idx)
{
  guint32 i;

  if (data)
    {
      for (i = 0; i < data->len; i++)
        {
          GDataElt *data_elt = &data->data[i];

          if (data_elt->key == key_id)
            {
              if (out_idx)
                *out_idx = i;
              return data_elt;
            }
        }
    }
  if (out_idx)
    *out_idx = G_MAXUINT32;
  return NULL;
}

/**
 * g_datalist_clear: (skip)
 * @datalist: a datalist.
 *
 * Frees all the data elements of the datalist.
 * The data elements' destroy functions are called
 * if they have been set.
 **/
void
g_datalist_clear (GData **datalist)
{
  GData *data;
  guint i;

  g_return_if_fail (datalist != NULL);

  data = g_datalist_lock_and_get (datalist);

  if (!data)
    {
      g_datalist_unlock (datalist);
      return;
    }

  g_datalist_unlock_and_set (datalist, NULL);

  for (i = 0; i < data->len; i++)
    {
      if (data->data[i].data && data->data[i].destroy)
        data->data[i].destroy (data->data[i].data);
    }

  g_free (data);
}

/* HOLDS: g_dataset_global_lock */
static inline GDataset*
g_dataset_lookup (gconstpointer	dataset_location)
{
  GDataset *dataset;
  
  if (g_dataset_cached && g_dataset_cached->location == dataset_location)
    return g_dataset_cached;
  
  dataset = g_hash_table_lookup (g_dataset_location_ht, dataset_location);
  if (dataset)
    g_dataset_cached = dataset;
  
  return dataset;
}

/* HOLDS: g_dataset_global_lock */
static void
g_dataset_destroy_internal (GDataset *dataset)
{
  gconstpointer dataset_location;
  
  dataset_location = dataset->location;
  while (dataset)
    {
      GData *data;
      guint i;

      data = G_DATALIST_GET_POINTER (&dataset->datalist);

      if (!data)
	{
	  if (dataset == g_dataset_cached)
	    g_dataset_cached = NULL;
	  g_hash_table_remove (g_dataset_location_ht, dataset_location);
	  g_slice_free (GDataset, dataset);
	  break;
	}

      G_DATALIST_SET_POINTER (&dataset->datalist, NULL);

      G_UNLOCK (g_dataset_global);

      for (i = 0; i < data->len; i++)
        {
          if (data->data[i].data && data->data[i].destroy)
            data->data[i].destroy (data->data[i].data);
        }
      g_free (data);

      G_LOCK (g_dataset_global);
      dataset = g_dataset_lookup (dataset_location);
    }
}

/**
 * g_dataset_destroy:
 * @dataset_location: (not nullable): the location identifying the dataset.
 *
 * Destroys the dataset, freeing all memory allocated, and calling any
 * destroy functions set for data elements.
 */
void
g_dataset_destroy (gconstpointer  dataset_location)
{
  g_return_if_fail (dataset_location != NULL);
  
  G_LOCK (g_dataset_global);
  if (g_dataset_location_ht)
    {
      GDataset *dataset;

      dataset = g_dataset_lookup (dataset_location);
      if (dataset)
	g_dataset_destroy_internal (dataset);
    }
  G_UNLOCK (g_dataset_global);
}

/* HOLDS: g_dataset_global_lock if dataset != null */
static inline gpointer
g_data_set_internal (GData	  **datalist,
		     GQuark         key_id,
		     gpointer       new_data,
		     GDestroyNotify new_destroy_func,
		     GDataset	   *dataset)
{
  GData *d;
  GData *new_d = NULL;
  GDataElt old, *data;
  guint32 idx;

  d = g_datalist_lock_and_get (datalist);

  data = datalist_find (d, key_id, &idx);

  if (new_data == NULL) /* remove */
    {
      if (data)
        {
          GData *d_to_free;

          old = *data;

          datalist_remove (d, idx);
          if (datalist_shrink (&d, &d_to_free))
            {
              g_datalist_unlock_and_set (datalist, d);

              /* the dataset destruction *must* be done
               * prior to invocation of the data destroy function
               */
              if (dataset && !d)
                g_dataset_destroy_internal (dataset);

              if (d_to_free)
                g_free (d_to_free);
            }
          else
            g_datalist_unlock (datalist);

          /* We found and removed an old value
           * the GData struct *must* already be unlinked
           * when invoking the destroy function.
           * we use (new_data==NULL && new_destroy_func!=NULL) as
           * a special hint combination to "steal"
           * data without destroy notification
           */
          if (old.destroy && !new_destroy_func)
            {
              if (dataset)
                G_UNLOCK (g_dataset_global);
              old.destroy (old.data);
              if (dataset)
                G_LOCK (g_dataset_global);
              old.data = NULL;
            }

          return old.data;
        }
    }
  else
    {
      if (data)
        {
          if (!data->destroy)
            {
              data->data = new_data;
              data->destroy = new_destroy_func;
              g_datalist_unlock (datalist);
            }
          else
            {
              old = *data;
              data->data = new_data;
              data->destroy = new_destroy_func;

              g_datalist_unlock (datalist);

              /* We found and replaced an old value
               * the GData struct *must* already be unlinked
               * when invoking the destroy function.
               */
              if (dataset)
                G_UNLOCK (g_dataset_global);
              old.destroy (old.data);
              if (dataset)
                G_LOCK (g_dataset_global);
            }
          return NULL;
        }

      /* The key was not found, insert it */
      if (datalist_append (&d, key_id, new_data, new_destroy_func))
        new_d = d;
    }

  if (new_d)
    g_datalist_unlock_and_set (datalist, new_d);
  else
    g_datalist_unlock (datalist);

  return NULL;

}

static inline void
g_data_remove_internal (GData  **datalist,
                        GQuark  *keys,
                        gsize    n_keys)
{
  GData *d;
  GDataElt *old;
  GDataElt *old_to_free = NULL;
  GData *d_to_free;
  gsize found_keys;
  gsize i_keys;
  guint32 i_data;

  d = g_datalist_lock_and_get (datalist);

  if (!d)
    {
      g_datalist_unlock (datalist);
      return;
    }

  /* Allocate an array of GDataElt to hold copies of the elements
   * that are removed from the datalist. Allow enough space for all
   * the keys; if a key is not found, the corresponding element of
   * old is not populated, so we initialize them all to NULL to
   * detect that case.
   *
   * At most allocate 400 bytes on the stack. Especially since we call
   * out to external code, we don't know how much stack we can use. */
  if (n_keys <= 400u / sizeof (GDataElt))
    old = g_newa0 (GDataElt, n_keys);
  else
    {
      old_to_free = g_new0 (GDataElt, n_keys);
      old = old_to_free;
    }

  i_data = 0;
  found_keys = 0;
  while (i_data < d->len && found_keys < n_keys)
    {
      GDataElt *data = &d->data[i_data];
      gboolean remove = FALSE;

      for (i_keys = 0; i_keys < n_keys; i_keys++)
        {
          if (data->key == keys[i_keys])
            {
              /* We must invoke the destroy notifications in the order of @keys.
               * Hence, build up the list @old at index @i_keys. */
              old[i_keys] = *data;
              found_keys++;
              remove = TRUE;
              break;
            }
        }

      if (!remove)
        {
          i_data++;
          continue;
        }

      datalist_remove (d, i_data);
    }

  if (found_keys > 0 && datalist_shrink (&d, &d_to_free))
    {
      g_datalist_unlock_and_set (datalist, d);
      if (d_to_free)
        g_free (d_to_free);
    }
  else
    g_datalist_unlock (datalist);

  if (found_keys > 0)
    {
      for (i_keys = 0; i_keys < n_keys; i_keys++)
        {
          if (old[i_keys].destroy)
            old[i_keys].destroy (old[i_keys].data);
        }
    }

  if (G_UNLIKELY (old_to_free))
    g_free (old_to_free);
}

/**
 * g_dataset_id_set_data_full: (skip)
 * @dataset_location: (not nullable): the location identifying the dataset.
 * @key_id: the #GQuark id to identify the data element.
 * @data: the data element.
 * @destroy_func: the function to call when the data element is
 *                removed. This function will be called with the data
 *                element and can be used to free any memory allocated
 *                for it.
 *
 * Sets the data element associated with the given #GQuark id, and also
 * the function to call when the data element is destroyed. Any
 * previous data with the same key is removed, and its destroy function
 * is called.
 **/
/**
 * g_dataset_set_data_full: (skip)
 * @l: the location identifying the dataset.
 * @k: the string to identify the data element.
 * @d: the data element.
 * @f: the function to call when the data element is removed. This
 *     function will be called with the data element and can be used to
 *     free any memory allocated for it.
 *
 * Sets the data corresponding to the given string identifier, and the
 * function to call when the data element is destroyed.
 **/
/**
 * g_dataset_id_set_data:
 * @l: the location identifying the dataset.
 * @k: the #GQuark id to identify the data element.
 * @d: the data element.
 *
 * Sets the data element associated with the given #GQuark id. Any
 * previous data with the same key is removed, and its destroy function
 * is called.
 **/
/**
 * g_dataset_set_data:
 * @l: the location identifying the dataset.
 * @k: the string to identify the data element.
 * @d: the data element.
 *
 * Sets the data corresponding to the given string identifier.
 **/
/**
 * g_dataset_id_remove_data:
 * @l: the location identifying the dataset.
 * @k: the #GQuark id identifying the data element.
 *
 * Removes a data element from a dataset. The data element's destroy
 * function is called if it has been set.
 **/
/**
 * g_dataset_remove_data:
 * @l: the location identifying the dataset.
 * @k: the string identifying the data element.
 *
 * Removes a data element corresponding to a string. Its destroy
 * function is called if it has been set.
 **/
void
g_dataset_id_set_data_full (gconstpointer  dataset_location,
			    GQuark         key_id,
			    gpointer       data,
			    GDestroyNotify destroy_func)
{
  GDataset *dataset;
  
  g_return_if_fail (dataset_location != NULL);
  if (!data)
    g_return_if_fail (destroy_func == NULL);
  if (!key_id)
    {
      if (data)
	g_return_if_fail (key_id > 0);
      else
	return;
    }
  
  G_LOCK (g_dataset_global);
  if (!g_dataset_location_ht)
    g_data_initialize ();
 
  dataset = g_dataset_lookup (dataset_location);
  if (!dataset)
    {
      dataset = g_slice_new (GDataset);
      dataset->location = dataset_location;
      g_datalist_init (&dataset->datalist);
      g_hash_table_insert (g_dataset_location_ht, 
			   (gpointer) dataset->location,
			   dataset);
    }
  
  g_data_set_internal (&dataset->datalist, key_id, data, destroy_func, dataset);
  G_UNLOCK (g_dataset_global);
}

/**
 * g_datalist_id_set_data_full: (skip)
 * @datalist: a datalist.
 * @key_id: the #GQuark to identify the data element.
 * @data: (nullable): the data element or %NULL to remove any previous element
 *        corresponding to @key_id.
 * @destroy_func: (nullable): the function to call when the data element is
 *                removed. This function will be called with the data
 *                element and can be used to free any memory allocated
 *                for it. If @data is %NULL, then @destroy_func must
 *                also be %NULL.
 *
 * Sets the data corresponding to the given #GQuark id, and the
 * function to be called when the element is removed from the datalist.
 * Any previous data with the same key is removed, and its destroy
 * function is called.
 **/
/**
 * g_datalist_set_data_full: (skip)
 * @dl: a datalist.
 * @k: the string to identify the data element.
 * @d: (nullable): the data element, or %NULL to remove any previous element
 *     corresponding to @k.
 * @f: (nullable): the function to call when the data element is removed.
 *     This function will be called with the data element and can be used to
 *     free any memory allocated for it. If @d is %NULL, then @f must
 *     also be %NULL.
 *
 * Sets the data element corresponding to the given string identifier,
 * and the function to be called when the data element is removed.
 **/
/**
 * g_datalist_id_set_data:
 * @dl: a datalist.
 * @q: the #GQuark to identify the data element.
 * @d: (nullable): the data element, or %NULL to remove any previous element
 *     corresponding to @q.
 *
 * Sets the data corresponding to the given #GQuark id. Any previous
 * data with the same key is removed, and its destroy function is
 * called.
 **/
/**
 * g_datalist_set_data:
 * @dl: a datalist.
 * @k: the string to identify the data element.
 * @d: (nullable): the data element, or %NULL to remove any previous element
 *     corresponding to @k.
 *
 * Sets the data element corresponding to the given string identifier.
 **/
/**
 * g_datalist_id_remove_data:
 * @dl: a datalist.
 * @q: the #GQuark identifying the data element.
 *
 * Removes an element, using its #GQuark identifier.
 **/
/**
 * g_datalist_remove_data:
 * @dl: a datalist.
 * @k: the string identifying the data element.
 *
 * Removes an element using its string identifier. The data element's
 * destroy function is called if it has been set.
 **/
void
g_datalist_id_set_data_full (GData	  **datalist,
			     GQuark         key_id,
			     gpointer       data,
			     GDestroyNotify destroy_func)
{
  g_return_if_fail (datalist != NULL);
  if (!data)
    g_return_if_fail (destroy_func == NULL);
  if (!key_id)
    {
      if (data)
	g_return_if_fail (key_id > 0);
      else
	return;
    }

  g_data_set_internal (datalist, key_id, data, destroy_func, NULL);
}

/**
 * g_datalist_id_remove_multiple:
 * @datalist: a datalist
 * @keys: (array length=n_keys): keys to remove
 * @n_keys: length of @keys.
 *
 * Removes multiple keys from a datalist.
 *
 * This is more efficient than calling g_datalist_id_remove_data()
 * multiple times in a row.
 *
 * Before 2.80, @n_keys had to be not larger than 16. Now it can be larger, but
 * note that GData does a linear search, so an excessive number of keys will
 * perform badly.
 *
 * Since: 2.74
 */
void
g_datalist_id_remove_multiple (GData  **datalist,
                               GQuark  *keys,
                               gsize    n_keys)
{
  g_data_remove_internal (datalist, keys, n_keys);
}

/**
 * g_dataset_id_remove_no_notify: (skip)
 * @dataset_location: (not nullable): the location identifying the dataset.
 * @key_id: the #GQuark ID identifying the data element.
 *
 * Removes an element, without calling its destroy notification
 * function.
 *
 * Returns: (nullable): the data previously stored at @key_id,
 *          or %NULL if none.
 **/
/**
 * g_dataset_remove_no_notify: (skip)
 * @l: the location identifying the dataset.
 * @k: the string identifying the data element.
 *
 * Removes an element, without calling its destroy notifier.
 **/
gpointer
g_dataset_id_remove_no_notify (gconstpointer  dataset_location,
			       GQuark         key_id)
{
  gpointer ret_data = NULL;

  g_return_val_if_fail (dataset_location != NULL, NULL);
  
  G_LOCK (g_dataset_global);
  if (key_id && g_dataset_location_ht)
    {
      GDataset *dataset;
  
      dataset = g_dataset_lookup (dataset_location);
      if (dataset)
	ret_data = g_data_set_internal (&dataset->datalist, key_id, NULL, (GDestroyNotify) 42, dataset);
    } 
  G_UNLOCK (g_dataset_global);

  return ret_data;
}

/**
 * g_datalist_id_remove_no_notify: (skip)
 * @datalist: a datalist.
 * @key_id: the #GQuark identifying a data element.
 *
 * Removes an element, without calling its destroy notification
 * function.
 *
 * Returns: (nullable): the data previously stored at @key_id,
 *          or %NULL if none.
 **/
/**
 * g_datalist_remove_no_notify: (skip)
 * @dl: a datalist.
 * @k: the string identifying the data element.
 *
 * Removes an element, without calling its destroy notifier.
 **/
gpointer
g_datalist_id_remove_no_notify (GData	**datalist,
				GQuark    key_id)
{
  gpointer ret_data = NULL;

  g_return_val_if_fail (datalist != NULL, NULL);

  if (key_id)
    ret_data = g_data_set_internal (datalist, key_id, NULL, (GDestroyNotify) 42, NULL);

  return ret_data;
}

/*< private >
 * g_datalist_id_update_atomic:
 * @datalist: the data list
 * @key_id: the key to add.
 * @callback: (scope call): callback to update (set, remove, steal, update) the
 *   data.
 * @user_data: the user data for @callback.
 *
 * Will call @callback while holding the lock on @datalist. Be careful to not
 * end up calling into another data-list function, because the lock is not
 * reentrant and deadlock will happen.
 *
 * The callback receives the current data and destroy function. If @key_id is
 * currently not in @datalist, they will be %NULL. The callback can update
 * those pointers, and @datalist will be updated with the result. Note that if
 * callback modifies a received data, then it MUST steal it and take ownership
 * on it. Possibly by freeing it with the provided destroy function.
 *
 * The point is to atomically access the entry, while holding a lock
 * of @datalist. Without this, the user would have to hold their own mutex
 * while handling @key_id entry.
 *
 * The return value of @callback is not used, except it becomes the return
 * value of the function. This is an alternative to returning a result via
 * @user_data.
 *
 * Returns: the value returned by @callback.
 *
 * Since: 2.80
 */
gpointer
g_datalist_id_update_atomic (GData **datalist,
                             GQuark key_id,
                             GDataListUpdateAtomicFunc callback,
                             gpointer user_data)
{
  GData *d;
  GDataElt *data;
  gpointer new_data;
  gpointer result;
  GDestroyNotify new_destroy;
  guint32 idx;
  gboolean to_unlock = TRUE;

  d = g_datalist_lock_and_get (datalist);

  data = datalist_find (d, key_id, &idx);

  if (data)
    {
      new_data = data->data;
      new_destroy = data->destroy;
    }
  else
    {
      new_data = NULL;
      new_destroy = NULL;
    }

  result = callback (key_id, &new_data, &new_destroy, user_data);

  if (data && !new_data)
    {
      GData *d_to_free;

      /* Remove. The callback indicates to drop the entry.
       *
       * The old data->data was stolen by callback(). */
      datalist_remove (d, idx);
      if (datalist_shrink (&d, &d_to_free))
        {
          g_datalist_unlock_and_set (datalist, d);
          if (d_to_free)
            g_free (d_to_free);
          to_unlock = FALSE;
        }
    }
  else if (data)
    {
      /* Update. The callback may have provided new pointers to an existing
       * entry.
       *
       * The old data was stolen by callback(). We only update the pointers and
       * are done. */
      data->data = new_data;
      data->destroy = new_destroy;
    }
  else if (!data && !new_data)
    {
      /* Absent. No change. The entry didn't exist and still does not. */
    }
  else
    {
      /* Add. Add a new entry that didn't exist previously. */
      if (datalist_append (&d, key_id, new_data, new_destroy))
        {
          g_datalist_unlock_and_set (datalist, d);
          to_unlock = FALSE;
        }
    }

  if (to_unlock)
    g_datalist_unlock (datalist);

  return result;
}

/**
 * g_dataset_id_get_data:
 * @dataset_location: (not nullable): the location identifying the dataset.
 * @key_id: the #GQuark id to identify the data element.
 *
 * Gets the data element corresponding to a #GQuark.
 *
 * Returns: (transfer none) (nullable): the data element corresponding to
 *          the #GQuark, or %NULL if it is not found.
 **/
/**
 * g_dataset_get_data:
 * @l: the location identifying the dataset.
 * @k: the string identifying the data element.
 *
 * Gets the data element corresponding to a string.
 *
 * Returns: (transfer none) (nullable): the data element corresponding to
 *          the string, or %NULL if it is not found.
 **/
gpointer
g_dataset_id_get_data (gconstpointer  dataset_location,
		       GQuark         key_id)
{
  gpointer retval = NULL;

  g_return_val_if_fail (dataset_location != NULL, NULL);
  
  G_LOCK (g_dataset_global);
  if (key_id && g_dataset_location_ht)
    {
      GDataset *dataset;
      
      dataset = g_dataset_lookup (dataset_location);
      if (dataset)
	retval = g_datalist_id_get_data (&dataset->datalist, key_id);
    }
  G_UNLOCK (g_dataset_global);
 
  return retval;
}

/**
 * g_datalist_id_get_data:
 * @datalist: a datalist.
 * @key_id: the #GQuark identifying a data element.
 *
 * Retrieves the data element corresponding to @key_id.
 *
 * Returns: (transfer none) (nullable): the data element, or %NULL if
 *          it is not found.
 */
gpointer
g_datalist_id_get_data (GData  **datalist,
			GQuark   key_id)
{
  return g_datalist_id_dup_data (datalist, key_id, NULL, NULL);
}

/**
 * GDuplicateFunc:
 * @data: the data to duplicate
 * @user_data: (closure): user data that was specified in
 *             g_datalist_id_dup_data()
 *
 * The type of functions that are used to 'duplicate' an object.
 * What this means depends on the context, it could just be
 * incrementing the reference count, if @data is a ref-counted
 * object.
 *
 * Returns: a duplicate of data
 */

/**
 * g_datalist_id_dup_data: (skip)
 * @datalist: location of a datalist
 * @key_id: the #GQuark identifying a data element
 * @dup_func: (scope call) (closure user_data) (nullable): function to
 *   duplicate the old value
 * @user_data: passed as user_data to @dup_func
 *
 * This is a variant of g_datalist_id_get_data() which
 * returns a 'duplicate' of the value. @dup_func defines the
 * meaning of 'duplicate' in this context, it could e.g.
 * take a reference on a ref-counted object.
 *
 * If the @key_id is not set in the datalist then @dup_func
 * will be called with a %NULL argument.
 *
 * Note that @dup_func is called while the datalist is locked, so it
 * is not allowed to read or modify the datalist.
 *
 * This function can be useful to avoid races when multiple
 * threads are using the same datalist and the same key.
 *
 * Returns: (nullable): the result of calling @dup_func on the value
 *     associated with @key_id in @datalist, or %NULL if not set.
 *     If @dup_func is %NULL, the value is returned unmodified.
 *
 * Since: 2.34
 */
gpointer
g_datalist_id_dup_data (GData          **datalist,
                        GQuark           key_id,
                        GDuplicateFunc   dup_func,
                        gpointer         user_data)
{
  gpointer val = NULL;
  gpointer retval = NULL;
  GData *d;
  GDataElt *data;

  d = g_datalist_lock_and_get (datalist);

  data = datalist_find (d, key_id, NULL);
  if (data)
    val = data->data;

  if (dup_func)
    retval = dup_func (val, user_data);
  else
    retval = val;

  g_datalist_unlock (datalist);

  return retval;
}

/**
 * g_datalist_id_replace_data: (skip)
 * @datalist: location of a datalist
 * @key_id: the #GQuark identifying a data element
 * @oldval: (nullable): the old value to compare against
 * @newval: (nullable): the new value to replace it with
 * @destroy: (nullable): destroy notify for the new value
 * @old_destroy: (out) (optional): destroy notify for the existing value
 *
 * Compares the member that is associated with @key_id in
 * @datalist to @oldval, and if they are the same, replace
 * @oldval with @newval.
 *
 * This is like a typical atomic compare-and-exchange
 * operation, for a member of @datalist.
 *
 * If the previous value was replaced then ownership of the
 * old value (@oldval) is passed to the caller, including
 * the registered destroy notify for it (passed out in @old_destroy).
 * Its up to the caller to free this as they wish, which may
 * or may not include using @old_destroy as sometimes replacement
 * should not destroy the object in the normal way.
 *
 * Returns: %TRUE if the existing value for @key_id was replaced
 *  by @newval, %FALSE otherwise.
 *
 * Since: 2.34
 */
gboolean
g_datalist_id_replace_data (GData          **datalist,
                            GQuark           key_id,
                            gpointer         oldval,
                            gpointer         newval,
                            GDestroyNotify   destroy,
                            GDestroyNotify  *old_destroy)
{
  gpointer val = NULL;
  GData *d;
  GDataElt *data;
  GData *d_to_free = NULL;
  gboolean set_d = FALSE;
  guint32 idx;

  g_return_val_if_fail (datalist != NULL, FALSE);
  g_return_val_if_fail (key_id != 0, FALSE);

  if (old_destroy)
    *old_destroy = NULL;

  d = g_datalist_lock_and_get (datalist);

  data = datalist_find (d, key_id, &idx);
  if (data)
    {
      val = data->data;
      if (val == oldval)
        {
          if (old_destroy)
            *old_destroy = data->destroy;
          if (newval != NULL)
            {
              data->data = newval;
              data->destroy = destroy;
            }
          else
            {
              datalist_remove (d, idx);
              if (datalist_shrink (&d, &d_to_free))
                set_d = TRUE;
            }
        }
    }

  if (val == NULL && oldval == NULL && newval != NULL)
    {
      if (datalist_append (&d, key_id, newval, destroy))
        {
          set_d = TRUE;
        }
    }

  if (set_d)
    g_datalist_unlock_and_set (datalist, d);
  else
    g_datalist_unlock (datalist);

  if (d_to_free)
    g_free (d_to_free);

  return val == oldval;
}

/**
 * g_datalist_get_data:
 * @datalist: a datalist.
 * @key: the string identifying a data element.
 *
 * Gets a data element, using its string identifier. This is slower than
 * g_datalist_id_get_data() because it compares strings.
 *
 * Returns: (transfer none) (nullable): the data element, or %NULL if it
 *          is not found.
 **/
gpointer
g_datalist_get_data (GData	 **datalist,
		     const gchar *key)
{
  gpointer res = NULL;
  GData *d;
  GDataElt *data, *data_end;

  g_return_val_if_fail (datalist != NULL, NULL);

  d = g_datalist_lock_and_get (datalist);
  if (d)
    {
      data = d->data;
      data_end = data + d->len;
      while (data < data_end)
	{
	  /* Here we intentionally compare by strings, instead of calling
	   * g_quark_try_string() first.
	   *
	   * See commit 1cceda49b60b ('Make g_datalist_get_data not look up the
	   * quark').
	   */
	  if (g_strcmp0 (g_quark_to_string (data->key), key) == 0)
	    {
	      res = data->data;
	      break;
	    }
	  data++;
	}
    }

  g_datalist_unlock (datalist);

  return res;
}

/**
 * GDataForeachFunc:
 * @key_id: the #GQuark id to identifying the data element.
 * @data: the data element.
 * @user_data: (closure): user data passed to g_dataset_foreach().
 *
 * Specifies the type of function passed to g_dataset_foreach(). It is
 * called with each #GQuark id and associated data element, together
 * with the @user_data parameter supplied to g_dataset_foreach().
 **/

/**
 * g_dataset_foreach:
 * @dataset_location: (not nullable): the location identifying the dataset.
 * @func: (scope call) (closure user_data): the function to call for each data element.
 * @user_data: user data to pass to the function.
 *
 * Calls the given function for each data element which is associated
 * with the given location. Note that this function is NOT thread-safe.
 * So unless @dataset_location can be protected from any modifications
 * during invocation of this function, it should not be called.
 *
 * @func can make changes to the dataset, but the iteration will not
 * reflect changes made during the g_dataset_foreach() call, other
 * than skipping over elements that are removed.
 **/
void
g_dataset_foreach (gconstpointer    dataset_location,
		   GDataForeachFunc func,
		   gpointer         user_data)
{
  GDataset *dataset;
  
  g_return_if_fail (dataset_location != NULL);
  g_return_if_fail (func != NULL);

  G_LOCK (g_dataset_global);
  if (g_dataset_location_ht)
    {
      dataset = g_dataset_lookup (dataset_location);
      G_UNLOCK (g_dataset_global);
      if (dataset)
	g_datalist_foreach (&dataset->datalist, func, user_data);
    }
  else
    {
      G_UNLOCK (g_dataset_global);
    }
}

/**
 * g_datalist_foreach:
 * @datalist: a datalist.
 * @func: (scope call) (closure user_data): the function to call for each data element.
 * @user_data: user data to pass to the function.
 *
 * Calls the given function for each data element of the datalist. The
 * function is called with each data element's #GQuark id and data,
 * together with the given @user_data parameter. Note that this
 * function is NOT thread-safe. So unless @datalist can be protected
 * from any modifications during invocation of this function, it should
 * not be called.
 *
 * @func can make changes to @datalist, but the iteration will not
 * reflect changes made during the g_datalist_foreach() call, other
 * than skipping over elements that are removed.
 **/
void
g_datalist_foreach (GData	   **datalist,
		    GDataForeachFunc func,
		    gpointer         user_data)
{
  GData *d;
  guint i, j, len;
  GQuark *keys;

  g_return_if_fail (datalist != NULL);
  g_return_if_fail (func != NULL);

  d = G_DATALIST_GET_POINTER (datalist);
  if (d == NULL) 
    return;

  /* We make a copy of the keys so that we can handle it changing
     in the callback */
  len = d->len;
  keys = g_new (GQuark, len);
  for (i = 0; i < len; i++)
    keys[i] = d->data[i].key;
  
  for (i = 0; i < len; i++)
    {
      /* A previous callback might have removed a later item, so always check that
	 it still exists before calling */
      d = G_DATALIST_GET_POINTER (datalist);
      
      if (d == NULL)
	break;
      for (j = 0; j < d->len; j++)
	{
	  if (d->data[j].key == keys[i]) {
	    func (d->data[i].key, d->data[i].data, user_data);
	    break;
	  }
	}
    }
  g_free (keys);
}

/**
 * g_datalist_init: (skip)
 * @datalist: a pointer to a pointer to a datalist.
 *
 * Resets the datalist to %NULL. It does not free any memory or call
 * any destroy functions.
 **/
void
g_datalist_init (GData **datalist)
{
  g_return_if_fail (datalist != NULL);

  g_atomic_pointer_set (datalist, NULL);
}

/**
 * g_datalist_set_flags:
 * @datalist: pointer to the location that holds a list
 * @flags: the flags to turn on. The values of the flags are
 *   restricted by %G_DATALIST_FLAGS_MASK (currently
 *   3; giving two possible boolean flags).
 *   A value for @flags that doesn't fit within the mask is
 *   an error.
 * 
 * Turns on flag values for a data list. This function is used
 * to keep a small number of boolean flags in an object with
 * a data list without using any additional space. It is
 * not generally useful except in circumstances where space
 * is very tight. (It is used in the base #GObject type, for
 * example.)
 *
 * Since: 2.8
 **/
void
g_datalist_set_flags (GData **datalist,
		      guint   flags)
{
  g_return_if_fail (datalist != NULL);
  g_return_if_fail ((flags & ~G_DATALIST_FLAGS_MASK) == 0);

  g_atomic_pointer_or (datalist, (gsize)flags);
}

/**
 * g_datalist_unset_flags:
 * @datalist: pointer to the location that holds a list
 * @flags: the flags to turn off. The values of the flags are
 *   restricted by %G_DATALIST_FLAGS_MASK (currently
 *   3: giving two possible boolean flags).
 *   A value for @flags that doesn't fit within the mask is
 *   an error.
 * 
 * Turns off flag values for a data list. See g_datalist_unset_flags()
 *
 * Since: 2.8
 **/
void
g_datalist_unset_flags (GData **datalist,
			guint   flags)
{
  g_return_if_fail (datalist != NULL);
  g_return_if_fail ((flags & ~G_DATALIST_FLAGS_MASK) == 0);

  g_atomic_pointer_and (datalist, ~(gsize)flags);
}

/**
 * g_datalist_get_flags:
 * @datalist: pointer to the location that holds a list
 * 
 * Gets flags values packed in together with the datalist.
 * See g_datalist_set_flags().
 * 
 * Returns: the flags of the datalist
 *
 * Since: 2.8
 **/
guint
g_datalist_get_flags (GData **datalist)
{
  g_return_val_if_fail (datalist != NULL, 0);
  
  return G_DATALIST_GET_FLAGS (datalist); /* atomic macro */
}

/* HOLDS: g_dataset_global_lock */
static void
g_data_initialize (void)
{
  g_return_if_fail (g_dataset_location_ht == NULL);

  g_dataset_location_ht = g_hash_table_new (g_direct_hash, NULL);
  g_dataset_cached = NULL;
}
