/* Read a directory.  Linux no-LFS version.
   Copyright (C) 2018-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <dirent.h>

#if !_DIRENT_MATCHES_DIRENT64
#include <dirstream.h>

/* Read a directory entry from DIRP.  */
struct dirent *
__readdir_unlocked (DIR *dirp)
{
  struct dirent *dp;
  int saved_errno = errno;

  if (dirp->offset >= dirp->size)
    {
      /* We've emptied out our buffer.  Refill it.  */

      size_t maxread = dirp->allocation;
      ssize_t bytes;

      bytes = __getdents (dirp->fd, dirp->data, maxread);
      if (bytes <= 0)
	{
	  /* Linux may fail with ENOENT on some file systems if the
	     directory inode is marked as dead (deleted).  POSIX
	     treats this as a regular end-of-directory condition, so
	     do not set errno in that case, to indicate success.  */
	  if (bytes == 0 || errno == ENOENT)
	    __set_errno (saved_errno);
	  return NULL;
	}
      dirp->size = (size_t) bytes;

      /* Reset the offset into the buffer.  */
      dirp->offset = 0;
    }

  dp = (struct dirent *) &dirp->data[dirp->offset];
  dirp->offset += dp->d_reclen;
  dirp->filepos = dp->d_off;

  return dp;
}

struct dirent *
__readdir (DIR *dirp)
{
  struct dirent *dp;

#if IS_IN (libc)
  __libc_lock_lock (dirp->lock);
#endif
  dp = __readdir_unlocked (dirp);
#if IS_IN (libc)
  __libc_lock_unlock (dirp->lock);
#endif

  return dp;
}
weak_alias (__readdir, readdir)

#endif
