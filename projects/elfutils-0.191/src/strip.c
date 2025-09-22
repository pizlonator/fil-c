/* Discard section not used at runtime from object files.
   Copyright (C) 2000-2012, 2014, 2015, 2016, 2017, 2018 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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
#include <byteswap.h>
#include <endian.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <gelf.h>
#include <libelf.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <elf-knowledge.h>
#include <libebl.h>
#include "libdwelf.h"
#include <libeu.h>
#include <system.h>
#include <printversion.h>

typedef uint8_t GElf_Byte;

/* Name and version of program.  */
ARGP_PROGRAM_VERSION_HOOK_DEF = print_version;

/* Bug report address.  */
ARGP_PROGRAM_BUG_ADDRESS_DEF = PACKAGE_BUGREPORT;


/* Values for the parameters which have no short form.  */
#define OPT_REMOVE_COMMENT	0x100
#define OPT_PERMISSIVE		0x101
#define OPT_STRIP_SECTIONS	0x102
#define OPT_RELOC_DEBUG 	0x103
#define OPT_KEEP_SECTION 	0x104
#define OPT_RELOC_DEBUG_ONLY    0x105


/* Definitions of arguments for argp functions.  */
static const struct argp_option options[] =
{
  { NULL, 0, NULL, 0, N_("Output selection:"), 0 },
  { "output", 'o', "FILE", 0, N_("Place stripped output into FILE"), 0 },
  { NULL, 'f', "FILE", 0, N_("Extract the removed sections into FILE"), 0 },
  { NULL, 'F', "FILE", 0, N_("Embed name FILE instead of -f argument"), 0 },

  { NULL, 0, NULL, 0, N_("Output options:"), 0 },
  { "strip-all", 's', NULL, OPTION_HIDDEN, NULL, 0 },
  { "strip-debug", 'g', NULL, 0, N_("Remove all debugging symbols"), 0 },
  { NULL, 'd', NULL, OPTION_ALIAS, NULL, 0 },
  { NULL, 'S', NULL, OPTION_ALIAS, NULL, 0 },
  { "strip-sections", OPT_STRIP_SECTIONS, NULL, 0,
    N_("Remove section headers (not recommended)"), 0 },
  { "preserve-dates", 'p', NULL, 0,
    N_("Copy modified/access timestamps to the output"), 0 },
  { "reloc-debug-sections", OPT_RELOC_DEBUG, NULL, 0,
    N_("Resolve all trivial relocations between debug sections if the removed sections are placed in a debug file (only relevant for ET_REL files, operation is not reversible, needs -f)"), 0 },
  { "reloc-debug-sections-only", OPT_RELOC_DEBUG_ONLY, NULL, 0,
    N_("Similar to --reloc-debug-sections, but resolve all trivial relocations between debug sections in place.  No other stripping is performed (operation is not reversible, incompatible with -f, -g, --remove-comment and --remove-section)"), 0 },
  { "remove-comment", OPT_REMOVE_COMMENT, NULL, 0,
    N_("Remove .comment section"), 0 },
  { "remove-section", 'R', "SECTION", 0, N_("Remove the named section.  SECTION is an extended wildcard pattern.  May be given more than once.  Only non-allocated sections can be removed."), 0 },
  { "keep-section", OPT_KEEP_SECTION, "SECTION", 0, N_("Keep the named section.  SECTION is an extended wildcard pattern.  May be given more than once."), 0 },
  { "permissive", OPT_PERMISSIVE, NULL, 0,
    N_("Relax a few rules to handle slightly broken ELF files"), 0 },
  { NULL, 0, NULL, 0, NULL, 0 }
};

/* Short description of program.  */
static const char doc[] = N_("Discard symbols from object files.");

/* Strings for arguments in help texts.  */
static const char args_doc[] = N_("[FILE...]");

/* Prototype for option handler.  */
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/* Data structure to communicate with argp functions.  */
static struct argp argp =
{
  options, parse_opt, args_doc, doc, NULL, NULL, NULL
};


/* Print symbols in file named FNAME.  */
static int process_file (const char *fname);

/* Handle one ELF file.  */
static int handle_elf (int fd, Elf *elf, const char *prefix,
		       const char *fname, mode_t mode, struct timespec tvp[2]);

/* Handle all files contained in the archive.  */
static int handle_ar (int fd, Elf *elf, const char *prefix, const char *fname,
		      struct timespec tvp[2]) __attribute__ ((unused));

static int debug_fd = -1;
static char *tmp_debug_fname = NULL;

/* Close debug file descriptor, if opened. And remove temporary debug file.  */
static void cleanup_debug (void);

#define INTERNAL_ERROR(fname) \
  do { \
    cleanup_debug (); \
    error_exit (0, _("%s: INTERNAL ERROR %d (%s): %s"),			\
		fname, __LINE__, PACKAGE_VERSION, elf_errmsg (-1));	\
  } while (0)


/* Name of the output file.  */
static const char *output_fname;

/* Name of the debug output file.  */
static const char *debug_fname;

/* Name to pretend the debug output file has.  */
static const char *debug_fname_embed;

/* If true output files shall have same date as the input file.  */
static bool preserve_dates;

/* If true .comment sections will be removed.  */
static bool remove_comment;

/* If true remove all debug sections.  */
static bool remove_debug;

/* If true remove all section headers.  */
static bool remove_shdrs;

/* If true relax some ELF rules for input files.  */
static bool permissive;

/* If true perform relocations between debug sections.  */
static bool reloc_debug;

/* If true perform relocations between debug sections only.  */
static bool reloc_debug_only;

/* Sections the user explicitly wants to keep or remove.  */
struct section_pattern
{
  char *pattern;
  struct section_pattern *next;
};

static struct section_pattern *keep_secs = NULL;
static struct section_pattern *remove_secs = NULL;

static void
add_pattern (struct section_pattern **patterns, const char *pattern)
{
  struct section_pattern *p = xmalloc (sizeof *p);
  p->pattern = xstrdup (pattern);
  p->next = *patterns;
  *patterns = p;
}

static void
free_sec_patterns (struct section_pattern *patterns)
{
  struct section_pattern *pattern = patterns;
  while (pattern != NULL)
    {
      struct section_pattern *p = pattern;
      pattern = p->next;
      free (p->pattern);
      free (p);
    }
}

static void
free_patterns (void)
{
  free_sec_patterns (keep_secs);
  free_sec_patterns (remove_secs);
}

static bool
section_name_matches (struct section_pattern *patterns, const char *name)
{
  struct section_pattern *pattern = patterns;
  while (pattern != NULL)
    {
      if (fnmatch (pattern->pattern, name, FNM_EXTMATCH) == 0)
	return true;
      pattern = pattern->next;
    }
  return false;
}


int
main (int argc, char *argv[])
{
  int remaining;
  int result = 0;

  /* We use no threads here which can interfere with handling a stream.  */
  __fsetlocking (stdin, FSETLOCKING_BYCALLER);
  __fsetlocking (stdout, FSETLOCKING_BYCALLER);
  __fsetlocking (stderr, FSETLOCKING_BYCALLER);

  /* Set locale.  */
  setlocale (LC_ALL, "");

  /* Make sure the message catalog can be found.  */
  bindtextdomain (PACKAGE_TARNAME, LOCALEDIR);

  /* Initialize the message catalog.  */
  textdomain (PACKAGE_TARNAME);

  /* Parse and process arguments.  */
  if (argp_parse (&argp, argc, argv, 0, &remaining, NULL) != 0)
    return EXIT_FAILURE;

  if (reloc_debug && debug_fname == NULL)
    error_exit (0, _("--reloc-debug-sections used without -f"));

  if (reloc_debug_only &&
      (debug_fname != NULL || remove_secs != NULL
       || remove_comment == true || remove_debug == true))
    error_exit (0,
		_("--reloc-debug-sections-only incompatible with -f, -g, --remove-comment and --remove-section"));

  /* Tell the library which version we are expecting.  */
  elf_version (EV_CURRENT);

  if (remaining == argc)
    /* The user didn't specify a name so we use a.out.  */
    result = process_file ("a.out");
  else
    {
      /* If we have seen the '-o' or '-f' option there must be exactly one
	 input file.  */
      if ((output_fname != NULL || debug_fname != NULL)
	  && remaining + 1 < argc)
	error_exit (0, _("Only one input file allowed together with '-o' and '-f'"));

      /* Process all the remaining files.  */
      do
	result |= process_file (argv[remaining]);
      while (++remaining < argc);
    }

  free_patterns ();
  return result;
}


/* Handle program arguments.  */
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case 'f':
      if (debug_fname != NULL)
	{
	  error (0, 0, _("-f option specified twice"));
	  return EINVAL;
	}
      debug_fname = arg;
      break;

    case 'F':
      if (debug_fname_embed != NULL)
	{
	  error (0, 0, _("-F option specified twice"));
	  return EINVAL;
	}
      debug_fname_embed = arg;
      break;

    case 'o':
      if (output_fname != NULL)
	{
	  error (0, 0, _("-o option specified twice"));
	  return EINVAL;
	}
      output_fname = arg;
      break;

    case 'p':
      preserve_dates = true;
      break;

    case OPT_RELOC_DEBUG:
      reloc_debug = true;
      break;

    case OPT_RELOC_DEBUG_ONLY:
      reloc_debug_only = true;
      break;

    case OPT_REMOVE_COMMENT:
      remove_comment = true;
      break;

    case 'R':
      if (fnmatch (arg, ".comment", FNM_EXTMATCH) == 0)
	remove_comment = true;
      add_pattern (&remove_secs, arg);
      break;

    case OPT_KEEP_SECTION:
      add_pattern (&keep_secs, arg);
      break;

    case 'g':
    case 'd':
    case 'S':
      remove_debug = true;
      break;

    case OPT_STRIP_SECTIONS:
      remove_shdrs = true;
      break;

    case OPT_PERMISSIVE:
      permissive = true;
      break;

    case 's':			/* Ignored for compatibility.  */
      break;

    case ARGP_KEY_SUCCESS:
      if (remove_comment == true
	  && section_name_matches (keep_secs, ".comment"))
	{
	  argp_error (state,
		      _("cannot both keep and remove .comment section"));
	  return EINVAL;
	}
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

static const char *
secndx_name (Elf *elf, size_t ndx)
{
  size_t shstrndx;
  GElf_Shdr mem;
  Elf_Scn *sec = elf_getscn (elf, ndx);
  GElf_Shdr *shdr = gelf_getshdr (sec, &mem);
  if (shdr == NULL || elf_getshdrstrndx (elf, &shstrndx) < 0)
    return "???";
  return elf_strptr (elf, shstrndx, shdr->sh_name) ?: "???";
}

/* Get the extended section index table data for a symbol table section.  */
static Elf_Data *
get_xndxdata (Elf *elf, Elf_Scn *symscn)
{
  Elf_Data *xndxdata = NULL;
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr = gelf_getshdr (symscn, &shdr_mem);
  if (shdr != NULL && shdr->sh_type == SHT_SYMTAB)
    {
      size_t scnndx = elf_ndxscn (symscn);
      Elf_Scn *xndxscn = NULL;
      while ((xndxscn = elf_nextscn (elf, xndxscn)) != NULL)
	{
	  GElf_Shdr xndxshdr_mem;
	  GElf_Shdr *xndxshdr = gelf_getshdr (xndxscn, &xndxshdr_mem);

	  if (xndxshdr != NULL
	      && xndxshdr->sh_type == SHT_SYMTAB_SHNDX
	      && xndxshdr->sh_link == scnndx)
	    {
	      xndxdata = elf_getdata (xndxscn, NULL);
	      break;
	    }
	}
    }

  return xndxdata;
}

/* Updates the shdrstrndx for the given Elf by updating the Ehdr and
   possibly the section zero extension field.  Returns zero on success.  */
static int
update_shdrstrndx (Elf *elf, size_t shdrstrndx)
{
  GElf_Ehdr ehdr;
  if (gelf_getehdr (elf, &ehdr) == 0)
    return 1;

  if (shdrstrndx < SHN_LORESERVE)
    ehdr.e_shstrndx = shdrstrndx;
  else
    {
      ehdr.e_shstrndx = SHN_XINDEX;
      Elf_Scn *scn0 = elf_getscn (elf, 0);
      GElf_Shdr shdr0_mem;
      GElf_Shdr *shdr0 = gelf_getshdr (scn0, &shdr0_mem);
      if (shdr0 == NULL)
	return 1;

      shdr0->sh_link = shdrstrndx;
      if (gelf_update_shdr (scn0, shdr0) == 0)
	return 1;
    }

  if (unlikely (gelf_update_ehdr (elf, &ehdr) == 0))
    return 1;

  return 0;
}


/* Apply one relocation.  Returns true when trivial
   relocation actually done.  */
static bool
relocate (Elf *elf, GElf_Addr offset, const GElf_Sxword addend,
	  Elf_Data *tdata, unsigned int ei_data, const char *fname,
	  bool is_rela, GElf_Sym *sym, int addsub, Elf_Type type)
{
  /* These are the types we can relocate.  */
#define TYPES   DO_TYPE (BYTE, Byte); DO_TYPE (HALF, Half);		\
      DO_TYPE (WORD, Word); DO_TYPE (SWORD, Sword);		\
      DO_TYPE (XWORD, Xword); DO_TYPE (SXWORD, Sxword)

  size_t size;

#define DO_TYPE(NAME, Name) GElf_##Name Name;
  union { TYPES; } tmpbuf;
#undef DO_TYPE

  switch (type)
    {
#define DO_TYPE(NAME, Name)				\
      case ELF_T_##NAME:			\
	size = sizeof (GElf_##Name);	\
	tmpbuf.Name = 0;			\
	break;
      TYPES;
#undef DO_TYPE
    default:
      return false;
    }

  if (offset > tdata->d_size
      || tdata->d_size - offset < size)
    {
      cleanup_debug ();
      error_exit (0, _("bad relocation"));
    }

  /* When the symbol value is zero then for SHT_REL
     sections this is all that needs to be checked.
     The addend is contained in the original data at
     the offset already.  So if the (section) symbol
     address is zero and the given addend is zero
     just remove the relocation, it isn't needed
     anymore.  */
  if (addend == 0 && sym->st_value == 0)
    return true;

  Elf_Data tmpdata =
    {
      .d_type = type,
      .d_buf = &tmpbuf,
      .d_size = size,
      .d_version = EV_CURRENT,
    };
  Elf_Data rdata =
    {
      .d_type = type,
      .d_buf = tdata->d_buf + offset,
      .d_size = size,
      .d_version = EV_CURRENT,
    };

  GElf_Addr value = sym->st_value;
  if (is_rela)
    {
      /* For SHT_RELA sections we just take the
	 given addend and add it to the value.  */
      value += addend;
      /* For ADD/SUB relocations we need to fetch the
	 current section contents.  */
      if (addsub != 0)
	{
	  Elf_Data *d = gelf_xlatetom (elf, &tmpdata,
				       &rdata,
				       ei_data);
	  if (d == NULL)
	    INTERNAL_ERROR (fname);
	  assert (d == &tmpdata);
	}
    }
  else
    {
      /* For SHT_REL sections we have to peek at
	 what is already in the section at the given
	 offset to get the addend.  */
      Elf_Data *d = gelf_xlatetom (elf, &tmpdata,
				   &rdata,
				   ei_data);
      if (d == NULL)
	INTERNAL_ERROR (fname);
      assert (d == &tmpdata);
    }

  switch (type)
    {
#define DO_TYPE(NAME, Name)					 \
      case ELF_T_##NAME:				 \
	if (addsub < 0)				 \
	  tmpbuf.Name -= (GElf_##Name) value;	 \
	else					 \
	  tmpbuf.Name += (GElf_##Name) value;	 \
	break;
      TYPES;
#undef DO_TYPE
    default:
      abort ();
    }

  /* Now finally put in the new value.  */
  Elf_Data *s = gelf_xlatetof (elf, &rdata,
			       &tmpdata,
			       ei_data);
  if (s == NULL)
    INTERNAL_ERROR (fname);
  assert (s == &rdata);

  return true;
}

/* Remove any relocations between debug sections in ET_REL
   for the debug file when requested.  These relocations are always
   zero based between the unallocated sections.  */
static void
remove_debug_relocations (Ebl *ebl, Elf *elf, GElf_Ehdr *ehdr,
			  const char *fname, size_t shstrndx)
{
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      /* We need the actual section and header from the elf
	 not just the cached original in shdr_info because we
	 might want to change the size.  */
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
      if (shdr != NULL
	  && (shdr->sh_type == SHT_REL || shdr->sh_type == SHT_RELA))
	{
	  /* Make sure that this relocation section points to a
	     section to relocate with contents, that isn't
	     allocated and that is a debug section.  */
	  Elf_Scn *tscn = elf_getscn (elf, shdr->sh_info);
	  GElf_Shdr tshdr_mem;
	  GElf_Shdr *tshdr = gelf_getshdr (tscn, &tshdr_mem);
	  if (tshdr == NULL
	      || tshdr->sh_type == SHT_NOBITS
	      || tshdr->sh_size == 0
	      || (tshdr->sh_flags & SHF_ALLOC) != 0)
	    continue;

	  const char *tname =  elf_strptr (elf, shstrndx,
					   tshdr->sh_name);
	  if (! tname || ! ebl_debugscn_p (ebl, tname))
	    continue;

	  /* OK, lets relocate all trivial cross debug section
	     relocations. */
	  Elf_Data *reldata = elf_getdata (scn, NULL);
	  if (reldata == NULL || reldata->d_buf == NULL)
	    INTERNAL_ERROR (fname);

	  /* Make sure we adjust the uncompressed debug data
	     (and recompress if necessary at the end).  */
	  GElf_Chdr tchdr;
	  int tcompress_type = 0;
	  bool is_gnu_compressed = false;
	  if (startswith (tname, ".zdebug"))
	    {
	      is_gnu_compressed = true;
	      if (elf_compress_gnu (tscn, 0, 0) != 1)
		INTERNAL_ERROR (fname);
	    }
	  else
	    {
	      if (gelf_getchdr (tscn, &tchdr) != NULL)
		{
		  tcompress_type = tchdr.ch_type;
		  if (elf_compress (tscn, 0, 0) != 1)
		    INTERNAL_ERROR (fname);
		}
	    }

	  Elf_Data *tdata = elf_getdata (tscn, NULL);
	  if (tdata == NULL || tdata->d_buf == NULL
	      || tdata->d_type != ELF_T_BYTE)
	    INTERNAL_ERROR (fname);

	  /* Pick up the symbol table and shndx table to
	     resolve relocation symbol indexes.  */
	  Elf64_Word symt = shdr->sh_link;
	  Elf_Data *symdata, *xndxdata;
	  Elf_Scn * symscn = elf_getscn (elf, symt);
	  symdata = elf_getdata (symscn, NULL);
	  xndxdata = get_xndxdata (elf, symscn);
	  if (symdata == NULL)
	    INTERNAL_ERROR (fname);

	  if (shdr->sh_entsize == 0)
	    INTERNAL_ERROR (fname);

	  size_t nrels = shdr->sh_size / shdr->sh_entsize;
	  size_t next = 0;
	  const bool is_rela = (shdr->sh_type == SHT_RELA);
	  const unsigned int ei_data = ehdr->e_ident[EI_DATA];

	  for (size_t relidx = 0; relidx < nrels; ++relidx)
	    {
	      int rtype, symndx, offset, addend;
	      union { GElf_Rela rela; GElf_Rel rel; } mem;
	      void *rel_p; /* Pointer to either rela or rel above */

	      if (is_rela)
		{
		  GElf_Rela *r = gelf_getrela (reldata, relidx, &mem.rela);
		  if (r == NULL)
		    INTERNAL_ERROR (fname);
		  offset = r->r_offset;
		  addend = r->r_addend;
		  rtype = GELF_R_TYPE (r->r_info);
		  symndx = GELF_R_SYM (r->r_info);
		  rel_p = r;
		}
	      else
		{
		  GElf_Rel *r = gelf_getrel (reldata, relidx, &mem.rel);
		  if (r == NULL)
		    INTERNAL_ERROR (fname);
		  offset = r->r_offset;
		  addend = 0;
		  rtype = GELF_R_TYPE (r->r_info);
		  symndx = GELF_R_SYM (r->r_info);
		  rel_p = r;
		}

	      /* R_*_NONE relocs can always just be removed.  */
	      if (rtype == 0)
		continue;

	      /* We only do simple absolute relocations.  */
	      int addsub = 0;
	      Elf_Type type = ebl_reloc_simple_type (ebl, rtype, &addsub);
	      if (type == ELF_T_NUM)
		goto relocate_failed;

	      /* And only for relocations against other debug sections.  */
	      GElf_Sym sym_mem;
	      Elf32_Word xndx;
	      GElf_Sym *sym = gelf_getsymshndx (symdata, xndxdata,
						symndx, &sym_mem,
						  &xndx);
	      if (sym == NULL)
		INTERNAL_ERROR (fname);
	      Elf32_Word sec = (sym->st_shndx == SHN_XINDEX
				? xndx : sym->st_shndx);

	      bool dbg_scn = ebl_debugscn_p (ebl, secndx_name (elf, sec));

	      if (!dbg_scn)
		goto relocate_failed;

	      if (! relocate (elf, offset, addend,
			    tdata, ei_data, fname, is_rela,
			    sym, addsub, type))
	      goto relocate_failed;

	      continue; /* Next */

relocate_failed:
	      if (relidx != next)
		{
		  int updated;
		  if (is_rela)
		    updated = gelf_update_rela (reldata, next, rel_p);
		  else
		    updated = gelf_update_rel (reldata, next, rel_p);
		  if (updated == 0)
		    INTERNAL_ERROR (fname);
		}
	      ++next;
	    }

	  nrels = next;
	  shdr->sh_size = reldata->d_size = nrels * shdr->sh_entsize;
	  if (gelf_update_shdr (scn, shdr) == 0)
	    INTERNAL_ERROR (fname);

	  if (is_gnu_compressed)
	    {
	      if (elf_compress_gnu (tscn, 1, ELF_CHF_FORCE) != 1)
		INTERNAL_ERROR (fname);
	    }
	  else if (tcompress_type != 0)
	    {
	      if (elf_compress (tscn, tcompress_type, ELF_CHF_FORCE) != 1)
		INTERNAL_ERROR (fname);
	    }
	}
    }
}

static int
process_file (const char *fname)
{
  /* If we have to preserve the modify and access timestamps get them
     now.  We cannot use fstat() after opening the file since the open
     would change the access time.  */
  struct stat pre_st;
  struct timespec tv[2];
 again:
  if (preserve_dates)
    {
      if (stat (fname, &pre_st) != 0)
	{
	  error (0, errno, _("cannot stat input file '%s'"), fname);
	  return 1;
	}

      /* If we have to preserve the timestamp, we need it in the
	 format utimes() understands.  */
      tv[0] = pre_st.st_atim;
      tv[1] = pre_st.st_mtim;
    }

  /* Open the file.  */
  int fd = open (fname, output_fname == NULL ? O_RDWR : O_RDONLY);
  if (fd == -1)
    {
      error (0, errno, _("while opening '%s'"), fname);
      return 1;
    }

  /* We always use fstat() even if we called stat() before.  This is
     done to make sure the information returned by stat() is for the
     same file.  */
  struct stat st;
  if (fstat (fd, &st) != 0)
    {
      error (0, errno, _("cannot stat input file '%s'"), fname);
      return 1;
    }
  /* Paranoid mode on.  */
  if (preserve_dates
      && (st.st_ino != pre_st.st_ino || st.st_dev != pre_st.st_dev))
    {
      /* We detected a race.  Try again.  */
      close (fd);
      goto again;
    }

  /* Now get the ELF descriptor.  */
  Elf *elf = elf_begin (fd, output_fname == NULL ? ELF_C_RDWR : ELF_C_READ,
			NULL);
  int result;
  switch (elf_kind (elf))
    {
    case ELF_K_ELF:
      result = handle_elf (fd, elf, NULL, fname, st.st_mode & ACCESSPERMS,
			   preserve_dates ? tv : NULL);
      break;

    case ELF_K_AR:
      /* It is not possible to strip the content of an archive direct
	 the output to a specific file.  */
      if (unlikely (output_fname != NULL || debug_fname != NULL))
	{
	  error (0, 0, _("%s: cannot use -o or -f when stripping archive"),
		 fname);
	  result = 1;
	}
      else
	{
	  /* We would like to support ar archives, but currently it just
	     doesn't work at all since we call elf_clone on the members
	     which doesn't really support ar members.
	     result = handle_ar (fd, elf, NULL, fname,
				 preserve_dates ? tv : NULL);
	   */
	  error (0, 0, _("%s: no support for stripping archive"),
		 fname);
	  result = 1;
	}
      break;

    default:
      error (0, 0, _("%s: File format not recognized"), fname);
      result = 1;
      break;
    }

  if (unlikely (elf_end (elf) != 0))
    INTERNAL_ERROR (fname);

  close (fd);

  return result;
}

/* Processing for --reloc-debug-sections-only.  */
static int
handle_debug_relocs (Elf *elf, Ebl *ebl, Elf *new_elf,
		     GElf_Ehdr *ehdr, const char *fname, size_t shstrndx,
		     GElf_Off *last_offset, GElf_Xword *last_size)
{

  /* Copy over the ELF header.  */
  if (gelf_update_ehdr (new_elf, ehdr) == 0)
    {
      error (0, 0, "couldn't update new ehdr: %s", elf_errmsg (-1));
      return 1;
    }

  /* Copy over sections and record end of allocated sections.  */
  GElf_Off lastoffset = 0;
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      /* Get the header.  */
      GElf_Shdr shdr;
      if (gelf_getshdr (scn, &shdr) == NULL)
	{
	  error (0, 0, "couldn't get shdr: %s", elf_errmsg (-1));
	  return 1;
	}

      /* Create new section.  */
      Elf_Scn *new_scn = elf_newscn (new_elf);
      if (new_scn == NULL)
	{
	  error (0, 0, "couldn't create new section: %s", elf_errmsg (-1));
	  return 1;
	}

      if (gelf_update_shdr (new_scn, &shdr) == 0)
	{
	  error (0, 0, "couldn't update shdr: %s", elf_errmsg (-1));
	  return 1;
	}

      /* Copy over section data.  */
      Elf_Data *data = NULL;
      while ((data = elf_getdata (scn, data)) != NULL)
	{
	  Elf_Data *new_data = elf_newdata (new_scn);
	  if (new_data == NULL)
	    {
	      error (0, 0, "couldn't create new section data: %s",
		     elf_errmsg (-1));
	      return 1;
	    }
	  *new_data = *data;
	}

      /* Record last offset of allocated section.  */
      if ((shdr.sh_flags & SHF_ALLOC) != 0)
	{
	  GElf_Off filesz = (shdr.sh_type != SHT_NOBITS
			     ? shdr.sh_size : 0);
	  if (lastoffset < shdr.sh_offset + filesz)
	    lastoffset = shdr.sh_offset + filesz;
	}
    }

  /* Make sure section header name table is setup correctly, we'll
     need it to determine whether to relocate sections.  */
  if (update_shdrstrndx (new_elf, shstrndx) != 0)
    {
      error (0, 0, "error updating shdrstrndx: %s", elf_errmsg (-1));
      return 1;
    }

  /* Adjust the relocation sections.  */
  remove_debug_relocations (ebl, new_elf, ehdr, fname, shstrndx);

  /* Adjust the offsets of the non-allocated sections, so they come after
     the allocated sections.  */
  scn = NULL;
  while ((scn = elf_nextscn (new_elf, scn)) != NULL)
    {
      /* Get the header.  */
      GElf_Shdr shdr;
      if (gelf_getshdr (scn, &shdr) == NULL)
	{
	  error (0, 0, "couldn't get shdr: %s", elf_errmsg (-1));
	  return 1;
	}

      /* Adjust non-allocated section offsets to be after any allocated.  */
      if ((shdr.sh_flags & SHF_ALLOC) == 0)
	{
	  shdr.sh_offset = ((lastoffset + shdr.sh_addralign - 1)
			    & ~((GElf_Off) (shdr.sh_addralign - 1)));
	  if (gelf_update_shdr (scn, &shdr) == 0)
	    {
	      error (0, 0, "couldn't update shdr: %s", elf_errmsg (-1));
	      return 1;
	    }

	  GElf_Off filesz = (shdr.sh_type != SHT_NOBITS
			     ? shdr.sh_size : 0);
	  lastoffset = shdr.sh_offset + filesz;
	  *last_offset = shdr.sh_offset;
	  *last_size = filesz;
	}
    }

  return 0;
}

/* Update section headers when the data size has changed.
   We also update the SHT_NOBITS section in the debug
   file so that the section headers match in sh_size.  */
static inline void
update_section_size (Elf_Scn *scn,
		     const Elf_Data *newdata,
		     Elf *debugelf,
		     size_t cnt,
		     const char *fname)
{
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
  shdr->sh_size = newdata->d_size;
  if (gelf_update_shdr (scn, shdr) == 0)
    INTERNAL_ERROR (fname);
  if (debugelf != NULL)
    {
      /* libelf will use d_size to set sh_size.  */
      Elf_Data *debugdata = elf_getdata (elf_getscn (debugelf,
						     cnt), NULL);
      if (debugdata == NULL)
	INTERNAL_ERROR (fname);
      debugdata->d_size = newdata->d_size;
    }
}

/* Maximum size of array allocated on stack.  */
#define MAX_STACK_ALLOC	(400 * 1024)

static int
handle_elf (int fd, Elf *elf, const char *prefix, const char *fname,
	    mode_t mode, struct timespec tvp[2])
{
  size_t prefix_len = prefix == NULL ? 0 : strlen (prefix);
  size_t fname_len = strlen (fname) + 1;
  char *fullname = alloca (prefix_len + 1 + fname_len);
  char *cp = fullname;
  Elf *debugelf = NULL;
  tmp_debug_fname = NULL;
  int result = 0;
  size_t shdridx = 0;
  GElf_Off lastsec_offset = 0;
  Elf64_Xword lastsec_size = 0;
  size_t shstrndx;
  struct shdr_info
  {
    Elf_Scn *scn;
    GElf_Shdr shdr;
    Elf_Data *data;
    Elf_Data *debug_data;
    const char *name;
    Elf32_Word idx;		/* Index in new file.  */
    Elf32_Word old_sh_link;	/* Original value of shdr.sh_link.  */
    Elf32_Word symtab_idx;
    Elf32_Word version_idx;
    Elf32_Word group_idx;
    Elf32_Word group_cnt;
    Elf_Scn *newscn;
    Dwelf_Strent *se;
    Elf32_Word *newsymidx;
  } *shdr_info = NULL;
  Elf_Scn *scn;
  size_t cnt;
  size_t idx;
  bool changes;
  GElf_Ehdr newehdr_mem;
  GElf_Ehdr *newehdr;
  GElf_Ehdr debugehdr_mem;
  GElf_Ehdr *debugehdr;
  Dwelf_Strtab *shst = NULL;
  Elf_Data debuglink_crc_data;
  bool any_symtab_changes = false;
  Elf_Data *shstrtab_data = NULL;
  void *debuglink_buf = NULL;

  /* Create the full name of the file.  */
  if (prefix != NULL)
    {
      cp = mempcpy (cp, prefix, prefix_len);
      *cp++ = ':';
    }
  memcpy (cp, fname, fname_len);

  /* If we are not replacing the input file open a new file here.  */
  if (output_fname != NULL)
    {
      fd = open (output_fname, O_RDWR | O_CREAT, mode);
      if (unlikely (fd == -1))
	{
	  error (0, errno, _("cannot open '%s'"), output_fname);
	  return 1;
	}
    }

  debug_fd = -1;

  /* Get the EBL handling.  Removing all debugging symbols with the -g
     option or resolving all relocations between debug sections with
     the --reloc-debug-sections option are currently the only reasons
     we need EBL so don't open the backend unless necessary.  */
  Ebl *ebl = NULL;
  if (remove_debug || reloc_debug || reloc_debug_only)
    {
      ebl = ebl_openbackend (elf);
      if (ebl == NULL)
	{
	  error (0, errno, _("cannot open EBL backend"));
	  result = 1;
	  goto fail;
	}
    }

  /* Open the additional file the debug information will be stored in.  */
  if (debug_fname != NULL)
    {
      /* Create a temporary file name.  We do not want to overwrite
	 the debug file if the file would not contain any
	 information.  */
      size_t debug_fname_len = strlen (debug_fname);
      tmp_debug_fname = xmalloc (debug_fname_len + sizeof (".XXXXXX"));
      strcpy (mempcpy (tmp_debug_fname, debug_fname, debug_fname_len),
	      ".XXXXXX");

      debug_fd = mkstemp (tmp_debug_fname);
      if (unlikely (debug_fd == -1))
	{
	  error (0, errno, _("cannot open '%s'"), debug_fname);
	  result = 1;
	  goto fail;
	}
    }

  /* Get the information from the old file.  */
  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr = gelf_getehdr (elf, &ehdr_mem);
  if (ehdr == NULL)
    INTERNAL_ERROR (fname);

  /* Get the section header string table index.  */
  if (unlikely (elf_getshdrstrndx (elf, &shstrndx) < 0))
    {
      cleanup_debug ();
      error_exit (0, _("cannot get section header string table index"));
    }

  /* Get the number of phdrs in the old file.  */
  size_t phnum;
  if (elf_getphdrnum (elf, &phnum) != 0)
    {
      cleanup_debug ();
      error_exit (0, _("cannot get number of phdrs"));
    }

  /* We now create a new ELF descriptor for the same file.  We
     construct it almost exactly in the same way with some information
     dropped.  */
  Elf *newelf;
  if (output_fname != NULL)
    newelf = elf_begin (fd, ELF_C_WRITE_MMAP, NULL);
  else
    newelf = elf_clone (elf, ELF_C_EMPTY);

  if (unlikely (gelf_newehdr (newelf, gelf_getclass (elf)) == 0))
    {
      error (0, 0, _("cannot create new ehdr for file '%s': %s"),
	     output_fname ?: fname, elf_errmsg (-1));
      goto fail;
    }

  /* Copy over the old program header if needed.  */
  if (phnum > 0)
    {
      if (unlikely (gelf_newphdr (newelf, phnum) == 0))
	{
	  error (0, 0, _("cannot create new phdr for file '%s': %s"),
		 output_fname ?: fname, elf_errmsg (-1));
	  goto fail;
	}

      for (cnt = 0; cnt < phnum; ++cnt)
	{
	  GElf_Phdr phdr_mem;
	  GElf_Phdr *phdr = gelf_getphdr (elf, cnt, &phdr_mem);
	  if (phdr == NULL
	      || unlikely (gelf_update_phdr (newelf, cnt, phdr) == 0))
	    INTERNAL_ERROR (fname);
	}
    }

  if (reloc_debug_only)
    {
      if (handle_debug_relocs (elf, ebl, newelf, ehdr, fname, shstrndx,
			       &lastsec_offset, &lastsec_size) != 0)
	{
	  result = 1;
	  goto fail_close;
	}
      idx = shstrndx;
      goto done; /* Skip all actual stripping operations.  */
    }

  if (debug_fname != NULL)
    {
      /* Also create an ELF descriptor for the debug file */
      debugelf = elf_begin (debug_fd, ELF_C_WRITE, NULL);
      if (unlikely (gelf_newehdr (debugelf, gelf_getclass (elf)) == 0))
	{
	  error (0, 0, _("cannot create new ehdr for file '%s': %s"),
		 debug_fname, elf_errmsg (-1));
	  goto fail_close;
	}

      /* Copy over the old program header if needed.  */
      if (phnum > 0)
	{
	  if (unlikely (gelf_newphdr (debugelf, phnum) == 0))
	    {
	      error (0, 0, _("cannot create new phdr for file '%s': %s"),
		     debug_fname, elf_errmsg (-1));
	      goto fail_close;
	    }

	  for (cnt = 0; cnt < phnum; ++cnt)
	    {
	      GElf_Phdr phdr_mem;
	      GElf_Phdr *phdr = gelf_getphdr (elf, cnt, &phdr_mem);
	      if (phdr == NULL
		  || unlikely (gelf_update_phdr (debugelf, cnt, phdr) == 0))
		INTERNAL_ERROR (fname);
	    }
	}
    }

  /* Number of sections.  */
  size_t shnum;
  if (unlikely (elf_getshdrnum (elf, &shnum) < 0))
    {
      error (0, 0, _("cannot determine number of sections: %s"),
	     elf_errmsg (-1));
      goto fail_close;
    }

  if (shstrndx >= shnum)
    goto illformed;

#define elf_assert(test) do { if (!(test)) goto illformed; } while (0)

  /* Storage for section information.  We leave room for two more
     entries since we unconditionally create a section header string
     table.  Maybe some weird tool created an ELF file without one.
     The other one is used for the debug link section.  */
  if ((shnum + 2) * sizeof (struct shdr_info) > MAX_STACK_ALLOC)
    shdr_info = xcalloc (shnum + 2, sizeof (struct shdr_info));
  else
    {
      shdr_info = (struct shdr_info *) alloca ((shnum + 2)
					       * sizeof (struct shdr_info));
      memset (shdr_info, '\0', (shnum + 2) * sizeof (struct shdr_info));
    }

  /* Track whether allocated sections all come before non-allocated ones.  */
  bool seen_allocated = false;
  bool seen_unallocated = false;
  bool mixed_allocated_unallocated = false;

  /* Prepare section information data structure.  */
  scn = NULL;
  cnt = 1;
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      /* This should always be true (i.e., there should not be any
	 holes in the numbering).  */
      elf_assert (elf_ndxscn (scn) == cnt);

      shdr_info[cnt].scn = scn;

      /* Get the header.  */
      if (gelf_getshdr (scn, &shdr_info[cnt].shdr) == NULL)
	INTERNAL_ERROR (fname);

      /* Normally (in non-ET_REL files) we see all allocated sections first,
	 then all non-allocated.  */
      if ((shdr_info[cnt].shdr.sh_flags & SHF_ALLOC) == 0)
	seen_unallocated = true;
      else
	{
	  if (seen_unallocated && seen_allocated)
	    mixed_allocated_unallocated = true;
	  seen_allocated = true;
	}

      /* Get the name of the section.  */
      shdr_info[cnt].name = elf_strptr (elf, shstrndx,
					shdr_info[cnt].shdr.sh_name);
      if (shdr_info[cnt].name == NULL)
	{
	illformed:
	  error (0, 0, _("illformed file '%s'"), fname);
	  goto fail_close;
	}

      /* Sanity check the user.  */
      if (section_name_matches (remove_secs, shdr_info[cnt].name))
	{
	  if ((shdr_info[cnt].shdr.sh_flags & SHF_ALLOC) != 0)
	    {
	      error (0, 0,
		     _("Cannot remove allocated section '%s'"),
		     shdr_info[cnt].name);
	      result = 1;
	      goto fail_close;
	    }

	  if (section_name_matches (keep_secs, shdr_info[cnt].name))
	    {
	      error (0, 0,
		     _("Cannot both keep and remove section '%s'"),
		     shdr_info[cnt].name);
	      result = 1;
	      goto fail_close;
	    }
	}

      /* Mark them as present but not yet investigated.  */
      shdr_info[cnt].idx = 1;

      /* Remember the shdr.sh_link value.  */
      shdr_info[cnt].old_sh_link = shdr_info[cnt].shdr.sh_link;
      if (shdr_info[cnt].old_sh_link >= shnum)
	goto illformed;

      /* Sections in files other than relocatable object files which
	 not loaded can be freely moved by us.  In theory we can also
	 freely move around allocated nobits sections.  But we don't
	 to keep the layout of all allocated sections as similar as
	 possible to the original file.  In relocatable object files
	 everything can be moved.  */
      if (phnum == 0
	  || (shdr_info[cnt].shdr.sh_flags & SHF_ALLOC) == 0)
	shdr_info[cnt].shdr.sh_offset = 0;

      /* If this is an extended section index table store an
	 appropriate reference.  */
      if (unlikely (shdr_info[cnt].shdr.sh_type == SHT_SYMTAB_SHNDX))
	{
	  elf_assert (shdr_info[shdr_info[cnt].shdr.sh_link].symtab_idx == 0);
	  shdr_info[shdr_info[cnt].shdr.sh_link].symtab_idx = cnt;
	}
      else if (unlikely (shdr_info[cnt].shdr.sh_type == SHT_GROUP))
	{
	  /* Cross-reference the sections contained in the section
	     group.  */
	  shdr_info[cnt].data = elf_getdata (shdr_info[cnt].scn, NULL);
	  if (shdr_info[cnt].data == NULL
	      || shdr_info[cnt].data->d_size < sizeof (Elf32_Word))
	    INTERNAL_ERROR (fname);

	  /* XXX Fix for unaligned access.  */
	  Elf32_Word *grpref = (Elf32_Word *) shdr_info[cnt].data->d_buf;
	  size_t inner;
	  for (inner = 1;
	       inner < shdr_info[cnt].data->d_size / sizeof (Elf32_Word);
	       ++inner)
	    {
	      if (grpref[inner] < shnum)
		shdr_info[grpref[inner]].group_idx = cnt;
	      else
		goto illformed;
	    }

	  if (inner == 1 || (inner == 2 && (grpref[0] & GRP_COMDAT) == 0))
	    /* If the section group contains only one element and this
	       is n COMDAT section we can drop it right away.  */
	    shdr_info[cnt].idx = 0;
	  else
	    shdr_info[cnt].group_cnt = inner - 1;
	}
      else if (unlikely (shdr_info[cnt].shdr.sh_type == SHT_GNU_versym))
	{
	  elf_assert (shdr_info[shdr_info[cnt].shdr.sh_link].version_idx == 0);
	  shdr_info[shdr_info[cnt].shdr.sh_link].version_idx = cnt;
	}

      /* If this section is part of a group make sure it is not
	 discarded right away.  */
      if ((shdr_info[cnt].shdr.sh_flags & SHF_GROUP) != 0)
	{
	  elf_assert (shdr_info[cnt].group_idx != 0);

	  if (shdr_info[shdr_info[cnt].group_idx].idx == 0)
	    {
	      /* The section group section might be removed.
		 Don't remove the SHF_GROUP flag.  The section is
		 either also removed, in which case the flag doesn't matter.
		 Or it moves with the group into the debug file, then
		 it will be reconnected with the new group and should
		 still have the flag set.  */
	      shdr_info[cnt].group_idx = 0;
	    }
	}

      /* Increment the counter.  */
      ++cnt;
    }

  /* Now determine which sections can go away.  The general rule is that
     all sections which are not used at runtime are stripped out.  But
     there are a few exceptions:

     - special sections named ".comment" and ".note" are kept
     - OS or architecture specific sections are kept since we might not
       know how to handle them
     - if a section is referred to from a section which is not removed
       in the sh_link or sh_info element it cannot be removed either
     - the user might have explicitly said to remove or keep a section
  */
  for (cnt = 1; cnt < shnum; ++cnt)
    /* Check whether the section can be removed.  Since we will create
       a new .shstrtab assume it will be removed too.  */
    if (remove_shdrs ? !(shdr_info[cnt].shdr.sh_flags & SHF_ALLOC)
	: (ebl_section_strip_p (ebl, &shdr_info[cnt].shdr,
				shdr_info[cnt].name, remove_comment,
				remove_debug)
	   || cnt == shstrndx
	   || section_name_matches (remove_secs, shdr_info[cnt].name)))
      {
	/* The user might want to explicitly keep this one.  */
	if (section_name_matches (keep_secs, shdr_info[cnt].name))
	  continue;

	/* For now assume this section will be removed.  */
	shdr_info[cnt].idx = 0;

	idx = shdr_info[cnt].group_idx;
	while (idx != 0)
	  {
	    /* The section group data is already loaded.  */
	    elf_assert (shdr_info[idx].data != NULL
			&& shdr_info[idx].data->d_buf != NULL
			&& shdr_info[idx].data->d_size >= sizeof (Elf32_Word));

	    /* If the references section group is a normal section
	       group and has one element remaining, or if it is an
	       empty COMDAT section group it is removed.  */
	    bool is_comdat = (((Elf32_Word *) shdr_info[idx].data->d_buf)[0]
			      & GRP_COMDAT) != 0;

	    --shdr_info[idx].group_cnt;
	    if ((!is_comdat && shdr_info[idx].group_cnt == 1)
		|| (is_comdat && shdr_info[idx].group_cnt == 0))
	      {
		shdr_info[idx].idx = 0;
		/* Continue recursively.  */
		idx = shdr_info[idx].group_idx;
	      }
	    else
	      break;
	  }
      }

  /* Mark the SHT_NULL section as handled.  */
  shdr_info[0].idx = 2;


  /* Handle exceptions: section groups and cross-references.  We might
     have to repeat this a few times since the resetting of the flag
     might propagate.  */
  do
    {
      changes = false;

      for (cnt = 1; cnt < shnum; ++cnt)
	{
	  if (shdr_info[cnt].idx == 0)
	    {
	      /* If a relocation section is marked as being removed make
		 sure the section it is relocating is removed, too.  */
	      if (shdr_info[cnt].shdr.sh_type == SHT_REL
		   || shdr_info[cnt].shdr.sh_type == SHT_RELA)
		{
		  if (shdr_info[cnt].shdr.sh_info >= shnum)
		    goto illformed;
		  else if (shdr_info[shdr_info[cnt].shdr.sh_info].idx != 0)
		    shdr_info[cnt].idx = 1;
		}

	      /* If a group section is marked as being removed make
		 sure all the sections it contains are being removed, too.  */
	      if (shdr_info[cnt].shdr.sh_type == SHT_GROUP)
		{
		  Elf32_Word *grpref;
		  grpref = (Elf32_Word *) shdr_info[cnt].data->d_buf;
		  for (size_t in = 1;
		       in < shdr_info[cnt].data->d_size / sizeof (Elf32_Word);
		       ++in)
		    if (grpref[in] < shnum)
		      {
			if (shdr_info[grpref[in]].idx != 0)
			  {
			    shdr_info[cnt].idx = 1;
			    break;
			  }
		      }
		    else
		      goto illformed;
		}
	    }

	  if (shdr_info[cnt].idx == 1)
	    {
	      /* The content of symbol tables we don't remove must not
		 reference any section which we do remove.  Otherwise
		 we cannot remove the section.  */
	      if (debug_fname != NULL
		  && shdr_info[cnt].debug_data == NULL
		  && (shdr_info[cnt].shdr.sh_type == SHT_DYNSYM
		      || shdr_info[cnt].shdr.sh_type == SHT_SYMTAB))
		{
		  /* Make sure the data is loaded.  */
		  if (shdr_info[cnt].data == NULL)
		    {
		      shdr_info[cnt].data
			= elf_getdata (shdr_info[cnt].scn, NULL);
		      if (shdr_info[cnt].data == NULL)
			INTERNAL_ERROR (fname);
		    }
		  Elf_Data *symdata = shdr_info[cnt].data;

		  /* If there is an extended section index table load it
		     as well.  */
		  if (shdr_info[cnt].symtab_idx != 0
		      && shdr_info[shdr_info[cnt].symtab_idx].data == NULL)
		    {
		      elf_assert (shdr_info[cnt].shdr.sh_type == SHT_SYMTAB);

		      shdr_info[shdr_info[cnt].symtab_idx].data
			= elf_getdata (shdr_info[shdr_info[cnt].symtab_idx].scn,
				       NULL);
		      if (shdr_info[shdr_info[cnt].symtab_idx].data == NULL)
			INTERNAL_ERROR (fname);
		    }
		  Elf_Data *xndxdata
		    = shdr_info[shdr_info[cnt].symtab_idx].data;

		  /* Go through all symbols and make sure the section they
		     reference is not removed.  */
		  size_t elsize = gelf_fsize (elf, ELF_T_SYM, 1, EV_CURRENT);

		  for (size_t inner = 0;
		       inner < shdr_info[cnt].data->d_size / elsize;
		       ++inner)
		    {
		      GElf_Sym sym_mem;
		      Elf32_Word xndx;
		      GElf_Sym *sym = gelf_getsymshndx (symdata, xndxdata,
							inner, &sym_mem,
							&xndx);
		      if (sym == NULL)
			INTERNAL_ERROR (fname);

		      size_t scnidx = sym->st_shndx;
		      if (scnidx == SHN_UNDEF || scnidx >= shnum
			  || (scnidx >= SHN_LORESERVE
			      && scnidx <= SHN_HIRESERVE
			      && scnidx != SHN_XINDEX)
			  /* Don't count in the section symbols.  */
			  || GELF_ST_TYPE (sym->st_info) == STT_SECTION)
			/* This is no section index, leave it alone.  */
			continue;
		      else if (scnidx == SHN_XINDEX)
			scnidx = xndx;

		      if (scnidx >= shnum)
			goto illformed;

		      if (shdr_info[scnidx].idx == 0)
			/* This symbol table has a real symbol in
			   a discarded section.  So preserve the
			   original table in the debug file.  Unless
			   it is a redundant data marker to a debug
			   (data only) section.  */
			if (! (ebl_section_strip_p (ebl,
						    &shdr_info[scnidx].shdr,
						    shdr_info[scnidx].name,
						    remove_comment,
						    remove_debug)
			       && ebl_data_marker_symbol (ebl, sym,
					elf_strptr (elf,
						    shdr_info[cnt].shdr.sh_link,
						    sym->st_name))))
			  shdr_info[cnt].debug_data = symdata;
		    }
		}

	      /* Cross referencing happens:
		 - for the cases the ELF specification says.  That are
		   + SHT_DYNAMIC in sh_link to string table
		   + SHT_HASH in sh_link to symbol table
		   + SHT_REL and SHT_RELA in sh_link to symbol table
		   + SHT_SYMTAB and SHT_DYNSYM in sh_link to string table
		   + SHT_GROUP in sh_link to symbol table
		   + SHT_SYMTAB_SHNDX in sh_link to symbol table
		   Other (OS or architecture-specific) sections might as
		   well use this field so we process it unconditionally.
		 - references inside section groups
		 - specially marked references in sh_info if the SHF_INFO_LINK
		 flag is set
	      */

	      if (shdr_info[shdr_info[cnt].shdr.sh_link].idx == 0)
		{
		  shdr_info[shdr_info[cnt].shdr.sh_link].idx = 1;
		  changes |= shdr_info[cnt].shdr.sh_link < cnt;
		}

	      /* Handle references through sh_info.  */
	      if (SH_INFO_LINK_P (&shdr_info[cnt].shdr))
		{
		  if (shdr_info[cnt].shdr.sh_info >= shnum)
		    goto illformed;
		  else if ( shdr_info[shdr_info[cnt].shdr.sh_info].idx == 0)
		    {
		      shdr_info[shdr_info[cnt].shdr.sh_info].idx = 1;
		      changes |= shdr_info[cnt].shdr.sh_info < cnt;
		    }
		}

	      /* Mark the section as investigated.  */
	      shdr_info[cnt].idx = 2;
	    }

	  if (debug_fname != NULL
	      && (shdr_info[cnt].idx == 0 || shdr_info[cnt].debug_data != NULL))
	    {
	      /* This section is being preserved in the debug file.
		 Sections it refers to must be preserved there too.

		 In this pass we mark sections to be preserved in both
		 files by setting the .debug_data pointer to the original
		 file's .data pointer.  Below, we'll copy the section
		 contents.  */
	      size_t shdr_indices[2] = { shdr_info[cnt].shdr.sh_link, 0 };
	      int n = 1;

	      if (SH_INFO_LINK_P (&shdr_info[cnt].shdr))
		{
		  shdr_indices[1] = shdr_info[cnt].shdr.sh_info;
		  n++;
		}

	      for (int j = 0; j < n; j++)
		{
		  size_t i = shdr_indices[j];
		  if (i != 0 && i < shnum + 2 && shdr_info[i].idx != 0
		      && shdr_info[i].debug_data == NULL)
		    {
		      if (shdr_info[i].data == NULL)
			shdr_info[i].data = elf_getdata (shdr_info[i].scn, NULL);
		      if (shdr_info[i].data == NULL)
			INTERNAL_ERROR (fname);

		      shdr_info[i].debug_data = shdr_info[i].data;
		      changes |= i < cnt;
		    }
		}
	    }
	}
    }
  while (changes);

  /* Copy the removed sections to the debug output file.
     The ones that are not removed in the stripped file are SHT_NOBITS.  */
  if (debug_fname != NULL)
    {
      for (cnt = 1; cnt < shnum; ++cnt)
	{
	  scn = elf_newscn (debugelf);
	  if (scn == NULL)
	    {
	      cleanup_debug ();
	      error_exit (0, _("while generating output file: %s"),
			  elf_errmsg (-1));
	    }

	  bool discard_section = (shdr_info[cnt].idx > 0
				  && shdr_info[cnt].debug_data == NULL
				  && shdr_info[cnt].shdr.sh_type != SHT_NOTE
				  && shdr_info[cnt].shdr.sh_type != SHT_GROUP
				  && cnt != shstrndx);

	  /* Set the section header in the new file.  */
	  GElf_Shdr debugshdr = shdr_info[cnt].shdr;
	  if (discard_section)
	    debugshdr.sh_type = SHT_NOBITS;

	  if (unlikely (gelf_update_shdr (scn, &debugshdr) == 0))
	    /* There cannot be any overflows.  */
	    INTERNAL_ERROR (fname);

	  /* Get the data from the old file if necessary. */
	  if (shdr_info[cnt].data == NULL)
	    {
	      shdr_info[cnt].data = elf_getdata (shdr_info[cnt].scn, NULL);
	      if (shdr_info[cnt].data == NULL)
		INTERNAL_ERROR (fname);
	    }

	  /* Set the data.  This is done by copying from the old file.  */
	  Elf_Data *debugdata = elf_newdata (scn);
	  if (debugdata == NULL)
	    INTERNAL_ERROR (fname);

	  /* Copy the structure.  This data may be modified in place
	     before we write out the file.  */
	  *debugdata = *shdr_info[cnt].data;
	  if (discard_section)
	    debugdata->d_buf = NULL;
	  else if (shdr_info[cnt].debug_data != NULL
		   || shdr_info[cnt].shdr.sh_type == SHT_GROUP)
	    {
	      /* Copy the original data before it gets modified.  */
	      shdr_info[cnt].debug_data = debugdata;
	      if (debugdata->d_buf == NULL)
		INTERNAL_ERROR (fname);
	      debugdata->d_buf = memcpy (xmalloc (debugdata->d_size),
					 debugdata->d_buf, debugdata->d_size);
	    }
	}

      /* Finish the ELF header.  Fill in the fields not handled by
	 libelf from the old file.  */
      debugehdr = gelf_getehdr (debugelf, &debugehdr_mem);
      if (debugehdr == NULL)
	INTERNAL_ERROR (fname);

      memcpy (debugehdr->e_ident, ehdr->e_ident, EI_NIDENT);
      debugehdr->e_type = ehdr->e_type;
      debugehdr->e_machine = ehdr->e_machine;
      debugehdr->e_version = ehdr->e_version;
      debugehdr->e_entry = ehdr->e_entry;
      debugehdr->e_flags = ehdr->e_flags;

      if (unlikely (gelf_update_ehdr (debugelf, debugehdr) == 0))
	{
	  error (0, 0, _("%s: error while updating ELF header: %s"),
		 debug_fname, elf_errmsg (-1));
	  result = 1;
	  goto fail_close;
	}

      size_t shdrstrndx;
      if (elf_getshdrstrndx (elf, &shdrstrndx) < 0)
	{
	  error (0, 0, _("%s: error while getting shdrstrndx: %s"),
		 fname, elf_errmsg (-1));
	  result = 1;
	  goto fail_close;
	}

      if (update_shdrstrndx (debugelf, shdrstrndx) != 0)
	{
	  error (0, 0, _("%s: error updating shdrstrndx: %s"),
		 debug_fname, elf_errmsg (-1));
	  result = 1;
	  goto fail_close;
	}
    }

  /* Although we always create a new section header string table we
     don't explicitly mark the existing one as unused.  It can still
     be used through a symbol table section we are keeping.  If not it
     will already be marked as unused.  */

  /* We need a string table for the section headers.  */
  shst = dwelf_strtab_init (true);
  if (shst == NULL)
    {
      cleanup_debug ();
      error_exit (errno, _("while preparing output for '%s'"),
		  output_fname ?: fname);
    }

  /* Assign new section numbers.  */
  shdr_info[0].idx = 0;
  for (cnt = idx = 1; cnt < shnum; ++cnt)
    if (shdr_info[cnt].idx > 0)
      {
	shdr_info[cnt].idx = idx++;

	/* Create a new section.  */
	shdr_info[cnt].newscn = elf_newscn (newelf);
	if (shdr_info[cnt].newscn == NULL)
	  {
	    cleanup_debug ();
	    error_exit (0,
			_("while generating output file: %s"),
		   elf_errmsg (-1));
	  }

	elf_assert (elf_ndxscn (shdr_info[cnt].newscn) == shdr_info[cnt].idx);

	/* Add this name to the section header string table.  */
	shdr_info[cnt].se = dwelf_strtab_add (shst, shdr_info[cnt].name);
      }

  /* Test whether we are doing anything at all.  Either all removable
     sections are already gone.  Or the only section we would remove is
     the .shstrtab section which we would add again.  */
  bool removing_sections = !(cnt == idx
			     || (cnt == idx + 1
				 && shdr_info[shstrndx].idx == 0));
  if (output_fname == NULL && !removing_sections)
      goto fail_close;

  /* Create the reference to the file with the debug info (if any).  */
  if (debug_fname != NULL && !remove_shdrs && removing_sections)
    {
      /* Add the section header string table section name.  */
      shdr_info[cnt].se = dwelf_strtab_add_len (shst, ".gnu_debuglink", 15);
      shdr_info[cnt].idx = idx++;

      /* Create the section header.  */
      shdr_info[cnt].shdr.sh_type = SHT_PROGBITS;
      shdr_info[cnt].shdr.sh_flags = 0;
      shdr_info[cnt].shdr.sh_addr = 0;
      shdr_info[cnt].shdr.sh_link = SHN_UNDEF;
      shdr_info[cnt].shdr.sh_info = SHN_UNDEF;
      shdr_info[cnt].shdr.sh_entsize = 0;
      shdr_info[cnt].shdr.sh_addralign = 4;
      /* We set the offset to zero here.  Before we write the ELF file the
	 field must have the correct value.  This is done in the final
	 loop over all section.  Then we have all the information needed.  */
      shdr_info[cnt].shdr.sh_offset = 0;

      /* Create the section.  */
      shdr_info[cnt].newscn = elf_newscn (newelf);
      if (shdr_info[cnt].newscn == NULL)
	{
	  cleanup_debug ();
	  error_exit (0, _("while create section header section: %s"),
		      elf_errmsg (-1));
	}
      elf_assert (elf_ndxscn (shdr_info[cnt].newscn) == shdr_info[cnt].idx);

      shdr_info[cnt].data = elf_newdata (shdr_info[cnt].newscn);
      if (shdr_info[cnt].data == NULL)
	{
	  cleanup_debug ();
	  error_exit (0, _("cannot allocate section data: %s"),
		      elf_errmsg (-1));
	}

      const char *debug_basename = xbasename (debug_fname_embed ?: debug_fname);
      off_t crc_offset = strlen (debug_basename) + 1;
      /* Align to 4 byte boundary */
      crc_offset = ((crc_offset - 1) & ~3) + 4;

      shdr_info[cnt].data->d_align = 4;
      shdr_info[cnt].shdr.sh_size = shdr_info[cnt].data->d_size
	= crc_offset + 4;
      debuglink_buf = xcalloc (1, shdr_info[cnt].data->d_size);
      shdr_info[cnt].data->d_buf = debuglink_buf;

      strcpy (shdr_info[cnt].data->d_buf, debug_basename);

      /* Cache this Elf_Data describing the CRC32 word in the section.
	 We'll fill this in when we have written the debug file.  */
      debuglink_crc_data = *shdr_info[cnt].data;
      debuglink_crc_data.d_buf = ((char *) debuglink_crc_data.d_buf
				  + crc_offset);
      debuglink_crc_data.d_size = 4;

      /* One more section done.  */
      ++cnt;
    }

  /* Index of the section header table in the shdr_info array.  */
  shdridx = cnt;

  /* Add the section header string table section name.  */
  shdr_info[cnt].se = dwelf_strtab_add_len (shst, ".shstrtab", 10);
  shdr_info[cnt].idx = idx;

  /* Create the section header.  */
  shdr_info[cnt].shdr.sh_type = SHT_STRTAB;
  shdr_info[cnt].shdr.sh_flags = 0;
  shdr_info[cnt].shdr.sh_addr = 0;
  shdr_info[cnt].shdr.sh_link = SHN_UNDEF;
  shdr_info[cnt].shdr.sh_info = SHN_UNDEF;
  shdr_info[cnt].shdr.sh_entsize = 0;
  /* We set the offset to zero here.  Before we write the ELF file the
     field must have the correct value.  This is done in the final
     loop over all section.  Then we have all the information needed.  */
  shdr_info[cnt].shdr.sh_offset = 0;
  shdr_info[cnt].shdr.sh_addralign = 1;

  /* Create the section.  */
  shdr_info[cnt].newscn = elf_newscn (newelf);
  if (shdr_info[cnt].newscn == NULL)
    {
      cleanup_debug ();
      error_exit (0, _("while create section header section: %s"),
		  elf_errmsg (-1));
    }
  elf_assert (elf_ndxscn (shdr_info[cnt].newscn) == idx);

  /* Finalize the string table and fill in the correct indices in the
     section headers.  */
  shstrtab_data = elf_newdata (shdr_info[cnt].newscn);
  if (shstrtab_data == NULL)
    {
      cleanup_debug ();
      error_exit (0, _("while create section header string table: %s"),
		  elf_errmsg (-1));
    }
  if (dwelf_strtab_finalize (shst, shstrtab_data) == NULL)
    {
      cleanup_debug ();
      error_exit (0, _("no memory to create section header string table"));
    }

  /* We have to set the section size.  */
  shdr_info[cnt].shdr.sh_size = shstrtab_data->d_size;

  /* Update the section information.  */
  GElf_Off lastoffset = 0;
  for (cnt = 1; cnt <= shdridx; ++cnt)
    if (shdr_info[cnt].idx > 0)
      {
	Elf_Data *newdata;

	scn = elf_getscn (newelf, shdr_info[cnt].idx);
	elf_assert (scn != NULL);

	/* Update the name.  */
	shdr_info[cnt].shdr.sh_name = dwelf_strent_off (shdr_info[cnt].se);

	/* Update the section header from the input file.  Some fields
	   might be section indices which now have to be adjusted.  Keep
	   the index to the "current" sh_link in case we need it to lookup
	   symbol table names.  */
	size_t sh_link = shdr_info[cnt].shdr.sh_link;
	if (shdr_info[cnt].shdr.sh_link != 0)
	  shdr_info[cnt].shdr.sh_link =
	    shdr_info[shdr_info[cnt].shdr.sh_link].idx;

	if (shdr_info[cnt].shdr.sh_type == SHT_GROUP)
	  {
	    elf_assert (shdr_info[cnt].data != NULL
			&& shdr_info[cnt].data->d_buf != NULL);

	    Elf32_Word *grpref = (Elf32_Word *) shdr_info[cnt].data->d_buf;
	    /* First word is the section group flag.
	       Followed by section indexes, that need to be renumbered.  */
	    for (size_t inner = 1;
		 inner < shdr_info[cnt].data->d_size / sizeof (Elf32_Word);
		 ++inner)
	      if (grpref[inner] < shnum)
		grpref[inner] = shdr_info[grpref[inner]].idx;
	      else
		goto illformed;
	  }

	/* Handle the SHT_REL, SHT_RELA, and SHF_INFO_LINK flag.  */
	if (SH_INFO_LINK_P (&shdr_info[cnt].shdr))
	  shdr_info[cnt].shdr.sh_info =
	    shdr_info[shdr_info[cnt].shdr.sh_info].idx;

	/* Get the data from the old file if necessary.  We already
	   created the data for the section header string table.  */
	if (cnt < shnum)
	  {
	    if (shdr_info[cnt].data == NULL)
	      {
		shdr_info[cnt].data = elf_getdata (shdr_info[cnt].scn, NULL);
		if (shdr_info[cnt].data == NULL)
		  INTERNAL_ERROR (fname);
	      }

	    /* Set the data.  This is done by copying from the old file.  */
	    newdata = elf_newdata (scn);
	    if (newdata == NULL)
	      INTERNAL_ERROR (fname);

	    /* Copy the structure.  */
	    *newdata = *shdr_info[cnt].data;

	    /* We know the size.  */
	    shdr_info[cnt].shdr.sh_size = shdr_info[cnt].data->d_size;

	    /* We have to adjust symbol tables.  The st_shndx member might
	       have to be updated.  */
	    if (shdr_info[cnt].shdr.sh_type == SHT_DYNSYM
		|| shdr_info[cnt].shdr.sh_type == SHT_SYMTAB)
	      {
		Elf_Data *versiondata = NULL;
		Elf_Data *shndxdata = NULL;

		size_t elsize = gelf_fsize (elf, ELF_T_SYM, 1, EV_CURRENT);

		if (shdr_info[cnt].symtab_idx != 0)
		  {
		    elf_assert (shdr_info[cnt].shdr.sh_type == SHT_SYMTAB_SHNDX);
		    /* This section has extended section information.
		       We have to modify that information, too.  */
		    shndxdata = elf_getdata (shdr_info[shdr_info[cnt].symtab_idx].scn,
					     NULL);

		    elf_assert (shndxdata != NULL
				&& shndxdata->d_buf != NULL
				&& ((shndxdata->d_size / sizeof (Elf32_Word))
				    >= shdr_info[cnt].data->d_size / elsize));
		  }

		if (shdr_info[cnt].version_idx != 0)
		  {
		    elf_assert (shdr_info[cnt].shdr.sh_type == SHT_DYNSYM);
		    /* This section has associated version
		       information.  We have to modify that
		       information, too.  */
		    versiondata = elf_getdata (shdr_info[shdr_info[cnt].version_idx].scn,
					       NULL);

		    elf_assert (versiondata != NULL
				&& versiondata->d_buf != NULL
				&& ((versiondata->d_size / sizeof (GElf_Versym))
				    >= shdr_info[cnt].data->d_size / elsize));
		  }

		shdr_info[cnt].newsymidx
		  = xcalloc (shdr_info[cnt].data->d_size / elsize,
			     sizeof (Elf32_Word));

		bool last_was_local = true;
		size_t destidx;
		size_t inner;
		for (destidx = inner = 1;
		     inner < shdr_info[cnt].data->d_size / elsize;
		     ++inner)
		  {
		    Elf32_Word sec;
		    GElf_Sym sym_mem;
		    Elf32_Word xshndx;
		    GElf_Sym *sym = gelf_getsymshndx (shdr_info[cnt].data,
						      shndxdata, inner,
						      &sym_mem, &xshndx);
		    if (sym == NULL)
		      INTERNAL_ERROR (fname);

		    if (sym->st_shndx == SHN_UNDEF
			|| (sym->st_shndx >= SHN_LORESERVE
			    && sym->st_shndx != SHN_XINDEX))
		      {
			/* This is no section index, leave it alone
			   unless it is moved.  */
			if (destidx != inner
			    && gelf_update_symshndx (shdr_info[cnt].data,
						     shndxdata,
						     destidx, sym,
						     xshndx) == 0)
			  INTERNAL_ERROR (fname);

			shdr_info[cnt].newsymidx[inner] = destidx++;

			if (last_was_local
			    && GELF_ST_BIND (sym->st_info) != STB_LOCAL)
			  {
			    last_was_local = false;
			    shdr_info[cnt].shdr.sh_info = destidx - 1;
			  }

			continue;
		      }

		    /* Get the full section index, if necessary from the
		       XINDEX table.  */
		    if (sym->st_shndx == SHN_XINDEX)
		      elf_assert (shndxdata != NULL
				  && shndxdata->d_buf != NULL);
		    size_t sidx = (sym->st_shndx != SHN_XINDEX
				   ? sym->st_shndx : xshndx);
		    elf_assert (sidx < shnum);
		    sec = shdr_info[sidx].idx;

		    if (sec != 0)
		      {
			GElf_Section nshndx;
			Elf32_Word nxshndx;

			if (sec < SHN_LORESERVE)
			  {
			    nshndx = sec;
			    nxshndx = 0;
			  }
			else
			  {
			    nshndx = SHN_XINDEX;
			    nxshndx = sec;
			  }

			elf_assert (sec < SHN_LORESERVE || shndxdata != NULL);

			if ((inner != destidx || nshndx != sym->st_shndx
			     || (shndxdata != NULL && nxshndx != xshndx))
			    && (sym->st_shndx = nshndx,
				gelf_update_symshndx (shdr_info[cnt].data,
						      shndxdata,
						      destidx, sym,
						      nxshndx) == 0))
			  INTERNAL_ERROR (fname);

			shdr_info[cnt].newsymidx[inner] = destidx++;

			if (last_was_local
			    && GELF_ST_BIND (sym->st_info) != STB_LOCAL)
			  {
			    last_was_local = false;
			    shdr_info[cnt].shdr.sh_info = destidx - 1;
			  }
		      }
		    else if ((shdr_info[cnt].shdr.sh_flags & SHF_ALLOC) != 0
			     && GELF_ST_TYPE (sym->st_info) != STT_SECTION
			     && shdr_info[sidx].shdr.sh_type != SHT_GROUP)
		      {
			/* Removing a real symbol from an allocated
			   symbol table is hard and probably a
			   mistake.  Really removing it means
			   rewriting the dynamic segment and hash
			   sections.  Just warn and set the symbol
			   section to UNDEF.  */
			error (0, 0,
			       _("Cannot remove symbol [%zd] from allocated symbol table [%zd]"), inner, cnt);
			sym->st_shndx = SHN_UNDEF;
			if (gelf_update_sym (shdr_info[cnt].data, destidx,
					     sym) == 0)
			  INTERNAL_ERROR (fname);
			shdr_info[cnt].newsymidx[inner] = destidx++;
		      }
		    else if (debug_fname != NULL
			     && shdr_info[cnt].debug_data == NULL)
		      /* The symbol points to a section that is discarded
			 but isn't preserved in the debug file. Check that
			 this is a section or group signature symbol
			 for a section which has been removed.  Or a special
			 data marker symbol to a debug section.  */
		      {
			elf_assert (GELF_ST_TYPE (sym->st_info) == STT_SECTION
				    || ((shdr_info[sidx].shdr.sh_type
					 == SHT_GROUP)
					&& (shdr_info[sidx].shdr.sh_info
					    == inner))
				    || ebl_data_marker_symbol (ebl, sym,
						elf_strptr (elf, sh_link,
							    sym->st_name)));
		      }
		  }

		if (destidx != inner)
		  {
		    /* The size of the symbol table changed.  */
		    shdr_info[cnt].shdr.sh_size = newdata->d_size
		      = destidx * elsize;
		    any_symtab_changes = true;
		  }
		else
		  {
		    /* The symbol table didn't really change.  */
		    free (shdr_info[cnt].newsymidx);
		    shdr_info[cnt].newsymidx = NULL;
		  }
	      }
	  }

	/* If we have to, compute the offset of the section.
	   If allocate and unallocated sections are mixed, we only update
	   the allocated ones now.  The unallocated ones come second.  */
	if (! mixed_allocated_unallocated
	    || (shdr_info[cnt].shdr.sh_flags & SHF_ALLOC) != 0)
	  {
	    if (shdr_info[cnt].shdr.sh_offset == 0)
	      shdr_info[cnt].shdr.sh_offset
		= ((lastoffset + shdr_info[cnt].shdr.sh_addralign - 1)
		   & ~((GElf_Off) (shdr_info[cnt].shdr.sh_addralign - 1)));

	    /* Set the section header in the new file.  */
	    if (unlikely (gelf_update_shdr (scn, &shdr_info[cnt].shdr) == 0))
	      /* There cannot be any overflows.  */
	      INTERNAL_ERROR (fname);

	    /* Remember the last section written so far.  */
	    GElf_Off filesz = (shdr_info[cnt].shdr.sh_type != SHT_NOBITS
			       ? shdr_info[cnt].shdr.sh_size : 0);
	    if (lastoffset < shdr_info[cnt].shdr.sh_offset + filesz)
	      lastoffset = shdr_info[cnt].shdr.sh_offset + filesz;
	  }
      }

  /* We might have to update the unallocated sections after we done the
     allocated ones.  lastoffset is set to right after the last allocated
     section.  */
  if (mixed_allocated_unallocated)
    for (cnt = 1; cnt <= shdridx; ++cnt)
      if (shdr_info[cnt].idx > 0)
	{
	  scn = elf_getscn (newelf, shdr_info[cnt].idx);
	  if ((shdr_info[cnt].shdr.sh_flags & SHF_ALLOC) == 0)
	    {
	      if (shdr_info[cnt].shdr.sh_offset == 0)
		shdr_info[cnt].shdr.sh_offset
		  = ((lastoffset + shdr_info[cnt].shdr.sh_addralign - 1)
		     & ~((GElf_Off) (shdr_info[cnt].shdr.sh_addralign - 1)));

	      /* Set the section header in the new file.  */
	      if (unlikely (gelf_update_shdr (scn, &shdr_info[cnt].shdr) == 0))
		/* There cannot be any overflows.  */
		INTERNAL_ERROR (fname);

	      /* Remember the last section written so far.  */
	      GElf_Off filesz = (shdr_info[cnt].shdr.sh_type != SHT_NOBITS
				 ? shdr_info[cnt].shdr.sh_size : 0);
	      if (lastoffset < shdr_info[cnt].shdr.sh_offset + filesz)
		lastoffset = shdr_info[cnt].shdr.sh_offset + filesz;
	    }
	}

  /* Adjust symbol references if symbol tables changed.  */
  if (any_symtab_changes)
    /* Find all relocation sections which use this symbol table.  */
    for (cnt = 1; cnt <= shdridx; ++cnt)
      {
	struct shdr_info *info = &shdr_info[cnt];
	if (info->idx == 0 && debug_fname == NULL)
	  /* Ignore sections which are discarded.  When we are saving a
	     relocation section in a separate debug file, we must fix up
	     the symbol table references.  */
	  continue;

	const Elf32_Word symtabidx = info->old_sh_link;
	elf_assert (symtabidx < shnum + 2);
	const Elf32_Word *const newsymidx = shdr_info[symtabidx].newsymidx;

	/* If the symbol table hasn't changed, do not do anything.  */
	if (newsymidx == NULL)
	  continue;

	/* If the symbol table is not discarded, but additionally
	   duplicated in the separate debug file and this section
	   is discarded, don't adjust anything.  */
	if (info->idx == 0 && shdr_info[symtabidx].debug_data != NULL)
	  continue;

	switch (info->shdr.sh_type)
	  {
	  case SHT_REL:
	  case SHT_RELA:
	    scn = (info->idx == 0
		   ? elf_getscn (debugelf, cnt)
		   : elf_getscn (newelf, info->idx));
	    Elf_Data *d = elf_getdata (scn, NULL);
	    elf_assert (d != NULL && d->d_buf != NULL
			&& info->shdr.sh_entsize != 0);
	    size_t nrels = (info->shdr.sh_size / info->shdr.sh_entsize);

	    size_t symsize = gelf_fsize (elf, ELF_T_SYM, 1, EV_CURRENT);
	    const Elf32_Word symidxn = (shdr_info[symtabidx].data->d_size
					/ symsize);
	    if (info->shdr.sh_type == SHT_REL)
	      for (size_t relidx = 0; relidx < nrels; ++relidx)
		{
		  GElf_Rel rel_mem;
		  if (gelf_getrel (d, relidx, &rel_mem) == NULL)
		    INTERNAL_ERROR (fname);

		  size_t symidx = GELF_R_SYM (rel_mem.r_info);
		  elf_assert (symidx < symidxn);
		  if (newsymidx[symidx] != symidx)
		    {
		      rel_mem.r_info
			= GELF_R_INFO (newsymidx[symidx],
				       GELF_R_TYPE (rel_mem.r_info));

		      if (gelf_update_rel (d, relidx, &rel_mem) == 0)
			INTERNAL_ERROR (fname);
		    }
		}
	    else
	      for (size_t relidx = 0; relidx < nrels; ++relidx)
		{
		  GElf_Rela rel_mem;
		  if (gelf_getrela (d, relidx, &rel_mem) == NULL)
		    INTERNAL_ERROR (fname);

		  size_t symidx = GELF_R_SYM (rel_mem.r_info);
		  elf_assert (symidx < symidxn);
		  if (newsymidx[symidx] != symidx)
		    {
		      rel_mem.r_info
			= GELF_R_INFO (newsymidx[symidx],
				       GELF_R_TYPE (rel_mem.r_info));

		      if (gelf_update_rela (d, relidx, &rel_mem) == 0)
			INTERNAL_ERROR (fname);
		    }
		}
	    break;

	  case SHT_HASH:
	    /* We have to recompute the hash table.  */

	    elf_assert (info->idx > 0);

	    /* The hash section in the new file.  */
	    scn = elf_getscn (newelf, info->idx);

	    /* The symbol table data.  */
	    Elf_Data *symd = elf_getdata (elf_getscn (newelf,
						      shdr_info[symtabidx].idx),
					  NULL);
	    elf_assert (symd != NULL && symd->d_buf != NULL);

	    /* The hash table data.  */
	    Elf_Data *hashd = elf_getdata (scn, NULL);
	    elf_assert (hashd != NULL && hashd->d_buf != NULL);

	    if (info->shdr.sh_entsize == sizeof (Elf32_Word))
	      {
		/* Sane arches first.  */
		elf_assert (hashd->d_size >= 2 * sizeof (Elf32_Word));
		Elf32_Word *bucket = (Elf32_Word *) hashd->d_buf;

		size_t strshndx = shdr_info[symtabidx].old_sh_link;
		size_t elsize = gelf_fsize (elf, ELF_T_SYM, 1, EV_CURRENT);

		Elf32_Word nchain = bucket[1];
		Elf32_Word nbucket = bucket[0];
		uint64_t used_buf = ((2ULL + nchain + nbucket)
				     * sizeof (Elf32_Word));
		elf_assert (used_buf <= hashd->d_size);

		/* Adjust the nchain value.  The symbol table size
		   changed.  We keep the same size for the bucket array.  */
		bucket[1] = symd->d_size / elsize;
		bucket += 2;
		Elf32_Word *chain = bucket + nbucket;

		/* New size of the section.  */
		size_t n_size = ((2 + symd->d_size / elsize + nbucket)
				 * sizeof (Elf32_Word));
		elf_assert (n_size <= hashd->d_size);
		hashd->d_size = n_size;
		update_section_size (scn, hashd, debugelf, cnt, fname);

		/* Clear the arrays.  */
		memset (bucket, '\0',
			(symd->d_size / elsize + nbucket)
			* sizeof (Elf32_Word));

		for (size_t inner = shdr_info[symtabidx].shdr.sh_info;
		     inner < symd->d_size / elsize; ++inner)
		  {
		    GElf_Sym sym_mem;
		    GElf_Sym *sym = gelf_getsym (symd, inner, &sym_mem);
		    elf_assert (sym != NULL);

		    const char *name = elf_strptr (elf, strshndx,
						   sym->st_name);
		    elf_assert (name != NULL && nbucket != 0);
		    size_t hidx = elf_hash (name) % nbucket;

		    if (bucket[hidx] == 0)
		      bucket[hidx] = inner;
		    else
		      {
			hidx = bucket[hidx];

			while (chain[hidx] != 0 && chain[hidx] < nchain)
			  hidx = chain[hidx];

			chain[hidx] = inner;
		      }
		  }
	      }
	    else
	      {
		/* Alpha and S390 64-bit use 64-bit SHT_HASH entries.  */
		elf_assert (info->shdr.sh_entsize == sizeof (Elf64_Xword));

		Elf64_Xword *bucket = (Elf64_Xword *) hashd->d_buf;

		size_t strshndx = shdr_info[symtabidx].old_sh_link;
		size_t elsize = gelf_fsize (elf, ELF_T_SYM, 1, EV_CURRENT);

		elf_assert (symd->d_size >= 2 * sizeof (Elf64_Xword));
		Elf64_Xword nbucket = bucket[0];
		Elf64_Xword nchain = bucket[1];
		uint64_t maxwords = hashd->d_size / sizeof (Elf64_Xword);
		elf_assert (maxwords >= 2
			    && maxwords - 2 >= nbucket
			    && maxwords - 2 - nbucket >= nchain);

		/* Adjust the nchain value.  The symbol table size
		   changed.  We keep the same size for the bucket array.  */
		bucket[1] = symd->d_size / elsize;
		bucket += 2;
		Elf64_Xword *chain = bucket + nbucket;

		/* New size of the section.  */
		size_t n_size = ((2 + symd->d_size / elsize + nbucket)
				 * sizeof (Elf64_Xword));
		elf_assert (n_size <= hashd->d_size);
		hashd->d_size = n_size;
		update_section_size (scn, hashd, debugelf, cnt, fname);

		/* Clear the arrays.  */
		memset (bucket, '\0',
			(symd->d_size / elsize + nbucket)
			* sizeof (Elf64_Xword));

		for (size_t inner = shdr_info[symtabidx].shdr.sh_info;
		     inner < symd->d_size / elsize; ++inner)
		  {
		    GElf_Sym sym_mem;
		    GElf_Sym *sym = gelf_getsym (symd, inner, &sym_mem);
		    elf_assert (sym != NULL);

		    const char *name = elf_strptr (elf, strshndx,
						   sym->st_name);
		    elf_assert (name != NULL && nbucket != 0);
		    size_t hidx = elf_hash (name) % nbucket;

		    if (bucket[hidx] == 0)
		      bucket[hidx] = inner;
		    else
		      {
			hidx = bucket[hidx];

			while (chain[hidx] != 0 && chain[hidx] < nchain)
			  hidx = chain[hidx];

			chain[hidx] = inner;
		      }
		  }
	      }
	    break;

	  case SHT_GNU_versym:
	    /* If the symbol table changed we have to adjust the entries.  */
	    elf_assert (info->idx > 0);

	    /* The symbol version section in the new file.  */
	    scn = elf_getscn (newelf, info->idx);

	    /* The symbol table data.  */
	    symd = elf_getdata (elf_getscn (newelf, shdr_info[symtabidx].idx),
				NULL);
	    elf_assert (symd != NULL && symd->d_buf != NULL);
	    size_t symz = gelf_fsize (elf, ELF_T_SYM, 1, EV_CURRENT);
	    const Elf32_Word syms = (shdr_info[symtabidx].data->d_size / symz);

	    /* The version symbol data.  */
	    Elf_Data *verd = elf_getdata (scn, NULL);
	    elf_assert (verd != NULL && verd->d_buf != NULL);

	    /* The symbol version array.  */
	    GElf_Half *verstab = (GElf_Half *) verd->d_buf;

	    /* Walk through the list and */
	    size_t elsize = gelf_fsize (elf, verd->d_type, 1, EV_CURRENT);
	    Elf32_Word vers = verd->d_size / elsize;
	    for (size_t inner = 1; inner < vers && inner < syms; ++inner)
	      if (newsymidx[inner] != 0 && newsymidx[inner] < vers)
		/* Overwriting the same array works since the
		   reordering can only move entries to lower indices
		   in the array.  */
		verstab[newsymidx[inner]] = verstab[inner];

	    /* New size of the section.  */
	    verd->d_size = gelf_fsize (newelf, verd->d_type,
				       symd->d_size
				       / gelf_fsize (elf, symd->d_type, 1,
						     EV_CURRENT),
				       EV_CURRENT);
	    update_section_size (scn, verd, debugelf, cnt, fname);
	    break;

	  case SHT_GROUP:
	    /* Yes, the symbol table changed.
	       Update the section header of the section group.  */
	    scn = elf_getscn (newelf, info->idx);
	    GElf_Shdr shdr_mem;
	    GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
	    elf_assert (shdr != NULL);

	    size_t symsz = gelf_fsize (elf, ELF_T_SYM, 1, EV_CURRENT);
	    const Elf32_Word symn = (shdr_info[symtabidx].data->d_size
				     / symsz);
	    elf_assert (shdr->sh_info < symn);
	    shdr->sh_info = newsymidx[shdr->sh_info];

	    (void) gelf_update_shdr (scn, shdr);
	    break;
	  }
      }

  /* Remove any relocations between debug sections in ET_REL
     for the debug file when requested.  These relocations are always
     zero based between the unallocated sections.  */
  if (debug_fname != NULL && removing_sections
      && reloc_debug && ehdr->e_type == ET_REL)
    remove_debug_relocations (ebl, debugelf, ehdr, fname, shstrndx);

  /* Now that we have done all adjustments to the data,
     we can actually write out the debug file.  */
  if (debug_fname != NULL && removing_sections)
    {
      /* Finally write the file.  */
      if (unlikely (elf_update (debugelf, ELF_C_WRITE) == -1))
	{
	  error (0, 0, _("while writing '%s': %s"),
		 tmp_debug_fname, elf_errmsg (-1));
	  result = 1;
	  goto fail_close;
	}

      /* Create the real output file.  First rename, then change the
	 mode.  */
      if (rename (tmp_debug_fname, debug_fname) != 0
	  || fchmod (debug_fd, mode) != 0)
	{
	  error (0, errno, _("while creating '%s'"), debug_fname);
	  result = 1;
	  goto fail_close;
	}

      /* The temporary file does not exist anymore.  */
      free (tmp_debug_fname);
      tmp_debug_fname = NULL;

      if (!remove_shdrs)
	{
	  uint32_t debug_crc;
	  Elf_Data debug_crc_data =
	    {
	      .d_type = ELF_T_WORD,
	      .d_buf = &debug_crc,
	      .d_size = sizeof (debug_crc),
	      .d_version = EV_CURRENT
	    };

	  /* Compute the checksum which we will add to the executable.  */
	  if (crc32_file (debug_fd, &debug_crc) != 0)
	    {
	      error (0, errno, _("\
while computing checksum for debug information"));
	      unlink (debug_fname);
	      result = 1;
	      goto fail_close;
	    }

	  /* Store it in the debuglink section data.  */
	  if (unlikely (gelf_xlatetof (newelf, &debuglink_crc_data,
				       &debug_crc_data, ehdr->e_ident[EI_DATA])
			!= &debuglink_crc_data))
	    INTERNAL_ERROR (fname);
	}
    }

  lastsec_offset = shdr_info[shdridx].shdr.sh_offset;
  lastsec_size = shdr_info[shdridx].shdr.sh_size;

 done:
  /* Finally finish the ELF header.  Fill in the fields not handled by
     libelf from the old file.  */
  newehdr = gelf_getehdr (newelf, &newehdr_mem);
  if (newehdr == NULL)
    INTERNAL_ERROR (fname);

  memcpy (newehdr->e_ident, ehdr->e_ident, EI_NIDENT);
  newehdr->e_type = ehdr->e_type;
  newehdr->e_machine = ehdr->e_machine;
  newehdr->e_version = ehdr->e_version;
  newehdr->e_entry = ehdr->e_entry;
  newehdr->e_flags = ehdr->e_flags;
  newehdr->e_phoff = ehdr->e_phoff;

  /* We need to position the section header table.  */
  const size_t offsize = gelf_fsize (elf, ELF_T_OFF, 1, EV_CURRENT);
  newehdr->e_shoff = ((lastsec_offset + lastsec_size + offsize - 1)
		      & ~((GElf_Off) (offsize - 1)));
  newehdr->e_shentsize = gelf_fsize (elf, ELF_T_SHDR, 1, EV_CURRENT);

  if (gelf_update_ehdr (newelf, newehdr) == 0)
    {
      error (0, 0, _("%s: error while creating ELF header: %s"),
	     output_fname ?: fname, elf_errmsg (-1));
      result = 1;
    }

  /* The new section header string table index.  */
  if (update_shdrstrndx (newelf, idx) != 0)
    {
      error (0, 0, _("%s: error updating shdrstrndx: %s"),
	     output_fname ?: fname, elf_errmsg (-1));
      result = 1;
    }

  /* We have everything from the old file.  */
  if (elf_cntl (elf, ELF_C_FDDONE) != 0)
    {
      error (0, 0, _("%s: error while reading the file: %s"),
	     fname, elf_errmsg (-1));
      result = 1;
    }

  /* The ELF library better follows our layout when this is not a
     relocatable object file.  */
  elf_flagelf (newelf, ELF_C_SET,
	       (phnum > 0 ? ELF_F_LAYOUT : 0)
	       | (permissive ? ELF_F_PERMISSIVE : 0));

  /* Finally write the file.  */
  if (elf_update (newelf, ELF_C_WRITE) == -1)
    {
      error (0, 0, _("while writing '%s': %s"),
	     output_fname ?: fname, elf_errmsg (-1));
      result = 1;
    }

  if (remove_shdrs)
    {
      /* libelf can't cope without the section headers being properly intact.
	 So we just let it write them normally, and then we nuke them later.  */

      if (newehdr->e_ident[EI_CLASS] == ELFCLASS32)
	{
	  assert (offsetof (Elf32_Ehdr, e_shentsize) + sizeof (Elf32_Half)
		  == offsetof (Elf32_Ehdr, e_shnum));
	  assert (offsetof (Elf32_Ehdr, e_shnum) + sizeof (Elf32_Half)
		  == offsetof (Elf32_Ehdr, e_shstrndx));
	  const Elf32_Off zero_off = 0;
	  const Elf32_Half zero[3] = { 0, 0, SHN_UNDEF };
	  if (pwrite_retry (fd, &zero_off, sizeof zero_off,
			    offsetof (Elf32_Ehdr, e_shoff)) != sizeof zero_off
	      || (pwrite_retry (fd, zero, sizeof zero,
				offsetof (Elf32_Ehdr, e_shentsize))
		  != sizeof zero)
	      || ftruncate (fd, lastsec_offset) < 0)
	    {
	      error (0, errno, _("while writing '%s'"),
		     output_fname ?: fname);
	      result = 1;
	    }
	}
      else
	{
	  assert (offsetof (Elf64_Ehdr, e_shentsize) + sizeof (Elf64_Half)
		  == offsetof (Elf64_Ehdr, e_shnum));
	  assert (offsetof (Elf64_Ehdr, e_shnum) + sizeof (Elf64_Half)
		  == offsetof (Elf64_Ehdr, e_shstrndx));
	  const Elf64_Off zero_off = 0;
	  const Elf64_Half zero[3] = { 0, 0, SHN_UNDEF };
	  if (pwrite_retry (fd, &zero_off, sizeof zero_off,
			    offsetof (Elf64_Ehdr, e_shoff)) != sizeof zero_off
	      || (pwrite_retry (fd, zero, sizeof zero,
				offsetof (Elf64_Ehdr, e_shentsize))
		  != sizeof zero)
	      || ftruncate (fd, lastsec_offset) < 0)
	    {
	      error (0, errno, _("while writing '%s'"),
		     output_fname ?: fname);
	      result = 1;
	    }
	}
    }

 fail_close:
  if (shdr_info != NULL)
    {
      /* For some sections we might have created an table to map symbol
	 table indices.  Or we might kept (original) data around to put
	 into the .debug file.  */
      for (cnt = 1; cnt <= shdridx; ++cnt)
	{
	  free (shdr_info[cnt].newsymidx);
	  if (shdr_info[cnt].debug_data != NULL)
	    free (shdr_info[cnt].debug_data->d_buf);
	}

      /* Free data we allocated for the .gnu_debuglink section. */
      free (debuglink_buf);

      /* Free the memory.  */
      if ((shnum + 2) * sizeof (struct shdr_info) > MAX_STACK_ALLOC)
	free (shdr_info);
    }

  /* Free other resources.  */
  if (shstrtab_data != NULL)
    free (shstrtab_data->d_buf);
  if (shst != NULL)
    dwelf_strtab_free (shst);

  /* That was it.  Close the descriptors.  */
  if (elf_end (newelf) != 0)
    {
      error (0, 0, _("error while finishing '%s': %s"),
	     output_fname ?: fname, elf_errmsg (-1));
      result = 1;
    }

  if (debugelf != NULL && elf_end (debugelf) != 0)
    {
      error (0, 0, _("error while finishing '%s': %s"), debug_fname,
	     elf_errmsg (-1));
      result = 1;
    }

 fail:
  /* Close the EBL backend.  */
  if (ebl != NULL)
    ebl_closebackend (ebl);

  cleanup_debug ();

  /* If requested, preserve the timestamp.  */
  if (tvp != NULL)
    {
      if (futimens (fd, tvp) != 0)
	{
	  error (0, errno, _("\
cannot set access and modification date of '%s'"),
		 output_fname ?: fname);
	  result = 1;
	}
    }

  /* Close the file descriptor if we created a new file.  */
  if (output_fname != NULL)
    {
      close (fd);
      if (result != 0)
       unlink (output_fname);
    }

  return result;
}

static void
cleanup_debug (void)
{
  if (debug_fd >= 0)
    {
      if (tmp_debug_fname != NULL)
	{
	  unlink (tmp_debug_fname);
	  free (tmp_debug_fname);
	  tmp_debug_fname = NULL;
	}
      close (debug_fd);
      debug_fd = -1;
    }
}

static int
handle_ar (int fd, Elf *elf, const char *prefix, const char *fname,
	   struct timespec tvp[2])
{
  size_t prefix_len = prefix == NULL ? 0 : strlen (prefix);
  size_t fname_len = strlen (fname) + 1;
  char new_prefix[prefix_len + 1 + fname_len];
  char *cp = new_prefix;

  /* Create the full name of the file.  */
  if (prefix != NULL)
    {
      cp = mempcpy (cp, prefix, prefix_len);
      *cp++ = ':';
    }
  memcpy (cp, fname, fname_len);


  /* Process all the files contained in the archive.  */
  Elf *subelf;
  Elf_Cmd cmd = ELF_C_RDWR;
  int result = 0;
  while ((subelf = elf_begin (fd, cmd, elf)) != NULL)
    {
      /* The the header for this element.  */
      Elf_Arhdr *arhdr = elf_getarhdr (subelf);

      if (elf_kind (subelf) == ELF_K_ELF)
	result |= handle_elf (fd, subelf, new_prefix, arhdr->ar_name, 0, NULL);
      else if (elf_kind (subelf) == ELF_K_AR)
	result |= handle_ar (fd, subelf, new_prefix, arhdr->ar_name, NULL);

      /* Get next archive element.  */
      cmd = elf_next (subelf);
      if (unlikely (elf_end (subelf) != 0))
	INTERNAL_ERROR (fname);
    }

  if (tvp != NULL)
    {
      if (unlikely (futimens (fd, tvp) != 0))
	{
	  error (0, errno, _("\
cannot set access and modification date of '%s'"), fname);
	  result = 1;
	}
    }

  if (unlikely (close (fd) != 0))
    error_exit (errno, _("while closing '%s'"), fname);

  return result;
}


#include "debugpred.h"
