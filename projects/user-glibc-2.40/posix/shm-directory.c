/* Determine directory for shm/sem files.  Generic POSIX version.
   Copyright (C) 2014-2024 Free Software Foundation, Inc.
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

#include <unistd.h>

#if _POSIX_MAPPED_FILES

#include <alloc_buffer.h>
#include <shm-directory.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>

int
__shm_get_name (struct shmdir_name *result, const char *name, bool sem_prefix)
{
  struct alloc_buffer buffer;
  size_t namelen;

  buffer = alloc_buffer_create (result->name, sizeof (result->name));
  alloc_buffer_copy_bytes (&buffer, SHMDIR, strlen (SHMDIR));

#if defined (SHM_ANON) && defined (O_TMPFILE)
  if (name == SHM_ANON)
    {
      /* For SHM_ANON, we want shm_open () to pass O_TMPFILE to open (),
         with SHMDIR itself as the path.  So, leave it at that.  */
      alloc_buffer_add_byte (&buffer, 0);
      if (alloc_buffer_has_failed (&buffer))
        return -1;
      return 0;
    }
#endif

  while (name[0] == '/')
    ++name;
  namelen = strlen (name);

  if (sem_prefix)
    alloc_buffer_copy_bytes (&buffer, "sem.", strlen ("sem."));
  alloc_buffer_copy_bytes (&buffer, name, namelen + 1);
  if (namelen == 0 || memchr (name, '/', namelen) != NULL)
    return EINVAL;
  if (alloc_buffer_has_failed (&buffer))
    {
      if (namelen > NAME_MAX)
        return ENAMETOOLONG;
      return EINVAL;
    }
  return 0;
}
libc_hidden_def (__shm_get_name)

#endif
