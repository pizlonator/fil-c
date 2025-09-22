/* Update data structures for changes.
   Copyright (C) 2000-2010, 2015, 2016 Red Hat, Inc.
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
#include <libelf.h>
#include <stdbool.h>
#include <string.h>

#include "libelfP.h"
#include "elf-knowledge.h"

#ifndef LIBELFBITS
# define LIBELFBITS 32
#endif

/* Some fields contain 32/64 sizes.  We cannot use Elf32/64_Word for those,
   since those are both 32bits.  Elf32/64_Xword is always 64bits.  */
#define Elf32_SizeWord Elf32_Word
#define Elf64_SizeWord Elf64_Xword


static int
ELFW(default_ehdr,LIBELFBITS) (Elf *elf, ElfW2(LIBELFBITS,Ehdr) *ehdr,
			       size_t shnum, int *change_bop)
{
  /* Always write the magic bytes.  */
  if (memcmp (&ehdr->e_ident[EI_MAG0], ELFMAG, SELFMAG) != 0)
    {
      memcpy (&ehdr->e_ident[EI_MAG0], ELFMAG, SELFMAG);
      elf->state.ELFW(elf,LIBELFBITS).ehdr_flags |= ELF_F_DIRTY;
    }

  /* Always set the file class.  */
  update_if_changed (ehdr->e_ident[EI_CLASS], ELFW(ELFCLASS,LIBELFBITS),
		     elf->state.ELFW(elf,LIBELFBITS).ehdr_flags);

  /* Set the data encoding if necessary.  */
  if (unlikely (ehdr->e_ident[EI_DATA] == ELFDATANONE))
    {
      ehdr->e_ident[EI_DATA] =
	BYTE_ORDER == BIG_ENDIAN ? ELFDATA2MSB : ELFDATA2LSB;
      elf->state.ELFW(elf,LIBELFBITS).ehdr_flags |= ELF_F_DIRTY;
    }
  else if (unlikely (ehdr->e_ident[EI_DATA] >= ELFDATANUM))
    {
      __libelf_seterrno (ELF_E_DATA_ENCODING);
      return 1;
    }
  else
    *change_bop = ((BYTE_ORDER == LITTLE_ENDIAN
		    && ehdr->e_ident[EI_DATA] != ELFDATA2LSB)
		   || (BYTE_ORDER == BIG_ENDIAN
		       && ehdr->e_ident[EI_DATA] != ELFDATA2MSB));

  /* Unconditionally overwrite the ELF version.  */
  update_if_changed (ehdr->e_ident[EI_VERSION], EV_CURRENT,
		     elf->state.ELFW(elf,LIBELFBITS).ehdr_flags);

  if (unlikely (ehdr->e_version == EV_NONE))
    {
      ehdr->e_version = EV_CURRENT;
      elf->state.ELFW(elf,LIBELFBITS).ehdr_flags |= ELF_F_DIRTY;
    }
  else if (unlikely (ehdr->e_version != EV_CURRENT))
    {
      __libelf_seterrno (ELF_E_UNKNOWN_VERSION);
      return 1;
    }

  if (unlikely (shnum >= SHN_LORESERVE))
    {
      update_if_changed (ehdr->e_shnum, 0,
			 elf->state.ELFW(elf,LIBELFBITS).ehdr_flags);
    }
  else
    update_if_changed (ehdr->e_shnum, shnum,
		       elf->state.ELFW(elf,LIBELFBITS).ehdr_flags);

  if (unlikely (ehdr->e_ehsize != elf_typesize (LIBELFBITS, ELF_T_EHDR, 1)))
    {
      ehdr->e_ehsize = elf_typesize (LIBELFBITS, ELF_T_EHDR, 1);
      elf->state.ELFW(elf,LIBELFBITS).ehdr_flags |= ELF_F_DIRTY;
    }

  /* If phnum is zero make sure e_phoff is also zero and not some random
     value.  That would cause trouble in update_file.  */
  if (ehdr->e_phnum == 0 && ehdr->e_phoff != 0)
    {
      ehdr->e_phoff = 0;
      elf->state.ELFW(elf,LIBELFBITS).ehdr_flags |= ELF_F_DIRTY;
    }

  return 0;
}


int64_t
internal_function
__elfw2(LIBELFBITS,updatenull_wrlock) (Elf *elf, int *change_bop, size_t shnum)
{
  ElfW2(LIBELFBITS,Ehdr) *ehdr;
  int changed = 0;
  int ehdr_flags = 0;

  ehdr = __elfw2(LIBELFBITS,getehdr_wrlock) (elf);

  /* Set the default values.  */
  if (ELFW(default_ehdr,LIBELFBITS) (elf, ehdr, shnum, change_bop) != 0)
    return -1;

  /* At least the ELF header is there.  */
  ElfW2(LIBELFBITS,SizeWord) size = elf_typesize (LIBELFBITS, ELF_T_EHDR, 1);

  /* Set the program header position.  */
  if (elf->state.ELFW(elf,LIBELFBITS).phdr == NULL)
    (void) __elfw2(LIBELFBITS,getphdr_wrlock) (elf);
  if (elf->state.ELFW(elf,LIBELFBITS).phdr != NULL)
    {
      size_t phnum;
      if (unlikely (__elf_getphdrnum_rdlock (elf, &phnum) != 0))
	return -1;

      if (elf->flags & ELF_F_LAYOUT)
	{
	  /* The user is supposed to fill out e_phoff.  Use it and
	     e_phnum to determine the maximum extend.  */
	  size = MAX (size,
		      ehdr->e_phoff
		      + elf_typesize (LIBELFBITS, ELF_T_PHDR, phnum));
	}
      else
	{
	  update_if_changed (ehdr->e_phoff,
			     elf_typesize (LIBELFBITS, ELF_T_EHDR, 1),
			     ehdr_flags);

	  /* We need no alignment here.  */
	  size += elf_typesize (LIBELFBITS, ELF_T_PHDR, phnum);
	}
    }

  if (shnum > 0)
    {
      struct Elf_Scn *scn1 = NULL;
      Elf_ScnList *list;
      bool first = true;

      assert (elf->state.ELFW(elf,LIBELFBITS).scns.cnt > 0);

      if (shnum >= SHN_LORESERVE)
	{
	  /* We have to  fill in the number of sections in the header
	     of the zeroth section.  */
	  Elf_Scn *scn0 = &elf->state.ELFW(elf,LIBELFBITS).scns.data[0];

	  update_if_changed (scn0->shdr.ELFW(e,LIBELFBITS)->sh_size,
			     shnum, scn0->shdr_flags);
	}

      /* Go over all sections and find out how large they are.  */
      list = &elf->state.ELFW(elf,LIBELFBITS).scns;

      /* Find the first section. */
      if (list->cnt > 1)
	scn1 = &list->data[1];
      else if (list->next != NULL)
	scn1 = &list->next->data[0];

      /* Load the section headers if necessary.  This loads the
	 headers for all sections.  */
      if (scn1 != NULL && scn1->shdr.ELFW(e,LIBELFBITS) == NULL)
	(void) __elfw2(LIBELFBITS,getshdr_wrlock) (scn1);

      do
	{
	  for (size_t cnt = first == true; cnt < list->cnt; ++cnt)
	    {
	      Elf_Scn *scn = &list->data[cnt];
	      ElfW2(LIBELFBITS,Shdr) *shdr = scn->shdr.ELFW(e,LIBELFBITS);
	      int64_t offset = 0;

	      assert (shdr != NULL);
	      ElfW2(LIBELFBITS,SizeWord) sh_entsize = shdr->sh_entsize;
	      ElfW2(LIBELFBITS,SizeWord) sh_align = shdr->sh_addralign ?: 1;
	      if (unlikely (! powerof2 (sh_align)))
		{
		  __libelf_seterrno (ELF_E_INVALID_ALIGN);
		  return -1;
		}

	      /* Set the sh_entsize value if we can reliably detect it.  */
	      switch (shdr->sh_type)
		{
		case SHT_SYMTAB:
		  sh_entsize = elf_typesize (LIBELFBITS, ELF_T_SYM, 1);
		  break;
		case SHT_RELA:
		  sh_entsize = elf_typesize (LIBELFBITS, ELF_T_RELA, 1);
		  break;
		case SHT_GROUP:
		  /* Only relocatable files can contain section groups.  */
		  if (ehdr->e_type != ET_REL)
		    {
		      __libelf_seterrno (ELF_E_GROUP_NOT_REL);
		      return -1;
		    }
		  FALLTHROUGH;
		case SHT_SYMTAB_SHNDX:
		  sh_entsize = elf_typesize (32, ELF_T_WORD, 1);
		  break;
		case SHT_HASH:
		  sh_entsize = SH_ENTSIZE_HASH (ehdr);
		  break;
		case SHT_DYNAMIC:
		  sh_entsize = elf_typesize (LIBELFBITS, ELF_T_DYN, 1);
		  break;
		case SHT_REL:
		  sh_entsize = elf_typesize (LIBELFBITS, ELF_T_REL, 1);
		  break;
		case SHT_DYNSYM:
		  sh_entsize = elf_typesize (LIBELFBITS, ELF_T_SYM, 1);
		  break;
		case SHT_SUNW_move:
		  sh_entsize = elf_typesize (LIBELFBITS, ELF_T_MOVE, 1);
		  break;
		case SHT_SUNW_syminfo:
		  sh_entsize = elf_typesize (LIBELFBITS, ELF_T_SYMINFO, 1);
		  break;
		case SHT_RELR:
		  sh_entsize = elf_typesize (LIBELFBITS, ELF_T_RELR, 1);
		  break;
		default:
		  break;
		}

	      /* If the section header contained the wrong entry size
		 correct it and mark the header as modified.  */
	      update_if_changed (shdr->sh_entsize, sh_entsize,
				 scn->shdr_flags);

	      /* Likewise for the alignment of a compressed section.
	         For a SHF_COMPRESSED section set the correct
	         sh_addralign value, which must match the d_align of
	         the data (see __libelf_set_rawdata in elf_getdata.c).  */
	      if ((shdr->sh_flags & SHF_COMPRESSED) != 0)
		{
		  sh_align = __libelf_type_align (ELFW(ELFCLASS,LIBELFBITS),
						  ELF_T_CHDR);
		  update_if_changed (shdr->sh_addralign, sh_align,
				     scn->shdr_flags);
		}

	      if (scn->data_read == 0
		  && __libelf_set_rawdata_wrlock (scn) != 0)
		/* Something went wrong.  The error value is already set.  */
		return -1;

	      /* Iterate over all data blocks.  */
	      if (list->data[cnt].data_list_rear != NULL)
		{
		  Elf_Data_List *dl = &scn->data_list;

		  while (dl != NULL)
		    {
		      Elf_Data *data = &dl->data.d;
		      if (dl == &scn->data_list && data->d_buf == NULL
			  && scn->rawdata.d.d_buf != NULL)
			data = &scn->rawdata.d;

		      if (unlikely (data->d_version != EV_CURRENT))
			{
			  __libelf_seterrno (ELF_E_UNKNOWN_VERSION);
			  return -1;
			}

		      if (unlikely (! powerof2 (data->d_align)))
			{
			  __libelf_seterrno (ELF_E_INVALID_ALIGN);
			  return -1;
			}

		      sh_align = MAX (sh_align, data->d_align);

		      if (elf->flags & ELF_F_LAYOUT)
			{
			  /* The user specified the offset and the size.
			     All we have to do is check whether this block
			     fits in the size specified for the section.  */
			  if (unlikely ((ElfW2(LIBELFBITS,SizeWord))
					(data->d_off + data->d_size)
					> shdr->sh_size))
			    {
			      __libelf_seterrno (ELF_E_SECTION_TOO_SMALL);
			      return -1;
			    }
			}
		      else
			{
			  /* Determine the padding.  */
			  offset = ((offset + data->d_align - 1)
				    & ~(data->d_align - 1));

			  update_if_changed (data->d_off, offset, changed);

			  offset += data->d_size;
			}

		      /* Next data block.  */
		      dl = dl->next;
		    }
		}
	      else
		/* Get the size of the section from the raw data.  If
		   none is available the value is zero.  */
		offset += scn->rawdata.d.d_size;

	      if (elf->flags & ELF_F_LAYOUT)
		{
		  size = MAX (size,
			      (shdr->sh_type != SHT_NOBITS
			       ? shdr->sh_offset + shdr->sh_size : 0));

		  /* The alignment must be a power of two.  This is a
		     requirement from the ELF specification.  Additionally
		     we test for the alignment of the section being large
		     enough for the largest alignment required by a data
		     block.  */
		  if (unlikely (! powerof2 (shdr->sh_addralign))
		      || unlikely ((shdr->sh_addralign ?: 1) < sh_align))
		    {
		      __libelf_seterrno (ELF_E_INVALID_ALIGN);
		      return -1;
		    }
		}
	      else
		{
		  /* How much alignment do we need for this section.  */
		  update_if_changed (shdr->sh_addralign, sh_align,
				     scn->shdr_flags);

		  size = (size + sh_align - 1) & ~(sh_align - 1);
		  int offset_changed = 0;
		  update_if_changed (shdr->sh_offset, size, offset_changed);
		  changed |= offset_changed;

		  if (offset_changed && scn->data_list_rear == NULL)
		    {
		      /* The position of the section in the file
			 changed.  Create the section data list.  */
		      if (__elf_getdata_rdlock (scn, NULL) == NULL)
			return -1;
		    }

		  /* See whether the section size is correct.  */
		  int size_changed = 0;
		  update_if_changed (shdr->sh_size,
				     (ElfW2(LIBELFBITS,SizeWord)) offset,
				     size_changed);
		  changed |= size_changed;

		  if (shdr->sh_type != SHT_NOBITS)
		    size += offset;

		  scn->shdr_flags |= (offset_changed | size_changed);
		  scn->flags |= changed;
		}

	      /* Check that the section size is actually a multiple of
		 the entry size.  */
	      if (shdr->sh_entsize != 0 && shdr->sh_entsize != 1
		  && (elf->flags & ELF_F_PERMISSIVE) == 0)
		{
		  /* For compressed sections check the uncompressed size.  */
		  ElfW2(LIBELFBITS,SizeWord) sh_size;
		  if ((shdr->sh_flags & SHF_COMPRESSED) == 0)
		    sh_size = shdr->sh_size;
		  else
		    {
		      ElfW2(LIBELFBITS,Chdr) *chdr;
		      chdr = __elfw2(LIBELFBITS,getchdr_wrlock) (scn);
		      if (unlikely (chdr == NULL))
			return -1;
		      sh_size = chdr->ch_size;
		    }

		  if (unlikely (sh_size % shdr->sh_entsize != 0))
		    {
		      __libelf_seterrno (ELF_E_INVALID_SHENTSIZE);
		      return -1;
		    }
		}
	    }

	  assert (list->next == NULL || list->cnt == list->max);

	  first = false;
	}
      while ((list = list->next) != NULL);

      /* Store section information.  */
      update_if_changed (ehdr->e_shentsize,
			 elf_typesize (LIBELFBITS, ELF_T_SHDR, 1), ehdr_flags);
      if (elf->flags & ELF_F_LAYOUT)
	{
	  /* The user is supposed to fill out e_shoff.  Use it and
	     e_shnum (or sh_size of the dummy, first section header)
	     to determine the maximum extend.  */
	  size = MAX (size,
		      (ehdr->e_shoff
		       + (elf_typesize (LIBELFBITS, ELF_T_SHDR, shnum))));
	}
      else
	{
	  /* Align for section header table.

	     Yes, we use `sizeof' and not `__alignof__' since we do not
	     want to be surprised by architectures with less strict
	     alignment rules.  */
#define SHDR_ALIGN sizeof (ElfW2(LIBELFBITS,Off))
	  size = (size + SHDR_ALIGN - 1) & ~(SHDR_ALIGN - 1);

	  update_if_changed (ehdr->e_shoff, size, elf->flags);

	  /* Account for the section header size.  */
	  size += elf_typesize (LIBELFBITS, ELF_T_SHDR, shnum);
	}
    }

  elf->state.ELFW(elf,LIBELFBITS).ehdr_flags |= ehdr_flags;

  return size;
}
