/* Functions to handle creation of Linux archives.
   Copyright (C) 2007-2012, 2016 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2007.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <gelf.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <libeu.h>

#include "system.h"
#include "arlib.h"


/* The one symbol table we hanble.  */
struct arlib_symtab symtab;


/* Initialize ARLIB_SYMTAB structure.  */
void
arlib_init (void)
{
#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free free
  obstack_init (&symtab.symsoffob);
  obstack_init (&symtab.symsnameob);
  obstack_init (&symtab.longnamesob);

  /* We add the archive header here as well, that avoids allocating
     another memory block.  */
  struct ar_hdr ar_hdr;
  memcpy (ar_hdr.ar_name, "/               ", sizeof (ar_hdr.ar_name));
  /* Using snprintf here has a problem: the call always wants to add a
     NUL byte.  We could use a trick whereby we specify the target
     buffer size longer than it is and this would not actually fail,
     since all the fields are consecutive and we fill them in
     sequence (i.e., the NUL byte gets overwritten).  But
     _FORTIFY_SOURCE=2 would not let us play these games.  Therefore
     we play it safe.  */
  char tmpbuf[sizeof (ar_hdr.ar_date) + 1];
  int s = snprintf (tmpbuf, sizeof (tmpbuf), "%-*lld",
		    (int) sizeof (ar_hdr.ar_date),
                    (arlib_deterministic_output ? 0
                     : (long long int) time (NULL)));
  memcpy (ar_hdr.ar_date, tmpbuf, s);
  assert ((sizeof (struct ar_hdr)  % sizeof (uint32_t)) == 0);

  /* Note the string for the ar_uid and ar_gid cases is longer than
     necessary.  This does not matter since we copy only as much as
     necessary but it helps the compiler to use the same string for
     the ar_mode case.  */
  memcpy (ar_hdr.ar_uid, "0       ", sizeof (ar_hdr.ar_uid));
  memcpy (ar_hdr.ar_gid, "0       ", sizeof (ar_hdr.ar_gid));
  memcpy (ar_hdr.ar_mode, "0       ", sizeof (ar_hdr.ar_mode));
  memcpy (ar_hdr.ar_fmag, ARFMAG, sizeof (ar_hdr.ar_fmag));

  /* Add the archive header to the file content.  */
  obstack_grow (&symtab.symsoffob, &ar_hdr, sizeof (ar_hdr));

  /* The first word in the offset table specifies the size.  Create
     such an entry now.  The real value will be filled-in later.  For
     all supported platforms the following is true.  */
  assert (sizeof (uint32_t) == sizeof (int));
  obstack_int_grow (&symtab.symsoffob, 0);

  /* The long name obstack also gets its archive header.  As above,
     some of the input strings are longer than required but we only
     copy the necessary part.  */
  memcpy (ar_hdr.ar_name, "//              ", sizeof (ar_hdr.ar_name));
  memcpy (ar_hdr.ar_date, "            ", sizeof (ar_hdr.ar_date));
  memcpy (ar_hdr.ar_uid, "            ", sizeof (ar_hdr.ar_uid));
  memcpy (ar_hdr.ar_gid, "            ", sizeof (ar_hdr.ar_gid));
  memcpy (ar_hdr.ar_mode, "            ", sizeof (ar_hdr.ar_mode));
  /* The ar_size field will be filled in later and ar_fmag is already OK.  */
  obstack_grow (&symtab.longnamesob, &ar_hdr, sizeof (ar_hdr));

  /* All other members are zero.  */
  symtab.symsofflen = 0;
  symtab.symsoff = NULL;
  symtab.symsnamelen = 0;
  symtab.symsname = NULL;
}


/* Finalize ARLIB_SYMTAB content.  */
void
arlib_finalize (void)
{
  /* Note that the size is stored as decimal string in 10 chars,
     without zero terminator (we add + 1 here only so snprintf can
     put it at the end, we then don't use it when we memcpy it).  */
  char tmpbuf[sizeof (((struct ar_hdr *) NULL)->ar_size) + 1];

  symtab.longnameslen = obstack_object_size (&symtab.longnamesob);
  if (symtab.longnameslen != sizeof (struct ar_hdr))
    {
      if ((symtab.longnameslen & 1) != 0)
	{
	  /* Add one more byte to make length even.  */
	  obstack_grow (&symtab.longnamesob, "\n", 1);
	  ++symtab.longnameslen;
	}

      symtab.longnames = obstack_finish (&symtab.longnamesob);

      int s = snprintf (tmpbuf, sizeof (tmpbuf), "%-*" PRIu32 "",
			(int) sizeof (((struct ar_hdr *) NULL)->ar_size),
			(uint32_t) (symtab.longnameslen - sizeof (struct ar_hdr)));
      memcpy (&((struct ar_hdr *) symtab.longnames)->ar_size, tmpbuf, s);
    }

  symtab.symsofflen = obstack_object_size (&symtab.symsoffob);
  assert (symtab.symsofflen % sizeof (uint32_t) == 0);
  if (symtab.symsofflen != 0)
    {
      symtab.symsoff = (uint32_t *) obstack_finish (&symtab.symsoffob);

      /* Fill in the number of offsets now.  */
      symtab.symsoff[AR_HDR_WORDS] = le_bswap_32 ((symtab.symsofflen
						    - sizeof (struct ar_hdr))
						   / sizeof (uint32_t) - 1);
    }

  symtab.symsnamelen = obstack_object_size (&symtab.symsnameob);
  if ((symtab.symsnamelen & 1) != 0)
    {
      /* Add one more NUL byte to make length even.  */
      obstack_grow (&symtab.symsnameob, "", 1);
      ++symtab.symsnamelen;
    }
  symtab.symsname = obstack_finish (&symtab.symsnameob);

  /* Determine correction for the offsets in the symbol table.   */
  off_t disp = 0;
  if (symtab.symsnamelen > 0)
    disp = symtab.symsofflen + symtab.symsnamelen;
  if (symtab.longnameslen > sizeof (struct ar_hdr))
    disp += symtab.longnameslen;

  if (disp != 0 && symtab.symsoff != NULL)
    {
      uint32_t nsyms = le_bswap_32 (symtab.symsoff[AR_HDR_WORDS]);

      for (uint32_t cnt = 1; cnt <= nsyms; ++cnt)
	{
	  uint32_t val = le_bswap_32 (symtab.symsoff[AR_HDR_WORDS + cnt]);
	  val += disp;
	  symtab.symsoff[AR_HDR_WORDS + cnt] = le_bswap_32 (val);
	}
    }

  /* See comment for ar_date above.  */
  memcpy (&((struct ar_hdr *) symtab.symsoff)->ar_size, tmpbuf,
	  snprintf (tmpbuf, sizeof (tmpbuf), "%-*" PRIu32 "",
		    (int) sizeof (((struct ar_hdr *) NULL)->ar_size),
		    (uint32_t) (symtab.symsofflen + symtab.symsnamelen
				- sizeof (struct ar_hdr))));
}


/* Free resources for ARLIB_SYMTAB.  */
void
arlib_fini (void)
{
  obstack_free (&symtab.symsoffob, NULL);
  obstack_free (&symtab.symsnameob, NULL);
  obstack_free (&symtab.longnamesob, NULL);
}


/* Add name a file offset of a symbol.  */
void
arlib_add_symref (const char *symname, off_t symoff)
{
  /* For all supported platforms the following is true.  */
  assert (sizeof (uint32_t) == sizeof (int));
  obstack_int_grow (&symtab.symsoffob, (int) le_bswap_32 (symoff));

  size_t symname_len = strlen (symname) + 1;
  obstack_grow (&symtab.symsnameob, symname, symname_len);
}


/* Add symbols from ELF with value OFFSET to the symbol table SYMTAB.  */
void
arlib_add_symbols (Elf *elf, const char *arfname, const char *membername,
		   off_t off)
{
  if (sizeof (off) > sizeof (uint32_t) && off > ~((uint32_t) 0))
    /* The archive is too big.  */
    error_exit (0, _("the archive '%s' is too large"),
		arfname);

  /* We only add symbol tables for ELF files.  It makes not much sense
     to add symbols from executables but we do so for compatibility.
     For DSOs and executables we use the dynamic symbol table, for
     relocatable files all the DT_SYMTAB tables.  */
  if (elf_kind (elf) != ELF_K_ELF)
    return;

  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr = gelf_getehdr (elf, &ehdr_mem);
  if (ehdr == NULL)
    error_exit (0, _("cannot read ELF header of %s(%s): %s"),
		arfname, membername, elf_errmsg (-1));

  GElf_Word symtype;
  if (ehdr->e_type == ET_REL)
    symtype = SHT_SYMTAB;
  else if (ehdr->e_type == ET_EXEC || ehdr->e_type == ET_DYN)
    symtype = SHT_DYNSYM;
  else
    /* We do not handle that type.  */
    return;

  /* Iterate over all sections.  */
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      /* Get the section header.  */
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
      if (shdr == NULL)
	continue;

      if (shdr->sh_type != symtype)
	continue;

      Elf_Data *data = elf_getdata (scn, NULL);
      if (data == NULL)
	continue;

      if (shdr->sh_entsize == 0)
	continue;

      int nsyms = shdr->sh_size / shdr->sh_entsize;
      for (int ndx = shdr->sh_info; ndx < nsyms; ++ndx)
	{
	  GElf_Sym sym_mem;
	  GElf_Sym *sym = gelf_getsym (data, ndx, &sym_mem);
	  if (sym == NULL)
	    continue;

	  /* Ignore undefined symbols.  */
	  if (sym->st_shndx == SHN_UNDEF)
	    continue;

	  /* Use this symbol.  */
	  const char *symname = elf_strptr (elf, shdr->sh_link, sym->st_name);
	  if (symname != NULL)
	    arlib_add_symref (symname, off);
	}

      /* Only relocatable files can have more than one symbol table.  */
      if (ehdr->e_type != ET_REL)
	break;
    }
}
