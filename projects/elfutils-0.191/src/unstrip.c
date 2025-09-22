/* Combine stripped files with separate symbols and debug information.
   Copyright (C) 2007-2012, 2014, 2015 Red Hat, Inc.
   This file is part of elfutils.
   Written by Roland McGrath <roland@redhat.com>, 2007.

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

/* TODO:

  * SHX_XINDEX

  * prelink vs .debug_* linked addresses

 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <argp.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <gelf.h>
#include <libebl.h>
#include <libdwfl.h>
#include "system.h"
#include "libdwelf.h"
#include "libeu.h"
#include "printversion.h"

/* Name and version of program.  */
ARGP_PROGRAM_VERSION_HOOK_DEF = print_version;

/* Bug report address.  */
ARGP_PROGRAM_BUG_ADDRESS_DEF = PACKAGE_BUGREPORT;

/* Definitions of arguments for argp functions.  */
static const struct argp_option options[] =
{
  /* Group 2 will follow group 1 from dwfl_standard_argp.  */
  { "match-file-names", 'f', NULL, 0,
    N_("Match MODULE against file names, not module names"), 2 },
  { "ignore-missing", 'i', NULL, 0, N_("Silently skip unfindable files"), 0 },

  { NULL, 0, NULL, 0, N_("Output options:"), 0 },
  { "output", 'o', "FILE", 0, N_("Place output into FILE"), 0 },
  { "output-directory", 'd', "DIRECTORY",
    0, N_("Create multiple output files under DIRECTORY"), 0 },
  { "module-names", 'm', NULL, 0, N_("Use module rather than file names"), 0 },
  { "all", 'a', NULL, 0,
    N_("Create output for modules that have no separate debug information"),
    0 },
  { "relocate", 'R', NULL, 0,
    N_("Apply relocations to section contents in ET_REL files"), 0 },
  { "list-only", 'n', NULL, 0,
    N_("Only list module and file names, build IDs"), 0 },
 { "force", 'F', NULL, 0,
    N_("Force combining files even if some ELF headers don't seem to match"),
   0 },
  { NULL, 0, NULL, 0, NULL, 0 }
};

struct arg_info
{
  const char *output_file;
  const char *output_dir;
  Dwfl *dwfl;
  char **args;
  bool list;
  bool all;
  bool ignore;
  bool modnames;
  bool match_files;
  bool relocate;
  bool force;
};

/* Handle program arguments.  */
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  struct arg_info *info = state->input;

  switch (key)
    {
    case ARGP_KEY_INIT:
      state->child_inputs[0] = &info->dwfl;
      break;

    case 'o':
      if (info->output_file != NULL)
	{
	  argp_error (state, _("-o option specified twice"));
	  return EINVAL;
	}
      info->output_file = arg;
      break;

    case 'd':
      if (info->output_dir != NULL)
	{
	  argp_error (state, _("-d option specified twice"));
	  return EINVAL;
	}
      info->output_dir = arg;
      break;

    case 'm':
      info->modnames = true;
      break;
    case 'f':
      info->match_files = true;
      break;
    case 'a':
      info->all = true;
      break;
    case 'i':
      info->ignore = true;
      break;
    case 'n':
      info->list = true;
      break;
    case 'R':
      info->relocate = true;
      break;
    case 'F':
      info->force = true;
      break;

    case ARGP_KEY_ARGS:
    case ARGP_KEY_NO_ARGS:
      /* We "consume" all the arguments here.  */
      info->args = &state->argv[state->next];

      if (info->output_file != NULL && info->output_dir != NULL)
	{
	  argp_error (state, _("only one of -o or -d allowed"));
	  return EINVAL;
	}

      if (info->list && (info->dwfl == NULL
			 || info->output_dir != NULL
			 || info->output_file != NULL))
	{
	  argp_error (state,
		      _("-n cannot be used with explicit files or -o or -d"));
	  return EINVAL;
	}

      if (info->output_dir != NULL)
	{
	  struct stat st;
	  error_t fail = 0;
	  if (stat (info->output_dir, &st) < 0)
	    fail = errno;
	  else if (!S_ISDIR (st.st_mode))
	    fail = ENOTDIR;
	  if (fail)
	    {
	      argp_failure (state, EXIT_FAILURE, fail,
			    _("output directory '%s'"), info->output_dir);
	      return fail;
	    }
	}

      if (info->dwfl == NULL)
	{
	  if (state->next + 2 != state->argc)
	    {
	      argp_error (state, _("exactly two file arguments are required"));
	      return EINVAL;
	    }

	  if (info->ignore || info->all || info->modnames || info->relocate)
	    {
	      argp_error (state, _("\
-m, -a, -R, and -i options not allowed with explicit files"));
	      return EINVAL;
	    }

	  /* Bail out immediately to prevent dwfl_standard_argp's parser
	     from defaulting to "-e a.out".  */
	  return ENOSYS;
	}
      else if (info->output_file == NULL && info->output_dir == NULL
	       && !info->list)
	{
	  argp_error (state,
		      _("-o or -d is required when using implicit files"));
	  return EINVAL;
	}
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

#define ELF_CHECK(call, msg)						      \
  do									      \
    {									      \
      if (unlikely (!(call)))						      \
	error_exit (0, msg, elf_errmsg (-1));				      \
    } while (0)

/* Copy INELF to newly-created OUTELF, exit via error for any problems.  */
static void
copy_elf (Elf *outelf, Elf *inelf)
{
  ELF_CHECK (gelf_newehdr (outelf, gelf_getclass (inelf)),
	     _("cannot create ELF header: %s"));

  size_t shstrndx;
  ELF_CHECK (elf_getshdrstrndx (inelf, &shstrndx) == 0,
	     _("cannot get shdrstrndx:%s"));

  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr = gelf_getehdr (inelf, &ehdr_mem);
  ELF_CHECK (ehdr != NULL, _("cannot get ELF header: %s"));
  if (shstrndx < SHN_LORESERVE)
    ehdr->e_shstrndx = shstrndx;
  else
    {
      ehdr->e_shstrndx = SHN_XINDEX;
      Elf_Scn *scn0 = elf_getscn (outelf, 0);
      GElf_Shdr shdr0_mem;
      GElf_Shdr *shdr0 = gelf_getshdr (scn0, &shdr0_mem);
      ELF_CHECK (shdr0 != NULL,
		 _("cannot get new zero section: %s"));
      shdr0->sh_link = shstrndx;
      ELF_CHECK (gelf_update_shdr (scn0, shdr0),
		 _("cannot update new zero section: %s"));
    }

  ELF_CHECK (gelf_update_ehdr (outelf, ehdr),
	     _("cannot copy ELF header: %s"));

  size_t phnum;
  ELF_CHECK (elf_getphdrnum (inelf, &phnum) == 0,
	     _("cannot get number of program headers: %s"));

  if (phnum > 0)
    {
      ELF_CHECK (gelf_newphdr (outelf, phnum),
		 _("cannot create program headers: %s"));

      GElf_Phdr phdr_mem;
      for (size_t i = 0; i < phnum; ++i)
	ELF_CHECK (gelf_update_phdr (outelf, i,
				     gelf_getphdr (inelf, i, &phdr_mem)),
		   _("cannot copy program header: %s"));
    }

  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (inelf, scn)) != NULL)
    {
      Elf_Scn *newscn = elf_newscn (outelf);

      GElf_Shdr shdr_mem;
      ELF_CHECK (gelf_update_shdr (newscn, gelf_getshdr (scn, &shdr_mem)),
		 _("cannot copy section header: %s"));

      Elf_Data *data = elf_getdata (scn, NULL);
      ELF_CHECK (data != NULL, _("cannot get section data: %s"));
      Elf_Data *newdata = elf_newdata (newscn);
      ELF_CHECK (newdata != NULL, _("cannot copy section data: %s"));
      *newdata = *data;
      elf_flagdata (newdata, ELF_C_SET, ELF_F_DIRTY);
    }
}

/* Create directories containing PATH.  */
static void
make_directories (const char *path)
{
  const char *lastslash = strrchr (path, '/');
  if (lastslash == NULL)
    return;

  while (lastslash > path && lastslash[-1] == '/')
    --lastslash;
  if (lastslash == path)
    return;

  char *dir = strndup (path, lastslash - path);
  if (dir == NULL)
    error(EXIT_FAILURE, errno, _("memory exhausted"));

  while (mkdir (dir, ACCESSPERMS) < 0 && errno != EEXIST)
    {
      if (errno == ENOENT)
        make_directories (dir);
      else
        error_exit (errno, _("cannot create directory '%s'"), dir);
    }
  free (dir);
}

/* Keep track of new section data we are creating, so we can free it
   when done.  */
struct data_list
{
  void *data;
  struct data_list *next;
};

struct data_list *new_data_list;

static void
record_new_data (void *data)
{
  struct data_list *next = new_data_list;
  new_data_list = xmalloc (sizeof (struct data_list));
  new_data_list->data = data;
  new_data_list->next = next;
}

static void
free_new_data (void)
{
  struct data_list *list = new_data_list;
  while (list != NULL)
    {
      struct data_list *next = list->next;
      free (list->data);
      free (list);
      list = next;
    }
  new_data_list = NULL;
}

/* The binutils linker leaves gratuitous section symbols in .symtab
   that strip has to remove.  Older linkers likewise include a
   symbol for every section, even unallocated ones, in .dynsym.
   Because of this, the related sections can shrink in the stripped
   file from their original size.  Older versions of strip do not
   adjust the sh_size field in the debuginfo file's SHT_NOBITS
   version of the section header, so it can appear larger.  */
static bool
section_can_shrink (const GElf_Shdr *shdr)
{
  switch (shdr->sh_type)
    {
    case SHT_SYMTAB:
    case SHT_DYNSYM:
    case SHT_HASH:
    case SHT_GNU_versym:
      return true;
    }
  return false;
}

/* See if this symbol table has a leading section symbol for every single
   section, in order.  The binutils linker produces this.  While we're here,
   update each section symbol's st_value.  */
static size_t
symtab_count_leading_section_symbols (Elf *elf, Elf_Scn *scn, size_t shnum,
				      Elf_Data *newsymdata)
{
  Elf_Data *data = elf_getdata (scn, NULL);
  Elf_Data *shndxdata = NULL;	/* XXX */

  for (size_t i = 1; i < shnum; ++i)
    {
      GElf_Sym sym_mem;
      GElf_Word shndx = SHN_UNDEF;
      GElf_Sym *sym = gelf_getsymshndx (data, shndxdata, i, &sym_mem, &shndx);
      ELF_CHECK (sym != NULL, _("cannot get symbol table entry: %s"));

      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (elf_getscn (elf, i), &shdr_mem);
      ELF_CHECK (shdr != NULL, _("cannot get section header: %s"));

      if (sym->st_shndx != SHN_XINDEX)
	shndx = sym->st_shndx;

      if (shndx != i || GELF_ST_TYPE (sym->st_info) != STT_SECTION)
	return i;

      sym->st_value = shdr->sh_addr;
      if (sym->st_shndx != SHN_XINDEX)
	shndx = SHN_UNDEF;
      ELF_CHECK (gelf_update_symshndx (newsymdata, shndxdata, i, sym, shndx),
		 _("cannot update symbol table: %s"));
    }

  return shnum;
}

static void
update_shdr (Elf_Scn *outscn, GElf_Shdr *newshdr)
{
  ELF_CHECK (gelf_update_shdr (outscn, newshdr),
	     _("cannot update section header: %s"));
}

/* We expanded the output section, so update its header.  */
static void
update_sh_size (Elf_Scn *outscn, const Elf_Data *data)
{
  GElf_Shdr shdr_mem;
  GElf_Shdr *newshdr = gelf_getshdr (outscn, &shdr_mem);
  ELF_CHECK (newshdr != NULL, _("cannot get section header: %s"));

  newshdr->sh_size = data->d_size;

  update_shdr (outscn, newshdr);
}

static inline void
adjust_reloc (GElf_Xword *info,
	      size_t map[], size_t map_size)
{
  size_t ndx = GELF_R_SYM (*info);
  if (ndx != STN_UNDEF)
    {
      if (ndx > map_size)
	error_exit (0, "bad symbol ndx section");
      *info = GELF_R_INFO (map[ndx - 1], GELF_R_TYPE (*info));
    }
}

/* Update relocation sections using the symbol table.  */
static void
adjust_relocs (Elf_Scn *outscn, Elf_Scn *inscn, const GElf_Shdr *shdr,
	       size_t map[], size_t map_size, const GElf_Shdr *symshdr)
{
  Elf_Data *data = elf_getdata (outscn, NULL);

  switch (shdr->sh_type)
    {
    case SHT_REL:
      if (shdr->sh_entsize == 0)
	error_exit (0, "REL section cannot have zero sh_entsize");

      for (size_t i = 0; i < shdr->sh_size / shdr->sh_entsize; ++i)
	{
	  GElf_Rel rel_mem;
	  GElf_Rel *rel = gelf_getrel (data, i, &rel_mem);
	  ELF_CHECK (rel != NULL, _("gelf_getrel failed: %s"));
	  adjust_reloc (&rel->r_info, map, map_size);
	  ELF_CHECK (gelf_update_rel (data, i, rel),
		     _("cannot update relocation: %s"));
	}
      break;

    case SHT_RELA:
      if (shdr->sh_entsize == 0)
	error_exit (0, "RELA section cannot have zero sh_entsize");

      for (size_t i = 0; i < shdr->sh_size / shdr->sh_entsize; ++i)
	{
	  GElf_Rela rela_mem;
	  GElf_Rela *rela = gelf_getrela (data, i, &rela_mem);
	  ELF_CHECK (rela != NULL, _("gelf_getrela failed: %s"));
	  adjust_reloc (&rela->r_info, map, map_size);
	  ELF_CHECK (gelf_update_rela (data, i, rela),
		     _("cannot update relocation: %s"));
	}
      break;

    case SHT_GROUP:
      {
	GElf_Shdr shdr_mem;
	GElf_Shdr *newshdr = gelf_getshdr (outscn, &shdr_mem);
	ELF_CHECK (newshdr != NULL, _("cannot get section header: %s"));
	if (newshdr->sh_info != STN_UNDEF)
	  {
	    newshdr->sh_info = map[newshdr->sh_info - 1];
	    update_shdr (outscn, newshdr);
	  }
	break;
      }

    case SHT_HASH:
      /* We must expand the table and rejigger its contents.  */
      {
	if (shdr->sh_entsize == 0)
	  error_exit (0, "HASH section cannot have zero sh_entsize");
	if (symshdr->sh_entsize == 0)
	  error_exit (0, "Symbol table cannot have zero sh_entsize");
	const size_t nsym = symshdr->sh_size / symshdr->sh_entsize;
	const size_t onent = shdr->sh_size / shdr->sh_entsize;
	if (data->d_size != shdr->sh_size)
	  error_exit (0, "HASH section has inconsistent size");

#define CONVERT_HASH(Hash_Word)						      \
	{								      \
	  const Hash_Word *const old_hash = data->d_buf;		      \
	  const size_t nbucket = old_hash[0];				      \
	  const size_t nchain = old_hash[1];				      \
	  const Hash_Word *const old_bucket = &old_hash[2];		      \
	  const Hash_Word *const old_chain = &old_bucket[nbucket];	      \
	  if (onent != 2 + nbucket + nchain)				      \
	    error_exit (0, "HASH section has inconsistent entsize");	      \
									      \
	  const size_t nent = 2 + nbucket + nsym;			      \
	  Hash_Word *const new_hash = xcalloc (nent, sizeof new_hash[0]);     \
	  Hash_Word *const new_bucket = &new_hash[2];			      \
	  Hash_Word *const new_chain = &new_bucket[nbucket];		      \
									      \
	  new_hash[0] = nbucket;					      \
	  new_hash[1] = nsym;						      \
	  for (size_t i = 0; i < nbucket; ++i)				      \
	    if (old_bucket[i] != STN_UNDEF)				      \
	      new_bucket[i] = map[old_bucket[i] - 1];			      \
									      \
	  for (size_t i = 1; i < nchain; ++i)				      \
	    if (old_chain[i] != STN_UNDEF)				      \
	      new_chain[map[i - 1]] = map[old_chain[i] - 1];		      \
									      \
	  record_new_data (new_hash);					\
	  data->d_buf = new_hash;					      \
	  data->d_size = nent * sizeof new_hash[0];			      \
	}

	switch (shdr->sh_entsize)
	  {
	  case 4:
	    CONVERT_HASH (Elf32_Word);
	    break;
	  case 8:
	    CONVERT_HASH (Elf64_Xword);
	    break;
	  default:
	    abort ();
	  }

	elf_flagdata (data, ELF_C_SET, ELF_F_DIRTY);
	update_sh_size (outscn, data);

#undef	CONVERT_HASH
      }
      break;

    case SHT_GNU_versym:
      /* We must expand the table and move its elements around.  */
      {
	if (shdr->sh_entsize == 0)
	  error_exit (0, "GNU_versym section cannot have zero sh_entsize");
	if (symshdr->sh_entsize == 0)
	  error_exit (0, "Symbol table cannot have zero sh_entsize");
	const size_t nent = symshdr->sh_size / symshdr->sh_entsize;
	const size_t onent = shdr->sh_size / shdr->sh_entsize;
	assert (nent >= onent);

	/* We don't bother using gelf_update_versym because there is
	   really no conversion to be done.  */
	assert (sizeof (Elf32_Versym) == sizeof (GElf_Versym));
	assert (sizeof (Elf64_Versym) == sizeof (GElf_Versym));
	GElf_Versym *versym = xcalloc (nent, sizeof versym[0]);

	for (size_t i = 1; i < onent; ++i)
	  {
	    GElf_Versym *v = gelf_getversym (data, i, &versym[map[i - 1]]);
	    ELF_CHECK (v != NULL, _("cannot get symbol version: %s"));
	  }

	record_new_data (versym);
	data->d_buf = versym;
	data->d_size = nent * sizeof versym[0];
	elf_flagdata (data, ELF_C_SET, ELF_F_DIRTY);
	update_sh_size (outscn, data);
      }
      break;

    default:
      error_exit (0,
		  _("unexpected section type in [%zu] with sh_link to symtab"),
		  elf_ndxscn (inscn));
    }
}

/* Adjust all the relocation sections in the file.  */
static void
adjust_all_relocs (Elf *elf, Elf_Scn *symtab, const GElf_Shdr *symshdr,
		   size_t map[], size_t map_size, bool scn_filter[])
{
  size_t new_sh_link = elf_ndxscn (symtab);
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    if (scn != symtab)
      {
	if (scn_filter != NULL)
	  {
	    size_t ndx = elf_ndxscn (scn);

	    /* Skip relocations that were already mapped during adjust_relocs
	       for the stripped symtab.  This is to avoid mapping a relocation's
	       symbol index from X to Y during the first adjust_relocs and then
	       wrongly remapping it from Y to Z during the second call.  */
	    if (scn_filter[ndx])
	      continue;
	  }

	GElf_Shdr shdr_mem;
	GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
	ELF_CHECK (shdr != NULL, _("cannot get section header: %s"));

	if (shdr->sh_type != SHT_NOBITS && shdr->sh_link == new_sh_link)
	  adjust_relocs (scn, scn, shdr, map, map_size, symshdr);
      }
}

/* The original file probably had section symbols for all of its
   sections, even the unallocated ones.  To match it as closely as
   possible, add in section symbols for the added sections.  */
static Elf_Data *
add_new_section_symbols (Elf_Scn *old_symscn, size_t old_shnum,
			 Elf *elf, bool rel, Elf_Scn *symscn, size_t shnum)
{
  const size_t added = shnum - old_shnum;

  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr = gelf_getshdr (symscn, &shdr_mem);
  ELF_CHECK (shdr != NULL, _("cannot get section header: %s"));
  if (shdr->sh_entsize == 0)
    error_exit (0, "Symbol table section cannot have zero sh_entsize");

  const size_t nsym = shdr->sh_size / shdr->sh_entsize;
  size_t symndx_map[nsym - 1];

  shdr->sh_info += added;
  shdr->sh_size += added * shdr->sh_entsize;
  update_shdr (symscn, shdr);

  Elf_Data *symdata = elf_getdata (symscn, NULL);
  Elf_Data *shndxdata = NULL;	/* XXX */

  symdata->d_size = shdr->sh_size;
  symdata->d_buf = xmalloc (symdata->d_size);
  record_new_data (symdata->d_buf);

  /* Copy the existing section symbols.  */
  Elf_Data *old_symdata = elf_getdata (old_symscn, NULL);
  for (size_t i = 0; i < old_shnum; ++i)
    {
      GElf_Sym sym_mem;
      GElf_Word shndx = SHN_UNDEF;
      GElf_Sym *sym = gelf_getsymshndx (old_symdata, shndxdata,
					i, &sym_mem, &shndx);
      ELF_CHECK (sym != NULL, _("cannot get symbol table entry: %s"));
      ELF_CHECK (gelf_update_symshndx (symdata, shndxdata, i,
				       sym, shndx),
		 _("cannot update symbol table: %s"));

      if (i > 0)
	symndx_map[i - 1] = i;
    }

  /* Add in the new section symbols.  */
  for (size_t i = old_shnum; i < shnum; ++i)
    {
      GElf_Shdr i_shdr_mem;
      GElf_Shdr *i_shdr = gelf_getshdr (elf_getscn (elf, i), &i_shdr_mem);
      ELF_CHECK (i_shdr != NULL, _("cannot get section header: %s"));
      GElf_Sym sym =
	{
	  .st_value = rel ? 0 : i_shdr->sh_addr,
	  .st_info = GELF_ST_INFO (STB_LOCAL, STT_SECTION),
	  .st_shndx = i < SHN_LORESERVE ? i : SHN_XINDEX
	};
      GElf_Word shndx = i < SHN_LORESERVE ? SHN_UNDEF : i;
      ELF_CHECK (gelf_update_symshndx (symdata, shndxdata, i,
				       &sym, shndx),
		 _("cannot update symbol table: %s"));
    }

  /* Now copy the rest of the existing symbols.  */
  for (size_t i = old_shnum; i < nsym; ++i)
    {
      GElf_Sym sym_mem;
      GElf_Word shndx = SHN_UNDEF;
      GElf_Sym *sym = gelf_getsymshndx (old_symdata, shndxdata,
					i, &sym_mem, &shndx);
      ELF_CHECK (sym != NULL, _("cannot get symbol table entry: %s"));
      ELF_CHECK (gelf_update_symshndx (symdata, shndxdata,
				       i + added, sym, shndx),
		 _("cannot update symbol table: %s"));

      symndx_map[i - 1] = i + added;
    }

  /* Adjust any relocations referring to the old symbol table.  */
  adjust_all_relocs (elf, symscn, shdr, symndx_map, nsym - 1, NULL);

  return symdata;
}

/* This has the side effect of updating STT_SECTION symbols' values,
   in case of prelink adjustments.  */
static Elf_Data *
check_symtab_section_symbols (Elf *elf, bool rel, Elf_Scn *scn,
			      size_t shnum, size_t shstrndx,
			      Elf_Scn *oscn, size_t oshnum, size_t oshstrndx,
			      size_t debuglink)
{
  size_t n = symtab_count_leading_section_symbols (elf, oscn, oshnum,
						   elf_getdata (scn, NULL));

  if (n == oshnum)
    return add_new_section_symbols (oscn, n, elf, rel, scn, shnum);

  if (n == oshstrndx || (n == debuglink && n == oshstrndx - 1))
    return add_new_section_symbols (oscn, n, elf, rel, scn, shstrndx);

  return NULL;
}

struct section
{
  Elf_Scn *scn;
  const char *name;
  const char *sig;
  Elf_Scn *outscn;
  Dwelf_Strent *strent;
  GElf_Shdr shdr;
};

static int
compare_alloc_sections (const struct section *s1, const struct section *s2,
			bool rel)
{
  if (!rel)
    {
      /* Sort by address.  */
      if (s1->shdr.sh_addr < s2->shdr.sh_addr)
	return -1;
      if (s1->shdr.sh_addr > s2->shdr.sh_addr)
	return 1;
    }

  /* At the same address, preserve original section order.  */
  return (ssize_t) elf_ndxscn (s1->scn) - (ssize_t) elf_ndxscn (s2->scn);
}

static int
compare_unalloc_sections (const GElf_Shdr *shdr1, const GElf_Shdr *shdr2,
			  const char *name1, const char *name2,
			  const char *sig1, const char *sig2)
{
  /* Sort by sh_flags as an arbitrary ordering.  */
  if (shdr1->sh_flags < shdr2->sh_flags)
    return -1;
  if (shdr1->sh_flags > shdr2->sh_flags)
    return 1;

  /* Sizes should be the same.  */
  if (shdr1->sh_size < shdr2->sh_size)
    return -1;
  if (shdr1->sh_size > shdr2->sh_size)
    return 1;

  /* Are they both SHT_GROUP sections? Then compare signatures.  */
  if (sig1 != NULL && sig2 != NULL)
    return strcmp (sig1, sig2);

  /* Sort by name as last resort.  */
  return strcmp (name1, name2);
}

static int
compare_sections (const void *a, const void *b, bool rel)
{
  const struct section *s1 = a;
  const struct section *s2 = b;

  /* Sort all non-allocated sections last.  */
  if ((s1->shdr.sh_flags ^ s2->shdr.sh_flags) & SHF_ALLOC)
    return (s1->shdr.sh_flags & SHF_ALLOC) ? -1 : 1;

  return ((s1->shdr.sh_flags & SHF_ALLOC)
	  ? compare_alloc_sections (s1, s2, rel)
	  : compare_unalloc_sections (&s1->shdr, &s2->shdr,
				      s1->name, s2->name,
				      s1->sig, s2->sig));
}

static int
compare_sections_rel (const void *a, const void *b)
{
  return compare_sections (a, b, true);
}

static int
compare_sections_nonrel (const void *a, const void *b)
{
  return compare_sections (a, b, false);
}


struct symbol
{
  size_t *map;

  union
  {
    const char *name;
    Dwelf_Strent *strent;
  };
  union
  {
    struct
    {
      GElf_Addr value;
      GElf_Xword size;
      GElf_Word shndx;
      union
      {
	struct
	{
	  uint8_t info;
	  uint8_t other;
	} info;
	int16_t compare;
      };
    };

    /* For a symbol discarded after first sort, this matches its better's
       map pointer.  */
    size_t *duplicate;
  };
};

/* Collect input symbols into our internal form.  */
static void
collect_symbols (Elf *outelf, bool rel, Elf_Scn *symscn, Elf_Scn *strscn,
		 const size_t nent, const GElf_Addr bias,
		 const size_t scnmap[], struct symbol *table, size_t *map,
		 struct section *split_bss)
{
  Elf_Data *symdata = elf_getdata (symscn, NULL);
  ELF_CHECK (symdata != NULL, _("cannot get symbol section data: %s"));
  Elf_Data *strdata = elf_getdata (strscn, NULL);
  ELF_CHECK (strdata != NULL, _("cannot get string section data: %s"));
  Elf_Data *shndxdata = NULL;	/* XXX */

  for (size_t i = 1; i < nent; ++i)
    {
      GElf_Sym sym_mem;
      GElf_Word shndx = SHN_UNDEF;
      GElf_Sym *sym = gelf_getsymshndx (symdata, shndxdata, i,
					&sym_mem, &shndx);
      ELF_CHECK (sym != NULL, _("cannot get symbol table entry: %s"));
      if (sym->st_shndx != SHN_XINDEX)
	shndx = sym->st_shndx;

      if (sym->st_name >= strdata->d_size
	  || memrchr (strdata->d_buf + sym->st_name, '\0',
		      strdata->d_size - sym->st_name) == NULL)
	error_exit (0,
		    _("invalid string offset in symbol [%zu]"), i);

      struct symbol *s = &table[i - 1];
      s->map = &map[i - 1];
      s->name = strdata->d_buf + sym->st_name;
      s->value = sym->st_value + bias;
      s->size = sym->st_size;
      s->shndx = shndx;
      s->info.info = sym->st_info;
      s->info.other = sym->st_other;
      s->duplicate = NULL;

      if (scnmap != NULL && shndx != SHN_UNDEF && shndx < SHN_LORESERVE)
	s->shndx = scnmap[shndx - 1];

      if (GELF_ST_TYPE (s->info.info) == STT_SECTION && !rel)
	{
	  /* Update the value to match the output section.  */
	  GElf_Shdr shdr_mem;
	  GElf_Shdr *shdr = gelf_getshdr (elf_getscn (outelf, s->shndx),
					  &shdr_mem);
	  ELF_CHECK (shdr != NULL, _("cannot get section header: %s"));
	  s->value = shdr->sh_addr;
	}
      else if (split_bss != NULL
	       && s->value < split_bss->shdr.sh_addr
	       && s->value >= split_bss[-1].shdr.sh_addr
	       && shndx == elf_ndxscn (split_bss->outscn))
	/* This symbol was in .bss and was split into .dynbss.  */
	s->shndx = elf_ndxscn (split_bss[-1].outscn);
    }
}


#define CMP(value)							      \
  if (s1->value < s2->value)						      \
    return -1;								      \
  if (s1->value > s2->value)						      \
    return 1

/* Symbol comparison used to sort symbols in preparation for deduplication.  */
static int
compare_symbols (const void *a, const void *b)
{
  const struct symbol *s1 = a;
  const struct symbol *s2 = b;

  CMP (value);
  CMP (size);
  CMP (shndx);

  int res = s1->compare - s2->compare;
  if (res != 0)
    return res;

  res = strcmp (s1->name, s2->name);
  if (res != 0)
    return res;

  /* Duplicates still have distinct positions in the symbol index map.
     Compare map positions to ensure that duplicate symbols are ordered
     consistently even if the sort function is unstable.  */
  CMP (map);
  error_exit (0, _("found two identical index map positions."));
}

/* Symbol comparison used to deduplicate symbols found in both the stripped
   and unstripped input files.

   Similar to compare_symbols, but does not differentiate symbols based
   on their position in the symbol index map.  Duplicates can't be found
   by comparing index map postions because they always have distinct
   positions in the map.  */
static int
compare_symbols_duplicate (const void *a, const void *b)
{
  const struct symbol *s1 = a;
  const struct symbol *s2 = b;

  CMP (value);
  CMP (size);
  CMP (shndx);

  return (s1->compare - s2->compare) ?: strcmp (s1->name, s2->name);
}

/* Compare symbols for output order after slots have been assigned.  */
static int
compare_symbols_output (const void *a, const void *b)
{
  const struct symbol *s1 = a;
  const struct symbol *s2 = b;
  int cmp;

  /* Sort discarded symbols last.  */
  cmp = (s1->name == NULL) - (s2->name == NULL);

  if (cmp == 0)
    /* Local symbols must come first.  */
    cmp = ((GELF_ST_BIND (s2->info.info) == STB_LOCAL)
	   - (GELF_ST_BIND (s1->info.info) == STB_LOCAL));

  if (cmp == 0)
    /* binutils always puts section symbols first.  */
    cmp = ((GELF_ST_TYPE (s2->info.info) == STT_SECTION)
	   - (GELF_ST_TYPE (s1->info.info) == STT_SECTION));

  if (cmp == 0)
    {
      if (GELF_ST_TYPE (s1->info.info) == STT_SECTION)
	{
	  /* binutils always puts section symbols in section index order.  */
	  CMP (shndx);
	  else if (s1 != s2)
	    error_exit (0, _("section symbols in unexpected order"));
	}

      /* Nothing really matters, so preserve the original order.  */
      CMP (map);
      else if (s1 != s2)
	error_exit (0, _("found two identical symbols"));
    }

  return cmp;
}

#undef CMP

/* Return true if the flags of the sections match, ignoring the SHF_INFO_LINK
   flag if the section contains relocation information.  */
static bool
sections_flags_match (Elf64_Xword sh_flags1, Elf64_Xword sh_flags2,
		      Elf64_Word sh_type)
{
  if (sh_type == SHT_REL || sh_type == SHT_RELA)
    {
      sh_flags1 &= ~SHF_INFO_LINK;
      sh_flags2 &= ~SHF_INFO_LINK;
    }

  return sh_flags1 == sh_flags2;
}

/* Return true iff the flags, size, and name match.  */
static bool
sections_match (const struct section *sections, size_t i,
		const GElf_Shdr *shdr, const char *name)
{
  return (sections_flags_match (sections[i].shdr.sh_flags, shdr->sh_flags,
				sections[i].shdr.sh_type)
	  && (sections[i].shdr.sh_size == shdr->sh_size
	      || (sections[i].shdr.sh_size < shdr->sh_size
		  && section_can_shrink (&sections[i].shdr)))
	  && !strcmp (sections[i].name, name));
}

/* Locate a matching allocated section in SECTIONS.  */
static struct section *
find_alloc_section (const GElf_Shdr *shdr, GElf_Addr bias, const char *name,
		    struct section sections[], size_t nalloc)
{
  const GElf_Addr addr = shdr->sh_addr + bias;
  size_t l = 0, u = nalloc;
  while (l < u)
    {
      size_t i = (l + u) / 2;
      if (addr < sections[i].shdr.sh_addr)
	u = i;
      else if (addr > sections[i].shdr.sh_addr)
	l = i + 1;
      else
	{
	  /* We've found allocated sections with this address.
	     Find one with matching size, flags, and name.  */
	  while (i > 0 && sections[i - 1].shdr.sh_addr == addr)
	    --i;
	  for (; i < nalloc && sections[i].shdr.sh_addr == addr;
	       ++i)
	    if (sections_match (sections, i, shdr, name))
	      return &sections[i];
	  break;
	}
    }
  return NULL;
}

static inline const char *
get_section_name (size_t ndx, const GElf_Shdr *shdr, const Elf_Data *shstrtab)
{
  if (shdr->sh_name >= shstrtab->d_size)
    error_exit (0, _("cannot read section [%zu] name: %s"),
		ndx, elf_errmsg (-1));
  return shstrtab->d_buf + shdr->sh_name;
}

/* Returns the signature of a group section, or NULL if the given
   section isn't a group.  */
static const char *
get_group_sig (Elf *elf, GElf_Shdr *shdr)
{
  if (shdr->sh_type != SHT_GROUP)
    return NULL;

  Elf_Scn *symscn = elf_getscn (elf, shdr->sh_link);
  if (symscn == NULL)
    error_exit (0, _("bad sh_link for group section: %s"),
		elf_errmsg (-1));

  GElf_Shdr symshdr_mem;
  GElf_Shdr *symshdr = gelf_getshdr (symscn, &symshdr_mem);
  if (symshdr == NULL)
    error_exit (0, _("couldn't get shdr for group section: %s"),
		elf_errmsg (-1));

  Elf_Data *symdata = elf_getdata (symscn, NULL);
  if (symdata == NULL)
    error_exit (0, _("bad data for group symbol section: %s"),
		elf_errmsg (-1));

  GElf_Sym sym_mem;
  GElf_Sym *sym = gelf_getsym (symdata, shdr->sh_info, &sym_mem);
  if (sym == NULL)
    error_exit (0, _("couldn't get symbol for group section: %s"),
		elf_errmsg (-1));

  const char *sig = elf_strptr (elf, symshdr->sh_link, sym->st_name);
  if (sig == NULL)
    error_exit (0, _("bad symbol name for group section: %s"),
		elf_errmsg (-1));

  return sig;
}

static inline bool
check_match (bool match, Elf_Scn *scn, const char *name)
{
  if (!match)
    {
      error (0, 0, _("cannot find matching section for [%zu] '%s'"),
	     elf_ndxscn (scn), name);
      return true;
    }

  return false;
}


/* Fix things up when prelink has moved some allocated sections around
   and the debuginfo file's section headers no longer match up.
   This fills in SECTIONS[0..NALLOC-1].outscn or exits.
   If there was a .bss section that was split into two sections
   with the new one preceding it in sh_addr, we return that pointer.  */
static struct section *
find_alloc_sections_prelink (Elf *debug, Elf_Data *debug_shstrtab,
			     Elf *main, const GElf_Ehdr *main_ehdr,
			     Elf_Data *main_shstrtab, GElf_Addr bias,
			     struct section *sections,
			     size_t nalloc, size_t nsections)
{
  Elf_Scn *undo = NULL;
  for (size_t i = nalloc; i < nsections; ++i)
    {
      const struct section *sec = &sections[i];
      if (sec->shdr.sh_type == SHT_PROGBITS
	  && !(sec->shdr.sh_flags & SHF_ALLOC)
	  && !strcmp (sec->name, ".gnu.prelink_undo"))
	{
	  undo = sec->scn;
	  break;
	}
    }

  /* Find the original allocated sections before prelinking.  */
  struct section *undo_sections = NULL;
  size_t undo_nalloc = 0;
  if (undo != NULL)
    {
      /* Clear assignments that might have been bogus.  */
      for (size_t i = 0; i < nalloc; ++i)
	sections[i].outscn = NULL;

      Elf_Data *undodata = elf_rawdata (undo, NULL);
      ELF_CHECK (undodata != NULL,
		 _("cannot read '.gnu.prelink_undo' section: %s"));

      union
      {
	Elf32_Ehdr e32;
	Elf64_Ehdr e64;
      } ehdr;
      Elf_Data dst =
	{
	  .d_buf = &ehdr,
	  .d_size = sizeof ehdr,
	  .d_type = ELF_T_EHDR,
	  .d_version = EV_CURRENT
	};
      Elf_Data src = *undodata;
      src.d_size = gelf_fsize (main, ELF_T_EHDR, 1, EV_CURRENT);
      src.d_type = ELF_T_EHDR;
      ELF_CHECK (gelf_xlatetom (main, &dst, &src,
				main_ehdr->e_ident[EI_DATA]) != NULL,
		 _("cannot read '.gnu.prelink_undo' section: %s"));

      uint_fast16_t phnum;
      uint_fast16_t shnum;  /* prelink doesn't handle > SHN_LORESERVE.  */
      if (ehdr.e32.e_ident[EI_CLASS] == ELFCLASS32)
	{
	  phnum = ehdr.e32.e_phnum;
	  shnum = ehdr.e32.e_shnum;
	}
      else
	{
	  phnum = ehdr.e64.e_phnum;
	  shnum = ehdr.e64.e_shnum;
	}

      bool class32 = ehdr.e32.e_ident[EI_CLASS] == ELFCLASS32;
      size_t shsize = class32 ? sizeof (Elf32_Shdr) : sizeof (Elf64_Shdr);
      if (unlikely (shnum == 0 || shnum > SIZE_MAX / shsize + 1))
	error_exit (0, _("overflow with shnum = %zu in '%s' section"),
		    (size_t) shnum, ".gnu.prelink_undo");

      --shnum;

      size_t phsize = gelf_fsize (main, ELF_T_PHDR, phnum, EV_CURRENT);
      src.d_buf += src.d_size + phsize;
      src.d_size = gelf_fsize (main, ELF_T_SHDR, shnum, EV_CURRENT);
      src.d_type = ELF_T_SHDR;
      if ((size_t) (src.d_buf - undodata->d_buf) > undodata->d_size
	  || undodata->d_size - (src.d_buf - undodata->d_buf) != src.d_size)
	error_exit (0, _("invalid contents in '%s' section"),
		    ".gnu.prelink_undo");

      const size_t shdr_bytes = shnum * shsize;
      void *shdr = xmalloc (shdr_bytes);
      dst.d_buf = shdr;
      dst.d_size = shdr_bytes;
      ELF_CHECK (gelf_xlatetom (main, &dst, &src,
				main_ehdr->e_ident[EI_DATA]) != NULL,
		 _("cannot read '.gnu.prelink_undo' section: %s"));

      undo_sections = xmalloc (shnum * sizeof undo_sections[0]);
      for (size_t i = 0; i < shnum; ++i)
	{
	  struct section *sec = &undo_sections[undo_nalloc];
	  Elf32_Shdr (*s32)[shnum] = shdr;
	  Elf64_Shdr (*s64)[shnum] = shdr;
	  if (class32)
	    {
#define COPY(field) sec->shdr.field = (*s32)[i].field
	      COPY (sh_name);
	      COPY (sh_type);
	      COPY (sh_flags);
	      COPY (sh_addr);
	      COPY (sh_offset);
	      COPY (sh_size);
	      COPY (sh_link);
	      COPY (sh_info);
	      COPY (sh_addralign);
	      COPY (sh_entsize);
#undef	COPY
	    }
	  else
	    sec->shdr = (*s64)[i];
	  if (sec->shdr.sh_flags & SHF_ALLOC)
	    {
	      sec->shdr.sh_addr += bias;
	      sec->name = get_section_name (i + 1, &sec->shdr, main_shstrtab);
	      sec->scn = elf_getscn (main, i + 1); /* Really just for ndx.  */
	      sec->outscn = NULL;
	      sec->strent = NULL;
	      sec->sig = get_group_sig (main, &sec->shdr);
	      ++undo_nalloc;
	    }
	}
      qsort (undo_sections, undo_nalloc,
	     sizeof undo_sections[0], compare_sections_nonrel);
      free (shdr);
    }

  bool fail = false;
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (debug, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
      ELF_CHECK (shdr != NULL, _("cannot get section header: %s"));

      if (!(shdr->sh_flags & SHF_ALLOC))
	continue;

      const char *name = get_section_name (elf_ndxscn (scn), shdr,
					   debug_shstrtab);

      if (undo_sections != NULL)
	{
	  struct section *sec = find_alloc_section (shdr, 0, name,
						    undo_sections,
						    undo_nalloc);
	  if (sec != NULL)
	    {
	      sec->outscn = scn;
	      continue;
	    }
	}

      /* If there is no prelink info, we are just here to find
	 the sections to give error messages about.  */
      for (size_t i = 0; shdr != NULL && i < nalloc; ++i)
	if (sections[i].outscn == scn)
	  shdr = NULL;
      fail |= check_match (shdr == NULL, scn, name);
    }

  if (fail)
    exit (EXIT_FAILURE);

  /* Now we have lined up output sections for each of the original sections
     before prelinking.  Translate those to the prelinked sections.
     This matches what prelink's undo_sections does.  */
  struct section *split_bss = NULL;
  for (size_t i = 0; i < undo_nalloc; ++i)
    {
      const struct section *undo_sec = &undo_sections[i];

      const char *name = undo_sec->name;
      scn = undo_sec->scn; /* This is just for elf_ndxscn.  */

      for (size_t j = 0; j < nalloc; ++j)
	{
	  struct section *sec = &sections[j];
#define RELA_SCALED(field) \
	  (2 * sec->shdr.field == 3 * undo_sec->shdr.field)
	  if (sec->outscn == NULL
	      && sec->shdr.sh_name == undo_sec->shdr.sh_name
	      && sec->shdr.sh_flags == undo_sec->shdr.sh_flags
	      && sec->shdr.sh_addralign == undo_sec->shdr.sh_addralign
	      && (((sec->shdr.sh_type == undo_sec->shdr.sh_type
		    && sec->shdr.sh_entsize == undo_sec->shdr.sh_entsize
		    && (sec->shdr.sh_size == undo_sec->shdr.sh_size
			|| (sec->shdr.sh_size > undo_sec->shdr.sh_size
			    && main_ehdr->e_type == ET_EXEC
			    && !strcmp (sec->name, ".dynstr"))))
		   || (sec->shdr.sh_size == undo_sec->shdr.sh_size
		       && ((sec->shdr.sh_entsize == undo_sec->shdr.sh_entsize
			    && undo_sec->shdr.sh_type == SHT_NOBITS)
			   || undo_sec->shdr.sh_type == SHT_PROGBITS)
		       && !strcmp (sec->name, ".plt")))
		  || (sec->shdr.sh_type == SHT_RELA
		      && undo_sec->shdr.sh_type == SHT_REL
		      && RELA_SCALED (sh_entsize) && RELA_SCALED (sh_size))
		  || (sec->shdr.sh_entsize == undo_sec->shdr.sh_entsize
		      && (sec->shdr.sh_type == undo_sec->shdr.sh_type
			  || (sec->shdr.sh_type == SHT_PROGBITS
			      && undo_sec->shdr.sh_type == SHT_NOBITS))
		      && sec->shdr.sh_size <= undo_sec->shdr.sh_size
		      && (!strcmp (sec->name, ".bss")
			  || !strcmp (sec->name, ".sbss"))
		      && (sec->shdr.sh_size == undo_sec->shdr.sh_size
			  || (split_bss = sec) > sections))))
	    {
	      sec->outscn = undo_sec->outscn;
	      undo_sec = NULL;
	      break;
	    }
	}

      fail |= check_match (undo_sec == NULL, scn, name);
    }

  free (undo_sections);

  if (fail)
    exit (EXIT_FAILURE);

  return split_bss;
}

/* Create new .shstrtab contents, subroutine of copy_elided_sections.
   This can't be open coded there and still use variable-length auto arrays,
   since the end of our block would free other VLAs too.  */
static Elf_Data *
new_shstrtab (Elf *unstripped, size_t unstripped_shnum,
	      Elf_Data *shstrtab, size_t unstripped_shstrndx,
	      struct section *sections, size_t stripped_shnum,
	      Dwelf_Strtab *strtab)
{
  if (strtab == NULL)
    return NULL;

  Dwelf_Strent *unstripped_strent[unstripped_shnum];
  memset (unstripped_strent, 0, sizeof unstripped_strent);
  for (struct section *sec = sections;
       sec < &sections[stripped_shnum - 1];
       ++sec)
    if (sec->outscn != NULL)
      {
	if (sec->strent == NULL)
	  {
	    sec->strent = dwelf_strtab_add (strtab, sec->name);
	    ELF_CHECK (sec->strent != NULL,
		       _("cannot add section name to string table: %s"));
	  }
	unstripped_strent[elf_ndxscn (sec->outscn) - 1] = sec->strent;
      }

  /* Add names of sections we aren't touching.  */
  for (size_t i = 0; i < unstripped_shnum - 1; ++i)
    if (unstripped_strent[i] == NULL)
      {
	Elf_Scn *scn = elf_getscn (unstripped, i + 1);
	GElf_Shdr shdr_mem;
	GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
	ELF_CHECK (shdr != NULL, _("cannot get section header: %s"));
	const char *name = get_section_name (i + 1, shdr, shstrtab);
	unstripped_strent[i] = dwelf_strtab_add (strtab, name);
	ELF_CHECK (unstripped_strent[i] != NULL,
		   _("cannot add section name to string table: %s"));
      }
    else
      unstripped_strent[i] = NULL;

  /* Now finalize the string table so we can get offsets.  */
  Elf_Data *strtab_data = elf_getdata (elf_getscn (unstripped,
						   unstripped_shstrndx), NULL);
  ELF_CHECK (elf_flagdata (strtab_data, ELF_C_SET, ELF_F_DIRTY),
	     _("cannot update section header string table data: %s"));
  if (dwelf_strtab_finalize (strtab, strtab_data) == NULL)
    error_exit (0, "Not enough memory to create string table");

  /* Update the sh_name fields of sections we aren't modifying later.  */
  for (size_t i = 0; i < unstripped_shnum - 1; ++i)
    if (unstripped_strent[i] != NULL)
      {
	Elf_Scn *scn = elf_getscn (unstripped, i + 1);
	GElf_Shdr shdr_mem;
	GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
	ELF_CHECK (shdr != NULL, _("cannot get section header: %s"));
	shdr->sh_name = dwelf_strent_off (unstripped_strent[i]);
	if (i + 1 == unstripped_shstrndx)
	  shdr->sh_size = strtab_data->d_size;
	update_shdr (scn, shdr);
      }

  return strtab_data;
}

/* Fill in any SHT_NOBITS sections in UNSTRIPPED by
   copying their contents and sh_type from STRIPPED.  */
static void
copy_elided_sections (Elf *unstripped, Elf *stripped,
		      const GElf_Ehdr *stripped_ehdr, GElf_Addr bias)
{
  size_t unstripped_shstrndx;
  ELF_CHECK (elf_getshdrstrndx (unstripped, &unstripped_shstrndx) == 0,
	     _("cannot get section header string table section index: %s"));

  size_t stripped_shstrndx;
  ELF_CHECK (elf_getshdrstrndx (stripped, &stripped_shstrndx) == 0,
	     _("cannot get section header string table section index: %s"));

  size_t unstripped_shnum;
  ELF_CHECK (elf_getshdrnum (unstripped, &unstripped_shnum) == 0,
	     _("cannot get section count: %s"));

  size_t stripped_shnum;
  ELF_CHECK (elf_getshdrnum (stripped, &stripped_shnum) == 0,
	     _("cannot get section count: %s"));

  if (unlikely (stripped_shnum > unstripped_shnum))
    error_exit (0, _("\
more sections in stripped file than debug file -- arguments reversed?"));

  if (unlikely (stripped_shnum == 0))
    error_exit (0, _("no sections in stripped file"));

  /* Used as sanity check for allocated section offset, if the section
     offset needs to be preserved.  We want to know the max size of the
     ELF file, to check if any existing section offsets are OK.  */
  int64_t max_off = -1;
  if (stripped_ehdr->e_type != ET_REL)
    {
      elf_flagelf (stripped, ELF_C_SET, ELF_F_LAYOUT);
      max_off = elf_update (stripped, ELF_C_NULL);
    }

  /* Cache the stripped file's section details.  */
  struct section sections[stripped_shnum - 1];
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (stripped, scn)) != NULL)
    {
      size_t i = elf_ndxscn (scn) - 1;
      GElf_Shdr *shdr = gelf_getshdr (scn, &sections[i].shdr);
      ELF_CHECK (shdr != NULL, _("cannot get section header: %s"));
      sections[i].name = elf_strptr (stripped, stripped_shstrndx,
				     shdr->sh_name);
      if (sections[i].name == NULL)
	error_exit (0, _("cannot read section [%zu] name: %s"),
		    elf_ndxscn (scn), elf_errmsg (-1));
      sections[i].scn = scn;
      sections[i].outscn = NULL;
      sections[i].strent = NULL;
      sections[i].sig = get_group_sig (stripped, shdr);
    }

  const struct section *stripped_symtab = NULL;

  /* Sort the sections, allocated by address and others after.  */
  qsort (sections, stripped_shnum - 1, sizeof sections[0],
	 stripped_ehdr->e_type == ET_REL
	 ? compare_sections_rel : compare_sections_nonrel);
  size_t nalloc = stripped_shnum - 1;
  while (nalloc > 0 && !(sections[nalloc - 1].shdr.sh_flags & SHF_ALLOC))
    {
      --nalloc;
      if (sections[nalloc].shdr.sh_type == SHT_SYMTAB)
	stripped_symtab = &sections[nalloc];
    }

  Elf_Data *shstrtab = elf_getdata (elf_getscn (unstripped,
						unstripped_shstrndx), NULL);
  ELF_CHECK (shstrtab != NULL,
	     _("cannot read section header string table: %s"));

  /* Match each debuginfo section with its corresponding stripped section.  */
  bool check_prelink = false;
  Elf_Scn *unstripped_symtab = NULL;
  size_t unstripped_strndx = 0;
  size_t alloc_avail = 0;
  scn = NULL;
  while ((scn = elf_nextscn (unstripped, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
      ELF_CHECK (shdr != NULL, _("cannot get section header: %s"));

      if (shdr->sh_type == SHT_SYMTAB)
	{
	  unstripped_symtab = scn;
	  unstripped_strndx = shdr->sh_link;
	  continue;
	}

      const size_t ndx = elf_ndxscn (scn);
      if (ndx == unstripped_shstrndx || ndx == unstripped_strndx)
	continue;

      const char *name = get_section_name (ndx, shdr, shstrtab);

      struct section *sec = NULL;
      if (shdr->sh_flags & SHF_ALLOC)
	{
	  if (stripped_ehdr->e_type != ET_REL)
	    {
	      /* Look for the section that matches.  */
	      sec = find_alloc_section (shdr, bias, name, sections, nalloc);
	      if (sec == NULL)
		{
		  /* We couldn't figure it out.  It may be a prelink issue.  */
		  check_prelink = true;
		  continue;
		}
	    }
	  else
	    {
	      /* The sh_addr of allocated sections does not help us,
		 but the order usually matches.  */
	      if (likely (sections_match (sections, alloc_avail, shdr, name)))
		sec = &sections[alloc_avail++];
	      else
		for (size_t i = alloc_avail + 1; i < nalloc; ++i)
		  if (sections_match (sections, i, shdr, name))
		    {
		      sec = &sections[i];
		      break;
		    }
	    }
	}
      else
	{
	  /* Locate a matching unallocated section in SECTIONS.  */
	  const char *sig = get_group_sig (unstripped, shdr);
	  size_t l = nalloc, u = stripped_shnum - 1;
	  while (l < u)
	    {
	      size_t i = (l + u) / 2;
	      struct section *section = &sections[i];
	      int cmp = compare_unalloc_sections (shdr, &section->shdr,
						  name, section->name,
						  sig, section->sig);
	      if (cmp < 0)
		u = i;
	      else if (cmp > 0)
		l = i + 1;
	      else
		{
		  sec = section;
		  break;
		}
	    }

	  if (sec == NULL)
	    {
	      /* An additional unallocated section is fine if not SHT_NOBITS.
		 We looked it up anyway in case it's an unallocated section
		 copied in both files (e.g. SHT_NOTE), and don't keep both.  */
	      if (shdr->sh_type != SHT_NOBITS)
		continue;

	      /* Somehow some old .debug files wound up with SHT_NOBITS
		 .comment sections, so let those pass.  */
	      if (!strcmp (name, ".comment"))
		continue;
	    }
	}

      if (sec == NULL)
	error_exit (0, _("cannot find matching section for [%zu] '%s'"),
		    elf_ndxscn (scn), name);

      sec->outscn = scn;
    }

  /* If that failed due to changes made by prelink, we take another tack.
     We keep track of a .bss section that was partly split into .dynbss
     so that collect_symbols can update symbols' st_shndx fields.  */
  struct section *split_bss = NULL;
  if (check_prelink)
    {
      Elf_Data *data = elf_getdata (elf_getscn (stripped, stripped_shstrndx),
				    NULL);
      ELF_CHECK (data != NULL,
		 _("cannot read section header string table: %s"));
      split_bss = find_alloc_sections_prelink (unstripped, shstrtab,
					       stripped, stripped_ehdr,
					       data, bias, sections,
					       nalloc, stripped_shnum - 1);
    }

  /* Make sure each main file section has a place to go.  */
  const struct section *stripped_dynsym = NULL;
  size_t debuglink = SHN_UNDEF;
  size_t ndx_sec_num = stripped_shnum - 1;
  size_t ndx_section[ndx_sec_num];
  Dwelf_Strtab *strtab = NULL;
  for (struct section *sec = sections;
       sec < &sections[ndx_sec_num];
       ++sec)
    {
      size_t secndx = elf_ndxscn (sec->scn);

      if (sec->outscn == NULL)
	{
	  /* We didn't find any corresponding section for this.  */

	  if (secndx == stripped_shstrndx)
	    {
	      /* We only need one .shstrtab.  */
	      ndx_section[secndx - 1] = unstripped_shstrndx;
	      continue;
	    }

	  if (unstripped_symtab != NULL && sec == stripped_symtab)
	    {
	      /* We don't need a second symbol table.  */
	      ndx_section[secndx - 1] = elf_ndxscn (unstripped_symtab);
	      continue;
	    }

	  if (unstripped_symtab != NULL && stripped_symtab != NULL
	      && secndx == stripped_symtab->shdr.sh_link
	      && unstripped_strndx != 0)
	    {
	      /* ... nor its string table.  */
	      ndx_section[secndx - 1] = unstripped_strndx;
	      continue;
	    }

	  if (!(sec->shdr.sh_flags & SHF_ALLOC)
	      && !strcmp (sec->name, ".gnu_debuglink"))
	    {
	      /* This was created by stripping.  We don't want it.  */
	      debuglink = secndx;
	      ndx_section[secndx - 1] = SHN_UNDEF;
	      continue;
	    }

	  sec->outscn = elf_newscn (unstripped);
	  Elf_Data *newdata = elf_newdata (sec->outscn);
	  ELF_CHECK (newdata != NULL && gelf_update_shdr (sec->outscn,
							  &sec->shdr),
		     _("cannot add new section: %s"));

	  if (strtab == NULL)
	    strtab = dwelf_strtab_init (true);
	  sec->strent = dwelf_strtab_add (strtab, sec->name);
	  ELF_CHECK (sec->strent != NULL,
		     _("cannot add section name to string table: %s"));
	}

      /* Cache the mapping of original section indices to output sections.  */
      ndx_section[secndx - 1] = elf_ndxscn (sec->outscn);
    }

  /* We added some sections, so we need a new shstrtab.  */
  Elf_Data *strtab_data = new_shstrtab (unstripped, unstripped_shnum,
					shstrtab, unstripped_shstrndx,
					sections, stripped_shnum,
					strtab);

  /* Get the updated section count.  */
  ELF_CHECK (elf_getshdrnum (unstripped, &unstripped_shnum) == 0,
	     _("cannot get section count: %s"));

  bool placed[unstripped_shnum - 1];
  memset (placed, 0, sizeof placed);

  /* Now update the output sections and copy in their data.  */
  GElf_Off offset = 0;
  for (const struct section *sec = sections;
       sec < &sections[stripped_shnum - 1];
       ++sec)
    if (sec->outscn != NULL)
      {
	GElf_Shdr shdr_mem;
	GElf_Shdr *shdr = gelf_getshdr (sec->outscn, &shdr_mem);
	ELF_CHECK (shdr != NULL, _("cannot get section header: %s"));

	/* In an ET_REL file under --relocate, the sh_addr of SHF_ALLOC
	   sections will have been set nonzero by relocation.  This
	   touched the shdrs of whichever file had the symtab.  sh_addr
	   is still zero in the corresponding shdr.  The relocated
	   address is what we want to use.  */
	if (stripped_ehdr->e_type != ET_REL
	    || !(shdr_mem.sh_flags & SHF_ALLOC)
	    || shdr_mem.sh_addr == 0)
	  shdr_mem.sh_addr = sec->shdr.sh_addr;

	shdr_mem.sh_type = sec->shdr.sh_type;
	shdr_mem.sh_size = sec->shdr.sh_size;
	shdr_mem.sh_info = sec->shdr.sh_info;
	shdr_mem.sh_link = sec->shdr.sh_link;

	/* Buggy binutils objdump might have stripped the SHF_INFO_LINK
	   put it back if necessary.  */
	if ((sec->shdr.sh_type == SHT_REL || sec->shdr.sh_type == SHT_RELA)
	    && sec->shdr.sh_flags != shdr_mem.sh_flags
	    && (sec->shdr.sh_flags & SHF_INFO_LINK) != 0)
	  shdr_mem.sh_flags |= SHF_INFO_LINK;

	if (sec->shdr.sh_link != SHN_UNDEF)
	  {
	    if (sec->shdr.sh_link > ndx_sec_num)
	      error_exit (0,
			  "section [%zd] has invalid sh_link %" PRId32,
			  elf_ndxscn (sec->scn), sec->shdr.sh_link);
	    shdr_mem.sh_link = ndx_section[sec->shdr.sh_link - 1];
	  }
	if (SH_INFO_LINK_P (&sec->shdr) && sec->shdr.sh_info != 0)
	  {
	    if (sec->shdr.sh_info > ndx_sec_num)
	      error_exit (0,
			  "section [%zd] has invalid sh_info %" PRId32,
			  elf_ndxscn (sec->scn), sec->shdr.sh_info);
	    shdr_mem.sh_info = ndx_section[sec->shdr.sh_info - 1];
	  }

	if (strtab != NULL)
	  shdr_mem.sh_name = dwelf_strent_off (sec->strent);

	Elf_Data *indata = elf_getdata (sec->scn, NULL);
	ELF_CHECK (indata != NULL, _("cannot get section data: %s"));
	Elf_Data *outdata = elf_getdata (sec->outscn, NULL);
	ELF_CHECK (outdata != NULL, _("cannot copy section data: %s"));
	*outdata = *indata;
	elf_flagdata (outdata, ELF_C_SET, ELF_F_DIRTY);

	/* Preserve the file layout of the allocated sections.  */
	if (stripped_ehdr->e_type != ET_REL && (shdr_mem.sh_flags & SHF_ALLOC))
	  {
	    if (max_off > 0 && sec->shdr.sh_offset > (Elf64_Off) max_off)
		error_exit (0,
			    "allocated section offset too large [%zd] %" PRIx64,
			    elf_ndxscn (sec->scn), sec->shdr.sh_offset);

	    shdr_mem.sh_offset = sec->shdr.sh_offset;
	    placed[elf_ndxscn (sec->outscn) - 1] = true;

	    const GElf_Off end_offset = (shdr_mem.sh_offset
					 + (shdr_mem.sh_type == SHT_NOBITS
					    ? 0 : shdr_mem.sh_size));
	    if (end_offset > offset)
	      offset = end_offset;
	  }

	update_shdr (sec->outscn, &shdr_mem);

	if (shdr_mem.sh_type == SHT_SYMTAB || shdr_mem.sh_type == SHT_DYNSYM)
	  {
	    /* We must adjust all the section indices in the symbol table.  */

	    Elf_Data *shndxdata = NULL;	/* XXX */

	    if (shdr_mem.sh_entsize == 0)
	      error_exit (0,
			  "SYMTAB section cannot have zero sh_entsize");
	    for (size_t i = 1; i < shdr_mem.sh_size / shdr_mem.sh_entsize; ++i)
	      {
		GElf_Sym sym_mem;
		GElf_Word shndx = SHN_UNDEF;
		GElf_Sym *sym = gelf_getsymshndx (outdata, shndxdata,
						  i, &sym_mem, &shndx);
		ELF_CHECK (sym != NULL,
			   _("cannot get symbol table entry: %s"));
		if (sym->st_shndx != SHN_XINDEX)
		  shndx = sym->st_shndx;

		if (shndx != SHN_UNDEF && shndx < SHN_LORESERVE)
		  {
		    if (shndx >= stripped_shnum)
		      error_exit (0,
				  _("symbol [%zu] has invalid section index"), i);

		    shndx = ndx_section[shndx - 1];
		    if (shndx < SHN_LORESERVE)
		      {
			sym->st_shndx = shndx;
			shndx = SHN_UNDEF;
		      }
		    else
		      sym->st_shndx = SHN_XINDEX;

		    ELF_CHECK (gelf_update_symshndx (outdata, shndxdata,
						     i, sym, shndx),
			       _("cannot update symbol table: %s"));
		  }
	      }

	    if (shdr_mem.sh_type == SHT_SYMTAB)
	      stripped_symtab = sec;
	    if (shdr_mem.sh_type == SHT_DYNSYM)
	      stripped_dynsym = sec;
	  }

	if (shdr_mem.sh_type == SHT_GROUP)
	  {
	    /* We must adjust all the section indices in the group.
	       Skip the first word, which is the section group flag.
	       Everything else is a section index.  */
	    Elf32_Word *shndx = (Elf32_Word *) outdata->d_buf;
	    for (size_t i = 1; i < shdr_mem.sh_size / sizeof (Elf32_Word); ++i)
	      if (shndx[i]  == SHN_UNDEF || shndx[i] >= stripped_shnum)
		error_exit (0,
			    _("group has invalid section index [%zd]"), i);
	      else
		shndx[i] = ndx_section[shndx[i] - 1];
	  }
      }

  /* We may need to update the symbol table.  */
  Elf_Data *symdata = NULL;
  Dwelf_Strtab *symstrtab = NULL;
  Elf_Data *symstrdata = NULL;
  if (unstripped_symtab != NULL && (stripped_symtab != NULL
				    || check_prelink /* Section adjustments. */
				    || (stripped_ehdr->e_type != ET_REL
					&& bias != 0)))
    {
      /* Merge the stripped file's symbol table into the unstripped one.  */
      const size_t stripped_nsym = (stripped_symtab == NULL ? 1
				    : (stripped_symtab->shdr.sh_size
				       / (stripped_symtab->shdr.sh_entsize == 0
					  ? 1
					  : stripped_symtab->shdr.sh_entsize)));

      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (unstripped_symtab, &shdr_mem);
      ELF_CHECK (shdr != NULL, _("cannot get section header: %s"));
      if (shdr->sh_entsize == 0)
	error_exit (0,
		    "unstripped SYMTAB section cannot have zero sh_entsize");
      const size_t unstripped_nsym = shdr->sh_size / shdr->sh_entsize;

      /* First collect all the symbols from both tables.  */

      const size_t total_syms = stripped_nsym - 1 + unstripped_nsym - 1;
      struct symbol *symbols = xmalloc (total_syms * sizeof (struct symbol));
      size_t *symndx_map = xmalloc (total_syms * sizeof (size_t));

      if (stripped_symtab != NULL)
	collect_symbols (unstripped, stripped_ehdr->e_type == ET_REL,
			 stripped_symtab->scn,
			 elf_getscn (stripped, stripped_symtab->shdr.sh_link),
			 stripped_nsym, 0, ndx_section,
			 symbols, symndx_map, NULL);

      Elf_Scn *unstripped_strtab = elf_getscn (unstripped, shdr->sh_link);
      collect_symbols (unstripped, stripped_ehdr->e_type == ET_REL,
		       unstripped_symtab, unstripped_strtab, unstripped_nsym,
		       stripped_ehdr->e_type == ET_REL ? 0 : bias, NULL,
		       &symbols[stripped_nsym - 1],
		       &symndx_map[stripped_nsym - 1], split_bss);

      /* Next, sort our array of all symbols.  */
      qsort (symbols, total_syms, sizeof symbols[0], compare_symbols);

      /* Now we can weed out the duplicates.  Assign remaining symbols
	 new slots, collecting a map from old indices to new.  */
      size_t nsym = 0;
      for (struct symbol *s = symbols; s < &symbols[total_syms]; ++s)
	{
	  /* Skip a section symbol for a removed section.  */
	  if (s->shndx == SHN_UNDEF
	      && GELF_ST_TYPE (s->info.info) == STT_SECTION)
	    {
	      s->name = NULL;	/* Mark as discarded. */
	      *s->map = STN_UNDEF;
	      s->duplicate = NULL;
	      continue;
	    }

	  struct symbol *n = s;
	  while (n + 1 < &symbols[total_syms]
		 && !compare_symbols_duplicate (s, n + 1))
	    ++n;

	  while (s < n)
	    {
	      /* This is a duplicate.  Its twin will get the next slot.  */
	      s->name = NULL;	/* Mark as discarded. */
	      s->duplicate = n->map;
	      ++s;
	    }

	  /* Allocate the next slot.  */
	  *s->map = ++nsym;
	}

      /* Now we sort again, to determine the order in the output.  */
      qsort (symbols, total_syms, sizeof symbols[0], compare_symbols_output);

      if (nsym < total_syms)
	/* The discarded symbols are now at the end of the table.  */
	assert (symbols[nsym].name == NULL);

      /* Now a final pass updates the map with the final order,
	 and builds up the new string table.  */
      symstrtab = dwelf_strtab_init (true);
      for (size_t i = 0; i < nsym; ++i)
	{
	  assert (symbols[i].name != NULL);
	  assert (*symbols[i].map != 0);
	  *symbols[i].map = 1 + i;
	  symbols[i].strent = dwelf_strtab_add (symstrtab, symbols[i].name);
	}

      /* Scan the discarded symbols too, just to update their slots
	 in SYMNDX_MAP to refer to their live duplicates.  */
      for (size_t i = nsym; i < total_syms; ++i)
	{
	  assert (symbols[i].name == NULL);
	  if (symbols[i].duplicate == NULL)
	    assert (*symbols[i].map == STN_UNDEF);
	  else
	    {
	      assert (*symbols[i].duplicate != STN_UNDEF);
	      *symbols[i].map = *symbols[i].duplicate;
	    }
	}

      /* Now we are ready to write the new symbol table.  */
      symdata = elf_getdata (unstripped_symtab, NULL);
      symstrdata = elf_getdata (unstripped_strtab, NULL);
      Elf_Data *shndxdata = NULL;	/* XXX */

      /* If symtab and the section header table share the string table
	 add the section names to the strtab and then (after finalizing)
	 fixup the section header sh_names.  Also dispose of the old data.  */
      Dwelf_Strent *unstripped_strent[unstripped_shnum - 1];
      if (unstripped_shstrndx == elf_ndxscn (unstripped_strtab))
	{
	  for (size_t i = 0; i < unstripped_shnum - 1; ++i)
	    {
	      Elf_Scn *sec = elf_getscn (unstripped, i + 1);
	      GElf_Shdr mem;
	      GElf_Shdr *hdr = gelf_getshdr (sec, &mem);
	      const char *name = get_section_name (i + 1, hdr, shstrtab);
	      unstripped_strent[i] = dwelf_strtab_add (symstrtab, name);
	      ELF_CHECK (unstripped_strent[i] != NULL,
			 _("cannot add section name to string table: %s"));
	    }

	  if (strtab != NULL)
	    {
	      dwelf_strtab_free (strtab);
	      free (strtab_data->d_buf);
	      strtab = NULL;
	    }
	}

      if (dwelf_strtab_finalize (symstrtab, symstrdata) == NULL)
	error_exit (0, "Not enough memory to create symbol table");

      elf_flagdata (symstrdata, ELF_C_SET, ELF_F_DIRTY);

      /* And update the section header names if necessary.  */
      if (unstripped_shstrndx == elf_ndxscn (unstripped_strtab))
	{
	  for (size_t i = 0; i < unstripped_shnum - 1; ++i)
	    {
	      Elf_Scn *sec = elf_getscn (unstripped, i + 1);
	      GElf_Shdr mem;
	      GElf_Shdr *hdr = gelf_getshdr (sec, &mem);
	      shdr->sh_name = dwelf_strent_off (unstripped_strent[i]);
	      update_shdr (sec, hdr);
	    }
	}

      /* Now update the symtab shdr.  Reload symtab shdr because sh_name
	 might have changed above. */
      shdr = gelf_getshdr (unstripped_symtab, &shdr_mem);
      ELF_CHECK (shdr != NULL, _("cannot get section header: %s"));

      shdr->sh_size = symdata->d_size = (1 + nsym) * shdr->sh_entsize;
      symdata->d_buf = xmalloc (symdata->d_size);
      record_new_data (symdata->d_buf);

      GElf_Sym sym;
      memset (&sym, 0, sizeof sym);
      ELF_CHECK (gelf_update_symshndx (symdata, shndxdata, 0, &sym, SHN_UNDEF),
		 _("cannot update symbol table: %s"));

      shdr->sh_info = 1;
      for (size_t i = 0; i < nsym; ++i)
	{
	  struct symbol *s = &symbols[i];

	  /* Fill in the symbol details.  */
	  sym.st_name = dwelf_strent_off (s->strent);
	  sym.st_value = s->value; /* Already biased to output address.  */
	  sym.st_size = s->size;
	  sym.st_shndx = s->shndx; /* Already mapped to output index.  */
	  sym.st_info = s->info.info;
	  sym.st_other = s->info.other;

	  /* Keep track of the number of leading local symbols.  */
	  if (GELF_ST_BIND (sym.st_info) == STB_LOCAL)
	    {
	      assert (shdr->sh_info == 1 + i);
	      shdr->sh_info = 1 + i + 1;
	    }

	  ELF_CHECK (gelf_update_symshndx (symdata, shndxdata, 1 + i,
					   &sym, SHN_UNDEF),
		     _("cannot update symbol table: %s"));

	}
      elf_flagdata (symdata, ELF_C_SET, ELF_F_DIRTY);
      update_shdr (unstripped_symtab, shdr);

      /* Track which sections are adjusted during the first round
	 of calls to adjust_relocs.  */
      bool scn_adjusted[unstripped_shnum];
      memset (scn_adjusted, 0, sizeof scn_adjusted);

      if (stripped_symtab != NULL)
	{
	  /* Adjust any relocations referring to the old symbol table.  */
	  const size_t old_sh_link = elf_ndxscn (stripped_symtab->scn);
	  for (const struct section *sec = sections;
	       sec < &sections[stripped_shnum - 1];
	       ++sec)
	    if (sec->outscn != NULL && sec->shdr.sh_link == old_sh_link)
	      {
		adjust_relocs (sec->outscn, sec->scn, &sec->shdr,
			       symndx_map, total_syms, shdr);
		scn_adjusted[elf_ndxscn (sec->outscn)] = true;
	      }
	}

      /* Also adjust references to the other old symbol table.  */
      adjust_all_relocs (unstripped, unstripped_symtab, shdr,
			 &symndx_map[stripped_nsym - 1],
			 total_syms - (stripped_nsym - 1),
			 scn_adjusted);

      free (symbols);
      free (symndx_map);
    }
  else if (stripped_symtab != NULL && stripped_shnum != unstripped_shnum)
    check_symtab_section_symbols (unstripped,
				  stripped_ehdr->e_type == ET_REL,
				  stripped_symtab->scn,
				  unstripped_shnum, unstripped_shstrndx,
				  stripped_symtab->outscn,
				  stripped_shnum, stripped_shstrndx,
				  debuglink);

  if (stripped_dynsym != NULL)
    (void) check_symtab_section_symbols (unstripped,
					 stripped_ehdr->e_type == ET_REL,
					 stripped_dynsym->outscn,
					 unstripped_shnum,
					 unstripped_shstrndx,
					 stripped_dynsym->scn, stripped_shnum,
					 stripped_shstrndx, debuglink);

  /* We need to preserve the layout of the stripped file so the
     phdrs will match up.  This requires us to do our own layout of
     the added sections.  We do manual layout even for ET_REL just
     so we can try to match what the original probably had.  */

  elf_flagelf (unstripped, ELF_C_SET, ELF_F_LAYOUT);

  if (offset == 0)
    /* For ET_REL we are starting the layout from scratch.  */
    offset = gelf_fsize (unstripped, ELF_T_EHDR, 1, EV_CURRENT);

  bool skip_reloc = false;
  do
    {
      skip_reloc = !skip_reloc;
      for (size_t i = 0; i < unstripped_shnum - 1; ++i)
	if (!placed[i])
	  {
	    scn = elf_getscn (unstripped, 1 + i);

	    GElf_Shdr shdr_mem;
	    GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
	    ELF_CHECK (shdr != NULL, _("cannot get section header: %s"));

	    /* We must make sure we have read in the data of all sections
	       beforehand and marked them to be written out.  When we're
	       modifying the existing file in place, we might overwrite
	       this part of the file before we get to handling the section.  */

	    ELF_CHECK (elf_flagdata (elf_getdata (scn, NULL),
				     ELF_C_SET, ELF_F_DIRTY),
		       _("cannot read section data: %s"));

	    if (skip_reloc
		&& (shdr->sh_type == SHT_REL || shdr->sh_type == SHT_RELA))
	      continue;

	    GElf_Off align = shdr->sh_addralign ?: 1;
	    offset = (offset + align - 1) & -align;
	    shdr->sh_offset = offset;
	    if (shdr->sh_type != SHT_NOBITS)
	      offset += shdr->sh_size;

	    update_shdr (scn, shdr);

	    if (unstripped_shstrndx == 1 + i)
	      {
		/* Place the section headers immediately after
		   .shstrtab, and update the ELF header.  */

		GElf_Ehdr ehdr_mem;
		GElf_Ehdr *ehdr = gelf_getehdr (unstripped, &ehdr_mem);
		ELF_CHECK (ehdr != NULL, _("cannot get ELF header: %s"));

		GElf_Off sh_align = gelf_getclass (unstripped) * 4;
		offset = (offset + sh_align - 1) & -sh_align;
		ehdr->e_shnum = unstripped_shnum;
		ehdr->e_shoff = offset;
		offset += unstripped_shnum * ehdr->e_shentsize;
		ELF_CHECK (gelf_update_ehdr (unstripped, ehdr),
			   _("cannot update ELF header: %s"));
	      }

	    placed[i] = true;
	  }
    }
  while (skip_reloc);

  size_t phnum;
  ELF_CHECK (elf_getphdrnum (stripped, &phnum) == 0,
	     _("cannot get number of program headers: %s"));

  if (phnum > 0)
    ELF_CHECK (gelf_newphdr (unstripped, phnum),
	       _("cannot create program headers: %s"));

  /* Copy each program header from the stripped file.  */
  for (size_t i = 0; i < phnum; ++i)
    {
      GElf_Phdr phdr_mem;
      GElf_Phdr *phdr = gelf_getphdr (stripped, i, &phdr_mem);
      ELF_CHECK (phdr != NULL, _("cannot get program header: %s"));

      ELF_CHECK (gelf_update_phdr (unstripped, i, phdr),
		 _("cannot update program header: %s"));
    }

  /* Finally, write out the file.  */
  ELF_CHECK (elf_update (unstripped, ELF_C_WRITE) > 0,
	     _("cannot write output file: %s"));

  if (strtab != NULL)
    {
      dwelf_strtab_free (strtab);
      free (strtab_data->d_buf);
    }

  if (symstrtab != NULL)
    {
      dwelf_strtab_free (symstrtab);
      free (symstrdata->d_buf);
    }
  free_new_data ();
}

/* Process one pair of files, already opened.  */
static void
handle_file (const char *output_file, bool create_dirs,
	     Elf *stripped, const GElf_Ehdr *stripped_ehdr,
	     Elf *unstripped)
{
  size_t phnum;
  ELF_CHECK (elf_getphdrnum (stripped, &phnum) == 0,
	     _("cannot get number of program headers: %s"));

  /* Determine the address bias between the debuginfo file and the main
     file, which may have been modified by prelinking.  */
  GElf_Addr bias = 0;
  if (unstripped != NULL)
    for (size_t i = 0; i < phnum; ++i)
      {
	GElf_Phdr phdr_mem;
	GElf_Phdr *phdr = gelf_getphdr (stripped, i, &phdr_mem);
	ELF_CHECK (phdr != NULL, _("cannot get program header: %s"));
	if (phdr->p_type == PT_LOAD)
	  {
	    GElf_Phdr unstripped_phdr_mem;
	    GElf_Phdr *unstripped_phdr = gelf_getphdr (unstripped, i,
						       &unstripped_phdr_mem);
	    ELF_CHECK (unstripped_phdr != NULL,
		       _("cannot get program header: %s"));
	    bias = phdr->p_vaddr - unstripped_phdr->p_vaddr;
	    break;
	  }
      }

  /* One day we could adjust all the DWARF data (like prelink itself does).  */
  if (bias != 0)
    {
      if (output_file == NULL)
	error (0, 0, _("\
DWARF data not adjusted for prelinking bias; consider prelink -u"));
      else
	error (0, 0, _("\
DWARF data in '%s' not adjusted for prelinking bias; consider prelink -u"),
	       output_file);
    }

  if (output_file == NULL)
    /* Modify the unstripped file in place.  */
    copy_elided_sections (unstripped, stripped, stripped_ehdr, bias);
  else
    {
      if (create_dirs)
	make_directories (output_file);

      /* Copy the unstripped file and then modify it.  */
      int outfd = open (output_file, O_RDWR | O_CREAT,
			(stripped_ehdr->e_type == ET_REL
			 ? DEFFILEMODE : ACCESSPERMS));
      if (outfd < 0)
	error_exit (errno, _("cannot open '%s'"), output_file);
      Elf *outelf = elf_begin (outfd, ELF_C_WRITE, NULL);
      ELF_CHECK (outelf != NULL, _("cannot create ELF descriptor: %s"));

      if (unstripped == NULL)
	{
	  /* Actually, we are just copying out the main file as it is.  */
	  copy_elf (outelf, stripped);
	  if (stripped_ehdr->e_type != ET_REL)
	    elf_flagelf (outelf, ELF_C_SET, ELF_F_LAYOUT);
	  ELF_CHECK (elf_update (outelf, ELF_C_WRITE) > 0,
		     _("cannot write output file: %s"));
	}
      else
	{
	  copy_elf (outelf, unstripped);
	  copy_elided_sections (outelf, stripped, stripped_ehdr, bias);
	}

      elf_end (outelf);
      close (outfd);
    }
}

static int
open_file (const char *file, bool writable)
{
  int fd = open (file, writable ? O_RDWR : O_RDONLY);
  if (fd < 0)
    error_exit (errno, _("cannot open '%s'"), file);
  return fd;
}

/* Warn, and exit if not forced to continue, if some ELF header
   sanity check for the stripped and unstripped files failed.  */
static void
warn (const char *msg, bool force,
      const char *stripped_file, const char *unstripped_file)
{
  error (force ? 0 : EXIT_FAILURE, 0, "%s'%s' and '%s' %s%s.",
	 force ? _("WARNING: ") : "",
	 stripped_file, unstripped_file, msg,
	 force ? "" : _(", use --force"));
}

/* Handle a pair of files we need to open by name.  */
static void
handle_explicit_files (const char *output_file, bool create_dirs, bool force,
		       const char *stripped_file, const char *unstripped_file)
{
  int stripped_fd = open_file (stripped_file, false);
  Elf *stripped = elf_begin (stripped_fd, ELF_C_READ, NULL);
  GElf_Ehdr stripped_ehdr;
  ELF_CHECK (gelf_getehdr (stripped, &stripped_ehdr),
	     _("cannot create ELF descriptor: %s"));

  int unstripped_fd = -1;
  Elf *unstripped = NULL;
  if (unstripped_file != NULL)
    {
      unstripped_fd = open_file (unstripped_file, output_file == NULL);
      unstripped = elf_begin (unstripped_fd,
			      (output_file == NULL ? ELF_C_RDWR : ELF_C_READ),
			      NULL);
      GElf_Ehdr unstripped_ehdr;
      ELF_CHECK (gelf_getehdr (unstripped, &unstripped_ehdr),
		 _("cannot create ELF descriptor: %s"));

      if (memcmp (stripped_ehdr.e_ident,
		  unstripped_ehdr.e_ident, EI_NIDENT) != 0)
	warn (_("ELF header identification (e_ident) different"), force,
	      stripped_file, unstripped_file);

      if (stripped_ehdr.e_type != unstripped_ehdr.e_type)
	warn (_("ELF header type (e_type) different"), force,
	      stripped_file, unstripped_file);

      if (stripped_ehdr.e_machine != unstripped_ehdr.e_machine)
	warn (_("ELF header machine type (e_machine) different"), force,
	      stripped_file, unstripped_file);

      if (stripped_ehdr.e_phnum < unstripped_ehdr.e_phnum)
	warn (_("stripped program header (e_phnum) smaller than unstripped"),
	      force, stripped_file, unstripped_file);
    }

  handle_file (output_file, create_dirs, stripped, &stripped_ehdr, unstripped);

  elf_end (stripped);
  close (stripped_fd);

  elf_end (unstripped);
  close (unstripped_fd);
}


/* Handle a pair of files opened implicitly by libdwfl for one module.  */
static void
handle_dwfl_module (const char *output_file, bool create_dirs, bool force,
		    Dwfl_Module *mod, bool all, bool ignore, bool relocate)
{
  GElf_Addr bias;
  Elf *stripped = dwfl_module_getelf (mod, &bias);
  if (stripped == NULL)
    {
      if (ignore)
	return;

      const char *file;
      const char *modname = dwfl_module_info (mod, NULL, NULL, NULL,
					      NULL, NULL, &file, NULL);
      if (file == NULL)
	error_exit (0,
		    _("cannot find stripped file for module '%s': %s"),
		    modname, dwfl_errmsg (-1));
      else
	error_exit (0,
		    _("cannot open stripped file '%s' for module '%s': %s"),
		    modname, file, dwfl_errmsg (-1));
    }

  Elf *debug = dwarf_getelf (dwfl_module_getdwarf (mod, &bias));
  if (debug == NULL && !all)
    {
      if (ignore)
	return;

      const char *file;
      const char *modname = dwfl_module_info (mod, NULL, NULL, NULL,
					      NULL, NULL, NULL, &file);
      if (file == NULL)
	error_exit (0,
		    _("cannot find debug file for module '%s': %s"),
		    modname, dwfl_errmsg (-1));
      else
	error_exit (0,
		    _("cannot open debug file '%s' for module '%s': %s"),
		    modname, file, dwfl_errmsg (-1));
    }

  if (debug == stripped)
    {
      if (all)
	debug = NULL;
      else
	{
	  const char *file;
	  const char *modname = dwfl_module_info (mod, NULL, NULL, NULL,
						  NULL, NULL, &file, NULL);
	  error_exit (0, _("module '%s' file '%s' is not stripped"),
		      modname, file);
	}
    }

  GElf_Ehdr stripped_ehdr;
  ELF_CHECK (gelf_getehdr (stripped, &stripped_ehdr),
	     _("cannot create ELF descriptor: %s"));

  if (stripped_ehdr.e_type == ET_REL)
    {
      if (!relocate)
	{
	  /* We can't use the Elf handles already open,
	     because the DWARF sections have been relocated.  */

	  const char *stripped_file = NULL;
	  const char *unstripped_file = NULL;
	  (void) dwfl_module_info (mod, NULL, NULL, NULL, NULL, NULL,
				   &stripped_file, &unstripped_file);

	  handle_explicit_files (output_file, create_dirs, force,
				 stripped_file, unstripped_file);
	  return;
	}

      /* Relocation is what we want!  This ensures that all sections that can
	 get sh_addr values assigned have them, even ones not used in DWARF.
	 They might still be used in the symbol table.  */
      if (dwfl_module_relocations (mod) < 0)
	error_exit (0,
		    _("cannot cache section addresses for module '%s': %s"),
		    dwfl_module_info (mod, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
		    dwfl_errmsg (-1));
    }

  handle_file (output_file, create_dirs, stripped, &stripped_ehdr, debug);
}

/* Handle one module being written to the output directory.  */
static void
handle_output_dir_module (const char *output_dir, Dwfl_Module *mod, bool force,
			  bool all, bool ignore, bool modnames, bool relocate)
{
  if (! modnames)
    {
      /* Make sure we've searched for the ELF file.  */
      GElf_Addr bias;
      (void) dwfl_module_getelf (mod, &bias);
    }

  const char *file;
  const char *name = dwfl_module_info (mod, NULL, NULL, NULL,
				       NULL, NULL, &file, NULL);

  if (file == NULL && ignore)
    return;

  char *output_file = xasprintf ("%s/%s", output_dir, modnames ? name : file);

  handle_dwfl_module (output_file, true, force, mod, all, ignore, relocate);

  free (output_file);
}


static void
list_module (Dwfl_Module *mod)
{
  /* Make sure we have searched for the files.  */
  GElf_Addr bias;
  bool have_elf = dwfl_module_getelf (mod, &bias) != NULL;
  bool have_dwarf = dwfl_module_getdwarf (mod, &bias) != NULL;

  const char *file;
  const char *debug;
  Dwarf_Addr start;
  Dwarf_Addr end;
  const char *name = dwfl_module_info (mod, NULL, &start, &end,
				       NULL, NULL, &file, &debug);
  if (file != NULL && debug != NULL && (debug == file || !strcmp (debug, file)))
    debug = ".";

  const unsigned char *id;
  GElf_Addr id_vaddr;
  int id_len = dwfl_module_build_id (mod, &id, &id_vaddr);

  printf ("%#" PRIx64 "+%#" PRIx64 " ", start, end - start);

  if (id_len > 0)
    {
      do
	printf ("%02" PRIx8, *id++);
      while (--id_len > 0);
      if (id_vaddr != 0)
	printf ("@%#" PRIx64, id_vaddr);
    }
  else
    putchar ('-');

  printf (" %s %s %s\n",
	  file ?: have_elf ? "." : "-",
	  debug ?: have_dwarf ? "." : "-",
	  name);
}


struct match_module_info
{
  char **patterns;
  Dwfl_Module *found;
  bool match_files;
};

static int
match_module (Dwfl_Module *mod,
	      void **userdata __attribute__ ((unused)),
	      const char *name,
	      Dwarf_Addr start __attribute__ ((unused)),
	      void *arg)
{
  struct match_module_info *info = arg;

  if (info->patterns[0] == NULL) /* Match all.  */
    {
    match:
      info->found = mod;
      return DWARF_CB_ABORT;
    }

  if (info->match_files)
    {
      /* Make sure we've searched for the ELF file.  */
      GElf_Addr bias;
      (void) dwfl_module_getelf (mod, &bias);

      const char *file;
      const char *check = dwfl_module_info (mod, NULL, NULL, NULL,
					    NULL, NULL, &file, NULL);
      if (check == NULL || strcmp (check, name) != 0 || file == NULL)
	return DWARF_CB_OK;

      name = file;
    }

  for (char **p = info->patterns; *p != NULL; ++p)
    if (fnmatch (*p, name, 0) == 0)
      goto match;

  return DWARF_CB_OK;
}

/* Handle files opened implicitly via libdwfl.  */
static void
handle_implicit_modules (const struct arg_info *info)
{
  struct match_module_info mmi = { info->args, NULL, info->match_files };
  ptrdiff_t offset = dwfl_getmodules (info->dwfl, &match_module, &mmi, 0);
  if (offset == 0)
    error_exit (0, _("no matching modules found"));

  if (info->list)
    do
      list_module (mmi.found);
    while ((offset = dwfl_getmodules (info->dwfl, &match_module, &mmi,
				      offset)) > 0);
  else if (info->output_dir == NULL)
    {
      if (dwfl_getmodules (info->dwfl, &match_module, &mmi, offset) != 0)
	error_exit (0, _("matched more than one module"));
      handle_dwfl_module (info->output_file, false, info->force, mmi.found,
			  info->all, info->ignore, info->relocate);
    }
  else
    do
      handle_output_dir_module (info->output_dir, mmi.found, info->force,
				info->all, info->ignore,
				info->modnames, info->relocate);
    while ((offset = dwfl_getmodules (info->dwfl, &match_module, &mmi,
				      offset)) > 0);
}

int
main (int argc, char **argv)
{
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
  const struct argp_child argp_children[] =
    {
      {
	.argp = dwfl_standard_argp (),
	.header = N_("Input selection options:"),
	.group = 1,
      },
      { .argp = NULL },
    };
  const struct argp argp =
    {
      .options = options,
      .parser = parse_opt,
      .children = argp_children,
      .args_doc = N_("STRIPPED-FILE DEBUG-FILE\n[MODULE...]"),
      .doc = N_("\
Combine stripped files with separate symbols and debug information.\n\
\n\
The first form puts the result in DEBUG-FILE if -o was not given.\n\
\n\
MODULE arguments give file name patterns matching modules to process.\n\
With -f these match the file name of the main (stripped) file \
(slashes are never special), otherwise they match the simple module names.  \
With no arguments, process all modules found.\n\
\n\
Multiple modules are written to files under OUTPUT-DIRECTORY, \
creating subdirectories as needed.  \
With -m these files have simple module names, otherwise they have the \
name of the main file complete with directory underneath OUTPUT-DIRECTORY.\n\
\n\
With -n no files are written, but one line to standard output for each module:\
\n\tSTART+SIZE BUILDID FILE DEBUGFILE MODULENAME\n\
START and SIZE are hexadecimal giving the address bounds of the module.  \
BUILDID is hexadecimal for the build ID bits, or - if no ID is known; \
the hexadecimal may be followed by @0xADDR giving the address where the \
ID resides if that is known.  \
FILE is the file name found for the module, or - if none was found, \
or . if an ELF image is available but not from any named file.  \
DEBUGFILE is the separate debuginfo file name, \
or - if no debuginfo was found, or . if FILE contains the debug information.\
")
    };

  int remaining;
  struct arg_info info = { .args = NULL };
  error_t result = argp_parse (&argp, argc, argv, 0, &remaining, &info);
  if (result == ENOSYS)
    assert (info.dwfl == NULL);
  else if (result)
    return EXIT_FAILURE;
  assert (info.args != NULL);

  /* Tell the library which version we are expecting.  */
  elf_version (EV_CURRENT);

  if (info.dwfl == NULL)
    {
      assert (result == ENOSYS);

      if (info.output_dir != NULL)
	{
	  char *file = xasprintf ("%s/%s", info.output_dir, info.args[0]);
	  handle_explicit_files (file, true, info.force,
				 info.args[0], info.args[1]);
	  free (file);
	}
      else
	handle_explicit_files (info.output_file, false, info.force,
			       info.args[0], info.args[1]);
    }
  else
    {
      /* parse_opt checked this.  */
      assert (info.output_file != NULL || info.output_dir != NULL || info.list);

      handle_implicit_modules (&info);

      dwfl_end (info.dwfl);
    }

  return 0;
}


#include "debugpred.h"
