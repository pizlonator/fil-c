/* Rename a file relative to open directories.
   Copyright (C) 2009-2024 Free Software Foundation, Inc.

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

/* written by Eric Blake and Paul Eggert */

#include <config.h>

#include "renameatu.h"

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __linux__
# include <sys/syscall.h>
#endif

static int
errno_fail (int e)
{
  errno = e;
  return -1;
}

#if HAVE_RENAMEAT

# include <stdlib.h>
# include <string.h>

# include "dirname.h"
# include "openat.h"

#else
# include "openat-priv.h"

static int
rename_noreplace (char const *src, char const *dst)
{
  /* This has a race between the call to lstat and the call to rename.  */
  struct stat st;
  return (lstat (dst, &st) == 0 || errno == EOVERFLOW ? errno_fail (EEXIST)
          : errno == ENOENT ? rename (src, dst)
          : -1);
}
#endif

#undef renameat

#if HAVE_RENAMEAT

/* Act like renameat (FD1, SRC, FD2, DST), except fail with EEXIST if
   FLAGS is nonzero and it is easy to fail atomically if DST already exists.
   This lets renameatu be atomic when it can be implemented in terms
   of renameatx_np.  */
static int
renameat2ish (int fd1, char const *src, int fd2, char const *dst,
              unsigned int flags)
{
# ifdef RENAME_SWAP
  if (flags & RENAME_EXCHANGE)
    return renameatx_np (fd1, src, fd2, dst, RENAME_SWAP);
# endif
# ifdef RENAME_EXCL
  if (flags)
    {
      int r = renameatx_np (fd1, src, fd2, dst, RENAME_EXCL);
      if (r == 0 || errno != ENOTSUP)
        return r;
    }
# endif

  return renameat (fd1, src, fd2, dst);
}
#endif

/* Rename FILE1, in the directory open on descriptor FD1, to FILE2, in
   the directory open on descriptor FD2.  If possible, do it without
   changing the working directory.  Otherwise, resort to using
   save_cwd/fchdir, then rename/restore_cwd.  If either the save_cwd or
   the restore_cwd fails, then give a diagnostic and exit nonzero.

   Obey FLAGS when doing the renaming.  If FLAGS is zero, this
   function is equivalent to renameat (FD1, SRC, FD2, DST).
   Otherwise, attempt to implement FLAGS even if the implementation is
   not atomic; this differs from the GNU/Linux native renameat2,
   which fails if it cannot guarantee atomicity.  */

int
renameatu (int fd1, char const *src, int fd2, char const *dst,
           unsigned int flags)
{
  int ret_val = -1;
  int err = EINVAL;

#ifdef HAVE_RENAMEAT2
  ret_val = renameat2 (fd1, src, fd2, dst, flags);
  err = errno;
#elif defined SYS_renameat2
  ret_val = syscall (SYS_renameat2, fd1, src, fd2, dst, flags);
  err = errno;
#endif

  if (! (ret_val < 0 && (err == EINVAL || err == ENOSYS || err == ENOTSUP)))
    return ret_val;

#if HAVE_RENAMEAT
  {
  size_t src_len;
  size_t dst_len;
  char *src_temp = (char *) src;
  char *dst_temp = (char *) dst;
  bool src_slash;
  bool dst_slash;
  int rename_errno = ENOTDIR;
  struct stat src_st;
  struct stat dst_st;
  bool dst_found_nonexistent = false;

  switch (flags)
    {
    case 0:
      break;

    case RENAME_NOREPLACE:
      /* This has a race between the call to fstatat and the calls to
         renameat below.  This fstatat is needed even if RENAME_EXCL
         is defined, because RENAME_EXCL is buggy on macOS 11.2:
         renameatx_np (fd, "X", fd, "X", RENAME_EXCL) incorrectly
         succeeds when X exists.  */
      if (fstatat (fd2, dst, &dst_st, AT_SYMLINK_NOFOLLOW) == 0
          || errno == EOVERFLOW)
        return errno_fail (EEXIST);
      if (errno != ENOENT)
        return -1;
      dst_found_nonexistent = true;
      break;

    case RENAME_EXCHANGE:
#ifdef RENAME_SWAP
      break;
#endif
    default:
      return errno_fail (ENOTSUP);
    }

  /* Let strace see any ENOENT failure.  */
  src_len = strlen (src);
  dst_len = strlen (dst);
  if (!src_len || !dst_len)
    return renameat2ish (fd1, src, fd2, dst, flags);

  src_slash = src[src_len - 1] == '/';
  dst_slash = dst[dst_len - 1] == '/';
  if (!src_slash && !dst_slash)
    return renameat2ish (fd1, src, fd2, dst, flags);

  /* Presence of a trailing slash requires directory semantics.  If
     the source does not exist, or if the destination cannot be turned
     into a directory, give up now.  Otherwise, strip trailing slashes
     before calling rename.  */
  if (fstatat (fd1, src, &src_st, AT_SYMLINK_NOFOLLOW))
    return -1;
  if (dst_found_nonexistent)
    {
      if (!S_ISDIR (src_st.st_mode))
        return errno_fail (ENOENT);
    }
  else if (fstatat (fd2, dst, &dst_st, AT_SYMLINK_NOFOLLOW))
    {
      if (errno != ENOENT || !S_ISDIR (src_st.st_mode))
        return -1;
    }
  else if (!S_ISDIR (dst_st.st_mode))
    return errno_fail (ENOTDIR);
  else if (!S_ISDIR (src_st.st_mode))
    return errno_fail (EISDIR);

# if RENAME_TRAILING_SLASH_SOURCE_BUG
  /* See the lengthy comment in rename.c why Solaris 9 is forced to
     GNU behavior, while Solaris 10 is left with POSIX behavior,
     regarding symlinks with trailing slash.  */
  ret_val = -1;
  if (src_slash)
    {
      src_temp = strdup (src);
      if (!src_temp)
        {
          /* Rather than rely on strdup-posix, we set errno ourselves.  */
          rename_errno = ENOMEM;
          goto out;
        }
      strip_trailing_slashes (src_temp);
      if (fstatat (fd1, src_temp, &src_st, AT_SYMLINK_NOFOLLOW))
        {
          rename_errno = errno;
          goto out;
        }
      if (S_ISLNK (src_st.st_mode))
        goto out;
    }
  if (dst_slash)
    {
      dst_temp = strdup (dst);
      if (!dst_temp)
        {
          rename_errno = ENOMEM;
          goto out;
        }
      strip_trailing_slashes (dst_temp);
      char readlink_buf[1];
      if (readlinkat (fd2, dst_temp, readlink_buf, sizeof readlink_buf) < 0)
        {
          if (errno != ENOENT && errno != EINVAL)
            {
              rename_errno = errno;
              goto out;
            }
        }
      else
        goto out;
    }
# endif /* RENAME_TRAILING_SLASH_SOURCE_BUG */

  /* renameat does not honor trailing / on Solaris 10.  Solve it in a
     similar manner to rename.  No need to worry about bugs not present
     on Solaris, since all other systems either lack renameat or honor
     trailing slash correctly.  */

  ret_val = renameat2ish (fd1, src_temp, fd2, dst_temp, flags);
  rename_errno = errno;
  goto out;
 out:
  if (src_temp != src)
    free (src_temp);
  if (dst_temp != dst)
    free (dst_temp);
  errno = rename_errno;
  return ret_val;
  }
#else /* !HAVE_RENAMEAT */

  /* RENAME_NOREPLACE is the only flag currently supported.  */
  if (flags & ~RENAME_NOREPLACE)
    return errno_fail (ENOTSUP);
  return at_func2 (fd1, src, fd2, dst, flags ? rename_noreplace : rename);

#endif /* !HAVE_RENAMEAT */
}
