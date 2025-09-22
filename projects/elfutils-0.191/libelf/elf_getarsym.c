/* Return symbol table of archive.
   Copyright (C) 1998-2000, 2002, 2005, 2009, 2012, 2014, 2015 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 1998.

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
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <dl-hash.h>
#include "libelfP.h"


static int
read_number_entries (uint64_t *nump, Elf *elf, size_t *offp, bool index64_p)
{
  union u
  {
    uint64_t ret64;
    uint32_t ret32;
  } u;

  size_t w = index64_p ? 8 : 4;
  if (elf->map_address != NULL)
    /* Use memcpy instead of pointer dereference so as not to assume the
       field is naturally aligned within the file.  */
    memcpy (&u, elf->map_address + *offp, sizeof u);
  else if ((size_t) pread_retry (elf->fildes, &u, w, *offp) != w)
    return -1;

  *offp += w;

  if (BYTE_ORDER == LITTLE_ENDIAN)
    *nump = index64_p ? bswap_64 (u.ret64) : bswap_32 (u.ret32);
  else
    *nump = index64_p ? u.ret64 : u.ret32;

  return 0;
}

Elf_Arsym *
elf_getarsym (Elf *elf, size_t *ptr)
{
  if (elf->kind != ELF_K_AR)
    {
      /* This is no archive.  */
      __libelf_seterrno (ELF_E_NO_ARCHIVE);
      return NULL;
    }

  if (ptr != NULL)
    /* In case of an error or when we know the value store the expected
       value now.  Doing this allows us easier exits in an error case.  */
    *ptr = elf->state.ar.ar_sym_num;

  if (elf->state.ar.ar_sym == (Elf_Arsym *) -1l)
    {
      /* There is no index.  */
      __libelf_seterrno (ELF_E_NO_INDEX);
      return NULL;
    }

  Elf_Arsym *result = elf->state.ar.ar_sym;
  if (result == NULL)
    {
      /* We have not yet read the index.  */
      rwlock_wrlock (elf->lock);

      /* In case we find no index remember this for the next call.  */
      elf->state.ar.ar_sym = (Elf_Arsym *) -1l;

      /* We might have to allocate some temporary data for reading.  */
      void *temp_data = NULL;

      struct ar_hdr *index_hdr;
      if (elf->map_address == NULL)
	{
	  /* We must read index from the file.  */
	  assert (elf->fildes != -1);
	  if (pread_retry (elf->fildes, &elf->state.ar.ar_hdr,
			   sizeof (struct ar_hdr), elf->start_offset + SARMAG)
	      != sizeof (struct ar_hdr))
	    {
	      /* It is not possible to read the index.  Maybe it does not
		 exist.  */
	      __libelf_seterrno (ELF_E_READ_ERROR);
	      goto out;
	    }

	  index_hdr = &elf->state.ar.ar_hdr;
	}
      else
	{
	  if (SARMAG + sizeof (struct ar_hdr) > elf->maximum_size)
	    {
	      /* There is no room for the full archive.  */
	      __libelf_seterrno (ELF_E_NO_INDEX);
	      goto out;
	    }

	  index_hdr = (struct ar_hdr *) (elf->map_address
					 + elf->start_offset + SARMAG);
	}

      /* Now test whether this really is an archive.  */
      if (memcmp (index_hdr->ar_fmag, ARFMAG, 2) != 0)
	{
	  /* Invalid magic bytes.  */
	  __libelf_seterrno (ELF_E_ARCHIVE_FMAG);
	  goto out;
	}

      bool index64_p;
      /* Now test whether this is the index.  If the name is "/", this
	 is 32-bit index, if it's "/SYM64/", it's 64-bit index.

	 XXX This is not entirely true.  There are some more forms.
	 Which of them shall we handle?  */
      if (memcmp (index_hdr->ar_name, "/               ", 16) == 0)
	index64_p = false;
      else if (memcmp (index_hdr->ar_name, "/SYM64/         ", 16) == 0)
	index64_p = true;
      else
	{
	  /* If the index is not the first entry, there is no index.

	     XXX Is this true?  */
	  __libelf_seterrno (ELF_E_NO_INDEX);
	  goto out;
	}
      int w = index64_p ? 8 : 4;

      /* We have an archive.  The first word in there is the number of
	 entries in the table.  */
      uint64_t n = 0;
      size_t off = elf->start_offset + SARMAG + sizeof (struct ar_hdr);
      if (read_number_entries (&n, elf, &off, index64_p) < 0)
	{
	  /* Cannot read the number of entries.  */
	  __libelf_seterrno (ELF_E_NO_INDEX);
	  goto out;
	}

      /* Now we can perform some first tests on whether all the data
	 needed for the index is available.  */
      char tmpbuf[17];
      memcpy (tmpbuf, index_hdr->ar_size, 10);
      tmpbuf[10] = '\0';
      size_t index_size = atol (tmpbuf);

      if (index_size > elf->maximum_size
	  || elf->maximum_size - index_size < SARMAG + sizeof (struct ar_hdr)
#if SIZE_MAX <= 4294967295U
	  || n >= SIZE_MAX / sizeof (Elf_Arsym)
#endif
	  || n > index_size / w)
	{
	  /* This index table cannot be right since it does not fit into
	     the file.  */
	  __libelf_seterrno (ELF_E_NO_INDEX);
	  goto out;
	}

      /* Now we can allocate the arrays needed to store the index.  */
      size_t ar_sym_len = (n + 1) * sizeof (Elf_Arsym);
      elf->state.ar.ar_sym = malloc (ar_sym_len);
      if (elf->state.ar.ar_sym != NULL)
	{
	  void *file_data; /* unit32_t[n] or uint64_t[n] */
	  char *str_data;
	  size_t sz = n * w;

	  if (elf->map_address == NULL)
	    {
	      temp_data = malloc (sz);
	      if (unlikely (temp_data == NULL))
		{
		  __libelf_seterrno (ELF_E_NOMEM);
		  goto out;
		}
	      file_data = temp_data;

	      ar_sym_len += index_size - n * w;
	      Elf_Arsym *newp = realloc (elf->state.ar.ar_sym, ar_sym_len);
	      if (newp == NULL)
		{
		  free (elf->state.ar.ar_sym);
		  elf->state.ar.ar_sym = NULL;
		  __libelf_seterrno (ELF_E_NOMEM);
		  goto out;
		}
	      elf->state.ar.ar_sym = newp;

	      char *new_str = (char *) (elf->state.ar.ar_sym + n + 1);

	      /* Now read the data from the file.  */
	      if ((size_t) pread_retry (elf->fildes, file_data, sz, off) != sz
		  || ((size_t) pread_retry (elf->fildes, new_str,
					    index_size - sz, off + sz)
		      != index_size - sz))
		{
		  /* We were not able to read the data.  */
		  free (elf->state.ar.ar_sym);
		  elf->state.ar.ar_sym = NULL;
		  __libelf_seterrno (ELF_E_NO_INDEX);
		  goto out;
		}

	      str_data = (char *) new_str;
	    }
	  else
	    {
	      file_data = (void *) (elf->map_address + off);
	      if (!ALLOW_UNALIGNED
		  && ((uintptr_t) file_data & -(uintptr_t) n) != 0)
		{
		  temp_data = malloc (sz);
		  if (unlikely (temp_data == NULL))
		    {
		      __libelf_seterrno (ELF_E_NOMEM);
		      goto out;
		    }
		  file_data = memcpy (temp_data, elf->map_address + off, sz);
		}
	      str_data = (char *) (elf->map_address + off + sz);
	    }

	  /* Now we can build the data structure.  */
	  Elf_Arsym *arsym = elf->state.ar.ar_sym;
	  uint64_t (*u64)[n] = file_data;
	  uint32_t (*u32)[n] = file_data;
	  for (size_t cnt = 0; cnt < n; ++cnt)
	    {
	      arsym[cnt].as_name = str_data;
	      if (index64_p)
		{
		  uint64_t tmp = (*u64)[cnt];
		  if (BYTE_ORDER == LITTLE_ENDIAN)
		    tmp = bswap_64 (tmp);

		  arsym[cnt].as_off = tmp;

		  /* Check whether 64-bit offset fits into 32-bit
		     size_t.  */
		  if (sizeof (arsym[cnt].as_off) < 8
		      && arsym[cnt].as_off != tmp)
		    {
		      if (elf->map_address == NULL)
			{
			  free (elf->state.ar.ar_sym);
			  elf->state.ar.ar_sym = NULL;
			}

		      __libelf_seterrno (ELF_E_RANGE);
		      goto out;
		    }
		}
	      else if (BYTE_ORDER == LITTLE_ENDIAN)
		arsym[cnt].as_off = bswap_32 ((*u32)[cnt]);
	      else
		arsym[cnt].as_off = (*u32)[cnt];

	      arsym[cnt].as_hash = _dl_elf_hash (str_data);
#if HAVE_DECL_RAWMEMCHR
	      str_data = rawmemchr (str_data, '\0') + 1;
#else
	      char c;
	      do {
		c = *str_data;
		str_data++;
	      } while (c);
#endif
	    }

	  /* At the end a special entry.  */
	  arsym[n].as_name = NULL;
	  arsym[n].as_off = 0;
	  arsym[n].as_hash = ~0UL;

	  /* Tell the caller how many entries we have.  */
	  elf->state.ar.ar_sym_num = n + 1;
	}

      result = elf->state.ar.ar_sym;

    out:
      free (temp_data);
      rwlock_unlock (elf->lock);
    }

  if (ptr != NULL)
    *ptr = elf->state.ar.ar_sym_num;

  return result;
}
