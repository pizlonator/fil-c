/* gtkiconcache.c
 * Copyright (C) 2004  Anders Carlsson <andersca@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkdebug.h"
#include "gtkiconcacheprivate.h"
#include "gtkiconcachevalidatorprivate.h"
#include "gtkprivate.h"

#include <glib/gstdio.h>
#include <gdk-pixbuf/gdk-pixdata.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef G_OS_WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>


#ifndef _O_BINARY
#define _O_BINARY 0
#endif

#define GET_UINT16(cache, offset) (GUINT16_FROM_BE (*(guint16 *)((cache) + (offset))))
#define GET_UINT32(cache, offset) (GUINT32_FROM_BE (*(guint32 *)((cache) + (offset))))

struct _GtkIconCache {
  int ref_count;

  GMappedFile *map;
  char *buffer;

  guint32 last_chain_offset;
};

GtkIconCache *
gtk_icon_cache_ref (GtkIconCache *cache)
{
  cache->ref_count++;
  return cache;
}

void
gtk_icon_cache_unref (GtkIconCache *cache)
{
  cache->ref_count --;

  if (cache->ref_count == 0)
    {
      GTK_DEBUG (ICONTHEME, "unmapping icon cache");

      if (cache->map)
        g_mapped_file_unref (cache->map);
      g_free (cache);
    }
}

GtkIconCache *
gtk_icon_cache_new_for_path (const char *path)
{
  GtkIconCache *cache = NULL;
  GMappedFile *map;

  char *cache_filename;
  GStatBuf st;
  GStatBuf path_st;

   /* Check if we have a cache file */
  cache_filename = g_build_filename (path, "icon-theme.cache", NULL);

  GTK_DEBUG (ICONTHEME, "look for icon cache in %s", path);

  if (g_stat (path, &path_st) < 0)
    goto done;

  if (g_stat (cache_filename, &st) < 0 || st.st_size < 4)
    goto done;

  /* Verify cache is up-to-date */
  if (st.st_mtime < path_st.st_mtime)
    {
      GTK_DEBUG (ICONTHEME, "icon cache outdated");
      goto done;
    }

  map = g_mapped_file_new (cache_filename, FALSE, NULL);

  if (!map)
    goto done;

  if (GTK_DEBUG_CHECK (ICONTHEME))
    {
      CacheInfo info;

      info.cache = g_mapped_file_get_contents (map);
      info.cache_size = g_mapped_file_get_length (map);
      info.n_directories = 0;
      info.flags = CHECK_OFFSETS|CHECK_STRINGS;

      if (!gtk_icon_cache_validate (&info))
        {
          g_mapped_file_unref (map);
          g_warning ("Icon cache '%s' is invalid", cache_filename);

          goto done;
        }
    }

  GTK_DEBUG (ICONTHEME, "found icon cache for %s", path);

  cache = g_new0 (GtkIconCache, 1);
  cache->ref_count = 1;
  cache->map = map;
  cache->buffer = g_mapped_file_get_contents (map);

 done:
  g_free (cache_filename);

  return cache;
}

GtkIconCache *
gtk_icon_cache_new (const char *data)
{
  GtkIconCache *cache;

  cache = g_new0 (GtkIconCache, 1);
  cache->ref_count = 1;
  cache->map = NULL;
  cache->buffer = (char *)data;

  return cache;
}

static int
get_directory_index (GtkIconCache *cache,
                     const char *directory)
{
  guint32 dir_list_offset;
  int n_dirs;
  int i;

  dir_list_offset = GET_UINT32 (cache->buffer, 8);

  n_dirs = GET_UINT32 (cache->buffer, dir_list_offset);

  for (i = 0; i < n_dirs; i++)
    {
      guint32 name_offset = GET_UINT32 (cache->buffer, dir_list_offset + 4 + 4 * i);
      char *name = cache->buffer + name_offset;
      if (strcmp (name, directory) == 0)
        return i;
    }

  return -1;
}

GHashTable *
gtk_icon_cache_list_icons_in_directory (GtkIconCache *cache,
                                        const char   *directory,
                                        GtkStringSet *set)
{
  int directory_index;
  guint32 hash_offset, n_buckets;
  guint32 chain_offset;
  guint32 image_list_offset, n_images;
  int i, j;
  GHashTable *icons = NULL;
  GString *string;

  directory_index = get_directory_index (cache, directory);

  if (directory_index == -1)
    return NULL;

  hash_offset = GET_UINT32 (cache->buffer, 4);
  n_buckets = GET_UINT32 (cache->buffer, hash_offset);

  string = g_string_new ("");

  for (i = 0; i < n_buckets; i++)
    {
      chain_offset = GET_UINT32 (cache->buffer, hash_offset + 4 + 4 * i);
      while (chain_offset != 0xffffffff)
        {
          guint32 flags = 0;

          image_list_offset = GET_UINT32 (cache->buffer, chain_offset + 8);
          n_images = GET_UINT32 (cache->buffer, image_list_offset);

          for (j = 0; j < n_images; j++)
            {
              if (GET_UINT16 (cache->buffer, image_list_offset + 4 + 8 * j) ==
                  directory_index)
                {
                  flags = GET_UINT16 (cache->buffer, image_list_offset + 4 + 8 * j + 2);
                  break;
                }
            }

          if (flags != 0)
            {
              guint32 name_offset = GET_UINT32 (cache->buffer, chain_offset + 4);
              const char *name = cache->buffer + name_offset;
              const char *interned_name;
              guint32 hash_flags = 0;
              int len;

              /* Icons named foo.symbolic.png are stored in the cache as "foo.symbolic" with ICON_CACHE_FLAG_PNG,
               * but we convert it internally to ICON_CACHE_FLAG_SYMBOLIC_PNG.
               * Otherwise we use the same enum values and names as on disk. */

              len = strlen (name);
              if (g_str_equal (name + len - strlen (".symbolic"), ".symbolic") && (flags & ICON_CACHE_FLAG_PNG_SUFFIX) != 0)
                {
                  g_string_set_size (string, 0);
                  g_string_append_len (string, name, len - strlen (".symbolic"));
                  interned_name = gtk_string_set_add (set, string->str);
                  flags |= ICON_CACHE_FLAG_SYMBOLIC_PNG_SUFFIX;
                  flags &= ~ICON_CACHE_FLAG_PNG_SUFFIX;
                }
              else
                interned_name = gtk_string_set_add (set, name);

              if (!icons)
                icons = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);

              hash_flags = GPOINTER_TO_INT (g_hash_table_lookup (icons, interned_name));
              g_hash_table_replace (icons, (char *)interned_name, GUINT_TO_POINTER (hash_flags|flags));
            }

          chain_offset = GET_UINT32 (cache->buffer, chain_offset);
        }
    }

  g_string_free (string, TRUE);

  return icons;
}
