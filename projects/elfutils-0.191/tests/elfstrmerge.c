/* Merge string sections.
   Copyright (C) 2015, 2016 Red Hat, Inc.
   This file is part of elfutils.

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

#include <config.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>

#include <system.h>
#include <gelf.h>
#include ELFUTILS_HEADER(dwelf)
#include "elf-knowledge.h"

/* The original ELF file.  */
static int fd = -1;
static Elf *elf = NULL;
static bool replace;

/* The new ELF file.  */
static char *fnew = NULL;
static int fdnew = -1;
static Elf *elfnew = NULL;

/* The merged string table.  */
static Dwelf_Strtab *strings = NULL;

/* Section name strents.  */
static Dwelf_Strent **scnstrents = NULL;

/* Symbol name strends.  */
static Dwelf_Strent **symstrents = NULL;

/* New ELF file buffers.  */
static Elf_Data newstrtabdata = { .d_buf = NULL };
static size_t newshnums = 0;
static void **newscnbufs = NULL;

/* Release all files and resources allocated.  */
static void
release (void)
{
  /* The new string table.  */
  if (strings != NULL)
    dwelf_strtab_free (strings);

  free (scnstrents);
  free (symstrents);
  free (newstrtabdata.d_buf);

  /* Any new data buffers allocated.  */
  for (size_t i = 0; i < newshnums; i++)
    free (newscnbufs[i]);
  free (newscnbufs);

  /* The new ELF file.  */
  if (fdnew != -1)
    {
      unlink (fnew);
      elf_end (elfnew);
      close (fdnew);
    }
  // Don't release, we might need it in the error message.
  // if (replace)
  //   free (fnew);

  /* The original ELF file.  */
  elf_end (elf);
  close (fd);
}

/* The various ways we can fail... Cleanup and show some message to
   the user.  The file name may be NULL.  */
static void __attribute__ ((noreturn))
fail (const char *msg, const char *fname)
{
  release ();
  if (fname != NULL)
    error (1, 0, "%s: %s", fname, msg);
  else
    error (1, 0, "%s", msg);
  abort();
}

static void __attribute__ ((noreturn))
fail_errno (const char *msg, const char *fname)
{
  release ();
  if (fname != NULL)
    error (1, errno, "%s: %s", fname, msg);
  else
    error (1, errno, "%s", msg);
  abort();
}

static void __attribute__ ((noreturn))
fail_idx (const char *msg, const char *fname, size_t idx)
{
  release ();
  if (fname != NULL)
    error (1, 0, "%s: %s %zd", fname, msg, idx);
  else
    error (1, 0, "%s %zd", msg, idx);
  abort();
}

static void __attribute__ ((noreturn))
fail_elf (const char *msg, const char *fname)
{
  release ();
  if (fname != NULL)
    error (1, 0, "%s: %s: %s", fname, msg, elf_errmsg (-1));
  else
    error (1, 0, "%s: %s", msg, elf_errmsg (-1));
  abort();
}

static void __attribute__ ((noreturn))
fail_elf_idx (const char *msg, const char *fname, size_t idx)
{
  release ();
  if (fname != NULL)
    error (1, 0, "%s: %s %zd: %s", fname, msg, idx, elf_errmsg (-1));
  else
    error (1, 0, "%s %zd: %s", msg, idx, elf_errmsg (-1));
  abort();
}

/* section index mapping and sanity checking.  */
static size_t
newsecndx (size_t secndx, size_t shdrstrndx, size_t shdrnum,
	   const char *fname,
	   const char *what, size_t widx,
	   const char *member, size_t midx)
{
  if (unlikely (secndx == 0 || secndx == shdrstrndx || secndx >= shdrnum))
    {
      /* Don't use fail... too specialized messages.  Call release
	 ourselves and then error.  Ignores midx if widx is
	 zero.  */
      release ();
      if (widx == 0)
	error (1, 0, "%s: bad section index %zd in %s for %s",
	       fname, secndx, what, member);
      else if (midx == 0)
	error (1, 0, "%s: bad section index %zd in %s %zd for %s",
	       fname, secndx, what, widx, member);
      else
	error (1, 0, "%s: bad section index %zd in %s %zd for %s %zd",
	       fname, secndx, what, widx, member, midx);
    }

  return secndx < shdrstrndx ? secndx : secndx - 1;
}

static void
new_data_buf (Elf_Data *d, const char *fname,
	      size_t ndx, size_t shdrstrndx, size_t shdrnum)
{
  size_t s = d->d_size;
  if (s == 0)
    fail_idx ("Expected data in section", fname, ndx);
  void *b = malloc (d->d_size);
  if (b == NULL)
    fail_idx ("Couldn't allocated buffer for section", NULL, ndx);
  newscnbufs[newsecndx (ndx, shdrstrndx, shdrnum, fname,
			"section", ndx, "d_buf", 0)] = d->d_buf = b;
}

int
main (int argc, char **argv)
{
  elf_version (EV_CURRENT);

  /* Basic command line handling.  Need to replace the input file?  */
  if ((argc != 2 && argc != 4)
      || (argc == 4 && strcmp (argv[1], "-o") != 0))
    fail ("Usage argument: [-o <outputfile>] <inputfile>", NULL);
  replace = argc == 2;

  /* Get the ELF file.  */
  const char *fname;
  if (replace)
    fname = argv[1];
  else
    fname = argv[3];
  fd = open (fname, O_RDONLY);
  if (fd < 0)
    fail_errno ("couldn't open", fname);

  elf = elf_begin (fd, ELF_C_READ, NULL);
  if (elf == NULL)
    fail_elf ("couldn't open ELF file for reading", fname);

  GElf_Ehdr ehdr;
  if (gelf_getehdr (elf, &ehdr) == NULL)
    fail_elf ("Couldn't get ehdr", fname);

  /* Get the section header string table.  */
  size_t shdrstrndx;
  if (elf_getshdrstrndx (elf, &shdrstrndx) != 0)
    fail_elf ("couldn't get section header string table index", fname);

  Elf_Scn *shdrstrscn = elf_getscn (elf, shdrstrndx);
  GElf_Shdr shdrstrshdr_mem;
  GElf_Shdr *shdrstrshdr = gelf_getshdr (shdrstrscn, &shdrstrshdr_mem);
  if (shdrstrshdr == NULL)
    fail_elf ("couldn't get section header string table section", fname);

  if ((shdrstrshdr->sh_flags & SHF_ALLOC) != 0)
    fail ("section header string table is an allocated section", fname);

  /* Get the symtab section.  */
  size_t symtabndx = 0;
  Elf_Scn *symtabscn = NULL;
  GElf_Shdr symtabshdr_mem;
  GElf_Shdr *symtabshdr = NULL;
  while ((symtabscn = elf_nextscn (elf, symtabscn)) != NULL)
    {
      symtabshdr = gelf_getshdr (symtabscn, &symtabshdr_mem);
      if (symtabshdr == NULL)
	fail_elf ("couldn't get shdr", fname);

      if (symtabshdr->sh_type == SHT_SYMTAB)
	{
	  /* Just pick the first, we don't expect more than one. */
	  symtabndx = elf_ndxscn (symtabscn);
	  break;
	}
    }

  if (symtabshdr == NULL)
    fail ("No symtab found", fname);

  if ((symtabshdr->sh_flags & SHF_ALLOC) != 0)
    fail ("symtab is an allocated section", fname);

  /* Get the strtab of the symtab.  */
  size_t strtabndx = symtabshdr->sh_link;
  Elf_Scn *strtabscn = elf_getscn (elf, strtabndx);
  GElf_Shdr strtabshdr_mem;
  GElf_Shdr *strtabshdr = gelf_getshdr (strtabscn, &strtabshdr_mem);
  if (strtabshdr == NULL)
    fail_elf ("Couldn't get strtab section", fname);

  if (shdrstrndx == strtabndx)
    {
      error (0, 0, "%s: Nothing to do, shstrtab == strtab", fname);
      release ();
      return 0;
    }

  if ((strtabshdr->sh_flags & SHF_ALLOC) != 0)
    fail ("strtab is an allocated section", fname);

  size_t phnum;
  if (elf_getphdrnum (elf, &phnum) != 0)
    fail_elf ("Couldn't get number of phdrs", fname);

  /* If there are phdrs we want to maintain the layout of the
     allocated sections in the file.  */
  bool layout = phnum != 0;

  /* Create a new merged strings table that starts with the empty string.  */
  strings = dwelf_strtab_init (true);
  if (strings == NULL)
    fail ("No memory to create merged string table", NULL);

  /* Add the strings from all the sections.  */
  size_t shdrnum;
  if (elf_getshdrnum (elf, &shdrnum) != 0)
    fail_elf ("Couldn't get number of sections", fname);
  scnstrents = malloc (shdrnum * sizeof (Dwelf_Strent *));
  if (scnstrents == NULL)
    fail ("couldn't allocate memory for section strings", NULL);

  /* While going through all sections keep track of last allocated
     offset if needed to keep the layout.  We'll put any unallocated
     sections behind those (strtab is unallocated and will change
     size).  */
  GElf_Off last_offset = 0;
  if (layout)
    last_offset = (ehdr.e_phoff
		   + gelf_fsize (elf, ELF_T_PHDR, phnum, EV_CURRENT));
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      size_t scnnum = elf_ndxscn (scn);
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
      if (shdr == NULL)
	fail_elf_idx ("couldn't get shdr", fname, scnnum);
      /* Don't add the .shstrtab section itself, we'll not use it.  */
      if (shdr->sh_name != 0 && scnnum != shdrstrndx)
	{
	  const char *sname = elf_strptr (elf, shdrstrndx, shdr->sh_name);
	  if (sname == NULL)
	    fail_elf_idx ("couldn't get section name", fname, scnnum);
	  if ((scnstrents[scnnum] = dwelf_strtab_add (strings, sname)) == NULL)
	    fail ("No memory to add to merged string table", NULL);
	}

      if (layout)
	if ((shdr->sh_flags & SHF_ALLOC) != 0)
	  {
	    GElf_Off off = shdr->sh_offset + (shdr->sh_type != SHT_NOBITS
					      ? shdr->sh_size : 0);
	    if (last_offset < off)
	      last_offset = off;
	  }
    }

  /* Add the strings from all the symbols.  */
  size_t elsize = gelf_fsize (elf, ELF_T_SYM, 1, EV_CURRENT);
  Elf_Data *symd = elf_getdata (symtabscn, NULL);
  if (symd == NULL)
    fail_elf ("couldn't get symtab data", fname);
  size_t symsnum = symd->d_size / elsize;
  symstrents = malloc (symsnum * sizeof (Dwelf_Strent *));
  if (symstrents == NULL)
    fail_errno ("Couldn't allocate memory for symbol strings", NULL);
  for (size_t i = 0; i < symsnum; i++)
    {
      GElf_Sym sym_mem;
      GElf_Sym *sym = gelf_getsym (symd, i, &sym_mem);
      if (sym == NULL)
	fail_elf_idx ("Couldn't get symbol", fname, i);
      if (sym->st_name != 0)
	{
	  const char *sname = elf_strptr (elf, strtabndx, sym->st_name);
	  if (sname == NULL)
	    fail_elf_idx ("Couldn't get symbol name", fname, i);
	  if ((symstrents[i] = dwelf_strtab_add (strings, sname)) == NULL)
	    fail_idx ("No memory to add to merged string table symbol",
		      fname, i);
	}
    }

  /* We got all strings, build the new string table and store it as
     new strtab.  */
  dwelf_strtab_finalize (strings, &newstrtabdata);

  /* We share at least the empty string so the result is at least 1
     byte smaller.  */
  if (newstrtabdata.d_size >= shdrstrshdr->sh_size + strtabshdr->sh_size)
    fail ("Impossible, merged string table is larger", fname);

  struct stat st;
  if (fstat (fd, &st) != 0)
    fail_errno("Couldn't fstat", fname);

  /* Create a new (temporary) ELF file for the result.  */
  if (replace)
    {
      size_t fname_len = strlen (fname);
      fnew = malloc (fname_len + sizeof (".XXXXXX"));
      if (fnew == NULL)
	fail_errno ("couldn't allocate memory for new file name", NULL);
      strcpy (mempcpy (fnew, fname, fname_len), ".XXXXXX");

      fdnew = mkstemp (fnew);
    }
  else
    {
      fnew = argv[2];
      fdnew = open (fnew, O_WRONLY | O_CREAT, st.st_mode & ALLPERMS);
    }

  if (fdnew < 0)
    fail_errno ("couldn't create output file", fnew);

  elfnew = elf_begin (fdnew, ELF_C_WRITE, NULL);
  if (elfnew == NULL)
    fail_elf ("couldn't open new ELF for writing", fnew);

  /* Create the new ELF header and copy over all the data.  */
  if (gelf_newehdr (elfnew, gelf_getclass (elf)) == 0)
    fail_elf ("Couldn't create new ehdr", fnew);
  GElf_Ehdr newehdr;
  if (gelf_getehdr (elfnew, &newehdr) == NULL)
    fail_elf ("Couldn't get ehdr", fnew);

  newehdr.e_ident[EI_DATA] = ehdr.e_ident[EI_DATA];
  newehdr.e_type = ehdr.e_type;
  newehdr.e_machine = ehdr.e_machine;
  newehdr.e_version = ehdr.e_version;
  newehdr.e_entry = ehdr.e_entry;
  newehdr.e_flags = ehdr.e_flags;

  /* The new file uses the new strtab as shstrtab.  */
  size_t newstrtabndx = newsecndx (strtabndx, shdrstrndx, shdrnum,
				   fname, "ehdr", 0, "e_shstrndx", 0);
  if (newstrtabndx < SHN_LORESERVE)
    newehdr.e_shstrndx = newstrtabndx;
  else
    {
      Elf_Scn *zscn = elf_getscn (elfnew, 0);
      GElf_Shdr zshdr_mem;
      GElf_Shdr *zshdr = gelf_getshdr (zscn, &zshdr_mem);
      if (zshdr == NULL)
	fail_elf ("Couldn't get section zero", fnew);
      zshdr->sh_link = strtabndx;
      if (gelf_update_shdr (zscn, zshdr) == 0)
	fail_elf ("Couldn't update section zero", fnew);
      newehdr.e_shstrndx = SHN_XINDEX;
    }

  if (gelf_update_ehdr (elfnew, &newehdr) == 0)
    fail ("Couldn't update ehdr", fnew);

  /* Copy the program headers if any.  */
  if (phnum != 0)
    {
      if (gelf_newphdr (elfnew, phnum) == 0)
	fail_elf ("Couldn't create phdrs", fnew);

      for (size_t cnt = 0; cnt < phnum; ++cnt)
	{
	  GElf_Phdr phdr_mem;
	  GElf_Phdr *phdr = gelf_getphdr (elf, cnt, &phdr_mem);
	  if (phdr == NULL)
	    fail_elf_idx ("Couldn't get phdr", fname, cnt);
	  if (gelf_update_phdr (elfnew, cnt, phdr) == 0)
	    fail_elf_idx ("Couldn't create phdr", fnew, cnt);
	}
    }

  newshnums = shdrnum - 1;
  newscnbufs = calloc (newshnums, sizeof (void *));
  if (newscnbufs == NULL)
    fail_errno ("Couldn't allocate memory for new section buffers", NULL);

  /* Copy the sections, except the shstrtab, fill the strtab with the
     combined strings and adjust section references.  */
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      size_t ndx = elf_ndxscn (scn);

      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
      if (shdr == NULL)
	fail_elf_idx ("Couldn't get shdr", fname, ndx);

      /* Section zero is always created.  Skip the shtrtab.  */
      if (ndx == 0 || ndx == shdrstrndx)
	continue;

      Elf_Scn *newscn = elf_newscn (elfnew);
      if (newscn == NULL)
	fail_elf_idx ("couldn't create new section", fnew, ndx);

      GElf_Shdr newshdr;
      newshdr.sh_name = (shdr->sh_name != 0
			 ? dwelf_strent_off (scnstrents[ndx]) : 0);
      newshdr.sh_type = shdr->sh_type;
      newshdr.sh_flags = shdr->sh_flags;
      newshdr.sh_addr = shdr->sh_addr;
      newshdr.sh_size = shdr->sh_size;
      if (shdr->sh_link != 0)
	newshdr.sh_link = newsecndx (shdr->sh_link, shdrstrndx, shdrnum,
				     fname, "shdr", ndx, "sh_link", 0);
      else
	newshdr.sh_link = 0;
      if (SH_INFO_LINK_P (shdr) && shdr->sh_info != 0)
	newshdr.sh_info = newsecndx (shdr->sh_info, shdrstrndx, shdrnum,
				     fname, "shdr", ndx, "sh_info", 0);

      else
	newshdr.sh_info = shdr->sh_info;
      newshdr.sh_entsize = shdr->sh_entsize;

      /* Some sections need a new data buffer because they need to
	 manipulate the original data.  Allocate and check here, so we
	 have a list of all data buffers we might need to release when
	 done.  */
      Elf_Data *newdata = elf_newdata (newscn);
      if (newdata == NULL)
	fail_elf_idx ("Couldn't create new data for section", fnew, ndx);
      if (ndx == strtabndx)
	*newdata = newstrtabdata;
      else
	{
	  /* The symtab, dynsym, group and symtab_shndx sections
	     contain section indexes. Symbol tables (symtab and
	     dynsym) contain indexes to strings. Update both if
	     necessary.  */
	  Elf_Data *data = elf_getdata (scn, NULL);
	  if (data == NULL)
	    fail_elf_idx ("Couldn't get data from section", fname, ndx);
	  *newdata = *data;
	  switch (shdr->sh_type)
	    {
	    case SHT_SYMTAB:
	    case SHT_DYNSYM:
	      {
		/* We need to update the section numbers of the
		   symbols and if this symbol table uses the strtab
		   section also the name indexes.  */
		const bool update_name = shdr->sh_link == strtabndx;
		if (update_name && ndx != symtabndx)
		  fail ("Only one symbol table using strtab expected", fname);
		new_data_buf (newdata, fname, ndx, shdrstrndx, shdrnum);
		size_t syms = (data->d_size
			       / gelf_fsize (elf, ELF_T_SYM, 1, EV_CURRENT));
		for (size_t i = 0; i < syms; i++)
		  {
		    GElf_Sym sym;
		    if (gelf_getsym (data, i, &sym) == NULL)
		      fail_elf_idx ("Couldn't get symbol", fname, i);

		    if (GELF_ST_TYPE (sym.st_info) == STT_SECTION
			&& sym.st_shndx == shdrstrndx)
		      fprintf (stderr, "WARNING:"
			       " symbol table [%zd] contains section symbol %zd"
			       " for old shdrstrndx %zd\n", ndx, i, shdrstrndx);
		    else if (sym.st_shndx != SHN_UNDEF
			     && sym.st_shndx < SHN_LORESERVE)
		      sym.st_shndx = newsecndx (sym.st_shndx, shdrstrndx, shdrnum,
					 fname, "section", ndx, "symbol", i);
		    if (update_name && sym.st_name != 0)
		      sym.st_name = dwelf_strent_off (symstrents[i]);

		    /* We explicitly don't update the SHNDX table at
		       the same time, we do that below.  */
		    if (gelf_update_sym (newdata, i, &sym) == 0)
		      fail_elf_idx ("Couldn't update symbol", fnew, i);
		  }
	      }
	      break;

	    case SHT_GROUP:
	      {
		new_data_buf (newdata, fname, ndx, shdrstrndx, shdrnum);
		/* A section group contains Elf32_Words. The first
		   word is a flag value, the rest of the words are
		   indexes of the sections belonging to the group.  */
		Elf32_Word *group = (Elf32_Word *) data->d_buf;
		Elf32_Word *newgroup = (Elf32_Word *) newdata->d_buf;
		size_t words = data->d_size / sizeof (Elf32_Word);
		if (words == 0)
		  fail_idx ("Not enough data in group section", fname, ndx);
		newgroup[0] = group[0];
		for (size_t i = 1; i < words; i++)
		  newgroup[i] = newsecndx (group[i], shdrstrndx, shdrnum,
					   fname, "section", ndx, "group", i);
	      }
	      break;

	    case SHT_SYMTAB_SHNDX:
	      {
		new_data_buf (newdata, fname, ndx, shdrstrndx, shdrnum);
		/* A SHNDX just contains an array of section indexes
		   for the corresponding symbol table.  The entry is
		   SHN_UNDEF unless the corresponding symbol is
		   SHN_XINDEX.  */
		Elf32_Word *shndx = (Elf32_Word *) data->d_buf;
		Elf32_Word *newshndx = (Elf32_Word *) newdata->d_buf;
		size_t words = data->d_size / sizeof (Elf32_Word);
		for (size_t i = 0; i < words; i++)
		  if (shndx[i] == SHN_UNDEF)
		    newshndx[i] = SHN_UNDEF;
		  else
		    newshndx[i] = newsecndx (shndx[i], shdrstrndx, shdrnum,
					     fname, "section", ndx, "shndx", i);
	      }
	      break;

	    case SHT_DYNAMIC:
	      FALLTHROUGH;
	      /* There are string indexes in here, but
		 they (should) point to a allocated string table,
		 which we don't alter.  */
	    default:
	      /* Nothing to do.  Section data doesn't contain section
		 or strtab indexes.  */
	      break;
	    }
	}

      /* When we are responsible for the layout explicitly set
	 sh_addralign, sh_size and sh_offset.  Otherwise libelf will
	 calculate those from the Elf_Data.  */
      if (layout)
	{
	  /* We have just one Elf_Data.  */
	  newshdr.sh_size = newdata->d_size;
	  newshdr.sh_addralign = newdata->d_align;

	  /* Keep the offset of allocated sections so they are at the
	     same place in the file. Add unallocated ones after the
	     allocated ones.  */
	  if ((shdr->sh_flags & SHF_ALLOC) != 0)
	    newshdr.sh_offset = shdr->sh_offset;
	  else
	    {
	      /* Zero means one.  No alignment constraints.  */
	      size_t addralign = newshdr.sh_addralign ?: 1;
	      last_offset = (last_offset + addralign - 1) & ~(addralign - 1);
	      newshdr.sh_offset = last_offset;
	      if (newshdr.sh_type != SHT_NOBITS)
		last_offset += newshdr.sh_size;
	    }
	}
      else
	{
	  newshdr.sh_addralign = 0;
	  newshdr.sh_size = 0;
	  newshdr.sh_offset = 0;
	}

      if (gelf_update_shdr (newscn, &newshdr) == 0)
	fail_elf_idx ("Couldn't update section header", fnew, ndx);
    }

  /* If we have phdrs we want elf_update to layout the SHF_ALLOC
     sections precisely as in the original file.  In that case we are
     also responsible for setting phoff and shoff */
  if (layout)
    {
      /* Position the shdrs after the last (unallocated) section.  */
      if (gelf_getehdr (elfnew, &newehdr) == NULL)
	fail_elf ("Couldn't get ehdr", fnew);
      const size_t offsize = gelf_fsize (elf, ELF_T_OFF, 1, EV_CURRENT);
      newehdr.e_shoff = ((last_offset + offsize - 1)
			 & ~((GElf_Off) (offsize - 1)));

      /* The phdrs go in the same place as in the original file.
	 Normally right after the ELF header.  */
      newehdr.e_phoff = ehdr.e_phoff;

      if (gelf_update_ehdr (elfnew, &newehdr) == 0)
	fail_elf ("Couldn't update ehdr", fnew);

      elf_flagelf (elfnew, ELF_C_SET, ELF_F_LAYOUT);
    }

  if (elf_update (elfnew, ELF_C_WRITE) == -1)
    fail_elf ("Couldn't write ELF", fnew);

  elf_end (elfnew);
  elfnew = NULL;

  /* Try to match mode and owner.group of the original file.  */
  if (fchmod (fdnew, st.st_mode & ALLPERMS) != 0)
    error (0, errno, "Couldn't fchmod %s", fnew);
  if (fchown (fdnew, st.st_uid, st.st_gid) != 0)
    error (0, errno, "Couldn't fchown %s", fnew);

  /* Finally replace the old file with the new merged strings file.  */
  if (replace)
    if (rename (fnew, fname) != 0)
      fail_errno ("rename", fnew);

  /* We are finally done with the new file, don't unlink it now.  */
  close (fdnew);
  if (replace)
    free (fnew);
  fnew = NULL;
  fdnew = -1;

  release ();
  return 0;
}
