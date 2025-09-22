/* Compute CRC32 checksum of file contents.
   Copyright (C) 2006 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "libeu.h"
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "system.h"

int
crc32_file (int fd, uint32_t *resp)
{
  unsigned char buffer[1024 * 8];
  uint32_t crc = 0;
  off_t off = 0;
  ssize_t count;

  struct stat st;
  if (fstat (fd, &st) == 0)
    {
      /* Try mapping in the file data.  */
      size_t mapsize = st.st_size;
      void *mapped = mmap (NULL, mapsize, PROT_READ, MAP_PRIVATE, fd, 0);
      if (mapped == MAP_FAILED && errno == ENOMEM)
	{
	  const size_t pagesize = sysconf (_SC_PAGESIZE);
	  mapsize = ((mapsize / 2) + pagesize - 1) & -pagesize;
	  while (mapsize >= pagesize
		 && (mapped = mmap (NULL, mapsize, PROT_READ, MAP_PRIVATE,
				    fd, 0)) == MAP_FAILED && errno == ENOMEM)
	    mapsize /= 2;
	}
      if (mapped != MAP_FAILED)
	{
	  do
	    {
	      if (st.st_size <= (off_t) mapsize)
		{
		  *resp = crc32 (crc, mapped, st.st_size);
		  munmap (mapped, mapsize);
		  return 0;
		}
	      crc = crc32 (crc, mapped, mapsize);
	      off += mapsize;
	      st.st_size -= mapsize;
	    } while (mmap (mapped, mapsize, PROT_READ, MAP_FIXED|MAP_PRIVATE,
			   fd, off) == mapped);
	  munmap (mapped, mapsize);
	}
    }

  while ((count = TEMP_FAILURE_RETRY (pread (fd, buffer, sizeof buffer,
					     off))) > 0)
    {
      off += count;
      crc = crc32 (crc, buffer, count);
    }

  *resp = crc;

  return count == 0 ? 0 : -1;
}
