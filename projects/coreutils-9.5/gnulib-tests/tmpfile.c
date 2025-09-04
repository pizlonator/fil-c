/* Create a temporary file.
   Copyright (C) 2007, 2009-2024 Free Software Foundation, Inc.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Ben Pfaff. */

#include <config.h>

/* Specification.  */
#include <stdio.h>

#include <errno.h>

#if defined _WIN32 && ! defined __CYGWIN__
/* A native Windows platform.  */

# include <fcntl.h>
# include <string.h>
# include <sys/stat.h>

# include <io.h>

# define WIN32_LEAN_AND_MEAN  /* avoid including junk */
# include <windows.h>

#else

# include <unistd.h>

#endif

#include "pathmax.h"
#include "tempname.h"
#include "tmpdir.h"

/* PATH_MAX is guaranteed to be defined, because this replacement is only
   used on native Windows and Android.  */

#if defined _WIN32 && ! defined __CYGWIN__
/* A native Windows platform.  */

/* Don't assume that UNICODE is not defined.  */
# undef OSVERSIONINFO
# define OSVERSIONINFO OSVERSIONINFOA
# undef GetVersionEx
# define GetVersionEx GetVersionExA
# undef GetTempPath
# define GetTempPath GetTempPathA

/* On Windows, opening a file with _O_TEMPORARY has the effect of passing
   the FILE_FLAG_DELETE_ON_CLOSE flag to CreateFile(), which has the effect
   of deleting the file when it is closed - even when the program crashes.
   But (according to the Cygwin sources) it works only on Windows NT or newer.
   So we cache the info whether we are running on Windows NT or newer.  */

static bool
supports_delete_on_close ()
{
  static int known; /* 1 = yes, -1 = no, 0 = unknown */
  if (!known)
    {
      OSVERSIONINFO v;

      /* According to
         <https://docs.microsoft.com/en-us/windows/desktop/api/sysinfoapi/nf-sysinfoapi-getversionexa>
         this structure must be initialized as follows:  */
      v.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);

      if (GetVersionEx (&v))
        known = (v.dwPlatformId == VER_PLATFORM_WIN32_NT ? 1 : -1);
      else
        known = -1;
    }
  return (known > 0);
}

FILE *
tmpfile (void)
{
  char dir[PATH_MAX];
  DWORD retval;

  /* Find Windows temporary file directory.
     We provide this as the directory argument to path_search because Windows
     defines P_tmpdir to "\\" and will therefore try to put all temporary files
     in the root directory (unless $TMPDIR is set). */
  retval = GetTempPath (PATH_MAX, dir);
  if (retval > 0 && retval < PATH_MAX)
    {
      char xtemplate[PATH_MAX];

      if (path_search (xtemplate, PATH_MAX, dir, NULL, true) >= 0)
        {
          size_t len = strlen (xtemplate);
          int o_temporary = (supports_delete_on_close () ? _O_TEMPORARY : 0);
          int fd;

          do
            {
              memcpy (&xtemplate[len - 6], "XXXXXX", 6);
              if (gen_tempname (xtemplate, 0, 0, GT_NOCREATE) < 0)
                {
                  fd = -1;
                  break;
                }

              fd = _open (xtemplate,
                          _O_CREAT | _O_EXCL | o_temporary
                          | _O_RDWR | _O_BINARY,
                          _S_IREAD | _S_IWRITE);
            }
          while (fd < 0 && errno == EEXIST);

          if (fd >= 0)
            {
              FILE *fp = _fdopen (fd, "w+b");

              if (fp != NULL)
                return fp;
              else
                {
                  int saved_errno = errno;
                  _close (fd);
                  errno = saved_errno;
                }
            }
        }
    }
  else
    {
      if (retval > 0)
        errno = ENAMETOOLONG;
      else
        /* Ideally this should translate GetLastError () to an errno value.  */
        errno = ENOENT;
    }

  return NULL;
}

#else

FILE *
tmpfile (void)
{
  char buf[PATH_MAX];
  int fd;
  FILE *fp;

  /* Try $TMPDIR first, not /tmp nor P_tmpdir, because we need this replacement
     on Android, and /tmp does not exist on Android.  */

  if (path_search (buf, sizeof buf, NULL, "tmpf", true))
    return NULL;

  fd = gen_tempname (buf, 0, 0, GT_FILE);
  if (fd < 0)
    return NULL;

  /* Note that this relies on the Unix semantics that
     a file is not really removed until it is closed.  */
  (void) unlink (buf);

  if ((fp = fdopen (fd, "w+b")) == NULL)
    {
      int saved_errno = errno;
      close (fd);
      errno = saved_errno;
    }

  return fp;
}

#endif
