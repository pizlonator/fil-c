/* Write changed data structures.
   Copyright (C) 2000-2010, 2014, 2015, 2016, 2018 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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
#include <libelf.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "libelfP.h"


#ifndef LIBELFBITS
# define LIBELFBITS 32
#endif


static int
compare_sections (const void *a, const void *b)
{
  const Elf_Scn **scna = (const Elf_Scn **) a;
  const Elf_Scn **scnb = (const Elf_Scn **) b;

  if ((*scna)->shdr.ELFW(e,LIBELFBITS)->sh_offset
      < (*scnb)->shdr.ELFW(e,LIBELFBITS)->sh_offset)
    return -1;

  if ((*scna)->shdr.ELFW(e,LIBELFBITS)->sh_offset
      > (*scnb)->shdr.ELFW(e,LIBELFBITS)->sh_offset)
    return 1;

  if ((*scna)->shdr.ELFW(e,LIBELFBITS)->sh_size
      < (*scnb)->shdr.ELFW(e,LIBELFBITS)->sh_size)
    return -1;

  if ((*scna)->shdr.ELFW(e,LIBELFBITS)->sh_size
      > (*scnb)->shdr.ELFW(e,LIBELFBITS)->sh_size)
    return 1;

  if ((*scna)->index < (*scnb)->index)
    return -1;

  if ((*scna)->index > (*scnb)->index)
    return 1;

  return 0;
}


/* Insert the sections in the list into the provided array and sort
   them according to their start offsets.  For sections with equal
   start offsets, the size is used; for sections with equal start
   offsets and sizes, the section index is used.  Sorting by size
   ensures that zero-length sections are processed first, which
   is what we want since they do not advance our file writing position.  */
static void
sort_sections (Elf_Scn **scns, Elf_ScnList *list)
{
  Elf_Scn **scnp = scns;
  do
    for (size_t cnt = 0; cnt < list->cnt; ++cnt)
      *scnp++ = &list->data[cnt];
  while ((list = list->next) != NULL);

  qsort (scns, scnp - scns, sizeof (*scns), compare_sections);
}


static inline void
fill_mmap (size_t offset, char *last_position, char *scn_start,
           char *const shdr_start, char *const shdr_end)
{
  size_t written = 0;

  if (last_position < shdr_start)
    {
      written = MIN (scn_start + offset - last_position,
                     shdr_start - last_position);

      memset (last_position, __libelf_fill_byte, written);
    }

  if (last_position + written != scn_start + offset
      && shdr_end < scn_start + offset)
    {
      char *fill_start = MAX (shdr_end, scn_start);
      memset (fill_start, __libelf_fill_byte,
              scn_start + offset - fill_start);
    }
}

int
internal_function
__elfw2(LIBELFBITS,updatemmap) (Elf *elf, int change_bo, size_t shnum)
{
  bool previous_scn_changed = false;

  /* We need the ELF header several times.  */
  ElfW2(LIBELFBITS,Ehdr) *ehdr = elf->state.ELFW(elf,LIBELFBITS).ehdr;

  /* Write out the ELF header.  */
  if ((elf->state.ELFW(elf,LIBELFBITS).ehdr_flags | elf->flags) & ELF_F_DIRTY)
    {
      /* If the type sizes should be different at some time we have to
	 rewrite this code.  */
      assert (sizeof (ElfW2(LIBELFBITS,Ehdr))
	      == elf_typesize (LIBELFBITS, ELF_T_EHDR, 1));

      if (unlikely (change_bo))
	{
	  /* Today there is only one version of the ELF header.  */
#undef fctp
#define fctp __elf_xfctstom[ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_EHDR]

	  /* Do the real work.  */
	  (*fctp) ((char *) elf->map_address + elf->start_offset, ehdr,
		   sizeof (ElfW2(LIBELFBITS,Ehdr)), 1);
	}
      else if (elf->map_address + elf->start_offset != ehdr)
	memcpy (elf->map_address + elf->start_offset, ehdr,
		sizeof (ElfW2(LIBELFBITS,Ehdr)));

      elf->state.ELFW(elf,LIBELFBITS).ehdr_flags &= ~ELF_F_DIRTY;

      /* We start writing sections after the ELF header only if there is
	 no program header.  */
      previous_scn_changed = elf->state.ELFW(elf,LIBELFBITS).phdr == NULL;
    }

  size_t phnum;
  if (unlikely (__elf_getphdrnum_rdlock (elf, &phnum) != 0))
    return -1;

  /* Write out the program header table.  */
  if (elf->state.ELFW(elf,LIBELFBITS).phdr != NULL
      && ((elf->state.ELFW(elf,LIBELFBITS).phdr_flags | elf->flags)
	  & ELF_F_DIRTY))
    {
      /* If the type sizes should be different at some time we have to
	 rewrite this code.  */
      assert (sizeof (ElfW2(LIBELFBITS,Phdr))
	      == elf_typesize (LIBELFBITS, ELF_T_PHDR, 1));

      /* Maybe the user wants a gap between the ELF header and the program
	 header.  */
      if (ehdr->e_phoff > ehdr->e_ehsize)
	memset (elf->map_address + elf->start_offset + ehdr->e_ehsize,
		__libelf_fill_byte, ehdr->e_phoff - ehdr->e_ehsize);

      if (unlikely (change_bo))
	{
	  /* Today there is only one version of the ELF header.  */
#undef fctp
#define fctp __elf_xfctstom[ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_PHDR]

	  /* Do the real work.  */
	  (*fctp) (elf->map_address + elf->start_offset + ehdr->e_phoff,
		   elf->state.ELFW(elf,LIBELFBITS).phdr,
		   sizeof (ElfW2(LIBELFBITS,Phdr)) * phnum, 1);
	}
      else
	memmove (elf->map_address + elf->start_offset + ehdr->e_phoff,
		elf->state.ELFW(elf,LIBELFBITS).phdr,
		sizeof (ElfW2(LIBELFBITS,Phdr)) * phnum);

      elf->state.ELFW(elf,LIBELFBITS).phdr_flags &= ~ELF_F_DIRTY;

      /* We modified the program header.  Maybe this created a gap so
	 we have to write fill bytes, if necessary.  */
      previous_scn_changed = true;
    }

  /* From now on we have to keep track of the last position to eventually
     fill the gaps with the prescribed fill byte.  */
  char *last_position = ((char *) elf->map_address + elf->start_offset
			 + MAX (elf_typesize (LIBELFBITS, ELF_T_EHDR, 1),
				ehdr->e_phoff)
			 + elf_typesize (LIBELFBITS, ELF_T_PHDR, phnum));

  /* Write all the sections.  Well, only those which are modified.  */
  if (shnum > 0)
    {
      if (unlikely (shnum > SIZE_MAX / sizeof (Elf_Scn *)))
	return 1;

      Elf_ScnList *list = &elf->state.ELFW(elf,LIBELFBITS).scns;
      Elf_Scn **scns = malloc (shnum * sizeof (Elf_Scn *));
      if (unlikely (scns == NULL))
	{
	  __libelf_seterrno (ELF_E_NOMEM);
	  return -1;
	}
      char *const shdr_start = ((char *) elf->map_address + elf->start_offset
				+ ehdr->e_shoff);
      char *const shdr_end = shdr_start + shnum * ehdr->e_shentsize;

#undef shdr_fctp
#define shdr_fctp __elf_xfctstom[ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_SHDR]
#define shdr_dest ((ElfW2(LIBELFBITS,Shdr) *) shdr_start)

      /* Get all sections into the array and sort them.  */
      sort_sections (scns, list);

      /* We possibly have to copy the section header data because moving
	 the sections might overwrite the data.  */
      for (size_t cnt = 0; cnt < shnum; ++cnt)
	{
	  Elf_Scn *scn = scns[cnt];

	  if (!elf->state.ELFW(elf,LIBELFBITS).shdr_malloced
	      && (scn->shdr_flags & ELF_F_MALLOCED) == 0
	      && scn->shdr.ELFW(e,LIBELFBITS) != &shdr_dest[scn->index])
	    {
	      assert ((char *) elf->map_address + elf->start_offset
		      < (char *) scn->shdr.ELFW(e,LIBELFBITS));
	      assert ((char *) scn->shdr.ELFW(e,LIBELFBITS)
		      < ((char *) elf->map_address + elf->start_offset
			 + elf->maximum_size));

	      void *p = malloc (sizeof (ElfW2(LIBELFBITS,Shdr)));
	      if (unlikely (p == NULL))
		{
		  free (scns);
		  __libelf_seterrno (ELF_E_NOMEM);
		  return -1;
		}
	      scn->shdr.ELFW(e,LIBELFBITS)
		= memcpy (p, scn->shdr.ELFW(e,LIBELFBITS),
			  sizeof (ElfW2(LIBELFBITS,Shdr)));
	    }

	  /* If the file is mmaped and the original position of the
	     section in the file is lower than the new position we
	     need to save the section content since otherwise it is
	     overwritten before it can be copied.  If there are
	     multiple data segments in the list only the first can be
	     from the file.  */
	  if (((char *) elf->map_address + elf->start_offset
	       <= (char  *) scn->data_list.data.d.d_buf)
	      && ((char *) scn->data_list.data.d.d_buf
		  < ((char *) elf->map_address + elf->start_offset
		     + elf->maximum_size))
	      && (((char *) elf->map_address + elf->start_offset
		   + scn->shdr.ELFW(e,LIBELFBITS)->sh_offset)
		  > (char *) scn->data_list.data.d.d_buf))
	    {
	      void *p = malloc (scn->data_list.data.d.d_size);
	      if (unlikely (p == NULL))
		{
		  free (scns);
		  __libelf_seterrno (ELF_E_NOMEM);
		  return -1;
		}
	      scn->data_list.data.d.d_buf = scn->data_base
		= memcpy (p, scn->data_list.data.d.d_buf,
			  scn->data_list.data.d.d_size);
	    }
	}

      /* Iterate over all the section in the order in which they
	 appear in the output file.  */
      for (size_t cnt = 0; cnt < shnum; ++cnt)
	{
	  Elf_Scn *scn = scns[cnt];
	  if (scn->index == 0)
	    {
	      /* The dummy section header entry.  It should not be
		 possible to mark this "section" as dirty.  */
	      assert ((scn->flags & ELF_F_DIRTY) == 0);
	      continue;
	    }

	  ElfW2(LIBELFBITS,Shdr) *shdr = scn->shdr.ELFW(e,LIBELFBITS);
	  if (shdr->sh_type == SHT_NOBITS)
	    goto next;

	  char *scn_start = ((char *) elf->map_address
			     + elf->start_offset + shdr->sh_offset);
	  Elf_Data_List *dl = &scn->data_list;
	  bool scn_changed = false;

	  if (scn->data_list_rear != NULL)
	    do
	      {
		assert (dl->data.d.d_off >= 0);
		assert ((GElf_Off) dl->data.d.d_off <= shdr->sh_size);
		assert (dl->data.d.d_size <= (shdr->sh_size
					      - (GElf_Off) dl->data.d.d_off));

		/* If there is a gap, fill it.  */
		if (scn_start + dl->data.d.d_off > last_position
		    && (dl->data.d.d_off == 0
			|| ((scn->flags | dl->flags | elf->flags)
			    & ELF_F_DIRTY) != 0))
		  {
		    fill_mmap (dl->data.d.d_off, last_position, scn_start,
		               shdr_start, shdr_end);
		  }

		last_position = scn_start + dl->data.d.d_off;

		if ((scn->flags | dl->flags | elf->flags) & ELF_F_DIRTY)
		  {
		    /* Let it go backward if the sections use a bogus
		       layout with overlaps.  We'll overwrite the stupid
		       user's section data with the latest one, rather than
		       crashing.  */

		    if (unlikely (change_bo
				  && dl->data.d.d_size != 0
				  && dl->data.d.d_type != ELF_T_BYTE))
		      {
#undef fctp
#define fctp __elf_xfctstom[ELFW(ELFCLASS, LIBELFBITS) - 1][dl->data.d.d_type]

			size_t align;
			align = __libelf_type_align (ELFW(ELFCLASS,LIBELFBITS),
						     dl->data.d.d_type);
			if ((((uintptr_t) last_position)
			     & (uintptr_t) (align - 1)) == 0)
			  {
			    /* No need to copy, we can convert directly.  */
			    (*fctp) (last_position, dl->data.d.d_buf,
				     dl->data.d.d_size, 1);
			  }
			else
			  {
			    /* We have to do the conversion on properly
			       aligned memory first.  align is a power of 2,
			       but posix_memalign only works for alignments
			       which are a multiple of sizeof (void *).
			       So use normal malloc for smaller alignments.  */
			    size_t size = dl->data.d.d_size;
			    void *converted;
			    if (align < sizeof (void *))
			      converted = malloc (size);
			    else
			      {
				int res;
				res = posix_memalign (&converted, align, size);
				if (res != 0)
				  converted = NULL;
			      }

			    if (converted == NULL)
			      {
				free (scns);
				__libelf_seterrno (ELF_E_NOMEM);
				return 1;
			      }

			    (*fctp) (converted, dl->data.d.d_buf, size, 1);

			    /* And then write it to the mmapped file.  */
			    memcpy (last_position, converted, size);
			    free (converted);
			  }

			last_position += dl->data.d.d_size;
		      }
		    else if (dl->data.d.d_size != 0)
		      {
			memmove (last_position, dl->data.d.d_buf,
				 dl->data.d.d_size);
			last_position += dl->data.d.d_size;
		      }

		    scn_changed = true;
		  }
		else
		  last_position += dl->data.d.d_size;

		assert (scn_start + dl->data.d.d_off + dl->data.d.d_size
			== last_position);

		dl->flags &= ~ELF_F_DIRTY;

		dl = dl->next;
	      }
	    while (dl != NULL);
	  else
	    {
	      /* If the previous section (or the ELF/program
		 header) changed we might have to fill the gap.  */
	      if (scn_start > last_position && previous_scn_changed)
		fill_mmap (0, last_position, scn_start,
		           shdr_start, shdr_end);

	      /* We have to trust the existing section header information.  */
	      last_position = scn_start + shdr->sh_size;
	    }


	  previous_scn_changed = scn_changed;
	next:
	  scn->flags &= ~ELF_F_DIRTY;
	}

      /* Fill the gap between last section and section header table if
	 necessary.  */
      if ((elf->flags & ELF_F_DIRTY)
	  && last_position < ((char *) elf->map_address + elf->start_offset
			      + ehdr->e_shoff))
	memset (last_position, __libelf_fill_byte,
		(char *) elf->map_address + elf->start_offset + ehdr->e_shoff
		- last_position);

      /* Write the section header table entry if necessary.  */
      for (size_t cnt = 0; cnt < shnum; ++cnt)
	{
	  Elf_Scn *scn = scns[cnt];

	  if ((scn->shdr_flags | elf->flags) & ELF_F_DIRTY)
	    {
	      if (unlikely (change_bo))
		(*shdr_fctp) (&shdr_dest[scn->index],
			      scn->shdr.ELFW(e,LIBELFBITS),
			      sizeof (ElfW2(LIBELFBITS,Shdr)), 1);
	      else
		memcpy (&shdr_dest[scn->index],
			scn->shdr.ELFW(e,LIBELFBITS),
			sizeof (ElfW2(LIBELFBITS,Shdr)));

	      /* If we previously made a copy of the section header
		 entry we now have to adjust the pointer again so
		 point to new place in the mapping.  */
	      if (!elf->state.ELFW(elf,LIBELFBITS).shdr_malloced
		  && (scn->shdr_flags & ELF_F_MALLOCED) == 0
		  && scn->shdr.ELFW(e,LIBELFBITS) != &shdr_dest[scn->index])
		{
		  free (scn->shdr.ELFW(e,LIBELFBITS));
		  scn->shdr.ELFW(e,LIBELFBITS) = &shdr_dest[scn->index];
		}

	      scn->shdr_flags &= ~ELF_F_DIRTY;
	    }
	}
      free (scns);
    }

  /* That was the last part.  Clear the overall flag.  */
  elf->flags &= ~ELF_F_DIRTY;

  /* Make sure the content hits the disk.  */
  char *msync_start = ((char *) elf->map_address
		       + (elf->start_offset & ~(sysconf (_SC_PAGESIZE) - 1)));
  char *msync_end = ((char *) elf->map_address
		     + elf->start_offset + ehdr->e_shoff
		     + ehdr->e_shentsize * shnum);
  (void) msync (msync_start, msync_end - msync_start, MS_SYNC);

  return 0;
}


/* Size of the buffer we use to generate the blocks of fill bytes.  */
#define FILLBUFSIZE	4096

/* If we have to convert the section buffer contents we have to use
   temporary buffer.  Only buffers up to MAX_TMPBUF bytes are allocated
   on the stack.  */
#define MAX_TMPBUF	32768


/* Helper function to write out fill bytes.  */
static int
fill (int fd, int64_t pos, size_t len, char *fillbuf, size_t *filledp)
{
  size_t filled = *filledp;
  size_t fill_len = MIN (len, FILLBUFSIZE);

  if (unlikely (fill_len > filled) && filled < FILLBUFSIZE)
    {
      /* Initialize a few more bytes.  */
      memset (fillbuf + filled, __libelf_fill_byte, fill_len - filled);
      *filledp = filled = fill_len;
    }

  do
    {
      /* This many bytes we want to write in this round.  */
      size_t n = MIN (filled, len);

      if (unlikely ((size_t) pwrite_retry (fd, fillbuf, n, pos) != n))
	{
	  __libelf_seterrno (ELF_E_WRITE_ERROR);
	  return 1;
	}

      pos += n;
      len -= n;
    }
  while (len > 0);

  return 0;
}


int
internal_function
__elfw2(LIBELFBITS,updatefile) (Elf *elf, int change_bo, size_t shnum)
{
  char fillbuf[FILLBUFSIZE];
  size_t filled = 0;
  bool previous_scn_changed = false;

  /* We need the ELF header several times.  */
  ElfW2(LIBELFBITS,Ehdr) *ehdr = elf->state.ELFW(elf,LIBELFBITS).ehdr;

  /* Write out the ELF header.  */
  if ((elf->state.ELFW(elf,LIBELFBITS).ehdr_flags | elf->flags) & ELF_F_DIRTY)
    {
      ElfW2(LIBELFBITS,Ehdr) tmp_ehdr;
      ElfW2(LIBELFBITS,Ehdr) *out_ehdr = ehdr;

      /* If the type sizes should be different at some time we have to
	 rewrite this code.  */
      assert (sizeof (ElfW2(LIBELFBITS,Ehdr))
	      == elf_typesize (LIBELFBITS, ELF_T_EHDR, 1));

      if (unlikely (change_bo))
	{
	  /* Today there is only one version of the ELF header.  */
#undef fctp
#define fctp __elf_xfctstom[ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_EHDR]

	  /* Write the converted ELF header in a temporary buffer.  */
	  (*fctp) (&tmp_ehdr, ehdr, sizeof (ElfW2(LIBELFBITS,Ehdr)), 1);

	  /* This is the buffer we want to write.  */
	  out_ehdr = &tmp_ehdr;
	}

      /* Write out the ELF header.  */
      if (unlikely (pwrite_retry (elf->fildes, out_ehdr,
				  sizeof (ElfW2(LIBELFBITS,Ehdr)), 0)
		    != sizeof (ElfW2(LIBELFBITS,Ehdr))))
	{
	  __libelf_seterrno (ELF_E_WRITE_ERROR);
	  return 1;
	}

      elf->state.ELFW(elf,LIBELFBITS).ehdr_flags &= ~ELF_F_DIRTY;

      /* We start writing sections after the ELF header only if there is
	 no program header.  */
      previous_scn_changed = elf->state.ELFW(elf,LIBELFBITS).phdr == NULL;
    }

  /* If the type sizes should be different at some time we have to
     rewrite this code.  */
  assert (sizeof (ElfW2(LIBELFBITS,Phdr))
	  == elf_typesize (LIBELFBITS, ELF_T_PHDR, 1));

  size_t phnum;
  if (unlikely (__elf_getphdrnum_rdlock (elf, &phnum) != 0))
    return -1;

  /* Write out the program header table.  */
  if (elf->state.ELFW(elf,LIBELFBITS).phdr != NULL
      && ((elf->state.ELFW(elf,LIBELFBITS).phdr_flags | elf->flags)
	  & ELF_F_DIRTY))
    {
      ElfW2(LIBELFBITS,Phdr) *tmp_phdr = NULL;
      ElfW2(LIBELFBITS,Phdr) *out_phdr = elf->state.ELFW(elf,LIBELFBITS).phdr;

      /* Maybe the user wants a gap between the ELF header and the program
	 header.  */
      if (ehdr->e_phoff > ehdr->e_ehsize
	  && unlikely (fill (elf->fildes, ehdr->e_ehsize,
			     ehdr->e_phoff - ehdr->e_ehsize, fillbuf, &filled)
		       != 0))
	return 1;

      if (unlikely (change_bo))
	{
	  /* Today there is only one version of the ELF header.  */
#undef fctp
#define fctp __elf_xfctstom[ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_PHDR]

	  /* Allocate sufficient memory.  */
	  tmp_phdr = (ElfW2(LIBELFBITS,Phdr) *)
	    malloc (sizeof (ElfW2(LIBELFBITS,Phdr)) * phnum);
	  if (unlikely (tmp_phdr == NULL))
	    {
	      __libelf_seterrno (ELF_E_NOMEM);
	      return 1;
	    }

	  /* Write the converted ELF header in a temporary buffer.  */
	  (*fctp) (tmp_phdr, elf->state.ELFW(elf,LIBELFBITS).phdr,
		   sizeof (ElfW2(LIBELFBITS,Phdr)) * phnum, 1);

	  /* This is the buffer we want to write.  */
	  out_phdr = tmp_phdr;
	}

      /* Write out the ELF header.  */
      size_t phdr_size = sizeof (ElfW2(LIBELFBITS,Phdr)) * phnum;
      if (unlikely ((size_t) pwrite_retry (elf->fildes, out_phdr,
					   phdr_size, ehdr->e_phoff)
		    != phdr_size))
	{
	  __libelf_seterrno (ELF_E_WRITE_ERROR);
	  return 1;
	}

      /* This is a no-op we we have not allocated any memory.  */
      free (tmp_phdr);

      elf->state.ELFW(elf,LIBELFBITS).phdr_flags &= ~ELF_F_DIRTY;

      /* We modified the program header.  Maybe this created a gap so
	 we have to write fill bytes, if necessary.  */
      previous_scn_changed = true;
    }

  /* From now on we have to keep track of the last position to eventually
     fill the gaps with the prescribed fill byte.  */
  int64_t last_offset;
  if (elf->state.ELFW(elf,LIBELFBITS).phdr == NULL)
    last_offset = elf_typesize (LIBELFBITS, ELF_T_EHDR, 1);
  else
    last_offset = (ehdr->e_phoff + sizeof (ElfW2(LIBELFBITS,Phdr)) * phnum);

  /* Write all the sections.  Well, only those which are modified.  */
  if (shnum > 0)
    {
      if (unlikely (shnum > SIZE_MAX / (sizeof (Elf_Scn *)
					+ sizeof (ElfW2(LIBELFBITS,Shdr)))))
	return 1;

      int64_t shdr_offset = elf->start_offset + ehdr->e_shoff;
#undef shdr_fctp
#define shdr_fctp __elf_xfctstom[ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_SHDR]

      ElfW2(LIBELFBITS,Shdr) *shdr_data;
      ElfW2(LIBELFBITS,Shdr) *shdr_data_mem = NULL;
      if (change_bo || elf->state.ELFW(elf,LIBELFBITS).shdr == NULL
	  || (elf->flags & ELF_F_DIRTY))
	{
	  shdr_data_mem = (ElfW2(LIBELFBITS,Shdr) *)
	    malloc (shnum * sizeof (ElfW2(LIBELFBITS,Shdr)));
	  if (unlikely (shdr_data_mem == NULL))
	    {
	      __libelf_seterrno (ELF_E_NOMEM);
	      return -1;
	    }
	  shdr_data = shdr_data_mem;
	}
      else
	shdr_data = elf->state.ELFW(elf,LIBELFBITS).shdr;
      int shdr_flags = elf->flags;

      /* Get all sections into the array and sort them.  */
      Elf_ScnList *list = &elf->state.ELFW(elf,LIBELFBITS).scns;
      Elf_Scn **scns = malloc (shnum * sizeof (Elf_Scn *));
      if (unlikely (scns == NULL))
	{
	  free (shdr_data_mem);
	  __libelf_seterrno (ELF_E_NOMEM);
	  return -1;
	}
      sort_sections (scns, list);

      for (size_t cnt = 0; cnt < shnum; ++cnt)
	{
	  Elf_Scn *scn = scns[cnt];
	  if (scn->index == 0)
	    {
	      /* The dummy section header entry.  It should not be
		 possible to mark this "section" as dirty.  */
	      assert ((scn->flags & ELF_F_DIRTY) == 0);
	      goto next;
	    }

	  ElfW2(LIBELFBITS,Shdr) *shdr = scn->shdr.ELFW(e,LIBELFBITS);
	  if (shdr->sh_type == SHT_NOBITS)
	    goto next;

	  int64_t scn_start = elf->start_offset + shdr->sh_offset;
	  Elf_Data_List *dl = &scn->data_list;
	  bool scn_changed = false;

	  if (scn->data_list_rear != NULL)
	    do
	      {
		/* If there is a gap, fill it.  */
		if (scn_start + dl->data.d.d_off > last_offset
		    && ((previous_scn_changed && dl->data.d.d_off == 0)
			|| ((scn->flags | dl->flags | elf->flags)
			    & ELF_F_DIRTY) != 0))
		  {
		    if (unlikely (fill (elf->fildes, last_offset,
					(scn_start + dl->data.d.d_off)
					- last_offset, fillbuf,
					&filled) != 0))
		      {
		      fail_free:
			free (shdr_data_mem);
			free (scns);
			return 1;
		      }
		  }

		last_offset = scn_start + dl->data.d.d_off;

		if ((scn->flags | dl->flags | elf->flags) & ELF_F_DIRTY)
		  {
		    char tmpbuf[MAX_TMPBUF];
		    void *buf = dl->data.d.d_buf;

		    /* Let it go backward if the sections use a bogus
		       layout with overlaps.  We'll overwrite the stupid
		       user's section data with the latest one, rather than
		       crashing.  */

		    if (unlikely (change_bo))
		      {
#undef fctp
#define fctp __elf_xfctstom[ELFW(ELFCLASS, LIBELFBITS) - 1][dl->data.d.d_type]

			buf = tmpbuf;
			if (dl->data.d.d_size > MAX_TMPBUF)
			  {
			    buf = malloc (dl->data.d.d_size);
			    if (unlikely (buf == NULL))
			      {
				__libelf_seterrno (ELF_E_NOMEM);
				goto fail_free;
			      }
			  }

			/* Do the real work.  */
			(*fctp) (buf, dl->data.d.d_buf, dl->data.d.d_size, 1);
		      }

		    ssize_t n = pwrite_retry (elf->fildes, buf,
					      dl->data.d.d_size,
					      last_offset);
		    if (unlikely ((size_t) n != dl->data.d.d_size))
		      {
			if (buf != dl->data.d.d_buf && buf != tmpbuf)
			  free (buf);

			__libelf_seterrno (ELF_E_WRITE_ERROR);
			goto fail_free;
		      }

		    if (buf != dl->data.d.d_buf && buf != tmpbuf)
		      free (buf);

		    scn_changed = true;
		  }

		last_offset += dl->data.d.d_size;

		dl->flags &= ~ELF_F_DIRTY;

		dl = dl->next;
	      }
	    while (dl != NULL);
	  else
	    {
	      /* If the previous section (or the ELF/program
		 header) changed we might have to fill the gap.  */
	      if (scn_start > last_offset && previous_scn_changed)
		{
		  if (unlikely (fill (elf->fildes, last_offset,
				      scn_start - last_offset, fillbuf,
				      &filled) != 0))
		    goto fail_free;
		}

	      last_offset = scn_start + shdr->sh_size;
	    }

	  previous_scn_changed = scn_changed;
	next:
	  /* Collect the section header table information.  */
	  if (unlikely (change_bo))
	    (*shdr_fctp) (&shdr_data[scn->index],
			  scn->shdr.ELFW(e,LIBELFBITS),
			  sizeof (ElfW2(LIBELFBITS,Shdr)), 1);
	  else if (elf->state.ELFW(elf,LIBELFBITS).shdr == NULL
		   || (elf->flags & ELF_F_DIRTY))
	    memcpy (&shdr_data[scn->index], scn->shdr.ELFW(e,LIBELFBITS),
		    sizeof (ElfW2(LIBELFBITS,Shdr)));

	  shdr_flags |= scn->shdr_flags;
	  scn->shdr_flags &= ~ELF_F_DIRTY;
	}

      /* Fill the gap between last section and section header table if
	 necessary.  */
      if ((elf->flags & ELF_F_DIRTY) && last_offset < shdr_offset
	  && unlikely (fill (elf->fildes, last_offset,
			     shdr_offset - last_offset,
			     fillbuf, &filled) != 0))
	goto fail_free;

      /* Write out the section header table.  */
      if (shdr_flags & ELF_F_DIRTY
	  && unlikely ((size_t) pwrite_retry (elf->fildes, shdr_data,
					      sizeof (ElfW2(LIBELFBITS,Shdr))
					      * shnum, shdr_offset)
		       != sizeof (ElfW2(LIBELFBITS,Shdr)) * shnum))
	{
	  __libelf_seterrno (ELF_E_WRITE_ERROR);
	  goto fail_free;
	}

      free (shdr_data_mem);
      free (scns);
    }

  /* That was the last part.  Clear the overall flag.  */
  elf->flags &= ~ELF_F_DIRTY;

  return 0;
}
