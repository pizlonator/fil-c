/* Determine whether two file names refer to the same file.

   Copyright (C) 1997-2000, 2002-2006, 2009-2024 Free Software Foundation, Inc.

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

/* written by Jim Meyering */

#include <config.h>

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>

#include <string.h>

#include <limits.h>
#ifndef _POSIX_NAME_MAX
# define _POSIX_NAME_MAX 14
#endif

#include "same.h"
#include "dirname.h"
#include <error.h>
#include "same-inode.h"

#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/* Whether file name components are silently truncated (behavior that
   POSIX stopped allowing in 2008).  This enables checks whether
   truncated base names are the same, while checking the directories.  */
#if !_POSIX_NO_TRUNC && HAVE_FPATHCONF && defined _PC_NAME_MAX
# define CHECK_TRUNCATION true
#else
# define CHECK_TRUNCATION false
#endif

/* Return nonzero if SOURCE and DEST point to the same name in the same
   directory.  */

bool
same_name (const char *source, const char *dest)
{
  return same_nameat (AT_FDCWD, source, AT_FDCWD, dest);
}

/* Likewise, but interpret the file names relative to SOURCE_FD and DEST_FD,
   in the style of openat.  */

bool
same_nameat (int source_dfd, char const *source,
             int dest_dfd, char const *dest)
{
  /* Compare the basenames.  */
  char const *source_basename = last_component (source);
  char const *dest_basename = last_component (dest);
  size_t source_baselen = base_len (source_basename);
  size_t dest_baselen = base_len (dest_basename);
  bool identical_basenames =
    (source_baselen == dest_baselen
     && memcmp (source_basename, dest_basename, dest_baselen) == 0);
  bool compare_dirs = identical_basenames;
  bool same = false;

#if CHECK_TRUNCATION
  size_t slen_max = HAVE_LONG_FILE_NAMES ? 255 : _POSIX_NAME_MAX;
  size_t min_baselen = MIN (source_baselen, dest_baselen);
  if (slen_max <= min_baselen
      && memcmp (source_basename, dest_basename, slen_max) == 0)
    compare_dirs = true;
#endif

  if (compare_dirs)
    {
      struct stat source_dir_stats;
      struct stat dest_dir_stats;

      /* Compare the parent directories (via the device and inode numbers).  */
      char *source_dirname = dir_name (source);
      int flags = AT_SYMLINK_NOFOLLOW;
      if (fstatat (source_dfd, source_dirname, &source_dir_stats, flags) != 0)
        {
          /* Shouldn't happen.  */
          error (1, errno, "%s", source_dirname);
        }
      free (source_dirname);

      char *dest_dirname = dir_name (dest);

#if CHECK_TRUNCATION
      int destdir_errno = 0;
      int open_flags = O_SEARCH | O_CLOEXEC | O_DIRECTORY;
      int destdir_fd = openat (dest_dfd, dest_dirname, open_flags);
      if (destdir_fd < 0 || fstat (destdir_fd, &dest_dir_stats) != 0)
        destdir_errno = errno;
      else if (psame_inode (&source_dir_stats, &dest_dir_stats))
        {
          same = identical_basenames;
          if (! same)
            {
              errno = 0;
              long name_max = fpathconf (destdir_fd, _PC_NAME_MAX);
              if (name_max < 0)
                destdir_errno = errno;
              else
                same = (name_max <= min_baselen
                        && (memcmp (source_basename, dest_basename, name_max)
                            == 0));
            }
        }
      close (destdir_fd);
      if (destdir_errno != 0)
        {
          /* Shouldn't happen.  */
          error (1, destdir_errno, "%s", dest_dirname);
        }
#else
      if (fstatat (dest_dfd, dest_dirname, &dest_dir_stats, flags) != 0)
        {
          /* Shouldn't happen.  */
          error (1, errno, "%s", dest_dirname);
        }
      same = psame_inode (&source_dir_stats, &dest_dir_stats);
#endif

      free (dest_dirname);
    }

  return same;
}
