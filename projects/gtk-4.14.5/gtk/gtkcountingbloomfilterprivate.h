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
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#pragma once

#include <glib.h>


G_BEGIN_DECLS

/*
 * GtkCountingBloomFilter:
 *
 * This implements a counting bloom filter. A bloom filter is a space-efficient
 * probabilistic data structure that is used to test whether an element may be
 * a member of a set.  
 * The Wikipedia links provide a lot more details into how and why this data
 * structure works and when to use it.
 *
 * This implementation is based on similar implementations in web browsers, because it's
 * original use case is the same: Making CSS lookups fast.
 *
 * As such, the number of bits is hardcoded to 12 and the elements in the set
 * are 16bit hash values.  
 * It is possible to use 32bit hash values or a different number of bits, should this
 * be considered useful.
 *
 * See: [Bloom filter](https://en.wikipedia.org/wiki/Bloom_filter),
 *   [Counting Bloom filter](https://en.wikipedia.org/wiki/Counting_Bloom_filter)
 */

/* The number of bits from the hash we care about */
#define GTK_COUNTING_BLOOM_FILTER_BITS (12)

/* The necessary size of the filter */
#define GTK_COUNTING_BLOOM_FILTER_SIZE (1 << GTK_COUNTING_BLOOM_FILTER_BITS)

typedef struct _GtkCountingBloomFilter GtkCountingBloomFilter;

struct _GtkCountingBloomFilter
{
  guint8        buckets[GTK_COUNTING_BLOOM_FILTER_SIZE];
};

static inline void      gtk_counting_bloom_filter_add           (GtkCountingBloomFilter         *self,
                                                                 guint16                         hash);
static inline void      gtk_counting_bloom_filter_remove        (GtkCountingBloomFilter         *self,
                                                                 guint16                         hash);
static inline gboolean  gtk_counting_bloom_filter_may_contain   (const GtkCountingBloomFilter   *self,
                                                                 guint16                         hash);


/*
 * GTK_COUNTING_BLOOM_FILTER_INIT:
 *
 * Initialize the bloom filter. As bloom filters are always stack-allocated,
 * initialization should happen when defining them, like:
 * ```c
 * GtkCountingBloomFilter filter = GTK_COUNTING_BLOOM_FILTER_INIT;
 * ```
 *
 * The filter does not need to be freed.
 */
#define GTK_COUNTING_BLOOM_FILTER_INIT {{0}}

/*
 * gtk_counting_bloom_filter_add:
 * @self: a `GtkCountingBloomFilter`
 * @hash: a hash value to add to the filter
 *
 * Adds the hash value to the filter.
 *
 * If the same hash value gets added multiple times, it will
 * be considered as contained in the hash until it has been
 * removed as many times.
 **/
static inline void
gtk_counting_bloom_filter_add (GtkCountingBloomFilter *self,
                               guint16                 hash)
{
  guint16 bucket = hash % GTK_COUNTING_BLOOM_FILTER_SIZE;

  if (self->buckets[bucket] == 255)
    return;

  self->buckets[bucket]++;
}

/*
 * gtk_counting_bloom_filter_remove:
 * @self: a `GtkCountingBloomFilter`
 * @hash: a hash value to remove from the filter
 *
 * Removes a hash value from the filter that has previously
 * been added via gtk_counting_bloom_filter_add().
 **/
static inline void
gtk_counting_bloom_filter_remove (GtkCountingBloomFilter *self,
                                  guint16                 hash)
{
  guint16 bucket = hash % GTK_COUNTING_BLOOM_FILTER_SIZE;

  if (self->buckets[bucket] == 255)
    return;

  g_assert (self->buckets[bucket] > 0);

  self->buckets[bucket]--;
}

/*
 * gtk_counting_bloom_filter_may_contain:
 * @self: a `GtkCountingBloomFilter`
 * @hash: the hash value to check
 *
 * Checks if @hash may be contained in @self.
 *
 * A return value of %FALSE means that @hash is definitely not part
 * of @self.
 *
 * A return value of %TRUE means that @hash may or may not have been
 * added to @self. In that case a different method must be used to
 * confirm that @hash is indeed part of the set.
 *
 * Returns: %FALSE if @hash is not part of @self.
 **/
static inline gboolean
gtk_counting_bloom_filter_may_contain (const GtkCountingBloomFilter *self,
                                       guint16                       hash)
{
  guint16 bucket = hash % GTK_COUNTING_BLOOM_FILTER_SIZE;

  return self->buckets[bucket] != 0;
}


G_END_DECLS


