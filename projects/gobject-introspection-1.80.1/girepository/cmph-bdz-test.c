/* GObject introspection: Test cmph hashing
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

#include <glib-object.h>
#include "cmph.h"

static cmph_t *
build (void)
{
  cmph_config_t *config;
  cmph_io_adapter_t *io;
  char **strings;
  cmph_t *c;
  guint32 size;

  strings = g_strsplit ("foo,bar,baz", ",", -1);

  io = cmph_io_vector_adapter (strings, g_strv_length (strings));
  config = cmph_config_new (io);
  cmph_config_set_algo (config, CMPH_BDZ);

  c = cmph_new (config);
  size = cmph_size (c);
  g_assert (size == g_strv_length (strings));

  return c;
}

static void
assert_hashes_unique (guint    n_hashes,
		      guint32* hashes)
{
  guint i;

  for (i = 0; i < n_hashes; i++)
    {
      guint j = 0;
      for (j = 0; j < n_hashes; j++)
	{
	  if (j != i)
	    g_assert (hashes[i] != hashes[j]);
	}
    }
}

static void
test_search (void)
{
  cmph_t *c = build();
  guint i;
  guint32 hash;
  guint32 hashes[3];
  guint32 size;

  size = cmph_size (c);

  i = 0;
  hash = cmph_search (c, "foo", 3);
  g_assert (hash >= 0 && hash < size);
  hashes[i++] = hash;

  hash = cmph_search (c, "bar", 3);
  g_assert (hash >= 0 && hash < size);
  hashes[i++] = hash;

  hash = cmph_search (c, "baz", 3);
  g_assert (hash >= 0 && hash < size);
  hashes[i++] = hash;

  assert_hashes_unique (G_N_ELEMENTS (hashes), &hashes[0]);
}

static void
test_search_packed (void)
{
  cmph_t *c = build();
  guint32 bufsize;
  guint i;
  guint32 hash;
  guint32 hashes[3];
  guint32 size;
  guint8 *buf;

  bufsize = cmph_packed_size (c);
  buf = g_malloc (bufsize);
  cmph_pack (c, buf);

  size = cmph_size (c);

  cmph_destroy (c);
  c = NULL;

  i = 0;
  hash = cmph_search_packed (buf, "foo", 3);
  g_assert (hash >= 0 && hash < size);
  hashes[i++] = hash;

  hash = cmph_search_packed (buf, "bar", 3);
  g_assert (hash >= 0 && hash < size);
  hashes[i++] = hash;

  hash = cmph_search_packed (buf, "baz", 3);
  g_assert (hash >= 0 && hash < size);
  hashes[i++] = hash;

  assert_hashes_unique (G_N_ELEMENTS (hashes), &hashes[0]);
}

int
main(int argc, char **argv)
{
  gint ret;

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/cmph-bdz/search", test_search);
  g_test_add_func ("/cmph-bdz/search-packed", test_search_packed);

  ret = g_test_run ();

  return ret;
}

