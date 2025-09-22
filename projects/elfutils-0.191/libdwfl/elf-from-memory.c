/* Reconstruct an ELF file by reading the segments out of remote memory.
   Copyright (C) 2005-2011, 2014, 2015 Red Hat, Inc.
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

#include <config.h>
#include "libelfP.h"

#include "libdwflP.h"

#include <gelf.h>
#include <sys/types.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Reconstruct an ELF file by reading the segments out of remote memory
   based on the ELF file header at EHDR_VMA and the ELF program headers it
   points to.  If not null, *LOADBASEP is filled in with the difference
   between the addresses from which the segments were read, and the
   addresses the file headers put them at.

   The function READ_MEMORY is called to copy at least MINREAD and at most
   MAXREAD bytes from the remote memory at target address ADDRESS into the
   local buffer at DATA; it should return -1 for errors (with code in
   `errno'), 0 if it failed to read at least MINREAD bytes due to EOF, or
   the number of bytes read if >= MINREAD.  ARG is passed through.

   PAGESIZE is the minimum page size and alignment used for the PT_LOAD
   segments.  */

Elf *
elf_from_remote_memory (GElf_Addr ehdr_vma,
			GElf_Xword pagesize,
			GElf_Addr *loadbasep,
			ssize_t (*read_memory) (void *arg, void *data,
						GElf_Addr address,
						size_t minread,
						size_t maxread),
			void *arg)
{
  /* We might have to reserve some memory for the phdrs.  Set to NULL
     here so we can always safely free it.  */
  void *phdrsp = NULL;

  /* First read in the file header and check its sanity.  */

  const size_t initial_bufsize = 256;
  unsigned char *buffer = malloc (initial_bufsize);
  if (unlikely (buffer == NULL))
    {
    no_memory:
      __libdwfl_seterrno (DWFL_E_NOMEM);
      return NULL;
    }

  ssize_t nread = (*read_memory) (arg, buffer, ehdr_vma,
				  sizeof (Elf32_Ehdr), initial_bufsize);
  if (nread <= 0)
    {
    read_error:
      free (buffer);
      free (phdrsp);
      __libdwfl_seterrno (nread < 0 ? DWFL_E_ERRNO : DWFL_E_TRUNCATED);
      return NULL;
    }

  if (memcmp (buffer, ELFMAG, SELFMAG) != 0)
    {
    bad_elf:
      free (buffer);
      free (phdrsp);
      __libdwfl_seterrno (DWFL_E_BADELF);
      return NULL;
    }

  /* Extract the information we need from the file header.  */

  union
  {
    Elf32_Ehdr e32;
    Elf64_Ehdr e64;
  } ehdr;
  Elf_Data xlatefrom =
    {
      .d_type = ELF_T_EHDR,
      .d_buf = buffer,
      .d_version = EV_CURRENT,
    };
  Elf_Data xlateto =
    {
      .d_type = ELF_T_EHDR,
      .d_buf = &ehdr,
      .d_size = sizeof ehdr,
      .d_version = EV_CURRENT,
    };

  GElf_Off phoff;
  uint_fast16_t phnum;
  uint_fast16_t phentsize;
  GElf_Off shdrs_end;

  switch (buffer[EI_CLASS])
    {
    case ELFCLASS32:
      xlatefrom.d_size = sizeof (Elf32_Ehdr);
      if (elf32_xlatetom (&xlateto, &xlatefrom, buffer[EI_DATA]) == NULL)
	{
	libelf_error:
	  __libdwfl_seterrno (DWFL_E_LIBELF);
	  return NULL;
	}
      phoff = ehdr.e32.e_phoff;
      phnum = ehdr.e32.e_phnum;
      phentsize = ehdr.e32.e_phentsize;
      if (phentsize != sizeof (Elf32_Phdr) || phnum == 0)
	goto bad_elf;
      /* NOTE if the number of sections is > 0xff00 then e_shnum
	 is zero and the actual number would come from the section
	 zero sh_size field. We ignore this here because getting shdrs
	 is just a nice bonus (see below where we trim the last phdrs
	 PT_LOAD segment).  */
      shdrs_end = ehdr.e32.e_shoff + ehdr.e32.e_shnum * ehdr.e32.e_shentsize;
      break;

    case ELFCLASS64:
      xlatefrom.d_size = sizeof (Elf64_Ehdr);
      if (elf64_xlatetom (&xlateto, &xlatefrom, buffer[EI_DATA]) == NULL)
	goto libelf_error;
      phoff = ehdr.e64.e_phoff;
      phnum = ehdr.e64.e_phnum;
      phentsize = ehdr.e64.e_phentsize;
      if (phentsize != sizeof (Elf64_Phdr) || phnum == 0)
	goto bad_elf;
      /* See the NOTE above for shdrs_end and ehdr.e32.e_shnum.  */
      shdrs_end = ehdr.e64.e_shoff + ehdr.e64.e_shnum * ehdr.e64.e_shentsize;
      break;

    default:
      goto bad_elf;
    }


  /* The file header tells where to find the program headers.
     These are what we use to actually choose what to read.  */

  xlatefrom.d_type = xlateto.d_type = ELF_T_PHDR;
  xlatefrom.d_size = phnum * phentsize;

  if ((size_t) nread >= phoff + phnum * phentsize)
    /* We already have all the phdrs from the initial read.  */
    xlatefrom.d_buf = buffer + phoff;
  else
    {
      /* Read in the program headers.  */

      if (initial_bufsize < (size_t)phnum * phentsize)
	{
	  unsigned char *newbuf = realloc (buffer, phnum * phentsize);
	  if (newbuf == NULL)
	    {
	      free (buffer);
	      free (phdrsp);
	      goto no_memory;
	    }
	  buffer = newbuf;
	}
      nread = (*read_memory) (arg, buffer, ehdr_vma + phoff,
			      phnum * phentsize, phnum * phentsize);
      if (nread <= 0)
	goto read_error;

      xlatefrom.d_buf = buffer;
    }

  bool class32 = ehdr.e32.e_ident[EI_CLASS] == ELFCLASS32;
  size_t phdr_size = class32 ? sizeof (Elf32_Phdr) : sizeof (Elf64_Phdr);
  if (unlikely (phnum > SIZE_MAX / phdr_size))
    {
      free (buffer);
      goto no_memory;
    }
  const size_t phdrsp_bytes = phnum * phdr_size;
  phdrsp = malloc (phdrsp_bytes);
  if (unlikely (phdrsp == NULL))
    {
      free (buffer);
      goto no_memory;
    }

  xlateto.d_buf = phdrsp;
  xlateto.d_size = phdrsp_bytes;

  /* Scan for PT_LOAD segments to find the total size of the file image.  */
  size_t contents_size = 0;
  GElf_Off segments_end = 0;
  GElf_Off segments_end_mem = 0;
  GElf_Addr loadbase = ehdr_vma;
  bool found_base = false;
  Elf32_Phdr (*p32)[phnum] = phdrsp;
  Elf64_Phdr (*p64)[phnum] = phdrsp;

  if (class32)
    {
      if (! elf32_xlatetom (&xlateto, &xlatefrom, ehdr.e32.e_ident[EI_DATA]))
        goto libelf_error;
    }
  else
    {
      if (! elf64_xlatetom (&xlateto, &xlatefrom, ehdr.e64.e_ident[EI_DATA]))
        goto libelf_error;
    }

  for (uint_fast16_t i = 0; i < phnum; ++i)
    {
      GElf_Word type = class32 ? (*p32)[i].p_type : (*p64)[i].p_type;

      if (type != PT_LOAD)
        continue;

      GElf_Addr vaddr = class32 ? (*p32)[i].p_vaddr : (*p64)[i].p_vaddr;
      GElf_Xword memsz = class32 ? (*p32)[i].p_memsz : (*p64)[i].p_memsz;
      GElf_Off offset = class32 ? (*p32)[i].p_offset : (*p64)[i].p_offset;
      GElf_Xword filesz = class32 ? (*p32)[i].p_filesz : (*p64)[i].p_filesz;

      /* Sanity check the segment load aligns with the pagesize.  */
      if (((vaddr - offset) & (pagesize - 1)) != 0)
        goto bad_elf;

      GElf_Off segment_end = ((offset + filesz + pagesize - 1)
                              & -pagesize);

      if (segment_end > (GElf_Off) contents_size)
        contents_size = segment_end;

      if (!found_base && (offset & -pagesize) == 0)
        {
          loadbase = ehdr_vma - (vaddr & -pagesize);
          found_base = true;
        }

      segments_end = offset + filesz;
      segments_end_mem = offset + memsz;
    }

  /* Trim the last segment so we don't bother with zeros in the last page
     that are off the end of the file.  However, if the extra bit in that
     page includes the section headers and the memory isn't extended (which
     might indicate it will have been reused otherwise), keep them.  */
  if ((GElf_Off) contents_size > segments_end
      && (GElf_Off) contents_size >= shdrs_end
      && segments_end == segments_end_mem)
    {
      contents_size = segments_end;
      if ((GElf_Off) contents_size < shdrs_end)
	contents_size = shdrs_end;
    }
  else
    contents_size = segments_end;

  free (buffer);

  /* Now we know the size of the whole image we want read in.  */
  buffer = calloc (1, contents_size);
  if (buffer == NULL)
    {
      free (phdrsp);
      goto no_memory;
    }

  for (uint_fast16_t i = 0; i < phnum; ++i)
    {
      GElf_Word type = class32 ? (*p32)[i].p_type : (*p64)[i].p_type;

      if (type != PT_LOAD)
        continue;

      GElf_Addr vaddr = class32 ? (*p32)[i].p_vaddr : (*p64)[i].p_vaddr;
      GElf_Off offset = class32 ? (*p32)[i].p_offset : (*p64)[i].p_offset;
      GElf_Xword filesz = class32 ? (*p32)[i].p_filesz : (*p64)[i].p_filesz;

      GElf_Off start = offset & -pagesize;
      GElf_Off end = (offset + filesz + pagesize - 1) & -pagesize;
      if (end > (GElf_Off) contents_size)
        end = contents_size;
      nread = (*read_memory) (arg, buffer + start,
                              (loadbase + vaddr) & -pagesize,
                              end - start, end - start);
      if (nread <= 0)
        goto read_error;
    }

  /* If the segments visible in memory didn't include the section
     headers, then clear them from the file header.  */
  if (contents_size < shdrs_end)
    {
      if (class32)
        {
          ehdr.e32.e_shoff = 0;
          ehdr.e32.e_shnum = 0;
          ehdr.e32.e_shstrndx = 0;
        }
      else
        {
          ehdr.e64.e_shoff = 0;
          ehdr.e64.e_shnum = 0;
          ehdr.e64.e_shstrndx = 0;
        }
    }

  /* This will normally have been in the first PT_LOAD segment.  But it
     conceivably could be missing, and we might have just changed it.  */
  xlatefrom.d_type = xlateto.d_type = ELF_T_EHDR;
  xlateto.d_buf = buffer;
  if (class32)
    {
      xlatefrom.d_size = xlateto.d_size = sizeof ehdr.e32;
      xlatefrom.d_buf = &ehdr.e32;
      if (elf32_xlatetof (&xlateto, &xlatefrom,
                          ehdr.e32.e_ident[EI_DATA]) == NULL)
        goto libelf_error;
    }
  else
    {
      xlatefrom.d_size = xlateto.d_size = sizeof ehdr.e64;
      xlatefrom.d_buf = &ehdr.e64;
      if (elf64_xlatetof (&xlateto, &xlatefrom,
                          ehdr.e64.e_ident[EI_DATA]) == NULL)
        goto libelf_error;
    }

  free (phdrsp);
  phdrsp = NULL;

  /* Now we have the image.  Open libelf on it.  */

  Elf *elf = elf_memory ((char *) buffer, contents_size);
  if (elf == NULL)
    {
      free (buffer);
      goto libelf_error;
    }

  elf->flags |= ELF_F_MALLOCED;
  if (loadbasep != NULL)
    *loadbasep = loadbase;
  return elf;
}
