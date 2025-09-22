/* Get public symbol information.
   Copyright (C) 2002, 2003, 2004, 2005, 2008 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <libdwP.h>
#include <dwarf.h>
#include <system.h>


static int
get_offsets (Dwarf *dbg)
{
  size_t allocated = 0;
  size_t cnt = 0;
  struct pubnames_s *mem = NULL;
  const size_t entsize = sizeof (struct pubnames_s);
  unsigned char *const startp = dbg->sectiondata[IDX_debug_pubnames]->d_buf;
  unsigned char *readp = startp;
  unsigned char *endp = readp + dbg->sectiondata[IDX_debug_pubnames]->d_size;

  while (readp + 14 < endp)
    {
      /* If necessary, allocate more entries.  */
      if (cnt >= allocated)
	{
	  allocated = MAX (10, 2 * allocated);
	  struct pubnames_s *newmem = realloc (mem, allocated * entsize);
	  if (newmem == NULL)
	    {
	      __libdw_seterrno (DWARF_E_NOMEM);
	    err_return:
	      free (mem);
	      return -1;
	    }

	  mem = newmem;
	}

      /* Read the set header.  */
      int len_bytes = 4;
      Dwarf_Off len = read_4ubyte_unaligned_inc (dbg, readp);
      if (len == DWARF3_LENGTH_64_BIT)
	{
	  len = read_8ubyte_unaligned_inc (dbg, readp);
	  len_bytes = 8;
	}
      else if (unlikely (len >= DWARF3_LENGTH_MIN_ESCAPE_CODE
			 && len <= DWARF3_LENGTH_MAX_ESCAPE_CODE))
	{
	  __libdw_seterrno (DWARF_E_INVALID_DWARF);
	  goto err_return;
	}

      /* Now we know the offset of the first offset/name pair.  */
      mem[cnt].set_start = readp + 2 + 2 * len_bytes - startp;
      mem[cnt].address_len = len_bytes;
      size_t max_size = dbg->sectiondata[IDX_debug_pubnames]->d_size;
      if (mem[cnt].set_start >= max_size
	  || len - (2 + 2 * len_bytes) > max_size - mem[cnt].set_start)
	/* Something wrong, the first entry is beyond the end of
	   the section.  Or the length of the whole unit is too big.  */
	break;

      /* Read the version.  It better be two for now.  */
      uint16_t version = read_2ubyte_unaligned (dbg, readp);
      if (unlikely (version != 2))
	{
	  __libdw_seterrno (DWARF_E_INVALID_VERSION);
	  goto err_return;
	}

      /* Get the CU offset.  */
      if (__libdw_read_offset (dbg, dbg, IDX_debug_pubnames,
			       readp + 2, len_bytes,
			       &mem[cnt].cu_offset, IDX_debug_info, 3))
	/* Error has been already set in reader.  */
	goto err_return;

      /* Determine the size of the CU header.  */
      unsigned char *infop
	= ((unsigned char *) dbg->sectiondata[IDX_debug_info]->d_buf
	   + mem[cnt].cu_offset);
      if (read_4ubyte_unaligned_noncvt (infop) == DWARF3_LENGTH_64_BIT)
	mem[cnt].cu_header_size = 23;
      else
	mem[cnt].cu_header_size = 11;

      ++cnt;

      /* Advance to the next set.  */
      readp += len;
    }

  if (mem == NULL || cnt == 0)
    {
      free (mem);
      __libdw_seterrno (DWARF_E_NO_ENTRY);
      return -1;
    }

  dbg->pubnames_sets = realloc (mem, cnt * entsize);
  dbg->pubnames_nsets = cnt;

  return 0;
}


ptrdiff_t
dwarf_getpubnames (Dwarf *dbg,
		   int (*callback) (Dwarf *, Dwarf_Global *, void *),
		   void *arg, ptrdiff_t offset)
{
  if (dbg == NULL)
    return -1l;

  if (unlikely (offset < 0))
    {
      __libdw_seterrno (DWARF_E_INVALID_OFFSET);
      return -1l;
    }

  /* Make sure it is a valid offset.  */
  if (unlikely (dbg->sectiondata[IDX_debug_pubnames] == NULL
		|| ((size_t) offset
		    >= dbg->sectiondata[IDX_debug_pubnames]->d_size)))
    /* No (more) entry.  */
    return 0;

  /* If necessary read the set information.  */
  if (dbg->pubnames_nsets == 0 && unlikely (get_offsets (dbg) != 0))
    return -1l;

  /* Find the place where to start.  */
  size_t cnt;
  if (offset == 0)
    {
      cnt = 0;
      offset = dbg->pubnames_sets[0].set_start;
    }
  else
    {
      for (cnt = 0; cnt + 1 < dbg->pubnames_nsets; ++cnt)
	if ((Dwarf_Off) offset >= dbg->pubnames_sets[cnt].set_start)
	  {
	    assert ((Dwarf_Off) offset
		    < dbg->pubnames_sets[cnt + 1].set_start);
	    break;
	  }
      assert (cnt + 1 < dbg->pubnames_nsets);
    }

  unsigned char *startp
    = (unsigned char *) dbg->sectiondata[IDX_debug_pubnames]->d_buf;
  unsigned char *endp
    = startp + dbg->sectiondata[IDX_debug_pubnames]->d_size;
  unsigned char *readp = startp + offset;
  while (1)
    {
      Dwarf_Global gl;

      gl.cu_offset = (dbg->pubnames_sets[cnt].cu_offset
		      + dbg->pubnames_sets[cnt].cu_header_size);

      while (1)
	{
	  /* READP points to the next offset/name pair.  */
	  if (readp + dbg->pubnames_sets[cnt].address_len > endp)
	    goto invalid_dwarf;
	  if (dbg->pubnames_sets[cnt].address_len == 4)
	    gl.die_offset = read_4ubyte_unaligned_inc (dbg, readp);
	  else
	    gl.die_offset = read_8ubyte_unaligned_inc (dbg, readp);

	  /* If the offset is zero we reached the end of the set.  */
	  if (gl.die_offset == 0)
	    break;

	  /* Add the CU offset.  */
	  gl.die_offset += dbg->pubnames_sets[cnt].cu_offset;

	  gl.name = (char *) readp;
	  readp = (unsigned char *) memchr (gl.name, '\0', endp - readp);
	  if (unlikely (readp == NULL))
	    {
	    invalid_dwarf:
	      __libdw_seterrno (DWARF_E_INVALID_DWARF);
	      return -1l;
	    }
	  readp++;

	  /* We found name and DIE offset.  Report it.  */
	  if (callback (dbg, &gl, arg) != DWARF_CB_OK)
	    {
	      /* The user wants us to stop.  Return the offset of the
		 next entry.  */
	      return readp - startp;
	    }
	}

      if (++cnt == dbg->pubnames_nsets)
	/* This was the last set.  */
	break;

      startp = (unsigned char *) dbg->sectiondata[IDX_debug_pubnames]->d_buf;
      readp = startp + dbg->pubnames_sets[cnt].set_start;
    }

  /* We are done.  No more entries.  */
  return 0;
}
