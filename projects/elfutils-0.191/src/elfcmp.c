/* Compare relevant content of two ELF files.
   Copyright (C) 2005-2012, 2014, 2015 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2005.

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

#include <argp.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <printversion.h>
#include "../libelf/elf-knowledge.h"
#include "../libebl/libeblP.h"
#include "system.h"

/* Prototypes of local functions.  */
static Elf *open_file (const char *fname, int *fdp, Ebl **eblp);
static bool search_for_copy_reloc (Ebl *ebl, size_t scnndx, int symndx);
static  int regioncompare (const void *p1, const void *p2);


/* Name and version of program.  */
ARGP_PROGRAM_VERSION_HOOK_DEF = print_version;

/* Bug report address.  */
ARGP_PROGRAM_BUG_ADDRESS_DEF = PACKAGE_BUGREPORT;

/* Values for the parameters which have no short form.  */
#define OPT_GAPS		0x100
#define OPT_HASH_INEXACT	0x101
#define OPT_IGNORE_BUILD_ID	0x102

/* Definitions of arguments for argp functions.  */
static const struct argp_option options[] =
{
  { NULL, 0, NULL, 0, N_("Control options:"), 0 },
  { "verbose", 'l', NULL, 0,
    N_("Output all differences, not just the first"), 0 },
  { "gaps", OPT_GAPS, "ACTION", 0, N_("Control treatment of gaps in loadable segments [ignore|match] (default: ignore)"), 0 },
  { "hash-inexact", OPT_HASH_INEXACT, NULL, 0,
    N_("Ignore permutation of buckets in SHT_HASH section"), 0 },
  { "ignore-build-id", OPT_IGNORE_BUILD_ID, NULL, 0,
    N_("Ignore differences in build ID"), 0 },
  { "quiet", 'q', NULL, 0, N_("Output nothing; yield exit status only"), 0 },

  { NULL, 0, NULL, 0, N_("Miscellaneous:"), 0 },
  { NULL, 0, NULL, 0, NULL, 0 }
};

/* Short description of program.  */
static const char doc[] = N_("\
Compare relevant parts of two ELF files for equality.");

/* Strings for arguments in help texts.  */
static const char args_doc[] = N_("FILE1 FILE2");

/* Prototype for option handler.  */
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/* Data structure to communicate with argp functions.  */
static struct argp argp =
{
  options, parse_opt, args_doc, doc, NULL, NULL, NULL
};


/* How to treat gaps in loadable segments.  */
static enum
  {
    gaps_ignore = 0,
    gaps_match
  }
  gaps;

/* Structure to hold information about used regions.  */
struct region
{
  GElf_Addr from;
  GElf_Addr to;
  struct region *next;
};

/* Nonzero if only exit status is wanted.  */
static bool quiet;

/* True iff multiple differences should be output.  */
static bool verbose;

/* True iff SHT_HASH treatment should be generous.  */
static bool hash_inexact;

/* True iff build ID notes should be ignored.  */
static bool ignore_build_id;

static bool hash_content_equivalent (size_t entsize, Elf_Data *, Elf_Data *);


int
main (int argc, char *argv[])
{
  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  /* Make sure the message catalog can be found.  */
  (void) bindtextdomain (PACKAGE_TARNAME, LOCALEDIR);

  /* Initialize the message catalog.  */
  (void) textdomain (PACKAGE_TARNAME);

  /* Parse and process arguments.  */
  int remaining;
  (void) argp_parse (&argp, argc, argv, 0, &remaining, NULL);

  /* We expect exactly two non-option parameters.  */
  if (unlikely (remaining + 2 != argc))
    {
      fputs (_("Invalid number of parameters.\n"), stderr);
      argp_help (&argp, stderr, ARGP_HELP_SEE, program_invocation_short_name);
      exit (1);
    }

  if (quiet)
    verbose = false;

  /* Comparing the files is done in two phases:
     1. compare all sections.  Sections which are irrelevant (i.e., if
	strip would remove them) are ignored.  Some section types are
	handled special.
     2. all parts of the loadable segments which are not parts of any
	section is compared according to the rules of the --gaps option.
  */
  int result = 0;
  elf_version (EV_CURRENT);

  const char *const fname1 = argv[remaining];
  int fd1;
  Ebl *ebl1;
  Elf *elf1 = open_file (fname1, &fd1, &ebl1);

  const char *const fname2 = argv[remaining + 1];
  int fd2;
  Ebl *ebl2;
  Elf *elf2 = open_file (fname2, &fd2, &ebl2);

  GElf_Ehdr ehdr1_mem;
  GElf_Ehdr *ehdr1 = gelf_getehdr (elf1, &ehdr1_mem);
  if (ehdr1 == NULL)
    error (2, 0, _("cannot get ELF header of '%s': %s"),
	   fname1, elf_errmsg (-1));
  GElf_Ehdr ehdr2_mem;
  GElf_Ehdr *ehdr2 = gelf_getehdr (elf2, &ehdr2_mem);
  if (ehdr2 == NULL)
    error (2, 0, _("cannot get ELF header of '%s': %s"),
	   fname2, elf_errmsg (-1));

#define DIFFERENCE							      \
  do									      \
    {									      \
      result = 1;							      \
      if (! verbose)							      \
	goto out;							      \
    }									      \
  while (0)

  /* Compare the ELF headers.  */
  if (unlikely (memcmp (ehdr1->e_ident, ehdr2->e_ident, EI_NIDENT) != 0
		|| ehdr1->e_type != ehdr2->e_type
		|| ehdr1->e_machine != ehdr2->e_machine
		|| ehdr1->e_version != ehdr2->e_version
		|| ehdr1->e_entry != ehdr2->e_entry
		|| ehdr1->e_phoff != ehdr2->e_phoff
		|| ehdr1->e_flags != ehdr2->e_flags
		|| ehdr1->e_ehsize != ehdr2->e_ehsize
		|| ehdr1->e_phentsize != ehdr2->e_phentsize
		|| ehdr1->e_phnum != ehdr2->e_phnum
		|| ehdr1->e_shentsize != ehdr2->e_shentsize))
    {
      if (! quiet)
	error (0, 0, _("%s %s diff: ELF header"), fname1, fname2);
      DIFFERENCE;
    }

  size_t shnum1;
  size_t shnum2;
  if (unlikely (elf_getshdrnum (elf1, &shnum1) != 0))
    error (2, 0, _("cannot get section count of '%s': %s"),
	   fname1, elf_errmsg (-1));
  if (unlikely (elf_getshdrnum (elf2, &shnum2) != 0))
    error (2, 0, _("cannot get section count of '%s': %s"),
	   fname2, elf_errmsg (-1));
  if (unlikely (shnum1 != shnum2))
    {
      if (! quiet)
	error (0, 0, _("%s %s diff: section count"), fname1, fname2);
      DIFFERENCE;
    }

  size_t phnum1;
  size_t phnum2;
  if (unlikely (elf_getphdrnum (elf1, &phnum1) != 0))
    error (2, 0, _("cannot get program header count of '%s': %s"),
	   fname1, elf_errmsg (-1));
  if (unlikely (elf_getphdrnum (elf2, &phnum2) != 0))
    error (2, 0, _("cannot get program header count of '%s': %s"),
	   fname2, elf_errmsg (-1));
  if (unlikely (phnum1 != phnum2))
    {
      if (! quiet)
	error (0, 0, _("%s %s diff: program header count"),
	       fname1, fname2);
      DIFFERENCE;
    }

  size_t shstrndx1;
  size_t shstrndx2;
  if (elf_getshdrstrndx (elf1, &shstrndx1) != 0)
    error (2, 0, _("cannot get hdrstrndx of '%s': %s"),
	   fname1, elf_errmsg (-1));
  if (elf_getshdrstrndx (elf2, &shstrndx2) != 0)
    error (2, 0, _("cannot get hdrstrndx of '%s': %s"),
	   fname2, elf_errmsg (-1));
  if (shstrndx1 != shstrndx2)
    {
      if (! quiet)
	error (0, 0, _("%s %s diff: shdr string index"),
	       fname1, fname2);
      DIFFERENCE;
    }

  /* Iterate over all sections.  We expect the sections in the two
     files to match exactly.  */
  Elf_Scn *scn1 = NULL;
  Elf_Scn *scn2 = NULL;
  struct region *regions = NULL;
  size_t nregions = 0;
  while (1)
    {
      GElf_Shdr shdr1_mem;
      GElf_Shdr *shdr1;
      const char *sname1 = NULL;
      do
	{
	  scn1 = elf_nextscn (elf1, scn1);
	  shdr1 = gelf_getshdr (scn1, &shdr1_mem);
	  if (shdr1 != NULL)
	    sname1 = elf_strptr (elf1, shstrndx1, shdr1->sh_name);
	}
      while (scn1 != NULL && shdr1 != NULL
	     && ebl_section_strip_p (ebl1, shdr1, sname1, true, false));

      GElf_Shdr shdr2_mem;
      GElf_Shdr *shdr2;
      const char *sname2 = NULL;
      do
	{
	  scn2 = elf_nextscn (elf2, scn2);
	  shdr2 = gelf_getshdr (scn2, &shdr2_mem);
	  if (shdr2 != NULL)
	    sname2 = elf_strptr (elf2, shstrndx2, shdr2->sh_name);
	}
      while (scn2 != NULL && shdr2 != NULL
	     && ebl_section_strip_p (ebl2, shdr2, sname2, true, false));

      if (scn1 == NULL || scn2 == NULL || shdr1 == NULL || shdr2 == NULL)
	break;

      if (gaps != gaps_ignore && (shdr1->sh_flags & SHF_ALLOC) != 0)
	{
	  struct region *newp = (struct region *) alloca (sizeof (*newp));
	  newp->from = shdr1->sh_offset;
	  newp->to = shdr1->sh_offset + shdr1->sh_size;
	  newp->next = regions;
	  regions = newp;

	  ++nregions;
	}

      /* Compare the headers.  We allow the name to be at a different
	 location.  */
      if (unlikely (sname1 == NULL || sname2 == NULL
		    || strcmp (sname1, sname2) != 0))
	{
	  error (0, 0, _("%s %s differ: section [%zu], [%zu] name"),
		 fname1, fname2, elf_ndxscn (scn1), elf_ndxscn (scn2));
	  DIFFERENCE;
	}

      /* We ignore certain sections.  */
      if ((sname1 != NULL && strcmp (sname1, ".gnu_debuglink") == 0)
	  || (sname1 != NULL && strcmp (sname1, ".gnu.prelink_undo") == 0))
	continue;

      if (shdr1->sh_type != shdr2->sh_type
	  // XXX Any flags which should be ignored?
	  || shdr1->sh_flags != shdr2->sh_flags
	  || shdr1->sh_addr != shdr2->sh_addr
	  || (shdr1->sh_offset != shdr2->sh_offset
	      && (shdr1->sh_flags & SHF_ALLOC)
	      && ehdr1->e_type != ET_REL)
	  || shdr1->sh_size != shdr2->sh_size
	  || shdr1->sh_link != shdr2->sh_link
	  || shdr1->sh_info != shdr2->sh_info
	  || shdr1->sh_addralign != shdr2->sh_addralign
	  || shdr1->sh_entsize != shdr2->sh_entsize)
	{
	  error (0, 0, _("%s %s differ: section [%zu] '%s' header"),
		 fname1, fname2, elf_ndxscn (scn1), sname1);
	  DIFFERENCE;
	}

      Elf_Data *data1 = elf_getdata (scn1, NULL);
      if (data1 == NULL)
	error (2, 0,
	       _("cannot get content of section %zu in '%s': %s"),
	       elf_ndxscn (scn1), fname1, elf_errmsg (-1));

      Elf_Data *data2 = elf_getdata (scn2, NULL);
      if (data2 == NULL)
	error (2, 0,
	       _("cannot get content of section %zu in '%s': %s"),
	       elf_ndxscn (scn2), fname2, elf_errmsg (-1));

      switch (shdr1->sh_type)
	{
	case SHT_DYNSYM:
	case SHT_SYMTAB:
	  if (shdr1->sh_entsize == 0)
	    error (2, 0,
		   _("symbol table [%zu] in '%s' has zero sh_entsize"),
		   elf_ndxscn (scn1), fname1);

	  /* Iterate over the symbol table.  We ignore the st_size
	     value of undefined symbols.  */
	  for (int ndx = 0; ndx < (int) (shdr1->sh_size / shdr1->sh_entsize);
	       ++ndx)
	    {
	      GElf_Sym sym1_mem;
	      GElf_Sym *sym1 = gelf_getsym (data1, ndx, &sym1_mem);
	      if (sym1 == NULL)
		error (2, 0,
		       _("cannot get symbol in '%s': %s"),
		       fname1, elf_errmsg (-1));
	      GElf_Sym sym2_mem;
	      GElf_Sym *sym2 = gelf_getsym (data2, ndx, &sym2_mem);
	      if (sym2 == NULL)
		error (2, 0,
		       _("cannot get symbol in '%s': %s"),
		       fname2, elf_errmsg (-1));

	      const char *name1 = elf_strptr (elf1, shdr1->sh_link,
					      sym1->st_name);
	      const char *name2 = elf_strptr (elf2, shdr2->sh_link,
					      sym2->st_name);
	      if (unlikely (name1 == NULL || name2 == NULL
			    || strcmp (name1, name2) != 0
			    || sym1->st_value != sym2->st_value
			    || (sym1->st_size != sym2->st_size
				&& sym1->st_shndx != SHN_UNDEF)
			    || sym1->st_info != sym2->st_info
			    || sym1->st_other != sym2->st_other
			    || sym1->st_shndx != sym2->st_shndx))
		{
		  // XXX Do we want to allow reordered symbol tables?
		symtab_mismatch:
		  if (! quiet)
		    {
		      if (elf_ndxscn (scn1) == elf_ndxscn (scn2))
			error (0, 0,
			       _("%s %s differ: symbol table [%zu]"),
			       fname1, fname2, elf_ndxscn (scn1));
		      else
			error (0, 0, _("\
%s %s differ: symbol table [%zu,%zu]"),
			       fname1, fname2, elf_ndxscn (scn1),
			       elf_ndxscn (scn2));
		    }
		  DIFFERENCE;
		  break;
		}

	      if (sym1->st_shndx == SHN_UNDEF
		  && sym1->st_size != sym2->st_size)
		{
		  /* The size of the symbol in the object defining it
		     might have changed.  That is OK unless the symbol
		     is used in a copy relocation.  Look over the
		     sections in both files and determine which
		     relocation section uses this symbol table
		     section.  Then look through the relocations to
		     see whether any copy relocation references this
		     symbol.  */
		  if (search_for_copy_reloc (ebl1, elf_ndxscn (scn1), ndx)
		      || search_for_copy_reloc (ebl2, elf_ndxscn (scn2), ndx))
		    goto symtab_mismatch;
		}
	    }
	  break;

	case SHT_NOTE:
	  /* Parse the note format and compare the notes themselves.  */
	  {
	    GElf_Nhdr note1;
	    GElf_Nhdr note2;

	    size_t off1 = 0;
	    size_t off2 = 0;
	    size_t name_offset;
	    size_t desc_offset;
	    while (off1 < data1->d_size
		   && (off1 = gelf_getnote (data1, off1, &note1,
					    &name_offset, &desc_offset)) > 0)
	      {
		const char *name1 = (note1.n_namesz == 0
				     ? "" : data1->d_buf + name_offset);
		const void *desc1 = data1->d_buf + desc_offset;
		if (off2 >= data2->d_size)
		  {
		    if (! quiet)
		      error (0, 0, _("\
%s %s differ: section [%zu] '%s' number of notes"),
			     fname1, fname2, elf_ndxscn (scn1), sname1);
		    DIFFERENCE;
		  }
		off2 = gelf_getnote (data2, off2, &note2,
				     &name_offset, &desc_offset);
		if (off2 == 0)
		  error (2, 0, _("\
cannot read note section [%zu] '%s' in '%s': %s"),
			 elf_ndxscn (scn2), sname2, fname2, elf_errmsg (-1));
		const char *name2 = (note2.n_namesz == 0
				     ? "" : data2->d_buf + name_offset);
		const void *desc2 = data2->d_buf + desc_offset;

		if (note1.n_namesz != note2.n_namesz
		    || memcmp (name1, name2, note1.n_namesz))
		  {
		    if (! quiet)
		      error (0, 0, _("\
%s %s differ: section [%zu] '%s' note name"),
			     fname1, fname2, elf_ndxscn (scn1), sname1);
		    DIFFERENCE;
		  }
		if (note1.n_type != note2.n_type)
		  {
		    if (! quiet)
		      error (0, 0, _("\
%s %s differ: section [%zu] '%s' note '%s' type"),
			     fname1, fname2, elf_ndxscn (scn1), sname1, name1);
		    DIFFERENCE;
		  }
		if (note1.n_descsz != note2.n_descsz
		    || memcmp (desc1, desc2, note1.n_descsz))
		  {
		    if (note1.n_type == NT_GNU_BUILD_ID
			&& note1.n_namesz == sizeof "GNU"
			&& !memcmp (name1, "GNU", sizeof "GNU"))
		      {
			if (note1.n_descsz != note2.n_descsz)
			  {
			    if (! quiet)
			      error (0, 0, _("\
%s %s differ: build ID length"),
				     fname1, fname2);
			    DIFFERENCE;
			  }
			else if (! ignore_build_id)
			  {
			    if (! quiet)
			      error (0, 0, _("\
%s %s differ: build ID content"),
				     fname1, fname2);
			    DIFFERENCE;
			  }
		      }
		    else
		      {
			if (! quiet)
			  error (0, 0, _("\
%s %s differ: section [%zu] '%s' note '%s' content"),
				 fname1, fname2, elf_ndxscn (scn1), sname1,
				 name1);
			DIFFERENCE;
		      }
		  }
	      }
	    if (off2 < data2->d_size)
	      {
		if (! quiet)
		  error (0, 0, _("\
%s %s differ: section [%zu] '%s' number of notes"),
			 fname1, fname2, elf_ndxscn (scn1), sname1);
		DIFFERENCE;
	      }
	  }
	  break;

	default:
	  /* Compare the section content byte for byte.  */
	  assert (shdr1->sh_type == SHT_NOBITS
		  || (data1->d_buf != NULL || data1->d_size == 0));
	  assert (shdr2->sh_type == SHT_NOBITS
		  || (data2->d_buf != NULL || data1->d_size == 0));

	  if (unlikely (data1->d_size != data2->d_size
			|| (shdr1->sh_type != SHT_NOBITS
			    && data1->d_size != 0
			    && memcmp (data1->d_buf, data2->d_buf,
				       data1->d_size) != 0)))
	    {
	      if (hash_inexact
		  && shdr1->sh_type == SHT_HASH
		  && data1->d_size == data2->d_size
		  && hash_content_equivalent (shdr1->sh_entsize, data1, data2))
		break;

	      if (! quiet)
		{
		  if (elf_ndxscn (scn1) == elf_ndxscn (scn2))
		    error (0, 0, _("\
%s %s differ: section [%zu] '%s' content"),
			   fname1, fname2, elf_ndxscn (scn1), sname1);
		  else
		    error (0, 0, _("\
%s %s differ: section [%zu,%zu] '%s' content"),
			   fname1, fname2, elf_ndxscn (scn1),
			   elf_ndxscn (scn2), sname1);
		}
	      DIFFERENCE;
	    }
	  break;
	}
    }

  if (unlikely (scn1 != scn2))
    {
      if (! quiet)
	error (0, 0,
	       _("%s %s differ: unequal amount of important sections"),
	       fname1, fname2);
      DIFFERENCE;
    }

  /* We we look at gaps, create artificial ones for the parts of the
     program which we are not in sections.  */
  struct region ehdr_region;
  struct region phdr_region;
  if (gaps != gaps_ignore)
    {
      ehdr_region.from = 0;
      ehdr_region.to = ehdr1->e_ehsize;
      ehdr_region.next = &phdr_region;

      phdr_region.from = ehdr1->e_phoff;
      phdr_region.to = ehdr1->e_phoff + phnum1 * ehdr1->e_phentsize;
      phdr_region.next = regions;

      regions = &ehdr_region;
      nregions += 2;
    }

  /* If we need to look at the gaps we need access to the file data.  */
  char *raw1 = NULL;
  size_t size1 = 0;
  char *raw2 = NULL;
  size_t size2 = 0;
  struct region *regionsarr = alloca (nregions * sizeof (struct region));
  if (gaps != gaps_ignore)
    {
      raw1 = elf_rawfile (elf1, &size1);
      if (raw1 == NULL )
	error (2, 0, _("cannot load data of '%s': %s"),
	       fname1, elf_errmsg (-1));

      raw2 = elf_rawfile (elf2, &size2);
      if (raw2 == NULL )
	error (2, 0, _("cannot load data of '%s': %s"),
	       fname2, elf_errmsg (-1));

      for (size_t cnt = 0; cnt < nregions; ++cnt)
	{
	  regionsarr[cnt] = *regions;
	  regions = regions->next;
	}

      qsort (regionsarr, nregions, sizeof (regionsarr[0]), regioncompare);
    }

  /* Compare the program header tables.  */
  for (unsigned int ndx = 0; ndx < phnum1; ++ndx)
    {
      GElf_Phdr phdr1_mem;
      GElf_Phdr *phdr1 = gelf_getphdr (elf1, ndx, &phdr1_mem);
      if (phdr1 == NULL)
	error (2, 0,
	       _("cannot get program header entry %d of '%s': %s"),
	       ndx, fname1, elf_errmsg (-1));
      GElf_Phdr phdr2_mem;
      GElf_Phdr *phdr2 = gelf_getphdr (elf2, ndx, &phdr2_mem);
      if (phdr2 == NULL)
	error (2, 0,
	       _("cannot get program header entry %d of '%s': %s"),
	       ndx, fname2, elf_errmsg (-1));

      if (unlikely (memcmp (phdr1, phdr2, sizeof (GElf_Phdr)) != 0))
	{
	  if (! quiet)
	    error (0, 0, _("%s %s differ: program header %d"),
		   fname1, fname2, ndx);
	  DIFFERENCE;
	}

      if (gaps != gaps_ignore && phdr1->p_type == PT_LOAD)
	{
	  size_t cnt = 0;
	  while (cnt < nregions && regionsarr[cnt].to < phdr1->p_offset)
	    ++cnt;

	  GElf_Off last = phdr1->p_offset;
	  GElf_Off end = phdr1->p_offset + phdr1->p_filesz;
	  while (cnt < nregions && regionsarr[cnt].from < end)
	    {
	      if (last < regionsarr[cnt].from)
		{
		  /* Compare the [LAST,FROM) region.  */
		  assert (gaps == gaps_match);
		  if (unlikely (memcmp (raw1 + last, raw2 + last,
					regionsarr[cnt].from - last) != 0))
		    {
		    gapmismatch:
		      if (!quiet)
			error (0, 0, _("%s %s differ: gap"),
			       fname1, fname2);
		      DIFFERENCE;
		      break;
		    }

		}
	      last = regionsarr[cnt].to;
	      ++cnt;
	    }

	  if (cnt == nregions && last < end)
	    goto gapmismatch;
	}
    }

 out:
  elf_end (elf1);
  elf_end (elf2);
  ebl_closebackend (ebl1);
  ebl_closebackend (ebl2);
  close (fd1);
  close (fd2);

  return result;
}


/* Handle program arguments.  */
static error_t
parse_opt (int key, char *arg,
	   struct argp_state *state __attribute__ ((unused)))
{
  switch (key)
    {
    case 'q':
      quiet = true;
      break;

    case 'l':
      verbose = true;
      break;

    case OPT_GAPS:
      if (strcasecmp (arg, "ignore") == 0)
	gaps = gaps_ignore;
      else if (likely (strcasecmp (arg, "match") == 0))
	gaps = gaps_match;
      else
	{
	  fprintf (stderr,
		   _("Invalid value '%s' for --gaps parameter."),
		   arg);
	  argp_help (&argp, stderr, ARGP_HELP_SEE,
		     program_invocation_short_name);
	  exit (1);
	}
      break;

    case OPT_HASH_INEXACT:
      hash_inexact = true;
      break;

    case OPT_IGNORE_BUILD_ID:
      ignore_build_id = true;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}


static Elf *
open_file (const char *fname, int *fdp, Ebl **eblp)
{
  int fd = open (fname, O_RDONLY);
  if (unlikely (fd == -1))
    error (2, errno, _("cannot open '%s'"), fname);
  Elf *elf = elf_begin (fd, ELF_C_READ_MMAP, NULL);
  if (elf == NULL)
    error (2, 0,
	   _("cannot create ELF descriptor for '%s': %s"),
	   fname, elf_errmsg (-1));
  Ebl *ebl = ebl_openbackend (elf);
  if (ebl == NULL)
    error (2, 0,
	   _("cannot create EBL descriptor for '%s'"), fname);

  *fdp = fd;
  *eblp = ebl;
  return elf;
}


static bool
search_for_copy_reloc (Ebl *ebl, size_t scnndx, int symndx)
{
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (ebl->elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
      if (shdr == NULL)
	error (2, 0,
	       _("cannot get section header of section %zu: %s"),
	       elf_ndxscn (scn), elf_errmsg (-1));

      if ((shdr->sh_type != SHT_REL && shdr->sh_type != SHT_RELA)
	  || shdr->sh_link != scnndx)
	continue;

      Elf_Data *data = elf_getdata (scn, NULL);
      if (data == NULL)
	error (2, 0,
	       _("cannot get content of section %zu: %s"),
	       elf_ndxscn (scn), elf_errmsg (-1));

      if (shdr->sh_type == SHT_REL && shdr->sh_entsize != 0)
	for (int ndx = 0; ndx < (int) (shdr->sh_size / shdr->sh_entsize);
	     ++ndx)
	  {
	    GElf_Rel rel_mem;
	    GElf_Rel *rel = gelf_getrel (data, ndx, &rel_mem);
	    if (rel == NULL)
	      error (2, 0, _("cannot get relocation: %s"),
		     elf_errmsg (-1));

	    if ((int) GELF_R_SYM (rel->r_info) == symndx
		&& ebl_copy_reloc_p (ebl, GELF_R_TYPE (rel->r_info)))
	      return true;
	  }
      else if (shdr->sh_entsize != 0)
	for (int ndx = 0; ndx < (int) (shdr->sh_size / shdr->sh_entsize);
	     ++ndx)
	  {
	    GElf_Rela rela_mem;
	    GElf_Rela *rela = gelf_getrela (data, ndx, &rela_mem);
	    if (rela == NULL)
	      error (2, 0, _("cannot get relocation: %s"),
		     elf_errmsg (-1));

	    if ((int) GELF_R_SYM (rela->r_info) == symndx
		&& ebl_copy_reloc_p (ebl, GELF_R_TYPE (rela->r_info)))
	      return true;
	  }
    }

  return false;
}


static int
regioncompare (const void *p1, const void *p2)
{
  const struct region *r1 = (const struct region *) p1;
  const struct region *r2 = (const struct region *) p2;

  if (r1->from < r2->from)
    return -1;
  return 1;
}


static int
compare_Elf32_Word (const void *p1, const void *p2)
{
  const Elf32_Word *w1 = p1;
  const Elf32_Word *w2 = p2;
  return *w1 < *w2 ? -1 : *w1 > *w2 ? 1 : 0;
}

static int
compare_Elf64_Xword (const void *p1, const void *p2)
{
  const Elf64_Xword *w1 = p1;
  const Elf64_Xword *w2 = p2;
  return *w1 < *w2 ? -1 : *w1 > *w2 ? 1 : 0;
}

static bool
hash_content_equivalent (size_t entsize, Elf_Data *data1, Elf_Data *data2)
{
#define CHECK_HASH(Hash_Word)						      \
  {									      \
    const Hash_Word *const hash1 = data1->d_buf;			      \
    const Hash_Word *const hash2 = data2->d_buf;			      \
    const size_t nbucket = hash1[0];					      \
    const size_t nchain = hash1[1];					      \
    if (data1->d_size != (2 + nbucket + nchain) * sizeof hash1[0]	      \
	|| hash2[0] != nbucket || hash2[1] != nchain)			      \
      return false;							      \
									      \
    const Hash_Word *const bucket1 = &hash1[2];				      \
    const Hash_Word *const chain1 = &bucket1[nbucket];			      \
    const Hash_Word *const bucket2 = &hash2[2];				      \
    const Hash_Word *const chain2 = &bucket2[nbucket];			      \
									      \
    bool chain_ok[nchain];						      \
    Hash_Word temp1[nchain - 1];					      \
    Hash_Word temp2[nchain - 1];					      \
    memset (chain_ok, 0, sizeof chain_ok);				      \
    for (size_t i = 0; i < nbucket; ++i)				      \
      {									      \
	if (bucket1[i] >= nchain || bucket2[i] >= nchain)		      \
	  return false;							      \
									      \
	size_t b1 = 0;							      \
	for (size_t p = bucket1[i]; p != STN_UNDEF; p = chain1[p])	      \
	  if (p >= nchain || b1 >= nchain - 1)				      \
	    return false;						      \
	  else								      \
	    temp1[b1++] = p;						      \
									      \
	size_t b2 = 0;							      \
	for (size_t p = bucket2[i]; p != STN_UNDEF; p = chain2[p])	      \
	  if (p >= nchain || b2 >= nchain - 1)				      \
	    return false;						      \
	  else								      \
	    temp2[b2++] = p;						      \
									      \
	if (b1 != b2)							      \
	  return false;							      \
									      \
	qsort (temp1, b1, sizeof temp1[0], compare_##Hash_Word);	      \
	qsort (temp2, b2, sizeof temp2[0], compare_##Hash_Word);	      \
									      \
	for (b1 = 0; b1 < b2; ++b1)					      \
	  if (temp1[b1] != temp2[b1])					      \
	    return false;						      \
	  else								      \
	    chain_ok[temp1[b1]] = true;					      \
      }									      \
									      \
    for (size_t i = 0; i < nchain; ++i)					      \
      if (!chain_ok[i] && chain1[i] != chain2[i])			      \
	return false;							      \
									      \
    return true;							      \
  }

  switch (entsize)
    {
    case 4:
      CHECK_HASH (Elf32_Word);
      break;
    case 8:
      CHECK_HASH (Elf64_Xword);
      break;
    }

  return false;
}


#include "debugpred.h"
