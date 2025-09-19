/*
 * Copyright © 2020 Endless OS Foundation, LLC
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
 *
 * Author: Philip Withnall <withnall@endlessm.com>
 */

/* This modelling file needs to be uploaded to GLib’s Coverity configuration at
 * https://scan.coverity.com/projects/glib?tab=analysis_settings
 * by someone with the appropriate permissions on Coverity. It should be kept in
 * sync with what’s there.
 *
 * Reference: https://scan.coverity.com/tune
 */

/* Disable some esoteric options which Coverity doesn't understand because they
 * delve into assembly. */
#define NVALGRIND 1
#undef HAVE_DTRACE

#define TRACE(probe)  /* no-op */

/* libc definitions */
#define NULL ((void*)0)

void *malloc (size_t);
void *calloc (size_t, size_t);
void *realloc (void *, size_t);
void free (void *);

/* Define some standard GLib types. */
typedef size_t gsize;
typedef char gchar;
typedef unsigned char guchar;
typedef int gint;
typedef unsigned long gulong;
typedef unsigned int guint32;
typedef void* gpointer;
typedef unsigned int gboolean;

typedef enum
{
  /* log flags */
  G_LOG_FLAG_RECURSION          = 1 << 0,
  G_LOG_FLAG_FATAL              = 1 << 1,

  /* GLib log levels */
  G_LOG_LEVEL_ERROR             = 1 << 2,       /* always fatal */
  G_LOG_LEVEL_CRITICAL          = 1 << 3,
  G_LOG_LEVEL_WARNING           = 1 << 4,
  G_LOG_LEVEL_MESSAGE           = 1 << 5,
  G_LOG_LEVEL_INFO              = 1 << 6,
  G_LOG_LEVEL_DEBUG             = 1 << 7,

  G_LOG_LEVEL_MASK              = ~(G_LOG_FLAG_RECURSION | G_LOG_FLAG_FATAL)
} GLogLevelFlags;

typedef struct _GList GList;

struct _GList
{
  gpointer data;
  GList *next;
  GList *prev;
};

typedef struct _GError GError;

struct _GError
{
  /* blah */
};

/* Dummied from sys/stat.h. */
struct stat {};
extern int stat (const char *path, struct stat *buf);

/* g_stat() can be used to check whether a given path is safe (i.e. exists).
 * This is not a full solution for sanitising user-provided paths, but goes a
 * long way, and is the best we can do without more context about how the path
 * is used. */
typedef struct stat GStatBuf;
#undef g_stat

int
g_stat (const char *filename, GStatBuf *buf)
{
  __coverity_tainted_string_sanitize_content__ (filename);
  return stat (filename, buf);
}

/* g_path_skip_root() can be used to validate that a @file_name is absolute. It
 * returns %NULL otherwise. */
const char *
g_path_skip_root (const char *file_name)
{
  int is_ok;
  if (is_ok)
    {
      __coverity_tainted_string_sanitize_content__ (file_name);
      return file_name;
    }
  else
    {
      return 0;  /* NULL */
    }
}

/* Tainted string sanitiser. */
int
g_action_name_is_valid (const char *action_name)
{
  int is_ok;
  if (is_ok)
    {
      __coverity_tainted_string_sanitize_content__ (action_name);
      return 1;  /* TRUE */
    }
  else
    {
      return 0;  /* FALSE */
    }
}

/* Treat this like an assert(0). */
void
g_return_if_fail_warning (const char *log_domain,
                          const char *pretty_function,
                          const char *expression)
{
  __coverity_panic__();
}

/* Treat this like an assert(0). */
void
g_log (const gchar   *log_domain,
       GLogLevelFlags log_level,
       const gchar   *format,
       ...)
{
  if (log_level & G_LOG_LEVEL_CRITICAL)
    __coverity_panic__ ();
}

#define g_critical(...) __coverity_panic__ ();

/* Treat it as a memory sink to hide one-time allocation leaks. */
void
(g_once_init_leave) (volatile void *location,
                     gsize          result)
{
  __coverity_escape__ (result);
}

/* Coverity cannot model allocation management for linked lists, so just pretend
 * that it's a pass-through. */
GList *
g_list_reverse (GList *list)
{
  return list;
}

/* g_ascii_isspace() routinely throws data_index taint errors, saying that
 * tainted data is being used to index g_ascii_table. This is true, but the
 * table has defined values for all possible 8-bit indexes. */
gboolean
g_ascii_isspace (gchar c)
{
  int is_space;
  __coverity_tainted_string_sink_content__ (c);
  if (is_space)
    return 1;
  else
    return 0;
}

/* Coverity treats byte-swapping operations as suspicious, and taints all data
 * which is byte-swapped (because it thinks it therefore probably comes from an
 * external source, which is reasonable). That is not the case for checksum
 * calculation, however.
 *
 * Since the definitions of these two functions depends on the host byte order,
 * just model them as no-ops. */
void
md5_byte_reverse (guchar *buffer,
                  gulong  length)
{
  /* No-op. */
}

void
sha_byte_reverse (guint32 *buffer,
                  gint     length)
{
  /* No-op. */
}

/* Parse error printing does not care about sanitising the input. */
gchar *
g_variant_parse_error_print_context (GError      *error,
                                     const gchar *source_str)
{
  __coverity_tainted_data_sink__ (source_str);
  return __coverity_alloc_nosize__ ();
}

/* Coverity somehow analyses G_LIKELY(x) as sometimes meaning !x, for example
 * when analysing g_try_realloc(). Ignore that. */
#define G_LIKELY(x) x
#define G_UNLIKELY(x) x

typedef struct {} DIR;
typedef struct _GDir GDir;

struct _GDir
{
  DIR *dirp;
};

/* This is a private function to libglib, and Coverity can’t peek inside it when
 * analysing code in (say) GIO. */
GDir *
g_dir_new_from_dirp (gpointer dirp)
{
  GDir *dir;

  if (dirp == 0)
    __coverity_panic__();

  dir = malloc (sizeof (GDir));
  dir->dirp = dirp;

  return dir;
}
