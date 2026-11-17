/*
 *  Copyright 2000 Red Hat, Inc.
 *  g_execvpe implementation based on GNU libc execvp:
 *   Copyright 1991, 92, 95, 96, 97, 98, 99 Free Software Foundation, Inc.
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

#include <glib-unix.h>
#include <gio/gio.h>

#include "vtespawn.hh"
#include "reaper.hh"

#include "missing.hh"

/* This function is called between fork and execve/_exit and so must be
 * async-signal-safe; see man:signal-safety(7).
 */
static ssize_t
write_all (int fd,
           void const* vbuf,
           size_t to_write) noexcept
{
  char *buf = (char *) vbuf;

  while (to_write > 0)
    {
      auto count = write(fd, buf, to_write);
      if (count < 0)
        {
          if (errno != EINTR)
            return FALSE;
        }
      else
        {
          to_write -= count;
          buf += count;
        }
    }

  return TRUE;
}

/* This function is called between fork and execve/_exit and so must be
 * async-signal-safe; see man:signal-safety(7).
 */
void
_vte_write_err (int fd,
                int msg) noexcept
{
        int data[2] = {msg, errno};

        write_all(fd, data, sizeof(data));
}

/* Based on execvp from GNU C Library */

/* This function is called between fork and execve/_exit and so must be
 * async-signal-safe; see man:signal-safety(7).
 */
/* Returns false if failing before execv(e), or true if failing after it. */
static bool
script_execute (char const* file,
                char** argv,
                char** envp,
                void* workbuf,
                size_t workbufsize) noexcept
{
  /* Count the arguments.  */
  int argc = 0;
  while (argv[argc])
    ++argc;

  auto argv_buffer = reinterpret_cast<char**>(workbuf);
  auto argv_buffer_len = workbufsize / sizeof(char*);
  /* Construct an argument list for the shell. */
  if (size_t(argc + 2) > argv_buffer_len) {
    errno = ENOMEM;
    return false;
  }

  argv_buffer[0] = (char *) "/bin/sh";
  argv_buffer[1] = (char *) file;
  while (argc > 0)
    {
      argv_buffer[argc + 1] = argv[argc];
      --argc;
    }

  /* Execute the shell. */
  if (envp)
    execve (argv_buffer[0], argv_buffer, envp);
  else
    execv (argv_buffer[0], argv_buffer);

  return true;
}

/* This function is called between fork and execve/_exit and so must be
 * async-signal-safe; see man:signal-safety(7).
 */
int
_vte_execute (const char *file,
              char      **argv,
              char      **envp,
              char const* search_path,
              void* workbuf,
              size_t workbufsize) noexcept
{
  if (*file == '\0')
    {
      /* We check the simple case first. */
      errno = ENOENT;
      return -1;
    }

  if (!search_path || strchr (file, '/') != nullptr)
    {
      /* Don't search when it contains a slash. */
      if (envp)
        execve (file, argv, envp);
      else
        execv (file, argv);

      if (errno == ENOEXEC)
         script_execute(file, argv, envp, workbuf, workbufsize);
    }
  else
    {
      auto got_eacces = false;
      char const* path = search_path;
      char const* p;
      char* name = reinterpret_cast<char*>(workbuf);

      auto const len = strlen(file) + 1;
      auto const pathlen = strlen(path);

      if (workbufsize < pathlen + len + 1)
        {
          errno = ENOMEM;
          return -1;
        }

      /* Copy the file name at the top, including '\0'  */
      memcpy (name + pathlen + 1, file, len);
      name = name + pathlen;
      /* And add the slash before the filename  */
      *name = '/';

      p = path;
      do
	{
	  char *startp;

	  path = p;
	  p = strchrnul (path, ':');

	  if (p == path)
	    /* Two adjacent colons, or a colon at the beginning or the end
             * of 'PATH' means to search the current directory.
             */
	    startp = name + 1;
	  else
            startp = reinterpret_cast<char*>(memcpy (name - (p - path), path, p - path));

	  /* Try to execute this name.  If it works, execv will not return.  */
          if (envp)
            execve (startp, argv, envp);
          else
            execv (startp, argv);

          if (errno == ENOEXEC &&
              !script_execute(startp, argv, envp, workbuf, workbufsize))
            return -1;

	  switch (errno)
	    {
	    case EACCES:
	      /* Record the we got a 'Permission denied' error.  If we end
               * up finding no executable we can use, we want to diagnose
               * that we did find one but were denied access.
               */
	      got_eacces = TRUE;

              [[fallthrough]];
	    case ENOENT:
#ifdef ESTALE
	    case ESTALE:
#endif
#ifdef ENOTDIR
	    case ENOTDIR:
#endif
	      /* Those errors indicate the file is missing or not executable
               * by us, in which case we want to just try the next path
               * directory.
               */
	      break;

	    case ENODEV:
	    case ETIMEDOUT:
	      /* Some strange filesystems like AFS return even
	       * stranger error numbers.  They cannot reasonably mean anything
	       * else so ignore those, too.
	       */
	      break;

	    default:
	      /* Some other error means we found an executable file, but
               * something went wrong executing it; return the error to our
               * caller.
               */
	      return -1;
	    }
	}
      while (*p++ != '\0');

      /* We tried every element and none of them worked.  */
      if (got_eacces)
	/* At least one failure was due to permissions, so report that
         * error.
         */
        errno = EACCES;
    }

  /* Return the error from the last attempt (probably ENOENT).  */
  return -1;
}
