/* savedir.c -- save the list of files in a directory in a string

   Copyright (C) 1990, 1997-2001, 2003-2006, 2009-2024 Free Software
   Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by David MacKenzie <djm@gnu.ai.mit.edu>. */

#include <config.h>

#include "savedir.h"

#include <sys/types.h>

#include <errno.h>

#include "dirent--.h"
#ifndef _D_EXACT_NAMLEN
# define _D_EXACT_NAMLEN(dp)    strlen ((dp)->d_name)
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "attribute.h"
#include "xalloc.h"

/* Pacify GCC bug <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=113963>.  */
#if 13 <= __GNUC__
# pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
# pragma GCC diagnostic ignored "-Wanalyzer-null-dereference"
#endif

typedef struct
{
  /* Offset of file name in name_space.  */
  idx_t name;

#if D_INO_IN_DIRENT
  /* File inode number.  */
  ino_t ino;
#endif
} direntry_t;

/* Compare the names of two directory entries */

static int
direntry_cmp_name (void const *a, void const *b, void *arg)
{
  direntry_t const *dea = a;
  direntry_t const *deb = b;
  char const *name_space = arg;

  return strcmp (name_space + dea->name, name_space + deb->name);
}

#if D_INO_IN_DIRENT
/* Compare the inode numbers of two directory entries */

static int
direntry_cmp_inode (void const *a, void const *b, MAYBE_UNUSED void *arg)
{
  direntry_t const *dea = a;
  direntry_t const *deb = b;

  return _GL_CMP (dea->ino, deb->ino);
}
#endif

typedef int (*comparison_function) (void const *, void const *, void *);

static comparison_function const comparison_function_table[] =
  {
    0,
    direntry_cmp_name
#if D_INO_IN_DIRENT
    , direntry_cmp_inode
#endif
  };

/* Return a freshly allocated string containing the file names
   in directory DIRP, separated by '\0' characters;
   the end is marked by two '\0' characters in a row.
   Returned values are sorted according to OPTION.
   Return NULL (setting errno) if DIRP cannot be read.
   If DIRP is NULL, return NULL without affecting errno.  */

char *
streamsavedir (DIR *dirp, enum savedir_option option)
{
  char *name_space = NULL;
  idx_t allocated = 0;
  direntry_t *entries = NULL;
  idx_t entries_allocated = 0;
  idx_t entries_used = 0;
  idx_t used = 0;
  comparison_function cmp = comparison_function_table[option];

  if (dirp == NULL)
    return NULL;

  for (;;)
    {
      struct dirent const *dp;
      char const *entry;

      errno = 0;
      dp = readdir (dirp);
      if (! dp)
        break;

      /* Skip "", ".", and "..".  "" is returned by at least one buggy
         implementation: Solaris 2.4 readdir on NFS file systems.  */
      entry = dp->d_name;
      if (entry[entry[0] != '.' ? 0 : entry[1] != '.' ? 1 : 2] != '\0')
        {
          idx_t entry_size = _D_EXACT_NAMLEN (dp) + 1;
          if (allocated - used <= entry_size)
            name_space = xpalloc (name_space, &allocated,
                                  entry_size - (allocated - used),
                                  IDX_MAX - 1, sizeof *name_space);
          memcpy (name_space + used, entry, entry_size);
          if (cmp)
            {
              if (entries_allocated == entries_used)
                entries = xpalloc (entries, &entries_allocated, 1, -1,
                                   sizeof *entries);
              entries[entries_used].name = used;
#if D_INO_IN_DIRENT
              entries[entries_used].ino = dp->d_ino;
#endif
              entries_used++;
            }
          used += entry_size;
        }
    }

  if (errno != 0)
    {
      free (name_space);
      name_space = NULL;
    }
  else if (cmp)
    {
      if (entries_used)
        qsort_r (entries, entries_used, sizeof *entries, cmp, name_space);
      char *sorted_name_space = ximalloc (used + 1);
      char *p = sorted_name_space;
      for (idx_t i = 0; i < entries_used; i++)
        p = stpcpy (p, name_space + entries[i].name) + 1;
      *p = '\0';
      free (name_space);
      name_space = sorted_name_space;
    }
  else
    {
      if (used == allocated)
        name_space = xirealloc (name_space, used + 1);
      name_space[used] = '\0';
    }

  free (entries);
  return name_space;
}

/* Return a freshly allocated string containing the file names
   in directory DIR, separated by '\0' characters;
   the end is marked by two '\0' characters in a row.
   Return NULL (setting errno) if DIR cannot be opened, read, or closed.  */

char *
savedir (char const *dir, enum savedir_option option)
{
  DIR *dirp = opendir (dir);
  if (! dirp)
    return NULL;
  else
    {
      char *name_space = streamsavedir (dirp, option);
      if (closedir (dirp) != 0)
        {
          free (name_space);
          return NULL;
        }
      return name_space;
    }
}
