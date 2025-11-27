/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Typelib hashing
 *
 * Copyright (C) 2010 Red Hat, Inc.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <glib.h>
#include <glib-object.h>
#include <string.h>

#include "cmph/cmph.h"
#include "gitypelib-internal.h"

#define ALIGN_VALUE(this, boundary) \
  (( ((unsigned long)(this)) + (((unsigned long)(boundary)) -1)) & (~(((unsigned long)(boundary))-1)))

/*
 * String hashing in the typelib.  We have a set of static (fixed) strings,
 * and given one, we need to find its index number.  This problem is perfect
 * hashing: http://en.wikipedia.org/wiki/Perfect_hashing
 *
 * I chose CMPH (http://cmph.sourceforge.net/) as it seemed high
 * quality, well documented, and easy to embed.
 *
 * CMPH provides a number of algorithms; I chose BDZ, because while CHD
 * appears to be the "best", the simplicitly of BDZ appealed, and really,
 * we're only talking about thousands of strings here, not millions, so
 * a few microseconds is no big deal.
 *
 * In memory, the format is:
 * INT32 mph_size
 * MPH (mph_size bytes)
 * (padding for alignment to uint32 if necessary)
 * INDEX (array of guint16)
 *
 * Because BDZ is not order preserving, we need a lookaside table which
 * maps the hash value into the directory index.
 */

struct _GITypelibHashBuilder {
  gboolean prepared;
  gboolean buildable;
  cmph_t *c;
  GHashTable *strings;
  guint32 dirmap_offset;
  guint32 packed_size;
};

GITypelibHashBuilder *
_gi_typelib_hash_builder_new (void)
{
  GITypelibHashBuilder *builder = g_slice_new0 (GITypelibHashBuilder);
  builder->c = NULL;
  builder->strings = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  return builder;
}

void
_gi_typelib_hash_builder_add_string (GITypelibHashBuilder *builder,
				     const char           *str,
				     guint16               value)
{
  g_return_if_fail (builder->c == NULL);
  g_hash_table_insert (builder->strings, g_strdup (str), GUINT_TO_POINTER ((guint) value));
}

gboolean
_gi_typelib_hash_builder_prepare (GITypelibHashBuilder *builder)
{
  char **strs;
  GHashTableIter hashiter;
  gpointer key, value;
  cmph_io_adapter_t *io;
  cmph_config_t *config;
  guint32 num_elts;
  guint32 offset;
  guint i;

  if (builder->prepared)
    return builder->buildable;
  g_assert (builder->c == NULL);

  num_elts = g_hash_table_size (builder->strings);
  g_assert (num_elts <= 65536);

  strs = (char**) g_new (char *, num_elts + 1);

  i = 0;
  g_hash_table_iter_init (&hashiter, builder->strings);
  while (g_hash_table_iter_next (&hashiter, &key, &value))
    {
      const char *str = key;

      strs[i++] = g_strdup (str);
    }
  strs[i++] = NULL;

  io = cmph_io_vector_adapter (strs, num_elts);
  config = cmph_config_new (io);
  cmph_config_set_algo (config, CMPH_BDZ);

  builder->c = cmph_new (config);
  builder->prepared = TRUE;
  if (!builder->c)
    {
      builder->buildable = FALSE;
      goto out;
    }
  builder->buildable = TRUE;
  g_assert (cmph_size (builder->c) == num_elts);

  /* Pack a size counter at front */
  offset = sizeof(guint32) + cmph_packed_size (builder->c);
  builder->dirmap_offset = ALIGN_VALUE (offset, 4);
  builder->packed_size = builder->dirmap_offset + (num_elts * sizeof(guint16));
 out:
  cmph_config_destroy (config);
  cmph_io_vector_adapter_destroy (io);
  return builder->buildable;
}

guint32
_gi_typelib_hash_builder_get_buffer_size (GITypelibHashBuilder *builder)
{
  g_return_val_if_fail (builder != NULL, 0);
  g_return_val_if_fail (builder->prepared, 0);
  g_return_val_if_fail (builder->buildable, 0 );

  return builder->packed_size;
}

void
_gi_typelib_hash_builder_pack (GITypelibHashBuilder *builder, guint8* mem, guint32 len)
{
  guint16 *table;
  GHashTableIter hashiter;
  gpointer key, value;
  guint32 num_elts;
  guint8 *packed_mem;

  g_return_if_fail (builder != NULL);
  g_return_if_fail (builder->prepared);
  g_return_if_fail (builder->buildable);

  g_assert (len >= builder->packed_size);
  g_assert ((((size_t)mem) & 0x3) == 0);

  memset (mem, 0, len);

  *((guint32*) mem) = builder->dirmap_offset;
  packed_mem = (guint8*)(mem + sizeof(guint32));
  cmph_pack (builder->c, packed_mem);

  table = (guint16*) (mem + builder->dirmap_offset);

  num_elts = g_hash_table_size (builder->strings);
  g_hash_table_iter_init (&hashiter, builder->strings);
  while (g_hash_table_iter_next (&hashiter, &key, &value))
    {
      const char *str = key;
      guint16 strval = (guint16)GPOINTER_TO_UINT(value);
      guint32 hashv;

      hashv = cmph_search_packed (packed_mem, str, strlen (str));
      g_assert (hashv < num_elts);
      table[hashv] = strval;
    }
}

void
_gi_typelib_hash_builder_destroy (GITypelibHashBuilder *builder)
{
  if (builder->c)
    {
      cmph_destroy (builder->c);
      builder->c = NULL;
    }
  g_hash_table_destroy (builder->strings);
  g_slice_free (GITypelibHashBuilder, builder);
}

guint16
_gi_typelib_hash_search (guint8* memory, const char *str, guint n_entries)
{
  guint32 *mph;
  guint16 *table;
  guint32 dirmap_offset;
  guint32 offset;

  g_assert ((((size_t)memory) & 0x3) == 0);
  mph = ((guint32*)memory)+1;

  offset = cmph_search_packed (mph, str, strlen (str));

  /* Make sure that offset always lies in the entries array.  cmph
     cometimes generates offset larger than number of entries (for
     'str' argument which is not in the hashed list). In this case,
     fake the correct result and depend on caller's final check that
     the entry is really the one that the caller wanted. */
  if (offset >= n_entries)
    offset = 0;

  dirmap_offset = *((guint32*)memory);
  table = (guint16*) (memory + dirmap_offset);

  return table[offset];
}

