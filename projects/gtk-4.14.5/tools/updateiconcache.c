/* updateiconcache.c
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

#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>
#ifdef _MSC_VER
#include <io.h>
#include <sys/utime.h>
#else
#include <utime.h>
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <gdk-pixbuf/gdk-pixdata.h>
#include <glib/gi18n.h>
#include "gtkiconcachevalidatorprivate.h"

static gboolean force_update = FALSE;
static gboolean ignore_theme_index = FALSE;
static gboolean quiet = FALSE;
static gboolean index_only = TRUE;
static gboolean validate = FALSE;
static char *var_name = (char *) "-";

#define CACHE_NAME "icon-theme.cache"

#define HAS_SUFFIX_XPM (1 << 0)
#define HAS_SUFFIX_SVG (1 << 1)
#define HAS_SUFFIX_PNG (1 << 2)
#define HAS_ICON_FILE  (1 << 3)

#define MAJOR_VERSION 1
#define MINOR_VERSION 0
#define HASH_OFFSET 12

#define ALIGN_VALUE(this, boundary) \
  (( ((unsigned long)(this)) + (((unsigned long)(boundary)) -1)) & (~(((unsigned long)(boundary))-1)))

#ifdef HAVE_FTW_H

#include <ftw.h>

static GStatBuf cache_dir_stat;
static gboolean cache_up_to_date;

static int check_dir_mtime (const char        *dir,
                            const struct stat *sb,
                            int                tf)
{
  if (tf != FTW_NS && sb->st_mtime > cache_dir_stat.st_mtime)
    {
      cache_up_to_date = FALSE;
      /* stop tree walk */
      return 1;
    }

  return 0;
}

static gboolean
is_cache_up_to_date (const char *path)
{
  char *cache_path;
  int retval;

  cache_path = g_build_filename (path, CACHE_NAME, NULL);
  retval = g_stat (cache_path, &cache_dir_stat);
  g_free (cache_path);

  if (retval < 0)
    {
      /* Cache file not found */
      return FALSE;
    }

  cache_up_to_date = TRUE;

  ftw (path, check_dir_mtime, 20);

  return cache_up_to_date;
}

#else  /* !HAVE_FTW_H */

gboolean
is_cache_up_to_date (const char *path)
{
  GStatBuf path_stat, cache_stat;
  char *cache_path;
  int retval;

  retval = g_stat (path, &path_stat);

  if (retval < 0)
    {
      /* We can't stat the path,
       * assume we have a updated cache */
      return TRUE;
    }

  cache_path = g_build_filename (path, CACHE_NAME, NULL);
  retval = g_stat (cache_path, &cache_stat);
  g_free (cache_path);

  if (retval < 0)
    {
      /* Cache file not found */
      return FALSE;
    }

  /* Check mtime */
  return cache_stat.st_mtime >= path_stat.st_mtime;
}

#endif  /* !HAVE_FTW_H */

static gboolean
has_theme_index (const char *path)
{
  gboolean result;
  char *index_path;

  index_path = g_build_filename (path, "index.theme", NULL);

  result = g_file_test (index_path, G_FILE_TEST_IS_REGULAR);

  g_free (index_path);

  return result;
}


typedef struct
{
  GdkPixdata pixdata;
  gboolean has_pixdata;
  guint32 offset;
  guint size;
} ImageData;

typedef struct
{
  int has_embedded_rect;
  int x0, y0, x1, y1;

  int n_attach_points;
  int *attach_points;

  int n_display_names;
  char **display_names;

  guint32 offset;
  int size;
} IconData;

static GHashTable *image_data_hash = NULL;
static GHashTable *icon_data_hash = NULL;

typedef struct
{
  int flags;
  int dir_index;

  ImageData *image_data;
  guint pixel_data_size;

  IconData *icon_data;
  guint icon_data_size;
} Image;


static gboolean
foreach_remove_func (gpointer key, gpointer value, gpointer user_data)
{
  Image *image = (Image *)value;
  GHashTable *files = user_data;
  GList *list;
  gboolean free_key = FALSE;

  if (image->flags == HAS_ICON_FILE)
    {
      /* just a .icon file, throw away */
      g_free (key);
      g_free (image);

      return TRUE;
    }

  list = g_hash_table_lookup (files, key);
  if (list)
    free_key = TRUE;

  list = g_list_prepend (list, value);
  g_hash_table_insert (files, key, list);

  if (free_key)
    g_free (key);

  return TRUE;
}

static IconData *
load_icon_data (const char *path)
{
  GKeyFile *icon_file;
  char **split;
  gsize length;
  char *str;
  char *split_point;
  int i;
  int *ivalues;
  GError *error = NULL;
  char **keys;
  gsize n_keys;
  IconData *data;

  icon_file = g_key_file_new ();
  g_key_file_set_list_separator (icon_file, ',');
  g_key_file_load_from_file (icon_file, path, G_KEY_FILE_KEEP_TRANSLATIONS, &error);
  if (error)
    {
      g_error_free (error);
      g_key_file_free (icon_file);

      return NULL;
    }

  data = g_new0 (IconData, 1);

  ivalues = g_key_file_get_integer_list (icon_file,
					 "Icon Data", "EmbeddedTextRectangle",
					 &length, NULL);
  if (ivalues)
    {
      if (length == 4)
	{
	  data->has_embedded_rect = TRUE;
	  data->x0 = ivalues[0];
	  data->y0 = ivalues[1];
	  data->x1 = ivalues[2];
	  data->y1 = ivalues[3];
	}

      g_free (ivalues);
    }

  str = g_key_file_get_string (icon_file, "Icon Data", "AttachPoints", NULL);
  if (str)
    {
      split = g_strsplit (str, "|", -1);

      data->n_attach_points = g_strv_length (split);
      data->attach_points = g_new (int, 2 * data->n_attach_points);

      for (i = 0; i < data->n_attach_points; ++i)
	{
	  split_point = strchr (split[i], ',');
	  if (split_point)
	    {
	      *split_point = 0;
	      split_point++;
	      data->attach_points[2 * i] = atoi (split[i]);
	      data->attach_points[2 * i + 1] = atoi (split_point);
	    }
	}

      g_strfreev (split);
      g_free (str);
    }

  keys = g_key_file_get_keys (icon_file, "Icon Data", &n_keys, &error);
  data->display_names = g_new0 (char *, 2 * n_keys + 1);
  data->n_display_names = 0;

  for (i = 0; i < n_keys; i++)
    {
      char *lang, *name;

      if (g_str_has_prefix (keys[i], "DisplayName"))
	{
	  char *open, *close = NULL;

	  open = strchr (keys[i], '[');

	  if (open)
	    close = strchr (open, ']');

	  if (open && close)
	    {
	      lang = g_strndup (open + 1, close - open - 1);
	      name = g_key_file_get_locale_string (icon_file,
						   "Icon Data", "DisplayName",
						   lang, NULL);
	    }
	  else
	    {
	      lang = g_strdup ("C");
	      name = g_key_file_get_string (icon_file,
					    "Icon Data", "DisplayName",
					    NULL);
	    }

	  data->display_names[2 * data->n_display_names] = lang;
	  data->display_names[2 * data->n_display_names + 1] = name;
	  data->n_display_names++;
	}
    }

  g_strfreev (keys);

  g_key_file_free (icon_file);

  /* -1 means not computed yet, the real value depends
   * on string pool state, and will be computed
   * later
   */
  data->size = -1;

  return data;
}

/*
 * This function was copied from gtkfilesystemunix.c, it should
 * probably go to GLib
 */
static void
canonicalize_filename (char *filename)
{
  char *p, *q;
  gboolean last_was_slash = FALSE;

  p = filename;
  q = filename;

  while (*p)
    {
      if (*p == G_DIR_SEPARATOR)
	{
	  if (!last_was_slash)
	    *q++ = G_DIR_SEPARATOR;

	  last_was_slash = TRUE;
	}
      else
	{
	  if (last_was_slash && *p == '.')
	    {
	      if (*(p + 1) == G_DIR_SEPARATOR ||
		  *(p + 1) == '\0')
		{
		  if (*(p + 1) == '\0')
		    break;

		  p += 1;
		}
	      else if (*(p + 1) == '.' &&
		       (*(p + 2) == G_DIR_SEPARATOR ||
			*(p + 2) == '\0'))
		{
		  if (q > filename + 1)
		    {
		      q--;
		      while (q > filename + 1 &&
			     *(q - 1) != G_DIR_SEPARATOR)
			q--;
		    }

		  if (*(p + 2) == '\0')
		    break;

		  p += 2;
		}
	      else
		{
		  *q++ = *p;
		  last_was_slash = FALSE;
		}
	    }
	  else
	    {
	      *q++ = *p;
	      last_was_slash = FALSE;
	    }
	}

      p++;
    }

  if (q > filename + 1 && *(q - 1) == G_DIR_SEPARATOR)
    q--;

  *q = '\0';
}

static char *
follow_links (const char *path)
{
  char *target;
  char *d, *s;
  char *path2 = NULL;

  path2 = g_strdup (path);
  while (g_file_test (path2, G_FILE_TEST_IS_SYMLINK))
    {
      target = g_file_read_link (path2, NULL);

      if (target)
	{
	  if (g_path_is_absolute (target))
	    path2 = target;
	  else
	    {
	      d = g_path_get_dirname (path2);
	      s = g_build_filename (d, target, NULL);
	      g_free (d);
	      g_free (target);
	      g_free (path2);
	      path2 = s;
	    }
	}
      else
	break;
    }

  if (strcmp (path, path2) == 0)
    {
      g_free (path2);
      path2 = NULL;
    }

  return path2;
}

static void
maybe_cache_image_data (Image       *image,
			const char *path)
{
  if (!index_only && !image->image_data &&
      (g_str_has_suffix (path, ".png") || g_str_has_suffix (path, ".xpm")))
    {
      GdkPixbuf *pixbuf;
      ImageData *idata;
      char *path2;

      idata = g_hash_table_lookup (image_data_hash, path);
      path2 = follow_links (path);

      if (path2)
	{
	  ImageData *idata2;

	  canonicalize_filename (path2);

	  idata2 = g_hash_table_lookup (image_data_hash, path2);

	  if (idata && idata2 && idata != idata2)
	    g_error ("different idatas found for symlinked '%s' and '%s'\n",
		     path, path2);

	  if (idata && !idata2)
	    g_hash_table_insert (image_data_hash, g_strdup (path2), idata);

	  if (!idata && idata2)
	    {
	      g_hash_table_insert (image_data_hash, g_strdup (path), idata2);
	      idata = idata2;
	    }
	}

      if (!idata)
	{
	  idata = g_new0 (ImageData, 1);
	  g_hash_table_insert (image_data_hash, g_strdup (path), idata);
	  if (path2)
	    g_hash_table_insert (image_data_hash, g_strdup (path2), idata);
	}

      if (!idata->has_pixdata)
	{
	  pixbuf = gdk_pixbuf_new_from_file (path, NULL);

	  if (pixbuf)
	    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
	      gdk_pixdata_from_pixbuf (&idata->pixdata, pixbuf, FALSE);
G_GNUC_END_IGNORE_DEPRECATIONS;
	      idata->size = idata->pixdata.length + 8;
	      idata->has_pixdata = TRUE;
	    }
	}

      image->image_data = idata;

      g_free (path2);
    }
}

static void
maybe_cache_icon_data (Image       *image,
                       const char *path)
{
  if (g_str_has_suffix (path, ".icon"))
    {
      IconData *idata = NULL;
      char *path2 = NULL;

      idata = g_hash_table_lookup (icon_data_hash, path);
      path2 = follow_links (path);

      if (path2)
	{
	  IconData *idata2;

	  canonicalize_filename (path2);

	  idata2 = g_hash_table_lookup (icon_data_hash, path2);

	  if (idata && idata2 && idata != idata2)
	    g_error ("different idatas found for symlinked '%s' and '%s'\n",
		     path, path2);

	  if (idata && !idata2)
	    g_hash_table_insert (icon_data_hash, g_strdup (path2), idata);

	  if (!idata && idata2)
	    {
	      g_hash_table_insert (icon_data_hash, g_strdup (path), idata2);
	      idata = idata2;
	    }
	}

      if (!idata)
	{
	  idata = load_icon_data (path);
	  g_hash_table_insert (icon_data_hash, g_strdup (path), idata);
	  if (path2)
	    g_hash_table_insert (icon_data_hash, g_strdup (path2), idata);
        }

      image->icon_data = idata;

      g_free (path2);
    }
}

/*
 * Finds all dir separators and replaces them with “/”.
 * This makes sure that only /-separated paths are written in cache files,
 * maintaining compatibility with theme index files that use slashes as
 * directory separators on all platforms.
 */
static void
replace_backslashes_with_slashes (char *path)
{
  size_t i;
  if (path == NULL)
    return;
  for (i = 0; path[i]; i++)
    if (G_IS_DIR_SEPARATOR (path[i]))
      path[i] = '/';
}

static GList *
scan_directory (const char *base_path,
		const char *subdir,
		GHashTable  *files,
		GList       *directories,
		int          depth)
{
  GHashTable *dir_hash;
  GDir *dir;
  GList *list = NULL, *iterator = NULL;
  const char *name;
  char *dir_path;
  gboolean dir_added = FALSE;
  guint dir_index = 0xffff;

  dir_path = g_build_path ("/", base_path, subdir, NULL);

  /* FIXME: Use the gerror */
  dir = g_dir_open (dir_path, 0, NULL);

  if (!dir)
    return directories;

  dir_hash = g_hash_table_new (g_str_hash, g_str_equal);

  while ((name = g_dir_read_name (dir)))
    {
      list = g_list_prepend (list, g_strdup (name));
    }
  list = g_list_sort (list, (GCompareFunc) strcmp);
  for (iterator = list; iterator; iterator = iterator->next)
    {
      name = iterator->data;

      char *path;
      gboolean retval;
      int flags = 0;
      Image *image;
      char *basename, *dot;

      path = g_build_filename (dir_path, name, NULL);

      retval = g_file_test (path, G_FILE_TEST_IS_DIR);
      if (retval)
	{
	  char *subsubdir;

	  if (subdir)
	    subsubdir = g_build_path ("/", subdir, name, NULL);
	  else
	    subsubdir = g_strdup (name);
	  directories = scan_directory (base_path, subsubdir, files,
					directories, depth + 1);
	  g_free (subsubdir);

	  continue;
	}

      /* ignore images in the toplevel directory */
      if (subdir == NULL)
        continue;

      retval = g_file_test (path, G_FILE_TEST_IS_REGULAR);
      if (retval)
	{
	  if (g_str_has_suffix (name, ".png"))
	    flags |= HAS_SUFFIX_PNG;
	  else if (g_str_has_suffix (name, ".svg"))
	    flags |= HAS_SUFFIX_SVG;
	  else if (g_str_has_suffix (name, ".xpm"))
	    flags |= HAS_SUFFIX_XPM;
	  else if (g_str_has_suffix (name, ".icon"))
	    flags |= HAS_ICON_FILE;

	  if (flags == 0)
	    continue;

	  basename = g_strdup (name);
	  dot = strrchr (basename, '.');
	  *dot = '\0';

	  image = g_hash_table_lookup (dir_hash, basename);
	  if (!image)
	    {
	      if (!dir_added)
		{
		  dir_added = TRUE;
		  if (subdir)
		    {
		      dir_index = g_list_length (directories);
		      directories = g_list_append (directories, g_strdup (subdir));
		    }
		  else
		    dir_index = 0xffff;
		}

	      image = g_new0 (Image, 1);
	      image->dir_index = dir_index;
	      g_hash_table_insert (dir_hash, g_strdup (basename), image);
	    }

	  image->flags |= flags;

	  maybe_cache_image_data (image, path);
          maybe_cache_icon_data (image, path);

	  g_free (basename);
	}

      g_free (path);
    }

  g_list_free_full (list, g_free);
  g_dir_close (dir);

  /* Move dir into the big file hash */
  g_hash_table_foreach_remove (dir_hash, foreach_remove_func, files);

  g_hash_table_destroy (dir_hash);

  return directories;
}

typedef struct _HashNode HashNode;

struct _HashNode
{
  HashNode *next;
  char *name;
  GList *image_list;
  int offset;
};

static guint
icon_name_hash (gconstpointer key)
{
  const signed char *p = key;
  guint32 h = *p;

  if (h)
    for (p += 1; *p != '\0'; p++)
      h = (h << 5) - h + *p;

  return h;
}

typedef struct {
  int size;
  HashNode **nodes;
} HashContext;

static gboolean
convert_to_hash (gpointer key, gpointer value, gpointer user_data)
{
  HashContext *context = user_data;
  guint hash;
  HashNode *node;

  hash = icon_name_hash (key) % context->size;

  node = g_new0 (HashNode, 1);
  node->next = NULL;
  node->name = key;
  node->image_list = value;

  if (context->nodes[hash] != NULL)
    node->next = context->nodes[hash];

  context->nodes[hash] = node;

  return TRUE;
}

static GHashTable *string_pool = NULL;

static int
find_string (const char *n)
{
  return GPOINTER_TO_INT (g_hash_table_lookup (string_pool, n));
}

static void
add_string (const char *n, int offset)
{
  g_hash_table_insert (string_pool, (gpointer) n, GINT_TO_POINTER (offset));
}

static gboolean
write_string (FILE *cache, const char *n)
{
  char *s;
  int i, l;

  l = ALIGN_VALUE (strlen (n) + 1, 4);

  s = g_malloc0 (l);
  strcpy (s, n);

  i = fwrite (s, l, 1, cache);

  g_free (s);

  return i == 1;

}

static gboolean
write_card16 (FILE *cache, guint16 n)
{
  int i;

  n = GUINT16_TO_BE (n);

  i = fwrite ((char *)&n, 2, 1, cache);

  return i == 1;
}

static gboolean
write_card32 (FILE *cache, guint32 n)
{
  int i;

  n = GUINT32_TO_BE (n);

  i = fwrite ((char *)&n, 4, 1, cache);

  return i == 1;
}


static gboolean
write_image_data (FILE *cache, ImageData *image_data, int offset)
{
  guint8 *s;
  guint len;
  int i;
  GdkPixdata *pixdata = &image_data->pixdata;

  /* Type 0 is GdkPixdata */
  if (!write_card32 (cache, 0))
    return FALSE;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  s = gdk_pixdata_serialize (pixdata, &len);
G_GNUC_END_IGNORE_DEPRECATIONS;

  if (!write_card32 (cache, len))
    {
      g_free (s);
      return FALSE;
    }

  i = fwrite (s, len, 1, cache);

  g_free (s);

  return i == 1;
}

static gboolean
write_icon_data (FILE *cache, IconData *icon_data, int offset)
{
  int ofs = offset + 12;
  int j;
  int tmp, tmp2;

  if (icon_data->has_embedded_rect)
    {
      if (!write_card32 (cache, ofs))
        return FALSE;

       ofs += 8;
    }
  else
    {
      if (!write_card32 (cache, 0))
        return FALSE;
    }

  if (icon_data->n_attach_points > 0)
    {
      if (!write_card32 (cache, ofs))
        return FALSE;

      ofs += 4 + 4 * icon_data->n_attach_points;
    }
  else
    {
      if (!write_card32 (cache, 0))
        return FALSE;
    }

  if (icon_data->n_display_names > 0)
    {
      if (!write_card32 (cache, ofs))
	return FALSE;
    }
  else
    {
      if (!write_card32 (cache, 0))
        return FALSE;
    }

  if (icon_data->has_embedded_rect)
    {
      if (!write_card16 (cache, icon_data->x0) ||
          !write_card16 (cache, icon_data->y0) ||
	  !write_card16 (cache, icon_data->x1) ||
	  !write_card16 (cache, icon_data->y1))
        return FALSE;
    }

  if (icon_data->n_attach_points > 0)
    {
      if (!write_card32 (cache, icon_data->n_attach_points))
        return FALSE;

      for (j = 0; j < 2 * icon_data->n_attach_points; j++)
        {
          if (!write_card16 (cache, icon_data->attach_points[j]))
            return FALSE;
        }
    }

  if (icon_data->n_display_names > 0)
    {
      if (!write_card32 (cache, icon_data->n_display_names))
        return FALSE;

      ofs += 4 + 8 * icon_data->n_display_names;

      tmp = ofs;
      for (j = 0; j < 2 * icon_data->n_display_names; j++)
        {
          tmp2 = find_string (icon_data->display_names[j]);
          if (tmp2 == 0 || tmp2 == -1)
            {
              tmp2 = tmp;
              tmp += ALIGN_VALUE (strlen (icon_data->display_names[j]) + 1, 4);
              /* We're playing a little game with negative
               * offsets here to handle duplicate strings in
               * the array.
               */
              add_string (icon_data->display_names[j], -tmp2);
            }
          else if (tmp2 < 0)
            {
              tmp2 = -tmp2;
            }

          if (!write_card32 (cache, tmp2))
            return FALSE;

        }

      g_assert (ofs == ftell (cache));
      for (j = 0; j < 2 * icon_data->n_display_names; j++)
        {
          tmp2 = find_string (icon_data->display_names[j]);
          g_assert (tmp2 != 0 && tmp2 != -1);
          if (tmp2 < 0)
            {
              tmp2 = -tmp2;
              g_assert (tmp2 == ftell (cache));
              add_string (icon_data->display_names[j], tmp2);
              if (!write_string (cache, icon_data->display_names[j]))
                return FALSE;
            }
        }
    }

  return TRUE;
}

static gboolean
write_header (FILE *cache, guint32 dir_list_offset)
{
  return (write_card16 (cache, MAJOR_VERSION) &&
	  write_card16 (cache, MINOR_VERSION) &&
	  write_card32 (cache, HASH_OFFSET) &&
	  write_card32 (cache, dir_list_offset));
}

static int
get_image_meta_data_size (Image *image)
{
  int i;

  /* The complication with storing the size in both
   * IconData and Image is necessary since we attribute
   * the size of the IconData only to the first Image
   * using it (at which time it is written out in the
   * cache). Later Images just refer to the written out
   * IconData via the offset.
   */
  if (image->icon_data_size == 0)
    {
      if (image->icon_data && image->icon_data->size < 0)
	{
          IconData *data = image->icon_data;

          data->size = 0;

          if (data->has_embedded_rect ||
              data->n_attach_points > 0 ||
              data->n_display_names > 0)
            data->size += 12;

          if (data->has_embedded_rect)
            data->size += 8;

          if (data->n_attach_points > 0)
            data->size += 4 + data->n_attach_points * 4;

          if (data->n_display_names > 0)
            {
              data->size += 4 + 8 * data->n_display_names;

              for (i = 0; data->display_names[i]; i++)
                {
                  if (find_string (data->display_names[i]) == 0)
                    {
                      data->size += ALIGN_VALUE (strlen (data->display_names[i]) + 1, 4);
                      /* Adding the string to the pool with -1
                       * to indicate that it hasn't been written out
                       * to the cache yet. We still need it in the
                       * pool in case the same string occurs twice
                       * during a get_single_node_size() calculation.
                       */
                      add_string (data->display_names[i], -1);
                    }
                }
           }

	  image->icon_data_size = data->size;
	  data->size = 0;
	}
    }

  g_assert (image->icon_data_size % 4 == 0);

  return image->icon_data_size;
}

static int
get_image_pixel_data_size (Image *image)
{
  /* The complication with storing the size in both
   * ImageData and Image is necessary since we attribute
   * the size of the ImageData only to the first Image
   * using it (at which time it is written out in the
   * cache). Later Images just refer to the written out
   * ImageData via the offset.
   */
  if (image->pixel_data_size == 0)
    {
      if (image->image_data &&
	  image->image_data->has_pixdata)
	{
	  image->pixel_data_size = image->image_data->size;
	  image->image_data->size = 0;
	}
    }

  g_assert (image->pixel_data_size % 4 == 0);

  return image->pixel_data_size;
}

static int
get_image_data_size (Image *image)
{
  int len;

  len = 0;

  len += get_image_pixel_data_size (image);
  len += get_image_meta_data_size (image);

  /* Even if len is zero, we need to reserve space to
   * write the ImageData, unless this is an .svg without
   * .icon, in which case both image_data and icon_data
   * are NULL.
   */
  if (len > 0 || image->image_data || image->icon_data)
    len += 8;

  return len;
}

static void
get_single_node_size (HashNode *node, int *node_size, int *image_data_size)
{
  GList *list;

  /* Node pointers */
  *node_size = 12;

  /* Name */
  if (find_string (node->name) == 0)
    {
      *node_size += ALIGN_VALUE (strlen (node->name) + 1, 4);
      add_string (node->name, -1);
    }

  /* Image list */
  *node_size += 4 + g_list_length (node->image_list) * 8;

  /* Image data */
  *image_data_size = 0;
  for (list = node->image_list; list; list = list->next)
    {
      Image *image = list->data;

      *image_data_size += get_image_data_size (image);
    }
}

static gboolean
write_bucket (FILE *cache, HashNode *node, int *offset)
{
  while (node != NULL)
    {
      int node_size, image_data_size;
      int next_offset, image_data_offset;
      int data_offset;
      int name_offset;
      int name_size;
      int image_list_offset;
      int i, len;
      GList *list;

      g_assert (*offset == ftell (cache));

      node->offset = *offset;

      get_single_node_size (node, &node_size, &image_data_size);
      g_assert (node_size % 4 == 0);
      g_assert (image_data_size % 4 == 0);
      image_data_offset = *offset + node_size;
      next_offset = *offset + node_size + image_data_size;
      /* Chain offset */
      if (node->next != NULL)
        {
          if (!write_card32 (cache, next_offset))
            return FALSE;
        }
      else
        {
          if (!write_card32 (cache, 0xffffffff))
            return FALSE;
        }

      name_size = 0;
      name_offset = find_string (node->name);
      if (name_offset <= 0)
        {
          name_offset = *offset + 12;
          name_size = ALIGN_VALUE (strlen (node->name) + 1, 4);
          add_string (node->name, name_offset);
        }
      if (!write_card32 (cache, name_offset))
        return FALSE;

      image_list_offset = *offset + 12 + name_size;
      if (!write_card32 (cache, image_list_offset))
        return FALSE;

      /* Icon name */
      if (name_size > 0)
        {
          if (!write_string (cache, node->name))
            return FALSE;
        }

      /* Image list */
      len = g_list_length (node->image_list);
      if (!write_card32 (cache, len))
        return FALSE;

      list = node->image_list;
      data_offset = image_data_offset;
      for (i = 0; i < len; i++)
        {
          Image *image = list->data;
          int image_size = get_image_data_size (image);

          /* Directory index */
          if (!write_card16 (cache, image->dir_index))
            return FALSE;

          /* Flags */
          if (!write_card16 (cache, image->flags))
            return FALSE;

          /* Image data offset */
          if (image_size > 0)
            {
              if (!write_card32 (cache, data_offset))
                return FALSE;
              data_offset += image_size;
            }
          else
            {
              if (!write_card32 (cache, 0))
                return FALSE;
            }

          list = list->next;
        }

      /* Now write the image data */
      list = node->image_list;
      for (i = 0; i < len; i++, list = list->next)
        {
          Image *image = list->data;
          int pixel_data_size = get_image_pixel_data_size (image);
          int meta_data_size = get_image_meta_data_size (image);

          if (get_image_data_size (image) == 0)
            continue;

          /* Pixel data */
          if (pixel_data_size > 0)
            {
              image->image_data->offset = image_data_offset + 8;
              if (!write_card32 (cache, image->image_data->offset))
                return FALSE;
            }
          else
            {
              if (!write_card32 (cache, (guint32) (image->image_data ? image->image_data->offset : 0)))
                return FALSE;
            }

          if (meta_data_size > 0)
            {
              image->icon_data->offset = image_data_offset + pixel_data_size + 8;
              if (!write_card32 (cache, image->icon_data->offset))
                return FALSE;
            }
          else
            {
              if (!write_card32 (cache, image->icon_data ? image->icon_data->offset : 0))
                return FALSE;
            }

          if (pixel_data_size > 0)
            {
              if (!write_image_data (cache, image->image_data, image->image_data->offset))
                return FALSE;
            }

          if (meta_data_size > 0)
            {
              if (!write_icon_data (cache, image->icon_data, image->icon_data->offset))
                return FALSE;
            }

          image_data_offset += pixel_data_size + meta_data_size + 8;
        }

      *offset = next_offset;
      node = node->next;
    }

  return TRUE;
}

static gboolean
write_hash_table (FILE *cache, HashContext *context, int *new_offset)
{
  int offset = HASH_OFFSET;
  int node_offset;
  int i;

  if (!(write_card32 (cache, context->size)))
    return FALSE;

  offset += 4;
  node_offset = offset + context->size * 4;
  /* Just write zeros here, we will rewrite this later */
  for (i = 0; i < context->size; i++)
    {
      if (!write_card32 (cache, 0))
	return FALSE;
    }

  /* Now write the buckets */
  for (i = 0; i < context->size; i++)
    {
      if (!context->nodes[i])
	continue;

      g_assert (node_offset % 4 == 0);
      if (!write_bucket (cache, context->nodes[i], &node_offset))
	return FALSE;
    }

  *new_offset = node_offset;

  /* Now write out the bucket offsets */

  fseek (cache, offset, SEEK_SET);

  for (i = 0; i < context->size; i++)
    {
      if (context->nodes[i] != NULL)
        node_offset = context->nodes[i]->offset;
      else
	node_offset = 0xffffffff;
      if (!write_card32 (cache, node_offset))
        return FALSE;
    }

  fseek (cache, 0, SEEK_END);

  return TRUE;
}

static gboolean
write_dir_index (FILE *cache, int offset, GList *directories)
{
  int n_dirs;
  GList *d;
  char *dir;
  int tmp, tmp2;

  n_dirs = g_list_length (directories);

  if (!write_card32 (cache, n_dirs))
    return FALSE;

  offset += 4 + n_dirs * 4;

  tmp = offset;
  for (d = directories; d; d = d->next)
    {
      dir = d->data;

      tmp2 = find_string (dir);

      if (tmp2 == 0 || tmp2 == -1)
        {
          tmp2 = tmp;
          tmp += ALIGN_VALUE (strlen (dir) + 1, 4);
          /* We're playing a little game with negative
           * offsets here to handle duplicate strings in
           * the array, even though that should not
           * really happen for the directory index.
           */
          add_string (dir, -tmp2);
        }
      else if (tmp2 < 0)
        {
          tmp2 = -tmp2;
        }

      if (!write_card32 (cache, tmp2))
	return FALSE;
    }

  g_assert (offset == ftell (cache));
  for (d = directories; d; d = d->next)
    {
      dir = d->data;

      tmp2 = find_string (dir);
      g_assert (tmp2 != 0 && tmp2 != -1);
      if (tmp2 < 0)
        {
          tmp2 = -tmp2;
          g_assert (tmp2 == ftell (cache));
          add_string (dir, tmp2);
          if (!write_string (cache, dir))
	    return FALSE;
        }
    }

  return TRUE;
}

static gboolean
write_file (FILE *cache, GHashTable *files, GList *directories)
{
  HashContext context;
  int new_offset;

  /* Convert the hash table into something looking a bit more
   * like what we want to write to disk.
   */
  context.size = g_spaced_primes_closest (g_hash_table_size (files) / 3);
  context.nodes = g_new0 (HashNode *, context.size);

  g_hash_table_foreach_remove (files, convert_to_hash, &context);

  /* Now write the file */
  /* We write 0 as the directory list offset and go
   * back and change it later */
  if (!write_header (cache, 0))
    {
      g_printerr (_("Failed to write header\n"));
      return FALSE;
    }

  if (!write_hash_table (cache, &context, &new_offset))
    {
      g_printerr (_("Failed to write hash table\n"));
      return FALSE;
    }

  if (!write_dir_index (cache, new_offset, directories))
    {
      g_printerr (_("Failed to write folder index\n"));
      return FALSE;
    }

  rewind (cache);

  if (!write_header (cache, new_offset))
    {
      g_printerr (_("Failed to rewrite header\n"));
      return FALSE;
    }

  return TRUE;
}

static gboolean
validate_file (const char *file)
{
  GMappedFile *map;
  CacheInfo info;

  map = g_mapped_file_new (file, FALSE, NULL);
  if (!map)
    return FALSE;

  info.cache = g_mapped_file_get_contents (map);
  info.cache_size = g_mapped_file_get_length (map);
  info.n_directories = 0;
  info.flags = CHECK_OFFSETS|CHECK_STRINGS|CHECK_PIXBUFS;

  if (!gtk_icon_cache_validate (&info))
    {
      g_mapped_file_unref (map);
      return FALSE;
    }

  g_mapped_file_unref (map);

  return TRUE;
}

/**
 * safe_fclose:
 * @f: A FILE* stream, must have underlying fd
 *
 * Unix defaults for data preservation after system crash
 * are unspecified, and many systems will eat your data
 * in this situation unless you explicitly fsync().
 *
 * Returns: %TRUE on success, %FALSE on failure, and will set errno()
 */
static gboolean
safe_fclose (FILE *f)
{
  int fd = fileno (f);
  g_assert (fd >= 0);
  if (fflush (f) == EOF)
    return FALSE;
#ifndef G_OS_WIN32
  if (fsync (fd) < 0)
    return FALSE;
#endif
  if (fclose (f) == EOF)
    return FALSE;
  return TRUE;
}

static void
build_cache (const char *path)
{
  char *cache_path, *tmp_cache_path;
#ifdef G_OS_WIN32
  char *bak_cache_path = NULL;
#endif
  GHashTable *files;
  FILE *cache;
  GStatBuf path_stat, cache_stat;
  struct utimbuf utime_buf;
  GList *directories = NULL;
  int fd;
  int retry_count = 0;
#ifndef G_OS_WIN32
  mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
#else
  int mode = _S_IWRITE | _S_IREAD;
#endif
#ifndef _O_BINARY
#define _O_BINARY 0
#endif

  tmp_cache_path = g_build_filename (path, "."CACHE_NAME, NULL);
  cache_path = g_build_filename (path, CACHE_NAME, NULL);

opentmp:
  if ((fd = g_open (tmp_cache_path, O_WRONLY | O_CREAT | O_EXCL | O_TRUNC | _O_BINARY, mode)) == -1)
    {
      if (retry_count == 0)
        {
          retry_count++;
          g_remove (tmp_cache_path);
          goto opentmp;
        }
      g_printerr (_("Failed to open file %s : %s\n"), tmp_cache_path, g_strerror (errno));
      exit (1);
    }

  cache = fdopen (fd, "wb");

  if (!cache)
    {
      g_printerr (_("Failed to write cache file: %s\n"), g_strerror (errno));
      exit (1);
    }

  files = g_hash_table_new (g_str_hash, g_str_equal);
  image_data_hash = g_hash_table_new (g_str_hash, g_str_equal);
  icon_data_hash = g_hash_table_new (g_str_hash, g_str_equal);
  string_pool = g_hash_table_new (g_str_hash, g_str_equal);

  directories = scan_directory (path, NULL, files, NULL, 0);

  if (g_hash_table_size (files) == 0)
    {
      /* Empty table, just close and remove the file */

      fclose (cache);
      g_unlink (tmp_cache_path);
      g_unlink (cache_path);
      exit (0);
    }

  /* FIXME: Handle failure */
  if (!write_file (cache, files, directories))
    {
      g_unlink (tmp_cache_path);
      exit (1);
    }

  if (!safe_fclose (cache))
    {
      g_printerr (_("Failed to write cache file: %s\n"), g_strerror (errno));
      g_unlink (tmp_cache_path);
      exit (1);
    }
  cache = NULL;

  g_list_free_full (directories, g_free);

  if (!validate_file (tmp_cache_path))
    {
      g_printerr (_("The generated cache was invalid.\n"));
      /*g_unlink (tmp_cache_path);*/
      exit (1);
    }

#ifdef G_OS_WIN32
  if (g_file_test (cache_path, G_FILE_TEST_EXISTS))
    {
      bak_cache_path = g_strconcat (cache_path, ".bak", NULL);
      g_unlink (bak_cache_path);
      if (g_rename (cache_path, bak_cache_path) == -1)
	{
          int errsv = errno;

	  g_printerr (_("Could not rename %s to %s: %s, removing %s then.\n"),
		      cache_path, bak_cache_path,
		      g_strerror (errsv),
		      cache_path);
	  g_unlink (cache_path);
	  bak_cache_path = NULL;
	}
    }
#endif

  if (g_rename (tmp_cache_path, cache_path) == -1)
    {
      int errsv = errno;

      g_printerr (_("Could not rename %s to %s: %s\n"),
		  tmp_cache_path, cache_path,
		  g_strerror (errsv));
      g_unlink (tmp_cache_path);
#ifdef G_OS_WIN32
      if (bak_cache_path != NULL)
	if (g_rename (bak_cache_path, cache_path) == -1)
          {
            errsv = errno;

            g_printerr (_("Could not rename %s back to %s: %s.\n"),
                        bak_cache_path, cache_path,
                        g_strerror (errsv));
          }
#endif
      exit (1);
    }
#ifdef G_OS_WIN32
  if (bak_cache_path != NULL)
    g_unlink (bak_cache_path);
#endif

  /* Update time */
  /* FIXME: What do do if an error occurs here? */
  if (g_stat (path, &path_stat) < 0 ||
      g_stat (cache_path, &cache_stat))
    exit (1);

  utime_buf.actime = path_stat.st_atime;
  utime_buf.modtime = cache_stat.st_mtime;
  g_utime (path, &utime_buf);

  if (!quiet)
    g_printerr (_("Cache file created successfully.\n"));
}

static void
write_csource (const char *path)
{
  char *cache_path;
  char *data;
  gsize len;
  int i;

  cache_path = g_build_filename (path, CACHE_NAME, NULL);
  if (!g_file_get_contents (cache_path, &data, &len, NULL))
    exit (1);

  g_printf ("#ifdef __SUNPRO_C\n");
  g_printf ("#pragma align 4 (%s)\n", var_name);
  g_printf ("#endif\n");

  g_printf ("#ifdef __GNUC__\n");
  g_printf ("static const guint8 %s[] __attribute__ ((__aligned__ (4))) = \n", var_name);
  g_printf ("#else\n");
  g_printf ("static const guint8 %s[] = \n", var_name);
  g_printf ("#endif\n");

  g_printf ("{\n");
  for (i = 0; i < len - 1; i++)
    {
      if (i %12 == 0)
	g_printf ("  ");
      g_printf ("0x%02x, ", (guint8)data[i]);
      if (i % 12 == 11)
        g_printf ("\n");
    }

  g_printf ("0x%02x\n};\n", (guint8)data[i]);
}

static GOptionEntry args[] = {
  { "force", 'f', 0, G_OPTION_ARG_NONE, &force_update, N_("Overwrite an existing cache, even if up to date"), NULL },
  { "ignore-theme-index", 't', 0, G_OPTION_ARG_NONE, &ignore_theme_index, N_("Don’t check for the existence of index.theme"), NULL },
  { "index-only", 'i', 0, G_OPTION_ARG_NONE, &index_only, N_("Don’t include image data in the cache"), NULL },
  { "include-image-data", 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &index_only, N_("Include image data in the cache"), NULL },
  { "source", 'c', 0, G_OPTION_ARG_STRING, &var_name, N_("Output a C header file"), "NAME" },
  { "quiet", 'q', 0, G_OPTION_ARG_NONE, &quiet, N_("Turn off verbose output"), NULL },
  { "validate", 'v', 0, G_OPTION_ARG_NONE, &validate, N_("Validate existing icon cache"), NULL },
  { NULL }
};

static void
printerr_handler (const char *string)
{
  const char *charset;

  fputs (g_get_prgname (), stderr);
  fputs (": ", stderr);
  if (g_get_charset (&charset))
    fputs (string, stderr); /* charset is UTF-8 already */
  else
    {
      char *result;

      result = g_convert_with_fallback (string, -1, charset, "UTF-8", "?", NULL, NULL, NULL);

      if (result)
        {
          fputs (result, stderr);
          g_free (result);
        }

      fflush (stderr);
    }
}


int
main (int argc, char **argv)
{
  char *path;
  GOptionContext *context;

  if (argc < 2)
    return 0;

  g_set_printerr_handler (printerr_handler);

  setlocale (LC_ALL, "");

  bindtextdomain (GETTEXT_PACKAGE, GTK_LOCALEDIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

  context = g_option_context_new ("ICONPATH");
  g_option_context_add_main_entries (context, args, GETTEXT_PACKAGE);

  g_option_context_parse (context, &argc, &argv, NULL);

  path = argv[1];
#ifdef G_OS_WIN32
  path = g_locale_to_utf8 (path, -1, NULL, NULL, NULL);
#endif

  if (validate)
    {
       char *file = g_build_filename (path, CACHE_NAME, NULL);

       if (!g_file_test (file, G_FILE_TEST_IS_REGULAR))
         {
            if (!quiet)
              g_printerr (_("File not found: %s\n"), file);
            exit (1);
         }
       if (!validate_file (file))
         {
           if (!quiet)
             g_printerr (_("Not a valid icon cache: %s\n"), file);
           exit (1);
         }
       else
         {
           exit (0);
         }
    }

  if (!ignore_theme_index && !has_theme_index (path))
    {
      if (path)
	{
	  g_printerr (_("No theme index file.\n"));
	}
      else
	{
	  g_printerr (_("No theme index file in “%s”.\n"
		    "If you really want to create an icon cache here, use --ignore-theme-index.\n"), path);
	}

      return 1;
    }

  if (!force_update && is_cache_up_to_date (path))
    return 0;

  replace_backslashes_with_slashes (path);
  build_cache (path);

  if (strcmp (var_name, "-") != 0)
    write_csource (path);

  return 0;
}
