/* Lookup helper function for Hurd implementation of *at functions.
   Copyright (C) 2006-2024 Free Software Foundation, Inc.
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

#include <hurd.h>
#include <hurd/lookup.h>
#include <hurd/fd.h>
#include <string.h>
#include <fcntl.h>

file_t
__file_name_lookup_at (int fd, int at_flags,
		       const char *file_name, int flags, mode_t mode)
{
  error_t err;
  file_t result;
  int empty = at_flags & AT_EMPTY_PATH;
  int orig_flags;

  at_flags &= ~AT_EMPTY_PATH;

  err = __hurd_at_flags (&at_flags, &flags);
  if (err)
    return (__hurd_fail (err), MACH_PORT_NULL);

  if (empty != 0 && file_name[0] == '\0')
    {
      enum retry_type doretry;
      char retryname[1024];	/* XXX string_t LOSES! */

      err = HURD_DPORT_USE (fd, __dir_lookup (port, "", flags, mode,
					      &doretry, retryname,
					      &result));

      if (! err)
	err = __hurd_file_name_lookup_retry (&_hurd_ports_use, &__getdport,
					     NULL, doretry, retryname,
					     flags, mode, &result);

      return err ? (__hurd_dfail (fd, err), MACH_PORT_NULL) : result;
    }

  orig_flags = flags;
  if (flags & O_TMPFILE)
    flags = O_DIRECTORY;

  if (fd == AT_FDCWD || file_name[0] == '/')
    {
      err = __hurd_file_name_lookup (&_hurd_ports_use, &__getdport, 0,
                                     file_name, flags, mode & ~_hurd_umask,
                                     &result);
      if (err)
        {
          __hurd_fail (err);
          return MACH_PORT_NULL;
        }
    }
  else
    {
      file_t startdir;
      /* We need to look the file up relative to the given directory (and
         not our cwd).  For this to work, we supply our own wrapper for
         _hurd_ports_use, which replaces cwd with our startdir.  */
      error_t use_init_port (int which, error_t (*operate) (mach_port_t))
        {
          return (which == INIT_PORT_CWDIR ? (*operate) (startdir)
	          : _hurd_ports_use (which, operate));
        }

      err = HURD_DPORT_USE (fd, (startdir = port,
                                 __hurd_file_name_lookup (&use_init_port,
                                                          &__getdport, NULL,
                                                          file_name,
                                                          flags,
                                                          mode & ~_hurd_umask,
                                                          &result)));
      if (err)
        {
          __hurd_dfail (fd, err);
          return MACH_PORT_NULL;
        }
    }

  if (orig_flags & O_TMPFILE)
    {
      /* What we have looked up is not the file itself, but actually
         the directory to create the file in.  Do that now.  */
      file_t dir = result;

      err = __dir_mkfile (dir, orig_flags & ~(O_TMPFILE | O_DIRECTORY),
                          mode, &result);
      __mach_port_deallocate (__mach_task_self (), dir);
      if (err)
        {
          __hurd_fail (err);
          return MACH_PORT_NULL;
        }
    }

  return result;
}

file_t
__file_name_split_at (int fd, const char *file_name, char **name)
{
  error_t err;
  file_t result;

  if (fd == AT_FDCWD || file_name[0] == '/')
    return __file_name_split (file_name, name);

  err = __hurd_file_name_split (&_hurd_ports_use, &__getdport, 0,
				file_name, &result, name);

  file_t startdir;
  error_t use_init_port (int which, error_t (*operate) (mach_port_t))
  {
    return (which == INIT_PORT_CWDIR ? (*operate) (startdir)
	    : _hurd_ports_use (which, operate));
  }

  err = HURD_DPORT_USE (fd, (startdir = port,
			     __hurd_file_name_split (&use_init_port,
						     &__getdport, 0,
						     file_name,
						     &result, name)));

  return err ? (__hurd_dfail (fd, err), MACH_PORT_NULL) : result;
}

file_t
__directory_name_split_at (int fd, const char *directory_name, char **name)
{
  error_t err;
  file_t result;

  if (fd == AT_FDCWD || directory_name[0] == '/')
    return __directory_name_split (directory_name, name);

  file_t startdir;
  error_t use_init_port (int which, error_t (*operate) (mach_port_t))
    {
      return (which == INIT_PORT_CWDIR ? (*operate) (startdir)
	      : _hurd_ports_use (which, operate));
    }

  err = HURD_DPORT_USE (fd, (startdir = port,
			     __hurd_directory_name_split (&use_init_port,
							  &__getdport, 0,
							  directory_name,
							  &result, name)));

  return err ? (__hurd_dfail (fd, err), MACH_PORT_NULL) : result;
}
