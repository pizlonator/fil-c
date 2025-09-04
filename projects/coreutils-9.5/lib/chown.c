/* provide consistent interface to chown for systems that don't interpret
   an ID of -1 as meaning "don't change the corresponding ID".

   Copyright (C) 1997, 2004-2007, 2009-2024 Free Software Foundation, Inc.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* written by Jim Meyering */

#include <config.h>

/* Specification.  */
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#if !HAVE_CHOWN

/* Simple stub that always fails with ENOSYS, for mingw.  */
int
chown (_GL_UNUSED const char *file, _GL_UNUSED uid_t uid,
       _GL_UNUSED gid_t gid)
{
  errno = ENOSYS;
  return -1;
}

#else /* HAVE_CHOWN */

/* Below we refer to the system's chown().  */
# undef chown

/* Provide a more-closely POSIX-conforming version of chown on
   systems with one or both of the following problems:
   - chown doesn't treat an ID of -1 as meaning
   "don't change the corresponding ID".
   - chown doesn't dereference symlinks.  */

int
rpl_chown (const char *file, uid_t uid, gid_t gid)
{
# if (CHOWN_CHANGE_TIME_BUG || CHOWN_FAILS_TO_HONOR_ID_OF_NEGATIVE_ONE \
      || CHOWN_TRAILING_SLASH_BUG)
  struct stat st;
  bool stat_valid = false;
# endif
  int result;

# if CHOWN_CHANGE_TIME_BUG /* OpenBSD 7.2 */
  if (gid != (gid_t) -1 || uid != (uid_t) -1)
    {
      if (stat (file, &st))
        return -1;
      stat_valid = true;
    }
# endif

# if CHOWN_FAILS_TO_HONOR_ID_OF_NEGATIVE_ONE /* some very old platforms */
  if (gid == (gid_t) -1 || uid == (uid_t) -1)
    {
      /* Stat file to get id(s) that should remain unchanged.  */
      if (!stat_valid && stat (file, &st))
        return -1;
      if (gid == (gid_t) -1)
        gid = st.st_gid;
      if (uid == (uid_t) -1)
        uid = st.st_uid;
    }
# endif

# if CHOWN_MODIFIES_SYMLINK /* some very old platforms */
  /* The system-supplied chown function does not follow symlinks.
     If the file is a symlink, open the file (following symlinks), and
     fchown the resulting descriptor.  Although the open might fail
     due to lack of permissions, it's the best we can easily do.  */
  char linkbuf[1];
  if (0 <= readlink (file, linkbuf, sizeof linkbuf))
    {
      int open_flags = O_NONBLOCK | O_NOCTTY | O_CLOEXEC;
      int fd = open (file, O_RDONLY | open_flags);
      if (fd < 0
          && (errno != EACCES
              || ((fd = open (file, O_WRONLY | open_flags)) < 0))
          && (errno != EACCES || O_EXEC == O_RDONLY
              || ((fd = open (file, O_EXEC | open_flags)) < 0))
          && (errno != EACCES || O_SEARCH == O_RDONLY || O_SEARCH == O_EXEC
              || ((fd = open (file, O_SEARCH | open_flags)) < 0)))
        return fd;

      int r = fchown (fd, uid, gid);
      int err = errno;
      close (fd);
      errno = err;
      return r;
    }
# endif

# if CHOWN_TRAILING_SLASH_BUG /* macOS 12.5, FreeBSD 7.2, AIX 7.3.1, Solaris 9 */
  if (!stat_valid)
    {
      size_t len = strlen (file);
      if (len && file[len - 1] == '/' && stat (file, &st))
        return -1;
    }
# endif

  result = chown (file, uid, gid);

# if CHOWN_CHANGE_TIME_BUG /* OpenBSD 7.2 */
  if (result == 0 && stat_valid
      && (uid == st.st_uid || uid == (uid_t) -1)
      && (gid == st.st_gid || gid == (gid_t) -1))
    {
      /* No change in ownership, but at least one argument was not -1,
         so we are required to update ctime.  Since chown succeeded,
         we assume that chmod will do likewise.  Fortunately, on all
         known systems where a 'no-op' chown skips the ctime update, a
         'no-op' chmod still does the trick.  */
      result = chmod (file, st.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO
                                          | S_ISUID | S_ISGID | S_ISVTX));
    }
# endif

  return result;
}

#endif /* HAVE_CHOWN */
