/* Print information from ELF file in human-readable form.
   Copyright (C) 2005, 2006, 2007, 2009, 2011, 2012, 2014, 2015 Red Hat, Inc.
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
#include <fcntl.h>
#include <inttypes.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libeu.h>
#include <system.h>
#include <color.h>
#include <printversion.h>
#include "../libebl/libeblP.h"


/* Name and version of program.  */
ARGP_PROGRAM_VERSION_HOOK_DEF = print_version;

/* Bug report address.  */
ARGP_PROGRAM_BUG_ADDRESS_DEF = PACKAGE_BUGREPORT;


/* Definitions of arguments for argp functions.  */
static const struct argp_option options[] =
{
  { NULL, 0, NULL, 0, N_("Mode selection:"), 0 },
  { "reloc", 'r', NULL, 0, N_("Display relocation information."), 0 },
  { "full-contents", 's', NULL, 0,
    N_("Display the full contents of all sections requested"), 0 },
  { "disassemble", 'd', NULL, 0,
    N_("Display assembler code of executable sections"), 0 },

  { NULL, 0, NULL, 0, N_("Output content selection:"), 0 },
  { "section", 'j', "NAME", 0,
    N_("Only display information for section NAME."), 0 },

  { NULL, 0, NULL, 0, NULL, 0 }
};

/* Short description of program.  */
static const char doc[] = N_("\
Show information from FILEs (a.out by default).");

/* Strings for arguments in help texts.  */
static const char args_doc[] = N_("[FILE...]");

/* Prototype for option handler.  */
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/* Parser children.  */
static struct argp_child argp_children[] =
  {
    { &color_argp, 0, N_("Output formatting"), 2 },
    { NULL, 0, NULL, 0}
  };

/* Data structure to communicate with argp functions.  */
static const struct argp argp =
{
  options, parse_opt, args_doc, doc, argp_children, NULL, NULL
};


/* Print symbols in file named FNAME.  */
static int process_file (const char *fname, bool more_than_one);

/* Handle content of archive.  */
static int handle_ar (int fd, Elf *elf, const char *prefix, const char *fname,
		      const char *suffix);

/* Handle ELF file.  */
static int handle_elf (Elf *elf, const char *prefix, const char *fname,
		       const char *suffix);


#define INTERNAL_ERROR(fname) \
  error_exit (0, _("%s: INTERNAL ERROR %d (%s): %s"),      \
	      fname, __LINE__, PACKAGE_VERSION, elf_errmsg (-1))


/* List of sections which should be used.  */
static struct section_list
{
  bool is_name;
  union
  {
    const char *name;
    uint32_t scnndx;
  };
  struct section_list *next;
} *section_list;


/* If true print archive index.  */
static bool print_relocs;

/* If true print full contents of requested sections.  */
static bool print_full_content;

/* If true print disassembled output..  */
static bool print_disasm;


int
main (int argc, char *argv[])
{
  /* We use no threads here which can interfere with handling a stream.  */
  (void) __fsetlocking (stdin, FSETLOCKING_BYCALLER);
  (void) __fsetlocking (stdout, FSETLOCKING_BYCALLER);
  (void) __fsetlocking (stderr, FSETLOCKING_BYCALLER);

  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  /* Make sure the message catalog can be found.  */
  (void) bindtextdomain (PACKAGE_TARNAME, LOCALEDIR);

  /* Initialize the message catalog.  */
  (void) textdomain (PACKAGE_TARNAME);

  /* Parse and process arguments.  */
  int remaining;
  (void) argp_parse (&argp, argc, argv, 0, &remaining, NULL);

  /* Tell the library which version we are expecting.  */
  (void) elf_version (EV_CURRENT);

  int result = 0;
  if (remaining == argc)
    /* The user didn't specify a name so we use a.out.  */
    result = process_file ("a.out", false);
  else
    {
      /* Process all the remaining files.  */
      const bool more_than_one = remaining + 1 < argc;

      do
	result |= process_file (argv[remaining], more_than_one);
      while (++remaining < argc);
    }

  return result;
}


/* Handle program arguments.  */
static error_t
parse_opt (int key, char *arg,
	   struct argp_state *state __attribute__ ((unused)))
{
  /* True if any of the control options is set.  */
  static bool any_control_option;

  switch (key)
    {
    case 'j':
      {
	struct section_list *newp = xmalloc (sizeof (*newp));
	char *endp;
	newp->scnndx = strtoul (arg, &endp, 0);
	if (*endp == 0)
	  newp->is_name = false;
	else
	  {
	    newp->name = arg;
	    newp->is_name = true;
	  }
	newp->next = section_list;
	section_list = newp;
      }
      any_control_option = true;
      break;

    case 'd':
      print_disasm = true;
      any_control_option = true;
      break;

    case 'r':
      print_relocs = true;
      any_control_option = true;
      break;

    case 's':
      print_full_content = true;
      any_control_option = true;
      break;

    case ARGP_KEY_FINI:
      if (! any_control_option)
	{
	  fputs (_("No operation specified.\n"), stderr);
	  argp_help (&argp, stderr, ARGP_HELP_SEE,
		     program_invocation_short_name);
	  exit (EXIT_FAILURE);
	}
      /* We only use this for checking the number of arguments, we don't
	 actually want to consume them.  */
      FALLTHROUGH;
    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}


/* Open the file and determine the type.  */
static int
process_file (const char *fname, bool more_than_one)
{
  /* Open the file.  */
  int fd = open (fname, O_RDONLY);
  if (fd == -1)
    {
      error (0, errno, _("cannot open %s"), fname);
      return 1;
    }

  /* Now get the ELF descriptor.  */
  Elf *elf = elf_begin (fd, ELF_C_READ_MMAP, NULL);
  if (elf != NULL)
    {
      if (elf_kind (elf) == ELF_K_ELF)
	{
	  int result = handle_elf (elf, more_than_one ? "" : NULL,
				   fname, NULL);

	  if (elf_end (elf) != 0)
	    INTERNAL_ERROR (fname);

	  if (close (fd) != 0)
	    error_exit (errno, _("while close `%s'"), fname);

	  return result;
	}
      else if (elf_kind (elf) == ELF_K_AR)
	{
	  int result = handle_ar (fd, elf, NULL, fname, NULL);

	  if (elf_end (elf) != 0)
	    INTERNAL_ERROR (fname);

	  if (close (fd) != 0)
	    error_exit (errno, _("while close `%s'"), fname);

	  return result;
	}

      /* We cannot handle this type.  Close the descriptor anyway.  */
      if (elf_end (elf) != 0)
	INTERNAL_ERROR (fname);
    }

  error (0, 0, _("%s: File format not recognized"), fname);

  return 1;
}


static int
handle_ar (int fd, Elf *elf, const char *prefix, const char *fname,
	   const char *suffix)
{
  size_t fname_len = strlen (fname) + 1;
  size_t prefix_len = prefix != NULL ? strlen (prefix) : 0;
  char new_prefix[prefix_len + fname_len + 2];
  size_t suffix_len = suffix != NULL ? strlen (suffix) : 0;
  char new_suffix[suffix_len + 2];
  Elf *subelf;
  Elf_Cmd cmd = ELF_C_READ_MMAP;
  int result = 0;

  char *cp = new_prefix;
  if (prefix != NULL)
    cp = stpcpy (cp, prefix);
  cp = stpcpy (cp, fname);
  stpcpy (cp, "[");

  cp = new_suffix;
  if (suffix != NULL)
    cp = stpcpy (cp, suffix);
  stpcpy (cp, "]");

  /* Process all the files contained in the archive.  */
  while ((subelf = elf_begin (fd, cmd, elf)) != NULL)
    {
      /* The the header for this element.  */
      Elf_Arhdr *arhdr = elf_getarhdr (subelf);

      /* Skip over the index entries.  */
      if (strcmp (arhdr->ar_name, "/") != 0
	  && strcmp (arhdr->ar_name, "//") != 0)
	{
	  if (elf_kind (subelf) == ELF_K_ELF)
	    result |= handle_elf (subelf, new_prefix, arhdr->ar_name,
				  new_suffix);
	  else if (elf_kind (subelf) == ELF_K_AR)
	    result |= handle_ar (fd, subelf, new_prefix, arhdr->ar_name,
				 new_suffix);
	  else
	    {
	      error (0, 0, _("%s%s%s: file format not recognized"),
		     new_prefix, arhdr->ar_name, new_suffix);
	      result = 1;
	    }
	}

      /* Get next archive element.  */
      cmd = elf_next (subelf);
      if (elf_end (subelf) != 0)
	INTERNAL_ERROR (fname);
    }

  return result;
}


static void
show_relocs_x (Ebl *ebl, GElf_Shdr *shdr, Elf_Data *symdata,
	       Elf_Data *xndxdata, size_t symstrndx, size_t shstrndx,
	       GElf_Addr r_offset, GElf_Xword r_info, GElf_Sxword r_addend)
{
  int elfclass = gelf_getclass (ebl->elf);
  char buf[128];

  printf ("%0*" PRIx64 " %-20s ",
	  elfclass == ELFCLASS32 ? 8 : 16, r_offset,
	  ebl_reloc_type_name (ebl, GELF_R_TYPE (r_info), buf, sizeof (buf)));

  Elf32_Word xndx;
  GElf_Sym symmem;
  GElf_Sym *sym = gelf_getsymshndx (symdata, xndxdata, GELF_R_SYM (r_info),
				    &symmem, &xndx);

  if (sym == NULL)
    printf ("<%s %ld>",
	    _("INVALID SYMBOL"), (long int) GELF_R_SYM (r_info));
  else if (GELF_ST_TYPE (sym->st_info) != STT_SECTION)
    printf ("%s",
	    elf_strptr (ebl->elf, symstrndx, sym->st_name));
  else
    {
      GElf_Shdr destshdr_mem;
      GElf_Shdr *destshdr;
      destshdr = gelf_getshdr (elf_getscn (ebl->elf,
					   sym->st_shndx == SHN_XINDEX
					   ? xndx : sym->st_shndx),
			       &destshdr_mem);

      if (shdr == NULL || destshdr == NULL)
	printf ("<%s %ld>",
		_("INVALID SECTION"),
		(long int) (sym->st_shndx == SHN_XINDEX
			    ? xndx : sym->st_shndx));
      else
	printf ("%s",
		elf_strptr (ebl->elf, shstrndx, destshdr->sh_name));
    }

  if (r_addend != 0)
    {
      char sign = '+';
      if (r_addend < 0)
	{
	  sign = '-';
	  r_addend = -r_addend;
	}
      printf ("%c%#" PRIx64, sign, r_addend);
    }
  putchar ('\n');
}


static void
show_relocs_rel (Ebl *ebl, GElf_Shdr *shdr, Elf_Data *data,
		 Elf_Data *symdata, Elf_Data *xndxdata, size_t symstrndx,
		 size_t shstrndx)
{
  size_t sh_entsize = gelf_fsize (ebl->elf, ELF_T_REL, 1, EV_CURRENT);
  int nentries = shdr->sh_size / sh_entsize;

  for (int cnt = 0; cnt < nentries; ++cnt)
    {
      GElf_Rel relmem;
      GElf_Rel *rel;

      rel = gelf_getrel (data, cnt, &relmem);
      if (rel != NULL)
	show_relocs_x (ebl, shdr, symdata, xndxdata, symstrndx, shstrndx,
		       rel->r_offset, rel->r_info, 0);
    }
}


static void
show_relocs_rela (Ebl *ebl, GElf_Shdr *shdr, Elf_Data *data,
		  Elf_Data *symdata, Elf_Data *xndxdata, size_t symstrndx,
		  size_t shstrndx)
{
  size_t sh_entsize = gelf_fsize (ebl->elf, ELF_T_RELA, 1, EV_CURRENT);
  int nentries = shdr->sh_size / sh_entsize;

  for (int cnt = 0; cnt < nentries; ++cnt)
    {
      GElf_Rela relmem;
      GElf_Rela *rel;

      rel = gelf_getrela (data, cnt, &relmem);
      if (rel != NULL)
	show_relocs_x (ebl, shdr, symdata, xndxdata, symstrndx, shstrndx,
		       rel->r_offset, rel->r_info, rel->r_addend);
    }
}


static bool
section_match (Elf *elf, uint32_t scnndx, GElf_Shdr *shdr, size_t shstrndx)
{
  if (section_list == NULL)
    return true;

  struct section_list *runp = section_list;
  const char *name = elf_strptr (elf, shstrndx, shdr->sh_name);

  do
    {
      if (runp->is_name)
	{
	  if (name && strcmp (runp->name, name) == 0)
	    return true;
	}
      else
	{
	  if (runp->scnndx == scnndx)
	    return true;
	}

      runp = runp->next;
    }
  while (runp != NULL);

  return false;
}


static int
show_relocs (Ebl *ebl, const char *fname, uint32_t shstrndx)
{
  int elfclass = gelf_getclass (ebl->elf);

  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (ebl->elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);

      if (shdr == NULL)
	INTERNAL_ERROR (fname);

      if (shdr->sh_type == SHT_REL || shdr->sh_type == SHT_RELA)
	{
	  if  (! section_match (ebl->elf, elf_ndxscn (scn), shdr, shstrndx))
	    continue;

	  GElf_Shdr destshdr_mem;
	  GElf_Shdr *destshdr = gelf_getshdr (elf_getscn (ebl->elf,
							  shdr->sh_info),
					      &destshdr_mem);
	  if (unlikely (destshdr == NULL))
	    continue;

	  printf (_("\nRELOCATION RECORDS FOR [%s]:\n"
			   "%-*s TYPE                 VALUE\n"),
		  elf_strptr (ebl->elf, shstrndx, destshdr->sh_name),
		  elfclass == ELFCLASS32 ? 8 : 16, _("OFFSET"));

	  /* Get the data of the section.  */
	  Elf_Data *data = elf_getdata (scn, NULL);
	  if (data == NULL)
	    continue;

	  /* Get the symbol table information.  */
	  Elf_Scn *symscn = elf_getscn (ebl->elf, shdr->sh_link);
	  GElf_Shdr symshdr_mem;
	  GElf_Shdr *symshdr = gelf_getshdr (symscn, &symshdr_mem);
	  Elf_Data *symdata = elf_getdata (symscn, NULL);
	  if (unlikely (symshdr == NULL || symdata == NULL))
	    continue;

	  /* Search for the optional extended section index table.  */
	  Elf_Data *xndxdata = NULL;
	  Elf_Scn *xndxscn = NULL;
	  while ((xndxscn = elf_nextscn (ebl->elf, xndxscn)) != NULL)
	    {
	      GElf_Shdr xndxshdr_mem;
	      GElf_Shdr *xndxshdr;

	      xndxshdr = gelf_getshdr (xndxscn, &xndxshdr_mem);
	      if (xndxshdr != NULL && xndxshdr->sh_type == SHT_SYMTAB_SHNDX
		  && xndxshdr->sh_link == elf_ndxscn (symscn))
		{
		  /* Found it.  */
		  xndxdata = elf_getdata (xndxscn, NULL);
		  break;
		}
	    }

	  if (shdr->sh_type == SHT_REL)
	    show_relocs_rel (ebl, shdr, data, symdata, xndxdata,
			     symshdr->sh_link, shstrndx);
	  else
	    show_relocs_rela (ebl, shdr, data, symdata, xndxdata,
			      symshdr->sh_link, shstrndx);

	  putchar ('\n');
	}
    }

  return 0;
}


static int
show_full_content (Ebl *ebl, const char *fname, uint32_t shstrndx)
{
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (ebl->elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);

      if (shdr == NULL)
	INTERNAL_ERROR (fname);

      if (shdr->sh_type == SHT_PROGBITS && shdr->sh_size > 0)
	{
	  if  (! section_match (ebl->elf, elf_ndxscn (scn), shdr, shstrndx))
	    continue;

	  printf (_("Contents of section %s:\n"),
		  elf_strptr (ebl->elf, shstrndx, shdr->sh_name));

	  /* Get the data of the section.  */
	  Elf_Data *data = elf_getdata (scn, NULL);
	  if (data == NULL)
	    continue;

	  unsigned char *cp = data->d_buf;
	  size_t cnt;
	  for (cnt = 0; cnt + 16 < data->d_size; cp += 16, cnt += 16)
	    {
	      printf (" %04zx ", cnt);

	      for (size_t inner = 0; inner < 16; inner += 4)
		printf ("%02hhx%02hhx%02hhx%02hhx ",
			cp[inner], cp[inner + 1], cp[inner + 2],
			cp[inner + 3]);
	      fputc_unlocked (' ', stdout);

	      for (size_t inner = 0; inner < 16; ++inner)
		fputc_unlocked (isascii (cp[inner]) && isprint (cp[inner])
				? cp[inner] : '.', stdout);
	      fputc_unlocked ('\n', stdout);
	    }

	  printf (" %04zx ", cnt);

	  size_t remaining = data->d_size - cnt;
	  size_t inner;
	  for (inner = 0; inner + 4 <= remaining; inner += 4)
	    printf ("%02hhx%02hhx%02hhx%02hhx ",
		    cp[inner], cp[inner + 1], cp[inner + 2], cp[inner + 3]);

	  for (; inner < remaining; ++inner)
	    printf ("%02hhx", cp[inner]);

	  for (inner = 2 * (16 - inner) + (16 - inner + 3) / 4 + 1; inner > 0;
	       --inner)
	    fputc_unlocked (' ', stdout);

	  for (inner = 0; inner < remaining; ++inner)
	    fputc_unlocked (isascii (cp[inner]) && isprint (cp[inner])
			    ? cp[inner] : '.', stdout);
	  fputc_unlocked ('\n', stdout);

	  fputc_unlocked ('\n', stdout);
	}
    }

  return 0;
}


struct disasm_info
{
  GElf_Addr addr;
  const uint8_t *cur;
  const uint8_t *last_end;
  const char *address_color;
  const char *bytes_color;
};


// XXX This is not the preferred output for all architectures.  Needs
// XXX customization, too.
static int
disasm_output (char *buf, size_t buflen, void *arg)
{
  struct disasm_info *info = (struct disasm_info *) arg;

  if (info->address_color != NULL)
    printf ("%s%8" PRIx64 "%s:   ",
	    info->address_color, (uint64_t) info->addr, color_off);
  else
    printf ("%8" PRIx64 ":   ", (uint64_t) info->addr);

  if (info->bytes_color != NULL)
    fputs_unlocked (info->bytes_color, stdout);
  size_t cnt;
  for (cnt = 0; cnt < (size_t) MIN (info->cur - info->last_end, 8); ++cnt)
    printf (" %02" PRIx8, info->last_end[cnt]);
  if (info->bytes_color != NULL)
    fputs_unlocked (color_off, stdout);

  printf ("%*s %.*s\n",
	  (int) (8 - cnt) * 3 + 1, "", (int) buflen, buf);

  info->addr += cnt;

  /* We limit the number of bytes printed before the mnemonic to 8.
     Print the rest on a separate, following line.  */
  if (info->cur - info->last_end > 8)
    {
      if (info->address_color != NULL)
	printf ("%s%8" PRIx64 "%s:   ",
		info->address_color, (uint64_t) info->addr, color_off);
      else
	printf ("%8" PRIx64 ":   ", (uint64_t) info->addr);

      if (info->bytes_color != NULL)
	fputs_unlocked (info->bytes_color, stdout);
      for (; cnt < (size_t) (info->cur - info->last_end); ++cnt)
	printf (" %02" PRIx8, info->last_end[cnt]);
      if (info->bytes_color != NULL)
	fputs_unlocked (color_off, stdout);
      putchar_unlocked ('\n');
      info->addr += info->cur - info->last_end - 8;
    }

  info->last_end = info->cur;

  return 0;
}


static int
show_disasm (Ebl *ebl, const char *fname, uint32_t shstrndx)
{
  DisasmCtx_t *ctx = disasm_begin (ebl, ebl->elf, NULL /* XXX TODO */);
  if (ctx == NULL)
    error_exit (0, _("cannot disassemble"));

  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (ebl->elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);

      if (shdr == NULL)
	INTERNAL_ERROR (fname);

      if (shdr->sh_type == SHT_PROGBITS && shdr->sh_size > 0
	  && (shdr->sh_flags & SHF_EXECINSTR) != 0)
	{
	  if  (! section_match (ebl->elf, elf_ndxscn (scn), shdr, shstrndx))
	    continue;

	  Elf_Data *data = elf_getdata (scn, NULL);
	  if (data == NULL)
	    continue;

	  printf ("Disassembly of section %s:\n\n",
		  elf_strptr (ebl->elf, shstrndx, shdr->sh_name));

	  struct disasm_info info;
	  info.addr = shdr->sh_addr;
	  info.last_end = info.cur = data->d_buf;
	  char *fmt;
	  if (color_mode)
	    {
	      info.address_color = color_address;
	      info.bytes_color = color_bytes;

	      fmt = xasprintf ("%s%%7m %s%%.1o,%s%%.2o,%s%%.3o,,%s%%.4o%s%%.5o%%34a %s%%l",
			       color_mnemonic ?: "",
			       color_operand1 ?: "",
			       color_operand2 ?: "",
			       color_operand3 ?: "",
			       color_operand4 ?: "",
			       color_operand5 ?: "",
			       color_label ?: "");
	    }
	  else
	    {
	      info.address_color = info.bytes_color = NULL;

	      fmt = "%7m %.1o,%.2o,%.3o,%.4o,%.5o%34a %l";
	    }

	  disasm_cb (ctx, &info.cur, info.cur + data->d_size, info.addr,
		     fmt, disasm_output, &info, NULL /* XXX */);

	  if (color_mode)
	    free (fmt);
	}
    }

  (void) disasm_end (ctx);

  return 0;
}


static int
handle_elf (Elf *elf, const char *prefix, const char *fname,
	    const char *suffix)
{

  /* Get the backend for this object file type.  */
  Ebl *ebl = ebl_openbackend (elf);
  if (ebl == NULL)
    error_exit (0, _("cannot create backend for elf file"));

  printf ("%s: elf%d-%s\n\n",
	  fname, gelf_getclass (elf) == ELFCLASS32 ? 32 : 64,
	  ebl_backend_name (ebl));

  /* Create the full name of the file.  */
  size_t prefix_len = prefix == NULL ? 0 : strlen (prefix);
  size_t suffix_len = suffix == NULL ? 0 : strlen (suffix);
  size_t fname_len = strlen (fname) + 1;
  char fullname[prefix_len + 1 + fname_len + suffix_len];
  char *cp = fullname;
  if (prefix != NULL)
    cp = mempcpy (cp, prefix, prefix_len);
  cp = mempcpy (cp, fname, fname_len);
  if (suffix != NULL)
    memcpy (cp - 1, suffix, suffix_len + 1);

  /* Get the section header string table index.  */
  size_t shstrndx;
  if (elf_getshdrstrndx (ebl->elf, &shstrndx) < 0)
    error_exit (0, _("cannot get section header string table index"));

  int result = 0;
  if (print_disasm)
    result = show_disasm (ebl, fullname, shstrndx);
  if (print_relocs && !print_disasm)
    result = show_relocs (ebl, fullname, shstrndx);
  if (print_full_content)
    result = show_full_content (ebl, fullname, shstrndx);

  /* Close the ELF backend library descriptor.  */
  ebl_closebackend (ebl);

  return result;
}


#include "debugpred.h"
